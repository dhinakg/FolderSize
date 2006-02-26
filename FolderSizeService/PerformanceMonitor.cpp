#include "StdAfx.h"
#include "PerformanceMonitor.h"

#define PERFORMANCE_THRESHOLD 0.7

PerformanceMonitor::PerformanceMonitor(const Path& pathVolume) :
	m_bDiskQueueTooLong(false), m_hQuitEvent(NULL), m_hThread(NULL)
{
	if (PathGetDriveNumber(pathVolume.c_str()) >= 0)
	{
		// create the name of the counter to monitor
		wsprintf(m_szCounter, _T("\\LogicalDisk(%c:)\\Avg. Disk Queue Length"), pathVolume[0]);

		m_hQuitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		DWORD dwThreadId;
		m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, &dwThreadId);
	}
}

PerformanceMonitor::~PerformanceMonitor()
{
	if (m_hThread != NULL)
	{
		SetEvent(m_hQuitEvent);
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
		CloseHandle(m_hQuitEvent);
	}
}

bool PerformanceMonitor::IsDiskQueueTooLong()
{
	return m_bDiskQueueTooLong;
}

DWORD WINAPI PerformanceMonitor::ThreadProc(LPVOID lpParam)
{
	PerformanceMonitor* pPerformanceMonitor = (PerformanceMonitor*)lpParam;
	pPerformanceMonitor->ThreadProc();
	return 0;
}

void PerformanceMonitor::ThreadProc()
{
	PDH_STATUS status;
	PDH_HQUERY hQuery;
	if ((status = PdhOpenQuery(NULL, 0, &hQuery)) == ERROR_SUCCESS)
	{
		PDH_HCOUNTER hCounter;
		if ((status = PdhAddCounter(hQuery, m_szCounter, 0, &hCounter)) == ERROR_SUCCESS)
		{
			status = PdhCollectQueryData(hQuery);
			if (status == ERROR_SUCCESS)
			{
				while (WaitForSingleObject(m_hQuitEvent, PERFORMANCE_UPDATE_FREQUENCY) == WAIT_TIMEOUT)
				{
					PdhCollectQueryData(hQuery);
					PDH_FMT_COUNTERVALUE Value;
					if ((status = PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, NULL, &Value)) == ERROR_SUCCESS)
					{
						m_bDiskQueueTooLong = Value.doubleValue > PERFORMANCE_THRESHOLD;
					}
				}
			}
		}
		PdhCloseQuery(hQuery);
	}
}
