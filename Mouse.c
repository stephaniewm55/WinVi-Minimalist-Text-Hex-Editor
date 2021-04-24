/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2011 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      3-Dec-2000	first publication of this source file
 *     29-Sep-2002	ShowMatch for EBCDIC
 *     14-Nov-2002	prevent mouse positioning after Ctrl+Alt+V
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *      9-Jan-2011	bugfix: mouse drag above or below the text in long lines
 */

#include <windows.h>
#include <string.h>
#include <wchar.h>
#include "winvi.h"
#include "page.h"
#include "paint.h"
#include "status.h"
#include "toolbar.h"

#if !defined(WS_EX_TOOLWINDOW)
  #define WS_EX_TOOLWINDOW 0x00000080L
#endif

extern LONG xOffset;
extern BOOL EscapeInput;

BOOL       Selecting = FALSE, LButtonPushed = FALSE;
UINT	   TimerRunning = 0;
POSITION   SelectAnchor;
ULONG	   SelectAnchorCount;
POSITION   PrevMousePos;
LONG       PrevMouseCount;

static void MouseSelect(void)
{	long CurrCount;

	if (!ComparePos(&CurrPos, &PrevMousePos)) return;
	CurrCount = CountBytes(&CurrPos);
	if (ComparePos(&CurrPos, &PrevMousePos) > 0) {
		long Amount = CurrCount - PrevMouseCount;

		InvalidateArea(&PrevMousePos, Amount, 1);
	} else {
		long Amount = PrevMouseCount - CurrCount;

		InvalidateArea(&CurrPos, Amount, 1);
	}
	if (ComparePos(&CurrPos, &SelectAnchor) > 0) {
		SelectStart   = SelectAnchor;
		SelectCount   = CurrCount - SelectAnchorCount;
		SelectBytePos = SelectAnchorCount;
	} else {
		SelectStart   = CurrPos;
		SelectCount   = SelectAnchorCount - CurrCount;
		SelectBytePos = CurrCount;
	}
	UpdateWindow(hwndMain);
	CheckClipboard();
	UpdateWindow(hwndMain);
	PrevMousePos   = CurrPos;
	PrevMouseCount = CurrCount;
}

void MouseInTextArea(UINT uMsg, WPARAM wPar, int Line, int xPixel, int yPixel,
					 BOOL Activated)
{	BOOL Down;

	PARAM_NOT_USED(wPar);
	PARAM_NOT_USED(yPixel);
	if (TimerRunning) {
		KillTimer(hwndMain, 100);
		TimerRunning = 0;
	}
	if (Activated || uMsg==WM_LBUTTONUP) {
		if (LButtonPushed) {
			LButtonPushed = Selecting = FALSE;
			ReleaseCapture();
		}
		return;
	}
	if (EscapeInput) return;
	if (LineInfo == NULL /*Windows NT 3.51 initial state*/) return;
	while (LineInfo[Line].Flags & LIF_EOF && Line) --Line;
	Down = uMsg==WM_LBUTTONDOWN || uMsg==WM_LBUTTONDBLCLK;
	if (Down || LButtonPushed) {
		BOOL DoCapture = TRUE, ShiftPressed;

		if (Disabled) {
			MessageBeep(MB_OK);
			return;
		}
		ShiftPressed = Mode==InsertMode && GetKeyState(VK_SHIFT)<0;
		if (ShiftPressed && !SelectCount) {
			SelectStart   = SelectAnchor	  = CurrPos;
			SelectBytePos = SelectAnchorCount = CountBytes(&CurrPos);
		}
		Indenting = FALSE;
		CurrPos = LineInfo[Line].Pos;
		CurrCol.PixelOffset = xPixel+xOffset;
		if (Mode==InsertMode && uMsg!=WM_LBUTTONDBLCLK)
			CurrCol.PixelOffset += AveCharWidth >> 1;
		AdvanceToCurr(&CurrPos,/*Line,*/ (Mode==InsertMode || Mode==ReplaceMode)
										  && uMsg!=WM_LBUTTONDBLCLK
										 ? Mode==InsertMode ? 3 : 1 : 0);
		if (Down) {
			if (hwndCmd) {
				char Buf[4];

				if (GetWindowText(hwndCmd, Buf, sizeof(Buf))==1 && Buf[0]==':')
					/*"empty" command window (status line), remove...*/
					PostMessage(hwndMain, WM_USER+1234, 0, 0);
			}
			Selecting = Mode==InsertMode;
			if (ShiftPressed) ExtendSelection();
			else {
				if (SelectCount) {
					Unselect();
					UpdateWindow(hwndMain);
				}
				if (Selecting) {
					SelectStart = CurrPos;
					if (uMsg == WM_LBUTTONDBLCLK) {
						/*select word...*/
						int c, f = CharFlags(CharAt(&CurrPos));

						if (!(f & 1)) {
							do {
								c = GoBackAndChar(&SelectStart);
							} while (!((f ^ CharFlags(c)) & 0x39));
							if (c != C_EOF) CharAndAdvance(&SelectStart);
							do {
								c = AdvanceAndChar(&CurrPos);
							} while (!((f ^ CharFlags(c)) & 0x39));
							SelectCount = CountBytes(&CurrPos) -
										  CountBytes(&SelectStart);
							InvalidateArea(&SelectStart, SelectCount, 0);
							CheckClipboard();
							DoCapture = FALSE;
						}
					}
					SelectAnchor      = SelectStart;
					SelectAnchorCount = SelectBytePos = CountBytes(&SelectStart);
				}
			}
			if (DoCapture) {
				LButtonPushed = TRUE;
				SetCapture(hwndMain);
				PrevMousePos   = CurrPos;
				PrevMouseCount = CountBytes(&CurrPos);
			}
		} else if (Selecting) MouseSelect();
		NewPosition(&CurrPos);
		if (!DoCapture) {
			/*last line may have scrolled, reselect again (11-May-98)...*/
			InvalidateArea(&SelectStart, SelectCount, 0);
			/*word selected, cursor is not mouse position (7-Oct-2000)...*/
			GetXPos(&CurrCol);
		}
		ShowEditCaret();
		if (Mode == CommandMode) {
			INT c = CharAt(&CurrPos);

			switch (c) {
				case '(': case '[': case '{':
				case ')': case ']': case '}':
					ShowMatch(0);
			}
		}
	}
}

void MouseSelectText(UINT uMsg, WPARAM wPar, LONG xPixel, BOOL BelowText)
{
	PARAM_NOT_USED(wPar);
	if (LButtonPushed) {
		if (uMsg == WM_LBUTTONUP) {
			LButtonPushed = FALSE;
			ReleaseCapture();
			if (TimerRunning) {
				KillTimer(hwndMain, 100);
				TimerRunning = 0;
			}
		} else {
			int i;
			static BOOL SaveBelowText;
			static LONG SaveXPixel;
			UINT        Flags = 0x402;

			if (uMsg == WM_TIMER) {
				BelowText = SaveBelowText;
				xPixel    = SaveXPixel;
			} else {
				SaveBelowText = BelowText;
				SaveXPixel    = xPixel;
			}
			if (Selecting)			Flags |= 16;
			if (Mode == InsertMode)	Flags |= 1;
			if (BelowText) {
				if (!(LineInfo[CrsrLines].Flags & LIF_EOF))
					Position(1, CTRL('e'), Flags);
				i = CrsrLines-1;
			} else {
				Position(1, CTRL('y'), Flags);
				i = 0;
			}
			if (Selecting) {
				PrevMousePos   = CurrPos;
				PrevMouseCount = CountBytes(&CurrPos);
			}
			while (LineInfo[i].Flags & LIF_EOF && i) --i;
			CurrPos = LineInfo[i].Pos;
			CurrCol.PixelOffset = xPixel+xOffset;
			if (Mode == InsertMode) CurrCol.PixelOffset += AveCharWidth >> 1;
			AdvanceToCurr(&CurrPos, /*i,*/ (Mode==InsertMode  ? 3 : 0)
										 | (Mode==ReplaceMode ? 1 : 0));
			NewPosition(&CurrPos);
			ShowEditCaret();
			if (Selecting) MouseSelect();
			if (!TimerRunning)
				TimerRunning = SetTimer(hwndMain, 100, 10, NULL);
		}
	}
}

static HWND hwndStatusTip;

VOID RemoveStatusTip(VOID)
{
	if (hwndStatusTip) {
		DestroyWindow(hwndStatusTip);
		hwndStatusTip = NULL;
		ReleaseCapture();
	}
}

BOOL MouseHandle(UINT uMsg, WPARAM wPar, LPARAM lPar)
{	int Activated = MouseActivated;

	MouseActivated = FALSE;
	if (MouseForToolbar(uMsg, wPar, lPar)) {
		/*nothing*/
	} else if (!Activated && !LButtonPushed
			&& (short)HIWORD(lPar) >= ClientRect.bottom-StatusHeight) {
		extern BOOL SizeGrip;

		if ((short)LOWORD(lPar) >= ClientRect.right-16 && SizeGrip) {
			if (uMsg != WM_LBUTTONDOWN) {
				SwitchCursor(SWC_SIZEGRIP);
				return (FALSE);
			}
			SendMessage(hwndMain, WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, lPar);
		} else {
			if ( (short)LOWORD(lPar) <	StatusFields[0].x+StatusFields[0].Width
			  && (short)LOWORD(lPar) >= StatusFields[0].x+1
			  && (short)HIWORD(lPar) >= ClientRect.bottom-StatusHeight+5
			  && (short)HIWORD(lPar) <	ClientRect.bottom-StatusHeight+5
														 +StatusTextHeight) {
				if (uMsg == WM_LBUTTONDOWN) {
					if (!hwndCmd && !EscapeInput) {
						RemoveStatusTip();
						CommandEdit(':');
					}
				} else if (uMsg == WM_RBUTTONDOWN) {
					POINT p;
					GetCursorPos(&p);
					StatusContextMenu(p.x, p.y);
				} else if (GetFocus() == hwndMain) {
					/*check for status tip to display...*/
					if (StatusFields[0].TextExtent+6 > StatusFields[0].Width) {
						if (!hwndStatusTip && StatusFields[0].Text &&
											 *StatusFields[0].Text) {
							POINT pt;
							pt.x = StatusFields[0].x;
							pt.y = ClientRect.bottom-StatusHeight + 5;
							ClientToScreen(hwndMain, &pt);
							hwndStatusTip =
								CreateWindowW(L"WinVi - ToolTip",
											  StatusFields[0].Text,
											  WS_BORDER | WS_POPUP,
											  pt.x, pt.y,
											  StatusFields[0].TextExtent+7,
											  StatusTextHeight,
											  0 /*no parent*/,	NULL,
											  hInst,			NULL
											 );
							SetWindowLong(hwndStatusTip, 0*sizeof(LONG),
										  (LONG)StatusStyle
												[StatusFields[0].Style].Color);
							SetWindowLong(hwndStatusTip, 1*sizeof(LONG),
										  GetSysColor(COLOR_BTNFACE));
							SetWindowLong(hwndStatusTip, 2*sizeof(LONG),
										  (UINT)StatusFont);
							SetWindowLong(hwndStatusTip, 3*sizeof(LONG), -1);
							SetWindowLong(hwndStatusTip, GWL_EXSTYLE,
									  GetWindowLong(hwndStatusTip, GWL_EXSTYLE)
									  | WS_EX_TOOLWINDOW);
							SetCapture(hwndMain);
							ShowWindow(hwndStatusTip, SW_SHOWNA);
						}
					}
				}
			} else RemoveStatusTip();
		}
	} else {
		RemoveStatusTip();
		if ((short)HIWORD(lPar) >= nToolbarHeight + YTEXT_OFFSET &&
			(short)HIWORD(lPar) < ClientRect.bottom-StatusHeight-YTEXT_OFFSET) {
				int Line = ((short)HIWORD(lPar) - nToolbarHeight - YTEXT_OFFSET)
				   		   / TextHeight;
				SwitchCursor(SWC_TEXTAREA);
				MouseInTextArea(uMsg, wPar, Line,
								(short)LOWORD(lPar) - StartX(FALSE),
								StatusHeight + YTEXT_OFFSET + Line*TextHeight,
								Activated);
				return (TRUE);
		} else if (LButtonPushed) {
			MouseSelectText(uMsg, wPar, (short)LOWORD(lPar) - StartX(FALSE),
							(short)HIWORD(lPar) >= nToolbarHeight+YTEXT_OFFSET);
			return (TRUE);
		}
	}
	SwitchCursor(SWC_STATUS_OR_TOOLBAR);
	return (FALSE);
}

BOOL StatusContextMenuCommand(WORD wCommand)
{	HGLOBAL	hMem;
	PWSTR	pMem, pSrc;
	int		nLen;

	if (wCommand == 910) {
		pSrc = StatusFields[0].Text;
		if (*pSrc == ' ') ++pSrc;
		nLen = wcslen(pSrc);
	} else if (wCommand == 911) {
		pSrc = wcschr(StatusFields[0].Text, 0253);
		if (pSrc++ == NULL || (pMem = wcsrchr(pSrc, 0273)) == NULL)
			return (FALSE);
		nLen = pMem - pSrc;
	} else return (FALSE);
	if (!OpenClipboard(hwndMain)) {
		ErrorBox(MB_ICONEXCLAMATION, 302);
		return (FALSE);
	}
	if (!EmptyClipboard()) {
		CloseClipboard();
		ErrorBox(MB_ICONEXCLAMATION, 303);
		return (FALSE);
	}
	hMem = GlobalAlloc(GMEM_MOVEABLE, 2*nLen + 2);
	if (!hMem) {
		CloseClipboard();
		ErrorBox(MB_ICONEXCLAMATION, 304);
		return (FALSE);
	}
	pMem = GlobalLock(hMem);
	if (pMem == NULL) {
		CloseClipboard();
		GlobalFree(hMem);
		ErrorBox(MB_ICONEXCLAMATION, 305);
		return (FALSE);
	}
	_fmemcpy(pMem, pSrc, 2*nLen);
	pMem[nLen] = '\0';

	/*unlock and publish...*/
	GlobalUnlock(hMem);
	SetClipboardData(CF_UNICODETEXT, hMem);
	CloseClipboard();
	CheckClipboard();
	return (TRUE);
}
