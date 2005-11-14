#include "StdAfx.h"
#include "Options.h"

#define REG_KEY                        _T("Software\\Brio\\FolderSize")
#define REG_VALUE_UPDATESHELL          _T("UpdateShell")
#define REG_VALUE_UPDATESHELLINTERVAL  _T("UpdateShellInterval")
#define REG_VALUE_SCANNERPAUSED        _T("ScannerPaused")
#define REG_VALUE_DISPLAYFORMAT        _T("DisplayFormat")
#define REG_VALUE_SYNCSCANS            _T("SynchronousScans")

Options::Options(IOptionEvents* pCallback)
: m_pCallback(pCallback), m_bUpdateShell(false), m_eDisplayFormat(DF_EXPLORER), m_bScannerPaused(false), m_nSyncScans(5), m_nUpdateShellInterval(2000)
{
	ZeroMemory(&m_fc, sizeof(m_fc));

	m_szLastScannedFolder[0] = _T('\0');

	QueryPerformanceCounter(&m_nPerformanceStartCount);
	QueryPerformanceFrequency(&m_nPerformanceFrequency);

	// load from the registry
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD dwType, dwData, cbData;
		cbData = sizeof(DWORD);
		if (RegQueryValueEx(hKey, REG_VALUE_SCANNERPAUSED, NULL, &dwType, (LPBYTE)&dwData, &cbData) == ERROR_SUCCESS)
		{
			if (dwType == REG_DWORD)
			{
				m_bScannerPaused = dwData != 0;
			}
		}
		cbData = sizeof(DWORD);
		if (RegQueryValueEx(hKey, REG_VALUE_DISPLAYFORMAT, NULL, &dwType, (LPBYTE)&dwData, &cbData) == ERROR_SUCCESS)
		{
			if (dwType == REG_DWORD && dwData >= 0 && dwData < DF_MAX)
			{
				m_eDisplayFormat = (DISPLAY_FORMAT)dwData;
			}
		}
		cbData = sizeof(DWORD);
		if (RegQueryValueEx(hKey, REG_VALUE_SYNCSCANS, NULL, &dwType, (LPBYTE)&dwData, &cbData) == ERROR_SUCCESS)
		{
			if (dwType == REG_DWORD)
			{
				m_nSyncScans = dwData;
			}
		}
		cbData = sizeof(DWORD);
		if (RegQueryValueEx(hKey, REG_VALUE_UPDATESHELL, NULL, &dwType, (LPBYTE)&dwData, &cbData) == ERROR_SUCCESS)
		{
			if (dwType == REG_DWORD)
			{
				m_bUpdateShell = dwData != 0;
			}
		}
		cbData = sizeof(DWORD);
		if (RegQueryValueEx(hKey, REG_VALUE_UPDATESHELLINTERVAL, NULL, &dwType, (LPBYTE)&dwData, &cbData) == ERROR_SUCCESS)
		{
			if (dwType == REG_DWORD)
			{
				m_nUpdateShellInterval = dwData;
			}
		}
		RegCloseKey(hKey);
	}

	m_hScannerUnpausedEvent = CreateEvent(NULL, TRUE, !m_bScannerPaused, NULL);
}

Options::~Options()
{
	CloseHandle(m_hScannerUnpausedEvent);
}


bool Options::GetUpdateShell() const
{
	return m_bUpdateShell;
}

void Options::SetUpdateShell(bool bUpdateShell)
{
	m_bUpdateShell = bUpdateShell;
	m_pCallback->EnableShellUpdate(bUpdateShell);

	HKEY hKey;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		DWORD dwData = bUpdateShell;
		RegSetValueEx(hKey, REG_VALUE_UPDATESHELL, 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD));
		RegCloseKey(hKey);
	}
}

UINT Options::GetUpdateShellInterval() const
{
	return m_nUpdateShellInterval;
}

void Options::SetUpdateShellInterval(UINT nInterval)
{
	m_nUpdateShellInterval = nInterval;
	m_pCallback->SetShellUpdateInterval(nInterval);

	HKEY hKey;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		DWORD dwData = nInterval;
		RegSetValueEx(hKey, REG_VALUE_UPDATESHELLINTERVAL, 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD));
		RegCloseKey(hKey);
	}
}

Options::DISPLAY_FORMAT Options::GetDisplayFormat() const
{
	return m_eDisplayFormat;
}

void Options::SetDisplayFormat(DISPLAY_FORMAT eDisplayFormat)
{
	m_eDisplayFormat = eDisplayFormat;

	HKEY hKey;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		DWORD dwData = eDisplayFormat;
		RegSetValueEx(hKey, REG_VALUE_DISPLAYFORMAT, 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD));
		RegCloseKey(hKey);
	}
}

bool Options::GetScannerPaused() const
{
	return m_bScannerPaused;
}

void Options::SetScannerPaused(bool bScannerPaused)
{
	m_bScannerPaused = bScannerPaused;

	if (bScannerPaused)
	{
		ResetEvent(m_hScannerUnpausedEvent);
	}
	else
	{
		SetEvent(m_hScannerUnpausedEvent);
	}

	HKEY hKey;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		DWORD dwData = bScannerPaused;
		RegSetValueEx(hKey, REG_VALUE_SCANNERPAUSED, 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD));
		RegCloseKey(hKey);
	}
}

int Options::GetNumberOfSyncScans()
{
	return m_nSyncScans;
}

void Options::SetNumberOfSyncScans(int nSyncScans)
{
	m_nSyncScans = nSyncScans;

	HKEY hKey;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		DWORD dwData = nSyncScans;
		RegSetValueEx(hKey, REG_VALUE_SYNCSCANS, 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD));
		RegCloseKey(hKey);
	}
}

HANDLE Options::GetScannerUnpausedEvent()
{
	return m_hScannerUnpausedEvent;
}

void Options::IncrementFolderCounter(FOLDER_COUNTER fc)
{
	InterlockedIncrement((LPLONG)&m_fc[fc]);
}

void Options::DecrementFolderCounter(FOLDER_COUNTER fc)
{
	InterlockedDecrement((LPLONG)&m_fc[fc]);
}

ULONG Options::GetFolderCounter(FOLDER_COUNTER fc)
{
	return m_fc[fc];
}

void Options::SetLastScannedFolder(LPCTSTR pszFolder)
{
	_tcscpy(m_szLastScannedFolder, pszFolder);
}

LPCTSTR Options::GetLastScannedFolder()
{
	return m_szLastScannedFolder;
}

ULONGLONG Options::GetMillisecondsRunning()
{
	LARGE_INTEGER nPerformanceCount;
	QueryPerformanceCounter(&nPerformanceCount);
	LARGE_INTEGER nRunning;
	nRunning.QuadPart = nPerformanceCount.QuadPart - m_nPerformanceStartCount.QuadPart;
	return nRunning.QuadPart * 1000 / m_nPerformanceFrequency.QuadPart;
}