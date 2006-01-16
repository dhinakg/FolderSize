#include "StdAfx.h"
#include "CacheManager.h"
#include "Cache.h"
#include "EventLog.h"

CacheManager::CacheManager(SERVICE_STATUS_HANDLE hSS)
: m_hSS(hSS)
{
	InitializeCriticalSection(&m_cs);
}

CacheManager::~CacheManager()
{
	for (set<HDEVNOTIFY>::iterator i=m_RegisteredDeviceNotifications.begin(); i!=m_RegisteredDeviceNotifications.end(); i++)
	{
		if (!UnregisterDeviceNotification(*i))
		{
			EventLog::Instance().ReportError(TEXT("UnregisterDeviceNotification"), GetLastError());
		}
	}
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
// bIsUNC returns whether or not it's a network path
bool MakeCacheId(LPCTSTR pszFolder, LPTSTR pszCacheId, LPTSTR& pszVolume, bool& bIsUNC)
{
	bIsUNC = PathIsUNC(pszFolder) != 0;
	if (bIsUNC)
	{
		// for a network path, the CacheId will be the username followed by the computer and share name
		DWORD dwChars = MAX_PATH;
		if (!GetUserName(pszCacheId, &dwChars))
			return false;
		
		// the volume starts after the username
		pszVolume = pszCacheId + dwChars - 1;

		// skip ahead 3 components
		LPCTSTR pszEnd = pszFolder;
		for (int i=0; i<3; i++)
		{
			pszEnd = PathFindNextComponent(pszEnd);
			if (pszEnd == NULL)
				return false;
		}
		// and only copy up to the third component
		lstrcpyn(pszVolume, pszFolder, (int)(pszEnd-pszFolder)+1);
		PathRemoveBackslash(pszVolume);
	}
	else
	{
		// for a local path, the CacheId is just the root of the path
		lstrcpy(pszCacheId, pszFolder);
		if (!PathStripToRoot(pszCacheId))
			return false;
		pszVolume = pszCacheId;
	}
	return true;
}

Cache* CacheManager::GetCacheForFolder(LPCTSTR pszFolder, bool bCreate)
{
	// make a cache id which will be the volume of the folder, optionally preceded by a username
	TCHAR szCacheId[MAX_CACHEID];
	LPTSTR pszVolume;
	bool bIsUNC;
	if (!MakeCacheId(pszFolder, szCacheId, pszVolume, bIsUNC))
		return NULL;

	Cache* pCache = NULL;
	if (!m_Map.Lookup(szCacheId, pCache))
	{
		if (bCreate)
		{
			// if it's a drive that has a letter...
			HANDLE hMonitor = INVALID_HANDLE_VALUE;
//			if (PathGetDriveNumber(pszFolder) >= 0)
			{
				hMonitor = CreateFile(pszVolume, FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
										NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL);
				if (hMonitor != INVALID_HANDLE_VALUE)
				{
					if (!bIsUNC && m_hSS != NULL)
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
					pCache = new Cache(pszVolume, hMonitor, this);
					m_Map.SetAt(szCacheId, pCache);
				}
			}
		}
	}

	return pCache;
}

bool CacheManager::GetInfoForFolder(LPCTSTR pszFolder, FOLDERINFO2& nSize)
{
	bool bRes = false;

	EnterCriticalSection(&m_cs);

	Cache* pCache = GetCacheForFolder(pszFolder, true);
	if (pCache != NULL)
	{
		pCache->GetInfoForFolder(pszFolder, nSize);
		bRes = true;
	}

	LeaveCriticalSection(&m_cs);

	return bRes;
}

void CacheManager::GetUpdateFolders(LPCTSTR pszFolder, Strings& strsFoldersToUpdate)
{
	EnterCriticalSection(&m_cs);

	Cache* pCache = GetCacheForFolder(pszFolder, false);
	if (pCache != NULL)
	{
		pCache->GetUpdateFoldersForFolder(pszFolder, strsFoldersToUpdate);
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