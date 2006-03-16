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
	case FSC_SIZE:
		psci->scid.fmtid = CLSID_FolderSizeObj;
		psci->scid.pid = dwIndex;
		psci->vt = VT_BSTR;
		psci->fmt = LVCFMT_RIGHT;
		psci->cChars = 15;
		psci->csFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT;
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_TITLE, psci->wszTitle, MAX_COLUMN_NAME_LEN);
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_DESCRIPTION, psci->wszDescription, MAX_COLUMN_DESC_LEN);
		return S_OK;

	case FSC_FILES:
		psci->scid.fmtid = CLSID_FolderSizeObj;
		psci->scid.pid = dwIndex;
		psci->vt = VT_UI8;
		psci->fmt = LVCFMT_RIGHT;
		psci->cChars = 13;
		psci->csFlags = SHCOLSTATE_TYPE_INT;
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_FILES_TITLE, psci->wszTitle, MAX_COLUMN_NAME_LEN);
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_FILES_DESCRIPTION, psci->wszDescription, MAX_COLUMN_DESC_LEN);
		return S_OK;

	case FSC_FOLDERS:
		psci->scid.fmtid = CLSID_FolderSizeObj;
		psci->scid.pid = dwIndex;
		psci->vt = VT_UI8;
		psci->fmt = LVCFMT_RIGHT;
		psci->cChars = 13;
		psci->csFlags = SHCOLSTATE_TYPE_INT;
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_FOLDERS_TITLE, psci->wszTitle, MAX_COLUMN_NAME_LEN);
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_FOLDERS_DESCRIPTION, psci->wszDescription, MAX_COLUMN_DESC_LEN);
		return S_OK;

	case FSC_SIBLINGS:
		psci->scid.fmtid = CLSID_FolderSizeObj;
		psci->scid.pid = dwIndex;
		psci->vt = VT_UI8;
		psci->fmt = LVCFMT_RIGHT;
		psci->cChars = 13;
		psci->csFlags = SHCOLSTATE_TYPE_INT;
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_SIBLINGS_TITLE, psci->wszTitle, MAX_COLUMN_NAME_LEN);
		LoadString(_AtlBaseModule.GetResourceInstance(), IDS_COLUMN_SIBLINGS_DESCRIPTION, psci->wszDescription, MAX_COLUMN_DESC_LEN);
		return S_OK;
	}

	return S_FALSE;
}

ULONGLONG GetFileSize(LPCTSTR pFileName)
{
	// GetFileAttributesEx and CreateFile fail if the file is open by the system (for example, the swap file).
	// FindFirstFile seems to be the only reliable way to get the size.
	// e.g. the PageFile fails.

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
	// Assume folders won't get larger than 10^(2^4 - 4) = 10^16 bytes = 1 TB
	// Adjust if necessary
	const int N_PREFIX = 4;
	const int N_DIGITS_MAX = 1 << N_PREFIX;
	
	// Paranoia.
	if (uiBufSize <= N_PREFIX)
		return;

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

	// Retrieve the dimension of the size value:
   //
   // The dimension is an integer so that for each pair of sizes with
   // the same dimension the sort order of "string sort" is the same
   // as the intended sort order
   // Retrieve the string representation for the size
   // as usual.
   //
   // The dimension depends on the prefix.
   ULONGLONG nSize1;
   int nDimension;

   if (bCompact)
   {
      for (nDimension = 0, nSize1 = nSize / 1024; nSize1 > 0; nSize1 /= 10, nDimension++) ;
   }
   else
   {
      for (nDimension = 0, nSize1 = nSize; nSize1 >= 1024; nSize1 /= 1024, nDimension += 4) ;
      for (; nSize1 > 0; nSize1 /= 10, nDimension++ ) ;
   }

	// Insert invisible prefix according to the dimension to maintain
	// the correct sort order for the string representation of the
	// size.
	//
	// The prefix is a combination of N_PREFIX invisible characters
	// (0x20 and 0xA0). For sizes with the same dimension,
	// the same prefix is assigned. The prefix is constructed 
	// in a way that a prefix for a larger size is sorted after a 
	// prefix for a smaller size with string sort. So, when sorting
	// by our "Folder size" column, sizes are sorted according to 
	// their dimension.
	//
	// String representations for sizes of the same dimension maintain
	// the correct sort order. 
	// The combination of prefix and the size's string 
	// representation yields a (visually unchanged)
	// representation for the size while maintaining correct sort 
	// order.
	//
	// However, three restrictions apply:
	// 1. The values are four spaces longer, so the folder size
	//    column must not be too small.
	// 2. The ">" and "~" symbols that indicate pending calculation
	//    destroy the sort order if placed in front of the size.
	//    They are now appended to the size.
	// 3. Files and folders with the same string representation for
	//    their size but different sizes are sorted according to their
	//    name, not to their real size. For more accurate performance,
	//    use the "Folder Size Sort" column.
	for (int i1 = N_PREFIX; (i1--) > 0;)
	{
		if (nDimension & (1 << i1)) 
			*(pszBuff++) = 0xA0; // non-breaking space
		else
			*(pszBuff++) = 0x20; // space
	}
	
	// Our buffer is smaller now.
	uiBufSize -= N_PREFIX;

	// Retrieve the string representation for the size
	// as usual.
	if (bCompact)
	{
		StrFormatByteSize64(nSize, pszBuff, uiBufSize);
	}
	else
	{
		StrFormatKBSize(nSize, pszBuff, uiBufSize);
	}
}

bool GetInfoForFolder(LPCWSTR pszFile, FOLDERINFO2& nSize)
{
	bool bRet = false;

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
				FOLDERINFO2 Size;
/*
				LARGE_INTEGER nFrequency, nCount1, nCount2;
				QueryPerformanceFrequency(&nFrequency);
				QueryPerformanceCounter(&nCount1);
*/
				if (ReadGetFolderSize(hPipe, nSize))
				{
/*
					QueryPerformanceCounter(&nCount2);
					DWORD dwMilliseconds = (DWORD)((nCount2.QuadPart-nCount1.QuadPart)*1000/nFrequency.QuadPart);
					TCHAR szMessage[1024];
					wsprintf(szMessage, _T("Waited for a pipe: %d\n"), dwMilliseconds);
					OutputDebugString(szMessage);
*/
					bRet = true;
				}
			}
		}

		CloseHandle(hPipe);
	}

	return bRet;
}

bool GetFolderInfoToBuffer(LPCTSTR pszFolder, LPTSTR pszBuffer, DWORD cch)
{
	pszBuffer[0] = _T('\0');
	FOLDERINFO2 nSize;
	SetLastError(0);
	if (!GetInfoForFolder(pszFolder, nSize))
	{
#ifdef _DEBUG
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, pszBuffer, cch, NULL);
#else
		return false;
#endif
	}
	else
	{
		FormatSizeWithOption(nSize.nSize, pszBuffer, cch);

		LPWSTR psz = pszBuffer + wcslen(pszBuffer);
		switch (nSize.giff)
		{
		case GIFF_DIRTY:
			*(psz++) = L'~';
			break;
		case GIFF_SCANNING:
			*(psz++) = L'+';
			break;
		}
		*psz = 0;
	}
	return true;
}

STDMETHODIMP CFolderSizeObj::GetItemData(LPCSHCOLUMNID pscid, LPCSHCOLUMNDATA pscd, VARIANT *pvarData)
{
	if (pscid->fmtid == CLSID_FolderSizeObj)
	{
		switch (pscid->pid)
		{
		case FSC_SIZE:
			{
				WCHAR buffer[50];
				if (pscd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (!GetFolderInfoToBuffer(pscd->wszFile, buffer, sizeof(buffer)/sizeof(WCHAR)))
						return S_FALSE;
				}
				else
				{
					ULONGLONG nSize = GetFileSize(pscd->wszFile);
					FormatSizeWithOption(nSize, buffer, sizeof(buffer)/sizeof(WCHAR));
				}
				V_VT(pvarData) = VT_BSTR;
				V_BSTR(pvarData) = SysAllocString(buffer);
				return S_OK;
			}
			
		case FSC_FILES:
		case FSC_FOLDERS:
		case FSC_SIBLINGS:
			{
				// Show empty column for files
				if (!(pscd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					return S_FALSE;

				FOLDERINFO2 nSize;
	            if (!GetInfoForFolder(pscd->wszFile, nSize))
					return S_FALSE;

				V_VT(pvarData) = VT_UI8;
				switch (pscid->pid) {
				case FSC_FILES:
					V_UI8(pvarData) = nSize.nFiles;
					break;

				case FSC_FOLDERS:
					V_UI8(pvarData) = nSize.nFolders;
					break;

				case FSC_SIBLINGS:
					V_UI8(pvarData) = nSize.nFiles + nSize.nFolders;
					break;
				}
				return S_OK;
			}
		}
	}

	return S_FALSE;
}
