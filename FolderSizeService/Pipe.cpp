#include "StdAfx.h"
#include "Pipe.h"
#include "CacheManager.h"

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
	PSID psidLocal;
	SID_IDENTIFIER_AUTHORITY SIDAuthLocal = SECURITY_LOCAL_SID_AUTHORITY;
	if (AllocateAndInitializeSid(&SIDAuthLocal, 1, SECURITY_LOCAL_RID, 0, 0, 0, 0, 0, 0, 0, &psidLocal))
	{
		EXPLICIT_ACCESS ea;
		ea.grfAccessPermissions = GENERIC_WRITE|GENERIC_READ;
		ea.grfAccessMode = SET_ACCESS;
		ea.grfInheritance = NO_INHERITANCE;
		ea.Trustee.pMultipleTrustee = NULL;
		ea.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
		ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
		ea.Trustee.ptstrName = (LPTSTR)psidLocal;
		
		if (SetEntriesInAcl(1, &ea, NULL, &m_pacl) == ERROR_SUCCESS)
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

void HandlePipeClient(HANDLE hPipe, CacheManager* pCacheManager)
{
	PIPE_CLIENT_REQUEST pcr;
	if (ReadRequest(hPipe, pcr))
	{
		switch (pcr)
		{
		case PCR_GETFOLDERSIZE:
			WCHAR szFile[MAX_PATH];
			if (ReadString(hPipe, szFile, MAX_PATH))
			{
				// if szFile is on a network share, impersonate the caller so we can access that share
				BOOL bImpersonating = FALSE;
				if (PathIsUNC(szFile))
				{
					bImpersonating = ImpersonateNamedPipeClient(hPipe);
				}
				FOLDERINFO2 Size;
				pCacheManager->GetInfoForFolder(szFile, Size);
				if (bImpersonating)
				{
					RevertToSelf();
				}
				WriteGetFolderSize(hPipe, Size);
			}
			break;

		case PCR_GETUPDATEDFOLDERS:
			Strings strsBrowsed, strsUpdated;
			if (ReadStringList(hPipe, strsBrowsed))
			{
				for (Strings::iterator i = strsBrowsed.begin(); i != strsBrowsed.end(); i++)
				{
					BOOL bImpersonating = FALSE;
					LPCTSTR pszFolderBrowsed = i->c_str();
					if (PathIsUNC(pszFolderBrowsed))
					{
						bImpersonating = ImpersonateNamedPipeClient(hPipe);
					}
					pCacheManager->GetUpdateFolders(pszFolderBrowsed, strsUpdated);
					if (bImpersonating)
					{
						RevertToSelf();
					}
				}
				WriteStringList(hPipe, strsUpdated);
			}
			break;
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
