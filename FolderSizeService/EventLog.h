#pragma once

class EventLog
{
public:
	EventLog();
	~EventLog();

	bool ReportError(LPCTSTR pszComponent, DWORD dwError);

private:
	HANDLE m_hEventLog;
};