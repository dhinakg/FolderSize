#include "StdAfx.h"
#include "Folder.h"
#include "Scanner.h"
#include "FolderManager.h"


CacheFolder::CacheFolder(FolderManager* pManager, CacheFolder* pParent, const Path& path)
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
	m_pManager(pManager),
	m_path(path)
{
	AddToParent(true);
}

CacheFolder::~CacheFolder()
{
	RemoveFromParent(true);

	// delete the children
	CacheFolder* pChild = m_pChild;
	while (pChild != NULL)
	{
		CacheFolder* pNextChild = pChild->m_pNextSibling;

		// don't want the child node to waste time updating my stats, so detach it
		// from me (me = the child's parent) so it thinks it's a root node
		pChild->m_pParent = NULL;
		delete pChild;

		pChild = pNextChild;
	}

	// say goodbye to the world
	m_pManager->Unregister(this);
}

const FOLDERINFO& CacheFolder::GetTotalSize() const
{
	return m_nTotalSize;
}

const Path& CacheFolder::GetName() const
{
	return m_path;
}

Path CacheFolder::GetFullPath() const
{
	if (m_pParent == NULL)
		return m_path;
	return m_pParent->GetFullPath() + m_path;
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

CacheFolder* CacheFolder::GetChild(const Path& name) const
{
	ChildMap::const_iterator i = m_children.find(name.c_str());
	if (i == m_children.end())
		return NULL;
	return i->second;
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
		// find the end of the list
		while (*ppFolder != NULL)
		{
			ppFolder = &(*ppFolder)->m_pNextSibling;
		}
		// put this node at the end of the list
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

void CacheFolder::Clean(const FOLDERINFO& nSize, LONGLONG nTime)
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

		m_nLastCleanTime = nTime;

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
			strsFolders.insert(pChild->GetFullPath());
			pChild->m_bNeedDisplayUpdate = false;
		}
	}
}

void CacheFolder::SetStatus(STATUS eStatus)
{
	if (eStatus != m_eStatus)
	{
		RemoveFromParent(false);

		m_eStatus = eStatus;

		AddToParent(false);
	}
}

void CacheFolder::Rename(const Path& pathNew)
{
	// we could be just renaming a folder, or we could be moving an entire folder tree
	RemoveFromParent(true);

	m_path = pathNew.GetName();
	m_pParent = m_pManager->GetFolderForPath(pathNew.GetParent(), true);

	AddToParent(true);
}

void CacheFolder::AddToParent(bool bFullyAttach)
{
	if (m_pParent == NULL)
		return;

	if (bFullyAttach)
	{
		m_pNextSibling = m_pParent->m_pChild;
		m_pParent->m_pChild = this;

		m_pParent->m_children.insert(ChildMap::value_type(m_path.c_str(), this));
	}

	int nEmptyChildren = (bFullyAttach ? m_nEmptyChildren : 0) + ((m_eStatus == FS_EMPTY) ? 1 : 0);
	int nDirtyChildren = (bFullyAttach ? m_nDirtyChildren : 0) + ((m_eStatus == FS_DIRTY) ? 1 : 0);
	FOLDERINFO nTotalSize;
	if (bFullyAttach)
		nTotalSize = m_nTotalSize;

	if (nEmptyChildren || nDirtyChildren || nTotalSize)
	{
		for (CacheFolder* pFolder = m_pParent; pFolder != NULL; pFolder = pFolder->m_pParent)
		{
			if (nEmptyChildren)
			{
				if (!pFolder->m_nEmptyChildren)
					pFolder->m_bNeedDisplayUpdate = true;
				pFolder->m_nEmptyChildren += nEmptyChildren;
			}
			if (nDirtyChildren)
			{
				if (!pFolder->m_nDirtyChildren)
					pFolder->m_bNeedDisplayUpdate = true;
				pFolder->m_nDirtyChildren += nDirtyChildren;
			}
			if (nTotalSize)
			{
				pFolder->m_nTotalSize += nTotalSize;
				pFolder->m_bNeedDisplayUpdate = true;
			}
		}
	}
}

void CacheFolder::RemoveFromParent(bool bFullyDetach)
{
	if (m_pParent == NULL)
		return;

	if (bFullyDetach)
	{
		CacheFolder** ppFolder = &m_pParent->m_pChild;
		while (*ppFolder != this)
		{
			ppFolder = &(*ppFolder)->m_pNextSibling;
			assert(*ppFolder != NULL);
		}
		*ppFolder = m_pNextSibling;

		m_pParent->m_children.erase(m_path.c_str());
	}

	int nEmptyChildren = (bFullyDetach ? m_nEmptyChildren : 0) + ((m_eStatus == FS_EMPTY) ? 1 : 0);
	int nDirtyChildren = (bFullyDetach ? m_nDirtyChildren : 0) + ((m_eStatus == FS_DIRTY) ? 1 : 0);
	FOLDERINFO nTotalSize;
	if (bFullyDetach)
		nTotalSize = m_nTotalSize;

	if (nEmptyChildren || nDirtyChildren || nTotalSize)
	{
		for (CacheFolder* pFolder = m_pParent; pFolder != NULL; pFolder = pFolder->m_pParent)
		{
			if (nEmptyChildren)
			{
				pFolder->m_nEmptyChildren -= nEmptyChildren;
				if (!pFolder->m_nEmptyChildren)
					pFolder->m_bNeedDisplayUpdate = true;
			}
			if (nDirtyChildren)
			{
				pFolder->m_nDirtyChildren -= nDirtyChildren;
				if (!pFolder->m_nDirtyChildren)
					pFolder->m_bNeedDisplayUpdate = true;
			}
			if (nTotalSize)
			{
				pFolder->m_nTotalSize -= nTotalSize;
				pFolder->m_bNeedDisplayUpdate = true;
			}
		}
	}
}
