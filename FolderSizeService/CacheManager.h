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
	Cache* GetCacheForFolder(const Path& pathVolume, bool bCreate);

	// ICacheCallback
	virtual void KillMe(Cache* pExpiredCache);

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
	// TODO change to stl map
	typedef CAtlMap<CString, Cache*, CStringHashTraits> MapType;
	MapType m_Map;
	CRITICAL_SECTION m_cs;

	int m_ScanDriveTypes;
};
