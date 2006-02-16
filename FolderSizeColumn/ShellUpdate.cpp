#include "StdAfx.h"
#include "ShellUpdate.h"
#include "FolderSize.h"
#include "..\Pipe\Pipe.h"

#define MUTEX_NAME TEXT("FolderSizeShellUpdateMutex")

ShellUpdate::ShellUpdate()
{
	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
}

ShellUpdate::~ShellUpdate()
{
	SetEvent(m_hQuitEvent);
	
	// This destructor is called from DllCanUnloadNow, on a thread created by Explorer,
	// so it may be used to route COM messages, so pump all messages.
	while (MsgWaitForMultipleObjects(1, &m_hThread, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
	{
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			DispatchMessage(&msg);
		}
	}

	CloseHandle(m_hQuitEvent);
	CloseHandle(m_hThread);
}

DWORD ShellUpdate::ThreadProc(LPVOID lpParameter)
{
	ShellUpdate* pShellUpdate = (ShellUpdate*)lpParameter;
	CoInitialize(NULL);

	// There can be multiple explorer.exe processes, each with this DLL loaded.
	// Since updating shell items broadcasts to all Explorer windows, only one instance
	// of the DLL should send the broadcasts. Use a named mutex to decide.
	HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);

	HANDLE hHandles[2];
	hHandles[0] = pShellUpdate->m_hQuitEvent;
	hHandles[1] = hMutex;
	DWORD dwWait = WaitForMultipleObjects(2, hHandles, FALSE, INFINITE);
	if (dwWait == WAIT_OBJECT_0 + 1)
	{
		// got the mutex, so enter the scan loop
		while (1)
		{
			DWORD dwWait = WaitForSingleObject(pShellUpdate->m_hQuitEvent, SHELL_UPDATE_INTERVAL);
			if (dwWait == WAIT_TIMEOUT)
				pShellUpdate->Update();
			else
				break;
		}
		ReleaseMutex(hMutex);
	}

	CloseHandle(hMutex);

	CoUninitialize();
	return 0;
}

void GetBrowsedFolders(Strings& strsFolders, IWebBrowser2** apWebBrowsers, int nCount)
{
	for (int i=0; i<nCount; i++)
	{
		IServiceProvider* psp;
		if (SUCCEEDED(apWebBrowsers[i]->QueryInterface(IID_IServiceProvider, (void**)&psp)))
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
							bool bColumnDisplayed = false;
							/*
							int nColumn = 0;
							SHCOLUMNID shcid;
							HRESULT hr;
							while (SUCCEEDED(hr = psf2->MapColumnToSCID(nColumn++, &shcid)))
							{
								if (shcid.fmtid == CLSID_FolderSizeObj)
								{
									bColumnDisplayed = true;
									break;
								}
							}
							*/
							bColumnDisplayed = true;
							if (bColumnDisplayed)
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
											strsFolders.insert(szPath);
										}
										CoTaskMemFree(pidl);
									}
									ppf2->Release();
								}
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
	}
}

bool GetShellWindows(IWebBrowser2**& pWebBrowsers, long& nCount)
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
				if (SUCCEEDED(psw->Item(varItem, &pDispatch)))
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

bool WindowIsABrowserWindow(HWND hwnd, IWebBrowser2** apWebBrowsers, int nCount)
{
	for (int i=0; i<nCount; i++)
	{
		HWND hwndBrowser;
		if (SUCCEEDED(apWebBrowsers[i]->get_HWND((SHANDLE_PTR*)&hwndBrowser)))
		{
			if (hwndBrowser == hwnd)
			{
				return true;
			}
		}
	}
	return false;
}

bool WindowIsClass(HWND hwnd, LPCTSTR pszClass)
{
	TCHAR szClass[256];
	if (GetClassName(hwnd, szClass, 256) == 0)
		return false;
	return lstrcmp(szClass, pszClass) == 0;
}

bool WindowLooksLikeAnAddressEdit(HWND hwnd)
{
	if (WindowIsClass(hwnd, TEXT("Edit")))
	{
		HWND hwndParent = GetParent(hwnd);
		if (hwndParent != NULL && WindowIsClass(hwndParent, TEXT("ComboBox")))
		{
			hwndParent = GetParent(hwndParent);
			if (hwndParent != NULL && WindowIsClass(hwndParent, TEXT("ComboBoxEx32")))
			{
				hwndParent = GetParent(hwndParent);
				if (hwndParent != NULL && WindowIsClass(hwndParent, TEXT("ReBarWindow32")))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void ShellUpdate::Update()
{
	Strings strsFoldersBrowsed;

	IWebBrowser2** apWebBrowsers;
	long nCount;
	if (GetShellWindows(apWebBrowsers, nCount))
	{
		// If we issue SHChangeNotify while the user is entering a new address
		// in the address bar, the new address will be reset to the current address.
		// So, if an address bar has the focus, don't update the shell.

		bool bUserEditingAddress = false;

		// check if the current focussed window is an address bar
		GUITHREADINFO GUIThreadInfo = { sizeof(GUITHREADINFO) };
		if (GetGUIThreadInfo(NULL, &GUIThreadInfo))
		{
			// check if any of the web browser windows are the active window
			if (WindowIsABrowserWindow(GUIThreadInfo.hwndActive, apWebBrowsers, nCount))
			{
				if (WindowLooksLikeAnAddressEdit(GUIThreadInfo.hwndFocus))
					bUserEditingAddress = true;
			}
		}

		if (!bUserEditingAddress)
		{
			GetBrowsedFolders(strsFoldersBrowsed, apWebBrowsers, nCount);
		}

		for (int i=0; i<nCount; i++)
			apWebBrowsers[i]->Release();
		delete[] apWebBrowsers;
	}

	if (!strsFoldersBrowsed.empty())
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
			if (WriteRequest(hPipe, PCR_GETUPDATEDFOLDERS))
			{
				if (WriteStringList(hPipe, strsFoldersBrowsed))
				{
					Strings strsFoldersToUpdate;
					if (ReadStringList(hPipe, strsFoldersToUpdate))
					{
						for (Strings::const_iterator i = strsFoldersToUpdate.begin(); i != strsFoldersToUpdate.end(); i++)
						{
							SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH|SHCNF_FLUSH, i->c_str(), NULL);
						}
					}
				}
			}
			CloseHandle(hPipe);
		}
	}
}
