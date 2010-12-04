#include "StdAfx.h"
#include "Pipe.h"

bool ReadRequest(HANDLE h, PIPE_CLIENT_REQUEST& pcr)
{
	unsigned short r;
	DWORD dwBytesRead;
	if (ReadFile(h, &r, sizeof(r), &dwBytesRead, NULL) && dwBytesRead == sizeof(r))
	{
		pcr = (PIPE_CLIENT_REQUEST)r;
		return true;
	}
	return false;
}

bool WriteRequest(HANDLE h, PIPE_CLIENT_REQUEST pcr)
{
	unsigned short r = (unsigned short)pcr;
	DWORD dwBytesWritten;
	return WriteFile(h, &r, sizeof(r), &dwBytesWritten, NULL) && dwBytesWritten == sizeof(r);
}

bool ReadString(HANDLE h, std::wstring& str)
{
	bool bRead = false;
	DWORD dwBytesRead;
	unsigned short nLen;
	if (ReadFile(h, &nLen, sizeof(nLen), &dwBytesRead, NULL))
	{
		if (dwBytesRead == sizeof(nLen))
		{
			wchar_t* ps = (wchar_t*)_malloca(nLen * sizeof(wchar_t));
			if (ReadFile(h, ps, nLen * sizeof(wchar_t), &dwBytesRead, NULL))
			{
				if (dwBytesRead == nLen * sizeof(wchar_t))
				{
					str.assign(ps, nLen);
					bRead = true;
				}
			}
			_freea(ps);
		}
	}
	return bRead;
}

bool WriteString(HANDLE h, const std::wstring& str)
{
	if (str.length() > USHRT_MAX)
		return false;

	unsigned short nLen = (unsigned short)str.length();
	DWORD dwBytesWritten;
	if (WriteFile(h, &nLen, sizeof(nLen), &dwBytesWritten, NULL) && dwBytesWritten == sizeof(nLen))
	{
		if (WriteFile(h, str.data(), nLen*sizeof(wchar_t), &dwBytesWritten, NULL))
		{
			return true;
		}
	}
	return false;
}

bool WriteGetFolderSizeRequest(HANDLE h, const std::wstring& strFolder)
{
	if (strFolder.size() > USHRT_MAX)
		return false;
	size_t buflen = sizeof(short) * 2 + strFolder.size() * sizeof(wchar_t);
	BYTE* buffer = (BYTE*)_malloca(buflen);
	BYTE* p = buffer;
	unsigned short data = PCR_GETFOLDERSIZE;
	memcpy(p, &data, sizeof(short));
	p += sizeof(short);
	data = (unsigned short)strFolder.size();
	memcpy(p, &data, sizeof(short));
	p += sizeof(short);
	memcpy(p, strFolder.data(), data * sizeof(wchar_t));
	p += data * sizeof(wchar_t);
	assert (p - buffer == buflen);
	DWORD dwBytesWritten;
	BOOL bWrote = WriteFile(h, buffer, (DWORD)buflen, &dwBytesWritten, NULL);
	_freea(buffer);
	return bWrote && dwBytesWritten == buflen;
}

bool ReadStringList(HANDLE h, Strings& strs)
{
	unsigned short nCount;
	DWORD dwBytesRead;
	if (!ReadFile(h, &nCount, sizeof(nCount), &dwBytesRead, NULL) || dwBytesRead != sizeof(nCount))
		return false;
	for (unsigned short i=0; i<nCount; i++)
	{
		std::wstring strFile;
		if (!ReadString(h, strFile))
			return false;
		strs.insert(strFile);
	}
	return true;
}

bool WriteStringList(HANDLE h, const Strings& strs)
{
	unsigned short nCount = (unsigned short)strs.size();
	DWORD dwBytesWritten;
	if (!WriteFile(h, &nCount, sizeof(nCount), &dwBytesWritten, NULL) || dwBytesWritten != sizeof(nCount))
		return false;
	for (Strings::const_iterator i=strs.begin(); i!=strs.end(); i++)
	{
		if (!WriteString(h, *i))
		{
			return false;
		}
	}
	return true;
}

bool WriteGetUpdatedFoldersRequest(HANDLE h, const Strings& strsFolders)
{
	unsigned short data = PCR_GETUPDATEDFOLDERS;
	DWORD dwBytesWritten;
	if (!WriteFile(h, &data, sizeof(data), &dwBytesWritten, NULL) || dwBytesWritten != sizeof(data))
		return false;
	return WriteStringList(h, strsFolders);
}

bool ReadGetFolderSize(HANDLE h, FOLDERINFO2& Size)
{
	DWORD dwBytesRead;
	return ReadFile(h, &Size, sizeof(Size), &dwBytesRead, NULL) && dwBytesRead == sizeof(Size);
}

bool WriteGetFolderSize(HANDLE h, const FOLDERINFO2& Size)
{
	DWORD dwBytesWritten;
	return WriteFile(h, &Size, sizeof(Size), &dwBytesWritten, NULL) && dwBytesWritten == sizeof(Size);
}

bool GetInfoForFolder(LPCWSTR pszFile, FOLDERINFO2& nSize)
{
	bool bRet = false;

	// try twice to connect to the pipe
	HANDLE hPipe = CreateFile(TEXT("\\\\.\\pipe\\") PIPE_NAME, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_PIPE_BUSY)
		{
			SetLastError(0);
			if (WaitNamedPipe(TEXT("\\\\.\\pipe\\") PIPE_NAME, 1000))
			{
				hPipe = CreateFile(TEXT("\\\\.\\pipe\\") PIPE_NAME, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			}
		}
	}

	if (hPipe != INVALID_HANDLE_VALUE)
	{
		if (WriteGetFolderSizeRequest(hPipe, pszFile))
		{
			FOLDERINFO2 Size;
			if (ReadGetFolderSize(hPipe, nSize))
			{
				bRet = true;
			}
		}

		CloseHandle(hPipe);
	}

	return bRet;
}
