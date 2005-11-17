#pragma once

#include "..\Pipe\FolderInfo.h"

class Options;

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
	Scanner(IScannerCallback* pCallback);
	~Scanner();

	void ScanFolder(LPCTSTR pszFolder);
	void Wakeup();

protected:
	static DWORD WINAPI ThreadProc(LPVOID lpParameter);
	bool GetAnItemFromTheQueue(LPTSTR pszFolder);

	IScannerCallback* m_pCallback;
	HANDLE m_hThread;
	HANDLE m_hQuitEvent;
	HANDLE m_hScanEvent;
};
