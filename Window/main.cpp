#include "StdAfx.h"

HINSTANCE g_hInstance;
int g_nWindows = 0;

static bool GetShellWindows(IWebBrowser2**& pWebBrowsers, long& nCount)
{
	bool bSuccess = false;
	IShellWindows* psw;
	if (SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, NULL, CLSCTX_ALL, IID_IShellWindows, (void**)&psw)))
	{
		long lItemCount;
		if (SUCCEEDED(psw->get_Count(&lItemCount)))
		{
			nCount = 0;

			pWebBrowsers = new IWebBrowser2*[lItemCount];

			VARIANT varItem;
			V_VT(&varItem) = VT_I4;
			for (long i=0; i<lItemCount; i++)
			{
				V_I4(&varItem) = i;
				IDispatch* pDispatch;
				// Item can return S_OK even if there is no IDispatch
				if (SUCCEEDED(psw->Item(varItem, &pDispatch)) && pDispatch)
				{
					IWebBrowser2* pWebBrowser;
					if (SUCCEEDED(pDispatch->QueryInterface(IID_IWebBrowser2, (void**)&pWebBrowser)))
					{
						pWebBrowsers[nCount++] = pWebBrowser;
					}
					pDispatch->Release();
				}
			}

			bSuccess = true;
		}
		psw->Release();
	}
	return bSuccess;
}

static void FormatSizeWithOption(ULONGLONG nSize, LPTSTR pszBuff, UINT uiBufSize)
{
	// Retrieve the string representation for the size
	// as usual.
	switch (0)
	{
	case 1:
		StrFormatByteSize64(nSize, pszBuff, uiBufSize);
		break;
	case 2:
		_i64tot(nSize, pszBuff, 10);
		break;
	case 0:
	default:
		StrFormatKBSize(nSize, pszBuff, uiBufSize);
	}
}

static bool GetFolderInfoToBuffer(LPCTSTR pszFolder, ULONGLONG FOLDERINFO2::* sizeMember, LPTSTR pszBuffer, DWORD cch)
{
	pszBuffer[0] = _T('\0');
	FOLDERINFO2 nSize;
	SetLastError(0);
	if (!GetInfoForFolder(pszFolder, nSize))
	{
#ifdef _DEBUG
		// FORMAT_MESSAGE_MAX_WIDTH_MASK converts newline chars in the message to spaces, which is better for single-line output
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_MAX_WIDTH_MASK,
			NULL, GetLastError(), 0, pszBuffer, cch, NULL);
#else
		return false;
#endif
	}
	else
	{
		FormatSizeWithOption(nSize.*sizeMember, pszBuffer, cch);

		size_t len = wcslen(pszBuffer);
		if (len < cch - 1 && (nSize.giff == GIFF_DIRTY || nSize.giff == GIFF_SCANNING))
		{
			pszBuffer[len] = nSize.giff == GIFF_DIRTY ? L'~' : L'+';
			pszBuffer[len + 1] = L'\0';
		}
	}
	return true;
}
__inline ULONGLONG MakeULongLong(DWORD dwHigh, DWORD dwLow)
{
	return ((ULONGLONG)dwHigh << 32) | dwLow;
}

static bool GetLogicalFileSize(LPCTSTR pFileName, ULONGLONG& nSize)
{
	// GetFileAttributesEx and CreateFile fail if the file is open by the system (for example, the swap file).
	// GetFileAttributesEx also seems to fail for very large files on Novell Netware networks.
	// FindFirstFile seems to be the only reliable way to get the size.
	WIN32_FIND_DATA FindData;
	HANDLE hFindFile = FindFirstFile(pFileName, &FindData);
	if (hFindFile == INVALID_HANDLE_VALUE)
		return false;
	FindClose(hFindFile);
	nSize = MakeULongLong(FindData.nFileSizeHigh, FindData.nFileSizeLow);
	return true;
}



class FSWindow :
	public CWindowImpl<FSWindow, CWindow, CWinTraits<WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX, WS_EX_TOOLWINDOW> >,
	public IDispEventSimpleImpl<1, FSWindow, &DIID_DWebBrowserEvents2>
{
public:
	FSWindow(IWebBrowser2* pWebBrowser) : m_lv(NULL), m_pWebBrowser(pWebBrowser) {}

BEGIN_MSG_MAP(FSWindow)
	MESSAGE_HANDLER(WM_CREATE, OnCreate)
	MESSAGE_HANDLER(WM_SIZE, OnSize)
	MESSAGE_HANDLER(WM_TIMER, OnTimer)
	MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
END_MSG_MAP()

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	virtual void OnFinalMessage(HWND hwnd);

static _ATL_FUNC_INFO NavigateCompleteInfo;
static _ATL_FUNC_INFO QuitInfo;
BEGIN_SINK_MAP(FSWindow)
	SINK_ENTRY_INFO(1, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2, OnNavigateComplete, &NavigateCompleteInfo)
	SINK_ENTRY_INFO(1, DIID_DWebBrowserEvents2, DISPID_ONQUIT, OnQuit, &QuitInfo)
END_SINK_MAP()

	void __stdcall OnNavigateComplete(IDispatch *pDisp, VARIANT *URL);
	void __stdcall OnQuit();

private:
	void SetListToFolder();
	void InsertEnumItems(IFolderView* pfv, IShellFolder2* psf2);
	void AdjustSizeForList();

	HWND m_lv;
	CComPtr<IWebBrowser2> m_pWebBrowser;
	TCHAR m_szFolder[MAX_PATH * 4];
};

LRESULT FSWindow::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	RECT rc;
	GetClientRect(&rc);
	m_lv = CreateWindow(WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHAREIMAGELISTS,
		0, 0, rc.right-rc.left, rc.bottom-rc.top,
		m_hWnd, 0, (HINSTANCE)GetWindowLongPtr(GWLP_HINSTANCE), NULL);
	if (!m_lv)
		return -1;

	ListView_SetExtendedListViewStyle(m_lv, LVS_EX_FULLROWSELECT | LVS_EX_AUTOSIZECOLUMNS);

	LVCOLUMN column = {0};
	column.mask = LVCF_FMT | LVCF_TEXT;
	column.fmt = LVCFMT_LEFT;
	column.pszText = _T("Name");
	ListView_InsertColumn(m_lv, 0, &column);
	column.fmt = LVCFMT_RIGHT;
	column.pszText = _T("Size");
	ListView_InsertColumn(m_lv, 1, &column);

	SHFILEINFO shfi;
	HIMAGELIST himl = (HIMAGELIST)SHGetFileInfo(_T(""), FILE_ATTRIBUTE_DIRECTORY, &shfi, sizeof(shfi), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	if (himl == NULL)
		return -1;
	ListView_SetImageList(m_lv, himl, LVSIL_SMALL);

	SetListToFolder();
	if (FAILED(DispEventAdvise(m_pWebBrowser)))
		return -1;

	HRESULT hr = SetWindowTheme(m_lv, L"Explorer", NULL);

	return 0;
}

LRESULT FSWindow::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	::MoveWindow(m_lv, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
	return 0;
}

LRESULT FSWindow::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam == 1)
	{
		// connect to the server and get the folders to update
		// try twice to connect to the pipe
		HANDLE hPipe = CreateFile(TEXT("\\\\.\\pipe\\") PIPE_NAME, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

		if (hPipe == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() == ERROR_PIPE_BUSY)
			{
				if (WaitNamedPipe(TEXT("\\\\.\\pipe\\") PIPE_NAME, 1000))
				{
					hPipe = CreateFile(TEXT("\\\\.\\pipe\\") PIPE_NAME, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
				}
			}
		}

		if (hPipe != INVALID_HANDLE_VALUE)
		{
			Strings strsFoldersBrowsed;
			strsFoldersBrowsed.insert(m_szFolder);
			if (WriteGetUpdatedFoldersRequest(hPipe, strsFoldersBrowsed))
			{
				Strings strsFoldersToUpdate;
				if (ReadStringList(hPipe, strsFoldersToUpdate))
				{
					for (Strings::const_iterator i = strsFoldersToUpdate.begin(); i != strsFoldersToUpdate.end(); i++)
					{
						// huh... this should really return the data.... but to avoid changing the server, let's connect again and get that info
						TCHAR buffer[50];
						GetFolderInfoToBuffer(i->c_str(), &FOLDERINFO2::nLogicalSize, buffer, sizeof(buffer)/sizeof(TCHAR));

						// this won't work if the file system name is different from the shell display name!
						LVFINDINFO fi;
						fi.flags = LVFI_STRING;
						fi.psz = PathFindFileName(i->c_str());
						int iItem = ListView_FindItem(m_lv, -1, &fi);
						if (iItem >= 0)
						{
							LVITEM item;
							item.mask = LVIF_TEXT;
							item.iItem = iItem;
							item.iSubItem = 1;
							item.pszText = buffer;
							ListView_SetItem(m_lv, &item);
						}
					}
				}
			}
			CloseHandle(hPipe);
		}
	}
	else if (wParam == 2)
	{
		KillTimer(2);
		SetListToFolder();
	}
	return 0;
}

LRESULT FSWindow::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	DispEventUnadvise(m_pWebBrowser);
	if (--g_nWindows == 0)
		PostQuitMessage(0);
	return 0;
}

void FSWindow::OnFinalMessage(HWND hwnd)
{
	delete this;
}

void FSWindow::SetListToFolder()
{
	m_szFolder[0] = '\0';
	ListView_DeleteAllItems(m_lv);
	AdjustSizeForList();

	IServiceProvider* psp;
	if (SUCCEEDED(m_pWebBrowser->QueryInterface(IID_IServiceProvider, (void**)&psp)))
	{
		IShellBrowser* psb;
		if (SUCCEEDED(psp->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&psb)))
		{
			IShellView* psv;
			if (SUCCEEDED(psb->QueryActiveShellView(&psv)))
			{
				IFolderView* pfv;
				if (SUCCEEDED(psv->QueryInterface(IID_IFolderView, (void**)&pfv)))
				{
					IShellFolder2* psf2;
					if (SUCCEEDED(pfv->GetFolder(IID_IShellFolder2, (void**)&psf2)))
					{
						IPersistFolder2* ppf2;
						if (SUCCEEDED(psf2->QueryInterface(IID_IPersistFolder2, (void**)&ppf2)))
						{
							LPITEMIDLIST pFolderID;
							if (SUCCEEDED(ppf2->GetCurFolder(&pFolderID)))
							{
								SHGetPathFromIDList(pFolderID, m_szFolder);
							}
						}

						InsertEnumItems(pfv, psf2);

						psf2->Release();
					}
					pfv->Release();
				}
				psv->Release();
			}
			psb->Release();
		}
		psp->Release();
	}

	if (m_szFolder[0])
	{
		SetTimer(1, 2000, NULL);
	}
	else
	{
		KillTimer(1);
	}
}

void FSWindow::InsertEnumItems(IFolderView* pvf, IShellFolder2* psf2)
{
	IEnumIDList* pEnum;
	if (SUCCEEDED(pvf->Items(SVGIO_FLAG_VIEWORDER, IID_IEnumIDList, (void**)&pEnum)))
	{
		LPITEMIDLIST pItemID;
		while (pEnum->Next(1, &pItemID, NULL) == S_OK)
		{
			STRRET strret;
			if (SUCCEEDED(psf2->GetDisplayNameOf(pItemID, SHGDN_FORPARSING, &strret)))
			{
				LPTSTR pPath;
				if (SUCCEEDED(StrRetToStr(&strret, pItemID, &pPath)))
				{
					SHFILEINFO shfi;
					shfi.dwAttributes = SFGAO_FILESYSTEM | SFGAO_FOLDER;
					if (SHGetFileInfo(pPath, 0, &shfi, sizeof(shfi),
						SHGFI_ATTRIBUTES | SHGFI_ATTR_SPECIFIED | SHGFI_DISPLAYNAME | SHGFI_SYSICONINDEX | SHGFI_SMALLICON)
						&& shfi.dwAttributes & SFGAO_FILESYSTEM)
					{
						TCHAR buffer[50];
						if (shfi.dwAttributes & SFGAO_FOLDER)
						{
							GetFolderInfoToBuffer(pPath, &FOLDERINFO2::nLogicalSize, buffer, sizeof(buffer)/sizeof(TCHAR));
						}
						else
						{
							ULONGLONG nSize;
							GetLogicalFileSize(pPath, nSize);
							FormatSizeWithOption(nSize, buffer, sizeof(buffer)/sizeof(TCHAR));
						}

						// insert into list
						LVITEM item = {0};
						item.mask = LVIF_TEXT | LVIF_IMAGE;
						item.iItem = INT_MAX;
						item.iSubItem = 0;
						item.pszText = shfi.szDisplayName;
						item.iImage = shfi.iIcon;
						item.iItem = ListView_InsertItem(m_lv, &item);
						item.mask = LVIF_TEXT;
						item.iSubItem = 1;
						item.pszText = buffer;
						ListView_SetItem(m_lv, &item);

						AdjustSizeForList();
					}
					CoTaskMemFree(pPath);
				}
			}
			CoTaskMemFree(pItemID);
		}
		pEnum->Release();
	}
}

void FSWindow::AdjustSizeForList()
{
	DWORD dwExLvStyle = ListView_GetExtendedListViewStyle(m_lv);
	ListView_SetExtendedListViewStyle(m_lv, dwExLvStyle & ~LVS_EX_AUTOSIZECOLUMNS);

	ListView_SetColumnWidth(m_lv, 0, LVSCW_AUTOSIZE);
	ListView_SetColumnWidth(m_lv, 1, LVSCW_AUTOSIZE);

	DWORD approx = ListView_ApproximateViewRect(m_lv, -1, -1, -1);
	RECT rc;
	::GetWindowRect(m_lv, &rc);
	rc.right = rc.left + LOWORD(approx);
	rc.bottom = rc.top + HIWORD(approx);
	AdjustWindowRectEx(&rc, GetWindowLongPtr(GWL_STYLE), FALSE, GetWindowLongPtr(GWL_EXSTYLE));

	HMONITOR hMonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi = { sizeof(mi) };
	GetMonitorInfo(hMonitor, &mi);
	if (rc.bottom > mi.rcWork.bottom)
	{
		rc.bottom = mi.rcWork.bottom;
		rc.right += GetSystemMetrics(SM_CXVSCROLL);
	}
	if (rc.right > mi.rcWork.right)
	{
		rc.right = mi.rcWork.right;
	}
	MoveWindow(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);

	ListView_SetExtendedListViewStyle(m_lv, dwExLvStyle);
}


_ATL_FUNC_INFO FSWindow::NavigateCompleteInfo = { CC_STDCALL, VT_EMPTY, 2, {VT_DISPATCH, VT_BYREF | VT_VARIANT} };
_ATL_FUNC_INFO FSWindow::QuitInfo = { CC_STDCALL, VT_EMPTY, 0, NULL };

void FSWindow::OnNavigateComplete(IDispatch* pDisp, VARIANT* URL)
{
	SetTimer(2, 250);
}

void FSWindow::OnQuit()
{
	PostMessage(WM_CLOSE, 0, 0);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	g_hInstance = hInstance;

	CoInitialize(NULL);

	IWebBrowser2** pWebBrowsers;
	long nCount;
	if (GetShellWindows(pWebBrowsers, nCount) && nCount)
	{
		InitCommonControls();

		for (long i=0; i<nCount; i++)
		{
			HWND hwndExplorer;
			if (SUCCEEDED(pWebBrowsers[i]->get_HWND((SHANDLE_PTR*)&hwndExplorer)))
			{
				RECT rc;
				GetWindowRect(hwndExplorer, &rc);
				rc.left = rc.right;
				FSWindow* pWnd = new FSWindow(pWebBrowsers[i]);
				if (pWnd->Create(hwndExplorer, rc, _T("Folder Size")))
				{
					g_nWindows ++;
					pWnd->ShowWindow(nCmdShow);
				}
			}
			pWebBrowsers[i]->Release();
		}

		delete[] pWebBrowsers;

		if (g_nWindows)
		{
			MSG msg;
			while (GetMessage(&msg, NULL, 0, 0))
				DispatchMessage(&msg);
		}
	}

	CoUninitialize();

	return 0;
}