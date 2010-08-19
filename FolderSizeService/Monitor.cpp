#include "StdAfx.h"
#include "Monitor.h"
#include "EventLog.h"

Monitor::Monitor(const Path& pathVolume, HANDLE hFile, IMonitorCallback* pCallback)
: m_hDirectory(hFile), m_pCallback(pCallback), m_hMonitorThread(NULL), m_pathVolume(pathVolume)
{
	ZeroMemory(&m_Overlapped, sizeof(OVERLAPPED));

	m_Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	m_hMonitorThread = CreateThread(NULL, 0, MonitorThread, this, 0, &m_dwMonitorThreadId);
}

Monitor::~Monitor()
{
	SetEvent(m_hQuitEvent);

	// Don't deadlock the monitor thread!
	// The monitor is always destroyed when the cache is destroyed,
	// and this can happen from different threads.
	// From an external thread such as a pipe thread, or a service thread,
	// or from our own internal thread, when an error is detected.
	// When the monitor is deleted from an external thread, we need to ensure
	// the thread has shut down before returning and deleting the instance.
	// When this is the 
	// In the case where our own thread deletes itself, we don't have to wait,
	// because we know we will not use any more member fields.
	if (GetCurrentThreadId() != m_dwMonitorThreadId)
		WaitForSingleObject(m_hMonitorThread, INFINITE);

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
			// log an unexpected error
			DWORD dwError = GetLastError();
			if (dwError != ERROR_NETNAME_DELETED)
			{
				EventLog::Instance().ReportError(_T("ReadDirectoryChangesW"), dwError);
			}
			m_pCallback->DirectoryError();
			return;
		}

		HANDLE hEvents[2] = {m_hQuitEvent, m_Overlapped.hEvent};
		if (WaitForMultipleObjects(2, hEvents, FALSE, INFINITE) == WAIT_OBJECT_0)
		{
			// exit normally
			return;
		}

		if (!GetOverlappedResult(m_hDirectory, &m_Overlapped, &dwBytesReturned, FALSE))
		{
			// log an unexpected error
			DWORD dwError = GetLastError();
			if (dwError != ERROR_NETNAME_DELETED)
			{
				EventLog::Instance().ReportError(_T("GetOverlappedResult"), dwError);
			}
			m_pCallback->DirectoryError();
			return;
		}

		if (dwBytesReturned == 0)
		{
			// Some notifications have been lost, so we have to kill the cache and start again.
#ifdef _DEBUG
			EventLog::Instance().ReportWarning(_T("ReadDirectoryChanges internal buffer overflowed"));
#endif
			m_pCallback->DirectoryError();
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
