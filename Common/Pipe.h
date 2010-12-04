#pragma once
// Here's how a client communicates with the service:
// Connect to the pipe called PIPE_NAME.
// Send 2 bytes for PIPE_CLIENT_REQUEST.

// For PCR_GETFOLDERSIZE, send a size-prefixed string.
// Then read from the pipe a FOLDERINFO2 struct.

// For PCR_GETUPDATEDFOLDERS, send a size-prefixed list
// of size-prefixed strings, and then read back the same type.

// All sizes are unsigned shorts; all strings are wide characters.

#include <string>
#include <set>
#include "FolderInfo.h"

typedef std::set<std::wstring> Strings;

#define PIPE_NAME              TEXT("FolderSize")
#define PIPE_BUFFER_SIZE_IN    4096
#define PIPE_BUFFER_SIZE_OUT   4096
#define PIPE_DEFAULT_TIME_OUT  1000

enum PIPE_CLIENT_REQUEST
{
	PCR_GETFOLDERSIZE,
	PCR_GETUPDATEDFOLDERS
};

bool ReadRequest(HANDLE h, PIPE_CLIENT_REQUEST& pcr);
bool ReadString(HANDLE h, std::wstring& str);

bool WriteGetFolderSizeRequest(HANDLE h, const std::wstring& strFolder);
bool WriteGetUpdatedFoldersRequest(HANDLE h, const Strings& strsFolders);

bool ReadStringList(HANDLE h, Strings& strs);
bool WriteStringList(HANDLE h, const Strings& strs);
bool ReadGetFolderSize(HANDLE h, FOLDERINFO2& Size);
bool WriteGetFolderSize(HANDLE h, const FOLDERINFO2& Size);

bool GetInfoForFolder(LPCWSTR pszFile, FOLDERINFO2& nSize);
