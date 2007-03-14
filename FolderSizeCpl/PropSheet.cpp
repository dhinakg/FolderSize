#include "StdAfx.h"
#include <shlobj.h>   // for IShellWindows
#include <olectl.h>
#include "PropSheet.h"
#include "Resource.h"
#include "Hyperlinks.h"
#include "Service.h"

#define COLUMN_SETTINGS_KEY      TEXT("Software\\Brio\\FolderSize")
#define SERVICE_PARAMETERS_KEY   TEXT("SYSTEM\\CurrentControlSet\\Services\\FolderSize\\Parameters")

bool LoadDWord(HKEY hKey, LPCTSTR lpValueName, DWORD& dw)
{
	DWORD dwType, dwData, cbData;
	cbData = sizeof(DWORD);
	if (RegQueryValueEx(hKey, lpValueName, NULL, &dwType, (LPBYTE)&dwData, &cbData) == ERROR_SUCCESS)
	{
		if (dwType == REG_DWORD)
		{
			dw = dwData;
			return true;
		}
	}
	return false;
}

void SaveDWord(HKEY hKey, LPCTSTR lpValueName, DWORD dw)
{
	RegSetValueEx(hKey, lpValueName, 0, REG_DWORD, (CONST BYTE*)&dw, sizeof(DWORD));
}

void LoadDisplayOptions(int& nDisplayFormat)
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, COLUMN_SETTINGS_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD dw;
		if (LoadDWord(hKey, TEXT("DisplayFormat"), dw))
			nDisplayFormat = dw;

		RegCloseKey(hKey);
	}
}

void SaveDisplayOptions(int nDisplayFormat)
{
	HKEY hKey;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, COLUMN_SETTINGS_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		DWORD dwData = nDisplayFormat;
		RegSetValueEx(hKey, TEXT("DisplayFormat"), 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD));
		RegCloseKey(hKey);
	}
}

#define SCANDRIVETYPE_LOCAL        0x01
#define SCANDRIVETYPE_CD           0x02
#define SCANDRIVETYPE_REMOVABLE    0x04
#define SCANDRIVETYPE_NETWORK      0x08

int LoadScanDriveTypes()
{
	int DriveTypes = 0;

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, SERVICE_PARAMETERS_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD dw;
		if (!LoadDWord(hKey, TEXT("ScanLocal"), dw) || dw != 0)
			DriveTypes |= SCANDRIVETYPE_LOCAL;
		if (!LoadDWord(hKey, TEXT("ScanCD"), dw) || dw != 0)
			DriveTypes |= SCANDRIVETYPE_CD;
		if (!LoadDWord(hKey, TEXT("ScanRemovable"), dw) || dw != 0)
			DriveTypes |= SCANDRIVETYPE_REMOVABLE;
		if (!LoadDWord(hKey, TEXT("ScanNetwork"), dw) || dw != 0)
			DriveTypes |= SCANDRIVETYPE_NETWORK;

		RegCloseKey(hKey);
	}

	return DriveTypes;
}

void SaveScanDriveTypes(int DriveTypes)
{
	HKEY hKey;
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, SERVICE_PARAMETERS_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		SaveDWord(hKey, TEXT("ScanLocal"), DriveTypes & SCANDRIVETYPE_LOCAL ? 1 : 0);
		SaveDWord(hKey, TEXT("ScanCD"), DriveTypes & SCANDRIVETYPE_CD ? 1 : 0);
		SaveDWord(hKey, TEXT("ScanRemovable"), DriveTypes & SCANDRIVETYPE_REMOVABLE ? 1 : 0);
		SaveDWord(hKey, TEXT("ScanNetwork"), DriveTypes & SCANDRIVETYPE_NETWORK ? 1 : 0);
		RegCloseKey(hKey);
	}
}

void RefreshShell()
{
	CoInitialize(NULL);
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
														SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_IDLIST, pidl, NULL);
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
	CoUninitialize();
}

INT_PTR CALLBACK DisplayProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			// localize the messages
			const LONGLONG llSampleSize = 2634192972;
			TCHAR szFormatMsg[256], szSize[256], szDisplayMsg[256];
			GetDlgItemText(hwndDlg, IDC_DISPLAY_EXPLORER, szFormatMsg, 256);
			StrFormatKBSize(llSampleSize, szSize, 256);
			wsprintf(szDisplayMsg, szFormatMsg, szSize);
			SetDlgItemText(hwndDlg, IDC_DISPLAY_EXPLORER, szDisplayMsg);
			GetDlgItemText(hwndDlg, IDC_DISPLAY_COMPACT, szFormatMsg, 256);
			StrFormatByteSize64(llSampleSize, szSize, 256);
			wsprintf(szDisplayMsg, szFormatMsg, szSize);
			SetDlgItemText(hwndDlg, IDC_DISPLAY_COMPACT, szDisplayMsg);

			// load the format option
			int nDisplayFormat = 0;
			LoadDisplayOptions(nDisplayFormat);
			UINT nIDCheck = nDisplayFormat == 1 ? IDC_DISPLAY_COMPACT : IDC_DISPLAY_EXPLORER;
			CheckDlgButton(hwndDlg, nIDCheck, BST_CHECKED);

			// load the drive type options
			int DriveTypes = LoadScanDriveTypes();
			CheckDlgButton(hwndDlg, IDC_DISPLAY_LOCAL, DriveTypes & SCANDRIVETYPE_LOCAL ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_DISPLAY_CD, DriveTypes & SCANDRIVETYPE_CD ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_DISPLAY_REMOVABLE, DriveTypes & SCANDRIVETYPE_REMOVABLE ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_DISPLAY_NETWORK, DriveTypes & SCANDRIVETYPE_NETWORK ? BST_CHECKED : BST_UNCHECKED);
			return TRUE;
		}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_DISPLAY_EXPLORER:
		case IDC_DISPLAY_COMPACT:
			PropSheet_Changed(GetParent(hwndDlg), hwndDlg);
			break;
		}
		break;
	case WM_NOTIFY:
		{
			LPNMHDR pnmhdr = (LPNMHDR)lParam;
			switch (pnmhdr->code)
			{
			case PSN_APPLY:
				// read the settings and write them to the registry
				int nDisplayFormat = IsDlgButtonChecked(hwndDlg, IDC_DISPLAY_EXPLORER) == BST_CHECKED ? 0 : 1;
				SaveDisplayOptions(nDisplayFormat);

				int DriveTypes = 0;
				if (IsDlgButtonChecked(hwndDlg, IDC_DISPLAY_LOCAL) == BST_CHECKED)
					DriveTypes |= SCANDRIVETYPE_LOCAL;
				if (IsDlgButtonChecked(hwndDlg, IDC_DISPLAY_CD) == BST_CHECKED)
					DriveTypes |= SCANDRIVETYPE_CD;
				if (IsDlgButtonChecked(hwndDlg, IDC_DISPLAY_REMOVABLE) == BST_CHECKED)
					DriveTypes |= SCANDRIVETYPE_REMOVABLE;
				if (IsDlgButtonChecked(hwndDlg, IDC_DISPLAY_NETWORK) == BST_CHECKED)
					DriveTypes |= SCANDRIVETYPE_NETWORK;
				SaveScanDriveTypes(DriveTypes);

				RefreshShell();
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

typedef struct
{
	UINT nNameID;
	BOOL bEnableStart;
	BOOL bEnableStop;
	BOOL bEnablePause;
	BOOL bEnableContinue;
} SERVER_STATE;

SERVER_STATE g_ServerStateLookup[] =
{
	{IDS_SERVICE_UNKNOWN, FALSE, FALSE, FALSE, FALSE},
	{IDS_SERVICE_STOPPED, TRUE, FALSE, FALSE, FALSE},
	{IDS_SERVICE_START_PENDING, FALSE, TRUE, FALSE, FALSE},
	{IDS_SERVICE_STOP_PENDING, FALSE, FALSE, FALSE, FALSE},
	{IDS_SERVICE_RUNNING, FALSE, TRUE, TRUE, FALSE},
	{IDS_SERVICE_CONTINUE_PENDING, FALSE, FALSE, FALSE, FALSE},
	{IDS_SERVICE_PAUSE_PENDING, FALSE, FALSE, FALSE, FALSE},
	{IDS_SERVICE_PAUSED, FALSE, TRUE, FALSE, TRUE}
};

void UpdateServiceStatus(HWND hwndDlg)
{
	DWORD dwStatus = GetServiceStatus();

	TCHAR szStatus[256];
	LoadString(g_hInstance, g_ServerStateLookup[dwStatus].nNameID, szStatus, countof(szStatus));
	SetDlgItemText(hwndDlg, IDC_SERVICE_STATUS, szStatus);
	EnableWindow(GetDlgItem(hwndDlg, IDC_SERVICE_START), g_ServerStateLookup[dwStatus].bEnableStart);
	EnableWindow(GetDlgItem(hwndDlg, IDC_SERVICE_STOP), g_ServerStateLookup[dwStatus].bEnableStop);
	EnableWindow(GetDlgItem(hwndDlg, IDC_SERVICE_PAUSE), g_ServerStateLookup[dwStatus].bEnablePause);
	EnableWindow(GetDlgItem(hwndDlg, IDC_SERVICE_RESUME), g_ServerStateLookup[dwStatus].bEnableContinue);
}

void DisplayError(HWND hwnd, HRESULT hr)
{
	LPTSTR pszMessage;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM, NULL, hr, 0, (LPTSTR)&pszMessage, 0, NULL))
	{
		TCHAR szTitle[256];
		LoadString(g_hInstance, IDS_NAME, szTitle, 256);
		MessageBox(hwnd, pszMessage, szTitle, MB_OK|MB_ICONSTOP);
		LocalFree(pszMessage);
	}
}



void SetBiggerFont(HWND hwndCtrl)
{
	HFONT hFont = (HFONT)SendMessage(hwndCtrl, WM_GETFONT, 0, 0);
	if (hFont != NULL)
	{
		LOGFONT lf;
		GetObject(hFont, sizeof(LOGFONT), &lf);
		lf.lfHeight = (lf.lfHeight * 3) / 2;
		lf.lfWidth = (lf.lfWidth * 3) / 2;
		HFONT hBigFont = CreateFontIndirect(&lf);
		if (hBigFont != NULL)
		{
			SendMessage(hwndCtrl, WM_SETFONT, (WPARAM)hBigFont, TRUE);
		}
	}
}

void ReleaseBiggerFont(HWND hwndCtrl)
{
	HFONT hFont = (HFONT)SendMessage(hwndCtrl, WM_GETFONT, 0, 0);
	if (hFont != NULL)
	{
		DeleteObject(hFont);
	}
}

INT_PTR CALLBACK ServiceProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			// set the info icon
			HICON hIcon = LoadIcon(NULL, IDI_INFORMATION);
			SendMessage(GetDlgItem(hwndDlg, IDC_SERVICE_INFOICON), STM_SETICON, (WPARAM)hIcon, 0);

			UpdateServiceStatus(hwndDlg);
			return TRUE;
		}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_SERVICE_START:
			if (!ModifyService(MS_START))
				DisplayError(hwndDlg, GetLastError());
			UpdateServiceStatus(hwndDlg);
			break;
		case IDC_SERVICE_STOP:
			if (!ModifyService(MS_STOP))
				DisplayError(hwndDlg, GetLastError());
			UpdateServiceStatus(hwndDlg);
			break;
		case IDC_SERVICE_PAUSE:
			if (!ModifyService(MS_PAUSE))
				DisplayError(hwndDlg, GetLastError());
			UpdateServiceStatus(hwndDlg);
			break;
		case IDC_SERVICE_RESUME:
			if (!ModifyService(MS_CONTINUE))
				DisplayError(hwndDlg, GetLastError());
			UpdateServiceStatus(hwndDlg);
			break;
		}
		break;
	}
	return FALSE;
}

INT_PTR CALLBACK AboutProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			SetBiggerFont(GetDlgItem(hwndDlg, IDC_TITLE));

			ConvertStaticToHyperlink(hwndDlg, IDC_TITLE);
			ConvertStaticToHyperlink(hwndDlg, IDC_HYPER_GPL);
			ConvertStaticToHyperlink(hwndDlg, IDC_HYPER_GNU);
			ConvertStaticToHyperlink(hwndDlg, IDC_PIC_BRIO);

			return TRUE;
		}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_TITLE:
			ShellExecute(hwndDlg, TEXT("open"), TEXT("http://foldersize.sourceforge.net"), NULL, NULL, SW_SHOWNORMAL);
			return TRUE;
		case IDC_HYPER_GPL:
			ShellExecute(hwndDlg, TEXT("open"), TEXT("http://www.gnu.org/copyleft/gpl.html"), NULL, NULL, SW_SHOWNORMAL);
			return TRUE;
		case IDC_HYPER_GNU:
			ShellExecute(hwndDlg, TEXT("open"), TEXT("http://www.gnu.org"), NULL, NULL, SW_SHOWNORMAL);
			return TRUE;
		case IDC_PIC_BRIO:
			ShellExecute(hwndDlg, TEXT("open"), TEXT("mailto:brio1337@users.sourceforge.net"), NULL, NULL, SW_SHOWNORMAL);
			return TRUE;
		}
		break;

	case WM_DESTROY:
		{
			ReleaseBiggerFont(GetDlgItem(hwndDlg, IDC_TITLE));
			return TRUE;
		}
	}
	return FALSE;
}


void DoPropertySheet(HWND hwndParent)
{
	PROPSHEETHEADER psh;
	PROPSHEETPAGE psp[3];
	int i=0;

	psp[i].dwSize = sizeof(PROPSHEETPAGE);
	psp[i].dwFlags = PSP_DEFAULT;
	psp[i].hInstance = g_hInstance;
	psp[i].pszTemplate = MAKEINTRESOURCE(IDD_DISPLAY);
	psp[i].pfnDlgProc = DisplayProc;
	psp[i].lParam = 0;
	i++;

	psp[i].dwSize = sizeof(PROPSHEETPAGE);
	psp[i].dwFlags = PSP_DEFAULT;
	psp[i].hInstance = g_hInstance;
	psp[i].pszTemplate = MAKEINTRESOURCE(IDD_SERVICE);
	psp[i].pfnDlgProc = ServiceProc;
	psp[i].lParam = 0;
	i++;

	psp[i].dwSize = sizeof(PROPSHEETPAGE);
	psp[i].dwFlags = PSP_DEFAULT;
	psp[i].hInstance = g_hInstance;
	psp[i].pszTemplate = MAKEINTRESOURCE(IDD_ABOUT);
	psp[i].pfnDlgProc = AboutProc;
	psp[i].lParam = 0;
	i++;

	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_NOCONTEXTHELP | PSH_PROPSHEETPAGE | PSH_PROPTITLE;
	psh.hwndParent = hwndParent;
	psh.hInstance = g_hInstance;
	psh.pszCaption = MAKEINTRESOURCE(IDS_NAME);
	psh.nPages = i;
	psh.nStartPage = 0;
	psh.ppsp = psp;

	PropertySheet(&psh);
}