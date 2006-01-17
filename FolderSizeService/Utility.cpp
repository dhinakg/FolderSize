#include "StdAfx.h"
#include "Utility.h"

// this is what the PathIsNetworkPath API should really do...
bool MyPathIsNetworkPath(LPCTSTR pszPath)
{
	bool bNetPath = false;

	if (PathIsNetworkPath(pszPath))
	{
		bNetPath = true;
	}
	else
	{
		if (!PathIsUNC(pszPath))
		{
			TCHAR szDrive[_MAX_DRIVE + 1];
			lstrcpyn(szDrive, pszPath, _MAX_DRIVE + 1);

			if (GetDriveType(szDrive) == DRIVE_REMOTE)
			{
				bNetPath = true;
			}
		}
	}

	return bNetPath;
}
