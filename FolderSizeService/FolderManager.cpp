#include "StdAfx.h"
#include "FolderManager.h"
#include "Folder.h"


FolderManager::FolderManager(int nDrive)
{
	TCHAR szRoot[MAX_PATH];
	PathBuildRoot(szRoot, nDrive);
	m_pFolderRoot = new CacheFolder(this, NULL, szRoot);
	m_Map.SetAt(szRoot, m_pFolderRoot);
}

FolderManager::~FolderManager()
{
	delete m_pFolderRoot;
}

CacheFolder* FolderManager::GetFolderForPath(LPCTSTR pszPath, bool bCreate)
{
	CacheFolder* pFolder = NULL;
	if (!m_Map.Lookup(pszPath, pFolder))
	{
		if (bCreate)
		{
			pFolder = CreateNewFolder(pszPath);
		}
	}
	return pFolder;
}

CacheFolder* FolderManager::GetNextScanFolder()
{
	CacheFolder* pFolder = NULL;
	while (pFolder == NULL && !m_RequestStack.empty())
	{
		REQUESTITEM& ri = m_RequestStack.back();
		pFolder = ri.pFolder->GetNextScanFolder(&ri.nTime);
		if (pFolder == NULL)
		{
			m_RequestStack.pop_back();
		}
	}
	return pFolder;
}

void FolderManager::Register(CacheFolder* pFolder)
{
	m_Map.SetAt(pFolder->GetPath(), pFolder);
}

void FolderManager::ChangeFolderPath(CacheFolder* pFolder, LPCTSTR pszNewPath)
{
	m_Map.RemoveKey(pFolder->GetPath());
	m_Map.SetAt(pszNewPath, pFolder);
}

void FolderManager::Unregister(CacheFolder* pFolder)
{
	// remove from the path map
	m_Map.RemoveKey(pFolder->GetPath());

	// and the request stack
	for (RequestStackType::iterator i=m_RequestStack.begin(); i!=m_RequestStack.end(); )
	{
		if (i->pFolder == pFolder)
		{
			i = m_RequestStack.erase(i);
		}
		else
		{
			i++;
		}
	}
}

void FolderManager::UserRequested(CacheFolder* pFolder)
{
	LARGE_INTEGER nTime;
	QueryPerformanceCounter(&nTime);
	bool bAddToStack = false;
	if (m_RequestStack.empty())
	{
		bAddToStack = true;
	}
	else
	{
		REQUESTITEM& ri = m_RequestStack.back();
		if (ri.pFolder == pFolder)
		{
			ri.nTime = nTime.QuadPart;
		}
		else
		{
			bAddToStack = true;
		}
	}
	if (bAddToStack)
	{
		REQUESTITEM ri;
		ri.nTime = nTime.QuadPart;
		ri.pFolder = pFolder;
		m_RequestStack.push_back(ri);
	}
}

CacheFolder* FolderManager::CreateNewFolder(LPCTSTR pszPath)
{
	// create parent nodes as necessary
	TCHAR szParentPath[MAX_PATH];
	_tcscpy(szParentPath, pszPath);
	PathRemoveFileSpec(szParentPath);
	CacheFolder* pParentFolder;
	if (!m_Map.Lookup(szParentPath, pParentFolder))
	{
		pParentFolder = CreateNewFolder(szParentPath);
	}
	// link it into the tree and initialize the fields
	CacheFolder* pFolder = new CacheFolder(this, pParentFolder, pszPath);
	
	return pFolder;
}
