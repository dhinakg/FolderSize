#pragma once

enum MODIFY_SERVICE
{
	MS_START,
	MS_STOP,
	MS_PAUSE,
	MS_CONTINUE,
	MS_PARAMCHANGE,
	MS_MAX
};

void ModifyService(HWND hwnd, MODIFY_SERVICE ms);

DWORD GetServiceStatus();
