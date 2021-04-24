/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2012 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      3-Dec-2000	first publication of source code
 *     23-Jun-2002	new code for disabling printer button
 *     22-Jul-2002	use of private myassert.h
 *     24-Jul-2002	connect "new" and "open" toolbar buttons to file menu items
 *     12-Sep-2002	+EBCDIC quick hack (Christian Franke)
 *     14-Nov-2002	fixed positioning bug in first line if empty and
 *              	   lf terminated when switching from hex to text mode
 *     12-Apr-2005	crash fixed if ToggleHexMode() is called while initializing
 *     27-Jun-2007	support for UTF-8 and UTF-16 buttons
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *     28-Dec-2009	suppressing toolbar painting when exiting
 *     24-Dec-2011	fixed a positioning bug for UTF-8 text files with 
 *              	   incorrectly encoded characters
 */

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#include "myassert.h"
#include "resource.h"
#include "winvi.h"
#include "page.h"
#include "paint.h"
#include "status.h"
#include "toolbar.h"

#if defined(WIN32)
#	define ROW_WIDTH	   410
#	define ROW_HEIGHT	   22
#	define NEXT_ROW		   ROW_HEIGHT
#	define PUSHED_LOCKED   ROW_WIDTH
#	define PUSHED_DISABLED (ROW_WIDTH+24)
#else
#	define ROW_WIDTH	   363
#	define ROW_HEIGHT	   22
#	define NEXT_ROW		   (ROW_HEIGHT-1)
#	define PUSHED_LOCKED   (ROW_WIDTH-1)
#	define PUSHED_DISABLED (ROW_WIDTH-1+24)
#	if !defined(WS_EX_TOOLWINDOW)
#		define  WS_EX_TOOLWINDOW 0x00000080L
#	endif
#endif

struct {
	INT   Id, xPos, Width;
	BOOL  Pushed, Locked, Disabled;
	PSTR  Tip;
	PWSTR StatusTip;
} ToolButtons[] = {
	{ID_NEW,	  0,24,	 FALSE,FALSE,FALSE, NULL,NULL},
	{ID_OPEN,	 24,24,	 FALSE,FALSE,FALSE, NULL,NULL},
	{ID_SAVE,	 48,24,	 FALSE,FALSE,TRUE,  NULL,NULL},
	{ID_PRINT,	 72,24,	 FALSE,FALSE,FALSE, NULL,NULL},
	{ID_CUT,	106,24,	 FALSE,FALSE,FALSE, NULL,NULL},
	{ID_COPY,	130,24,	 FALSE,FALSE,FALSE, NULL,NULL},
	{ID_PASTE,	154,24,	 FALSE,FALSE,FALSE, NULL,NULL},
	{ID_UNDO,	188,24,	 FALSE,FALSE,TRUE,  NULL,NULL},
	{ID_SEARCH,	222,24,	 FALSE,FALSE,FALSE, NULL,NULL},
	{ID_SRCHAGN,246,24,	 FALSE,FALSE,TRUE,  NULL,NULL},
	{ID_ANSI,	280,24,  TRUE, TRUE, FALSE, NULL,NULL},
	{ID_OEM,	304,24,  FALSE,FALSE,FALSE, NULL,NULL},
	{ID_UTF8,	328,24,  FALSE,FALSE,FALSE, NULL,NULL},
	{ID_UTF16,	352,24,  FALSE,FALSE,FALSE, NULL,NULL},
	{ID_HEX,	386,24,	 FALSE,FALSE,FALSE, NULL,NULL},
	{0,0,0,0,0,0,0,0}
};

void SetToolbarColor(LPRGBQUAD lpRgbQuad, INT SysColor)
{	COLORREF   ColorRef = GetSysColor(SysColor);

	lpRgbQuad->rgbRed   = GetRValue(ColorRef);
	lpRgbQuad->rgbGreen = GetGValue(ColorRef);
	lpRgbQuad->rgbBlue  = GetBValue(ColorRef);
}

HBITMAP Buttons = NULL; // STEPHANIE

void PrepareBitmap(HDC hdcScreen)
{
	return; // STEPHANIE
	HRSRC		 BitmapResource;
	HGLOBAL		 LoadedBitmap = NULL;
	LPBITMAPINFO LockedBitmap, NewBitmap = NULL;

	yButtonOffset = YBUTTON_OFFSET;
	nToolbarHeight = TOOLS_HEIGHT;
	if (WinVersion >= MAKEWORD(95,3)) {
		++yButtonOffset;
		++nToolbarHeight;
	}
	if (Buttons) {
		DeleteObject(Buttons);
		Buttons = NULL;
	}
#	if !defined(WIN32)		/*prevent never-used warning in BCC*/
		LockedBitmap = NULL;
#	endif

	/*try to create a bitmap with 3D system colors...*/
	do {	/*for break only*/
		BitmapResource = FindResource(hInst, MAKEINTRESOURCE(100), RT_BITMAP);
		if (!BitmapResource) break;
		LoadedBitmap = LoadResource(hInst, BitmapResource);
		if (!LoadedBitmap) break;
		LockedBitmap = (LPBITMAPINFO)LockResource(LoadedBitmap);
		if (!LockedBitmap) break;
		{	INT Size = (INT)SizeofResource(hInst, BitmapResource);

			if (Size < 1000 || Size > 100000) break;
			NewBitmap = _fcalloc(1, Size);
			if (!NewBitmap) break;
			hmemcpy(NewBitmap, LockedBitmap, Size);
		}

#		if defined(WIN32)
			/*bmpColors[] before redefinition:
			 *  6=RGB(000,128,128)	dark cyan
			 *  7=RGB(192,192,192)	light gray,
			 *  8=RGB(128,128,128)	dark gray,
			 * 14=RGB(000,255,255)	light cyan
			 */
			NewBitmap->bmiColors[6] = LockedBitmap->bmiColors[7];
			SetToolbarColor(&NewBitmap->bmiColors[7],  COLOR_BTNFACE);
			SetToolbarColor(&NewBitmap->bmiColors[8],  COLOR_BTNSHADOW);
			SetToolbarColor(&NewBitmap->bmiColors[14], COLOR_BTNHIGHLIGHT);
#		else
			/*bmpColors[] before redefinition:
			 *  0=RGB(000,000,000)	black,
			 *  4=RGB(000,000,128)	dark blue,
			 *  7=RGB(192,192,192)	light gray,
			 *  8=RGB(128,128,128)	dark gray,
			 * 15=RGB(255,255,255)	white
			 */
			SetToolbarColor(&NewBitmap->bmiColors[0],  COLOR_WINDOWFRAME);
			SetToolbarColor(&NewBitmap->bmiColors[4],  COLOR_BTNTEXT);
			SetToolbarColor(&NewBitmap->bmiColors[7],  COLOR_BTNFACE);
			SetToolbarColor(&NewBitmap->bmiColors[8],  COLOR_BTNSHADOW);
			SetToolbarColor(&NewBitmap->bmiColors[15], COLOR_BTNHIGHLIGHT);
#		endif

		Buttons = CreateDIBitmap(hdcScreen, &NewBitmap->bmiHeader, CBM_INIT,
								 &NewBitmap->bmiColors[16], NewBitmap,
								 DIB_RGB_COLORS);

	} while (FALSE);
	if (NewBitmap) _ffree(NewBitmap);
#	if !defined(WIN32)
		if (LockedBitmap) (VOID)UnlockResource(LoadedBitmap);
#	endif
	if (LoadedBitmap) FreeResource(LoadedBitmap);
	if (Buttons) return;

	/*system color bitmap creation failed, use normal bitmap resource*/
	Buttons = LoadBitmap(hInst, MAKEINTRESOURCE(100));
}

void RepaintButton(INT Which)
{
	if (hwndMain) {
		RECT r;

		r.left   = XBUTTON_OFFSET + ToolButtons[Which].xPos;
		r.top    = yButtonOffset;
		r.right  = r.left + ToolButtons[Which].Width;
		r.bottom = r.top + ROW_HEIGHT;
		InvalidateRect(hwndMain, &r, FALSE);
		UpdateWindow(hwndMain);
	}
}

static CHAR	 TipTexts[256];
static WCHAR StatusTips1[256], StatusTips2[256], StatusTips3[256];

void LoadTips(void)
{	PSTR  p;
	PWSTR pw;
	INT	  i;

	LOADSTRING (hInst, 905, TipTexts,	 WSIZE(TipTexts));
	LOADSTRINGW(hInst, 906, StatusTips1, WSIZE(StatusTips1));
	LOADSTRINGW(hInst, 907, StatusTips2, WSIZE(StatusTips2));
	LOADSTRINGW(hInst, 908, StatusTips3, WSIZE(StatusTips3));
	p = TipTexts;
	for (i=0; ToolButtons[i].Id; ++i)
		ToolButtons[i].Tip		 = SeparateStringA(&p);
	pw = StatusTips1;
	for (i=0; ToolButtons[i].Id != ID_UNDO; ++i)
		ToolButtons[i].StatusTip = SeparateStringW(&pw);
	pw = StatusTips2;
	for (; ToolButtons[i].Id != ID_ANSI; ++i)
		ToolButtons[i].StatusTip = SeparateStringW(&pw);
	pw = StatusTips3;
	for (; ToolButtons[i].Id; ++i)
		ToolButtons[i].StatusTip = SeparateStringW(&pw);
}

void PaintTools(HDC hDC, PRECT pRect)
{
	return; // STEPHANIE
	INT			 i;
	HDC			 hCompDC;
	static POINT Line1[2], Line2[2], Line3[2];
	HPEN		 OldPen;
	RECT		 Background;

	if (!*TipTexts) LoadTips();
	Background.left   = pRect->left;
	Background.top	  = 1;
	Background.right  = pRect->right;
	Background.bottom = nToolbarHeight - 2;
	FillRect(hDC, &Background, FaceBrush);
	Line1[1].x = Line2[1].x = Line3[1].x = pRect->right;
	Line2[0].y = Line2[1].y = nToolbarHeight - 2;
	Line3[0].y = Line3[1].y = nToolbarHeight - 1;
	OldPen = SelectObject(hDC, ShadowPen);
	if (WinVersion >= MAKEWORD(95,3)) {
		Line1[0].y = Line1[1].y = 0;
		Polyline(hDC, Line1, 2);
		Line1[0].y = ++Line1[1].y;
	}
	Polyline(hDC, Line2, 2);
	SelectObject(hDC, HilitePen);
	Polyline(hDC, Line1, 2);
	SelectObject(hDC, BlackPen);
	Polyline(hDC, Line3, 2);
	SelectObject(hDC, OldPen);
	hCompDC = CreateCompatibleDC(hDC);
	if (hCompDC) {
		HBITMAP OldBitmap = SelectObject(hCompDC, Buttons);

		BitBlt(hDC, XBUTTON_OFFSET,  yButtonOffset, ROW_WIDTH, ROW_HEIGHT,
			   hCompDC, 0, Disabled>1 ? 2*NEXT_ROW : 0, SRCCOPY);
		SelectObject(hCompDC, OldBitmap);
		DeleteDC(hCompDC);
	}
	for (i=0; ToolButtons[i].Id; ++i)
		if (ToolButtons[i].Pushed || (ToolButtons[i].Disabled && Disabled<=1)) {
			HDC hDC = GetDC(hwndMain);

			if (hDC) {
				HDC hCompDC = CreateCompatibleDC(hDC);

				if (hCompDC) {
					INT		SrcX = ToolButtons[i].xPos, SrcY;
					HBITMAP	OldBitmap = SelectObject(hCompDC, Buttons);

					if (Disabled>1 || ToolButtons[i].Disabled) {
						SrcY = 2 * NEXT_ROW;
						if (ToolButtons[i].Pushed) {
							switch (i) {
								case IDB_ANSI:
								case IDB_OEM:
								case IDB_UTF8:
								case IDB_UTF16:
									SrcX += PUSHED_LOCKED -
											ToolButtons[IDB_ANSI].xPos;
									SrcY  = NEXT_ROW;
									break;

								case IDB_HEX:
									SrcX  = PUSHED_LOCKED +
											ToolButtons[IDB_HEX].Width;
									break;
							}
						}
					} else {
						SrcY = NEXT_ROW;
						if (ToolButtons[i].Locked) {
							switch (i) {
								case IDB_ANSI:
								case IDB_OEM:
								case IDB_UTF8:
								case IDB_UTF16:
									SrcX += PUSHED_LOCKED -
											ToolButtons[IDB_ANSI].xPos;
									SrcY  = 0;
									break;

								case IDB_HEX:
									SrcX  = PUSHED_LOCKED;
									SrcY += NEXT_ROW;
									break;
							}
						}
					}
					BitBlt(hDC, XBUTTON_OFFSET + ToolButtons[i].xPos,
								yButtonOffset, ToolButtons[i].Width, ROW_HEIGHT,
						   hCompDC, SrcX, SrcY, SRCCOPY);
					SelectObject(hCompDC, OldBitmap);
					DeleteDC(hCompDC);
				}
				ReleaseDC(hwndMain, hDC);
			}
		}
}

static INT DisabledCount = 0;

void Disable(WORD Flags)
	/* Flags: 1=repaint toolbar
	 *		  2=suppress caret hiding and cursor switch to hourglass
	 */
{	HDC hDC, hCompDC;

	if (!(Flags & 2)) HideEditCaret();
	++DisabledCount;
	if (Disabled > (INT)(Flags & 1)) return;
	if (Flags & 1 && (hDC = GetDC(hwndMain)) != 0) {
		hCompDC = CreateCompatibleDC(hDC);
		if (hCompDC) {
			HBITMAP	OldBitmap = SelectObject(hCompDC, Buttons);
			INT		i;

			BitBlt(hDC, XBUTTON_OFFSET, yButtonOffset, ROW_WIDTH-1, ROW_HEIGHT,
				   hCompDC, 0, 2*NEXT_ROW, SRCCOPY);
			ToolButtons[IDB_OPEN].Pushed = ToolButtons[IDB_SAVE].Pushed = FALSE;
			if (ToolButtons[i=IDB_ANSI ].Pushed ||
				ToolButtons[i=IDB_OEM  ].Pushed ||
				ToolButtons[i=IDB_UTF8 ].Pushed ||
				ToolButtons[i=IDB_UTF16].Pushed)
					BitBlt(hDC,
						   XBUTTON_OFFSET + ToolButtons[i].xPos, yButtonOffset,
						   ToolButtons[i].Width, ROW_HEIGHT,
						   hCompDC, PUSHED_LOCKED - ToolButtons[IDB_ANSI].xPos
						   						  + ToolButtons[i].xPos,
						   NEXT_ROW, SRCCOPY);
			if (ToolButtons[IDB_HEX].Pushed)
				BitBlt(hDC,
					   XBUTTON_OFFSET+ToolButtons[IDB_HEX].xPos, yButtonOffset,
					   ToolButtons[IDB_HEX].Width, ROW_HEIGHT,
					   hCompDC, PUSHED_DISABLED, 2*NEXT_ROW, SRCCOPY);
			SelectObject(hCompDC, OldBitmap);
			DeleteDC(hCompDC);
		}
		ReleaseDC(hwndMain, hDC);
	}
	if (!(Flags & 2)) SwitchCursor(SWC_HOURGLASS_ON);
	Disabled = (Flags & 1) + 1;
	Interruptable = TRUE;
}

void Enable(void)
{	static RECT r;
	HDC hDC;

	if (DisabledCount && --DisabledCount) return;
	if (Disabled > 1 && hwndMain) {
		Disabled = 0;  /*must be reset before painting tool bottons*/
		r.right  = ClientRect.right;
		r.bottom = nToolbarHeight;
		hDC = GetDC(hwndMain);
		if (hDC) {
			PaintTools(hDC, &r);
			ReleaseDC(hwndMain, hDC);
		}
	} else Disabled = 0;
	Interruptable = FALSE;
	SwitchCursor(SWC_HOURGLASS_OFF);
}

BOOL IsToolButtonEnabled(INT Which)
{
	return !ToolButtons[Which].Disabled;
}

void EnableToolButton(INT Which, BOOL Enable)
{
	if (ToolButtons[Which].Disabled != !Enable) {
/*		ToolButtons[Which].Disabled  = !Enable;
 *Above line does not work properly with lcc, the line below seems to be ok...
 */
		ToolButtons[Which].Disabled  = !ToolButtons[Which].Disabled;
		if (hwndMain) {
			RECT r;

			r.left   = XBUTTON_OFFSET + ToolButtons[Which].xPos;
			r.top    = yButtonOffset;
			r.right  = r.left + ToolButtons[Which].Width;
			r.bottom = r.top + ROW_HEIGHT;
			InvalidateRect(hwndMain, &r, FALSE);
			UpdateWindow(hwndMain);
		}
	}
}

void ToggleHexMode(void)
{	extern BOOL	HexEditTextSide;
	extern INT	HexEditFirstNibble;
	extern INT	HexTextHeight, TextTextHeight;
	extern BOOL IsFixedFont;

	/*HexEditTextSide = FALSE;*/
	HexEditFirstNibble = -1;
	if (HexEditMode == ToolButtons[IDB_HEX].Locked) {
		/*command got via menu or ex command, button not yet changed*/
		ToolButtons[IDB_HEX].Locked = ToolButtons[IDB_HEX].Pushed ^= TRUE;
		RepaintButton(IDB_HEX);
	}
	HideEditCaret();
	HexEditMode = ToolButtons[IDB_HEX].Locked;
	if (HexEditMode) {
		extern LONG xOffset;
		INT		    BytePos = (INT)CountBytes(&CurrPos) & 15;

		ScreenStart = CurrPos;
		GoBack(&ScreenStart, 16*CurrLine + BytePos);
		xOffset = 0;
		TextHeight = HexTextHeight;
	} else {
		INT c, LinesBack = CurrLine;

		if (CharFlags(c = CharAt(&CurrPos)) & 1) {
			if (c == '\n') {
				/*check if position is between CR and LF...*/
				if (GoBack(&CurrPos, 1 + (UtfEncoding == 16))
					&& (c = CharAt(&CurrPos)) != C_CRLF)
						Advance(&CurrPos, 1 + (UtfEncoding == 16));
			}
			if (Mode==CommandMode && CharFlags(c = GoBackAndChar(&CurrPos)) & 1)
				if (c != C_EOF) CharAndAdvance(&CurrPos);
		} else if (c == C_UNDEF && UtfEncoding == 8) {
			if ((c = ByteAt(&CurrPos)) >= 0x80 && c < 0xc0)
				if ((c = GoBackAndByte(&CurrPos)) >= 0x80 && c < 0xc0)
					if ((c = GoBackAndByte(&CurrPos)) >= 0x80 && c < 0xc0)
						c = GoBackAndByte(&CurrPos);
			if (CharAt(&CurrPos) == C_BOM && CountBytes(&CurrPos) == 0)
				Advance(&CurrPos, 3);
		}
		ScreenStart = CurrPos;
		do {
			do; while (!(CharFlags(c = GoBackAndChar(&ScreenStart)) & 1));
		} while (--LinesBack >= 0);
		if (c != C_EOF) CharAndAdvance(&ScreenStart);
		VerifyPageStructure();
		TextHeight = TextTextHeight;
	}
	AveCharWidth = 0;		/*set again in first paint operation*/
	IsFixedFont  = FALSE;	/*...*/
	AdjustWindowParts(ClientRect.bottom, ClientRect.bottom);
	NewScrollPos();
	InvalidateText();
	if (CurrPos.p != NULL) NewPosition(&CurrPos);
	/*else WinVi is in initialization phase, position is updated later*/
	ShowEditCaret();
	GetXPos(&CurrCol);
}

void DisableMenuItems(INT MenuIndex, HMENU hMenu)
{
	switch (MenuIndex) {
		case 0:	EnableMenuItem(hMenu, ID_NEW,	 ToolButtons[IDB_NEW].Disabled
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_OPEN,	 ToolButtons[IDB_OPEN].Disabled
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_SAVE,	 ToolButtons[IDB_SAVE].Disabled
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_ALTERNATE,
							   *AltFileName=='\0' || ExecRunning()
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_READFILE, ViewOnlyFlag
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				break;
		case 1:	EnableMenuItem(hMenu, ID_TAG, !(CharFlags(CharAt(&CurrPos)) & 8)
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_TAGBACK, IsTagStackEmpty()
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_UNDO,	 ToolButtons[IDB_UNDO].Disabled
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_CUT,	 ToolButtons[IDB_CUT].Disabled
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_COPY,	 ToolButtons[IDB_COPY].Disabled
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_DELETE, ToolButtons[IDB_CUT].Disabled
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_PASTE,  ToolButtons[IDB_PASTE].Disabled
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				break;
		case 2: EnableMenuItem(hMenu, ID_SEARCH,
							   ToolButtons[IDB_SEARCH].Disabled
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_SRCHAGN,
							   ToolButtons[IDB_SRCHAGN].Disabled
							   ? MF_BYCOMMAND | MF_GRAYED : MF_BYCOMMAND);
		case 3: CheckMenuItem(hMenu, ID_ANSI,
							  ToolButtons[IDB_ANSI].Pushed
							  ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND);
				CheckMenuItem(hMenu, ID_OEM,
							  ToolButtons[IDB_OEM].Pushed
							  ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND);
				CheckMenuItem(hMenu, ID_EBCDIC,
							  !(ToolButtons[IDB_ANSI].Pushed ||
								ToolButtons[IDB_OEM].Pushed  ||
								ToolButtons[IDB_UTF8].Pushed ||
								ToolButtons[IDB_UTF16].Pushed)
							  ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND);
				CheckMenuItem(hMenu, ID_UTF8,
							  ToolButtons[IDB_UTF8].Pushed
							  ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND);
				CheckMenuItem(hMenu, ID_UTF16LE,
							  ToolButtons[IDB_UTF16].Pushed && UtfLsbFirst
							  ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND);
				CheckMenuItem(hMenu, ID_UTF16BE,
							  ToolButtons[IDB_UTF16].Pushed && !UtfLsbFirst
							  ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND);
				CheckMenuItem(hMenu, ID_HEX,
							  ToolButtons[IDB_HEX].Pushed
							  ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND);
	}
}

void CheckClipboard(void)
{	RECT r;

	if (!SelectCount != ToolButtons[IDB_COPY].Disabled) {
		extern HWND hwndSearch;
		ToolButtons[IDB_COPY].Disabled ^= TRUE^FALSE;
		r.left  = ToolButtons[IDB_COPY].xPos;
		if (ViewOnlyFlag) {
			if (!ToolButtons[IDB_CUT].Disabled) {
				ToolButtons[IDB_CUT].Disabled = TRUE;
				r.left  = ToolButtons[IDB_CUT].xPos;
			}
		} else {
			ToolButtons[IDB_CUT].Disabled = ToolButtons[IDB_COPY].Disabled;
			r.left  = ToolButtons[IDB_CUT].xPos;
		}
		r.right = ToolButtons[IDB_COPY].xPos + ToolButtons[IDB_COPY].Width;
		if (hwndSearch) {
			HWND Button = GetDlgItem(hwndSearch, IDC_REPLACE);

			if (Button && IsWindowVisible(Button))
				EnableWindow(Button, SelectCount!=0);
		}
	} else r.left = 0;
	if ((!IsClipboardFormatAvailable(CF_TEXT) || ViewOnlyFlag)
			!= ToolButtons[IDB_PASTE].Disabled) {
		ToolButtons[IDB_PASTE].Disabled ^= TRUE^FALSE;
		if (!r.left) r.left = ToolButtons[IDB_PASTE].xPos;
		r.right = ToolButtons[IDB_PASTE].xPos + ToolButtons[IDB_PASTE].Width;
	}
	/*check for a default printer...*/
	{	CHAR Buf[82];
		INT  n = GetProfileString("windows", "device", "", Buf, sizeof(Buf));

		if ((n == 0) != ToolButtons[IDB_PRINT].Disabled) {
			ToolButtons[IDB_PRINT].Disabled ^= TRUE^FALSE;
			if (!r.left) r.right = ToolButtons[IDB_PRINT].xPos
								 + ToolButtons[IDB_PRINT].Width;
			r.left = ToolButtons[IDB_PRINT].xPos;
		}
	}
	if (r.left && hwndMain) {
		r.left	+= XBUTTON_OFFSET;
		r.right += XBUTTON_OFFSET;
		r.bottom = (r.top = yButtonOffset+2) + ROW_HEIGHT-5;
		InvalidateRect(hwndMain, &r, FALSE);
	}
}

INT AbsByte(INT where)
{	POSITION Pos;

	Pos.p = FirstPage;
	Pos.i = where;
	Pos.f = 0;
	return ByteAt(&Pos);
}

BOOL AdditionalNullByte = FALSE;

void SetCharSet(UINT CSet, UINT Flags)
	/*Flags: 1=Invalidate Text and show cursor
	 *		 2=Update toolbar buttons
	 *		 4=Guess Little/Big Endian if switching to UTF-16
	 */
{	INT  BomBytes = 0;
	BOOL CountNewlines = FALSE;

	if (CSet == CS_AUTOENCODING) CSet = CS_ANSI;
	if (Flags & 4 && (CharSet == CS_UTF16LE || CharSet == CS_UTF16BE)) {
		/* typing UTF-16 button toggles between LE and BE... */
		CSet = CharSet == CS_UTF16LE ? CS_UTF16BE : CS_UTF16LE;
		Flags &= ~4;
	}
	CountNewlines |= CSet == CS_UTF16LE || CSet == CS_UTF16BE
										|| UtfEncoding == 16;
	UtfEncoding = CSet == CS_UTF8 ? 8 : CSet == CS_UTF16LE ||
										CSet == CS_UTF16BE ? 16 : 0;
	UtfLsbFirst = !(Flags & 4) && CSet == CS_UTF16LE;
	if (CharSet == CSet) return;
	if (CSet == CS_EBCDIC) {
		/* EBCDIC on */
		if (hwndMain) {
			RECT r;

			r.left   = ToolButtons[IDB_ANSI+CharSet].xPos   + XBUTTON_OFFSET;
			r.right  = r.left + ToolButtons[IDB_ANSI].Width + XBUTTON_OFFSET;
			r.bottom = (r.top = yButtonOffset)              + NEXT_ROW;
			InvalidateRect(hwndMain, &r, FALSE);
		}
		CharSet = CSet;
		SearchNewCset();
		ToolButtons[IDB_ANSI ].Locked = ToolButtons[IDB_ANSI ].Pushed =
		ToolButtons[IDB_OEM  ].Locked = ToolButtons[IDB_OEM  ].Pushed =
		ToolButtons[IDB_UTF8 ].Locked = ToolButtons[IDB_UTF8 ].Pushed =
		ToolButtons[IDB_UTF16].Locked = ToolButtons[IDB_UTF16].Pushed = FALSE;
		CountNewlines = TRUE;
	} else {
		if (CharSet == CS_EBCDIC) {
			Flags |= 2; /*force repaint if switching from EBCDIC*/
			CountNewlines = TRUE;
		}
		CharSet = CSet;
		SearchNewCset();
		if (ToolButtons[IDB_ANSI ].Pushed != (CSet==CS_ANSI ))	 Flags |= 2;
		if (ToolButtons[IDB_OEM  ].Pushed != (CSet==CS_OEM  ))	 Flags |= 2;
		if (ToolButtons[IDB_UTF8 ].Pushed != (CSet==CS_UTF8 ))	 Flags |= 2;
		if (ToolButtons[IDB_UTF16].Pushed != (CSet==CS_UTF16LE ||
											  CSet==CS_UTF16BE)) Flags |= 2;
		if (Flags & 2) {
			RECT r;
			INT  i, j;

			switch (CSet) {
				default:		 i = IDB_ANSI;	break;
				case CS_OEM:	 i = IDB_OEM;	break;
				case CS_UTF8:	 i = IDB_UTF8;	break;
				case CS_UTF16LE:
				case CS_UTF16BE: i = IDB_UTF16;	break;
			}
			ToolButtons[i].Locked = ToolButtons[i].Pushed = TRUE;
			for (j = IDB_ANSI; j <= IDB_UTF16; ++j)
				if (j != i)
					ToolButtons[j].Locked = ToolButtons[j].Pushed = FALSE;
			if (hwndMain) {
				r.left   = ToolButtons[IDB_ANSI ].xPos  + XBUTTON_OFFSET;
				r.right  = ToolButtons[IDB_UTF16].xPos  +
						   ToolButtons[IDB_UTF16].Width + XBUTTON_OFFSET;
				r.bottom = (r.top = yButtonOffset)      + NEXT_ROW;
				InvalidateRect(hwndMain, &r, FALSE);
			}
		}
	}
	if (FirstPage != NULL) {
		LPPAGE	Page, PrevPage;
		LONG	CntNl[9], Newlines, Carry;
		DWORD	Flags2;

		if (CountNewlines) {
			memset(CntNl, 0, sizeof(CntNl));
			Flags2	 = 0;
			Newlines = 0;
			Carry	 = -1;
			Page	 = FirstPage;
			PrevPage = NULL;
			if (CSet == CS_UTF8 || CSet == CS_UTF16LE || CSet == CS_UTF16BE) {
				POSITION p;

				BeginOfFile(&p);
				BomBytes = CountBytes(&p);
				if (!BomBytes && Flags & 4 && UtfEncoding == 16) {
					UtfLsbFirst = TRUE;
					switch (AbsByte(0)) {
						case 0xff:
							if (AbsByte(1) != 0xfe) break;
							BomBytes += 2;
							break;

						case 0:
							UtfLsbFirst = FALSE;
							break;
					}
				}
			}
			SwitchCursor(SWC_HOURGLASS_ON);
			while (Page != NULL) {
				if (Page->Fill > 0) {
					if (Page->PageBuf != NULL || ReloadPage(Page)) {
						if (UtfEncoding == 16) {
							if (Carry != -1 || Page->Fill & 1) {
								INT LastChar = Page->PageBuf[Page->Fill-1];

								if (Carry != -1) {
									memmove(Page->PageBuf+1, Page->PageBuf,
											(Page->Fill - 1) | 1);
									Page->PageBuf[0] = (BYTE)Carry;
									if (Page->Fill & 1) {
										Carry = -1;
										++Page->Fill;
									} else Carry = LastChar;
									if (CurrPos.p == Page) ++CurrPos.i;
								} else {
									Carry = LastChar;
									--Page->Fill;
								}
								Page->Flags &= ~ISSAFE;
							}
						}
						Newlines += CountNewlinesInBuf(Page->PageBuf,
													   Page->Fill,
													   CntNl, &Flags2);
						PrevPage = Page;
					}
				}
				Page->Newlines = Newlines;
				Page = Page->Next;
			}
			if (Carry != -1) {
				if (AdditionalNullByte && Carry == 0)
					AdditionalNullByte = FALSE;
				else {
					Page = PrevPage;
					if (Page->Fill == PAGESIZE) {
						if (Page->Next == NULL) {
							PrevPage->Next = Page = NewPage();
							if (Page != NULL) {
								LastPage	   = Page;
								Page->Newlines = Newlines;
								Page->PageNo   = PrevPage->PageNo + 1;
								Page->Prev	   = PrevPage;
							}
						} else {
							Page = Page->Next;
							if (Page->PageBuf == NULL) ReloadPage(Page);
						}
					}
					if (Page != NULL) {
						assert(Page->PageBuf != NULL);
						Page->PageBuf[Page->Fill++] = (BYTE)Carry;
						Page->PageBuf[Page->Fill++] = '\0';
						AdditionalNullByte = TRUE;
					}
				}
			} else if (AdditionalNullByte && UtfEncoding != 16) {
				assert(PrevPage != NULL && PrevPage->PageBuf != NULL);
				assert(PrevPage->Fill > 1);
				if (PrevPage->PageBuf[PrevPage->Fill-1] == '\0')
					--PrevPage->Fill;
				AdditionalNullByte = FALSE;
			}
			LinesInFile = Newlines;
			while (CurrPos.i >= CurrPos.p->Fill && CurrPos.p->Next != NULL) {
				CurrPos.i -= CurrPos.p->Fill;
				CurrPos.p  = CurrPos.p->Next;
			}
			if (CSet == CS_UTF16LE || CSet == CS_UTF16BE) {
				if (CurrPos.i   & 1) --CurrPos.i;
				if (SelectCount & 1) ++SelectCount;
			}
#if 0
//			if (UtfEncoding == 16 && SelectDefaultCharSet == CS_AUTOENCODING) {
//				INT c = AbsByte(0);
//
//				UtfLsbFirst = (c >= 0xfe && (c ^ AbsByte(1)) == 1
//							   ? (c == 0xff) : (CntNl[4] <= CntNl[5]));
//			}
#endif
			if (UtfEncoding == 16) {
				if (UtfLsbFirst != (CharSet == CS_UTF16LE))
					CharSet ^= CS_UTF16LE ^ CS_UTF16BE;
			} else {
				POSITION	p = CurrPos;

				if (CharAt(&p) == '\0' && AdvanceAndChar(&p) != '\0')
					CurrPos = p;
			}
			SwitchCursor(SWC_HOURGLASS_OFF);
			{	extern INT CountedDefaultNl;

				CountedDefaultNl = CntNl[0] > 0		   ? 1 :
								   CntNl[1] > CntNl[2] ? 3 :
								   CntNl[2] > 0		   ? 2 : 1;
				//SetDefaultNl();
			}
		} else if (CSet == CS_UTF8) {
			POSITION p;

			BeginOfFile(&p);
			BomBytes = CountBytes(&p);
		}
		if (CurrPos.p != NULL) {
			if (!HexEditMode) {
				ULONG i;
				INT	  i2, c;

				i = CountBytes(&CurrPos);
				if ((UINT)i < (UINT)BomBytes)
					Advance(&CurrPos, BomBytes-CountBytes(&CurrPos));
				else if (CSet == CS_UTF8) {
					POSITION	p = CurrPos;

					if ((c = ByteAt(&p)) >= 0x80 && c < 0xc0) {
						GoBackAndChar(&p);
						if ((c = ByteAt(&p)) >= 0x80 && c < 0xc0) {
							GoBackAndChar(&p);
							if ((c = ByteAt(&p)) >= 0x80 && c < 0xc0)
								GoBackAndChar(&p);
						}
					}
					if (CharAt(&p) != C_UNDEF) {
						POSITION	p2 = p;

						if (CharAndAdvance(&p2)!=C_UNDEF && CountBytes(&p2)>i)
							CurrPos = p;
					}
				}
				ScreenStart = CurrPos;
				for (i2 = CurrLine; i2 >= 0; --i2) {
					do; while (!(CharFlags(c=GoBackAndChar(&ScreenStart)) & 1));
					if (c == C_EOF) {
						CurrLine -= i2;
						break;
					}
				}
				if (c != C_EOF) CharAndAdvance(&ScreenStart);
			}
			if (Flags & 1 && hwndMain) {
				InvalidateText();
				ShowEditCaret();
				NewScrollPos();
				UpdateWindow(hwndMain);
			}
			NewPosition(&CurrPos);
		}
		FindValidPosition(&CurrPos, Mode != CommandMode);
	}
}

INT	  ActButton = -1;
PWSTR SaveStatus0String;
INT	  SaveStatus0Style;
BOOL  HideCmd;

static BOOL CheckToolButtonCaptureRelease(UINT uMsg)
{	BOOL SuppressBeep;

	if (uMsg != WM_LBUTTONUP) return FALSE;
	ActButton = -1;
	ReleaseCapture();
	if (hwndCmd) {
		HideCmd = FALSE;
		ShowWindow(hwndCmd, SW_SHOW);
		SetFocus(hwndCmd);
	}
	SuppressBeep = SuppressStatusBeep;
	SuppressStatusBeep = TRUE;
	NewStatus(0, SaveStatus0String, SaveStatus0Style);
	SuppressStatusBeep = SuppressBeep;
	return TRUE;
}

INT CursorButton(LPARAM lPar)
{	INT i;

	for (i=0; ToolButtons[i].Id; ++i) {
		INT x = XBUTTON_OFFSET + ToolButtons[i].xPos;

		if ((short)LOWORD(lPar) >= x &&
			(short)LOWORD(lPar) < x+ToolButtons[i].Width)
				return i;
	}
	return -1;
}

INT		 TooltipSuccess, TooltipButton;
LPARAM	 TooltipPosition;
COLORREF TooltipFgColor, TooltipBgColor;
HWND	 TooltipWindow;

LRESULT CALLBACK TooltipProc(HWND hWnd, UINT uMsg, WPARAM wPar, LPARAM lPar)
{
	switch(uMsg) {
		case WM_ERASEBKGND:
			{	RECT   r;
				HBRUSH BkBrush;

				GetClientRect(hWnd, &r);
				BkBrush = CreateSolidBrush
								((COLORREF)GetWindowLong(hWnd, sizeof(LONG)));
				if (BkBrush) {
					FillRect((HDC)wPar, &r, BkBrush);
					DeleteObject(BkBrush);
				}
			}
			return TRUE;

		case WM_PAINT:
			{	PAINTSTRUCT	ps;
				HDC			hDC = BeginPaint(hWnd, &ps);
				HFONT		hfontOld;
				WCHAR		Buf[300];
				INT			YPos;

				if (!hDC) break;
				hfontOld = SelectObject(hDC,
										(HFONT)GetWindowLong(hWnd,
															 2*sizeof(LONG)));
				SetTextColor(hDC, (COLORREF)GetWindowLong(hWnd, 0));
				SetBkMode(hDC, TRANSPARENT);
				YPos = (INT)GetWindowLong(hWnd, 3*sizeof(LONG));
				GetWindowTextW(hWnd, Buf, WSIZE(Buf));
				TextOutW(hDC, 2, YPos, Buf, wcslen(Buf));
				SelectObject(hDC, hfontOld);
				EndPaint(hWnd, &ps);
			}
			break;

		case WM_SETFOCUS:
			SetFocus(hwndMain);
			break;

		default:
			return DefWindowProcW(hWnd, uMsg, wPar, lPar);
	}
	return 0;
}

INT TooltipPopupTime = 500;

void TooltipTimer(void)
{	POINT pt;

	if (TooltipSuccess && TooltipButton >= 0) {
		if (++TooltipSuccess == 2 && GetFocus() == hwndMain) {
			/*show tooltip window...*/
			HDC   hDC;
			SIZE  Size;

			if (!TooltipFont) {
				#if defined(WIN32)
				{	static NONCLIENTMETRICS ncmInfo;

					TooltipFgColor = GetSysColor(COLOR_INFOTEXT);
					TooltipBgColor = GetSysColor(COLOR_INFOBK);
					ncmInfo.cbSize = sizeof(ncmInfo);
					if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
											  0, &ncmInfo, 0)) {
						HDC   hDC;

						lstrcpyn(ncmInfo.lfStatusFont.lfFaceName,
								 "MS Sans Serif",
								 sizeof(ncmInfo.lfStatusFont.lfFaceName));
						hDC = GetDC(NULL);
						if (hDC) {
							ncmInfo.lfStatusFont.lfHeight =
								-MulDiv(ncmInfo.lfStatusFont.lfHeight,
										GetDeviceCaps(hDC, LOGPIXELSY), 72);
							ReleaseDC(NULL, hDC);
						} else ncmInfo.lfStatusFont.lfHeight = 10;
					}
					TooltipFont = CreateFontIndirect(&ncmInfo.lfStatusFont);
				}
				#else
				{	static LOGFONT LogFont;
					HDC hDC;

					TooltipFgColor = TooltipBgColor = RGB(0,0,0);
					lstrcpyn(LogFont.lfFaceName, "MS Sans Serif",
							 sizeof(LogFont.lfFaceName));
					if (hDC = GetDC(NULL)) {
						LogFont.lfHeight = -MulDiv(8,
											GetDeviceCaps(hDC, LOGPIXELSY), 72);
						ReleaseDC(NULL, hDC);
					} else LogFont.lfHeight = 10;
					TooltipFont = CreateFontIndirect(&LogFont);
				}
				#endif
				if (TooltipFgColor == TooltipBgColor) {
					TooltipFgColor = RGB(000,000,000);
					TooltipBgColor = RGB(255,255,192);
				}
				if (!TooltipFont) return;
			}
			hDC = GetDC(NULL);
			if (hDC) {
				HFONT hfontOld = SelectObject(hDC, TooltipFont);
				GetTextExtentPoint(hDC,    ToolButtons[TooltipButton].Tip,
								   lstrlen(ToolButtons[TooltipButton].Tip),
								   &Size);
				SelectObject(hDC, hfontOld);
				ReleaseDC(NULL, hDC);
			} else Size.cx = 0;
			if (GetKeyState(VK_LBUTTON) < 0) return;
			GetCursorPos(&pt);
			ScreenToClient(hwndMain, &pt);
			if (!Size.cx) return;
			if (	pt.y <  yButtonOffset ||
					pt.y >= yButtonOffset  + ROW_HEIGHT ||
					pt.x <  XBUTTON_OFFSET + ToolButtons[TooltipButton].xPos ||
					pt.x >= XBUTTON_OFFSET + ToolButtons[TooltipButton].xPos
										   + ToolButtons[TooltipButton].Width)
				return;
			ClientToScreen(hwndMain, &pt);
			TooltipWindow = CreateWindow("WinVi - ToolTip",
										 ToolButtons[TooltipButton].Tip,
										 WS_BORDER | WS_POPUP,
										 pt.x, pt.y+19, Size.cx+7, Size.cy+4,
										 0 /*no parent*/, NULL, hInst, NULL);
			SetWindowLong(TooltipWindow, 0*sizeof(LONG), TooltipFgColor);
			SetWindowLong(TooltipWindow, 1*sizeof(LONG), TooltipBgColor);
			SetWindowLong(TooltipWindow, 2*sizeof(LONG), (UINT)TooltipFont);
			SetWindowLong(TooltipWindow, 3*sizeof(LONG), 1);
			SetWindowLong(TooltipWindow, GWL_EXSTYLE,
						  GetWindowLong(TooltipWindow, GWL_EXSTYLE)
						  | WS_EX_TOOLWINDOW);
			ShowWindow(TooltipWindow, SW_SHOWNA);
			SetCapture(hwndMain);
			SetTimer(hwndMain, 101, 5000, NULL);
		} else {
			if (TooltipSuccess <  3) TooltipSuccess = 3;
			if (TooltipSuccess == 3) {
				if (TooltipWindow) {
					DestroyWindow(TooltipWindow);
					TooltipWindow = NULL;
					if (GetCapture() == hwndMain) ReleaseCapture();
				}
				TooltipPopupTime = 500;
				KillTimer(hwndMain, 101);
			}
		}
	}
}

void TooltipHide(INT i)
{
	TooltipSuccess = 2;
	TooltipTimer();
	if (i != -1) TooltipButton  = i;
	UpdateWindow(hwndMain);
}

BOOL MouseForToolbar(UINT uMsg, WPARAM wPar, LPARAM lPar)
{	BOOL IsWithin = (short)HIWORD(lPar) <  yButtonOffset + ROW_HEIGHT
				 && (short)HIWORD(lPar) >= yButtonOffset;

	PARAM_NOT_USED(wPar);
	if (ActButton >= 0) {
		BOOL Within = IsWithin
						&& (short)LOWORD(lPar) >= XBUTTON_OFFSET
												+ ToolButtons[ActButton].xPos
						&& (short)LOWORD(lPar) <  XBUTTON_OFFSET
												+ ToolButtons[ActButton].Width
												+ ToolButtons[ActButton].xPos;

		if (ToolButtons[ActButton].Pushed && !Disabled) {
			HDC hDC;
			INT a = ActButton;

			if (uMsg == WM_LBUTTONUP) {
				if (ActButton >= IDB_ANSI && ActButton <= IDB_HEX) {
					if (Within) {
						if (ActButton == IDB_HEX)
							ToolButtons[ActButton].Locked ^= TRUE;
						else {
							ToolButtons[ActButton].Locked = TRUE;
							for (a = IDB_ANSI; a <= IDB_UTF16; ++a)
								if (a != ActButton && ToolButtons[a].Locked) {
									ToolButtons[a].Locked = FALSE;
									break;
								}
							if (a > IDB_UTF16) ++a;
						}
						PostMessage(hwndMain, WM_COMMAND,
									ToolButtons[ActButton].Id, 0L);
						hDC = GetDC(hwndMain);
						if (hDC) {
							HDC hCompDC = CreateCompatibleDC(hDC);

							if (hCompDC) {
								HBITMAP OldBitmap =
										SelectObject(hCompDC, Buttons);
								INT x = PUSHED_LOCKED;
								INT y = 0;

								switch (ActButton) {
									case IDB_HEX:
										y += 2 * NEXT_ROW;
										/*FALLTHROUGH*/
									case IDB_ANSI:
									case IDB_OEM:
									case IDB_UTF8:
									case IDB_UTF16:
										if (ActButton != IDB_HEX) {
											x += ToolButtons[ActButton].xPos -
												 ToolButtons[IDB_ANSI].xPos;
										}
										BitBlt(hDC,XBUTTON_OFFSET +
												   ToolButtons[ActButton].xPos,
												   yButtonOffset,
												   ToolButtons[ActButton].Width,
												   ROW_HEIGHT,
											   hCompDC, x, y, SRCCOPY);
										/*FALLTHROUGH*/
									default:
										break;
								}
								SelectObject(hCompDC, OldBitmap);
								DeleteDC(hCompDC);
							}
							ReleaseDC(hwndMain, hDC);
						}
					}
				} else {
					a = ActButton;
					PostMessage(hwndMain, WM_COMMAND, ToolButtons[a].Id, 0L);
				}
			} else if (uMsg != WM_MOUSEMOVE || Within
					   || ToolButtons[ActButton].Locked)
				return TRUE;
			if (a <= IDB_HEX && !ToolButtons[a].Locked) {
				ToolButtons[a].Pushed = FALSE;
				hDC = GetDC(hwndMain);
				if (hDC) {
					HDC hCompDC = CreateCompatibleDC(hDC);

					if (hCompDC) {
						HBITMAP OldBitmap = SelectObject(hCompDC, Buttons);

						BitBlt(hDC,
							   XBUTTON_OFFSET + ToolButtons[a].xPos,
							   yButtonOffset, ToolButtons[a].Width,
							   ROW_HEIGHT, hCompDC,
							   ToolButtons[a].xPos, 0, SRCCOPY);
						SelectObject(hCompDC, OldBitmap);
						DeleteDC(hCompDC);
					}
					ReleaseDC(hwndMain, hDC);
				}
			}
			CheckToolButtonCaptureRelease(uMsg);
			return TRUE;
		}
		if (CheckToolButtonCaptureRelease(uMsg)) return TRUE;
		if (!Within) return TRUE;
		uMsg = WM_LBUTTONDOWN;	/*fake for next if condition*/
	}
	if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK) {
		if (IsWithin) {
			INT i = CursorButton(lPar);

			if (i >= 0) {
				HDC hDC;

				TooltipHide(i);
				if (Disabled) {
					MessageBeep(MB_OK);
					return TRUE;
				}
				if (ToolButtons[i].Disabled) return TRUE;
				hDC = GetDC(hwndMain);
				if (hDC) {
					HDC hCompDC = CreateCompatibleDC(hDC);

					if (hCompDC) {
						HBITMAP OldBitmap = SelectObject(hCompDC, Buttons);
						INT		x, y;
						x = ToolButtons[i].xPos;
						y = NEXT_ROW;
						if (ToolButtons[i].Locked) {
							switch (i) {
								case IDB_ANSI:
								case IDB_OEM:
								case IDB_UTF8:
								case IDB_UTF16:
									x += PUSHED_LOCKED -
										 ToolButtons[IDB_ANSI].xPos;
									y  = 0;
									break;
								case IDB_HEX:
									x  = PUSHED_LOCKED;
									y += NEXT_ROW;
									break;
							}
						}
						BitBlt(hDC, XBUTTON_OFFSET + ToolButtons[i].xPos,
									yButtonOffset,
									ToolButtons[i].Width, ROW_HEIGHT,
							   hCompDC, x, y, SRCCOPY);
						SelectObject(hCompDC, OldBitmap);
						DeleteDC(hCompDC);
					}
					ReleaseDC(hwndMain, hDC);
				}
				if (ActButton == -1) {
					SaveStatus0String = StatusFields[0].Text;
					SaveStatus0Style  = StatusFields[0].Style;
					if (hwndCmd) {
						HideCmd = TRUE;
						SetFocus(hwndMain);
						ShowWindow(hwndCmd, SW_HIDE);
					}
					NewStatus(0, ToolButtons[i].StatusTip, NS_TOOLTIP);
					ActButton = i;
					SetCapture(hwndMain);
				}
				ToolButtons[i].Pushed = TRUE;
			}
		}
	} else if (IsWithin) {
		INT Curr = CursorButton(lPar);

		if (TooltipSuccess <= 1) {
			if (Curr >= 0) {
				TooltipSuccess	= 1;
				TooltipPosition	= lPar;
				if (TooltipButton == -1) {
					TooltipButton = Curr;
					TooltipTimer();
				} else TooltipButton = Curr;
				SetTimer(hwndMain, 101, TooltipPopupTime, NULL);
			}
		} else if (Curr != TooltipButton) {
			if (TooltipButton >= 0) {
				TooltipSuccess = 2;
				TooltipTimer();
				TooltipSuccess = 0;
			}
			if ((TooltipButton	 = Curr) >= 0) {
				TooltipSuccess	 = 1;
				TooltipPosition	 = lPar;
				TooltipPopupTime = 20;
				SetTimer(hwndMain, 101, TooltipPopupTime, NULL);
			} else {
				TooltipPopupTime = 500;
				KillTimer(hwndMain, 101);
			}
		} else if (TooltipSuccess == 2) SetTimer(hwndMain, 101, 5000, NULL);
	} else if (TooltipSuccess) {
		TooltipButton = 0;
		TooltipSuccess = 2;
		TooltipTimer();
		TooltipSuccess = 0;
	}
	return FALSE;
}
