#include "StdAfx.h"
#include "ShellUpdate.h"
#include "FolderSize.h"
#include "..\Pipe\Pipe.h"

ShellUpdate::ShellUpdate()
{
	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
}

ShellUpdate::~ShellUpdate()
{
	SetEvent(m_hQuitEvent);
	WaitForSingleObject(m_hThread, INFINITE);
	CloseHandle(m_hQuitEvent);
	CloseHandle(m_hThread);
}

DWORD ShellUpdate::ThreadProc(LPVOID lpParameter)
{
	ShellUpdate* pShellUpdate = (ShellUpdate*)lpParameter;
	CoInitialize(NULL);

	while (1)
	{
		DWORD dwWait = WaitForSingleObject(pShellUpdate->m_hQuitEvent, SHELL_UPDATE_INTERVAL);
		if (dwWait == WAIT_TIMEOUT)
		{
			pShellUpdate->Update();
		}
		else
		{
			break;
		}
	}

	CoUninitialize();
	return 0;
}

void GetBrowsedFolders(Strings& strsFolders)
{
	IShellWindows* psw;
	if (SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, NULL, CLSCTX_ALL, IID_IShellWindows, (void**)&psw)))
	{
		IUnknown* pUnkEnum;
		if (SUCCEEDED(psw->_NewEnum(&pUnkEnum)))
		{
			IEnumVARIANT* pEnum;
			if (SUCCEEDED(pUnkEnum->QueryInterface(IID_IEnumVARIANT, (void**)&pEnum)))
			{
				VARIANT var;
				while (pEnum->Next(1, &var, NULL) == S_OK)
				{
					if (V_VT(&var) == VT_DISPATCH)
					{
						IServiceProvider* psp;
						if (SUCCEEDED(V_DISPATCH(&var)->QueryInterface(IID_IServiceProvider, (void**)&psp)))
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
					VariantClear(&var);
				}
				pEnum->Release();
			}
			pUnkEnum->Release();
		}
		psw->Release();
	}
}


void ShellUpdate::Update()
{
	Strings strsFoldersBrowsed, strsFoldersToUpdate;
	GetBrowsedFolders(strsFoldersBrowsed);

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
