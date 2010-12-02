#include "StdAfx.h"

HINSTANCE g_hInstance;
int g_nWindows = 0;


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


class ListItem
{
public:
	ListItem(LPCTSTR filename, LPITEMIDLIST pidl, FOLDERINFO2& size, int index, int icon, LPCTSTR name) : m_pidl(pidl), m_size(size), m_index(index), m_icon(icon)
	{
		lstrcpyn(m_filename, filename, MAX_PATH);
		lstrcpyn(m_name, name, MAX_PATH);
	}
	~ListItem()
	{
		CoTaskMemFree(m_pidl);
	}

	LPCTSTR GetFileName() { return m_filename; }
	LPITEMIDLIST GetPidl() { return m_pidl; }
	FOLDERINFO2& GetSize() { return m_size; }
	int GetIndex() { return m_index; }
	int GetIcon() { return m_icon; }
	LPCTSTR GetName() { return m_name; }
	
	void UpdateSize(FOLDERINFO2& size) { m_size = size; }

private:
	LPITEMIDLIST m_pidl;
	FOLDERINFO2 m_size;
	int m_index;
	int m_icon;
	TCHAR m_filename[MAX_PATH];
	TCHAR m_name[MAX_PATH];
};

class RefreshItem
{
public:
	RefreshItem(LPCTSTR filename, FOLDERINFO2& size) : m_size(size) { lstrcpyn(m_filename, filename, MAX_PATH); }

	LPCTSTR GetFileName() { return m_filename; }
	FOLDERINFO2& GetSize() { return m_size; }

private:
	FOLDERINFO2 m_size;
	TCHAR m_filename[MAX_PATH];
};

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

#define WM_NEWITEMS      (WM_APP + 1)
#define WM_REFRESHITEMS  (WM_APP + 2)


class FolderViewScanner
{
public:
	FolderViewScanner(IWebBrowser2* pWebBrowser, HWND hWndPost);
	~FolderViewScanner();

	std::vector<ListItem*> GetNewItems();
	std::vector<RefreshItem*> GetRefreshItems();

	void Quit();

private:
	static unsigned int __stdcall ThreadProc(void* arg);
	DWORD ThreadProc();

	HWND m_hWndBrowser;
	HWND m_hWndPost;

	HANDLE m_hThread;
	HANDLE m_hQuitEvent;

	CRITICAL_SECTION m_cs;
	std::vector<ListItem*> m_newItems;
	std::vector<RefreshItem*> m_refreshItems;
};

FolderViewScanner::FolderViewScanner(IWebBrowser2* pWebBrowser, HWND hWndPost) : m_hWndBrowser(NULL), m_hWndPost(hWndPost), m_hThread(NULL), m_hQuitEvent(NULL)
{
	// ok, get the current index for this web browser, so i can open it in the other thread
	InitializeCriticalSection(&m_cs);
	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hWndBrowser;
	if (SUCCEEDED(pWebBrowser->get_HWND((SHANDLE_PTR*)&m_hWndBrowser)))
	{
		unsigned int threadId;
		m_hThread = (HANDLE)_beginthreadex(NULL, 0, ThreadProc, this, 0, &threadId);
	}
}

FolderViewScanner::~FolderViewScanner()
{
	// the thread is deleting itself, so don't wait for it
	if (m_hThread)
		CloseHandle(m_hThread);

	if (m_hQuitEvent)
		CloseHandle(m_hQuitEvent);

	DeleteCriticalSection(&m_cs);
}

std::vector<ListItem*> FolderViewScanner::GetNewItems()
{
	EnterCriticalSection(&m_cs);
	std::vector<ListItem*> items = m_newItems;
	m_newItems.clear();
	LeaveCriticalSection(&m_cs);
	return items;
}

std::vector<RefreshItem*> FolderViewScanner::GetRefreshItems()
{
	EnterCriticalSection(&m_cs);
	std::vector<RefreshItem*> items = m_refreshItems;
	m_refreshItems.clear();
	LeaveCriticalSection(&m_cs);
	return items;
}

void FolderViewScanner::Quit()
{
	SetEvent(m_hQuitEvent);
}

IWebBrowser2* GetWebBrowserFromHandle(HWND hWnd)
{
	IWebBrowser2* pWB = NULL;
	IWebBrowser2** pWebBrowsers;
	long nCount;
	if (GetShellWindows(pWebBrowsers, nCount))
	{
		for (long i=0; i<nCount; i++)
		{
			HWND hwndExplorer;
			if (pWB == NULL && SUCCEEDED(pWebBrowsers[i]->get_HWND((SHANDLE_PTR*)&hwndExplorer)) && hwndExplorer == hWnd)
				pWB = pWebBrowsers[i];
			else
				pWebBrowsers[i]->Release();
		}
		delete[] pWebBrowsers;
	}
	return pWB;
}

DWORD FolderViewScanner::ThreadProc()
{
	IWebBrowser2* pWebBrowser = GetWebBrowserFromHandle(m_hWndBrowser);
	if (pWebBrowser == NULL)
		return ERROR_INVALID_WINDOW_HANDLE;

	// keep checking the state of the quit event

	TCHAR szFolder[MAX_PATH];
	szFolder[0] = '\0';

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
							LPITEMIDLIST pFolderID;
							if (SUCCEEDED(ppf2->GetCurFolder(&pFolderID)))
							{
								SHGetPathFromIDList(pFolderID, szFolder);
							}
						}

						IEnumIDList* pEnum;
						if (SUCCEEDED(pfv->Items(SVGIO_FLAG_VIEWORDER, IID_IEnumIDList, (void**)&pEnum)))
						{
							int index = 0;
							LPITEMIDLIST pItemID;
							while (WaitForSingleObject(m_hQuitEvent, 0) == WAIT_TIMEOUT && pEnum->Next(1, &pItemID, NULL) == S_OK)
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
											FOLDERINFO2 fi;
											if (shfi.dwAttributes & SFGAO_FOLDER)
											{
												GetInfoForFolder(pPath, fi);
											}
											else
											{
												GetLogicalFileSize(pPath, fi.nLogicalSize);
												fi.giff = GIFF_CLEAN;
											}

											// the ListItem takes ownership of the PIDL
											ListItem* pItem = new ListItem(PathFindFileName(pPath), pItemID, fi, index++, shfi.iIcon, shfi.szDisplayName);
											pItemID = NULL;

											EnterCriticalSection(&m_cs);
											if (m_newItems.empty())
												PostMessage(m_hWndPost, WM_NEWITEMS, 0, 0);
											m_newItems.push_back(pItem);
											LeaveCriticalSection(&m_cs);
										}
										CoTaskMemFree(pPath);
									}
								}
								CoTaskMemFree(pItemID);
							}
							pEnum->Release();
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

	if (!szFolder[0])
	{
		WaitForSingleObject(m_hQuitEvent, INFINITE);
		return 0;
	}

	// loop on a refresh timer

	while (WaitForSingleObject(m_hQuitEvent, 2000) == WAIT_TIMEOUT)
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
			strsFoldersBrowsed.insert(szFolder);
			if (WriteGetUpdatedFoldersRequest(hPipe, strsFoldersBrowsed))
			{
				Strings strsFoldersToUpdate;
				if (ReadStringList(hPipe, strsFoldersToUpdate))
				{
					for (Strings::const_iterator i = strsFoldersToUpdate.begin(); i != strsFoldersToUpdate.end(); i++)
					{
						FOLDERINFO2 fi;
						GetInfoForFolder(i->c_str(), fi);
						RefreshItem* pItem = new RefreshItem(PathFindFileName(i->c_str()), fi);
						EnterCriticalSection(&m_cs);
						if (m_refreshItems.empty())
							PostMessage(m_hWndPost, WM_REFRESHITEMS, 0, 0);
						m_refreshItems.push_back(pItem);
						LeaveCriticalSection(&m_cs);
					}
				}
			}
			CloseHandle(hPipe);
		}
	}

	return 0;
}

unsigned int __stdcall FolderViewScanner::ThreadProc(void* arg)
{
	CoInitialize(NULL);

	FolderViewScanner* p = (FolderViewScanner*)arg;
	unsigned int ret = p->ThreadProc();

	// The client calls Quit, and then forgets about this object.
	// Responsibility for destruction is left to the thread, so if it's stalled,
	// it can clean itself up when it's done.
	delete p;

	CoUninitialize();
	return ret;
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



class FSWindow :
	public CWindowImpl<FSWindow, CWindow, CWinTraits<WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX, WS_EX_TOOLWINDOW> >,
	public IDispEventSimpleImpl<1, FSWindow, &DIID_DWebBrowserEvents2>
{
public:
	FSWindow(IWebBrowser2* pWebBrowser) : m_lv(NULL), m_pWebBrowser(pWebBrowser), m_pScanner(NULL) {}
	~FSWindow() { if (m_pScanner) m_pScanner->Quit(); }

	static CWndClassInfo& GetWndClassInfo()
	{
		static CWndClassInfo wc =
		{
			{ sizeof(WNDCLASSEX), 0, StartWindowProc },
			NULL, NULL, IDC_ARROW, TRUE, 0, _T("")
		};
		return wc;
	}

BEGIN_MSG_MAP(FSWindow)
	MESSAGE_HANDLER(WM_CREATE, OnCreate)
	MESSAGE_HANDLER(WM_SIZE, OnSize)
	MESSAGE_HANDLER(WM_NEWITEMS, OnNewItems)
	MESSAGE_HANDLER(WM_REFRESHITEMS, OnRefreshItems)
	NOTIFY_HANDLER(1, LVN_ITEMACTIVATE, OnItemActivate)
	NOTIFY_HANDLER(1, LVN_COLUMNCLICK, OnColumnClick)
	NOTIFY_HANDLER(1, LVN_DELETEALLITEMS, OnDeleteAllItems);
	MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
END_MSG_MAP()

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnNewItems(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnRefreshItems(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnItemActivate(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
	LRESULT OnColumnClick(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
	LRESULT OnDeleteAllItems(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
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
	void AdjustSizeForList();

	HWND m_lv;
	CComPtr<IWebBrowser2> m_pWebBrowser;
	TCHAR m_szFolder[MAX_PATH * 4];
	FolderViewScanner* m_pScanner;
	std::map<std::wstring, ListItem*> m_nameMap;
};

LRESULT FSWindow::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	RECT rc;
	GetClientRect(&rc);
	m_lv = CreateWindow(WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHAREIMAGELISTS,
		0, 0, rc.right-rc.left, rc.bottom-rc.top,
		m_hWnd, (HMENU)1, (HINSTANCE)GetWindowLongPtr(GWLP_HINSTANCE), NULL);
	if (!m_lv)
		return -1;

	ListView_SetExtendedListViewStyle(m_lv, LVS_EX_FULLROWSELECT | LVS_EX_AUTOSIZECOLUMNS | LVS_EX_DOUBLEBUFFER);

	SetWindowTheme(m_lv, L"Explorer", NULL);

	LVCOLUMN column;
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

	if (FAILED(DispEventAdvise(m_pWebBrowser)))
		return -1;

	m_pScanner = new FolderViewScanner(m_pWebBrowser, m_hWnd);

	return 0;
}

LRESULT FSWindow::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	::MoveWindow(m_lv, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
	return 0;
}

static void FormatFolderInfoBuffer(FOLDERINFO2& fi, LPTSTR buffer, size_t cch)
{
	FormatSizeWithOption(fi.nLogicalSize, buffer, 50);

	size_t len = wcslen(buffer);
	if (len < 50 - 1 && (fi.giff == GIFF_DIRTY || fi.giff == GIFF_SCANNING))
	{
		buffer[len] = fi.giff == GIFF_DIRTY ? L'~' : L'+';
		buffer[len + 1] = L'\0';
	}
}

LRESULT FSWindow::OnNewItems(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	std::vector<ListItem*> items = m_pScanner->GetNewItems();
	if (!items.empty())
	{
		for (size_t i=0; i<items.size(); i++)
		{
			LVITEM item = {0};
			item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
			item.iItem = INT_MAX;
			item.iSubItem = 0;
			item.pszText = (LPTSTR)items[i]->GetName();
			item.iImage = items[i]->GetIcon();
			item.lParam = (LPARAM)items[i];
			item.iItem = ListView_InsertItem(m_lv, &item);
			item.mask = LVIF_TEXT;
			item.iSubItem = 1;
			TCHAR buffer[50];
			FormatFolderInfoBuffer(items[i]->GetSize(), buffer, 50);
			item.pszText = buffer;
			ListView_SetItem(m_lv, &item);

			m_nameMap.insert(pair<wstring, ListItem*>(items[i]->GetFileName(), items[i]));
		}
		AdjustSizeForList();
	}
	return 0;
}

LRESULT FSWindow::OnRefreshItems(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	std::vector<RefreshItem*> items = m_pScanner->GetRefreshItems();
	if (!items.empty())
	{
		for (size_t i=0; i<items.size(); i++)
		{
			map<wstring, ListItem*>::iterator mapFind = m_nameMap.find(items[i]->GetFileName());
			if (mapFind != m_nameMap.end())
			{
				mapFind->second->UpdateSize(items[i]->GetSize());
				LVFINDINFO lvfi;
				lvfi.flags = LVFI_PARAM;
				lvfi.lParam = (LPARAM)mapFind->second;
				int iFind = ListView_FindItem(m_lv, -1, &lvfi);
				if (iFind >= 0)
				{
					LVITEM lvi;
					lvi.mask = LVIF_TEXT;
					lvi.iItem = iFind;
					lvi.iSubItem = 1;
					TCHAR buffer[50];
					FormatFolderInfoBuffer(items[i]->GetSize(), buffer, 50);
					lvi.pszText = buffer;
					ListView_SetItem(m_lv, &lvi);
				}
			}
			delete items[i];
		}
		AdjustSizeForList();
	}
	return 0;
}

int CALLBACK compareIndex(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	return ((ListItem*)lParam1)->GetIndex() - ((ListItem*)lParam2)->GetIndex();
}

int CALLBACK compareSize(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	ULONGLONG size1 = ((ListItem*)lParam1)->GetSize().nLogicalSize;
	ULONGLONG size2 = ((ListItem*)lParam2)->GetSize().nLogicalSize;
	return size1 == size2 ? 0 : size1 > size2 ? -1 : 1;
}

LRESULT FSWindow::OnItemActivate(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	int i = ListView_GetNextItem(m_lv, -1, LVNI_SELECTED);
	if (i < 0)
		return 0;
	LVITEM lvi;
	lvi.mask = LVIF_PARAM;
	lvi.iItem = i;
	lvi.iSubItem = 0;
	ListView_GetItem(m_lv, &lvi);
	ListItem* pItem = (ListItem*)lvi.lParam;

	IServiceProvider* psp;
	if (SUCCEEDED(m_pWebBrowser->QueryInterface(IID_IServiceProvider, (void**)&psp)))
	{
		IShellBrowser* psb;
		if (SUCCEEDED(psp->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&psb)))
		{
			psb->BrowseObject(pItem->GetPidl(), SBSP_DEFBROWSER | SBSP_RELATIVE);
			psb->Release();
		}
		psp->Release();
	}
	return 0;
}

LRESULT FSWindow::OnColumnClick(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	LPNMLISTVIEW pnmlv = (LPNMLISTVIEW)pnmh;
	PFNLVCOMPARE compareFunc = pnmlv->iSubItem == 0 ? compareIndex : compareSize;
	ListView_SortItems(m_lv, compareFunc, 0);
	return 0;
}

LRESULT FSWindow::OnDeleteAllItems(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	for (map<wstring, ListItem*>::iterator itr=m_nameMap.begin(); itr!=m_nameMap.end(); itr++)
		delete itr->second;
	m_nameMap.clear();
	return TRUE;
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


void FSWindow::AdjustSizeForList()
{
	DWORD dwExLvStyle = ListView_GetExtendedListViewStyle(m_lv);
	ListView_SetExtendedListViewStyle(m_lv, dwExLvStyle & ~LVS_EX_AUTOSIZECOLUMNS);

	ListView_SetColumnWidth(m_lv, 0, LVSCW_AUTOSIZE);
	ListView_SetColumnWidth(m_lv, 1, LVSCW_AUTOSIZE);

	DWORD approx = ListView_ApproximateViewRect(m_lv, -1, -1, -1);
	RECT rc;
	::GetWindowRect(m_lv, &rc);
	// the list actually needs to be a few pixels wider than it reports
	rc.right = rc.left + LOWORD(approx) + 4;
	rc.bottom = rc.top + HIWORD(approx);
	AdjustWindowRectEx(&rc, GetWindowLong(GWL_STYLE), FALSE, GetWindowLong(GWL_EXSTYLE));

	HMONITOR hMonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi = { sizeof(mi) };
	GetMonitorInfo(hMonitor, &mi);
	// if the title bar somehow got too high for the screen, align it with the top of the monitor
	if (rc.top < mi.rcWork.top)
	{
		rc.bottom += mi.rcWork.top - rc.top;
		rc.top += mi.rcWork.top - rc.top;
	}
	// if we're going off the bottom, shift the window up if there's space
	if (rc.bottom > mi.rcWork.bottom && rc.top > mi.rcWork.top)
	{
		int shift = min(rc.top - mi.rcWork.top, rc.bottom - mi.rcWork.bottom);
		rc.top -= shift;
		rc.bottom -= shift;
	}
	// if we're still going off the bottom, trim it at the bottom and get ready for a scroll bar
	if (rc.bottom > mi.rcWork.bottom)
	{
		rc.bottom = mi.rcWork.bottom;
		rc.right += GetSystemMetrics(SM_CXVSCROLL);
	}
	// if we're bumping out past the right of the monitor, bump it left instead
	if (rc.right > mi.rcWork.right)
	{
		rc.left -= rc.right - mi.rcWork.right;
		rc.right = mi.rcWork.right;
	}
	MoveWindow(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

	ListView_SetExtendedListViewStyle(m_lv, dwExLvStyle);
}


_ATL_FUNC_INFO FSWindow::NavigateCompleteInfo = { CC_STDCALL, VT_EMPTY, 2, {VT_DISPATCH, VT_BYREF | VT_VARIANT} };
_ATL_FUNC_INFO FSWindow::QuitInfo = { CC_STDCALL, VT_EMPTY, 0, NULL };

void FSWindow::OnNavigateComplete(IDispatch* pDisp, VARIANT* URL)
{
	m_pScanner->Quit();

	// the notification from the control will clean up the old items
	ListView_DeleteAllItems(m_lv);

	AdjustSizeForList();

	m_pScanner = new FolderViewScanner(m_pWebBrowser, m_hWnd);
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
				WINDOWPLACEMENT wp = { sizeof(wp) };
				GetWindowPlacement(hwndExplorer, &wp);
				if (wp.showCmd != SW_SHOWMINIMIZED)
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