#pragma once

#define _WIN32_WINNT 0x0501
#define _WIN32_IE    0x0500
#define _ATL_APARTMENT_THREADED

// for XP theme support
#define ISOLATION_AWARE_ENABLED 1

// ATL
#include <atlbase.h>
#include <atlcom.h>

// Windows
#include <commctrl.h> // for GUI support
#include <shellapi.h> // for shell notification icon
#include <shlobj.h>   // for IColumnProvider
#include <dbt.h>      // for WM_DEVICECHANGE messages

// CRT
#include <tchar.h>
#include <assert.h>

// STL
#include <string>
#ifdef LOG_REGKEY
#include <fstream>
#endif
