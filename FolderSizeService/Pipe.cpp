#include "StdAfx.h"
#include "Pipe.h"
#include "CacheManager.h"
#include "EventLog.h"

class SecurityDescriptor
{
public:
	SecurityDescriptor();
	~SecurityDescriptor();
	const PSECURITY_DESCRIPTOR GetSD() const;

private:
	PSECURITY_DESCRIPTOR m_psd;
	PACL m_pacl;
};

SecurityDescriptor::SecurityDescriptor()
: m_psd(NULL), m_pacl(NULL)
{
	// get the SID for the current process
	PSID pSidSelf = NULL;
	HANDLE hToken;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		DWORD dwReturnLength;
		if (!GetTokenInformation(hToken, TokenUser, NULL, 0, &dwReturnLength) && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			TOKEN_USER* pTokenUser = (TOKEN_USER*)_alloca(dwReturnLength);
			if (GetTokenInformation(hToken, TokenUser, pTokenUser, dwReturnLength, &dwReturnLength))
			{
				pSidSelf = pTokenUser->User.Sid;
			}
		}
		CloseHandle(hToken);
	}

	PSID psidLocal;
	SID_IDENTIFIER_AUTHORITY SIDAuthLocal = SECURITY_LOCAL_SID_AUTHORITY;
	if (AllocateAndInitializeSid(&SIDAuthLocal, 1, SECURITY_LOCAL_RID, 0, 0, 0, 0, 0, 0, 0, &psidLocal))
	{
		EXPLICIT_ACCESS ea[2];
		// any local user can read/write the pipe
		ea[0].grfAccessPermissions = GENERIC_WRITE|GENERIC_READ;
		ea[0].grfAccessMode = SET_ACCESS;
		ea[0].grfInheritance = NO_INHERITANCE;
		ea[0].Trustee.pMultipleTrustee = NULL;
		ea[0].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
		ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
		ea[0].Trustee.ptstrName = (LPTSTR)psidLocal;
		
		// this process can read/write and create the pipe
		ea[1].grfAccessPermissions = GENERIC_WRITE|GENERIC_READ|FILE_CREATE_PIPE_INSTANCE;
		ea[1].grfAccessMode = SET_ACCESS;
		ea[1].grfInheritance = NO_INHERITANCE;
		ea[1].Trustee.pMultipleTrustee = NULL;
		ea[1].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
		ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea[1].Trustee.TrusteeType = TRUSTEE_IS_USER;
		ea[1].Trustee.ptstrName = (LPTSTR)pSidSelf;

		if (SetEntriesInAcl(2, ea, NULL, &m_pacl) == ERROR_SUCCESS)
		{
			m_psd = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
			if (!InitializeSecurityDescriptor(m_psd, SECURITY_DESCRIPTOR_REVISION) ||
				!SetSecurityDescriptorDacl(m_psd, TRUE, m_pacl, FALSE))
			{
				LocalFree(m_psd);
				m_psd = NULL;
			}
		}
		FreeSid(psidLocal);
	}
}

SecurityDescriptor::~SecurityDescriptor()
{
	LocalFree(m_psd);
	LocalFree(m_pacl);
}

const PSECURITY_DESCRIPTOR SecurityDescriptor::GetSD() const
{
	return m_psd;
}

Pipe::Pipe(CacheManager* pCacheManager)
: m_hQuitEvent(NULL), m_hThread(NULL), m_pCacheManager(pCacheManager)
{
	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	DWORD dwThreadId;
	m_hThread = CreateThread(NULL, 0, PipeThread, this, 0, &dwThreadId);
}

Pipe::~Pipe()
{
	SetEvent(m_hQuitEvent);
	WaitForSingleObject(m_hThread, INFINITE);
	CloseHandle(m_hThread);
	CloseHandle(m_hQuitEvent);
}


class NamedPipeClientImpersonator
{
public:
	NamedPipeClientImpersonator(HANDLE hPipe) : m_hPipe(hPipe), m_bImpersonating(false) { Impersonate(); }
	~NamedPipeClientImpersonator() { Revert(); }
	void Impersonate()
	{
		if (!m_bImpersonating)
		{
			if (!ImpersonateNamedPipeClient(m_hPipe))
				EventLog::Instance().ReportError(_T("ImpersonateNamedPipeClient"), GetLastError());
			m_bImpersonating = true;
		}
	}
	void Revert()
	{
		if (m_bImpersonating)
		{
			if (!RevertToSelf())
				EventLog::Instance().ReportError(_T("RevertToSelf"), GetLastError());
			m_bImpersonating = false;
		}
	}
private:
	HANDLE m_hPipe;
	bool m_bImpersonating;
};

void HandlePipeClient(HANDLE hPipe, CacheManager* pCacheManager)
{
	PIPE_CLIENT_REQUEST pcr;
	if (ReadRequest(hPipe, pcr))
	{
		switch (pcr)
		{
		case PCR_GETFOLDERSIZE:
			{
				Path path;
				if (ReadString(hPipe, path))
				{
					// impersonate the caller so we'll interpret his mapped drives correctly
					NamedPipeClientImpersonator Impersonator(hPipe);

					if (!path.IsNetwork())
						Impersonator.Revert();

					FOLDERINFO2 Size;
					bool bGotInfo = pCacheManager->GetInfoForFolder(path, Size);
					Impersonator.Revert();

					// if we didn't get info, just disconnect the client
					if (bGotInfo)
						WriteGetFolderSize(hPipe, Size);
				}
				break;
			}

		case PCR_GETUPDATEDFOLDERS:
			{
				Strings strsBrowsed, strsUpdated;
				if (ReadStringList(hPipe, strsBrowsed))
				{
					NamedPipeClientImpersonator Impersonator(hPipe);
					for (Strings::iterator i = strsBrowsed.begin(); i != strsBrowsed.end(); i++)
					{
						Path pathBrowsed = i->c_str();

						Impersonator.Impersonate();
						if (!pathBrowsed.IsNetwork())
							Impersonator.Revert();

						pCacheManager->GetUpdateFolders(pathBrowsed, strsUpdated);
					}
					Impersonator.Revert();

					WriteStringList(hPipe, strsUpdated);
				}
				break;
			}
		}
	}
	FlushFileBuffers(hPipe);
	DisconnectNamedPipe(hPipe);
}

DWORD WINAPI Pipe::PipeThread(LPVOID lpParameter)
{
	Pipe* pPipe = (Pipe*)lpParameter;
	return pPipe->PipeThread();
}

DWORD Pipe::PipeThread()
{
	SecurityDescriptor sd;
	SECURITY_ATTRIBUTES sa = { sizeof(sa) };
	sa.lpSecurityDescriptor = sd.GetSD();

	HANDLE hPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\") PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_BYTE, PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE_OUT, PIPE_BUFFER_SIZE_IN, PIPE_DEFAULT_TIME_OUT, &sa);

	if (hPipe == INVALID_HANDLE_VALUE)
	{
		EventLog::Instance().ReportError(TEXT("CreateNamedPipe"), GetLastError());
		return 0;
	}
	
	HANDLE hWaitHandles[2];
	hWaitHandles[0] = m_hQuitEvent;
	hWaitHandles[1] = hPipe;

	OVERLAPPED o;
	ZeroMemory(&o, sizeof(o));

	while (true)
	{
		if (ConnectNamedPipe(hPipe, &o))
		{
			HandlePipeClient(hPipe, m_pCacheManager);
		}
		else
		{
			DWORD dwLastError = GetLastError();
			if (dwLastError == ERROR_PIPE_CONNECTED)
			{
				HandlePipeClient(hPipe, m_pCacheManager);
			}
			else if (dwLastError == ERROR_IO_PENDING)
			{
				DWORD dwWait = WaitForMultipleObjects(2, hWaitHandles, FALSE, INFINITE);
				if (dwWait == WAIT_OBJECT_0 + 1)
				{
					HandlePipeClient(hPipe, m_pCacheManager);
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

	CloseHandle(hPipe);

	return NOERROR;
}
