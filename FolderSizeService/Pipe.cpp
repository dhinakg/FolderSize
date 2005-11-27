#include "StdAfx.h"
#include "Pipe.h"
#include "CacheManager.h"

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
				FOLDERINFO2 Size;
				pCacheManager->GetInfoForFolder(szFile, Size);
				WriteGetFolderSize(hPipe, Size);
			}
			break;

		case PCR_GETUPDATEDFOLDERS:
			Strings strsBrowsed, strsUpdated;
			if (ReadStringList(hPipe, strsBrowsed))
			{
				pCacheManager->GetUpdateFoldersForBrowsedFolders(strsBrowsed, strsUpdated);
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
	HANDLE hPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\") PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_BYTE, PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE_OUT, PIPE_BUFFER_SIZE_IN, PIPE_DEFAULT_TIME_OUT, NULL);

	if (hPipe == INVALID_HANDLE_VALUE)
	{
		return GetLastError();
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
