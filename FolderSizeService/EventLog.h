#pragma once

class EventLog
{
public:
	static EventLog& Instance();

	bool ReportError(LPCTSTR pszComponent, DWORD dwError);
	bool ReportWarning(LPCTSTR pszMsg);

private:
	EventLog();
	~EventLog();

	HANDLE m_hEventLog;
};
