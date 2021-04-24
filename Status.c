/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2007 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of source code
 *	   12-Dec-2000	several changes reflecting the extended color settings
 */

#include <windows.h>
#include <wchar.h>
#include "winvi.h"
#include "paint.h"
#include "status.h"

extern BOOL SizeGrip;

COLORREF StatusErrorBackground = RGB(0,0,0);

struct tagSTATUSSTYLE StatusStyle[5] = {
	{RGB(0,0,0),	   TRUE},	/*normal:	black text in frame		*/
	{RGB(128,128,128), TRUE},	/*cmd done:	gray  text in frame		*/
	{RGB(255,0,0),	   TRUE},	/*error:	red   text in frame		*/
	{RGB(0,128,0),	   TRUE},	/*i/o busy:	green text in frame		*/
	{RGB(0,0,0),	  FALSE},	/*tooltip:	black text without frame*/
};
HFONT StatusFont   = 0;
INT   StatusHeight = 0;

static INT StatusWidth(PWSTR Str) {
	HDC   hDC = GetDC(NULL);
	DWORD w;
	HFONT OldFont;
	SIZE  Size;

	if (StatusFont) OldFont = SelectObject(hDC, StatusFont);
	StatusHeight = GetTextExtentPointW(hDC, Str, wcslen(Str), &Size)
				   ? Size.cy + 9 : 25;
	w = Size.cx;
	if (SizeGrip && WinVersion >= MAKEWORD(95,3)) StatusHeight -= 2;
	if (StatusFont && OldFont) SelectObject(hDC, OldFont);
	ReleaseDC(NULL, hDC);
	return (LOWORD(w) + 5);
}

void RecalcStatusRow(void) {
	/*called initially and if main window (and status row) changes width*/
	INT Right = ClientRect.right;

	if (WinVersion < MAKEWORD(95,3) || !SizeGrip) Right -= XFIELD_OFFSET;
	else Right -= XFIELD_OFFSET - 2;
	if (Right > 0) {
		INT			 i, w = 0;
		WCHAR		 b[12];
		extern WCHAR StatusLineFormat[8], StatusColFormat[8];

		if (SizeGrip) Right -= 15;
		StatusFields[0].Width = StatusWidth(L"nnnnnnnnnnnnnnnnnnnn");
		StatusFields[1].Width = StatusWidth(L"Aguantado");
		StatusFields[2].Width = StatusWidth(CapsLock);
		StatusFields[3].Width = StatusWidth(NumLock);
		StatusFields[4].Width = StatusWidth(L"--100%--");
		_snwprintf(b, WSIZE(b), StatusLineFormat, 0L);
		StatusFields[5].Width = StatusWidth(b);
		_snwprintf(b, WSIZE(b), StatusColFormat, 0L);
		StatusFields[6].Width = StatusWidth(b);
		StatusFields[1].Centered = StatusFields[4].Centered = TRUE;
		for (i=0; StatusFields[i].Width; ++i)
			w += StatusFields[i].Width + 6;
		if (w > ClientRect.right) {
			w -= StatusFields[2].Width + 6;
			w -= StatusFields[3].Width + 6;
			StatusFields[2].Width = StatusFields[3].Width = 0;
		}
		if (w > ClientRect.right) {
			w -= StatusFields[4].Width + 6;
			StatusFields[4].Width = 0;
		}
		if (w > ClientRect.right) {
			w -= StatusFields[5].Width + 6;
			w -= StatusFields[6].Width + 6;
			StatusFields[5].Width = StatusFields[6].Width = 0;
		}
		if (w > ClientRect.right) {
			/*w -= StatusFields[1].Width + 6;*/
			StatusFields[1].Width = 0;
		}
		w = Right;
		for (i=6; i>=0; --i) {
			INT w2;

			w2 = StatusFields[i].Width;
			StatusFields[i].x = w - w2;
			if (w2) {
				w -= w2 + 4;
				if (i==4 || i==2) w -= 5;
			}
		}
		StatusFields[0].Width += StatusFields[0].x;
		StatusFields[0].x      = 0;
		if (WinVersion < MAKEWORD(95,3) || !SizeGrip) {
			StatusFields[0].Width -= XFIELD_OFFSET;
			StatusFields[0].x	  += XFIELD_OFFSET;
		}
		StatusTextHeight = StatusHeight -
						   (WinVersion>=MAKEWORD(95,3) && SizeGrip ? 6 : 8);
		if (StatusFields[4].Text) return;
		StatusFields[2].Text = GetKeyState(VK_CAPITAL)&1 ? CapsLock : (PWSTR)0;
		StatusFields[3].Text = GetKeyState(VK_NUMLOCK)&1 ? NumLock	: (PWSTR)0;
		StatusFields[4].Text = L"--100%--";
		StatusFields[5].Text = L"00001";
		StatusFields[6].Text = L"001";
	}
}

void NewCapsString(VOID) {

	LOADSTRINGW(hInst, 903, CapsLock, WSIZE(CapsLock));
	RecalcStatusRow();
	AdjustWindowParts(ClientRect.bottom, ClientRect.bottom);
}

void PaintStatus(HDC hDC, PRECT pRect, PRECT pWholeWindow) {
	extern BOOL  SizeGrip;
	static POINT Line[3][2];
	static POINT InnerFrame[2][3];
	INT    i;
	HPEN   OldPen;
	HFONT  OldFont;
	RECT   Background;

	if (!StatusHeight) return;
	Background.left   = pRect->left;
	Background.top	  = pWholeWindow->bottom - StatusHeight + 2;
	Background.right  = pRect->right;
	Background.bottom = pWholeWindow->bottom;
	FillRect(hDC, &Background, FaceBrush);
	if (StatusFont) OldFont = SelectObject(hDC, StatusFont);
	SetBkMode(hDC, TRANSPARENT);
	Line[0][0].y = Line[0][1].y = pWholeWindow->bottom - StatusHeight;
	Line[1][0].y = Line[1][1].y = pWholeWindow->bottom - StatusHeight + 1;
	Line[2][0].y = Line[2][1].y = pWholeWindow->bottom - 1;
	Line[0][1].x = Line[1][1].x = Line[2][1].x = pRect->right;
	OldPen = SelectObject(hDC, ShadowPen);
	Polyline(hDC, Line[0], 2);
	SelectObject(hDC, HilitePen);
	Polyline(hDC, Line[1], 2);
#	if defined(LOWERSHADOW)
		if (WinVersion <= 0x351) {
			SelectObject(hDC, ShadowPen);
			Polyline(hDC, Line[2], 2);
		}
#	endif
	if (SizeGrip) {
		static POINT Offsets[6] = {{-2,-1}, {-1,-2}, {-1,-3}, {-4,0},
								   {-4,-1},  {0,-5}};
		POINT		 HiliteShadow[6];

		for (i=0; i<6; ++i) {
			HiliteShadow[i].x = Offsets[i].x + ClientRect.right;
			HiliteShadow[i].y = Offsets[i].y + ClientRect.bottom;
			if (WinVersion < MAKEWORD(95,3) || !SizeGrip) {
				HiliteShadow[i].x -= 2;
				HiliteShadow[i].y -= 2;
			}
		}
		for (i=0; i<3; ++i) {
			INT j;

			SelectObject(hDC, ShadowPen);
			Polyline(hDC, HiliteShadow, 4);
			SelectObject(hDC, HilitePen);
			Polyline(hDC, HiliteShadow+4, 2);
			for (j=0; j<6; ++j) {
				if ((j&3)==0 || (j&3)==3) HiliteShadow[j].x -= 5;
				else					  HiliteShadow[j].y -= 5;
			}
		}
	}
	for (i=6; i>=0; --i) {
		PWSTR Text;

		if (StatusFields[i].x >= pRect->right) continue;
		if (StatusFields[i].x + StatusFields[i].Width < pRect->left) continue;
		StatusFields[i].TextExtent = 0;
		if (!StatusFields[i].Width) continue;
		if (StatusStyle[StatusFields[i].Style].LoweredFrame) {
			InnerFrame[0][0].x = InnerFrame[0][1].x = InnerFrame[1][0].x
				= StatusFields[i].x;
			InnerFrame[0][2].x = InnerFrame[1][1].x = InnerFrame[1][2].x
				= StatusFields[i].x + StatusFields[i].Width;
			InnerFrame[0][0].y = InnerFrame[1][0].y = InnerFrame[1][1].y
				= ClientRect.bottom -
				  (WinVersion>=MAKEWORD(95,3) && SizeGrip ? 1 : 3);
			InnerFrame[0][1].y = InnerFrame[0][2].y = InnerFrame[1][2].y
				= ClientRect.bottom - StatusHeight + 4;
			SelectObject(hDC, HilitePen);
			Polyline(hDC, InnerFrame[1], 3);
			++InnerFrame[0][2].x;
			SelectObject(hDC, ShadowPen);
			Polyline(hDC, InnerFrame[0], 3);
		}
		if ((Text = StatusFields[i].Text) != NULL && *Text) {
			INT  x, y, Len;
			SIZE Size;

			Len = wcslen(Text);
			x = StatusFields[i].x + 3;
			ExcludeClipRect(hDC,
							StatusFields[i].x + StatusFields[i].Width,
							ClientRect.bottom - StatusHeight + 1,
							ClientRect.right,
							ClientRect.bottom);
			if (StatusFields[i].Centered) {
				INT c = (StatusFields[i].Width - StatusWidth(Text)) >> 1;

				if (c > 0) x += c;
			}
			y = ClientRect.bottom - StatusHeight + 5;
			SetTextColor(hDC, StatusStyle[StatusFields[i].Style].Color);
			TextOutW(hDC, x, y, Text, Len);
			StatusFields[i].TextExtent =
				GetTextExtentPointW(hDC, Text, Len, &Size) ? Size.cx : 0;
		}
	}
	if (StatusFont && OldFont) SelectObject(hDC, OldFont);
	SelectObject(hDC, OldPen);
	SetTextColor(hDC, RGB(0,0,0));
}

BOOL SuppressStatusBeep = FALSE;

void NewStatus(INT Field, PWSTR Text, INT Style) {
	BOOL PaintFrame = StatusStyle[StatusFields[Field].Style].LoweredFrame
				   != StatusStyle[Style].LoweredFrame;

	StatusFields[Field].Text		 = Text;
	StatusFields[Field].Style		 = Style;
	if (!hwndMain) return;
	if (StatusFields[Field].Width) {
		RECT r;

		r.left	 = StatusFields[Field].x + 1;
		r.top	 = ClientRect.bottom - StatusHeight + 5;
		r.right  = r.left + StatusFields[Field].Width - 1;
		r.bottom = r.top + StatusTextHeight;
		if (PaintFrame) {
			--r.left;
			--r.top;
			++r.right;
			++r.bottom;
		}
		UpdateWindow(hwndMain);
		InvalidateRect(hwndMain, &r, TRUE);
	}
	if (Field == 0) {
		if (Style==NS_ERROR && !SuppressStatusBeep)
			MessageBeep(MB_ICONEXCLAMATION);
		RemoveStatusTip();
		if (GetSystemMetrics(SM_MOUSEPRESENT)) {
			POINT pt;

			GetCursorPos(&pt);
			ScreenToClient(hwndMain, &pt);
			PostMessage(hwndMain, WM_MOUSEMOVE, 0, MAKELPARAM(pt.x, pt.y));
		}
	}
	UpdateWindow(hwndMain);
}

void SetStatusCommand(PWSTR pCmd, BOOL InProgress) {
	static WCHAR CmdBuf[40];
	static BOOL	 WasBlack;
	BOOL		 Paint;

	Paint = pCmd && wcscmp(pCmd, CmdBuf);
	if (Paint) wcscpy(CmdBuf, pCmd);
	else Paint = WasBlack != InProgress;
	if (Paint) NewStatus(1, CmdBuf, InProgress ? NS_NORMAL : NS_GRAY);
	WasBlack = InProgress;
}

HMENU hStatusPopupMenu;

VOID StatusContextMenu(INT x, INT y) {
	CHAR Buf[30];

	if (StatusFields[0].Text && *StatusFields[0].Text &&
			(hStatusPopupMenu = CreatePopupMenu()) != NULL) {
		if (LOADSTRING(hInst, 910, Buf, sizeof(Buf))) {
			AppendMenu(hStatusPopupMenu, MF_STRING, 910, Buf);
			if ((BYTE)*StatusFields[0].Text==0253 ||
				(BYTE)StatusFields[0].Text[wcslen(StatusFields[0].Text)-1] ==
																		0273) {
					/*status line contains filename...*/
					if (LOADSTRING(hInst, 911, Buf, sizeof(Buf)))
						AppendMenu(hStatusPopupMenu, MF_STRING, 911, Buf);
			}
			TrackPopupMenu(hStatusPopupMenu,
						   TPM_LEFTALIGN | TPM_RIGHTBUTTON,
						   x, y, 0, hwndMain, NULL);
		}
		DestroyMenu(hStatusPopupMenu);
		hStatusPopupMenu = NULL;
	}
}
