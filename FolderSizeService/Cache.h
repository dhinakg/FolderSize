#pragma once
#include "..\Pipe\FolderInfo.h"
#include "Scanner.h"
#include "Monitor.h"
#include "Path.h"

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
	Cache(const Path& pathVolume, HANDLE hMonitor, ICacheCallback* pCallback);
	~Cache();

	// the CacheManager calls these
	void GetInfoForFolder(const Path& path, FOLDERINFO2& nSize);
	void GetUpdateFoldersForFolder(const Path& path, Strings& strsFoldersToUpdate);
	void EnableScanner(bool bEnable);
	HANDLE GetMonitoringHandle();

	// IScannerCallback
	virtual void FoundFolder(const Path& path);
	virtual void GotScanResult(const Path& path, const FOLDERINFO& nSize);
	virtual bool GetNextScanFolder(Path& path);

	// IMonitorCallback
	virtual void PathChanged(FILE_EVENT fe, const Path& path, const Path& pathNew);
	virtual void DirectoryError(DWORD dwError);

private:
	void Create();
	void Clear();
	void DoSyncScans(CacheFolder* pFolder);

	// the cache data and the lock for it
	Path m_pathVolume;
	CRITICAL_SECTION m_cs;
	FolderManager* m_pFolderManager;

	// helper classes
	Monitor* m_pMonitor;
	Scanner* m_pScanner;
	bool m_bScannerEnabled;

	// suicidal tendencies
	ICacheCallback* m_pCallback;
};
