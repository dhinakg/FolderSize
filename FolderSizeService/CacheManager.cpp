#include "StdAfx.h"
#include "CacheManager.h"
#include "Cache.h"
#include "EventLog.h"
#include "../Settings/Settings.h"

CacheManager::CacheManager(SERVICE_STATUS_HANDLE hSS)
: m_hSS(hSS)
{
	InitializeCriticalSection(&m_cs);

	m_ScanDriveTypes = LoadScanDriveTypes();
}

CacheManager::~CacheManager()
{
	// clean up handle
	for (set<HDEVNOTIFY>::iterator i=m_RegisteredDeviceNotifications.begin(); i!=m_RegisteredDeviceNotifications.end(); i++)
	{
		if (!UnregisterDeviceNotification(*i))
		{
			EventLog::Instance().ReportError(TEXT("UnregisterDeviceNotification"), GetLastError());
		}
	}
	// cleanup caches for each volume
	POSITION pos = m_Map.GetStartPosition();
	while (pos != NULL)
	{
		CString strVolume;
		Cache* pCache;
		m_Map.GetNextAssoc(pos, strVolume, pCache);
		delete pCache;
	}
	DeleteCriticalSection(&m_cs);
}

// max cache id is a username and network share, each of which is probably less than MAX_PATH
#define MAX_CACHEID (MAX_PATH*2)

// pszFolder is the full folder path to make the cache id from
// pszCacheId must point to a buffer to store the cache id
// pszVolume will return a pointer into the cache id buffer specifying the volume
bool MakeCacheId(const Path& path, LPTSTR pszCacheId, LPTSTR& pszVolume)
{
	if (path.IsNetwork())
	{
		// for a network path, the CacheId will be the username followed by the computer and share name
		DWORD dwChars = MAX_PATH;
		if (!GetUserName(pszCacheId, &dwChars))
			return false;

		// insert a path separator after the username
		pszCacheId[dwChars - 1] = _T('\\');

		// and the volume will be after the separator
		pszVolume = pszCacheId + dwChars;
	}
	else
	{
		// for a local path, the CacheId is just the root of the path
		pszVolume = pszCacheId;
	}

	lstrcpy(pszVolume, path.GetVolume().c_str());

	return true;
}

// Do fancy stuff to support network
Cache* CacheManager::GetCacheForFolder(const Path& path, bool bCreate)
{
	// make a cache id which will be the volume of the folder, optionally preceded by a username
	TCHAR szCacheId[MAX_CACHEID];
	LPTSTR pszVolume;
	if (!MakeCacheId(path, szCacheId, pszVolume))
		return NULL;

	Cache* pCache = NULL;
	if (!m_Map.Lookup(szCacheId, pCache))
	{
		if (bCreate)
		{
			// if pszVolume is a network path, we should be impersonating the client right now
			HANDLE hMonitor = CreateFile(pszVolume, FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
									NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL);
			if (hMonitor != INVALID_HANDLE_VALUE)
			{
				// make a new cache
				pCache = new Cache(pszVolume, hMonitor, this);
				m_Map.SetAt(szCacheId, pCache);

				// register a device notification for a local cache
				if (!path.IsNetwork() && m_hSS != NULL)
				{
					DEV_BROADCAST_HANDLE dbh = {sizeof(dbh)};
					dbh.dbch_devicetype = DBT_DEVTYP_HANDLE;
					dbh.dbch_handle = hMonitor;
					HDEVNOTIFY hDevNotify = RegisterDeviceNotification(m_hSS, &dbh, DEVICE_NOTIFY_SERVICE_HANDLE);
					if (hDevNotify == NULL)
					{
						EventLog::Instance().ReportError(TEXT("RegisterDeviceNotification"), GetLastError());
					}
					else
					{
						m_RegisteredDeviceNotifications.insert(hDevNotify);
					}
				}
			}
		}
	}

	return pCache;
}

bool CacheManager::GetInfoForFolder(const Path& path, FOLDERINFO2& nSize)
{
	bool bScan = true;
	switch (path.GetDriveType())
	{
	case DRIVE_REMOVABLE:
		bScan = m_ScanDriveTypes & SCANDRIVETYPE_REMOVABLE ? true : false;
		break;
	case DRIVE_FIXED:
		bScan = m_ScanDriveTypes & SCANDRIVETYPE_LOCAL ? true : false;
		break;
	case DRIVE_REMOTE:
		bScan = m_ScanDriveTypes & SCANDRIVETYPE_NETWORK ? true : false;
		break;
	case DRIVE_CDROM:
		bScan = m_ScanDriveTypes & SCANDRIVETYPE_CD ? true : false;
		break;
	}

	bool bRes = false;
	if (bScan)
	{
		EnterCriticalSection(&m_cs);

		Cache* pCache = GetCacheForFolder(path, true);
		if (pCache != NULL)
		{
			bRes = pCache->GetInfoForFolder(path, nSize);
		}

		LeaveCriticalSection(&m_cs);
	}

	return bRes;
}

void CacheManager::GetUpdateFolders(const Path& path, Strings& strsFoldersToUpdate)
{
	EnterCriticalSection(&m_cs);

	Cache* pCache = GetCacheForFolder(path, false);
	if (pCache != NULL)
	{
		pCache->GetUpdateFoldersForFolder(path, strsFoldersToUpdate);
	}

	LeaveCriticalSection(&m_cs);
}

void CacheManager::EnableScanners(bool bEnable)
{
	EnterCriticalSection(&m_cs);

	POSITION pos = m_Map.GetStartPosition();
	while (pos != NULL)
	{
		CString strVolume;
		Cache* pCache;
		m_Map.GetNextAssoc(pos, strVolume, pCache);
		pCache->EnableScanner(bEnable);
	}

	LeaveCriticalSection(&m_cs);
}

void CacheManager::DeviceRemoveEvent(PDEV_BROADCAST_HANDLE pdbh)
{
	bool bFound = false;
	m_RegisteredDeviceNotifications.erase(pdbh->dbch_hdevnotify);

	EnterCriticalSection(&m_cs);

	POSITION pos = m_Map.GetStartPosition();
	while (pos != NULL)
	{
		POSITION nextpos = pos;
		CString strVolume;
		Cache* pCache;
		m_Map.GetNextAssoc(nextpos, strVolume, pCache);
		if (pCache->GetMonitoringHandle() == pdbh->dbch_handle)
		{
			delete pCache;
			m_Map.RemoveAtPos(pos);
			bFound = true;
			break;
		}
		pos = nextpos;
	}

	LeaveCriticalSection(&m_cs);

	if (!bFound)
	{
		EventLog::Instance().ReportError(TEXT("CacheManager::RemoveDevice"), GetLastError());
	}
	if (!UnregisterDeviceNotification(pdbh->dbch_hdevnotify))
	{
		EventLog::Instance().ReportError(TEXT("UnregisterDeviceNotification"), GetLastError());
	}
}

void CacheManager::ParamChange()
{
	// destroy caches that have been disabled
	int oldTypes = m_ScanDriveTypes;
	int newTypes = m_ScanDriveTypes = LoadScanDriveTypes();
	int disabledTypes = oldTypes & ~newTypes;

	EnterCriticalSection(&m_cs);

	// iterate through the caches
	POSITION pos = m_Map.GetStartPosition();
	while (pos != NULL)
	{
		POSITION nextpos = pos;
		CString strVolume;
		Cache* pCache;
		m_Map.GetNextAssoc(nextpos, strVolume, pCache);
		// check if this is one of the disabled types
		Path path = strVolume;
		bool bDelete = false;
		switch (path.GetDriveType())
		{
		case DRIVE_REMOVABLE:
			bDelete = disabledTypes & SCANDRIVETYPE_REMOVABLE ? true : false;
			break;
		case DRIVE_FIXED:
			bDelete = disabledTypes & SCANDRIVETYPE_LOCAL ? true : false;
			break;
		case DRIVE_REMOTE:
			bDelete = disabledTypes & SCANDRIVETYPE_NETWORK ? true : false;
			break;
		case DRIVE_CDROM:
			bDelete = disabledTypes & SCANDRIVETYPE_CD ? true : false;
			break;
		}
		if (bDelete)
		{
			delete pCache;
			m_Map.RemoveAtPos(pos);
		}
		pos = nextpos;
	}

	LeaveCriticalSection(&m_cs);
}

void CacheManager::KillMe(Cache* pExpiredCache)
{
	EnterCriticalSection(&m_cs);

	POSITION pos = m_Map.GetStartPosition();
	while (pos != NULL)
	{
		POSITION nextpos = pos;
		CString strVolume;
		Cache* pCache;
		m_Map.GetNextAssoc(nextpos, strVolume, pCache);
		if (pCache == pExpiredCache)
		{
			delete pCache;
			m_Map.RemoveAtPos(pos);
			break;
		}
		pos = nextpos;
	}

	LeaveCriticalSection(&m_cs);
}