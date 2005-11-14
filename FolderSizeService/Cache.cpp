#include "StdAfx.h"
#include "Cache.h"
#include "Monitor.h"
#include "Scanner.h"
#include "CacheManager.h"
#include "Folder.h"
#include "FolderManager.h"
#include "..\Pipe\Pipe.h"


LARGE_INTEGER g_nPerformanceFrequency = {0};

Cache::Cache(int nDrive) :
	m_nDrive(nDrive), m_bScannerEnabled(true), m_pFolderManager(NULL), m_pScanner(NULL), m_pMonitor(NULL)
{
	InitializeCriticalSection(&m_cs);
	QueryPerformanceFrequency(&g_nPerformanceFrequency);
}

void Cache::Create()
{
	if (m_pFolderManager == NULL)
	{
		m_pFolderManager = new FolderManager(m_nDrive);
	}
	if (m_pScanner == NULL)
	{
		m_pScanner = new Scanner(this);
	}
	if (m_pMonitor == NULL)
	{
		m_pMonitor = new Monitor(m_nDrive, this);
	}
}

Cache::~Cache()
{
	Clear();
	DeleteCriticalSection(&m_cs);
}

static void WarningEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection, LPCTSTR pszFunction)
{
	LARGE_INTEGER nCount1, nCount2;
	QueryPerformanceCounter(&nCount1);
	if (!TryEnterCriticalSection(lpCriticalSection))
	{
		EnterCriticalSection(lpCriticalSection);
#ifdef _DEBUG
		QueryPerformanceCounter(&nCount2);
		DWORD dwMilliseconds = (DWORD)((nCount2.QuadPart-nCount1.QuadPart)*1000/g_nPerformanceFrequency.QuadPart);
		TCHAR szMessage[1024];
		wsprintf(szMessage, _T("%s waited for %d ms for the cache\n"), pszFunction, dwMilliseconds);
		OutputDebugString(szMessage);
#endif
	}
}

void Cache::Clear()
{
	if (m_pMonitor != NULL)
	{
		delete m_pMonitor;
		m_pMonitor = NULL;
	}
	if (m_pScanner != NULL)
	{
		delete m_pScanner;
		m_pScanner = NULL;
	}
	// Don't need to protect deleting the Monitor and Scanner - protecting them will just cause deadlocks.
	// Need to protect the FolderManager, since another thread could try to look up folder info while folders are being deleted.
	WarningEnterCriticalSection(&m_cs, _T("Clear"));
	if (m_pFolderManager != NULL)
	{
		delete m_pFolderManager;
		m_pFolderManager = NULL;
	}
	LeaveCriticalSection(&m_cs);
}

DWORD Cache::GetInfoForFolder(LPCTSTR pszFolder, ULONGLONG& nSize, HANDLE& hDevice)
{
	DWORD dwResult = 0;

	WarningEnterCriticalSection(&m_cs, _T("GetInfoForFolder"));

	if (m_pFolderManager == NULL)
	{
		Create();
		hDevice = m_pMonitor->GetFileHandle();
	}
	if (m_pFolderManager != NULL)
	{
		CacheFolder* pFolder = m_pFolderManager->GetFolderForPath(pszFolder, true);

		DoSyncScans(pFolder);

		nSize = pFolder->GetTotalSize();

		// let the folder know that it's being displayed
		pFolder->DisplayUpdated();

		dwResult = GFS_SUCCEEDED;
		if (pFolder->GetStatus() == CacheFolder::FS_DIRTY || pFolder->GetDirtyChildren())
		{
			dwResult |= GFS_DIRTY;
		}
		else if (pFolder->GetStatus() == CacheFolder::FS_EMPTY || pFolder->GetEmptyChildren())
		{
			dwResult |= GFS_EMPTY;
		}
	}

	LeaveCriticalSection(&m_cs);

	if (dwResult & (GFS_DIRTY|GFS_EMPTY))
	{
		// make sure the scanner is awake
		m_pScanner->Wakeup();
	}

	return dwResult;
}

// The scanner is currently scanning the PARENT of pszFolder, and it
// found this subfolder.
void Cache::FoundFolder(LPCTSTR pszFolder)
{
	WarningEnterCriticalSection(&m_cs, _T("FoundFolder"));
	CacheFolder* pFolder = m_pFolderManager->GetFolderForPath(pszFolder, true);
	LeaveCriticalSection(&m_cs);
}

void Cache::GotScanResult(LPCTSTR pszFolder, ULONGLONG nSize)
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
/*
void Cache::FileChanged(LPCTSTR pszFile, DWORD dwAction)
{
	// pszFile is a file that's changed, and szPath is the CacheFolder it's in
	WarningEnterCriticalSection(&m_cs, _T("FileChanged"));
	CacheFolder* pFolder = m_pFolderManager->GetFolderForPath(pszFile, false);
	if (pFolder != NULL)
	{
		// pszFile actually referred to a directory
		if (dwAction == FILE_ACTION_REMOVED)
		{
			delete pFolder;
		}
	}
	else
	{
		TCHAR szPath[MAX_PATH];
		_tcscpy(szPath, pszFile);
		PathRemoveFileSpec(szPath);
		pFolder = m_pFolderManager->GetFolderForPath(szPath, false);
		if (pFolder != NULL)
		{
			pFolder->Dirty();
		}
	}
	LeaveCriticalSection(&m_cs);
}
*/
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

bool Cache::ClearIfMonitoringHandle(HANDLE h)
{
	bool bCleared = false;
	WarningEnterCriticalSection(&m_cs, _T("ClearIfMonitoringHandle"));
	if (m_pMonitor != NULL)
	{
		if (m_pMonitor->GetFileHandle() == h)
		{
			Clear();
			bCleared = true;
		}
	}
	LeaveCriticalSection(&m_cs);
	return bCleared;
}

void Cache::EnableScanner(bool bEnable)
{
	m_bScannerEnabled = bEnable;
}

void Cache::DoSyncScans(CacheFolder* pFolder)
{
	if (m_bScannerEnabled)
	{
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
