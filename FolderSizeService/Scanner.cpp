#include "StdAfx.h"
#include "Scanner.h"


Scanner::Scanner(IScannerCallback* pCallback)
{
	m_pCallback = pCallback;
	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hScanEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
	// don't let the scanner hog the disk!
	//SetThreadPriority(m_hThread, THREAD_PRIORITY_BELOW_NORMAL);
}

Scanner::~Scanner()
{
	SetEvent(m_hQuitEvent);
	WaitForSingleObject(m_hThread, INFINITE);
	CloseHandle(m_hThread);
	CloseHandle(m_hQuitEvent);
	CloseHandle(m_hScanEvent);
}

void Scanner::ScanFolder(LPCTSTR pszFolder)
{
	ULONGLONG nSize = 0;
	TCHAR szFolder[MAX_PATH];
	_tcscpy(szFolder, pszFolder);
	PathAppend(szFolder, _T("*"));
	WIN32_FIND_DATA FindData;
	HANDLE hFind = FindFirstFile(szFolder, &FindData);
	if (hFind != INVALID_HANDLE_VALUE)
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
						_tcscpy(szFolder, pszFolder);
						PathAppend(szFolder, FindData.cFileName);
						m_pCallback->FoundFolder(szFolder);
					}
				}
				else
				{
					nSize += MakeULongLong(FindData.nFileSizeHigh, FindData.nFileSizeLow);
				}
			}
		} while (FindNextFile(hFind, &FindData));
		FindClose(hFind);
	}

	m_pCallback->GotScanResult(pszFolder, nSize);
}

void Scanner::Wakeup()
{
	SetEvent(m_hScanEvent);
}

bool Scanner::GetAnItemFromTheQueue(LPTSTR pszFolder)
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
		if (m_pCallback->GetNextScanFolder(pszFolder))
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

	TCHAR szFolder[MAX_PATH];
	while ((pScanner->GetAnItemFromTheQueue(szFolder)))
	{
		pScanner->ScanFolder(szFolder);
	}

	return 0;
}
