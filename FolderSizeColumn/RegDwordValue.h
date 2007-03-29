#pragma once

class RegDwordValue {
public:
	RegDwordValue(HKEY hKey, LPCTSTR pszKeyName, LPCTSTR pszValueName);
	~RegDwordValue();

	DWORD GetValue();

private:
	void ProvideValue();
	void InitNotify();
	void TryQueryValue();
	void SetValueEvent();

	static void CALLBACK WaitCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired);
	void WaitCallback();

private:
	// Read-only:
	const HKEY m_hKey;
	const std::basic_string<TCHAR> m_strKeyName;
	const std::basic_string<TCHAR> m_strValueName;

	// Atomic DWORD value:
	DWORD m_dwValue;
	BOOL m_bValid;

	// Used by QueryValue:
	HKEY m_hSubKey;
	std::basic_string<TCHAR> m_strSubKeyName;

	// Modified in main thread only:
	HANDLE m_hNotify;
	HANDLE m_hWaitObject;

#ifdef LOG_REGKEY
	std::wofstream m_ofLog;
	LONG m_lCount;
#endif

	CRITICAL_SECTION m_Cs;
};
