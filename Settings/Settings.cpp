#include "StdAfx.h"
#include "Settings.h"


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
		if (LoadDWord(hKey, DISPLAY_FORMAT_VALUE, dw))
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
		RegSetValueEx(hKey, DISPLAY_FORMAT_VALUE, 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD));
		RegCloseKey(hKey);
	}
}

int LoadScanDriveTypes()
{
	// they're all on by default, but a 0 in the registry will turn off each type
	int DriveTypes = SCANDRIVETYPE_LOCAL | SCANDRIVETYPE_CD | SCANDRIVETYPE_REMOVABLE | SCANDRIVETYPE_NETWORK;

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, SERVICE_PARAMETERS_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD dw;
		if (LoadDWord(hKey, TEXT("ScanLocal"), dw) && dw == 0)
			DriveTypes &= ~SCANDRIVETYPE_LOCAL;
		if (LoadDWord(hKey, TEXT("ScanCD"), dw) && dw == 0)
			DriveTypes &= ~SCANDRIVETYPE_CD;
		if (LoadDWord(hKey, TEXT("ScanRemovable"), dw) && dw == 0)
			DriveTypes &= ~SCANDRIVETYPE_REMOVABLE;
		if (LoadDWord(hKey, TEXT("ScanNetwork"), dw) && dw == 0)
			DriveTypes &= ~SCANDRIVETYPE_NETWORK;

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
