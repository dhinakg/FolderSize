#include "StdAfx.h"
#include "Path.h"

Path& Path::operator=(const Path& _Right)
{
	assign(_Right);
	return *this;
}

Path& Path::operator=(const wchar_t *_Ptr)
{
	assign(_Ptr);
	return *this;
}

Path Path::operator+(const Path& _Right) const
{
	if (empty())
		return _Right;
	if (_Right.empty())
		return *this;

	Path pathAppend;
	pathAppend.reserve(length() + 1 + _Right.length());
	pathAppend = *this;
	if (pathAppend[pathAppend.length() - 1] != _T('\\'))
		pathAppend.append(1, _T('\\'));
	pathAppend.append(_Right);
	return pathAppend;
}

Path Path::GetLongAPIRepresentation() const
{
	if (PathIsUNC(c_str()))
	{
		Path pathUNC = _T("\\\\?\\UNC\\");
		pathUNC.append(begin() + 2, end());
		return pathUNC;
	}
	return Path(_T("\\\\?")) + *this;
}

Path Path::GetParent() const
{
	wchar_t* buffer = (wchar_t*)_alloca((length() + 1) * sizeof(wchar_t));
	lstrcpy(buffer, c_str());
	PathRemoveFileSpec(buffer);
	return Path(buffer);
}

bool Path::IsNetwork() const
{
	LPCTSTR psz = c_str();

	if (PathIsNetworkPath(psz))
		return true;

	if (!PathIsUNC(psz))
	{
		TCHAR szDrive[_MAX_DRIVE + 1];
		lstrcpyn(szDrive, psz, _MAX_DRIVE + 1);

		if (::GetDriveType(szDrive) == DRIVE_REMOTE)
			return true;
	}

	return false;
}

Path Path::GetVolume() const
{
	LPCTSTR psz = c_str();

	if (PathIsUNC(psz))
	{
		// skip ahead 3 components
		LPCTSTR pszEnd = psz;
		for (int i=0; i<3; i++)
		{
			pszEnd = PathFindNextComponent(pszEnd);
			if (pszEnd == NULL)
				return Path();
		}
		// and only copy up to the third component
		int len = (int)(pszEnd - psz) + 1;
		wchar_t* pszVolume = (wchar_t*)_alloca(len * sizeof(wchar_t));
		lstrcpyn(pszVolume, psz, len);
		PathRemoveBackslash(pszVolume);
		return Path(pszVolume);
	}
	else
	{
		// a drive root like C:\ has 3 chars
		if (length() < 3)
			return Path();
		return Path(psz, 3);
	}
}

UINT Path::GetDriveType() const
{
	LPCTSTR psz = c_str();

	if (PathIsNetworkPath(psz) || PathIsUNC(psz))
		return DRIVE_REMOTE;

	TCHAR szDrive[_MAX_DRIVE + 1];
	lstrcpyn(szDrive, psz, _MAX_DRIVE + 1);
	return ::GetDriveType(szDrive);
}
