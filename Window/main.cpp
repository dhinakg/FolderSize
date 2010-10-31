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



#define ID_LIST  3

class FSWindow :
	public CWindowImpl<FSWindow>,
	public IDispEventSimpleImpl<1, FSWindow, &DIID_DWebBrowserEvents2>
{
public:
	FSWindow(IWebBrowser2* pWebBrowser) : m_lv(NULL), m_pWebBrowser(pWebBrowser) {}

BEGIN_MSG_MAP(FSWindow)
	MESSAGE_HANDLER(WM_CREATE, OnCreate)
	MESSAGE_HANDLER(WM_SIZE, OnSize)
	MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
END_MSG_MAP()

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
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

	HWND m_lv;
	CComPtr<IWebBrowser2> m_pWebBrowser;
};

LRESULT FSWindow::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	RECT rc;
	GetClientRect(&rc);
	m_lv = CreateWindow(WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT,
		0, 0, rc.right-rc.left, rc.bottom-rc.top,
		m_hWnd, (HMENU)ID_LIST, (HINSTANCE)GetWindowLongPtr(GWLP_HINSTANCE), NULL);
	if (!m_lv)
		return -1;
	LVCOLUMN column = {0};
	column.mask = LVCF_FMT | LVCF_TEXT;
	column.fmt = LVCFMT_LEFT;
	column.pszText = _T("Name");
	ListView_InsertColumn(m_lv, 0, &column);
	column.fmt = LVCFMT_RIGHT;
	column.pszText = _T("Size");
	ListView_InsertColumn(m_lv, 1, &column);

	SetListToFolder();
	if (FAILED(DispEventAdvise(m_pWebBrowser)))
		return -1;
	return 0;
}

LRESULT FSWindow::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	::MoveWindow(m_lv, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
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
	std::wstring strFolder = GetFolderFromWebBrowser(m_pWebBrowser);
	// what items to display?
	// if i had a pointer to a shell window, i could enumerate its items.

	ListView_DeleteAllItems(m_lv);

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
					ListView_InsertItem(m_lv, &item);
					item.iSubItem = 1;
					item.pszText = buffer;
					ListView_SetItem(m_lv, &item);
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

	ListView_SetColumnWidth(m_lv, 0, LVSCW_AUTOSIZE);
	ListView_SetColumnWidth(m_lv, 1, LVSCW_AUTOSIZE);

	// size the window to the list width
	int listWidth = ListView_GetColumnWidth(m_lv, 0) + ListView_GetColumnWidth(m_lv, 1);
	DWORD approx = ListView_ApproximateViewRect(m_lv, -1, -1, -1);
	RECT rc;
	::GetWindowRect(m_lv, &rc);
	rc.right = rc.left + LOWORD(approx);
	rc.bottom = rc.top + HIWORD(approx);
	AdjustWindowRectEx(&rc, GetWindowLongPtr(GWL_STYLE), FALSE, GetWindowLongPtr(GWL_EXSTYLE));
	MoveWindow(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
}

_ATL_FUNC_INFO FSWindow::NavigateCompleteInfo = { CC_STDCALL, VT_EMPTY, 2, {VT_DISPATCH, VT_BYREF | VT_VARIANT} };
_ATL_FUNC_INFO FSWindow::QuitInfo = { CC_STDCALL, VT_EMPTY, 0, NULL };

void FSWindow::OnNavigateComplete(IDispatch* pDisp, VARIANT* URL)
{
	SetListToFolder();
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
				if (pWnd->Create(hwndExplorer, rc, _T("Folder Size"), WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX, WS_EX_TOOLWINDOW))
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