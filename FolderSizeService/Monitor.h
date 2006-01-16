#pragma once

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

	virtual void PathChanged(LPCTSTR pszPath, LPCTSTR pszNewPath, FILE_EVENT fe) = 0;
	virtual void DirectoryError(DWORD dwError) = 0;
};

class Monitor
{
public:
	Monitor(LPCTSTR pszVolume, HANDLE hFile, IMonitorCallback* pCallback);
	~Monitor();

	HANDLE GetFileHandle();

protected:

	static DWORD WINAPI MonitorThread(LPVOID lpParameter);
	void MonitorThread();

	HANDLE m_hMonitorThread;
	HANDLE m_hQuitEvent;
	HANDLE m_hDirectory;
	OVERLAPPED m_Overlapped;
	BYTE m_Buffer[4096];

	TCHAR m_szVolume[MAX_PATH];
	IMonitorCallback* m_pCallback;
};
