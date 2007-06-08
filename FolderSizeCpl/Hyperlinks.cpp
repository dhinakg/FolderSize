// Hyperlinks.cpp
//
// Copyright 2002 Neal Stublen
// All rights reserved.
//
// http://www.awesoftware.com
//
// Modified 2005 Brio

#include "StdAfx.h"

#include "Hyperlinks.h"


#define PROP_ORIGINAL_FONT		TEXT("_Hyperlink_Original_Font_")
#define PROP_ORIGINAL_PROC		TEXT("_Hyperlink_Original_Proc_")
#define PROP_STATIC_HYPERLINK	TEXT("_Hyperlink_From_Static_")
#define PROP_UNDERLINE_FONT		TEXT("_Hyperlink_Underline_Font_")
#define PROP_TRACKING           TEXT("_Hyperlink_Tracking_")


LRESULT CALLBACK _HyperlinkParentProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WNDPROC pfnOrigProc = (WNDPROC) GetProp(hwnd, PROP_ORIGINAL_PROC);

	switch (message)
	{
	case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC) wParam;
			HWND hwndCtl = (HWND) lParam;

			BOOL fHyperlink = (NULL != GetProp(hwndCtl, PROP_STATIC_HYPERLINK));
			if (fHyperlink)
			{
				LRESULT lr = CallWindowProc(pfnOrigProc, hwnd, message, wParam, lParam);
				SetTextColor(hdc, RGB(0, 0, 192));
				return lr;
			}

			break;
		}
	case WM_DESTROY:
		{
			SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) pfnOrigProc);
			RemoveProp(hwnd, PROP_ORIGINAL_PROC);
			break;
		}
	}
	return CallWindowProc(pfnOrigProc, hwnd, message, wParam, lParam);
}

LRESULT CALLBACK _HyperlinkProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WNDPROC pfnOrigProc = (WNDPROC) GetProp(hwnd, PROP_ORIGINAL_PROC);

	switch (message)
	{
	case WM_DESTROY:
		{
			SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) pfnOrigProc);
			RemoveProp(hwnd, PROP_ORIGINAL_PROC);

			HFONT hOrigFont = (HFONT) GetProp(hwnd, PROP_ORIGINAL_FONT);
			SendMessage(hwnd, WM_SETFONT, (WPARAM) hOrigFont, 0);
			RemoveProp(hwnd, PROP_ORIGINAL_FONT);

			HFONT hFont = (HFONT) GetProp(hwnd, PROP_UNDERLINE_FONT);
			DeleteObject(hFont);
			RemoveProp(hwnd, PROP_UNDERLINE_FONT);

			RemoveProp(hwnd, PROP_STATIC_HYPERLINK);

			break;
		}
	case WM_MOUSEMOVE:
		{
			if (!GetProp(hwnd, PROP_TRACKING))
			{
				HFONT hFont = (HFONT) GetProp(hwnd, PROP_UNDERLINE_FONT);
				SendMessage(hwnd, WM_SETFONT, (WPARAM) hFont, TRUE);
				
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(tme);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hwnd;
				BOOL bTracking = TrackMouseEvent(&tme);
				SetProp(hwnd, PROP_TRACKING, (HANDLE) bTracking);
			}
			break;
		}
	case WM_MOUSELEAVE:
		{
			SetProp(hwnd, PROP_TRACKING, 0);
			HFONT hFont = (HFONT) GetProp(hwnd, PROP_ORIGINAL_FONT);
			SendMessage(hwnd, WM_SETFONT, (WPARAM) hFont, TRUE);
			return TRUE;
		}
	case WM_SETCURSOR:
		{
			SetCursor(LoadCursor(NULL, MAKEINTRESOURCE(IDC_HAND)));
			return TRUE;
		}
	}

	return CallWindowProc(pfnOrigProc, hwnd, message, wParam, lParam);
}

BOOL ConvertStaticToHyperlink(HWND hwndCtl)
{
	// Subclass the parent so we can color the controls as we desire.

	HWND hwndParent = GetParent(hwndCtl);
	if (NULL != hwndParent)
	{
		WNDPROC pfnOrigProc = (WNDPROC) GetWindowLongPtr(hwndParent, GWLP_WNDPROC);
		if (pfnOrigProc != _HyperlinkParentProc)
		{
			SetProp(hwndParent, PROP_ORIGINAL_PROC, (HANDLE) pfnOrigProc);
			SetWindowLongPtr(hwndParent, GWLP_WNDPROC, (LONG_PTR) (WNDPROC) _HyperlinkParentProc);
		}
	}

	// Make sure the control will send notifications.

	DWORD dwStyle = GetWindowLong(hwndCtl, GWL_STYLE);
	SetWindowLong(hwndCtl, GWL_STYLE, dwStyle | SS_NOTIFY);

	// Subclass the existing control.

	WNDPROC pfnOrigProc = (WNDPROC) GetWindowLongPtr(hwndCtl, GWLP_WNDPROC);
	SetProp(hwndCtl, PROP_ORIGINAL_PROC, (HANDLE) pfnOrigProc);
	SetWindowLongPtr(hwndCtl, GWLP_WNDPROC, (LONG_PTR) (WNDPROC) _HyperlinkProc);

	// Create an updated font by adding an underline.

	HFONT hOrigFont = (HFONT) SendMessage(hwndCtl, WM_GETFONT, 0, 0);
	SetProp(hwndCtl, PROP_ORIGINAL_FONT, (HANDLE) hOrigFont);

	LOGFONT lf;
	GetObject(hOrigFont, sizeof(lf), &lf);
	lf.lfUnderline = TRUE;

	HFONT hFont = CreateFontIndirect(&lf);
	SetProp(hwndCtl, PROP_UNDERLINE_FONT, (HANDLE) hFont);

	// Set a flag on the control so we know what color it should be.

	SetProp(hwndCtl, PROP_STATIC_HYPERLINK, (HANDLE) 1);

	return TRUE;
}

BOOL ConvertStaticToHyperlink(HWND hwndParent, UINT uiCtlId)
{
	return ConvertStaticToHyperlink(GetDlgItem(hwndParent, uiCtlId));
}
