#pragma once
#include "Cache.h"

class CacheManager
{
public:
	CacheManager(SERVICE_STATUS_HANDLE hSS);
	~CacheManager();

	bool GetInfoForFolder(LPCTSTR pszFolder, FOLDERINFO2& nSize);
	void GetUpdateFoldersForBrowsedFolders(const Strings& strsFoldersBrowsed, Strings& strsFoldersToUpdate);
	void EnableScanners(bool bEnable);
	void DeviceRemoveEvent(PDEV_BROADCAST_HANDLE pdbh);

protected:
	Cache* GetCacheForFolder(LPCTSTR pszVolume, bool bCreate);

	SERVICE_STATUS_HANDLE m_hSS;
	set<HDEVNOTIFY> m_RegisteredDeviceNotifications;

	class CStringHashTraits : public CElementTraits<CString>
	{
	public:
		static ULONG Hash(const CString& str)
		{
			ULONG nHash = 0;
			for (int i=0; i<str.GetLength(); i++)
				nHash = (nHash<<5) + nHash + str[i];
			return nHash;
		}
	};

	typedef CAtlMap<CString, Cache*, CStringHashTraits> MapType;
	MapType m_Map;
	CRITICAL_SECTION m_cs;
};
