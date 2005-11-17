#include "StdAfx.h"
#include "Folder.h"
#include "Scanner.h"
#include "FolderManager.h"


CacheFolder::CacheFolder(FolderManager* pManager, CacheFolder* pParent, LPCTSTR pszPath)
:
	m_pParent(pParent),
	m_pChild(NULL),
	m_pNextSibling(NULL),
	m_eStatus(FS_EMPTY),
	m_nDirtyChildren(0),
	m_nEmptyChildren(0),
	m_nLastCleanTime(0),
	m_bScanning(false),
	m_bIsScanValid(false),
	m_bNeedDisplayUpdate(true),
	m_pManager(pManager)
{
	RegisterStatusWithOptions();
	AddToParentsChildList();
	AddToParentsChildCounters(false);
	lstrcpyn(m_szPath, pszPath, MAX_PATH);

	// say hello to the world
	m_pManager->Register(this);
}

CacheFolder::~CacheFolder()
{
	// delete the children
	CacheFolder* pChild = m_pChild;
	while (pChild != NULL)
	{
		CacheFolder* pNextChild = pChild->m_pNextSibling;
		delete pChild;
		pChild = pNextChild;
	}

	for (CacheFolder* pParent = m_pParent; pParent != NULL; pParent = pParent->m_pParent)
	{
		pParent->m_nTotalSize -= m_nTotalSize;
	}

	RemoveFromParentsChildCounters(true);
	RemoveFromParentsChildList();
	UnregisterStatusWithOptions();

	// say goodbye to the world
	m_pManager->Unregister(this);
}

const FOLDERINFO& CacheFolder::GetTotalSize() const
{
	return m_nTotalSize;
}

LPCTSTR CacheFolder::GetPath() const
{
	return m_szPath;
}

CacheFolder::STATUS CacheFolder::GetStatus() const
{
	return m_eStatus;
}

UINT CacheFolder::GetDirtyChildren() const
{
	return m_nDirtyChildren;
}

UINT CacheFolder::GetEmptyChildren() const
{
	return m_nEmptyChildren;
}

void CacheFolder::DisplayUpdated()
{
	// bookkeeping for updating user display and scanner
	m_bNeedDisplayUpdate = false;
	m_pManager->UserRequested(m_pParent);
}

void CacheFolder::MakeLastChild()
{
	// if it's not already the last one
	if (m_pNextSibling != NULL)
	{
		// find the pointer to this node
		CacheFolder** ppFolder = &m_pParent->m_pChild;
		while (*ppFolder != this)
		{
			ppFolder = &(*ppFolder)->m_pNextSibling;
		}
		*ppFolder = m_pNextSibling;
		// insert this node
		while (*ppFolder != NULL)
		{
			ppFolder = &(*ppFolder)->m_pNextSibling;
		}
		*ppFolder = this;
		m_pNextSibling = NULL;
	}
}

CacheFolder* CacheFolder::GetNextScanFolder(const LONGLONG* pnRequestTime)
{
	// if this one needs to be scanned, and it hasn't yet been scanned since this request
	if (!m_bScanning && m_eStatus != FS_CLEAN && (pnRequestTime == NULL || m_nLastCleanTime < *pnRequestTime))
	{
		m_bScanning = true;
		m_bIsScanValid = true;
		return this;
	}

	for (CacheFolder* pChild = m_pChild; pChild != NULL; pChild = pChild->m_pNextSibling)
	{
		CacheFolder* pScanFolder = pChild->GetNextScanFolder(pnRequestTime);
		if (pScanFolder != NULL)
		{
			pChild->MakeLastChild();
			return pScanFolder;
		}
	}
	return NULL;
}

void CacheFolder::Dirty()
{
	// if it's empty, leave it empty
	if (m_eStatus == FS_CLEAN)
	{
		SetStatus(FS_DIRTY);
	}
	// if any scanner is currently scanning this, the result will not be valid
	m_bIsScanValid = false;
}

void CacheFolder::Clean(const FOLDERINFO& nSize)
{
	if (m_eStatus != FS_CLEAN)
	{
		if (m_nSize != nSize)
		{
			FOLDERINFO nChange = nSize;
			nChange -= m_nSize;
			m_nSize = nSize;
			for (CacheFolder* pFolder = this; pFolder != NULL; pFolder = pFolder->m_pParent)
			{
				pFolder->m_nTotalSize += nChange;
				pFolder->m_bNeedDisplayUpdate = true;
			}
		}

		// to eliminate disk thrashing, record the time so we won't scan if the request is older than this time
		LARGE_INTEGER nTime;
		QueryPerformanceCounter(&nTime);
		m_nLastCleanTime = nTime.QuadPart;

		// the scan is done
		m_bScanning = false;

		SetStatus(m_bIsScanValid ? FS_CLEAN : FS_DIRTY);
	}
}

void CacheFolder::GetChildrenToDisplay(Strings& strsFolders)
{
	for (CacheFolder* pChild = m_pChild; pChild != NULL; pChild = pChild->m_pNextSibling)
	{
		if (pChild->m_bNeedDisplayUpdate)
		{
			strsFolders.insert(pChild->m_szPath);
			pChild->m_bNeedDisplayUpdate = false;
		}
	}
}

void CacheFolder::SetStatus(STATUS eStatus)
{
	if (eStatus != m_eStatus)
	{
		RemoveFromParentsChildCounters(false);
		UnregisterStatusWithOptions();

		m_eStatus = eStatus;

		RegisterStatusWithOptions();
		AddToParentsChildCounters(false);
	}
}

void CacheFolder::Rename(LPCTSTR pszNewName)
{
	TCHAR szNewParent[MAX_PATH];
	_tcscpy(szNewParent, pszNewName);
	PathRemoveFileSpec(szNewParent);

	size_t nParentLength = PathFindFileName(m_szPath) - m_szPath;
	if (_tcsncmp(m_szPath, pszNewName, nParentLength) != 0)
	{
		RemoveFromParentsChildCounters(true);
		RemoveFromParentsChildList();
		m_pParent = m_pManager->GetFolderForPath(szNewParent, true);
		AddToParentsChildList();
		AddToParentsChildCounters(true);
	}
	
	InternalRename(pszNewName);
}

void CacheFolder::InternalRename(LPCTSTR pszNewName)
{
	// rename all the children
	for (CacheFolder* pChild = m_pChild; pChild != NULL; pChild = pChild->m_pNextSibling)
	{
		TCHAR szChildPath[MAX_PATH];
		_tcscpy(szChildPath, pszNewName);
		LPCTSTR pszChildName = pChild->GetPath() + _tcslen(m_szPath);
		PathAppend(szChildPath, pszChildName);
		pChild->InternalRename(szChildPath);
	}

	// update the lookup table in the manager
	m_pManager->ChangeFolderPath(this, pszNewName);

	_tcscpy(m_szPath, pszNewName);
}

void CacheFolder::AddToParentsChildList()
{
	if (m_pParent != NULL)
	{
		m_pNextSibling = m_pParent->m_pChild;
		m_pParent->m_pChild = this;
	}
}

void CacheFolder::RemoveFromParentsChildList()
{
	// unattach from the parent node
	if (m_pParent != NULL)
	{
		CacheFolder** ppFolder = &m_pParent->m_pChild;
		while (*ppFolder != this)
		{
			ppFolder = &(*ppFolder)->m_pNextSibling;
			assert(*ppFolder != NULL);
		}
		*ppFolder = m_pNextSibling;
	}
}

void CacheFolder::AddToParentsChildCounters(bool bIncludeSubtree)
{
	int nEmptyChildren = (bIncludeSubtree ? m_nEmptyChildren : 0) + ((m_eStatus == FS_EMPTY) ? 1 : 0);
	int nDirtyChildren = (bIncludeSubtree ? m_nDirtyChildren : 0) + ((m_eStatus == FS_DIRTY) ? 1 : 0);
	if (nEmptyChildren || nDirtyChildren)
	{
		for (CacheFolder* pFolder = m_pParent; pFolder != NULL; pFolder = pFolder->m_pParent)
		{
			if (nEmptyChildren)
			{
				if (!pFolder->m_nEmptyChildren)
				{
					pFolder->m_bNeedDisplayUpdate = true;
				}
				pFolder->m_nEmptyChildren += nEmptyChildren;
			}
			if (nDirtyChildren)
			{
				if (!pFolder->m_nDirtyChildren)
				{
					pFolder->m_bNeedDisplayUpdate = true;
				}
				pFolder->m_nDirtyChildren += nDirtyChildren;
			}
		}
	}
}

void CacheFolder::RemoveFromParentsChildCounters(bool bIncludeSubtree)
{
	int nEmptyChildren = (bIncludeSubtree ? m_nEmptyChildren : 0) + ((m_eStatus == FS_EMPTY) ? 1 : 0);
	int nDirtyChildren = (bIncludeSubtree ? m_nDirtyChildren : 0) + ((m_eStatus == FS_DIRTY) ? 1 : 0);
	if (nEmptyChildren || nDirtyChildren)
	{
		for (CacheFolder* pFolder = m_pParent; pFolder != NULL; pFolder = pFolder->m_pParent)
		{
			if (nEmptyChildren)
			{
				pFolder->m_nEmptyChildren -= nEmptyChildren;
				if (!pFolder->m_nEmptyChildren)
				{
					pFolder->m_bNeedDisplayUpdate = true;
				}
			}
			if (nDirtyChildren)
			{
				pFolder->m_nDirtyChildren -= nDirtyChildren;
				if (!pFolder->m_nDirtyChildren)
				{
					pFolder->m_bNeedDisplayUpdate = true;
				}
			}
		}
	}
}

void CacheFolder::RegisterStatusWithOptions()
{
/*
	switch (m_eStatus)
	{
	case FS_EMPTY:
		m_pManager->GetOptions()->IncrementFolderCounter(Options::FC_UNSCANNED);
		break;
	case FS_DIRTY:
		m_pManager->GetOptions()->IncrementFolderCounter(Options::FC_DIRTY);
		break;
	case FS_CLEAN:
		m_pManager->GetOptions()->IncrementFolderCounter(Options::FC_CLEAN);
		break;
	}
*/
}

void CacheFolder::UnregisterStatusWithOptions()
{
/*
	switch (m_eStatus)
	{
	case FS_EMPTY:
		m_pManager->GetOptions()->DecrementFolderCounter(Options::FC_UNSCANNED);
		break;
	case FS_DIRTY:
		m_pManager->GetOptions()->DecrementFolderCounter(Options::FC_DIRTY);
		break;
	case FS_CLEAN:
		m_pManager->GetOptions()->DecrementFolderCounter(Options::FC_CLEAN);
		break;
	}
*/
}
