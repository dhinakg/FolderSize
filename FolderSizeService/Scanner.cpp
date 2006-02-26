#include "StdAfx.h"
#include "Scanner.h"
#include "PerformanceMonitor.h"
#include "EventLog.h"

Scanner::Scanner(const Path& pathVolume, IScannerCallback* pCallback)
: m_hThread(NULL)
{
	m_pPerformanceMonitor = new PerformanceMonitor(pathVolume);
	m_pCallback = pCallback;
	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hScanEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// if this thread is impersonating the user, the scanner thread needs to have the same impersonation token
	HANDLE hToken;
	if (OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE, FALSE, &hToken))
	{
		// apparently it's a bad idea to create a thread while impersonating
		if (RevertToSelf())
		{
			m_hThread = CreateThread(NULL, 0, ThreadProc, this, CREATE_SUSPENDED, NULL);
			if (m_hThread != NULL)
			{
				if (!SetThreadToken(&m_hThread, hToken) ||
					!ResumeThread(m_hThread))
				{
					TerminateThread(m_hThread, 0);
					m_hThread = NULL;
				}
			}
			// restore the impersonation
			SetThreadToken(NULL, hToken);
		}
		CloseHandle(hToken);
	}
	else
	{
		// if this thread is not impersonating, just create the thread normally
		m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
	}

	if (m_hThread == NULL)
	{
		EventLog::Instance().ReportError(_T("Scanner"), GetLastError());
	}

}

Scanner::~Scanner()
{
	if (m_hThread != NULL)
	{
		SetEvent(m_hQuitEvent);
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
	}
	CloseHandle(m_hQuitEvent);
	CloseHandle(m_hScanEvent);
	delete m_pPerformanceMonitor;
}

void Scanner::ScanFolder(const Path& path)
{
	FOLDERINFO nSize;
	Path pathFind = path + Path(_T("*"));

	// for long path support
	std::wstring strFind = _T("\\\\?\\");
	strFind += pathFind;

	WIN32_FIND_DATA FindData;
	HANDLE hFind = FindFirstFile(strFind.c_str(), &FindData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		// we could be legitimately denied access to this folder
		if (GetLastError() != ERROR_ACCESS_DENIED)
		{
			EventLog::Instance().ReportError(_T("Scanner"), GetLastError());
		}
		return;
	}
	else
	{
		do
		{
			// ignore reparse points... they are links to other directories.
			// including them here might make some folders look smaller than 
			// you think it is, but that's better than multiply counting files.
			if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
			{
				if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (FindData.cFileName[0] != L'.' || (FindData.cFileName[1] != L'\0' && (FindData.cFileName[1] != L'.' || FindData.cFileName[2] != L'\0')))
					{
						Path pathFound = path + Path(FindData.cFileName);
						m_pCallback->FoundFolder(pathFound);

						nSize.nFolders++;
					}
				}
				else
				{
					nSize.nSize += MakeULongLong(FindData.nFileSizeHigh, FindData.nFileSizeLow);

					nSize.nFiles++;
				}
			}
		} while (FindNextFile(hFind, &FindData));
		FindClose(hFind);
	}

	m_pCallback->GotScanResult(path, nSize);
}

void Scanner::Wakeup()
{
	SetEvent(m_hScanEvent);
}

bool Scanner::GetAnItemFromTheQueue(Path& path)
{
	// check if we should quit
	if (WaitForSingleObject(m_hQuitEvent, 0) == WAIT_OBJECT_0)
	{
		return false;
	}

	while (true)
	{
		// We can't reset the event after calling GetNextScanFolder,
		// or we might wipe out the event set by a Wakeup call.
		// So, always set it before.
		ResetEvent(m_hScanEvent);
		if (m_pCallback->GetNextScanFolder(path))
		{
			return true;
		}
		HANDLE hHandles[2];
		hHandles[0] = m_hQuitEvent;
		hHandles[1] = m_hScanEvent;
		if (WaitForMultipleObjects(2, hHandles, FALSE, INFINITE) == WAIT_OBJECT_0)
		{
			return false;
		}
	}

	return true;
}

DWORD WINAPI Scanner::ThreadProc(LPVOID lpParameter)
{
	Scanner* pScanner = (Scanner*)lpParameter;
	pScanner->ThreadProc();
	return 0;
}

void Scanner::ThreadProc()
{
	Path path;
	while ((GetAnItemFromTheQueue(path)))
	{
		ScanFolder(path);
		// if the queue length is too long, wait for a bit
		while (m_pPerformanceMonitor->IsDiskQueueTooLong())
		{
			if (WaitForSingleObject(m_hQuitEvent, PERFORMANCE_UPDATE_FREQUENCY) == WAIT_OBJECT_0)
				return;
		}
	}
}
