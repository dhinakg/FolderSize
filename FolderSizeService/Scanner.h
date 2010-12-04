#pragma once

#include "Path.h"
#include "..\Common\FolderInfo.h"

class PerformanceMonitor;

class IScannerCallback
{
public:
	virtual void FoundFolder(const Path& path) = 0;
	virtual void GotScanResult(const Path& path, const FOLDERINFO& nSize, LONGLONG nTime) = 0;
	virtual bool GetNextScanFolder(Path& path) = 0;
};

class Scanner
{
public:
	Scanner(const Path& pathVolume, IScannerCallback* pCallback);
	~Scanner();

	void ScanFolder(const Path& path);
	void Wakeup();

protected:
	static DWORD WINAPI ThreadProc(LPVOID lpParameter);
	void ThreadProc();
	bool GetAnItemFromTheQueue(Path& path);

	PerformanceMonitor* m_pPerformanceMonitor;
	IScannerCallback* m_pCallback;
	HANDLE m_hThread;
	HANDLE m_hQuitEvent;
	HANDLE m_hScanEvent;
	DWORD m_BytesPerCluster;
	bool m_bCompressed;
};
