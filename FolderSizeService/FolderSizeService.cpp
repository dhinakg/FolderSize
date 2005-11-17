#include "StdAfx.h"
#include "CacheManager.h"
#include "DebugMessage.h"
#include "FolderSizeSvc.h"


CacheManager* g_pCacheManager = NULL;


// it's important that a pointer to this class
// is also a pointer to OVERLAPPED
class Pipe
{
public:
	Pipe();
	~Pipe();

private:
	static DWORD WINAPI PipeThread(LPVOID lpParameter);

	HANDLE m_hThread;
	HANDLE m_hQuitEvent;
};


Pipe::Pipe()
: m_hQuitEvent(NULL), m_hThread(NULL)
{
	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	DWORD dwThreadId;
	m_hThread = CreateThread(NULL, 0, PipeThread, this, 0, &dwThreadId);
}

Pipe::~Pipe()
{
	SetEvent(m_hQuitEvent);
	WaitForSingleObject(m_hThread, INFINITE);
	CloseHandle(m_hThread);
	CloseHandle(m_hQuitEvent);
}

void HandlePipeClient(HANDLE hPipe)
{
	PIPE_CLIENT_REQUEST pcr;
	if (ReadRequest(hPipe, pcr))
	{
		switch (pcr)
		{
		case PCR_GETFOLDERSIZE:
			WCHAR szFile[MAX_PATH];
			if (ReadString(hPipe, szFile, MAX_PATH))
			{
				FOLDERINFO2 Size;
				g_pCacheManager->GetInfoForFolder(szFile, Size);
				WriteGetFolderSize(hPipe, Size);
			}
			break;

		case PCR_GETUPDATEDFOLDERS:
			Strings strsBrowsed, strsUpdated;
			if (ReadStringList(hPipe, strsBrowsed))
			{
				g_pCacheManager->GetUpdateFoldersForBrowsedFolders(strsBrowsed, strsUpdated);
				WriteStringList(hPipe, strsUpdated);
			}
			break;
		}
	}
	FlushFileBuffers(hPipe);
	DisconnectNamedPipe(hPipe);
}

DWORD WINAPI Pipe::PipeThread(LPVOID lpParameter)
{
	Pipe* pPipe = (Pipe*)lpParameter;

	HANDLE hPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\") PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_BYTE, PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE_OUT, PIPE_BUFFER_SIZE_IN, PIPE_DEFAULT_TIME_OUT, NULL);

	if (hPipe == INVALID_HANDLE_VALUE)
	{
		return GetLastError();
	}
	
	HANDLE hWaitHandles[2];
	hWaitHandles[0] = pPipe->m_hQuitEvent;
	hWaitHandles[1] = hPipe;

	OVERLAPPED o;
	ZeroMemory(&o, sizeof(o));

	while (true)
	{
		if (ConnectNamedPipe(hPipe, &o))
		{
			HandlePipeClient(hPipe);
		}
		else
		{
			DWORD dwLastError = GetLastError();
			if (dwLastError == ERROR_PIPE_CONNECTED)
			{
				HandlePipeClient(hPipe);
			}
			else if (dwLastError == ERROR_IO_PENDING)
			{
				DWORD dwWait = WaitForMultipleObjects(2, hWaitHandles, FALSE, INFINITE);
				if (dwWait == WAIT_OBJECT_0 + 1)
				{
					HandlePipeClient(hPipe);
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

	CloseHandle(hPipe);
	return 0;
}


class Service
{
public:
	Service();
	~Service();

	SERVICE_STATUS_HANDLE GetHandle();
	void WaitUntilServiceStops();
	void SetStatus(DWORD dwCurrentState = -1, DWORD dwError = NO_ERROR);

private:
	static DWORD WINAPI HandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);

	SERVICE_STATUS_HANDLE m_hSS;
	SERVICE_STATUS m_ss;
	HANDLE m_hQuitEvent;
};

Service::Service()
{
	m_ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	m_ss.dwCurrentState = SERVICE_START_PENDING;
	m_ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
	m_ss.dwWin32ExitCode = NO_ERROR;
	m_ss.dwCheckPoint = 0;
	m_ss.dwWaitHint = 2000;

	m_hSS = RegisterServiceCtrlHandlerEx(SERVICE_NAME, HandlerEx, this);
	SetStatus();

	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

Service::~Service()
{
	if (m_hQuitEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hQuitEvent);
	}
}

SERVICE_STATUS_HANDLE Service::GetHandle()
{
	return m_hSS;
}

void Service::SetStatus(DWORD dwCurrentState, DWORD dwError)
{
	if (dwCurrentState != -1)
	{
		m_ss.dwCurrentState = dwCurrentState;
	}
	m_ss.dwWin32ExitCode = dwError;
	if (m_hSS != NULL)
	{
		SetServiceStatus(m_hSS, &m_ss);
	}
}

void Service::WaitUntilServiceStops()
{
	SetStatus(SERVICE_RUNNING);
	WaitForSingleObject(m_hQuitEvent, INFINITE);
	SetStatus(SERVICE_STOP_PENDING);
}

DWORD WINAPI Service::HandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	Service* pService = (Service*)lpContext;
	switch (dwControl)
	{
	case SERVICE_CONTROL_INTERROGATE:
		pService->SetStatus();
		return NO_ERROR;

	case SERVICE_CONTROL_PAUSE:
		pService->SetStatus(SERVICE_PAUSE_PENDING);
		g_pCacheManager->EnableScanners(false);
		pService->SetStatus(SERVICE_PAUSED);
		return NO_ERROR;

	case SERVICE_CONTROL_CONTINUE:
		pService->SetStatus(SERVICE_CONTINUE_PENDING);
		g_pCacheManager->EnableScanners(true);
		pService->SetStatus(SERVICE_RUNNING);
		return NO_ERROR;
		
	case SERVICE_CONTROL_STOP:
		SetEvent(pService->m_hQuitEvent);
		return NO_ERROR;

	case SERVICE_CONTROL_DEVICEEVENT:
		if ((dwEventType == DBT_DEVICEQUERYREMOVE || dwEventType == DBT_DEVICEREMOVECOMPLETE) &&
		   ((PDEV_BROADCAST_HDR)lpEventData)->dbch_devicetype == DBT_DEVTYP_HANDLE)
		{
			g_pCacheManager->DeviceRemoveEvent((PDEV_BROADCAST_HANDLE)lpEventData);
		}
		return NO_ERROR;
	}

	return ERROR_CALL_NOT_IMPLEMENTED;
}


#define NUM_PIPES 5


void WINAPI	ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	SetLastError(NO_ERROR);

	Service s;
	if (lpszArgv || s.GetHandle() != NULL)
	{
		CacheManager theCacheManager(s.GetHandle());
		g_pCacheManager = &theCacheManager;

		// create some pipes
		Pipe Pipes[NUM_PIPES];
		s.WaitUntilServiceStops();
	}

	DWORD dwError = GetLastError();
	if (dwError == ERROR_IO_PENDING)
		dwError = NO_ERROR;

	s.SetStatus(SERVICE_STOPPED, dwError);
}

//////////////////////////////////////////////////////////////////////////////

void InstallService()
{
	TCHAR szModulePathname[_MAX_PATH];
	SC_HANDLE hService;

	// Open	the	SCM	on this	machine.
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

	// Get our full	pathname
	GetModuleFileName(NULL,	szModulePathname, countof(szModulePathname));
	PathQuoteSpaces(szModulePathname);

	// Add this	service	to the SCM's database.
	hService = CreateService(hSCM, SERVICE_NAME, SERVICE_DISPLAY, 0,
		SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_IGNORE, 
		szModulePathname, NULL,	NULL, NULL,	NULL, NULL);

	if (hService != NULL)
	{
//		SERVICE_DESCRIPTION sd;
//		sd.lpDescription = TEXT("While the service runs, it keeps a cache in memory of sizes of folders viewed in Explorer. Folders are cached in the background. Disable this service to stop monitoring disk activity and stop background scanning.");
//		ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &sd);
		CloseServiceHandle(hService);
	}

	CloseServiceHandle(hSCM);
}

//////////////////////////////////////////////////////////////////////////////

void RemoveService()
{
	// Open	the	SCM	on this	machine.
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

	// Open	this service for DELETE	access
	SC_HANDLE hService = OpenService(hSCM, SERVICE_NAME, DELETE);

	// Remove this service from	the	SCM's database.
	DeleteService(hService);

	// Close the service and the SCM
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);
}

//////////////////////////////////////////////////////////////////////////////

int	WINAPI WinMain(HINSTANCE hinst,	HINSTANCE hinstExePrev,	LPSTR pszCmdLine, int nCmdShow)
{
	int nArgc = __argc;
	LPCTSTR *ppArgv = (LPCTSTR*)	CommandLineToArgvW(GetCommandLine(), &nArgc);

	BOOL	fStartService =	(nArgc < 2), fDebug	= FALSE;

	for (int i = 1; i < nArgc; i++) {
		if ((ppArgv[i][0] ==	__TEXT('-')) ||	(ppArgv[i][0] == __TEXT('/'))) {
			// Command line switch
			if (lstrcmpi(&ppArgv[i][1], __TEXT("install")) == 0)	
				InstallService();

			if (lstrcmpi(&ppArgv[i][1], __TEXT("remove")) == 0)
				RemoveService();

			if (lstrcmpi(&ppArgv[i][1], __TEXT("debug")) == 0)
				fDebug = TRUE;
		}
	}

	HeapFree(GetProcessHeap(), 0, (PVOID) ppArgv);

	if (fDebug)
	{
		// Running as EXE not as service, just run the service for debugging
		LPTSTR* p = fDebug ? (LPTSTR*)1 : (LPTSTR*)0;
		ServiceMain(0, p);
	}

	if (fStartService)
	{
		SERVICE_TABLE_ENTRY ServiceTable[] =
		{
			{ SERVICE_NAME, ServiceMain },
			{ NULL,		  NULL }   // End of list
		};
		StartServiceCtrlDispatcher(ServiceTable);
	}

	return 0;
}