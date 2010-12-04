// CLClient.cpp : Defines the entry point for the console application.
//

#include "StdAfx.h"
#include "..\Common\Pipe.h"

int _tmain(int argc, _TCHAR* argv[])
{
	// the user should pass a folder (or a list of folders?)
	if (argc != 2)
	{
		std::wcout << _T("Usage: ") << argv[0] << _T(" <path>") << std::endl;
		return 1;
	}

	FOLDERINFO2 fi;
	if (GetInfoForFolder(argv[1], fi))
	{
		LPCTSTR pszGIFF = _T("");

		switch (fi.giff)
		{
		case GIFF_SCANNING:
			pszGIFF = _T(" +");
			break;
		case GIFF_DIRTY:
			pszGIFF = _T(" ~");
			break;
		}

		std::wcout << fi.nLogicalSize << pszGIFF << _T("\n") << fi.nPhysicalSize << pszGIFF;

		return 0;
	}
	else
	{
		LPTSTR pszBuffer;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), 0, (LPTSTR)&pszBuffer, 0, NULL);
		std::wcerr << pszBuffer << std::endl;
		LocalFree(pszBuffer);
		return 1;
	}
}
