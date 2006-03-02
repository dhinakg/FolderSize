#include "StdAfx.h"
#include "FolderManager.h"
#include "Folder.h"


FolderManager::FolderManager(const Path& pathVolume)
{
	m_pFolderRoot = new CacheFolder(this, NULL, pathVolume);
	m_Map.SetAt(pathVolume, m_pFolderRoot);
}

FolderManager::~FolderManager()
{
	delete m_pFolderRoot;
}

CacheFolder* FolderManager::GetFolderForPath(const Path& path, bool bCreate)
{
	CacheFolder* pFolder = NULL;
	if (!m_Map.Lookup(path, pFolder))
	{
		if (bCreate)
		{
			pFolder = CreateNewFolder(path);
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

void FolderManager::ChangeFolderPath(CacheFolder* pFolder, const Path& pathNew)
{
	m_Map.RemoveKey(pFolder->GetPath());
	m_Map.SetAt(pathNew, pFolder);
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

CacheFolder* FolderManager::CreateNewFolder(const Path& path)
{
	// don't want to have a folder object if we can't read it,
	// and we don't want NTFS junctions
	DWORD dwAttributes = GetFileAttributes(path.GetLongAPIRepresentation().c_str());
	if (dwAttributes == INVALID_FILE_ATTRIBUTES || dwAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		return NULL;

	// create parent nodes recursively
	Path pathParent = path.GetParent();
	CacheFolder* pParentFolder;
	if (!m_Map.Lookup(pathParent, pParentFolder))
	{
		pParentFolder = CreateNewFolder(pathParent);
		if (pParentFolder == NULL)
			return NULL;
	}

	// link it into the tree and initialize the fields
	return new CacheFolder(this, pParentFolder, path);
}
