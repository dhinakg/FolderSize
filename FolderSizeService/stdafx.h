#pragma once

#define _WIN32_WINNT 0x0501
#define _WIN32_IE    0x0500

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <dbt.h>
#include <pdh.h>

// ATL
#include <atlcoll.h>
#include <atlstr.h>

// CRT
#include <tchar.h>
#include <assert.h>

// STL
#include <vector>
#include <set>
using namespace std;

#include "..\Pipe\Pipe.h"

__inline ULONGLONG MakeULongLong(DWORD dwHigh, DWORD dwLow)
{
	return ((ULONGLONG)dwHigh << 32) | dwLow;
}

#define countof(x) (sizeof(x)/sizeof((x)[0]))
