#pragma once

enum GETINFOFORFOLDER
{
	GIFF_SCANNING,
	GIFF_DIRTY,
	GIFF_CLEAN
};

struct FOLDERINFO {
	ULONGLONG nLogicalSize;
	ULONGLONG nPhysicalSize;
	ULONG nFiles;
	ULONG nFolders;


	FOLDERINFO() {
		ZeroMemory(this, sizeof(*this));
	}

	bool operator==(const FOLDERINFO& Info) const {
		return !memcmp(this, &Info, sizeof(*this));	}

	bool operator!=(const FOLDERINFO& Info) const {
		return !operator==(Info); }

	operator bool() {
		return nLogicalSize || nPhysicalSize || nFiles || nFolders;
	}

	FOLDERINFO& operator+=(const FOLDERINFO& Info) {
		nLogicalSize += Info.nLogicalSize;
		nPhysicalSize += Info.nPhysicalSize;
		nFiles += Info.nFiles;
		nFolders += Info.nFolders;
		return *this;
	}

	FOLDERINFO& operator-=(const FOLDERINFO& Info) {
		nLogicalSize -= Info.nLogicalSize;
		nPhysicalSize -= Info.nPhysicalSize;
		nFiles -= Info.nFiles;
		nFolders -= Info.nFolders;
		return *this;
	}
};

struct FOLDERINFO2 : public FOLDERINFO {
	GETINFOFORFOLDER giff;

	FOLDERINFO2() {
		ZeroMemory(this, sizeof(*this));
	}
};

