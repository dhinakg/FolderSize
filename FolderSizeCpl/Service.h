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

bool ModifyService(MODIFY_SERVICE ms);

DWORD GetServiceStatus();
