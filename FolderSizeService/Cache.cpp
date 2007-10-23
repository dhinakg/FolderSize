#include "StdAfx.h"
#include "Cache.h"
#include "Monitor.h"
#include "Scanner.h"
#include "CacheManager.h"
#include "Folder.h"
#include "FolderManager.h"
#include "..\Pipe\Pipe.h"

LARGE_INTEGER g_nPerformanceFrequency = {0};

static void WarningEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection, LPCTSTR pszFunction)
{
	LARGE_INTEGER nCount1;
	QueryPerformanceCounter(&nCount1);
	if (!TryEnterCriticalSection(lpCriticalSection))
	{
		EnterCriticalSection(lpCriticalSection);
#ifdef _DEBUG
		LARGE_INTEGER nCount2;
		QueryPerformanceCounter(&nCount2);
		DWORD dwMilliseconds = (DWORD)((nCount2.QuadPart-nCount1.QuadPart)*1000/g_nPerformanceFrequency.QuadPart);
		TCHAR szMessage[1024];
		wsprintf(szMessage, _T("%s waited for %d ms for the cache\n"), pszFunction, dwMilliseconds);
		OutputDebugString(szMessage);
#endif
	}
}

Cache::Cache(const Path& pathVolume, HANDLE hMonitor, ICacheCallback* pCallback) :
	m_pathVolume(pathVolume), m_bScannerEnabled(true), m_pFolderManager(NULL), m_pScanner(NULL), m_pMonitor(NULL), m_pCallback(pCallback)
{
	InitializeCriticalSection(&m_cs);
	QueryPerformanceFrequency(&g_nPerformanceFrequency);

	m_pFolderManager = new FolderManager(m_pathVolume);
	m_pScanner = new Scanner(m_pathVolume, this);
	if (hMonitor != INVALID_HANDLE_VALUE)
		m_pMonitor = new Monitor(m_pathVolume, hMonitor, this);
}

Cache::~Cache()
{
	delete m_pMonitor;
	delete m_pScanner;

	// Don't need to protect deleting the Monitor and Scanner - protecting them will just cause deadlocks.
	// Need to protect the FolderManager, since another thread could try to look up folder info while folders are being deleted.
	WarningEnterCriticalSection(&m_cs, _T("~Cache"));
	delete m_pFolderManager;
	LeaveCriticalSection(&m_cs);
	DeleteCriticalSection(&m_cs);
}

bool Cache::GetInfoForFolder(const Path& path, FOLDERINFO2& nSize)
{
	WarningEnterCriticalSection(&m_cs, _T("GetInfoForFolder"));

	CacheFolder* pFolder = m_pFolderManager->GetFolderForPath(path, true);
	if (pFolder != NULL)
	{
		DoSyncScans(pFolder);

		(FOLDERINFO&)nSize = pFolder->GetTotalSize();

		// let the folder know that it's being displayed
		pFolder->DisplayUpdated();

		if (pFolder->GetStatus() == CacheFolder::FS_DIRTY || pFolder->GetDirtyChildren())
		{
			nSize.giff = GIFF_DIRTY;
		}
		else if (pFolder->GetStatus() == CacheFolder::FS_EMPTY || pFolder->GetEmptyChildren())
		{
			nSize.giff = GIFF_SCANNING;
		}
		else
		{
			nSize.giff = GIFF_CLEAN;
		}
	}

	LeaveCriticalSection(&m_cs);

	if (pFolder == NULL)
		return false;

	// if we're requesting info on unclean folders, ensure scanner is awake
	if (nSize.giff != GIFF_CLEAN && m_bScannerEnabled)
		m_pScanner->Wakeup();

	return true;
}

// IScanner callbacks

// The scanner is currently scanning the PARENT of pszFolder, and it
// found this subfolder.
void Cache::FoundFolder(const Path& path)
{
	WarningEnterCriticalSection(&m_cs, _T("FoundFolder"));
	m_pFolderManager->GetFolderForPath(path, true);
	LeaveCriticalSection(&m_cs);
}

void Cache::GotScanResult(const Path& path, const FOLDERINFO& nSize)
{
	WarningEnterCriticalSection(&m_cs,  _T("GotScanResult"));
	// clean the scanned folder
	CacheFolder* pFolder = m_pFolderManager->GetFolderForPath(path, false);
	if (pFolder != NULL)
	{
		pFolder->Clean(nSize);
	}
	LeaveCriticalSection(&m_cs);
}

bool Cache::GetNextScanFolder(Path& path)
{
	bool bRet = false;
	if (m_bScannerEnabled)
	{
		WarningEnterCriticalSection(&m_cs, _T("GetNextScanFolder"));
		// find the next folder to scan
		CacheFolder* pFolder = m_pFolderManager->GetNextScanFolder();
		if (pFolder != NULL)
		{
			path = pFolder->GetFullPath();
			bRet = true;
		}
		LeaveCriticalSection(&m_cs);
	}
	return bRet;
}

void Cache::PathChanged(FILE_EVENT fe, const Path& path, const Path& pathNew)
{
	// i'd rather not call PathIsDirectory(), since the disk could be out of sync
	// with the received file system notifications

	CacheFolder* pFolder = NULL;

	switch (fe)
	{
	case FE_ADDED:
		WarningEnterCriticalSection(&m_cs, _T("PathChanged FE_ADDED"));
		pFolder = m_pFolderManager->GetFolderForPath(path.GetParent(), false);
		if (pFolder != NULL)
		{
			pFolder->Dirty();
		}
		LeaveCriticalSection(&m_cs);
		break;

	case FE_CHANGED:
		WarningEnterCriticalSection(&m_cs, _T("PathChanged FE_CHANGED"));
		pFolder = m_pFolderManager->GetFolderForPath(path.GetParent(), false);
		if (pFolder != NULL)
		{
			pFolder->Dirty();
		}
		LeaveCriticalSection(&m_cs);
		break;

	case FE_RENAMED:
		WarningEnterCriticalSection(&m_cs, _T("PathChanged FE_RENAMED"));
		pFolder = m_pFolderManager->GetFolderForPath(path, false);
		if (pFolder != NULL)
		{
			pFolder->Rename(pathNew);
		}
		LeaveCriticalSection(&m_cs);
		break;

	case FE_REMOVED:
		WarningEnterCriticalSection(&m_cs, _T("PathChanged FE_REMOVED"));
		pFolder = m_pFolderManager->GetFolderForPath(path, false);
		if (pFolder == NULL)
		{
			pFolder = m_pFolderManager->GetFolderForPath(path.GetParent(), false);
			if (pFolder != NULL)
			{
				pFolder->Dirty();
			}
		}
		else
		{
			delete pFolder;
		}
		LeaveCriticalSection(&m_cs);
		break;
	}
}

void Cache::GetUpdateFoldersForFolder(const Path& path, Strings& strsFoldersToUpdate)
{
	WarningEnterCriticalSection(&m_cs, _T("GetUpdateFoldersForFolder"));
	if (m_pFolderManager != NULL)
	{
		CacheFolder* pFolder = m_pFolderManager->GetFolderForPath(path, false);
		if (pFolder != NULL)
		{
			pFolder->GetChildrenToDisplay(strsFoldersToUpdate);
		}
	}
	LeaveCriticalSection(&m_cs);
}

HANDLE Cache::GetMonitoringHandle()
{
	if (m_pMonitor == NULL)
		return INVALID_HANDLE_VALUE;
	return m_pMonitor->GetFileHandle();
}

void Cache::EnableScanner(bool bEnable)
{
	m_bScannerEnabled = bEnable;
	if (m_bScannerEnabled)
		m_pScanner->Wakeup();
}

#define SYNC_SCAN_TIME 200

// Scenario 1:
//
// 1. No Explorer Window open
// 2. Files are changing
// 3. Received notification about files changed
// 4. We don't want to scan things, the user is currently looking at
//
// Scenario 2:
// 
// 1. Explorer Window open
// 2. Something changed in the window
// 3. We wan't the updated size, but it hasn't scanned yet
//
// 2 options left:
// 4 a) If we can do it fast, return the correct result
// 4 b) It might take longer, just return *dirty* and let the Scanner do its job
void Cache::DoSyncScans(CacheFolder* pFolder)
{
	if (m_bScannerEnabled)
	{/*
		// we have SYNC_SCAN_TIME time to scan the folder, so get to it!
		LARGE_INTEGER nStartCount;
		QueryPerformanceCounter(&nStartCount);

		// ok, we'll be done when the current count gets up to nTimeoutCount
		LARGE_INTEGER nTimeoutCount;
		nTimeoutCount.QuadPart = nStartCount.QuadPart + g_nPerformanceFrequency.QuadPart * 1000 / SYNC_SCAN_TIME;

		while (true)
		{
			CacheFolder* pScanFolder = pFolder->GetNextScanFolder(NULL);
			if (pScanFolder == NULL)
			{
				break;
			}
			m_pScanner->ScanFolder(pScanFolder->GetPath(), nTimeoutCount.QuadPart);
		}
*/


		// if the number of unclean folders in the pFolder tree is small (NumberOfSyncScans),
		// clean them synchronously
		LARGE_INTEGER nStartCount, nCurrentCount;
		QueryPerformanceCounter(&nStartCount);
		QueryPerformanceCounter(&nCurrentCount);

		// try to do it in less than 200ms
		int nScans = 0;
		while ((nCurrentCount.QuadPart-nStartCount.QuadPart)*1000/g_nPerformanceFrequency.QuadPart < 200 &&
			((pFolder->GetStatus() != CacheFolder::FS_CLEAN ? 1 : 0) + pFolder->GetDirtyChildren() + pFolder->GetEmptyChildren()) <= 5)
		{
			CacheFolder* pScanFolder = pFolder->GetNextScanFolder(NULL);
			if (pScanFolder == NULL)
			{
				break;
			}
			m_pScanner->ScanFolder(pScanFolder->GetFullPath());
			QueryPerformanceCounter(&nCurrentCount);
			nScans ++;
		}
#ifdef _DEBUG
		TCHAR szMessage[1024];
		wsprintf(szMessage, _T("SyncScans: %d\n"), nScans);
		OutputDebugString(szMessage);
#endif

	}
}

void Cache::DirectoryError(DWORD dwError)
{
	// log an unexpected error
	if (dwError != ERROR_NETNAME_DELETED)
	{
		EventLog::Instance().ReportError(_T("Cache"), dwError);
	}

	// the monitor can't read the disk anymore, the cache is hopelessly outdated...
	m_pCallback->KillMe(this);
}
