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
	void Register(CacheFolder* pFolder);
	void ChangeFolderPath(CacheFolder* pFolder, const Path& pathNew);
	void Unregister(CacheFolder* pFolder);
	void UserRequested(CacheFolder* pFolder);

protected:
	// internal function
	CacheFolder* CreateNewFolder(const Path& path);

	class PathHashTraits : public CElementTraits<Path>
	{
	public:
		static ULONG Hash(const Path& path)
		{
			ULONG nHash = 0;
			for (size_t i=0; i<path.length(); i++)
				nHash = (nHash<<5) + nHash + path[i];
			return nHash;
		}
	};

	// TODO  Replace me with a map in each folder hashmap<wchar_t*, CacheFolder*>
	//       hashmap<local part of the path, CacheFolder*>
	//       on rename the map needs to be updated
	typedef CAtlMap<Path, CacheFolder*, PathHashTraits> MapType;
	MapType m_Map;
	CacheFolder* m_pFolderRoot;

	typedef struct REQUESTITEM
	{
		LONGLONG nTime;
		CacheFolder* pFolder;
	};
	typedef vector<REQUESTITEM> RequestStackType;
	RequestStackType m_RequestStack;
};
