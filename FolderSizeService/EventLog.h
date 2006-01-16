#pragma once

class EventLog
{
public:
	static EventLog& Instance();

	bool ReportError(LPCTSTR pszComponent, DWORD dwError);

private:
	EventLog();
	~EventLog();

	HANDLE m_hEventLog;
};
