#pragma once

class RegDwordValue {
public:
	RegDwordValue(HKEY hKey, LPCTSTR pszKeyName, LPCTSTR pszValueName);
	~RegDwordValue();

	DWORD GetValue();

private:
	void ReadAndWatchValue();

	static void CALLBACK WaitCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired);
	void WaitCallback();

private:
	// Read-only:
	const HKEY m_hRootKey;
	const std::basic_string<TCHAR> m_strKeyName;
	const std::basic_string<TCHAR> m_strValueName;

	// Modified in main thread only:
	HANDLE m_hNotifyEvent;
	HANDLE m_hWaitObject;

#ifdef LOG_REGKEY
	std::wofstream m_ofLog;
	LONG m_lCount;
#endif

	// All following data members are synchronized by this critical section
	CRITICAL_SECTION m_Cs;

	// Atomic DWORD value:
	DWORD m_dwValue;
	bool m_bValid;
	// Handle of the key currently being watched
	HKEY m_hSubKey;
};
