#pragma once

enum GETINFOFORFOLDER
{
	GIFF_SCANNING,
	GIFF_DIRTY,
	GIFF_CLEAN
};

struct FOLDERINFO {
	ULONGLONG nSize;
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
		return nSize || nFiles || nFolders;
	}

	FOLDERINFO& operator+=(const FOLDERINFO& Info) {
		nSize += Info.nSize;
		nFiles += Info.nFiles;
		nFolders += Info.nFolders;
		return *this;
	}

	FOLDERINFO& operator-=(const FOLDERINFO& Info) {
		nSize -= Info.nSize;
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

