#include "StdAfx.h"
#include "PropSheet.h"
#include "Resource.h"

HINSTANCE g_hInstance = NULL;

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		g_hInstance = hInstance;
		DisableThreadLibraryCalls(hInstance);
	}
    return TRUE;
}

LONG APIENTRY CPlApplet(HWND hwndCPl, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
	switch (uMsg)
	{
	case CPL_INIT:
		return TRUE;
	case CPL_GETCOUNT:
		return 1;
	case CPL_INQUIRE:
		if (lParam1 == 0)
		{
			LPCPLINFO pInfo = (LPCPLINFO)lParam2;
			pInfo->idIcon = IDI_ICON;
			pInfo->idName = IDS_NAME;
			pInfo->idInfo = IDS_INFO;
			pInfo->lData = NULL;
			return 0;
		}
		break;
/*
	case CPL_NEWINQUIRE:
		if (lParam1 == 0)
		{
			LPNEWCPLINFO pInfo = (LPNEWCPLINFO)lParam2;
			pInfo->dwSize = sizeof(NEWCPLINFO);
			if (SUCCEEDED(CoInitialize(NULL)))
			{
				// get the icon from the shell
				SHFILEINFO shfi;
				SHGetFileInfo("", FILE_ATTRIBUTE_DIRECTORY, &shfi, sizeof(shfi), SHGFI_USEFILEATTRIBUTES | SHGFI_ICON);
//				pInfo->lData = IDI_ICON;
				pInfo->hIcon = shfi.hIcon;
				CoUninitialize();
			}
			return 0;
		}
		break;
*/
	case CPL_DBLCLK:
		if (lParam1 == 0)
		{
			DoPropertySheet(hwndCPl);
			return 0;
		}
		break;
	}
	return 0;
}