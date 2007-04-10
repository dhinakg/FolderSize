#include "StdAfx.h"
#include "RegDwordValue.h"

#include <sstream>

/////////////////////////////////////////////////////////////////////////////
// CRegDwordValue

RegDwordValue::RegDwordValue(HKEY hKey, LPCTSTR pszKeyName, LPCTSTR pszValueName) :
m_hRootKey(hKey),
m_strKeyName(pszKeyName),
m_strValueName(pszValueName),
m_dwValue(0),
m_bValid(false),
m_hSubKey(NULL)
{
	assert(hKey);
	assert(pszKeyName);
	assert(pszValueName);

	// Create the event object now and register the wait object later.
	m_hNotifyEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

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
	::RegisterWaitForSingleObject(&m_hWaitObject, m_hNotifyEvent, WaitCallback,
		this, INFINITE, WT_EXECUTEDEFAULT | WT_EXECUTEINWAITTHREAD);
}


RegDwordValue::~RegDwordValue()
{
	// Unregister Wait for pending notifications:
	::UnregisterWaitEx(m_hWaitObject, INVALID_HANDLE_VALUE);

	// Close the notification handle:
	::CloseHandle(m_hNotifyEvent);

	// Close the subkey waited for.
	if (m_hSubKey)
		::CloseHandle(m_hSubKey);

	// Finally, delete the critical section.
	::DeleteCriticalSection(&m_Cs);
}

DWORD RegDwordValue::GetValue()
{
	DWORD dwValue = 0;
	::EnterCriticalSection(&m_Cs);
	// if we're not waiting on a key, try to read it and wait on it
	if (m_hSubKey == NULL)
		ReadAndWatchValue();
	if (m_bValid)
		dwValue = m_dwValue;
	::LeaveCriticalSection(&m_Cs);
	return dwValue;
}

void RegDwordValue::ReadAndWatchValue()
{
#ifdef LOG_REGKEY
	m_ofLog << L"InitNotify" << std::endl;
#endif

	// Try to query the value and wait for modifications in the deepest subkey
	// that exists. Do not create any keys.
	bool bDeepestKey = true;
	for (std::basic_string<TCHAR> strSubKeyName = m_strKeyName;
		 !strSubKeyName.empty();
		 strSubKeyName.erase(strSubKeyName.rfind('\\')))
	{
		HKEY hSubKey;
		REGSAM samDesired = bDeepestKey ? KEY_QUERY_VALUE|KEY_NOTIFY : KEY_NOTIFY;
		if (RegOpenKeyEx(m_hRootKey, strSubKeyName.c_str(), 0, samDesired, &hSubKey) == ERROR_SUCCESS)
		{
			// Keep this handle around so we can watch it.
			m_hSubKey = hSubKey;

			// bGotDeepestKey is still true if this is the key that has the value we want to check.
			if (bDeepestKey)
			{
				// Signal m_hNotifyEvent if any value in m_hSubKey changes.
				::RegNotifyChangeKeyValue(m_hSubKey, FALSE, REG_NOTIFY_CHANGE_LAST_SET, m_hNotifyEvent, TRUE);

				DWORD dwType, dwData, cbData;
				cbData = sizeof(DWORD);
				if (RegQueryValueEx(m_hSubKey, m_strValueName.c_str(), NULL,
					&dwType, (LPBYTE)&dwData, &cbData) == ERROR_SUCCESS)
				{
					if (dwType == REG_DWORD)
					{
						m_dwValue = dwData;
						m_bValid = true;
					}
				}
			}
			else
			{
				// Signal m_hNotifyEvent if a new subkey is added.
				::RegNotifyChangeKeyValue(m_hSubKey, FALSE, REG_NOTIFY_CHANGE_NAME, m_hNotifyEvent, TRUE);
			}

			return;
		}

		bDeepestKey = false;
	}
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

	// Invalidate value.
	::EnterCriticalSection(&m_Cs);
	m_bValid = false;
	if (m_hSubKey != NULL)
	{
		CloseHandle(m_hSubKey);
		m_hSubKey = NULL;
	}
	::LeaveCriticalSection(&m_Cs);
}
