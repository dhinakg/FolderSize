#pragma once

class IMonitorCallback
{
public:
//	virtual void FileChanged(LPCTSTR pszFile, DWORD dwAction) = 0;

	enum FILE_EVENT
	{
		FE_ADDED,
		FE_CHANGED,
		FE_REMOVED,
		FE_RENAMED
	};

	virtual void PathChanged(LPCTSTR pszPath, LPCTSTR pszNewPath, FILE_EVENT fe) = 0;
//	virtual void FolderContentsChanged(LPCTSTR pszFolder);
//	virtual void FolderDeleted(LPCTSTR pszFolder);
//	virtual void FolderRenamed(LPCTSTR pszOldName, LPCTSTR pszNewName);
};

class Monitor
{
public:
	Monitor(int nDrive, IMonitorCallback* pCallback);
	~Monitor();

	HANDLE GetFileHandle();

protected:

	static DWORD WINAPI MonitorThread(LPVOID lpParameter);

	HANDLE m_hMonitorThread;
	HANDLE m_hQuitEvent;
	HANDLE m_hDirectory;
	OVERLAPPED m_Overlapped;
	BYTE m_Buffer[4096];

	int m_nDrive;
	IMonitorCallback* m_pCallback;
};
