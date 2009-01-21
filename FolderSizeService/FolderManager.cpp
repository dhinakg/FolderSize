#include "StdAfx.h"
#include "FolderManager.h"
#include "Folder.h"


FolderManager::FolderManager(const Path& pathVolume)
{
	m_pFolderRoot = new CacheFolder(this, NULL, pathVolume);
}

FolderManager::~FolderManager()
{
	delete m_pFolderRoot;
}

CacheFolder* FolderManager::GetFolderForPath(const Path& path, bool bCreate)
{
	assert (path.GetVolume() == m_pFolderRoot->GetName());

	PathSegmentIterator i = path.GetPathSegmentIterator();
	
	CacheFolder* pCurrentFolder = m_pFolderRoot;

	while (!i.AtEnd())
	{
		Path pathName = i.GetNextPathSegment();
		CacheFolder* pNextFolder = pCurrentFolder->GetChild(pathName);
		if (pNextFolder == NULL)
		{
			// this child doesn't exist yet
			// should we create it?
			if (!bCreate)
				return NULL;

			// don't want to have a folder object if we can't read it,
			// and we don't want NTFS junctions
			Path NewFullPath = pCurrentFolder->GetFullPath() + pathName;
			DWORD dwAttributes = GetFileAttributes(NewFullPath.GetLongAPIRepresentation().c_str());
			if (dwAttributes == INVALID_FILE_ATTRIBUTES || dwAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
				return NULL;

			pNextFolder = new CacheFolder(this, pCurrentFolder, pathName);
		}

		// advance
		pCurrentFolder = pNextFolder;
	}

	return pCurrentFolder;
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

void FolderManager::Unregister(CacheFolder* pFolder)
{
	// remove from the request stack
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

// Comes in from Folder::DisplayUpdated(), coming from CacheManager
void FolderManager::UserRequested(CacheFolder* pFolder)
{
	if (pFolder == NULL)
		pFolder = m_pFolderRoot;

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
