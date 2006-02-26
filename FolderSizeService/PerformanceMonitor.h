#pragma once

#include "Path.h"

#define PERFORMANCE_UPDATE_FREQUENCY 700

class PerformanceMonitor
{
public:
	PerformanceMonitor(const Path& pathVolume);
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
