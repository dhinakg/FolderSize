#include "StdAfx.h"
#include "Service.h"
#include "Resource.h"
#include "..\FolderSizeService\FolderSizeSvc.h"


struct MODIFY_SERVICE_PARAMS
{
	DWORD dwServiceAccess;
	DWORD dwControl;
	DWORD dwPending;
	DWORD dwState;
};

MODIFY_SERVICE_PARAMS msp[MS_MAX] =
{
	{ SERVICE_START,          0,                        SERVICE_START_PENDING,    SERVICE_RUNNING },
	{ SERVICE_STOP,           SERVICE_CONTROL_STOP,     SERVICE_STOP_PENDING,     SERVICE_STOPPED },
	{ SERVICE_PAUSE_CONTINUE, SERVICE_CONTROL_PAUSE,    SERVICE_PAUSE_PENDING,    SERVICE_PAUSED  },
	{ SERVICE_PAUSE_CONTINUE, SERVICE_CONTROL_CONTINUE, SERVICE_CONTINUE_PENDING, SERVICE_RUNNING },
	{ SERVICE_PAUSE_CONTINUE, SERVICE_CONTROL_PARAMCHANGE, 0,                     0               }
};

DWORD ModifyService(MODIFY_SERVICE ms)
{
	DWORD dwError = 0;
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (hSCM != NULL)
	{
		bool bWait = ms != MS_PARAMCHANGE;
		DWORD dwDesiredAccess = msp[ms].dwServiceAccess;
		if (bWait)
			dwDesiredAccess |= SERVICE_QUERY_STATUS;
		SC_HANDLE hService = OpenService(hSCM, SERVICE_NAME, dwDesiredAccess);
		if (hService != NULL)
		{
			SERVICE_STATUS ss;
			BOOL bRet;
			if (ms == MS_START)
			{
				bRet = StartService(hService, 0, NULL);
				if (bRet)
					bRet = QueryServiceStatus(hService, &ss);
			}
			else
			{
				bRet = ControlService(hService, msp[ms].dwControl, &ss);
			}
			if (bRet)
			{
				if (bWait)
				{
					HCURSOR hPrevCursor;
					bool bChangedCursor = false;

					while (true)
					{
						if (ss.dwCurrentState == msp[ms].dwState)
						{
							break;
						}
						else if (ss.dwCurrentState != msp[ms].dwPending)
						{
							// We didn't end up in the state we thought we were going to be in.
							// Hopefully the server gave us a handy error code.
							dwError = ss.dwWin32ExitCode;
							break;
						}
						else
						{
							// sleep sucks... show the wait cursor so the user knows we're waiting
							if (!bChangedCursor)
							{
								bChangedCursor = true;
								hPrevCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
							}
							// wait time algorithm from MSDN "Starting a Service"
							DWORD dwWaitTime = ss.dwWaitHint / 10;
							if( dwWaitTime < 500 )
								dwWaitTime = 500;
							else if ( dwWaitTime > 10000 )
								dwWaitTime = 10000;

							Sleep(dwWaitTime);
							if (!QueryServiceStatus(hService, &ss))
							{
								dwError = GetLastError();
								break;
							}
						}
					}

					if (bChangedCursor)
						SetCursor(hPrevCursor);

				}
			}
			else
			{
				dwError = GetLastError();
			}
			CloseServiceHandle(hService);
		}
		else
		{
			dwError = GetLastError();
		}
		CloseServiceHandle(hSCM);
	}
	else
	{
		dwError = GetLastError();
	}

	return dwError;
}

DWORD GetServiceStatus()
{
	DWORD dwStatus = 0;

	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (hSCM != NULL)
	{
		SC_HANDLE hService = OpenService(hSCM, SERVICE_NAME, SERVICE_QUERY_STATUS);
		if (hService != NULL)
		{
			SERVICE_STATUS ss;
			if (QueryServiceStatus(hService, &ss))
			{
				if (ss.dwCurrentState <= SERVICE_PAUSED)
				{
					dwStatus = ss.dwCurrentState;
				}
			}
			CloseServiceHandle(hService);
		}
		CloseServiceHandle(hSCM);
	}

	return dwStatus;
}
