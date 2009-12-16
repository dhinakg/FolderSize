#pragma once

#include "Path.h"
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
	const Path& GetName() const;
	Path GetFullPath() const;
	STATUS GetStatus() const;
	UINT GetDirtyChildren() const;
	UINT GetEmptyChildren() const;
	CacheFolder* GetChild(const Path& name) const;

	// change the CacheFolder's status
	void Dirty();
	void Clean(const FOLDERINFO& nSize, LONGLONG nTime);

	void Rename(const Path& pathNew);

	// special actions
	void DisplayUpdated();
	CacheFolder* GetNextScanFolder(const LONGLONG* pnRequestTime);
	void GetChildrenToDisplay(Strings& strsFolders);

protected:

	// only the FolderManager friend can create Folder objects
	friend class FolderManager;
	CacheFolder(FolderManager* pManager, CacheFolder* pParent, const Path& path);

	// helper functions
	void SetStatus(STATUS eStatus);
	void MakeLastChild();
	void AddToParent(bool bFullyAttach);
	void RemoveFromParent(bool bFullyDetach);

	// folders organize themselves into a tree
	CacheFolder* m_pParent;
	CacheFolder* m_pChild;		 // down the structure
	CacheFolder* m_pNextSibling; // single linked list

	// folders keep a name keyed list of their children
	struct ltstr
	{
		bool operator()(LPCTSTR s1, LPCTSTR s2) const
		{
			return _tcscmp(s1, s2) < 0;
		}
	};
	typedef std::map<LPCTSTR, CacheFolder*, ltstr> ChildMap;
	ChildMap m_children;

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

	// name of this folder
	Path m_path;
};
