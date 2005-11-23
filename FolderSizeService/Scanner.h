#pragma once

#include "..\Pipe\FolderInfo.h"

class PerformanceMonitor;

class IScannerCallback
{
public:
	virtual void FoundFolder(LPCTSTR pszFolder) = 0;
	virtual void GotScanResult(LPCTSTR pszFolder, const FOLDERINFO& nSize) = 0;
	virtual bool GetNextScanFolder(LPTSTR pszFolder) = 0;
};

class Scanner
{
public:
	Scanner(LPCTSTR pszVolume, IScannerCallback* pCallback);
	~Scanner();

	void ScanFolder(LPCTSTR pszFolder);
	void Wakeup();

protected:
	static DWORD WINAPI ThreadProc(LPVOID lpParameter);
	void ThreadProc();
	bool GetAnItemFromTheQueue(LPTSTR pszFolder);

	PerformanceMonitor* m_pPerformanceMonitor;
	IScannerCallback* m_pCallback;
	HANDLE m_hThread;
	HANDLE m_hQuitEvent;
	HANDLE m_hScanEvent;
};
