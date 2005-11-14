#include "StdAfx.h"
#include "DebugMessage.h"
#include "FolderSizeSvc.h"

void PostDebugMessage(LPCTSTR pszComponent, DWORD dwError)
{
	// Write out errors to the event log
	HANDLE hEventLog = RegisterEventSource(NULL, SERVICE_NAME);
	TCHAR szMessage[1024];
	wsprintf(szMessage, TEXT("%s failed: %#08X"), pszComponent, dwError);
	LPTSTR pszError = NULL;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
	              NULL, dwError, 0, (LPTSTR)&pszError, 0, NULL) > 0)
	{
		lstrcat(szMessage, TEXT(" "));
		lstrcat(szMessage, pszError);
		LocalFree(pszError);
	}
	LPCTSTR pszMessage = szMessage;
	ReportEvent(hEventLog, EVENTLOG_ERROR_TYPE, 1, 0, NULL, 1, 0, &pszMessage, NULL);
	DeregisterEventSource(hEventLog);

/*
	// Write out the message to Debug Out
	TCHAR szMessage[1024];
	wsprintf(szMessage, TEXT("Folder Size service: %s failed: %#08x"), pszComponent, dwError);
	LPTSTR pszError = NULL;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
	              NULL, dwError, 0, (LPTSTR)&pszError, 0, NULL) > 0)
	{
		lstrcat(szMessage, TEXT(" "));
		lstrcat(szMessage, pszError);
		LocalFree(pszError);
	}
	lstrcat(szMessage, TEXT("\n"));
	OutputDebugString(szMessage);
*/
}
