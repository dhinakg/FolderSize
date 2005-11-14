#pragma once

// update columns once every two and a half seconds
#define SHELL_UPDATE_INTERVAL 2500

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