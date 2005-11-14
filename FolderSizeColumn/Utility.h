#pragma once


__inline ULONGLONG MakeULongLong(DWORD dwHigh, DWORD dwLow)
{
	return ((ULONGLONG)dwHigh << 32) | dwLow;
}
