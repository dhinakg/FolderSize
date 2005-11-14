#pragma once

class IOptionEvents
{
public:
	virtual void EnableShellUpdate(bool bEnable) = 0;
	virtual void SetShellUpdateInterval(UINT nInterval) = 0;
//	void EnableScanner(bool bEnable);
};

class Options
{
public:
	Options(IOptionEvents* pCallback);
	~Options();

	// user options
	bool GetScannerPaused() const;
	void SetScannerPaused(bool bScannerPaused);
	HANDLE GetScannerUnpausedEvent();

	bool GetUpdateShell() const;
	void SetUpdateShell(bool bUpdateShell);

	UINT GetUpdateShellInterval() const;
	void SetUpdateShellInterval(UINT nInterval);

	enum DISPLAY_FORMAT
	{
		DF_EXPLORER,
		DF_COMPACT,
		DF_MAX
	};

	DISPLAY_FORMAT GetDisplayFormat() const;
	void SetDisplayFormat(DISPLAY_FORMAT eDisplayFormat);

	int GetNumberOfSyncScans();
	void SetNumberOfSyncScans(int nSyncScans);

	// stats
	ULONGLONG GetMillisecondsRunning();

	enum FOLDER_COUNTER
	{
		FC_UNSCANNED,
		FC_CLEAN,
		FC_DIRTY,
		FC_SCANNED,
		FC_MAX
	};
	void IncrementFolderCounter(FOLDER_COUNTER fc);
	void DecrementFolderCounter(FOLDER_COUNTER fc);
	void SetLastScannedFolder(LPCTSTR pszFolder);

	ULONG GetFolderCounter(FOLDER_COUNTER fc);
	LPCTSTR GetLastScannedFolder();

protected:
	IOptionEvents* m_pCallback;

	bool m_bUpdateShell;
	UINT m_nUpdateShellInterval;

	LARGE_INTEGER m_nPerformanceStartCount;
	LARGE_INTEGER m_nPerformanceFrequency;

	bool m_bScannerPaused;
	HANDLE m_hScannerUnpausedEvent;
	DISPLAY_FORMAT m_eDisplayFormat;
	int m_nSyncScans;

	ULONG m_fc[FC_MAX];

	TCHAR m_szLastScannedFolder[MAX_PATH];
};
