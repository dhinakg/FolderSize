#pragma once
// Here's how a client communicates with the service:
// Connect to the pipe called PIPE_NAME.
// Send 4 bytes for PIPE_CLIENT_REQUEST.

// For PCR_GETFOLDERSIZE, send a NULL-terminated string.
// Then read from the pipe a GETFOLDERSIZE_REPLY struct.

// For PCR_GETUPDATEDFOLDERS, send a NULL-terminated list
// of NULL-terminated strings, and then read back the same type.

// All strings are wide characters.

#include <string>
#include <hash_set>

typedef stdext::hash_set<std::wstring> Strings;

#define PIPE_NAME              TEXT("FolderSize")
#define PIPE_BUFFER_SIZE_IN    4096
#define PIPE_BUFFER_SIZE_OUT   4096
#define PIPE_DEFAULT_TIME_OUT  1000

enum PIPE_CLIENT_REQUEST
{
	PCR_GETFOLDERSIZE,
	PCR_GETUPDATEDFOLDERS
};

#define GFS_SUCCEEDED   1
#define GFS_DIRTY       2
#define GFS_EMPTY       4

struct PIPE_REPLY_GETFOLDERSIZE
{
	DWORD dwResult;
	ULONGLONG nSize;
};

bool ReadRequest(HANDLE h, PIPE_CLIENT_REQUEST& pcr);
bool WriteRequest(HANDLE h, PIPE_CLIENT_REQUEST prc);
bool ReadString(HANDLE h, LPWSTR psz, UINT cch);
bool WriteString(HANDLE h, LPCWSTR psz);
bool ReadStringList(HANDLE h, Strings& strs);
bool WriteStringList(HANDLE h, Strings strs);
bool ReadGetFolderSize(HANDLE h, PIPE_REPLY_GETFOLDERSIZE& gfs);
bool WriteGetFolderSize(HANDLE h, const PIPE_REPLY_GETFOLDERSIZE& gfs);