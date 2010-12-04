#include "StdAfx.h"
#include "FolderSizeModule.h"
#include "../Common/Settings.h"


CFolderSizeModule::CFolderSizeModule()
{
	m_pShellUpdate = NULL;
	m_pRegDisplayFormat = NULL;
}


void CFolderSizeModule::InitWatchers()
{
	if (m_pShellUpdate == NULL)
	{
		m_pShellUpdate = new ShellUpdate;
	}

	if (m_pRegDisplayFormat == NULL)
	{
		m_pRegDisplayFormat = new RegDwordValue(HKEY_CURRENT_USER, COLUMN_SETTINGS_KEY, DISPLAY_FORMAT_VALUE);
	}
}


void CFolderSizeModule::DestroyWatchers()
{
	if (m_pShellUpdate != NULL)
	{
		delete m_pShellUpdate;
		m_pShellUpdate = NULL;
	}

	if (m_pRegDisplayFormat != NULL)
	{
		delete m_pRegDisplayFormat;
		m_pRegDisplayFormat = NULL;
	}
}



CFolderSizeModule _AtlModule;

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	return _AtlModule.DllMain(dwReason, lpReserved); 
}


// Used to determine whether the DLL can be unloaded by OLE
STDAPI DllCanUnloadNow(void)
{
	HRESULT hr = _AtlModule.DllCanUnloadNow();
	if (hr == S_OK)
	{
		_AtlModule.DestroyWatchers();
	}
	return hr;
}


// Returns a class factory to create an object of the requested type
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	HRESULT hr = _AtlModule.DllGetClassObject(rclsid, riid, ppv);
	if (hr == S_OK)
	{
		// Start polling
		_AtlModule.InitWatchers();
	}
	return hr;
}


// DllRegisterServer - Adds entries to the system registry
STDAPI DllRegisterServer(void)
{
	// registers object, typelib and all interfaces in typelib
	HRESULT hr = _AtlModule.DllRegisterServer();
	return hr;
}


// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer(void)
{
	HRESULT hr = _AtlModule.DllUnregisterServer();
	return hr;
}
