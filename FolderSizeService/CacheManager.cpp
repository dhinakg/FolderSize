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

	// if there is ever an error accessing a drive, we always
	// want to fail the call, and not display a dialog
	SetErrorMode(SEM_FAILCRITICALERRORS);
}

CacheManager::~CacheManager()
{
	for (set<HDEVNOTIFY>::iterator itr = m_RegisteredDeviceNotifications.begin(); itr != m_RegisteredDeviceNotifications.end(); itr++)
	{
		if (!UnregisterDeviceNotification(*itr))
		{
			EventLog::Instance().ReportError(TEXT("UnregisterDeviceNotification"), GetLastError());
		}
	}

	// The caches are still alive, with their monitor threads (that call back with KillMe),
	// and scanners. Shut them down, using the critical section properly.
	Cache* pCache;
	do
	{
		pCache = NULL;

		EnterCriticalSection(&m_cs);
		if (!m_Map.empty())
		{
			pCache = m_Map.begin()->second;
			m_Map.erase(m_Map.begin());
		}
		LeaveCriticalSection(&m_cs);

		if (pCache)
		{
			pCache->Release();
		}
	} while (pCache);

	// now all the cache threads are dead

	DeleteCriticalSection(&m_cs);
}

// max cache id is a username and network share, each of which is probably less than MAX_PATH
#define MAX_CACHEID (MAX_PATH*2)

// path is the full folder path to make the cache id from
// pszCacheId must point to a buffer to store the cache id
// pszVolume will return a pointer into the cache id buffer specifying the volume
bool MakeCacheId(const Path& path, LPTSTR pszCacheId, LPTSTR& pszVolume)
{
	// the CacheId will be the username followed by either a volume, or a computer and share name
	DWORD dwChars = MAX_PATH;
	if (!GetUserName(pszCacheId, &dwChars))
		return false;

	// insert a path separator after the username
	pszCacheId[dwChars - 1] = _T('\\');

	// and the volume will be after the separator
	pszVolume = pszCacheId + dwChars;

	lstrcpy(pszVolume, path.GetVolume().c_str());

	return true;
}

bool CacheManager::DriveTypeEnabled(int type)
{
	switch (type)
	{
	case DRIVE_REMOVABLE:
		return m_ScanDriveTypes & SCANDRIVETYPE_REMOVABLE ? true : false;
	case DRIVE_FIXED:
		return m_ScanDriveTypes & SCANDRIVETYPE_LOCAL ? true : false;
	case DRIVE_REMOTE:
		return m_ScanDriveTypes & SCANDRIVETYPE_NETWORK ? true : false;
	case DRIVE_CDROM:
		return m_ScanDriveTypes & SCANDRIVETYPE_CD ? true : false;
	default:
		return false;
	}
}

// Return an AddRef'ed Cache
Cache* CacheManager::GetCacheForFolder(const Path& path)
{
	// make a cache id which will be the volume of the folder, preceded by a username
	TCHAR szCacheId[MAX_CACHEID];
	LPTSTR pszVolume;
	if (!MakeCacheId(path, szCacheId, pszVolume))
		return NULL;

	MapType::iterator itr = m_Map.find(szCacheId);
	if (itr != m_Map.end())
	{
		itr->second->AddRef();
		return itr->second;
	}

	// we should be impersonating the client right now
	HANDLE hMonitor = CreateFile(pszVolume, FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
							NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL);
	if (hMonitor == INVALID_HANDLE_VALUE)
		return NULL;

	// make a new cache
	Cache* pCache = new Cache(pszVolume, hMonitor, this);
	m_Map[szCacheId] = pCache;

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

	pCache->AddRef();
	return pCache;
}

bool CacheManager::GetInfoForFolder(const Path& path, FOLDERINFO2& nSize)
{
	// check that this drive type is enabled before locking the critical section,
	// and then check it again just to make sure it hasn't changed
	if (!DriveTypeEnabled(path.GetDriveType()))
		return false;

	Cache* pCache = NULL;

	EnterCriticalSection(&m_cs);
	if (DriveTypeEnabled(path.GetDriveType()))
		pCache = GetCacheForFolder(path);
	LeaveCriticalSection(&m_cs);

	if (pCache == NULL)
		return false;

	bool bRes = pCache->GetInfoForFolder(path, nSize);
	pCache->Release();
	return bRes;
}

void CacheManager::GetUpdateFolders(const Path& path, Strings& strsFoldersToUpdate)
{
	// check that this drive type is enabled before locking the critical section,
	// and then check it again just to make sure it hasn't changed
	if (!DriveTypeEnabled(path.GetDriveType()))
		return;

	Cache* pCache = NULL;

	EnterCriticalSection(&m_cs);
	if (DriveTypeEnabled(path.GetDriveType()))
		pCache = GetCacheForFolder(path);
	LeaveCriticalSection(&m_cs);

	if (pCache != NULL)
	{
		pCache->GetUpdateFoldersForFolder(path, strsFoldersToUpdate);
		pCache->Release();
	}
}

void CacheManager::EnableScanners(bool bEnable)
{
	EnterCriticalSection(&m_cs);

	for (MapType::iterator itr = m_Map.begin(); itr != m_Map.end(); itr++)
		itr->second->EnableScanner(bEnable);

	LeaveCriticalSection(&m_cs);
}

void CacheManager::DeviceRemoveEvent(PDEV_BROADCAST_HANDLE pdbh)
{
	bool bFound = false;
	m_RegisteredDeviceNotifications.erase(pdbh->dbch_hdevnotify);
	if (!UnregisterDeviceNotification(pdbh->dbch_hdevnotify))
	{
		EventLog::Instance().ReportError(TEXT("UnregisterDeviceNotification"), GetLastError());
	}

	EnterCriticalSection(&m_cs);

	for (MapType::iterator itr = m_Map.begin(); itr != m_Map.end(); itr++)
	{
		if (itr->second->GetMonitoringHandle() == pdbh->dbch_handle)
		{
			itr->second->Release();
			m_Map.erase(itr);
			bFound = true;
			break;
		}
	}

	LeaveCriticalSection(&m_cs);

	if (!bFound)
	{
		EventLog::Instance().ReportError(TEXT("CacheManager::RemoveDevice"), GetLastError());
	}
}

void CacheManager::ParamChange()
{
	// first reload our drive type configuration
	EnterCriticalSection(&m_cs);

	m_ScanDriveTypes = LoadScanDriveTypes();

	// delete any cache that is a disabled drive type
	for (MapType::iterator itr = m_Map.begin(); itr != m_Map.end(); )
	{
		if (!DriveTypeEnabled(itr->first.GetDriveType()))
		{
			itr->second->Release();
			itr = m_Map.erase(itr);
		}
		else
		{
			itr++;
		}
	}

	LeaveCriticalSection(&m_cs);
}

void CacheManager::KillMe(Cache* pExpiredCache)
{
	EnterCriticalSection(&m_cs);

	for (MapType::iterator itr = m_Map.begin(); itr != m_Map.end(); itr++)
	{
		if (itr->second == pExpiredCache)
		{
			m_Map.erase(itr);
			break;
		}
	}

	LeaveCriticalSection(&m_cs);

	pExpiredCache->Release();
}