#include "StdAfx.h"
#include "EventLog.h"
#include "FolderSizeSvc.h"


EventLog::EventLog()
{
	m_hEventLog = RegisterEventSource(NULL, SERVICE_NAME);
}

EventLog::~EventLog()
{
	DeregisterEventSource(m_hEventLog);
}

EventLog& EventLog::Instance()
{
	static EventLog log;
	return log;
}

bool EventLog::ReportError(LPCTSTR pszComponent, DWORD dwError)
{
	TCHAR szMessage[1024];
	wsprintf(szMessage, TEXT("%s reports error %d"), pszComponent, dwError);
	LPTSTR pszError = NULL;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
	              NULL, dwError, 0, (LPTSTR)&pszError, 0, NULL) > 0)
	{
		lstrcat(szMessage, TEXT(": "));
		lstrcat(szMessage, pszError);
		LocalFree(pszError);
	}
	LPCTSTR pszMessage = szMessage;
	return ReportEvent(m_hEventLog, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 1, 0, &pszMessage, NULL) != 0;
}

bool EventLog::ReportWarning(LPCTSTR pszMsg)
{
	return ReportEvent(m_hEventLog, EVENTLOG_WARNING_TYPE, 0, 0, NULL, 1, 0, &pszMsg, NULL) != 0;
}
