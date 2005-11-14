// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once


#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT 0x500

#include <windows.h>
#include <cpl.h>
#include <prsht.h>
#include <shellapi.h>
#include <objbase.h>

// useful macros
#define countof(x) (sizeof(x)/sizeof((x)[0]))

// globals
extern HINSTANCE g_hInstance;