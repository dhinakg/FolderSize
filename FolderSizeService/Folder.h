#pragma once

#include "..\Pipe\FolderInfo.h"

class Scanner;
class FolderManager;

class CacheFolder
{
public:
	~CacheFolder();

	enum STATUS
	{
		FS_EMPTY,
		FS_DIRTY,
		FS_CLEAN
	};

	// get info
	const FOLDERINFO& GetTotalSize() const;
	LPCTSTR GetPath() const;
	STATUS GetStatus() const;
	UINT GetDirtyChildren() const;
	UINT GetEmptyChildren() const;

	// change the CacheFolder's status
	void Dirty();
	void Clean(const FOLDERINFO& nSize);

	void Rename(LPCTSTR pszNewName);

	// special actions
	void DisplayUpdated();
	CacheFolder* GetNextScanFolder(const LONGLONG* pnRequestTime);
	void GetChildrenToDisplay(Strings& strsFolders);

protected:

	// only the FolderManager friend can create Folder objects
	friend class FolderManager;
	CacheFolder(FolderManager* pManager, CacheFolder* pParent, LPCTSTR pszPath);

	void InternalRename(LPCTSTR pszNewName);

	// helper functions
	void SetStatus(STATUS eStatus);
	void MakeLastChild();
	void AddToParentsChildList();
	void RemoveFromParentsChildList();
	void AddToParentsChildCounters(bool bIncludeSubtree);
	void RemoveFromParentsChildCounters(bool bIncludeSubtree);

	// folders organize themselves into a tree
	CacheFolder* m_pParent;
	CacheFolder* m_pChild;
	CacheFolder* m_pNextSibling;

	// the accuracy of nSize is determined by m_eStatus
	FOLDERINFO m_nSize;
	STATUS m_eStatus;

	bool m_bScanning;
	bool m_bIsScanValid;

	bool m_bNeedDisplayUpdate;
	LONGLONG m_nDisplayTime;

	LONGLONG m_nLastCleanTime;

	// This nSize struct contains the sum of all 
	// nSizes, nFiles and nFolders respectively in the tree rooted 
	// at this folder
	FOLDERINFO m_nTotalSize;

	// information about children
	UINT m_nDirtyChildren;
	UINT m_nEmptyChildren;

	// register and unregister with this manager
	FolderManager* m_pManager;

	// full path of the CacheFolder
	TCHAR m_szPath[MAX_PATH];
};
