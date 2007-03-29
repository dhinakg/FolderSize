#pragma once

#include "RegDwordValue.h"
#include "ShellUpdate.h"

class CFolderSizeModule : public CAtlDllModuleT<CFolderSizeModule>
{
public:
	CFolderSizeModule();
	void InitWatchers();
	void DestroyWatchers();

public:
	ShellUpdate* m_pShellUpdate;
	RegDwordValue* m_pRegDisplayFormat;
};


extern CFolderSizeModule _AtlModule;

