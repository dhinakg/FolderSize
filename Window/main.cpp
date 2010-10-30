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
					if(pDispatch) // avoid segfault
					{
						IWebBrowser2* pWebBrowser;
						if (SUCCEEDED(pDispatch->QueryInterface(IID_IWebBrowser2, (void**)&pWebBrowser)))
						{
							pWebBrowsers[nCount++] = pWebBrowser;
						}
						pDispatch->Release();
					}
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

bool GetLogicalFileSize(LPCTSTR pFileName, ULONGLONG& nSize)
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

std::wstring GetFolderFromWebBrowser(IWebBrowser2* pWebBrowser)
{
	std::wstring strFolder;
	IServiceProvider* psp;
	if (SUCCEEDED(pWebBrowser->QueryInterface(IID_IServiceProvider, (void**)&psp)))
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
							LPITEMIDLIST pidl;
							if (SUCCEEDED(ppf2->GetCurFolder(&pidl)))
							{
								TCHAR szPath[MAX_PATH];
								if (SHGetPathFromIDList(pidl, szPath))
								{
									strFolder = szPath;
								}
								CoTaskMemFree(pidl);
							}
							ppf2->Release();
						}
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
	return strFolder;
}

void SetListToFolder(HWND lv, IWebBrowser2* pWebBrowser)
{
	std::wstring strFolder = GetFolderFromWebBrowser(pWebBrowser);
	// what items to display?
	// if i had a pointer to a shell window, i could enumerate its items.

	ListView_DeleteAllItems(lv);

	int i = 0;

	TCHAR szPath[MAX_PATH];
	lstrcpy(szPath, strFolder.c_str());
	PathAppend(szPath, _T("*"));
	WIN32_FIND_DATA FindData;
	HANDLE hFind = FindFirstFile(szPath, &FindData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			TCHAR buffer[50];
			// for each name, add it to the list!
			if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (FindData.cFileName[0] != L'.' || (FindData.cFileName[1] != L'\0' && (FindData.cFileName[1] != L'.' || FindData.cFileName[2] != L'\0')))
				{
					// look it up from the service
					lstrcpy(szPath, strFolder.c_str());
					PathAppend(szPath, FindData.cFileName);
					GetFolderInfoToBuffer(szPath, &FOLDERINFO2::nLogicalSize, buffer, sizeof(buffer)/sizeof(TCHAR));

					// insert into list
					LVITEM item = {0};
					item.mask = LVIF_TEXT;
					item.iItem = i++;
					item.iSubItem = 0;
					item.pszText = FindData.cFileName;
					ListView_InsertItem(lv, &item);
					item.iSubItem = 1;
					item.pszText = buffer;
					ListView_SetItem(lv, &item);
				}
			}
			else
			{
				lstrcpy(szPath, strFolder.c_str());
				PathAppend(szPath, FindData.cFileName);
				ULONGLONG nSize;
				GetLogicalFileSize(szPath, nSize);
				FormatSizeWithOption(nSize, buffer, sizeof(buffer)/sizeof(TCHAR));
			}

		} while (FindNextFile(hFind, &FindData));
		FindClose(hFind);
	}

	ListView_SetColumnWidth(lv, 0, LVSCW_AUTOSIZE);
	ListView_SetColumnWidth(lv, 1, LVSCW_AUTOSIZE);

	// size the window to the list width
	int listWidth = ListView_GetColumnWidth(lv, 0) + ListView_GetColumnWidth(lv, 1);
	DWORD approx = ListView_ApproximateViewRect(lv, -1, -1, -1);
	HWND hwnd = GetParent(lv);
	RECT rc;
	GetWindowRect(lv, &rc);
	rc.right = rc.left + LOWORD(approx);
	rc.bottom = rc.top + HIWORD(approx);
	AdjustWindowRectEx(&rc, GetWindowLongPtr(hwnd, GWL_STYLE), FALSE, GetWindowLongPtr(hwnd, GWL_EXSTYLE));
	MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
}


#define ID_LIST  3

class WindowData :
	public IDispEventSimpleImpl<1, WindowData, &DIID_DWebBrowserEvents2>
{
public:
	static _ATL_FUNC_INFO NavigateCompleteInfo;
	static _ATL_FUNC_INFO QuitInfo;

BEGIN_SINK_MAP(WindowData)
	SINK_ENTRY_INFO(1, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2, OnNavigateComplete, &NavigateCompleteInfo)
	SINK_ENTRY_INFO(1, DIID_DWebBrowserEvents2, DISPID_ONQUIT, OnQuit, &QuitInfo)
END_SINK_MAP()

	WindowData(HWND lv, IWebBrowser2* pWebBrowser) : m_lv(lv), m_pWebBrowser(pWebBrowser)
	{
	}
	HRESULT Advise()
	{
		return DispEventAdvise(m_pWebBrowser);
	}
	virtual ~WindowData()
	{
		DispEventUnadvise(m_pWebBrowser);
	}

	void __stdcall OnNavigateComplete(IDispatch *pDisp, VARIANT *URL);
	void __stdcall OnQuit();

	HWND m_lv;
	CComPtr<IWebBrowser2> m_pWebBrowser;
	DWORD m_dwCookie;
};

_ATL_FUNC_INFO WindowData::NavigateCompleteInfo = { CC_STDCALL, VT_EMPTY, 2, {VT_DISPATCH, VT_BYREF | VT_VARIANT} };
_ATL_FUNC_INFO WindowData::QuitInfo = { CC_STDCALL, VT_EMPTY, 0, NULL };

void WindowData::OnNavigateComplete(IDispatch* pDisp, VARIANT* URL)
{
	SetListToFolder(m_lv, m_pWebBrowser);
}

void WindowData::OnQuit()
{
	PostMessage(GetParent(m_lv), WM_CLOSE, 0, 0);
}

LRESULT WINAPI WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		{
			RECT rc;
			GetClientRect(hwnd, &rc);
			HWND lv = CreateWindow(WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT,
				0, 0, rc.right-rc.left, rc.bottom-rc.top,
				hwnd, (HMENU)ID_LIST, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			if (!lv)
				return -1;
			LVCOLUMN column = {0};
			column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
			column.fmt = LVCFMT_LEFT;
			column.cx = 300;
			column.pszText = _T("Name");
			ListView_InsertColumn(lv, 0, &column);
			column.fmt = LVCFMT_RIGHT;
			column.cx = 200;
			column.pszText = _T("Size");
			ListView_InsertColumn(lv, 1, &column);

			IWebBrowser2* pWebBrowser = (IWebBrowser2*)((CREATESTRUCT*)lParam)->lpCreateParams;
			SetListToFolder(lv, pWebBrowser);

			WindowData* pWindowData = new WindowData(lv, pWebBrowser);
			HRESULT hr = pWindowData->Advise();
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pWindowData);

			return 0;
		}
	case WM_SIZE:
		MoveWindow(GetDlgItem(hwnd, ID_LIST), 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		return 0;
	case WM_DESTROY:
		{
			WindowData* pWindowData = (WindowData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			delete pWindowData;
			PostQuitMessage(0);
			return 0;
		}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
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

		WNDCLASS wndClass = { sizeof(wndClass) };
		wndClass.lpfnWndProc = WindowProc;
		wndClass.hInstance = hInstance;
		wndClass.lpszClassName = _T("Size");
		ATOM atmWindowClass = RegisterClass(&wndClass);

		for (long i=0; i<nCount; i++)
		{
			HWND hwndExplorer;
			if (SUCCEEDED(pWebBrowsers[i]->get_HWND((SHANDLE_PTR*)&hwndExplorer)))
			{
				RECT rc;
				GetWindowRect(hwndExplorer, &rc);
				HWND hwnd = CreateWindowEx(WS_EX_TOOLWINDOW, MAKEINTATOM(atmWindowClass),
						_T("Folder Size"), WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX,
						rc.right, rc.top, 0, 0, hwndExplorer, NULL, hInstance, pWebBrowsers[i]);
				if (hwnd)
				{
					g_nWindows ++;
					ShowWindow(hwnd, nCmdShow);
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