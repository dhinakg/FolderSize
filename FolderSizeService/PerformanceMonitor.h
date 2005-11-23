#pragma once

#define PERFORMANCE_UPDATE_FREQUENCY 500

class PerformanceMonitor
{
public:
	PerformanceMonitor(LPCTSTR pszVolume);
	~PerformanceMonitor();

	bool IsDiskQueueTooLong();

private:
	static DWORD WINAPI ThreadProc(LPVOID lpParam);
	void ThreadProc();

	HANDLE m_hThread;
	HANDLE m_hQuitEvent;
	bool m_bDiskQueueTooLong;
	TCHAR m_szCounter[256];
};
