#pragma once
#include "Cache.h"

class CacheManager
{
public:
	CacheManager(SERVICE_STATUS_HANDLE hSS);
	~CacheManager();

	DWORD GetInfoForFolder(LPCTSTR pszFolder, ULONGLONG& nSize);
	void GetUpdateFoldersForBrowsedFolders(const Strings& strsFoldersBrowsed, Strings& strsFoldersToUpdate);
	void EnableScanners(bool bEnable);
	void DeviceRemoveEvent(PDEV_BROADCAST_HANDLE pdbh);

protected:
	SERVICE_STATUS_HANDLE m_hSS;
	Cache* GetCacheForFolder(LPCTSTR pszFolder, bool bCreate);
	Cache* m_pCaches[26];
	set<HDEVNOTIFY> m_RegisteredDeviceNotifications;
};
