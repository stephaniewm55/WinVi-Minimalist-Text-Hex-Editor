/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2001 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of this source file
 *		6-Dec-2000	new (darker) pen for tabctrl hilite if backgrnd is bright
 *     16-Nov-2002	multiline tabs
 */

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include "winvi.h"
#include "tabctrl.h"
#include "status.h"
#include "resource.h"
#include "ctl3d.h"

#if defined(OWN_TABCTRL)

extern INT    CurrTab;
extern HPEN   DlgLitePen, ShadowPen, BlackPen;
extern HBRUSH DlgBkBrush;

static struct {
	PSTR Label;
	INT  Row, Position, Width;
} TabLabels[16];
static INT	 CurrentLabel, Base = 10, ActiveRow, MaxRow = 0, RowHeight = 10;
static HFONT hTabFont;
static RECT  TabCtrlFocusRect;
static BOOL  TabCtrlHasFocus;

BOOL TabInsertItem(HWND hTab, INT Which, TC_ITEM *TcItem)
{	INT   Len = TcItem->cchTextMax;
	LPSTR Str = TcItem->pszText;
	INT   Row = MaxRow;
	RECT  r;

	GetClientRect(hTab, &r);
	if (Len>0 && *Str==' ') {
		++Str;
		--Len;
	}
	if (Len>0 && Str[Len-1]==' ') --Len;
	memmove(TabLabels+Which+1, TabLabels+Which, sizeof(TabLabels[0])
			* (sizeof(TabLabels)/sizeof(TabLabels[0])-Which-1));
	TabLabels[Which].Label = malloc(Len + 1);
	if (TabLabels[Which].Label != NULL) {
		HDC hDC;

		_fmemcpy(TabLabels[Which].Label, Str, Len);
		TabLabels[Which].Label[Len] = '\0';
		if (Which == 0) {
			TabLabels[Which].Position = 2;
			TabLabels[Which].Row = 0;
		} /*else already set*/
		TabLabels[Which].Width = 24;
		if (hTabFont) {
			hDC = GetDC(NULL);
			if (hDC) {
				HFONT OldFont;
				SIZE  Size;

				OldFont = SelectObject(hDC, hTabFont);
				GetTextExtentPoint(hDC, Str, Len, &Size);
				SelectObject(hDC, OldFont);
				ReleaseDC(NULL, hDC);
				TabLabels[Which].Width = Size.cx + 15;
				if (Size.cy + 8 > RowHeight) RowHeight = Size.cy + 8;
			}
		}
		do {
			if (TabLabels[Which].Position+TabLabels[Which].Width > r.right-12) {
				TabLabels[Which].Position = 2;
				TabLabels[Which].Row = ++Row;
			}
			TabLabels[Which+1].Position
				= TabLabels[Which].Position + TabLabels[Which].Width;
			TabLabels[++Which].Row = Row;
		} while (TabLabels[Which].Label != NULL);
	}
	MaxRow = Row;
	Base = RowHeight * (MaxRow + 1) - 1;
	if (CurrentLabel < Which) ActiveRow = TabLabels[CurrentLabel].Row;
	return (TRUE);
}

BOOL TabSetCurSel(HWND hTab, INT Which)
{
	if (CurrentLabel != Which) {
		RECT  Rect;
		NMHDR NmHdr;

		if (TabLabels[Which].Row != ActiveRow) {
			GetClientRect(hTab, &Rect);
			Rect.left	 = 0;
			Rect.top	 = 0;
			Rect.bottom	 = Base + 1;
			ActiveRow	 = TabLabels[Which].Row;
		} else {
			Rect.left	 = TabLabels[CurrentLabel].Position - 2;
			Rect.top	 = Base + 1 - RowHeight;
			Rect.right	 = Rect.left + TabLabels[CurrentLabel].Width + 4;
			Rect.bottom	 = Base + 1;
			InvalidateRect(hTab, &Rect, TRUE);
			Rect.left	 = TabLabels[Which].Position - 2;
			Rect.right	 = Rect.left + TabLabels[Which].Width + 4;
		}
		CurrentLabel	 = Which;
		InvalidateRect(hTab, &Rect, TRUE);
		NmHdr.code		 = TCN_SELCHANGE;
		NmHdr.hwndFrom	 = hTab;
		SendMessage(GetParent(hTab), WM_NOTIFY, 0, (LPARAM)(LPNMHDR)&NmHdr);
	}
	return (TRUE);
}

BOOL TabCtrlPaint(HWND hWnd, HDC hDC, PAINTSTRUCT *Ps)
{	INT	  i;
	HPEN  OldPen;
	HFONT OldFont;
	RECT  Rect;
	INT   EndOfTabsInActiveRow = 0;

	PARAM_NOT_USED(Ps);
	if (!hTabFont) hTabFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
	GetClientRect(hWnd, &Rect);
	OldPen  = SelectObject(hDC, DlgLitePen);
	OldFont = SelectObject(hDC, hTabFont);
	SetBkMode(hDC, TRANSPARENT);
	#if defined(WIN32)
		SetTextColor(hDC, GetSysColor(COLOR_BTNTEXT));
	#else
		SetTextColor(hDC, GetSysColor(Ctl3dEnabled() ? COLOR_BTNTEXT
													 : COLOR_WINDOWTEXT));
	#endif
	MoveTo(hDC, 0, Rect.bottom);
	LineTo(hDC, 0, Base);
	if (TabLabels[CurrTab].Position > 3) LineTo(hDC, 2, Base);
	for (i=0; TabLabels[i].Label!=NULL; ++i) {
		INT DisplayRow = (MaxRow - TabLabels[i].Row + ActiveRow) % (MaxRow + 1);
		INT RowPos	   = DisplayRow * RowHeight;

		if (i == CurrTab) {
			TextOut(hDC, TabLabels[i].Position+7, RowPos + 4,
					TabLabels[i].Label, lstrlen(TabLabels[i].Label));
			TabCtrlFocusRect.left  = TabLabels[i].Position+2;
			TabCtrlFocusRect.right = TabLabels[i].Position+TabLabels[i].Width-3;
			TabCtrlFocusRect.top   = RowPos + 3;
			TabCtrlFocusRect.bottom= RowPos + RowHeight - 1;
			if (TabCtrlHasFocus) DrawFocusRect(hDC, &TabCtrlFocusRect);
			/*extend borders of tab by 2 pixels to the left, up and right...*/
			SelectObject(hDC, DlgLitePen);
			MoveTo(hDC, TabLabels[i].Position-2, RowPos+RowHeight-1);
			LineTo(hDC, TabLabels[i].Position-2, RowPos+2);
			LineTo(hDC, TabLabels[i].Position,   RowPos);
			LineTo(hDC, TabLabels[i].Position+TabLabels[i].Width,   RowPos);
			SelectObject(hDC, BlackPen);
			MoveTo(hDC, TabLabels[i].Position+TabLabels[i].Width,   RowPos+1);
			LineTo(hDC, TabLabels[i].Position+TabLabels[i].Width+1, RowPos+2);
			LineTo(hDC, TabLabels[i].Position+TabLabels[i].Width+1,
				RowPos+RowHeight-1);
			SelectObject(hDC, ShadowPen);
			MoveTo(hDC, TabLabels[i].Position+TabLabels[i].Width,
				RowPos+RowHeight-1);
			LineTo(hDC, TabLabels[i].Position+TabLabels[i].Width,   RowPos+1);
			SelectObject(hDC, DlgLitePen);
			EndOfTabsInActiveRow = TabLabels[i].Position+TabLabels[i].Width+2;
		} else {
			INT XOffset = DisplayRow < MaxRow ? 3 : 0;
			INT YOffset = 2 * (MaxRow - DisplayRow);
			INT XRightPos = TabLabels[i].Position+TabLabels[i].Width+XOffset;
			INT ActiveTabBeneath = XRightPos > TabLabels[CurrTab].Position &&
								   XRightPos < TabLabels[CurrTab].Position +
											   TabLabels[CurrTab].Width;

			TextOut(hDC, TabLabels[i].Position+7+XOffset, RowPos+5+YOffset,
					TabLabels[i].Label, lstrlen(TabLabels[i].Label));
			SelectObject(hDC, DlgLitePen);
			if (i == CurrTab+1 && TabLabels[i].Row == ActiveRow) {
				MoveTo(hDC, TabLabels[i].Position+XOffset+2, RowPos+YOffset+2);
			} else {
				INT XLeftPos = TabLabels[i].Position+XOffset;
				INT ActiveTabBeneath = XLeftPos > TabLabels[CurrTab].Position &&
									   XLeftPos < TabLabels[CurrTab].Position +
												  TabLabels[CurrTab].Width;

				MoveTo(hDC, XLeftPos,	RowPos+RowHeight
							  + (DisplayRow < MaxRow ? !ActiveTabBeneath : -1));
				LineTo(hDC, XLeftPos,	RowPos+YOffset+4);
				LineTo(hDC, XLeftPos+2,	RowPos+YOffset+2);
			}
			LineTo(hDC, XRightPos-2, RowPos+YOffset+2);
			if (i == CurrTab-1 && TabLabels[i].Row == ActiveRow) {
				MoveTo(hDC, XRightPos-2, RowPos+RowHeight+YOffset-1);
			} else {
				SelectObject(hDC, BlackPen);
				MoveTo(hDC, XRightPos-2, RowPos+YOffset+3);
				LineTo(hDC, XRightPos-1, RowPos+YOffset+4);
				LineTo(hDC, XRightPos-1, RowPos+RowHeight+YOffset-2
					+ (DisplayRow == MaxRow ? 1 : !ActiveTabBeneath << 1));
				SelectObject(hDC, ShadowPen);
				MoveTo(hDC, XRightPos-2,
					RowPos+RowHeight+YOffset-1-(ActiveTabBeneath<<1));
				LineTo(hDC, XRightPos-2, RowPos+YOffset+3);
				SelectObject(hDC, DlgLitePen);
				MoveTo(hDC, XRightPos,   RowPos+RowHeight+YOffset-1);
			}
			if (TabLabels[i].Row == ActiveRow) {
				LineTo(hDC, TabLabels[i].Position+XOffset,
					RowPos+RowHeight+YOffset-1);
				EndOfTabsInActiveRow = XRightPos;
			}
		}
	}
	if (MaxRow > 0) {
		MoveTo(hDC, EndOfTabsInActiveRow, Base - RowHeight + 4);
		LineTo(hDC, Rect.right - 6, Base - RowHeight + 4);
	}
	MoveTo(hDC, TabLabels[CurrTab].Position+TabLabels[CurrTab].Width+1, Base);
	LineTo(hDC, Rect.right-1, Base);
	SelectObject(hDC, BlackPen);
	LineTo(hDC, Rect.right-1, Rect.bottom-1);
	LineTo(hDC, -1,			  Rect.bottom-1);
	SelectObject(hDC, ShadowPen);
	MoveTo(hDC, 1,			  Rect.bottom-2);
	LineTo(hDC, Rect.right-2, Rect.bottom-2);
	LineTo(hDC, Rect.right-2, Base);
	if (MaxRow > 0) {
		MoveTo(hDC, Rect.right - 6, Base - RowHeight + 5);
		LineTo(hDC, Rect.right - 6, Base - 1);
	}
	if (OldPen)  SelectObject(hDC, OldPen);
	if (OldFont) SelectObject(hDC, OldFont);
	return (TRUE);
}

LRESULT CALLBACK TabCtrlProc(HWND hWnd, UINT uMsg, WPARAM wPar, LPARAM lPar)
{
	switch(uMsg) {
		case WM_PAINT:
			{	static PAINTSTRUCT Ps;
				HDC				   hDC = BeginPaint(hWnd, &Ps);

				TabCtrlPaint(hWnd, hDC, &Ps);
				EndPaint(hWnd, &Ps);
			}
			break;

		case WM_ERASEBKGND:
			{	HDC  hDC = (HDC)wPar;
				RECT r;

				GetClientRect(hWnd, &r);
				r.bottom = Base + 1;
				FillRect(hDC, &r, DlgBkBrush);
			}
			return (TRUE);

		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
			if (HIWORD(lPar) < (WORD)Base) {
				INT  DisplayRow = HIWORD(lPar) / (RowHeight + 2);
				INT  Row		   = (MaxRow - DisplayRow + ActiveRow) % (MaxRow+1);
				INT  i;
				WORD x = LOWORD(lPar) - 3 * (MaxRow - DisplayRow);

				for (i=0; TabLabels[i].Label!=NULL; ++i) {
					if (x >= (WORD)TabLabels[i].Position
						&& x < (WORD)(TabLabels[i].Position+TabLabels[i].Width)
						&& Row == TabLabels[i].Row) {
							if (i == CurrentLabel) SetFocus(hWnd);
							else TabSetCurSel(hWnd, i);
							break;
					}
				}
			}
			break;

		case WM_KEYDOWN:
			if (wPar == VK_LEFT) {
				if (CurrentLabel) TabSetCurSel(hWnd, CurrentLabel-1);
			} else if (wPar==VK_RIGHT && TabLabels[CurrentLabel+1].Label!=NULL)
				TabSetCurSel(hWnd, CurrentLabel+1);
			break;

		case TCM_GETCURSEL:
			return (CurrentLabel);

		case WM_SETFOCUS:
			TabCtrlHasFocus = TRUE;
			InvalidateRect(hWnd, &TabCtrlFocusRect, FALSE);
			break;

		case WM_KILLFOCUS:
			TabCtrlHasFocus = FALSE;
			InvalidateRect(hWnd, &TabCtrlFocusRect, TRUE);
			break;

		case WM_GETDLGCODE:
			return (DLGC_WANTARROWS);

		default:
			return (DefWindowProc(hWnd, uMsg, wPar, lPar));
	}
	return (0);
}

BOOL TabCtrlInit(HWND hParent, HFONT hFont)
{	INT i;

	PARAM_NOT_USED(hParent);
	MaxRow = 0;
	for (i=0; TabLabels[i].Label!=NULL; ++i) {
		free(TabLabels[i].Label);
		TabLabels[i].Label = NULL;
	}
	hTabFont = hFont;
	return (TRUE);
}

#endif
