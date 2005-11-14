#pragma once
#include "Scanner.h"
#include "Monitor.h"

class CacheFolder;
class FolderManager;

class Cache : protected IScannerCallback, protected IMonitorCallback
{
public:
	Cache(int nDrive);
	~Cache();

	// the CacheManager calls these
	DWORD GetInfoForFolder(LPCTSTR pszFolder, ULONGLONG& nSize, HANDLE& hDevice);
	void GetUpdateFoldersForFolder(LPCTSTR pszFolder, Strings& strsFoldersToUpdate);
	void EnableScanner(bool bEnable);
	bool ClearIfMonitoringHandle(HANDLE h);

	// IScannerCallback
	virtual void FoundFolder(LPCTSTR pszFolder);
	virtual void GotScanResult(LPCTSTR pszFolder, ULONGLONG nSize);
	virtual bool GetNextScanFolder(LPTSTR pszFolder);

	// IMonitorCallback
	virtual void PathChanged(LPCTSTR pszPath, LPCTSTR pszNewPath, FILE_EVENT fe);

private:
	void Create();
	void Clear();
	void DoSyncScans(CacheFolder* pFolder);

	// the cache data and the lock for it
	int m_nDrive;
	CRITICAL_SECTION m_cs;
	FolderManager* m_pFolderManager;

	// helper classes
	Monitor* m_pMonitor;
	Scanner* m_pScanner;
	bool m_bScannerEnabled;
};
