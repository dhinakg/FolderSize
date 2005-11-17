#include "StdAfx.h"
#include "CacheManager.h"
#include "Cache.h"
#include "DebugMessage.h"

CacheManager::CacheManager(SERVICE_STATUS_HANDLE hSS)
: m_hSS(hSS)
{
	for (int i=0; i<26; i++)
	{
		m_pCaches[i] = new Cache(i);
	}
}

CacheManager::~CacheManager()
{
	for (set<HDEVNOTIFY>::iterator i=m_RegisteredDeviceNotifications.begin(); i!=m_RegisteredDeviceNotifications.end(); i++)
	{
		if (!UnregisterDeviceNotification(*i))
		{
			PostDebugMessage(TEXT("UnregisterDeviceNotification"), GetLastError());
		}
	}
	for (int i=0; i<countof(m_pCaches); i++)
	{
		delete m_pCaches[i];
	}
}

bool CacheManager::GetInfoForFolder(LPCTSTR pszFolder, FOLDERINFO2& nSize)
{
	int nDrive = PathGetDriveNumber(pszFolder);
	if (nDrive < 0)
		return false;

	HANDLE hDevice = INVALID_HANDLE_VALUE;
	m_pCaches[nDrive]->GetInfoForFolder(pszFolder, nSize, hDevice);
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
				PostDebugMessage(TEXT("RegisterDeviceNotification"), GetLastError());
			}
			else
			{
				m_RegisteredDeviceNotifications.insert(hDevNotify);
			}
		}
	}
	return true;
}

void CacheManager::GetUpdateFoldersForBrowsedFolders(const Strings& strsFoldersBrowsed, Strings& strsFoldersToUpdate)
{
	for (Strings::const_iterator i = strsFoldersBrowsed.begin(); i != strsFoldersBrowsed.end(); i++)
	{
		int nDrive = PathGetDriveNumber(i->c_str());
		if (nDrive > 0)
		{
			m_pCaches[nDrive]->GetUpdateFoldersForFolder(i->c_str(), strsFoldersToUpdate);
		}
	}
}

void CacheManager::EnableScanners(bool bEnable)
{
	for (int i=0; i<countof(m_pCaches); i++)
	{
		if (m_pCaches[i] != NULL)
		{
			m_pCaches[i]->EnableScanner(bEnable);
		}
	}
}

void CacheManager::DeviceRemoveEvent(PDEV_BROADCAST_HANDLE pdbh)
{
	bool bFound = false;
	m_RegisteredDeviceNotifications.erase(pdbh->dbch_hdevnotify);
	for (int i=0; i<countof(m_pCaches); i++)
	{
		if (m_pCaches[i]->ClearIfMonitoringHandle(pdbh->dbch_handle))
		{
			bFound = true;
		}
	}
	if (!bFound)
	{
		PostDebugMessage(TEXT("CacheManager::RemoveDevice"), GetLastError());
	}
	if (!UnregisterDeviceNotification(pdbh->dbch_hdevnotify))
	{
		PostDebugMessage(TEXT("UnregisterDeviceNotification"), GetLastError());
	}
}
