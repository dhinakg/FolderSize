#pragma once

class CacheFolder;

// handles creation of Folder objects, and stores data common to all Folder objects
class FolderManager
{
public:
	FolderManager(LPCTSTR pszVolume);
	~FolderManager();

	CacheFolder* GetFolderForPath(LPCTSTR pszPath, bool bCreate);
	CacheFolder* GetNextScanFolder();

	// these are for the folder objects to register and unregister themselves with the manager
	void Register(CacheFolder* pFolder);
	void ChangeFolderPath(CacheFolder* pFolder, LPCTSTR pszNewPath);
	void Unregister(CacheFolder* pFolder);
	void UserRequested(CacheFolder* pFolder);

protected:
	// internal function
	CacheFolder* CreateNewFolder(LPCTSTR pszPath);

	class CStringHashTraits : public CElementTraits<CString>
	{
	public:
		static ULONG Hash(const CString& str)
		{
			ULONG nHash = 0;
			for (int i=0; i<str.GetLength(); i++)
				nHash = (nHash<<5) + nHash + str[i];
			return nHash;
		}
	};

	typedef CAtlMap<CString, CacheFolder*, CStringHashTraits> MapType;
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
