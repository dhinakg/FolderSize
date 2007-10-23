#pragma once
#include "Path.h"

class CacheFolder;

// handles creation of Folder objects, and stores data common to all Folder objects
class FolderManager
{
public:
	FolderManager(const Path& pathVolume);
	~FolderManager();

	CacheFolder* GetFolderForPath(const Path& path, bool bCreate);
	CacheFolder* GetNextScanFolder();

	// these are for the folder objects to register and unregister themselves with the manager
	void Unregister(CacheFolder* pFolder);
	void UserRequested(CacheFolder* pFolder);

protected:
	CacheFolder* m_pFolderRoot;

	typedef struct REQUESTITEM
	{
		LONGLONG nTime;
		CacheFolder* pFolder;
	};
	typedef vector<REQUESTITEM> RequestStackType;
	RequestStackType m_RequestStack;
};
