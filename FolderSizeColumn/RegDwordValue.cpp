#include "StdAfx.h"
#include "RegDwordValue.h"

#include <sstream>

/////////////////////////////////////////////////////////////////////////////
// CRegDwordValue

RegDwordValue::RegDwordValue(HKEY hKey, LPCTSTR pszKeyName, LPCTSTR pszValueName) :
m_hKey(hKey),
m_strKeyName(pszKeyName),
m_strValueName(pszValueName)
{
	m_dwValue = 0;
	m_bValid = FALSE;

	m_hSubKey = NULL;

	// Create the event object now and register the wait object later.
	m_hNotify = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hWaitObject = NULL;

#ifdef LOG_REGKEY
	std::wostringstream str;
	str << L"C:\\FolderSize"
		//<< (void*)this
		<< L".log";
	m_ofLog.open(str.str().c_str(), std::ios_base::out);

	m_lCount = 0;
#endif

	::InitializeCriticalSection(&m_Cs);

	// Register the wait object.
	// Execute the WaitCallback function in the wait thread 
	// (WT_EXECUTEINWAITTHREAD) to avoid race conditions
	// of unknown origin.
	// We can afford it since the callback only resets a flag and an event object.
	::RegisterWaitForSingleObject(&m_hWaitObject, m_hNotify, WaitCallback,
		this, INFINITE, WT_EXECUTEDEFAULT | WT_EXECUTEINWAITTHREAD);
}


RegDwordValue::~RegDwordValue()
{
	// Unregister Wait for pending notifications:
	::UnregisterWaitEx(m_hWaitObject, INVALID_HANDLE_VALUE);
	m_hWaitObject = NULL;

	// Close the notification handle:
	::CloseHandle(m_hNotify);
	m_hNotify = NULL;

	// Close the subkey waited for.
	if (m_hSubKey) {
		::CloseHandle(m_hSubKey);
		m_hSubKey = NULL;
	}

	// Finally, delete the critical section.
	::DeleteCriticalSection(&m_Cs);
}


DWORD RegDwordValue::GetValue()
{
	ProvideValue();
	assert(m_bValid);
	return m_dwValue;
}


void RegDwordValue::ProvideValue()
{
	if (!m_bValid) {
		::EnterCriticalSection(&m_Cs);
		if (!m_bValid)
			InitNotify();
		::LeaveCriticalSection(&m_Cs);
	}
}


void RegDwordValue::InitNotify()
{
#ifdef LOG_REGKEY
	m_ofLog << L"InitNotify" << std::endl;
#endif

	// Try to query the value and wait for modifications in the deepest subkey
	// that exists. Do not create any keys.
	TryQueryValue();
	SetValueEvent();
}


void RegDwordValue::TryQueryValue()
{
	m_dwValue = 0;

	if (!m_hKey)
		return;

	if (m_hSubKey) {
		::RegCloseKey(m_hSubKey);
		m_hSubKey = NULL;
	}

	// Clone m_hKey.
	::RegOpenKeyEx(m_hKey, NULL, 0, KEY_READ, &m_hSubKey);
	m_strSubKeyName = m_strKeyName;

	// Query each subkey, return on failure.
	while (m_strSubKeyName.length() > 0) {
#ifdef LOG_REGKEY
		m_ofLog << m_strSubKeyName << std::endl;
#endif

		size_t iPos = m_strSubKeyName.find('\\');

		std::basic_string<TCHAR> strSubKeyName = m_strSubKeyName.substr(0, iPos);

		HKEY hSubKey = NULL;
		::RegOpenKeyEx(m_hSubKey, strSubKeyName.c_str(), 0, KEY_READ, &hSubKey);

		if (!hSubKey)
			return;

		::RegCloseKey(m_hSubKey);
		m_hSubKey = hSubKey;

		m_strSubKeyName.erase(0, iPos);
		if (m_strSubKeyName.length())
			m_strSubKeyName.erase(0, 1);
	}

	// Query value, return on failure.
	DWORD dwType, dwData, cbData;
	cbData = sizeof(DWORD);
	if (RegQueryValueEx(m_hSubKey, m_strValueName.c_str(), NULL,
		&dwType, (LPBYTE)&dwData, &cbData) != ERROR_SUCCESS)
	{
		return;
	}

	// No DWORD? Return.
	if (dwType != REG_DWORD)
		return;

	// Modify value atomically:
	m_dwValue = dwData;
}


void RegDwordValue::SetValueEvent()
{
	m_bValid = TRUE;

	// Signal m_hNotify upon any change to m_hSubKey, including deletion.
	// This in turn invokes WaitCallbac.
	::RegNotifyChangeKeyValue(m_hSubKey, m_strValueName.length() > 0 ? TRUE : FALSE,
		REG_NOTIFY_CHANGE_NAME |
		REG_NOTIFY_CHANGE_ATTRIBUTES |
		REG_NOTIFY_CHANGE_LAST_SET |
		REG_NOTIFY_CHANGE_SECURITY,
		m_hNotify,
		TRUE);
}


void CALLBACK RegDwordValue::WaitCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	assert(!TimerOrWaitFired);
	reinterpret_cast<RegDwordValue*>(lpParameter)->WaitCallback();
}


void RegDwordValue::WaitCallback()
{
#ifdef LOG_REGKEY
	m_ofLog << L"WaitCallback(" << m_lCount++ << L")" << std::endl;
#endif

	// Invalidate value and reset event.
	m_bValid = FALSE;
	::ResetEvent(m_hNotify);
}
