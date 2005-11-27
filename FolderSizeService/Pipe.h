#pragma once

class CacheManager;

class Pipe
{
public:
	Pipe(CacheManager* pCacheManager);
	~Pipe();

private:
	static DWORD WINAPI PipeThread(LPVOID lpParameter);
	DWORD PipeThread();

	HANDLE m_hThread;
	HANDLE m_hQuitEvent;
	CacheManager* m_pCacheManager;
};

