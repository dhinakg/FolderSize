#pragma once

// update columns once every two seconds
// i was using 2.5 seconds... seemed just a bit long
#define SHELL_UPDATE_INTERVAL 2000

class ShellUpdate
{
public:
	ShellUpdate();
	~ShellUpdate();

protected:

	static DWORD WINAPI ThreadProc(LPVOID lpParameter);
	void Update();

	HANDLE m_hThread;
	HANDLE m_hQuitEvent;
};