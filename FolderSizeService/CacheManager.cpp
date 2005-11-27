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
			m_EventLog.ReportError(TEXT("UnregisterDeviceNotification"), GetLastError());
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

bool PathStripFolder(LPCTSTR pszFolder, LPTSTR pszVolume)
{
	if (PathIsUNC(pszVolume))
	{
		LPCTSTR pszEnd = PathFindNextComponent(pszFolder);
		if (pszEnd != NULL)
		{
			pszEnd = PathFindNextComponent(pszEnd);
			if (pszEnd != NULL)
			{
				lstrcpyn(pszVolume, pszFolder, (int)(pszEnd-pszFolder)+1);
				return true;
			}
		}
	}

	lstrcpy(pszVolume, pszFolder);
	return PathStripToRoot(pszVolume) != 0;
}

Cache* CacheManager::GetCacheForFolder(LPCTSTR pszFolder, bool bCreate)
{
	TCHAR szVolume[MAX_PATH];
	if (!PathStripFolder(pszFolder, szVolume))
		return NULL;

	Cache* pCache = NULL;
	if (!m_Map.Lookup(szVolume, pCache))
	{
		if (bCreate)
		{
			pCache = new Cache(szVolume);
			m_Map.SetAt(szVolume, pCache);
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
		HANDLE hDevice = INVALID_HANDLE_VALUE;
		pCache->GetInfoForFolder(pszFolder, nSize, hDevice);
		if (hDevice != INVALID_HANDLE_VALUE)
		{
			// the cache manager is returning the handle of a new cache we have to wait on
			DEV_BROADCAST_HANDLE dbh = {sizeof(dbh)};
			dbh.dbch_devicetype = DBT_DEVTYP_HANDLE;
			dbh.dbch_handle = hDevice;
			if (m_hSS != NULL)
			{
				HDEVNOTIFY hDevNotify = RegisterDeviceNotification(m_hSS, &dbh, DEVICE_NOTIFY_SERVICE_HANDLE);
				if (hDevNotify == NULL)
				{
					m_EventLog.ReportError(TEXT("RegisterDeviceNotification"), GetLastError());
				}
				else
				{
					m_RegisteredDeviceNotifications.insert(hDevNotify);
				}
			}
		}
		bRes = true;
	}

	LeaveCriticalSection(&m_cs);

	return bRes;
}

void CacheManager::GetUpdateFoldersForBrowsedFolders(const Strings& strsFoldersBrowsed, Strings& strsFoldersToUpdate)
{
	EnterCriticalSection(&m_cs);

	for (Strings::const_iterator i = strsFoldersBrowsed.begin(); i != strsFoldersBrowsed.end(); i++)
	{
		Cache* pCache = GetCacheForFolder(i->c_str(), false);
		if (pCache != NULL)
		{
			pCache->GetUpdateFoldersForFolder(i->c_str(), strsFoldersToUpdate);
		}
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
		CString strVolume;
		Cache* pCache;
		m_Map.GetNextAssoc(pos, strVolume, pCache);
		if (pCache->ClearIfMonitoringHandle(pdbh->dbch_handle))
		{
			bFound = true;
		}
	}

	LeaveCriticalSection(&m_cs);

	if (!bFound)
	{
		m_EventLog.ReportError(TEXT("CacheManager::RemoveDevice"), GetLastError());
	}
	if (!UnregisterDeviceNotification(pdbh->dbch_hdevnotify))
	{
		m_EventLog.ReportError(TEXT("UnregisterDeviceNotification"), GetLastError());
	}
}
