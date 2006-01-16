#pragma once
#include "..\Pipe\FolderInfo.h"
#include "Scanner.h"
#include "Monitor.h"

class CacheFolder;
class FolderManager;
class Cache;

class ICacheCallback
{
public:
	virtual void KillMe(Cache* pExpiredCache) = 0;
};

class Cache : protected IScannerCallback, protected IMonitorCallback
{
public:
	Cache(LPCTSTR pszVolume, HANDLE hMonitor, ICacheCallback* pCallback);
	~Cache();

	// the CacheManager calls these
	void GetInfoForFolder(LPCTSTR pszFolder, FOLDERINFO2& nSize);
	void GetUpdateFoldersForFolder(LPCTSTR pszFolder, Strings& strsFoldersToUpdate);
	void EnableScanner(bool bEnable);
	HANDLE GetMonitoringHandle();

	// IScannerCallback
	virtual void FoundFolder(LPCTSTR pszFolder);
	virtual void GotScanResult(LPCTSTR pszFolder, const FOLDERINFO& nSize);
	virtual bool GetNextScanFolder(LPTSTR pszFolder);

	// IMonitorCallback
	virtual void PathChanged(LPCTSTR pszPath, LPCTSTR pszNewPath, FILE_EVENT fe);
	virtual void DirectoryError(DWORD dwError);

private:
	void Create();
	void Clear();
	void DoSyncScans(CacheFolder* pFolder);

	// the cache data and the lock for it
	TCHAR m_szVolume[MAX_PATH];
	CRITICAL_SECTION m_cs;
	FolderManager* m_pFolderManager;

	// helper classes
	Monitor* m_pMonitor;
	Scanner* m_pScanner;
	bool m_bScannerEnabled;

	// suicidal tendencies
	ICacheCallback* m_pCallback;
};
