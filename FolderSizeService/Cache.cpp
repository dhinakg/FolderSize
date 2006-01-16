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

Cache::Cache(LPCTSTR pszVolume, HANDLE hMonitor, ICacheCallback* pCallback) :
	m_bScannerEnabled(true), m_pFolderManager(NULL), m_pScanner(NULL), m_pMonitor(NULL), m_pCallback(pCallback)
{
	lstrcpy(m_szVolume, pszVolume);
	InitializeCriticalSection(&m_cs);
	QueryPerformanceFrequency(&g_nPerformanceFrequency);

	m_pFolderManager = new FolderManager(m_szVolume);
	m_pScanner = new Scanner(m_szVolume, this);
	if (hMonitor != INVALID_HANDLE_VALUE)
		m_pMonitor = new Monitor(m_szVolume, hMonitor, this);
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

void Cache::GetInfoForFolder(LPCTSTR pszFolder, FOLDERINFO2& nSize)
{
	WarningEnterCriticalSection(&m_cs, _T("GetInfoForFolder"));

	CacheFolder* pFolder = m_pFolderManager->GetFolderForPath(pszFolder, true);

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

	LeaveCriticalSection(&m_cs);

	// if we're requesting info on unclean folders, ensure scanner is awake
	if (nSize.giff != GIFF_CLEAN)
		m_pScanner->Wakeup();
}

// The scanner is currently scanning the PARENT of pszFolder, and it
// found this subfolder.
void Cache::FoundFolder(LPCTSTR pszFolder)
{
	WarningEnterCriticalSection(&m_cs, _T("FoundFolder"));
	CacheFolder* pFolder = m_pFolderManager->GetFolderForPath(pszFolder, true);
	LeaveCriticalSection(&m_cs);
}

void Cache::GotScanResult(LPCTSTR pszFolder, const FOLDERINFO& nSize)
{
	WarningEnterCriticalSection(&m_cs,  _T("GotScanResult"));
	// clean the scanned folder
	CacheFolder* pFolder = m_pFolderManager->GetFolderForPath(pszFolder, false);
	if (pFolder != NULL)
	{
		pFolder->Clean(nSize);
	}
	LeaveCriticalSection(&m_cs);
}

bool Cache::GetNextScanFolder(LPTSTR pszFolder)
{
	bool bRet = false;
	if (m_bScannerEnabled)
	{
		WarningEnterCriticalSection(&m_cs, _T("GetNextScanFolder"));
		// find the next folder to scan
		CacheFolder* pFolder = m_pFolderManager->GetNextScanFolder();
		if (pFolder != NULL)
		{
			_tcscpy(pszFolder, pFolder->GetPath());
			bRet = true;
		}
		LeaveCriticalSection(&m_cs);
	}
	return bRet;
}

void Cache::PathChanged(LPCTSTR pszPath, LPCTSTR pszNewPath, FILE_EVENT fe)
{
	// i'd rather not call PathIsDirectory(), since the disk could be out of sync
	// with the received file system notifications

	TCHAR szFolder[MAX_PATH];
	CacheFolder* pFolder = NULL;

	switch (fe)
	{
	case FE_ADDED:
		_tcscpy(szFolder, pszPath);
		PathRemoveFileSpec(szFolder);
		WarningEnterCriticalSection(&m_cs, _T("PathChanged FE_ADDED"));
		pFolder = m_pFolderManager->GetFolderForPath(szFolder, false);
		if (pFolder != NULL)
		{
			pFolder->Dirty();
		}
		LeaveCriticalSection(&m_cs);
		break;

	case FE_CHANGED:
		assert(!PathIsDirectory(pszPath));
		_tcscpy(szFolder, pszPath);
		PathRemoveFileSpec(szFolder);
		WarningEnterCriticalSection(&m_cs, _T("PathChanged FE_CHANGED"));
		pFolder = m_pFolderManager->GetFolderForPath(szFolder, false);
		if (pFolder != NULL)
		{
			pFolder->Dirty();
		}
		LeaveCriticalSection(&m_cs);
		break;

	case FE_RENAMED:
		WarningEnterCriticalSection(&m_cs, _T("PathChanged FE_RENAMED"));
		pFolder = m_pFolderManager->GetFolderForPath(pszPath, false);
		if (pFolder != NULL)
		{
			pFolder->Rename(pszNewPath);
		}
		LeaveCriticalSection(&m_cs);
		break;

	case FE_REMOVED:
		WarningEnterCriticalSection(&m_cs, _T("PathChanged FE_REMOVED"));
		pFolder = m_pFolderManager->GetFolderForPath(pszPath, false);
		if (pFolder == NULL)
		{
			_tcscpy(szFolder, pszPath);
			PathRemoveFileSpec(szFolder);
			pFolder = m_pFolderManager->GetFolderForPath(szFolder, false);
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

void Cache::GetUpdateFoldersForFolder(LPCTSTR pszFolder, Strings& strsFoldersToUpdate)
{
	WarningEnterCriticalSection(&m_cs, _T("GetUpdateFoldersForFolder"));
	if (m_pFolderManager != NULL)
	{
		CacheFolder* pFolder = m_pFolderManager->GetFolderForPath(pszFolder, false);
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
}

#define SYNC_SCAN_TIME 200

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

		int nScans = 0;
		while ((nCurrentCount.QuadPart-nStartCount.QuadPart)*1000/g_nPerformanceFrequency.QuadPart < 200 &&
			((pFolder->GetStatus() != CacheFolder::FS_CLEAN ? 1 : 0) + pFolder->GetDirtyChildren() + pFolder->GetEmptyChildren()) <= 5)
		{
			CacheFolder* pScanFolder = pFolder->GetNextScanFolder(NULL);
			if (pScanFolder == NULL)
			{
				break;
			}
			m_pScanner->ScanFolder(pScanFolder->GetPath());
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
