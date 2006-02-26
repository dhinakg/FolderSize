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
			wchar_t* ps = (wchar_t*)_alloca((nLen + 1) * sizeof(wchar_t));
			if (ReadFile(h, ps, nLen * sizeof(wchar_t), &dwBytesRead, NULL))
			{
				if (dwBytesRead == nLen * sizeof(wchar_t))
				{
					ps[nLen] = L'\0';
					str = ps;
					bRead = true;
				}
			}

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
	if (!WriteFile(h, &nCount, sizeof(nCount), &dwBytesWritten, NULL))
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
