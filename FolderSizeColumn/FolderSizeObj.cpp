#include "StdAfx.h"
#include "FolderSize.h"
#include "FolderSizeObj.h"
#include "Utility.h"
#include "Resource.h"
#include "ShellUpdate.h"
#include "..\Pipe\Pipe.h"


/////////////////////////////////////////////////////////////////////////////
// CFolderSizeObj

void CFolderSizeObj::ObjectMain(bool bStarting)
{/*
	static ShellUpdate* pShellUpdate = NULL;
	if (bStarting)
	{
		pShellUpdate = new ShellUpdate;
	}
	else
	{
		delete pShellUpdate;
	}*/
}

STDMETHODIMP CFolderSizeObj::Initialize(LPCSHCOLUMNINIT psci)
{
	HRESULT hr = E_FAIL;
/*
	IShellFolder* pShellFolder;
	if(SUCCEEDED(SHGetDesktopFolder(&pShellFolder)))
	{
		IShellBrowser* pShellBrowser;
		if(SUCCEEDED(pShellFolder->QueryInterface(IID_IShellBrowser, (LPVOID*)&pShellBrowser)))
		{
			MessageBox(NULL, L"got shell browser", L"ok", MB_OK);
			pShellBrowser->Release();
		}
		else
			MessageBox(NULL, L"nope", L"oh well", MB_OK);
		pShellFolder->Release();
	}
*/
/*
	// get the SHCOLUMNID for the standard Size column
	IShellFolder* pShellFolder;
	if(SUCCEEDED(SHGetDesktopFolder(&pShellFolder)))
	{
		// get the PIDL list of the folder
		LPITEMIDLIST pidl;
		if(SUCCEEDED(pShellFolder->ParseDisplayName(NULL, NULL,
			const_cast<LPOLESTR>(psci->wszFolder), NULL, &pidl, 0)))
		{
			// open that CacheFolder object
			IShellFolder2* pShellFolder2;
			if(SUCCEEDED(pShellFolder->BindToObject(pidl, NULL, IID_IShellFolder2, (LPVOID*)&pShellFolder2)))
			{
				if(SUCCEEDED(pShellFolder2->MapColumnToSCID(1, &scidSize)))
					hr = S_OK;
				pShellFolder2->Release();
			}
		}
		pShellFolder->Release();
	}
*/
	// we weren't able to merge with the standard Size column,
	// so make our own column ID

	hr = S_OK;

	return hr;
}

STDMETHODIMP CFolderSizeObj::GetColumnInfo(DWORD dwIndex, SHCOLUMNINFO *psci)
{
	switch (dwIndex)
	{
	case 0:
		psci->scid.fmtid = CLSID_FolderSizeObj;
		psci->scid.pid = 0;
		psci->vt = VT_BSTR;
		psci->fmt = LVCFMT_RIGHT;
		psci->cChars = 18;
		psci->csFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT;
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_TITLE, psci->wszTitle, MAX_COLUMN_NAME_LEN);
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_DESCRIPTION, psci->wszDescription, MAX_COLUMN_DESC_LEN);
		return S_OK;

	case 1:
		psci->scid.fmtid = CLSID_FolderSizeObj;
		psci->scid.pid = 1;
		psci->vt = VT_UI8;
		psci->fmt = LVCFMT_RIGHT;
		psci->cChars = 18;
		psci->csFlags = SHCOLSTATE_TYPE_INT;
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_SORT_TITLE, psci->wszTitle, MAX_COLUMN_NAME_LEN);
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_SORT_DESCRIPTION, psci->wszDescription, MAX_COLUMN_DESC_LEN);
		return S_OK;
	}

	return S_FALSE;
}

ULONGLONG GetFileSize(LPCTSTR pFileName)
{
	// GetFileAttributesEx and CreateFile fail if the file is open by the system (for example, the swap file).
	// FindFirstFile seems to be the only reliable way to get the size.

	ULONGLONG llFileSize = 0;
	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (GetFileAttributesEx(pFileName, GetFileExInfoStandard, &fad))
	{
		llFileSize = MakeULongLong(fad.nFileSizeHigh, fad.nFileSizeLow);
	}
	else
	{
		WIN32_FIND_DATA FindData;
		HANDLE hFindFile = FindFirstFile(pFileName, &FindData);
		if (hFindFile != INVALID_HANDLE_VALUE)
		{
			llFileSize = MakeULongLong(FindData.nFileSizeHigh, FindData.nFileSizeLow);
			FindClose(hFindFile);
		}
	}
	return llFileSize;
}

void FormatSizeWithOption(ULONGLONG nSize, LPTSTR pszBuff, UINT uiBufSize)
{
	bool bCompact = false;

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Brio\\FolderSize"), 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
	{
		DWORD dwType, dwData, cbData;
		cbData = sizeof(DWORD);
		if (RegQueryValueEx(hKey, TEXT("DisplayFormat"), NULL, &dwType, (LPBYTE)&dwData, &cbData) == ERROR_SUCCESS)
		{
			if (dwType == REG_DWORD)
			{
				if (dwData == 1)
				{
					bCompact = true;
				}
			}
		}
		RegCloseKey(hKey);
	}

	if (bCompact)
	{
		StrFormatByteSize64(nSize, pszBuff, uiBufSize);
	}
	else
	{
		StrFormatKBSize(nSize, pszBuff, uiBufSize);
	}
}

DWORD GetInfoForFolder(LPCWSTR pszFile, ULONGLONG& nSize)
{
	DWORD dwResult = 0;

	// try twice to connect to the pipe
	HANDLE hPipe = CreateFile(TEXT("\\\\.\\pipe\\") PIPE_NAME, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_PIPE_BUSY)
		{
			SetLastError(0);
			if (WaitNamedPipe(TEXT("\\\\.\\pipe\\") PIPE_NAME, 1000))
			{
				hPipe = CreateFile(TEXT("\\\\.\\pipe\\") PIPE_NAME, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			}
		}
	}

	if (hPipe != INVALID_HANDLE_VALUE)
	{
		if (WriteRequest(hPipe, PCR_GETFOLDERSIZE))
		{
			if (WriteString(hPipe, pszFile))
			{
				PIPE_REPLY_GETFOLDERSIZE gfs;
				ZeroMemory(&gfs, sizeof(gfs));
/*
				LARGE_INTEGER nFrequency, nCount1, nCount2;
				QueryPerformanceFrequency(&nFrequency);
				QueryPerformanceCounter(&nCount1);
*/
				if (ReadGetFolderSize(hPipe, gfs))
				{
/*
					QueryPerformanceCounter(&nCount2);
					DWORD dwMilliseconds = (DWORD)((nCount2.QuadPart-nCount1.QuadPart)*1000/nFrequency.QuadPart);
					TCHAR szMessage[1024];
					wsprintf(szMessage, _T("Waited for a pipe: %d\n"), dwMilliseconds);
					OutputDebugString(szMessage);
*/
					dwResult = gfs.dwResult;
					nSize = gfs.nSize;
				}
			}
		}

		CloseHandle(hPipe);
	}

	return dwResult;
}

void GetFolderInfoToBuffer(LPCTSTR pszFolder, LPTSTR pszBuffer, DWORD cch)
{
	pszBuffer[0] = _T('\0');
	ULONGLONG nSize;
	SetLastError(0);
	DWORD dwResult = GetInfoForFolder(pszFolder, nSize);
	if (GetLastError() != NO_ERROR)
	{
#ifdef _DEBUG
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, pszBuffer, cch, NULL);
#endif
	}
	else
	{
		if (dwResult & GFS_SUCCEEDED)
		{
			LPWSTR psz = pszBuffer;
			if (dwResult & GFS_DIRTY)
			{
				*(psz++) = L'~';
				*(psz++) = L' ';
			}
			else if (dwResult & GFS_EMPTY)
			{
				*(psz++) = L'>';
				*(psz++) = L' ';
			}
			FormatSizeWithOption(nSize, psz, cch-2);
		}
	}
}

STDMETHODIMP CFolderSizeObj::GetItemData(LPCSHCOLUMNID pscid, LPCSHCOLUMNDATA pscd, VARIANT *pvarData)
{
	if (pscid->fmtid == CLSID_FolderSizeObj)
	{
		switch (pscid->pid)
		{
		case 0:
			{
				WCHAR buffer[50];
				if (pscd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					GetFolderInfoToBuffer(pscd->wszFile, buffer, 50);
				}
				else
				{
					ULONGLONG nSize = GetFileSize(pscd->wszFile);
					FormatSizeWithOption(nSize, buffer, sizeof(buffer)/sizeof(WCHAR));
				}
/*
				// remove the " KB"
//				lstrcpy(buffer + lstrlen(buffer) - 3, L".0 KB");
//				buffer[lstrlen(buffer)-3] = L'\0';

				WCHAR buffer2[50];
//				lstrcpy(buffer2, L"                     "); // 21 spaces
				for (int i=0; i<21; i++)
				{
					buffer2[i] = 32;
				}
				buffer2[21] = 0;
				lstrcpy(buffer2 + lstrlen(buffer2) - lstrlen(buffer), buffer);
*/
				V_VT(pvarData) = VT_BSTR;
				V_BSTR(pvarData) = SysAllocString(buffer);
				return S_OK;
			}
			
		case 1:
			{
				ULONGLONG nSize = 0;
				if (pscd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					GetInfoForFolder(pscd->wszFile, nSize);
				}
				else
				{
					nSize = GetFileSize(pscd->wszFile);
				}
				V_VT(pvarData) = VT_UI8;
				V_UI8(pvarData) = nSize;
				return S_OK;
			}
		}
	}

	return S_FALSE;
}
