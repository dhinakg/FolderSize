#include "StdAfx.h"
#include "Monitor.h"
#include "Resource.h"


Monitor::Monitor(int nDrive, IMonitorCallback* pCallback)
: m_nDrive(nDrive), m_pCallback(pCallback), m_hDirectory(NULL)
{
	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	TCHAR szRoot[MAX_PATH];
	PathBuildRoot(szRoot, m_nDrive);

	m_hDirectory = CreateFile(szRoot, FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL);

	ZeroMemory(&m_Overlapped, sizeof(OVERLAPPED));
	m_Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	DWORD dwThreadId;
	m_hMonitorThread = CreateThread(NULL, 0, MonitorThread, this, 0, &dwThreadId);
}

Monitor::~Monitor()
{
	SetEvent(m_hQuitEvent);
	WaitForSingleObject(m_hMonitorThread, INFINITE);
	CloseHandle(m_hMonitorThread);
	CloseHandle(m_hDirectory);
	CloseHandle(m_Overlapped.hEvent);
	CloseHandle(m_hQuitEvent);
}

HANDLE Monitor::GetFileHandle()
{
	return m_hDirectory;
}

DWORD WINAPI Monitor::MonitorThread(LPVOID lpParameter)
{
	// this thread will just keep waiting for directory change events,
	// and respond to them by dirtying the cache
	Monitor* pMonitor = (Monitor*)lpParameter;

	TCHAR szRoot[MAX_PATH];
	PathBuildRoot(szRoot, pMonitor->m_nDrive);

	TCHAR szRenameBuffer[MAX_PATH] = _T("");

	while (true)
	{
		DWORD dwBytesReturned;
		ReadDirectoryChangesW(pMonitor->m_hDirectory, pMonitor->m_Buffer, 4096, TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_SIZE,
			&dwBytesReturned, &pMonitor->m_Overlapped, NULL);

		HANDLE hEvents[2] = {pMonitor->m_hQuitEvent, pMonitor->m_Overlapped.hEvent};
		if (WaitForMultipleObjects(2, hEvents, FALSE, INFINITE) == WAIT_OBJECT_0)
		{
			break;
		}

		GetOverlappedResult(pMonitor->m_hDirectory, &pMonitor->m_Overlapped, &dwBytesReturned, TRUE);

		PFILE_NOTIFY_INFORMATION pfni = (PFILE_NOTIFY_INFORMATION)pMonitor->m_Buffer;
		while (true)
		{
			TCHAR szFile[MAX_PATH];
			_tcscpy(szFile, szRoot);
			lstrcpyn(szFile + 3, pfni->FileName, pfni->FileNameLength/sizeof(WCHAR) + 1);

			// can't do PathIsDirectory on this because OLD_NAMEs don't exist anymore!
			if (pfni->Action == FILE_ACTION_ADDED)
			{
				pMonitor->m_pCallback->PathChanged(szFile, NULL, IMonitorCallback::FE_ADDED);
			}
			else if (pfni->Action == FILE_ACTION_MODIFIED)
			{
				pMonitor->m_pCallback->PathChanged(szFile, NULL, IMonitorCallback::FE_CHANGED);
			}
			else if (pfni->Action == FILE_ACTION_RENAMED_OLD_NAME)
			{
				if (szRenameBuffer[0] == _T('\0'))
				{
					_tcscpy(szRenameBuffer, szFile);
				}
				else
				{
					assert(false);
				}
			}
			else if (pfni->Action == FILE_ACTION_RENAMED_NEW_NAME)
			{
				// there better be an old name stored
				assert(szRenameBuffer[0] != _T('\0'));
				pMonitor->m_pCallback->PathChanged(szRenameBuffer, szFile, IMonitorCallback::FE_RENAMED);
				// empty the buffer - we're ready for the next FILE_ACTION_RENAMED_OLD_NAME now
				szRenameBuffer[0] = _T('\0');
			}
			else if (pfni->Action == FILE_ACTION_REMOVED)
			{
				pMonitor->m_pCallback->PathChanged(szFile, NULL, IMonitorCallback::FE_REMOVED);
			}

			if (pfni->NextEntryOffset == 0)
				break;

			pfni = (PFILE_NOTIFY_INFORMATION)(((LPBYTE)pfni) + pfni->NextEntryOffset);
		}
	}

	return 0;
}
