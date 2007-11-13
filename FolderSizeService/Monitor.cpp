#include "StdAfx.h"
#include "Monitor.h"


Monitor::Monitor(const Path& pathVolume, HANDLE hFile, IMonitorCallback* pCallback)
: m_hDirectory(hFile), m_pCallback(pCallback), m_hMonitorThread(NULL), m_pathVolume(pathVolume)
{
	ZeroMemory(&m_Overlapped, sizeof(OVERLAPPED));

	m_Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	DWORD dwThreadId;
	m_hMonitorThread = CreateThread(NULL, 0, MonitorThread, this, 0, &dwThreadId);
}

Monitor::~Monitor()
{
	SetEvent(m_hQuitEvent);
	// remove this or there will be a deadlock in the KillMe callback
	//WaitForSingleObject(m_hMonitorThread, INFINITE);
	CloseHandle(m_hMonitorThread);
	CloseHandle(m_Overlapped.hEvent);
	CloseHandle(m_hQuitEvent);

	// the Monitor owns the directory handle it's passed, so close it here
	CloseHandle(m_hDirectory);
}

HANDLE Monitor::GetFileHandle()
{
	return m_hDirectory;
}

DWORD WINAPI Monitor::MonitorThread(LPVOID lpParameter)
{
	Monitor* pMonitor = (Monitor*)lpParameter;
	pMonitor->MonitorThread();
	return 0;
}

void Monitor::MonitorThread()
{
	// this thread will just keep waiting for directory change events,
	// and respond to them by dirtying the cache
	Path pathRename;

	while (true)
	{
		// TODO maybe add NTFS journalling support
		DWORD dwBytesReturned;
		if (!ReadDirectoryChangesW(m_hDirectory, m_Buffer, 4096, TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_SIZE,
			&dwBytesReturned, &m_Overlapped, NULL))
		{
			m_pCallback->DirectoryError(GetLastError());
			return;
		}

		HANDLE hEvents[2] = {m_hQuitEvent, m_Overlapped.hEvent};
		if (WaitForMultipleObjects(2, hEvents, FALSE, INFINITE) == WAIT_OBJECT_0)
		{
			// exit normally
			return;
		}

		if (!GetOverlappedResult(m_hDirectory, &m_Overlapped, &dwBytesReturned, TRUE))
		{
			m_pCallback->DirectoryError(GetLastError());
			return;
		}

		PFILE_NOTIFY_INFORMATION pfni = (PFILE_NOTIFY_INFORMATION)m_Buffer;
		while (true)
		{
			Path pathNotifyFile(pfni->FileName, pfni->FileNameLength/sizeof(WCHAR));

			Path pathFile = m_pathVolume + pathNotifyFile;

			// can't do PathIsDirectory on this because OLD_NAMEs don't exist anymore!
			if (pfni->Action == FILE_ACTION_ADDED)
			{
				m_pCallback->PathChanged(IMonitorCallback::FE_ADDED, pathFile, Path());
			}
			else if (pfni->Action == FILE_ACTION_MODIFIED)
			{
				m_pCallback->PathChanged(IMonitorCallback::FE_CHANGED, pathFile, Path());
			}
			else if (pfni->Action == FILE_ACTION_RENAMED_OLD_NAME)
			{
				assert(pathRename.empty());
				pathRename = pathFile;
			}
			else if (pfni->Action == FILE_ACTION_RENAMED_NEW_NAME)
			{
				// there better be an old name stored
				assert(!pathRename.empty());
				m_pCallback->PathChanged(IMonitorCallback::FE_RENAMED, pathRename, pathFile);
				// empty the buffer - we're ready for the next FILE_ACTION_RENAMED_OLD_NAME now
				pathRename.clear();
			}
			else if (pfni->Action == FILE_ACTION_REMOVED)
			{
				m_pCallback->PathChanged(IMonitorCallback::FE_REMOVED, pathFile, Path());
			}

			if (pfni->NextEntryOffset == 0)
				break;

			pfni = (PFILE_NOTIFY_INFORMATION)(((LPBYTE)pfni) + pfni->NextEntryOffset);
		}
	}
}
