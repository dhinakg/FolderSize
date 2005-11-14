#include "StdAfx.h"
#include "ShellUpdate.h"


class CFolderSizeModule : public CAtlDllModuleT<CFolderSizeModule>
{
};

CFolderSizeModule _AtlModule;

static ShellUpdate* g_pShellUpdate = NULL;

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
		if (g_pShellUpdate != NULL)
		{
			delete g_pShellUpdate;
			g_pShellUpdate = NULL;
		}
	}
	return hr;
}


// Returns a class factory to create an object of the requested type
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	HRESULT hr = _AtlModule.DllGetClassObject(rclsid, riid, ppv);
	if (hr == S_OK)
	{
		if (g_pShellUpdate == NULL)
		{
			g_pShellUpdate = new ShellUpdate;
		}
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
