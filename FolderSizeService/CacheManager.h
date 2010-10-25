#pragma once
#include "Cache.h"
#include "EventLog.h"

// Handles creation/deletion of Cache objects
// Resolves volumes from paths and dispatches onto the right Cache obj
class CacheManager : protected ICacheCallback
{
public:
	CacheManager(SERVICE_STATUS_HANDLE hSS);
	~CacheManager();

	bool GetInfoForFolder(const Path& path, FOLDERINFO2& nSize);
	void GetUpdateFolders(const Path& path, Strings& strsFoldersToUpdate);
	void EnableScanners(bool bEnable);
	void DeviceRemoveEvent(PDEV_BROADCAST_HANDLE pdbh);
	void ParamChange();

protected:
	bool DriveTypeEnabled(int type);
	Cache* GetCacheForFolder(const Path& pathVolume);

	// ICacheCallback
	virtual void KillMe(Cache* pExpiredCache);

	SERVICE_STATUS_HANDLE m_hSS;
	int m_ScanDriveTypes;

	CRITICAL_SECTION m_cs;
	typedef std::map<Path, std::pair<Cache*, HDEVNOTIFY> > MapType;
	MapType m_Map;
};
