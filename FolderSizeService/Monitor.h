#pragma once

#include "Path.h"

class IMonitorCallback
{
public:
	enum FILE_EVENT
	{
		FE_ADDED,
		FE_CHANGED,
		FE_REMOVED,
		FE_RENAMED
	};

	virtual void PathChanged(FILE_EVENT fe, const Path& path, const Path& pathNew) = 0;
	virtual void DirectoryError() = 0;
};

class Monitor
{
public:
	Monitor(const Path& pathVolume, HANDLE hFile, IMonitorCallback* pCallback);
	~Monitor();

protected:

	static DWORD WINAPI MonitorThread(LPVOID lpParameter);
	void MonitorThread();

	HANDLE m_hMonitorThread;
	DWORD m_dwMonitorThreadId;
	HANDLE m_hQuitEvent;
	HANDLE m_hDirectory;
	OVERLAPPED m_Overlapped;
	BYTE m_Buffer[4096];

	Path m_pathVolume;
	IMonitorCallback* m_pCallback;
};
