#pragma once

class Path : public std::wstring
{
public:
	Path() {}
	Path(const Path& _Right) : std::wstring(_Right) {}
	Path(const wchar_t *_Ptr) : std::wstring(_Ptr) {}
	Path(const wchar_t *_Ptr, size_type _Count) : std::wstring(_Ptr, _Count) {}

	Path& operator=(const Path& _Right);
	Path& operator=(const wchar_t *_Ptr);
	Path operator+(const Path& _Right) const;

	Path GetLongAPIRepresentation() const;
	Path GetVolume() const;
	Path GetParent() const;
	bool IsNetwork() const;
};