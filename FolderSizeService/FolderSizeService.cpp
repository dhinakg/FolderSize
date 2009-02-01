#include "StdAfx.h"
#include "CacheManager.h"
#include "FolderSizeSvc.h"
#include "Pipe.h"
#include "Resource.h"

void RegisterServiceDescription(SC_HANDLE hService)
{
	TCHAR szDescription[1024];
	LoadString(GetModuleHandle(NULL), IDS_SERVICE_DESCRIPTION, szDescription, 1024);
	SERVICE_DESCRIPTION sd;
	sd.lpDescription = szDescription;
	ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &sd);
}

class Service
{
public:
	Service();
	~Service();

	void SetCacheManager(CacheManager* pCacheManager);
	SERVICE_STATUS_HANDLE GetHandle();
	void WaitUntilServiceStops();
	void SetStatus(DWORD dwCurrentState = -1, DWORD dwError = NO_ERROR);

private:
	static DWORD WINAPI HandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
	DWORD HandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData);

	SERVICE_STATUS_HANDLE m_hSS;
	SERVICE_STATUS m_ss;
	HANDLE m_hQuitEvent;
	CacheManager* m_pCacheManager;
};

Service::Service()
: m_pCacheManager(NULL)
{
	m_ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	m_ss.dwCurrentState = SERVICE_START_PENDING;
	m_ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_PARAMCHANGE;
	m_ss.dwWin32ExitCode = NO_ERROR;
	m_ss.dwCheckPoint = 0;
	m_ss.dwWaitHint = 500;

	m_hSS = RegisterServiceCtrlHandlerEx(SERVICE_NAME, HandlerEx, this);
	SetStatus();

	m_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// the service description doesn't get set by the installer,
	// so the service registers its description when it is created
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (hSCManager != NULL)
	{
		SC_HANDLE hService = OpenService(hSCManager, SERVICE_NAME, SERVICE_CHANGE_CONFIG);
		if (hService != NULL)
		{
			RegisterServiceDescription(hService);
			CloseServiceHandle(hService);
		}
		CloseServiceHandle(hSCManager);
	}
}

Service::~Service()
{
	if (m_hQuitEvent != NULL)
	{
		CloseHandle(m_hQuitEvent);
	}
}

void Service::SetCacheManager(CacheManager* pCacheManager)
{
	m_pCacheManager = pCacheManager;
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
	WaitForSingleObject(m_hQuitEvent, INFINITE);
}

DWORD WINAPI Service::HandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	Service* pService = (Service*)lpContext;
	return pService->HandlerEx(dwControl, dwEventType, lpEventData);
}

DWORD Service::HandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData)
{
	switch (dwControl)
	{
	case SERVICE_CONTROL_INTERROGATE:
		SetStatus();
		return NO_ERROR;

	case SERVICE_CONTROL_PAUSE:
		SetStatus(SERVICE_PAUSE_PENDING);
		m_pCacheManager->EnableScanners(false);
		SetStatus(SERVICE_PAUSED);
		return NO_ERROR;

	case SERVICE_CONTROL_CONTINUE:
		SetStatus(SERVICE_CONTINUE_PENDING);
		m_pCacheManager->EnableScanners(true);
		SetStatus(SERVICE_RUNNING);
		return NO_ERROR;
		
	case SERVICE_CONTROL_STOP:
		SetStatus(SERVICE_STOP_PENDING);
		SetEvent(m_hQuitEvent);
		return NO_ERROR;

	// Get notified if a device (e.g. usb drive) gets removed
	case SERVICE_CONTROL_DEVICEEVENT:
		if ((dwEventType == DBT_DEVICEQUERYREMOVE || dwEventType == DBT_DEVICEREMOVECOMPLETE) &&
		   ((PDEV_BROADCAST_HDR)lpEventData)->dbch_devicetype == DBT_DEVTYP_HANDLE)
		{
			m_pCacheManager->DeviceRemoveEvent((PDEV_BROADCAST_HANDLE)lpEventData);
		}
		return NO_ERROR;

	case SERVICE_CONTROL_PARAMCHANGE:
		SetStatus();
		m_pCacheManager->ParamChange();
		return NO_ERROR;
	}

	return ERROR_CALL_NOT_IMPLEMENTED;
}


void WINAPI	ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	SetLastError(NO_ERROR);

	Service s;
	if (lpszArgv || s.GetHandle() != NULL)
	{
		CacheManager theCacheManager(s.GetHandle());
		s.SetCacheManager(&theCacheManager);

		// the number of pipes listed here is the number of simultaneous clients
		Pipe aPipes[] = {
			Pipe(&theCacheManager),
			Pipe(&theCacheManager),
			Pipe(&theCacheManager),
			Pipe(&theCacheManager),
			Pipe(&theCacheManager)
		};

		s.SetStatus(SERVICE_RUNNING);
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
	// Open	the	SCM	on this	machine.
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

	if (hSCM != NULL)
	{
		// Get our full	pathname
		TCHAR szModulePathname[MAX_PATH];
		GetModuleFileName(NULL,	szModulePathname, countof(szModulePathname));
		PathQuoteSpaces(szModulePathname);

		// Add this	service	to the SCM's database.
		SC_HANDLE hService = CreateService(hSCM, SERVICE_NAME, SERVICE_DISPLAY, 0,
			SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_IGNORE, 
			szModulePathname, NULL,	NULL, NULL,	NULL, NULL);

		if (hService != NULL)
		{
			RegisterServiceDescription(hService);
			CloseServiceHandle(hService);
		}

		CloseServiceHandle(hSCM);
	}
}

//////////////////////////////////////////////////////////////////////////////

void RemoveService()
{
	// Open	the	SCM	on this	machine.
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (hSCM != NULL)
	{
		// Open	this service for DELETE	access
		SC_HANDLE hService = OpenService(hSCM, SERVICE_NAME, DELETE);
		if (hService != NULL)
		{
			// Remove this service from	the	SCM's database.
			DeleteService(hService);
			CloseServiceHandle(hService);
		}
		CloseServiceHandle(hSCM);
	}
}

//////////////////////////////////////////////////////////////////////////////

int	WINAPI WinMain(HINSTANCE hinst,	HINSTANCE hinstExePrev,	LPSTR pszCmdLine, int nCmdShow)
{
	int nArgc;
	LPWSTR* ppArgv = CommandLineToArgvW(GetCommandLine(), &nArgc);
	if (ppArgv == NULL)
	{
		return GetLastError();
	}

	bool bStartService = true;
	bool bDebug = false;

	for (int i = 1; i < nArgc; i++)
	{
		if ((ppArgv[i][0] == TEXT('-')) || (ppArgv[i][0] == TEXT('/')))
		{
			if (lstrcmpi(&ppArgv[i][1], TEXT("install")) == 0)
			{
				InstallService();
				bStartService = false;
			}

			if (lstrcmpi(&ppArgv[i][1], TEXT("remove")) == 0)
			{
				RemoveService();
				bStartService = false;
			}

			if (lstrcmpi(&ppArgv[i][1], TEXT("debug")) == 0)
			{
				bDebug = true;
				bStartService = false;
			}
		}
	}

	GlobalFree(ppArgv);

	if (bDebug)
	{
		// Running as EXE not as service, just run the service for debugging
		LPTSTR* p = bDebug ? (LPTSTR*)1 : (LPTSTR*)0;
		ServiceMain(0, p);
	}

	if (bStartService)
	{
		SERVICE_TABLE_ENTRY ServiceTable[] =
		{
			{ SERVICE_NAME, ServiceMain },
			{ NULL,		  NULL }   // End of list
		};
		if (!StartServiceCtrlDispatcher(ServiceTable))
			return GetLastError();
	}

	return 0;
}