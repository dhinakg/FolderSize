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

Path Path::GetName() const
{
	return Path(PathFindFileName(c_str()));
}

bool Path::IsNetwork() const
{
	return PathIsNetworkPath(c_str()) ? true : false;
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
		size_t len = pszEnd - psz;
		return Path(psz, len);
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

	if (PathIsUNC(psz))
		return DRIVE_REMOTE;

	TCHAR szDrive[_MAX_DRIVE + 1];
	lstrcpyn(szDrive, psz, _MAX_DRIVE + 1);
	return ::GetDriveType(szDrive);
}

PathSegmentIterator Path::GetPathSegmentIterator() const
{
	// get a pointer to the first segment of the path after the volume
	LPCTSTR psz = c_str();
	if (PathIsUNC(psz))
	{
		// skip ahead 3 components
		for (int i=0; i<3; i++)
		{
			psz = PathFindNextComponent(psz);
			if (psz == NULL)
				break;
		}
	}
	else
	{
		// a drive root like C:\ has 3 chars
		if (length() < 3)
			psz = NULL;
		else
			psz += 3;
	}
	return PathSegmentIterator(psz);
}


PathSegmentIterator::PathSegmentIterator(const wchar_t* p) : m_p(p)
{
}

bool PathSegmentIterator::AtEnd() const
{
	return m_p == NULL || *m_p == _T('\0');
}

Path PathSegmentIterator::GetNextPathSegment()
{
	if (PathIsFileSpec(m_p))
	{
		Path p(m_p);
		m_p = NULL;
		return p;
	}
	const wchar_t* pEnd = PathFindNextComponent(m_p);
	assert (pEnd != NULL);
	if (pEnd == NULL)
	{
		Path p(m_p);
		m_p = NULL;
		return p;
	}
	else
	{
		Path p(m_p, pEnd - m_p - 1);
		m_p = pEnd;
		return p;
	}
}
