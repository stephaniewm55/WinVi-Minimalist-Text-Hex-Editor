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
 *      6-Dec-2000	correct cursor position after pixel 32767 with 16-bit ints
 *      7-Dec-2000	appropriate handling of input factor/count overflow
 *     20-Jul-2002	don't accept excape key while inserting command output
 *     22-Jul-2002	use of private myassert.h
 *     13-Sep-2002	+EBCDIC
 *      4-Oct-2002	new positioning command CTRL('p')
 *      6-Oct-2002	new file management commands CTRL('o') und CTRL('s')
 *     29-Oct-2002	less processor load when showing matching parentheses
 *     12-Jan-2003	help contexts added
 *      7-Mar-2006	new calculation of caret dimensions
 *     10-Jul-2007	added UTF-8 and UTF-16 support
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *      6-Sep-2009	fixed positioning in UTF-8/16 files
 *      9-Jan-2010	Ctrl+End positioning to end of last line
 *     17-Jun-2010	marker position not changed when escaping search input
 */

#include <windows.h>
#include <ctype.h>
#include <string.h>
#include <wchar.h>
#include "myassert.h"
#include "resource.h"
#include "winvi.h"
#include "page.h"
#include "paint.h"		/*for XTEXT_OFFSET only*/
#include "intercom.h"
#include "status.h"
#include "toolbar.h"

extern WCHAR NumLock[], CapsLock[];
extern INT	 IndentChars;
extern BOOL	 SuppressMap, RestoreCaret, EscapeInput;
extern LONG	 xOffset;

MODEENUM	 ModeAfterTfmq;
BOOL		 InNumber;
WCHAR		 StatusCmd[40];
INT			 ShiftWidth = 4;
INT			 HexEditFirstNibble = -1;
BOOL		 HexEditTextSide = FALSE;
signed char	 CaretProps[20];
static INT	 PrevCaretWidth, PrevCaretHeight;

BOOL IsViewOnly(void)
{
	if (!ViewOnlyFlag) return (FALSE);
	Error(236);
	return (TRUE);
}

#define CARET_DEBUG(msg) /*OutputDebugString(msg)*/

void HorizontalScroll(LONG MinAmount)
{	LONG i;
	INT  Step = 5 * AveCharWidth;

	if (MinAmount >= 0) {
		i = MinAmount + Step - 1;
		i -= i % Step;
		i = -i;
	} else {
		if (!xOffset) return;
		i = -MinAmount + Step - 1;
		i -= i % Step;
		if (i > xOffset) i = xOffset;
	}
	if (hwndMain) {
		HDC hDC;

		if (CaretActive) {
			HideCaret(hwndMain);
			CARET_DEBUG("Hide Caret (HorizontalScrollIntoView)\n");
			CaretActive = FALSE;
		}
		hDC = GetDC(hwndMain);
		if (hDC) {
			RECT r, ir;

			r = ClientRect;
			TextAreaOnly(&r, TRUE);
			if (i < -32000 || i > 32000) ir = r;
			else ScrollDC(hDC, (INT)i, 0, &r, &r, NULL, &ir);
			ReleaseDC(hwndMain, hDC);
			xOffset -= i;
			InvalidateRect(hwndMain, &ir, TRUE);
			UpdateWindow(hwndMain);
		}
	}
}

void HorizontalScrollIntoView(void)
{	LONG Amount = CaretX - XTEXT_OFFSET - xOffset - (ClientRect.right>>1);

	if (Amount < 0) HorizontalScroll(Amount);
	else {
		LONG TooFar = CaretX + CaretWidth - xOffset - ClientRect.right +
					  GetSystemMetrics(SM_CXVSCROLL);
		if (TooFar > 0) HorizontalScroll(TooFar);
	}
}

BOOL HideEditCaret(void)
{
	if (!CaretActive) return (FALSE);
	DestroyCaret();
	CARET_DEBUG("Hide & Destroy Caret (HideEditCaret)\n");
	CaretActive = FALSE;
	PrevCaretWidth = 0;
	return (TRUE);
}

void ShowEditCaret(void)
{	BOOL		HasFocus		= GetFocus() == hwndMain;
	INT			ShowCaretWidth	= CaretWidth;
	INT			ShowCaretHeight	= CaretHeight;
	INT			ShowCaretX		= CaretX;
	INT			ShowCaretY		= CaretY;
	signed char	*ShowCaretProps = CaretProps;

	if (hwndMain && HasFocus && (!Disabled || EscapeInput) && !RefreshBitmap
				 && CaretWidth) {
		if (Mode == InsertMode) {
			ShowCaretProps += 10;
			if (*ShowCaretProps == '\0')
				memcpy(ShowCaretProps, "\x59\xff\2\0\0", 5);
		} else if (Mode == ReplaceMode) {
			ShowCaretProps += 15;
			if (*ShowCaretProps == '\0')
				memcpy(ShowCaretProps, "\x96\0\0\xfd\3", 5);
		} else /*Mode == CommandMode*/ {
			if (CaretType == COMMAND_CARET_EMPTYLINE) {
				ShowCaretProps += 5;
				if (*ShowCaretProps == '\0')
					memcpy(ShowCaretProps, "\x97\0\0\xff\2", 5);
			} else {
				if (*ShowCaretProps == '\0')
					memcpy(ShowCaretProps, "\x99\0\0\0\0", 5);
			}
		}
		switch (*ShowCaretProps & 0x03) {
		case 0x01:
			ShowCaretY += ShowCaretProps[3];
			break;
		case 0x02:
			ShowCaretY += ShowCaretHeight + ShowCaretProps[3];
			break;
		case 0x03:
			ShowCaretY += (ShowCaretHeight >> 1) + ShowCaretProps[3];
			break;
		}
		switch (*ShowCaretProps & 0x0c) {
		case 0x04:
			ShowCaretHeight  = ShowCaretProps[4];
			break;
		case 0x08:
			ShowCaretHeight += ShowCaretProps[4] - (ShowCaretY - CaretY);
			break;
		}
		switch (*ShowCaretProps & 0x30) {
		case 0x10:
			ShowCaretX += ShowCaretProps[1];
			break;
		case 0x20:
			ShowCaretX += ShowCaretWidth + ShowCaretProps[1];
			break;
		case 0x30:
			ShowCaretX += (ShowCaretWidth >> 1) + ShowCaretProps[1];
			break;
		}
		switch (*ShowCaretProps & 0xc0) {
		case 0x40:
			ShowCaretWidth  = ShowCaretProps[2];
			break;
		case 0x80:
			ShowCaretWidth += ShowCaretProps[2] - (ShowCaretX - CaretX);
			break;
		}
		if (ShowCaretWidth != PrevCaretWidth
				|| ShowCaretHeight != PrevCaretHeight) {
			if (CaretActive) {
				DestroyCaret();
				CARET_DEBUG("Hide and DestroyCaret (ShowEditCaret)\n");
				CaretActive = FALSE;
			}
			CreateCaret(hwndMain, NULL, PrevCaretWidth  = ShowCaretWidth,
										PrevCaretHeight = ShowCaretHeight);
			CARET_DEBUG("Create Caret (ShowEditCaret)\n");
		}
		HorizontalScrollIntoView();
		SetCaretPos((INT)(ShowCaretX-xOffset), ShowCaretY);
		CARET_DEBUG("SetPos Caret (ShowEditCaret)\n");
		if (!CaretActive) {
			ShowCaret(hwndMain);
			CARET_DEBUG("Show Caret (ShowEditCaret)\n");
			CaretActive = TRUE;
		}
		RestoreCaret = FALSE;
	}
}

void SwitchCursor(WORD wFlags)
{	static HCURSOR hTextCursor, hCurrentCursor, hDisplayCursor;
	static BOOL    fInTextArea, fShowingHourGlass;
	HCURSOR		   hNewCursor    = hCurrentCursor;
	BOOL		   fForceSetting = FALSE;

	hTextCursor = Mode==InsertMode ? hcurIBeam : hcurArrow;
	if (wFlags & (SWC_STATUS_OR_TOOLBAR | SWC_SIZEGRIP | SWC_TEXTAREA)) {
		fInTextArea = (wFlags & SWC_TEXTAREA) != 0;
		if (fInTextArea) hNewCursor = hTextCursor;
		else hNewCursor = wFlags & SWC_SIZEGRIP ? hcurSizeNwSe : hcurArrow;
		hCurrentCursor = hNewCursor;
		fForceSetting = TRUE;
	} else if (wFlags & SWC_TEXTCURSOR && fInTextArea)
		hCurrentCursor = hNewCursor = hTextCursor;
	if (wFlags & SWC_HOURGLASS_ON) {
		if (!fShowingHourGlass) {
			fShowingHourGlass = TRUE;
			if (!ShowCursor(TRUE)) fForceSetting = TRUE;
		}
	}
	if (wFlags & SWC_HOURGLASS_OFF) {
		if (fShowingHourGlass) {
			fShowingHourGlass = FALSE;
			hDisplayCursor	  = hcurHourGlass;		/*temporary*/
			ShowCursor(FALSE);
		}
	}
	if (fShowingHourGlass) hNewCursor = hcurHourGlass;
	if (fForceSetting) SetCursor(hDisplayCursor = hNewCursor);
	else if (hNewCursor != hDisplayCursor) {
		POINT pt;
		GetCursorPos(&pt);
		if (WindowFromPoint(pt) == hwndMain)
			SetCursor(hDisplayCursor = hNewCursor);
	}
}

void FindValidPosition(PPOSITION Pos, WORD Flags)
{	/* Flags:	1 = position may be behind last character of line (InsertMode),
	 *			2 = position may be behind last line of file
	 *			4 = suppress cursor positioning afterwards
	 */
	INT c = CharAt(Pos);
	INT f = CharFlags(c);

	if (c == C_BOM && CountBytes(Pos) == 0)
		f = CharFlags(c = AdvanceAndChar(Pos));
	if (f & 1) do {	 /*no loop, do is for break only*/
		if (c == C_EOF) {
			if (Flags & 2) break;
			if (HexEditMode) {
				GoBackAndByte(Pos);
			} else {
				f = CharFlags(c = GoBackAndChar(Pos));
				if (!(f & 1) && Flags & 1) AdvanceAndChar(Pos);
			}
		}
		if (f & 1 && !HexEditMode) {
			if (c == '\n') {
				if ((c = GoBackAndChar(Pos)) == C_EOF) break;
				if (CharFlags(c) & 1 || Flags & 1) AdvanceAndChar(Pos);
				break;
			}
			if (!(Flags&1) && CharFlags(c=GoBackAndChar(Pos)) & 1 && c!=C_EOF)
				AdvanceAndChar(Pos);
		}
	} while (FALSE);
	if (!(Flags & 4)) {
		NewPosition(Pos);
		GetXPos(&CurrCol);
	}
}

void AdvanceToCurr(PPOSITION Pos, /*INT Line,*/ INT Flags)
{	/*Pos must point to the start of the line*/
	/*Flags:		1 = position may be behind last char of line (ins/rep mode)
	 *				2 = hex insert mode, position between characters
	 */
	if (HexEditMode) {
		/*currently assuming fix character font*/
		HexEditTextSide = CurrCol.PixelOffset >= 62*AveCharWidth;
		if (HexEditTextSide) {
			/*right hand side ascii column...*/
			LONG i = CurrCol.PixelOffset - (4-1);
			INT	 Max = Flags & 2 ? 16 : 15;

			if (UtfEncoding == 16) Max >>= 1;
			i = i / AveCharWidth - 62;
			if (i > Max) i = Max;
			if (i > 0) Advance(Pos, UtfEncoding == 16 ? 2 * i : i);
		} else {
			/*hex display part...*/
			static INT QuadPositions[16] = {
				48,60,72,86, 99,111,123,137, 150,162,174,188, 201,213,225,237
			};
			LONG QuadCurrCol = (CurrCol.PixelOffset-(4-2)) << 2;
			INT  i, n = Flags & 2 ? 16 : 15 - (UtfEncoding == 16);

			if (Flags & 2) QuadCurrCol += AveCharWidth << 2;
			for (i=0; i<n; ++i) {
				if (UtfEncoding == 16) ++i;
				if (QuadCurrCol <= AveCharWidth*QuadPositions[i]) break;
				Advance(Pos, UtfEncoding == 16 ? 2 : 1);
			}
		}
		if (CharAt(Pos)==C_EOF && Mode!=InsertMode && Mode!=ReplaceMode)
			GoBack(Pos, UtfEncoding == 16 ? 2 : 1);
		HexEditFirstNibble = -1;
		return;
	}
	if (CurrCol.ScreenLine) {
		/*TODO: calculate continuation line*/
	}
	if (CurrCol.PixelOffset) {
		static WCHAR Buf[8];
		INT			 x = 0, Col = 0, c;
		BOOL		 TooFar = FALSE;
		HDC			 hDC;
		HFONT		 OldFont;

		hDC = GetDC(NULL);
		if (!hDC) return;
		if (TextFont) OldFont = SelectObject(hDC, TextFont);
		c = CharAt(Pos);
		while (x < CurrCol.PixelOffset) {
			if (CharFlags(c) & 1) break;
			if (c == '\t' && !ListFlag) {
				do; while (++Col % TabStop);
				x = Col * AveCharWidth;
			} else {
				INT n;

				if (CharSet == CS_OEM) {
					if (c == '\177') {
						wcscpy(Buf, L"\\7F");
						n = 3;
					} else {
						Buf[1] = c;
						UnicodeConvert(Buf, Buf+1, 1);
						n = 1;
					}
				} else {
					if (CharFlags(c) & 2) {
						INT  i;
						CHAR Buf2[4];

						if (c == C_UNDEF || CharSet == CS_EBCDIC)
							c = ByteAt(Pos);
						if (c < ' ' && CharSet != CS_EBCDIC)
							 n = wsprintf(Buf2, "^%c", c + ('A'-1));
						else n = wsprintf(Buf2, "\\%02X", c);
						for (i = 0; i < n; ++i) Buf[i] = Buf2[i];
					} else {
						*Buf = c;
						n = 1;
					}
				}
				x += GetTextWidth(hDC, Buf, n);
				Col += n;
			}
			if (x > CurrCol.PixelOffset) TooFar = TRUE;
			c = AdvanceAndChar(Pos);
			#if 0
			//	if (Pos->i==LineInfo[Line+1].Pos.i
			//			&& Pos->p==LineInfo[Line+1].Pos.p) {
			//		TooFar = TRUE;
			//		break;
			//	}
			#endif
		}
		if (TextFont && OldFont) SelectObject(hDC, OldFont);
		ReleaseDC(NULL, hDC);
		if (TooFar || (x && CharFlags(CharAt(Pos)) & 1 && !(Flags & 1)))
			GoBackAndChar(Pos);
	}
}

void CheckUtf16Lsb(void)
{
	switch (AbsByte(0)) {
		case 0xfe:
			if (AbsByte(1) == 0xff) {
				if (UtfLsbFirst) {
					UtfLsbFirst = FALSE;
					InvalidateText();
				}
			}
			break;

		case 0xff:
			if (AbsByte(1) == 0xfe) {
				if (!UtfLsbFirst) {
					UtfLsbFirst = TRUE;
					InvalidateText();
				}
			}
			break;
	}
}

INT QuoteCommandChar, QuoteChar;
INT TfCommandChar,    TfSearchChar;
INT BracketCommandChar;
INT DblQuoteChar;

typedef struct tagREDO {
	struct tagREDO FAR *Next;
	WCHAR			   Buffer[128];
} REDO, FAR *LPREDO;
REDO RedoChain;
LONG RedoFill;
UINT RedoCount;
BYTE RedoCmd[5];
BOOL Repeating;

BOOL DoCompoundCommand(UINT Multiplier, WPARAM Character, WPARAM EnteredChar,
					   WORD Flags)
{	/*Flags: 1=display command in second status field
	 *		 0x200=Position around line boundary (WrapPosFlag==TRUE)
	 */
	BOOL	 DoAdvance=FALSE, DontExtend;
	COLUMN	 YankCol;
	POSITION StartPos;
	LONG	 Amount, PositiveAmount;

	if ((Character != 'Z' || Unsafe) && IsViewOnly()) {
		Mode = CommandMode;
		return (FALSE);
	}
	if (Multiplier) {
		if (Number) Number *= Multiplier;
		else Number = Multiplier;
	}
	if (Flags & 1) {
		if (InNumber && CmdFirstChar != 'Z' && EnteredChar >= ' ')
			wcscat(StatusCmd, L" ");
		_snwprintf(StatusCmd + wcslen(StatusCmd),
				   WSIZE(StatusCmd) - wcslen(StatusCmd),
				   EnteredChar < ' ' ? L" ^%c"				 : L"%c",
				   EnteredChar < ' ' ? EnteredChar + ('A'-1) : EnteredChar);
		SetStatusCommand(StatusCmd, TRUE);
	}
	Flags |= 0x27;
	HideEditCaret();
	switch (CmdFirstChar) {
		case 'c': case 'd':
		case '<': case '>':
					if (!Repeating) {
						PSTR RedoP = RedoCmd;

						RedoCount = Number;
						if (DblQuoteChar) {
							*RedoP++ = '"';
							*RedoP++ = DblQuoteChar;
						}
						if (Mode == CmdGotFirstChar) {
							*RedoP++ = CmdFirstChar;
							*RedoP++ = Character;
						} else *RedoP++ = EnteredChar;
						*RedoP = '\0';
						RedoFill = 0;
					}
					if (CmdFirstChar != 'c') {
						Mode = CommandMode;
						break;
					}
					Mode = InsertMode;
					Flags |= 0x40;
					break;

		case 'y':	Mode = CommandMode;
					#if 0
					//	Flags |= 0x800;
					#endif
					break;

		case '!':	Mode = CommandMode;
					Flags |= 0x48;
					break;

		case 'Z':	Mode = CommandMode;
					if (Character == CmdFirstChar) CommandExec(L":x");
					else if (Character == 'Q') CommandExec(L":q!");
					else MessageBeep(MB_ICONEXCLAMATION);
					SetStatusCommand(NULL, FALSE);
					ShowEditCaret();
					return (Character == CmdFirstChar);

		default:	Mode = CommandMode;
	}
	DontExtend = Character==CmdFirstChar;
	if (DontExtend) {
		switch (CmdFirstChar) {
			case 'c': case '<': case '>':
				Position(1, '^', 0x403);
				Character = HexEditMode && CmdFirstChar=='c' ? '\r' : '$';
				break;

			case 'y':
				GetXPos(&YankCol);
				/*FALLTHROUGH*/
			default:
				Position(1, '0', 0x403);
				DoAdvance = TRUE;
				Character = HexEditMode ? '\r' : 'j';
				break;
		}
	}
	if (!Position(Number, Character, Flags)) {
		Mode = CommandMode;
		MessageBeep(MB_ICONEXCLAMATION);
		ShowEditCaret();
		return (FALSE);
	} else if (Mode != CommandMode) NewPosition(&CurrPos);
	Number	 = 0;
	Amount	 = PositiveAmount = RelativePosition;
	StartPos = CurrPos;
	if (Amount < 0) PositiveAmount = GoBack(&StartPos, -Amount);
	if (CmdFirstChar!='<' && CmdFirstChar!='>' && FullRowFlag
			&& !DontExtend && (CmdFirstChar=='!' || FullRowExtend)) {
		INT		 c;
		POSITION EndPos;

		do ++PositiveAmount;
		while (!(CharFlags(c = GoBackAndChar(&StartPos)) & 1));
		if (c != C_EOF) c = AdvanceAndChar(&StartPos);
		--PositiveAmount;
		if (CmdFirstChar=='c') {
			while (CharFlags(c) & 16) {
				c = AdvanceAndChar(&StartPos);
				--PositiveAmount;
			}
		}
		EndPos = StartPos;
		Advance(&EndPos, PositiveAmount);
		c = CharAt(&EndPos);
		while (!(CharFlags(c) & 1)) {
			++PositiveAmount;
			c = AdvanceAndChar(&EndPos);
		}
		if (CmdFirstChar!='c' && c!=C_EOF) {
			if (c == C_CRLF) ++PositiveAmount;
			++PositiveAmount;
		}
		if (CmdFirstChar=='d') DoAdvance = TRUE;
	}
	switch (CmdFirstChar) {
		case 'c': case 'd': case '!':
			GetXPos(&CurrCol);
			NewPosition(&StartPos);
			GetXPos(&CurrCol);
			if (SelectCount) InvalidateArea(&SelectStart, SelectCount, 1);
			SelectBytePos = CountBytes(&CurrPos);
			SelectStart   = CurrPos;
			SelectCount   = PositiveAmount;
			StartUndoSequence();
			if (CmdFirstChar != 'd') {
				InvalidateArea(&SelectStart, SelectCount, 0);
				CheckClipboard();
				UpdateWindow(hwndMain);
				if (CmdFirstChar == '!') CommandEdit('!');
			} else {
				InvalidateArea(&SelectStart, SelectCount, 3);
				DeleteSelected(1);
				if (DoAdvance) Position(1, '^', 0);
				else if (!HexEditMode && CharFlags(CharAt(&CurrPos)) & 1)
					Position(1, 'h', 0);
				if (UtfEncoding==16 && HexEditMode && CountBytes(&CurrPos)<=2)
					CheckUtf16Lsb();
				StatusCmd[0] = '\0';
				SetStatusCommand(NULL, FALSE);
			}
			SwitchCursor(SWC_TEXTCURSOR);
			break;

		case 'y':
			Yank(&StartPos, PositiveAmount, 0);
			if (DoAdvance) {
				CurrCol = YankCol;
				AdvanceToCurr(&CurrPos, /*CurrLine,*/ 0);
				NewPosition(&CurrPos);
			}
			SetStatusCommand(NULL, FALSE);
			break;

		case '<': case '>':
			StartUndoSequence();
			Shift(Amount, (WORD)(CmdFirstChar=='>' ? 8 : 0));
			break;
	}
	ShowEditCaret();
	return (TRUE);
}

static BOOL	IsSelected = TRUE;
UINT		Multiplier;

static BOOL DeleteKey(WORD Flags)
{	/*Flags: 0x200=delete character across line boundary (WrapPosFlag==TRUE)
	 */
	INT Pos, Disp;

	if (IsViewOnly()) return (FALSE);
	if (GetKeyState(VK_SHIFT) < 0) {
		if (GetKeyState(VK_CONTROL) < 0) return (FALSE);
		Pos = 'h'; Disp = 'X';
	} else if (GetKeyState(VK_CONTROL) < 0) {
		Pos = '$'; Disp = 'D';
	} else {
		Pos = ' '; Disp = 'x';
	}
	if (SelectCount) {
		if (Pos == 'h') ClipboardCut();
		else GotChar('\b', 0);
	} else if (Mode == InsertMode || Mode == ReplaceMode) {
		INT		 c;
		POSITION p;

		SelectStart = CurrPos;
		if (Pos == 'h') {
			c = GoBackAndChar(&SelectStart);
			if (CharFlags(c) & 1) return (FALSE);
		}
		SelectBytePos = CountBytes(&SelectStart);
		p = SelectStart;
		if (Pos == '$') {
			do; while (!(CharFlags(c = CharAndAdvance(&p)) & 1));
			if (c != C_EOF) GoBackAndChar(&p);
			SelectCount = CountBytes(&p) - SelectBytePos;
		} else if (!(CharFlags(c = CharAt(&p)) & 1) ||
				Flags & 0x200 || HexEditMode)
			if (c != C_EOF) {
				SelectCount += 1 + (c==C_CRLF && !HexEditMode);
				if (UtfEncoding)
					if (UtfEncoding == 16) SelectCount <<= 1;
					else {
						CharAndAdvance(&p);
						SelectCount = CountBytes(&p) - SelectBytePos;
					}
			}
		if (!SelectCount) return (FALSE);
		HexEditFirstNibble = -1;
		IsSelected = FALSE;
		GotChar('\b', 0);
		IsSelected = TRUE;
	} else if (Mode == CommandMode) {
		CmdFirstChar = 'd';
		DoCompoundCommand(Multiplier, Pos, Disp, (WORD)(Flags | (Number!=0)));
		Multiplier = 0;
		SetStatusCommand(NULL, FALSE);
	} else GotChar(Pos, 0);
	return (TRUE);
}

INT  RepeatCommand;

void GotKeydown(WPARAM VirtualKey)
{	UINT  Num;
	INT   Pos = 0;
	PWSTR NewStatusString = L"";
	UINT  Flags = 0;

	Num = InNumber ? Number : 0;
	switch (VirtualKey) {
		case '6':	/*check for <Ctrl+6>*/
		case 0xdc:	/*check for <Ctrl+^> (German keyboard layout)*/
			if (GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SHIFT) >= 0) {
				MSG m;
				if (!PeekMessage(&m, hwndMain, WM_CHAR, WM_CHAR, PM_NOREMOVE))
					GotChar(CTRL('^'), 0);
			}
			return;

		case VK_NUMLOCK:
			NewStatus(3, GetKeyState(VirtualKey)&1 ? NumLock  : L"", NS_NORMAL);
			return;
		case VK_CAPITAL:
			NewStatus(2, GetKeyState(VirtualKey)&1 ? CapsLock : L"", NS_NORMAL);
			return;

		case VK_TAB:
			if (GetKeyState(VK_CONTROL) < 0) GotChar('\t', 0);
			return;

		case VK_F2:
			if (GetKeyState(VK_SHIFT) >= 0) New(hwndMain, TRUE);
			else ArgsCmd();
			break;

		case VK_F3:
			if (InNumber) SetStatusCommand(NULL, FALSE);
			SearchAgain((WORD)(2 + (GetKeyState(VK_SHIFT) < 0)));
			break;

		case VK_F5:
			if (hwndMain) {
				HideEditCaret();
				InvalidateRect(hwndMain, NULL, TRUE);
				UpdateWindow(hwndMain);
				NewPosition(&CurrPos);
				ShowEditCaret();
			}
			break;

		case VK_F6:
			if (GetKeyState(VK_CONTROL) < 0) {
				/*switch between WinVi instances...*/
				InterCommActivate(GetKeyState(VK_SHIFT) < 0);
			}
			break;

		case VK_F12:
			if (GetKeyState(VK_SHIFT) >= 0) return;
			if (!IsToolButtonEnabled(IDB_SAVE)) {
				/*switch to command mode even if file is not written*/
				if (Mode!=CommandMode && !DefaultInsFlag) GotChar('\033', 1);
				return;
			}
			if (InNumber) SetStatusCommand(NULL, FALSE);
			Save(L"");
			break;

		case VK_LEFT:	Pos = GetKeyState(VK_CONTROL)<0 ?'B'	   :'h'; break;
		case VK_UP:		Pos = GetKeyState(VK_CONTROL)<0 ?CTRL('y') :'k'; break;
		case VK_RIGHT:	Pos = GetKeyState(VK_CONTROL)<0 ?'W'	   :'l'; break;
		case VK_DOWN:	Pos = GetKeyState(VK_CONTROL)<0 ?CTRL('e') :'j'; break;

		case VK_PRIOR:	Pos = CTRL('b');								 break;
		case VK_NEXT:	Pos = CTRL('f');								 break;
		case VK_HOME:	if (GetKeyState(VK_CONTROL) < 0) {
							Num = 1; Pos = 'G';
						} else Pos = '^';								 break;
		case VK_END:	if (GetKeyState(VK_CONTROL) < 0) {
							Num = 0; Pos = 'G';
							Flags |= 0x1000;
						} else Pos = '$';								 break;
		case VK_INSERT: if (!EscapeInput) {
							if (GetKeyState(VK_SHIFT) < 0) {
								if (GetKeyState(VK_CONTROL) >= 0)
									ClipboardPaste();
							} else if (GetKeyState(VK_CONTROL)<0)
								ClipboardCopy();
							else if (!ExecRunning()) {
								if (Mode == InsertMode) {
									Mode = CommandMode;
									GotChar('R', 0);
								} else {
									Mode = CommandMode;
									GotChar('i', 0);
								}
							}
						}
						break;
		case VK_DELETE: if (!EscapeInput)
							DeleteKey((WORD)(WrapPosFlag ? 0x200 : 0));
						break;
		default:		return;
	}
	HexEditFirstNibble = -1;
	if (Pos) {
		if (Mode != CommandMode && Mode != CmdGotFirstChar) {
			if (Number || Mode < InsertMode) {
				MessageBeep(MB_ICONEXCLAMATION);
				if (Mode != InsertMode) {
					Mode = CommandMode;
					SetStatusCommand(NULL, FALSE);
				}
				return;
			}
		}
		if (Mode == CmdGotFirstChar) {
			if (Num) {
				PWSTR p = StatusCmd;

				if (DblQuoteChar)
					p += _snwprintf(p, WSIZE(StatusCmd) - (p-StatusCmd),
									L"\"%c ", DblQuoteChar);
				if (Multiplier)
					p += _snwprintf(p, WSIZE(StatusCmd) - (p-StatusCmd),
									L"%u ",   Multiplier);
				_snwprintf(p, WSIZE(StatusCmd) - (p-StatusCmd),
						   L"%c %u", CmdFirstChar, Num);
				InNumber = TRUE;
				Number = Num;
			}
			DoCompoundCommand(Multiplier, Pos, Pos, TRUE);
			InNumber = FALSE;
			Multiplier = 0;
			if (Mode != CommandMode) return;
			NewStatusString = NULL;
		} else {
			if (InNumber) {
				if (Pos < ' ')
					 _snwprintf(StatusCmd, WSIZE(StatusCmd),
					 			L"%u ^%c", Number, Pos + ('A'-1));
				else _snwprintf(StatusCmd, WSIZE(StatusCmd),
								L"%u %c",  Number, Pos);
				SetStatusCommand(StatusCmd, TRUE);
				NewStatusString = NULL;
			}
			if (Mode != CommandMode) Flags |=  1;
			if (GetKeyState(VK_SHIFT) < 0 && VirtualKey != VK_F3) Flags |= 17;
			if (WrapPosFlag && Pos >= 'h') Flags |= 0x200;
			if (Pos == '^' && !Num) {
				Position(Num, Pos, 4 | (Mode==InsertMode));
				if (!RelativePosition) Pos = '0';
			}
			Position(Num, Pos, Flags);
		}
	}
	if (Mode == CommandMode) {
		*StatusCmd = '\0';
		SetStatusCommand(NewStatusString, FALSE);
		Number = 0;
	}
	InNumber = FALSE;
}

extern INT DefaultNewLine;

void PrepareInsert(BOOL JumpFwd, BOOL SearchEol, BOOL AddEol)
{	INT c;

	if (JumpFwd) {
		BOOL Redraw = TRUE;
		if (HexEditMode) Advance(&CurrPos, 1 + (UtfEncoding == 16));
		do {
			if (CharFlags(c = CharAt(&CurrPos)) & 1) break;
			if (HexEditMode && !SearchEol) break;
			Redraw = FALSE;
			CharAndAdvance(&CurrPos);
		} while (SearchEol);
		if (AddEol) InsertEol(Redraw, DefaultNewLine);
	} else if (SearchEol) {
		do {
			c = GoBackAndChar(&CurrPos);
		} while (!(CharFlags(c) & 1));
		if (c != C_EOF) c = AdvanceAndChar(&CurrPos);
		if (AddEol) {
			InsertEol(3, DefaultNewLine);
			GoBackAndChar(&CurrPos);
		} else {
			/*advance to start of non-space area*/
			while (CharFlags(c) & 16) c = AdvanceAndChar(&CurrPos);
		}
	}
}

BYTE CaseConvert[256] = {	/*initial values for Western ANSI characters*/
	  0,  1,  2,  3,  4,  5,  6,  7,   8,  9, 10, 11, 12, 13, 14, 15,
	 16, 17, 18, 19, 20, 21, 22, 23,  24, 25, 26, 27, 28, 29, 30, 31,
	 32, 33, 34, 35, 36, 37, 38, 39,  40, 41, 42, 43, 44, 45, 46, 47,
	 48, 49, 50, 51, 52, 53, 54, 55,  56, 57, 58, 59, 60, 61, 62, 63,
	 64,'a','b','c','d','e','f','g', 'h','i','j','k','l','m','n','o',
	'p','q','r','s','t','u','v','w', 'x','y','z', 91, 92, 93, 94, 95,
	 96,'A','B','C','D','E','F','G', 'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W', 'X','Y','Z',123,124,125,126,127,
	128,129,130,131,132,133,134,135, 136,137,154,139,156,141,142,143,
	144,145,146,147,148,149,150,151, 152,153,138,155,140,157,158,255,
	160,161,162,163,164,165,166,167, 168,169,170,171,172,173,174,175,
	176,177,178,179,180,181,182,183, 184,185,186,187,188,189,190,191,
	224,225,226,227,228,229,230,231, 232,233,234,235,236,237,238,239,
	240,241,242,243,244,245,246,215, 248,249,250,251,252,253,254,223,
	192,193,194,195,196,197,198,199, 200,201,202,203,204,205,206,207,
	208,209,210,211,212,213,214,247, 216,217,218,219,220,221,222,159
};

void ChangeCase(LONG Repeat)
{	INT  c, Oldc;
	BOOL Changed = FALSE, Advanced = FALSE;

	for (;;) {
		Oldc = c = CharAt(&CurrPos);
		if (HexEditMode ? c == C_EOF : CharFlags(c) & 1) {
			if (Advanced) GoBackAndChar(&CurrPos);
			break;
		}
		if (!Repeat--) break;
		if (c >= 0x100) c = Oldc;
		else if (CharSet == CS_OEM) {
			WCHAR wc;

			wc = c;
			UnicodeConvert(&wc, &wc, 1);
			if (wc <= 0xff) c = UnicodeToOemChar(CaseConvert[wc]);
			else c = Oldc;
		} else {
			c = CaseConvert[c];
			if (CharSet == CS_EBCDIC) c = c>=0x100 ? Oldc : MapAnsiToEbcdic[c];
		}
		if (c != Oldc) {
			INT OldSize = CharSize(Oldc);
			INT NewSize = CharSize(c);

			if (!CurrPos.p->PageBuf || CurrPos.i >= CurrPos.p->Fill)
				/*must not occur*/
				return;
			if (!Changed) {
				StartUndoSequence();
				Changed = TRUE;
			}
			EnterDeleteForUndo(&CurrPos, OldSize, 0);
			EnterInsertForUndo(&CurrPos, NewSize);
			CurrPos.p->PageBuf[CurrPos.i] = c;
			CurrPos.p->Flags &= ~ISSAFE;
			if (UtfEncoding) {
				if (UtfEncoding == 16) {
					if (UtfLsbFirst) {
						Advance(&CurrPos, 1);
						CurrPos.p->PageBuf[CurrPos.i] = c >> 8;
						CurrPos.p->Flags &= ~ISSAFE;
					} else {
						CurrPos.p->PageBuf[CurrPos.i] = c >> 8;
						Advance(&CurrPos, 1);
						CurrPos.p->PageBuf[CurrPos.i] = c;
						CurrPos.p->Flags &= ~ISSAFE;
					}
					GoBack(&CurrPos, 1);
				} else if (c >= 0x80) {
					if (c < 0x800) {
						CurrPos.p->PageBuf[CurrPos.i] = 0xc0 | (c >> 6);
						Advance(&CurrPos, 1);
						CurrPos.p->PageBuf[CurrPos.i] = 0x80 | (c & 0x3f);
						CurrPos.p->Flags &= ~ISSAFE;
						GoBack(&CurrPos, 1);
					} else if (c < 0x10000) {
						CurrPos.p->PageBuf[CurrPos.i] = 0xe0 | (c >> 12);
						Advance(&CurrPos, 1);
						CurrPos.p->PageBuf[CurrPos.i] = 0x80 | ((c>>6) & 0x3f);
						CurrPos.p->Flags &= ~ISSAFE;
						Advance(&CurrPos, 1);
						CurrPos.p->PageBuf[CurrPos.i] = 0x80 | (c & 0x3f);
						CurrPos.p->Flags &= ~ISSAFE;
						GoBack(&CurrPos, 2);
					} else {
						/*should be impossible, case unknown in this area*/
						assert(c < 0x10000);
					}
				}
			}
			InvalidateArea(&CurrPos, 1, 1);
		}
		CharAndAdvance(&CurrPos);
		Advanced = TRUE;
	}
	if (Changed) SetUnsafe();
	NewPosition(&CurrPos);
	ShowEditCaret();
	GetXPos(&CurrCol);
}

VOID SetNormalHelpContext(VOID)
{
	if (Mode == InsertMode) HelpContext = HelpInsertmode;
	else if (Mode == ReplaceMode) HelpContext = HelpReplacemode;
	else HelpContext = HexEditMode ? HelpEditHexmode: HelpEditTextmode;
}

BOOL SearchPending;

void ViCommandDone(void)
{
	Multiplier = Number = 0;
	DblQuoteChar = 0;
	Mode = CommandMode;
	SetNormalHelpContext();
	SearchPending = FALSE;
	if (*StatusCmd) {
		StatusCmd[0] = '\0';
		SetStatusCommand(NULL, FALSE);
	}
}

extern POSITION	MarkPos[27];

void CommandLineDone(BOOL Escaped, PWSTR Cmd)
{
	if (!Escaped && (*Cmd == '/' || *Cmd == '?')) {
		POSITION SearchFailOldPos;

		SearchFailOldPos = MarkPos[0];
		MarkPos[0] = CurrPos;
		if (Mode == CmdGotFirstChar && SearchPending) {
			if (DoCompoundCommand(Multiplier, 'n', 'n', FALSE) &&
					CmdFirstChar == '!')
				return;
		} else if (!Position(Number, 'n', Mode==InsertMode))
			MarkPos[0] = SearchFailOldPos;
	}
	if (Escaped || Mode == CommandMode) ViCommandDone();
}

void ReplayRedo(LONG Fill)
{	INT    i = 0;
	LPREDO r = &RedoChain;

	while (Fill--) {
		GotChar(r->Buffer[i], 0);
		if (++i >= WSIZE(r->Buffer)) {
			i = 0;
			r = r->Next;
			if (r == NULL) break;
		}
	}
}

void ShowChar(INT Character, BOOL SeparateWithBlank)
{	PWSTR p = StatusCmd + wcslen(StatusCmd);

	if (Character != '\033') {
		if (SeparateWithBlank) *p++ = ' ';
		if (Character < ' ') {
			if (!SeparateWithBlank) *p++ = ' ';
			*p++ = '^';
			*p++ = Character + ('A'-1);
		} else *p++ = Character;
		*p = '\0';
	}
}

void Unselect(void)
{
	InvalidateArea(&SelectStart, SelectCount, 1);
	SelectCount = 0;
	CheckClipboard();
}

void NewDigit(INT Digit, BOOL Display)
{	PWSTR p = StatusCmd;
	ULONG NewNumber;

	if (Number > (UINT)-1 / 10U
			|| (NewNumber = 10U*Number+Digit-'0',
			    (UINT)NewNumber < Number || (LONG)NewNumber < 0)) {
		Number = (UINT)-1 & 0x7fffffff;
		MessageBeep(MB_ICONEXCLAMATION);
	} else Number = (UINT)NewNumber;
	if (!Display) return;
	if (Multiplier)
		p += _snwprintf(p, WSIZE(StatusCmd)-(p-StatusCmd), L"%u ", Multiplier);
	if (DblQuoteChar)
		p += _snwprintf(p, WSIZE(StatusCmd)-(p-StatusCmd), L"\"%c ",
						DblQuoteChar);
	_snwprintf(p, WSIZE(StatusCmd)-(p-StatusCmd), L"%u", Number);
	SetStatusCommand(StatusCmd, TRUE);
}

VOID SelectAll(VOID)
{	LPPAGE p;

	SelectBytePos = SelectCount = 0;
	SelectStart.i = 0;
	SelectStart.p = FirstPage;
	for (p=FirstPage; p; p=p->Next)
		SelectCount += p->Fill;
	if (UtfEncoding && CharAt(&SelectStart) == C_BOM) {
		INT BomLen = 2 + (UtfEncoding == 8);

		SelectStart.i += BomLen;
		SelectBytePos += BomLen;
		SelectCount   -= BomLen;
		ByteAt(&SelectStart);
	}
	InvalidateText();
	CheckClipboard();
}

BOOL ShowParenMatch;

void GotChar(INT Character, WORD Flags)
	/*Flags: 1=no showmatch*/
{	INT				ShowNewStatusCmd = 0, c;
					/*ShowNewStatusCmd: 1=show character, 2=show with DblQuote*/
	BOOL			CommandDone = TRUE;
	static POSITION ShiftOutPos;
	static INT		ShiftOutPrefix;
					/*previously inserted character for test of [0^]<Ctrl+D>*/

	if (Character == CTRL('o') || Character == CTRL('s')) {
		static WCHAR SavText[40];
		BOOL		 Saved;

		Saved = (StatusFields[1].Style == NS_NORMAL
				 && StatusFields[1].Text != NULL);
		if (Saved) {
			wcsncpy(SavText, StatusFields[1].Text, 39);
			SavText[39] = '\0';
		}
		if (Character == CTRL('o')) {
			wcscpy(StatusCmd, L"^O");
			SetStatusCommand(StatusCmd, TRUE);
			Open(L"", 0);
			if (Mode != CommandMode) {
				if (Saved) {
					wcscpy(StatusCmd, SavText);
					SetStatusCommand(StatusCmd, TRUE);
				} else SetStatusCommand(NULL, FALSE);
				return;
			}
		} else {
			if (Mode != CommandMode && !DefaultInsFlag && !ExecRunning())
				GotChar('\033', 1);
			wcscpy(StatusCmd, L"^S");
			SetStatusCommand(StatusCmd, TRUE);
			if (Unsafe) Save(L"");
			if (Mode != CommandMode) {
				SetStatusCommand(NULL, FALSE);
				return;
			}
		}
	} else switch (Mode) {
		case CommandMode:
			if (InNumber && (Character<'0' || Character>'9') && !Repeating) {
				PWSTR p = StatusCmd;

				if (Multiplier)   p += _snwprintf(p, WSIZE(StatusCmd)
													 - (p-StatusCmd),
												  L"%u ", Multiplier);
				if (DblQuoteChar) p += _snwprintf(p, WSIZE(StatusCmd)
													 - (p-StatusCmd),
												  L"\"%c ", DblQuoteChar);
				_snwprintf(p, WSIZE(StatusCmd) - (p-StatusCmd), L"%u", Number);
				ShowChar(Character, TRUE);
				SetStatusCommand(StatusCmd, TRUE);
			}
			switch (Character) {
				case '1': case '2': case '3':
				case '4': case '5': case '6':
				case '7': case '8': case '9':
					InNumber = TRUE;
					NewDigit(Character, TRUE);
					return;
				case '0':
					if (InNumber) {
						NewDigit(Character, TRUE);
						return;
					}
					/*FALLTHROUGH*/
				case 'n': case 'N':					/*search reg. exp. again*/
				case '%':							/*find matching paren	*/
				case CTRL('d'): case CTRL('u'):	/*half screen Down	  / Up	*/
					if (!InNumber && Character != '0') {
						/*slow operation, show immediately in status...*/
						if (Character > ' ')
							 _snwprintf(StatusCmd, WSIZE(StatusCmd),
							 			L"%c", Character);
						else _snwprintf(StatusCmd, WSIZE(StatusCmd),
										L"^%c", Character + ('A'-1));
						SetStatusCommand(StatusCmd, TRUE);
					}
					/*FALLTHROUGH*/
				case '\b':
					if (Character=='\b' && SelectCount) {
						DeleteSelected(17);
						break;
					}
					/*FALLTHROUGH*/
				case CTRL('e'): case CTRL('y'):
								/*scroll without relative cursor movement	*/
				case CTRL('f'): case CTRL('b'):	/*full screen Forward / Back*/
				case 'l': case 'h': case 'k': case 'j':
				case 'w': case 'W': case 'b': case 'B': case 'e': case 'E':
				case '^': case '$': case '|':
				case '+': case '-': case 'G':
				case 'H': case 'M': case 'L':
							   /*Upper, Middle, Lower line of visible window*/
				case ';': case ',':					/*search character again*/
				case '(': case ')':					/**/
				case '{': case '}':					/**/
				case CTRL('p'): case CTRL('n'): case '\n': case '\r': case ' ':
				case '*': case '#':		/*new 23-Mar-98*/
					Position(Number, Character, 0);
					break;

				case '/': case '?':
				case ':':
					CommandEdit(Character);
					InNumber = CommandDone = FALSE;
					if (Character == ':') Number = Multiplier = 0;
					else Multiplier = Number;
					++ShowNewStatusCmd;
					break;

				case 'X':	/*delete previous character in current line*/
				case 'x':	/*delete current character*/
					CmdFirstChar = 'd';
					ShowNewStatusCmd = 2;
					DoCompoundCommand(Multiplier, Character=='x' ? ' ' : 'h',
									  Character, FALSE);
					break;

				case 'D':	/*delete to end of line*/
					CmdFirstChar = 'd';
					ShowNewStatusCmd = 2;
					DoCompoundCommand(Multiplier, '$',
									  Character, (WORD)!InNumber);
					break;

				case 'J':	/*Join*/
					RedoCount  = Number;
					RedoCmd[0] = Character;
					RedoCmd[1] = '\0';
					RedoFill   = 0;
					if (!Number) ++Number;
					else if (Number > 1) --Number;	/*Join 2 lines = 1x join*/
					StartUndoSequence();
					Interrupted = FALSE;
					for (;;) {
						Join();
						if (!--Number) break;
						if (!Disabled) Disable(Disabled = TRUE);
						MessageLoop(FALSE);
						if (Interrupted) {
							static WCHAR BufferW[80];

							LOADSTRINGW(hInst, 218, BufferW, WSIZE(BufferW));
							NewStatus(0, BufferW, NS_ERROR);
							break;
						}
					}
					if (Disabled) Enable();
					FindValidPosition(&CurrPos, 0);
					ShowEditCaret();
					break;

				case '~':	/*convert uppercase/lowercase*/
					RedoCount  = Number;
					RedoCmd[0] = Character;
					RedoCmd[1] = '\0';
					RedoFill   = 0;
					if (!Number) ++Number;
					ChangeCase(Number);
					NewPosition(&CurrPos);
					ShowEditCaret();
					break;

				case '"':	/*specifify yank/delete buffer*/
					++ShowNewStatusCmd;
					Mode = GetDblQuoteChar;
					CommandDone = FALSE;
					Multiplier = Number;
					Number = 0;
					HelpContext = HelpEditDoublequote;
					break;

				case 'p': case 'P':	/*put yank/delete buffer*/
					if (DblQuoteChar) {
						if (!InNumber) {
							wcscat(StatusCmd, L" x");
							StatusCmd[wcslen(StatusCmd)-1] = Character;
							SetStatusCommand(StatusCmd, TRUE);
						}
						RedoCmd[0] = '"';
						RedoCmd[1] = DblQuoteChar;
						RedoCmd[2] = Character;
						RedoCmd[3] = '\0';
						ShowNewStatusCmd = 2;
						InNumber = FALSE;	/*for status update*/
					} else {
						RedoCmd[0] = Character;
						RedoCmd[1] = '\0';
					}
					if (!Repeating) {
						if (Number || Multiplier) {
							if (!Multiplier) Multiplier  = Number;
							else if (Number) Multiplier *= Number;
						}
						RedoCount = Multiplier;
						RedoFill  = 0;
						StartUndoSequence();
					}
					if (!Put(Multiplier, Character=='p'))
						MessageBeep(MB_ICONEXCLAMATION);
					break;

				case '.':
					if (*RedoCmd == 'u') {
						SetStatusCommand(L"u", TRUE);
						Undo(2);
						SetStatusCommand(L"u", FALSE);
					} else if (*RedoCmd) {
						PSTR p;
						LONG Fill = RedoFill;

						if (Number) RedoCount = Number;
						else if (RedoCount) {
							static CHAR Buf[12];

							wsprintf(Buf, "%u", RedoCount);
							p = Buf;
							while (*p != '\0') GotChar(*p++, 0);
						}
						p = RedoCmd;
						do GotChar(*p++, 0); while (*p != '\0');
						if (Mode != CommandMode) ReplayRedo(Fill);
					} else {
						if (!InNumber) SetStatusCommand(L".", FALSE);
						Error(235);
					}
					break;

				case '&':
					CommandExec(L":s");
					break;

				case 'u':	/*undo*/
					StatusCmd[0] = RedoCmd[0] = 'u';
					StatusCmd[1] = RedoCmd[1] = '\0';
					SetStatusCommand(StatusCmd, TRUE);
					Undo(0);
					break;

				case 'U':	/*undo current line*/
					StatusCmd[0] = 'U';
					StatusCmd[1] = '\0';
					SetStatusCommand(StatusCmd, TRUE);
					Undo(4);
					break;

				case CTRL('z'):
					StatusCmd[0] = '^';
					StatusCmd[1] = 'Z';
					StatusCmd[2] = '\0';
					RedoCmd[0] = Character;
					RedoCmd[1] = '\0';
					SetStatusCommand(StatusCmd, TRUE);
					Undo(1);	/*same as <Alt+Bksp>*/
					break;

				case CTRL('l'):
					if (hwndMain) {
						HideEditCaret();
						InvalidateRect(hwndMain, NULL, TRUE);
						UpdateWindow(hwndMain);
						NewPosition(&CurrPos);
						ShowEditCaret();
					}
					break;

				case CTRL('g'):
					ShowFileName();
					break;

				case CTRL('^'):
					Number = 0;
					StatusCmd[0] = '^';
					StatusCmd[1] = '^';
					StatusCmd[2] = '\0';
					SetStatusCommand(StatusCmd, TRUE);
					CommandExec(L":e#");
					break;

				case CTRL(']'):
					Number = 0;
					StatusCmd[0] = '^';
					StatusCmd[1] = ']';
					StatusCmd[2] = '\0';
					SetStatusCommand(StatusCmd, TRUE);
					TagCurrentPosition();
					break;

				case CTRL('t'):
					Number = 0;
					StatusCmd[0] = '^';
					StatusCmd[1] = 'T';
					StatusCmd[2] = '\0';
					SetStatusCommand(StatusCmd, TRUE);
					PopTag(TRUE);
					break;

				case 'Z':	/*ZZ=exit*/
					--ShowNewStatusCmd;
					Number = 0;
					/*FALLTHROUGH*/
				case 'z':	/*z[.-<Enter>] for rearranging cursor position*/
					DblQuoteChar = 0;	/*ignored, don't show*/
					/*FALLTHROUGH*/
				case 'c':	/*change*/
				case 'd':	/*delete*/
				case 'y':	/*yank*/
					if (LCASE(Character) != 'z' && IsViewOnly()) break;
					++ShowNewStatusCmd;
					/*FALLTHROUGH*/
				case '<':	/*shift out*/
				case '>':	/*shift in*/
				case '!':	/*pipe through system command*/
					if (LCASE(Character) != 'z' && IsViewOnly()) break;
					if (Character < 'c') DblQuoteChar = 0;
					Mode = CmdGotFirstChar;
					CmdFirstChar = Character;
					++ShowNewStatusCmd;
					if (Number) {
						if (Multiplier) Multiplier *= Number;
						else Multiplier = Number;
					}
					Number = 0;
					InNumber = CommandDone = FALSE;
					break;

				case 'S':
					HideEditCaret();
					Position(1, '^', 1);
					/*FALLTHROUGH*/
				case 's':
				case 'C':
					if (!Number && !DblQuoteChar) StatusCmd[0] = '\0';
					ShowNewStatusCmd = 2;
					CmdFirstChar = 'c';
					DoCompoundCommand(Multiplier, Character=='s' ? ' ' : '$',
									  Character, (WORD)!InNumber);
					CommandDone = FALSE;
					HelpContext = HelpEditC;
					break;

				case 'Y':
					if (!InNumber && !DblQuoteChar) StatusCmd[0] = '\0';
					ShowNewStatusCmd = 2;
					DoCompoundCommand(Multiplier, CmdFirstChar = 'y',
									  Character, FALSE);
					HelpContext = HelpEditY;
					break;

				case 'A': case 'a':
				case 'I': case 'i':
				case 'O': case 'o':
					if ((Character=='o' || Character=='O') && IsViewOnly())
						break;
					if (!Repeating) {
						++ShowNewStatusCmd;
						if (Number == 1) Number = 0;
						RepeatCommand = Character=='O' || Character=='o'
										? 'o' : 'i';
						RedoCount  = Number;
						RedoCmd[0] = Character;
						RedoCmd[1] = '\0';
						RedoFill   = 0;
						StartUndoSequence();
					}
					DblQuoteChar = 0;
					Mode = InsertMode;
					PrepareInsert(LCASE(Character)=='a' || Character=='o',
								  Character!='a' && Character!='i',
								  LCASE(Character)=='o');
					NewPosition(&CurrPos);
					SwitchCursor(SWC_TEXTCURSOR);
					ShowEditCaret();
					GetXPos(&CurrCol);
					CommandDone = FALSE;
					SetNormalHelpContext();
					break;

				case 'R': case 'r':	/*replace until <escape> / single char*/
					if (IsViewOnly()) break;
					ReplaceSingle = Character == 'r';
					if (!Repeating) {
						++ShowNewStatusCmd;
						if (Number == 1) Number = 0;
						RedoCount  = Number;
						RedoCmd[0] = RepeatCommand = Character;
						RedoCmd[1] = '\0';
						RedoFill   = 0;
						StartUndoSequence();
					}
					Mode = ReplaceMode;
					NewPosition(&CurrPos);
					ShowEditCaret();
					CommandDone = FALSE;
					SetNormalHelpContext();
					break;

				case '\'': case '`': case 'm':
					QuoteCommandChar = Character;
					++ShowNewStatusCmd;
					ModeAfterTfmq = Mode;
					DblQuoteChar = 0;
					Mode = PositionQuote;
					CommandDone = FALSE;
					HelpContext = HelpEditMark;
					break;

				case 't': case 'T': case 'f': case 'F':
					TfCommandChar = Character;
					ModeAfterTfmq = Mode;
					DblQuoteChar = 0;
					Mode = PositionTf;
					ShowNewStatusCmd = 2;
					CommandDone = FALSE;
					HelpContext = HelpEditFT;
					break;

				case '[': case ']':
					BracketCommandChar = Character;
					ModeAfterTfmq = Mode;
					DblQuoteChar = 0;
					Mode = PositionBracket;
					ShowNewStatusCmd = 2;
					CommandDone = FALSE;
					break;

				case CTRL('c'):  ClipboardCopy();   break;
				case CTRL('v'):  ClipboardPaste();  break;
				case CTRL('x'):  ClipboardCut();    break;

				case CTRL('a'):	 SelectAll();		break;

				case '\033':
					Character = ' ';	/*empty status command field*/
					if (SelectCount) Unselect();
					break;

				case 'q':
					{	extern CHAR  QueryString[];
						extern DWORD QueryTime;

						if (QueryString[0] && GetTickCount()-QueryTime < 5000
								&& DebugMessages())
							MessageBox(hwndMain, QueryString, "WinVi - Query",
									   MB_OK | MB_ICONINFORMATION);
						else MessageBeep(MB_ICONEXCLAMATION);
					}
					break;

				case '\t':
					if (HexEditMode && GetKeyState(VK_CONTROL) < 0) {
						HexEditTextSide ^= TRUE;
						HideEditCaret();
						NewPosition(&CurrPos);
						ShowEditCaret();
						break;
					}
					/*FALLTHROUGH*/
				default:
					MessageBeep(MB_ICONEXCLAMATION);
					break;
			}
			if (!InNumber && !Repeating) {
				if (ShowNewStatusCmd) {
					PWSTR p = StatusCmd;

					if (Multiplier)
						p += _snwprintf(p, WSIZE(StatusCmd),
										L"%u ", Multiplier);
					if (ShowNewStatusCmd == 2 && DblQuoteChar) {
						*p++ = '"';
						*p++ = DblQuoteChar;
						*p++ = ' ';
					}
					*p++ = Character;
					*p = '\0';
					SetStatusCommand(StatusCmd, !CommandDone);
				} else if (Character != '.') {
					/*show character grayed...*/
					WCHAR *p = StatusCmd;

					if (Character < ' ') {
						*p++ = '^';
						*p   = Character + ('A'-1);
					} else *p = Character;
					*++p = '\0';
					SetStatusCommand(StatusCmd, FALSE);
				}
			}
			break;

		case CmdGotFirstChar:
			{	static INT InTfq;
				BOOL	   Ok = FALSE;
				if (!InTfq && (INT)Character >= ('1'-InNumber)
						   && (INT)Character <= '9') {
					if (!InNumber) {
						InNumber = TRUE;
						Number = 0;
					}
					NewDigit(Character, FALSE);
					if (CmdFirstChar != 'Z') {
						PWSTR p = StatusCmd;

						if (DblQuoteChar)
							p += _snwprintf(p, WSIZE(StatusCmd),
											L"\"%c ", DblQuoteChar);
						if (Multiplier)
							p += _snwprintf(p, WSIZE(StatusCmd),
											L"%u ",   Multiplier);
						_snwprintf(p, WSIZE(StatusCmd),
								   L"%c %u", CmdFirstChar, Number);
						SetStatusCommand(StatusCmd, TRUE);
					}
					return;
				}
				if (CmdFirstChar == 'z') {
					INT Shift;
					ShowChar(Character, FALSE);
					SetStatusCommand(StatusCmd, TRUE);
					Mode = CommandMode;
					switch (Character) {
						case '\r':	 Shift = 15;	break;
						case '.':	 Shift = 1;	break;
						case '-':	 Shift = 0;	break;
						default:	 MessageBeep(MB_ICONEXCLAMATION);
									 /*FALLTHROUGH*/
						case '\033': Shift = -1;	break;
					}
					if (Shift >= 0) {
						INT Line;
						if (Number || Multiplier) {
							if (Multiplier) {
								if (Number) Number *= Multiplier;
								else Number = Multiplier;
							}
							LinesVar   = Number;
							ColumnsVar = 0;
							ResizeWithLinesAndColumns();
						}
						Line = (CrsrLines-1) >> Shift;
						if (Line != CurrLine) {
							if (Line > CurrLine)
								 Position(Line-CurrLine, CTRL('y'), 0x102);
							else Position(CurrLine-Line, CTRL('e'), 0x102);
						}
					}
					ShowEditCaret();
				} else if (InTfq) {
					if (Character != '\033') {
						if (InTfq != ';') QuoteChar = Character;
						else TfSearchChar = Character;
						Ok = DoCompoundCommand(Multiplier,InTfq,Character,TRUE);
					} else Mode = CommandMode;
					Multiplier = 0;
					InTfq = 0;
				} else switch (CmdFirstChar!='Z' ? Character : 0) {
					case '/': case '?':
						ShowChar(Character, TRUE);
						SetStatusCommand(StatusCmd, TRUE);
						InNumber = FALSE;
						SearchPending = TRUE;
						CommandEdit(Character);
						return;

					case '\'': case '`':
						QuoteCommandChar = Character;
						ShowChar(Character, TRUE);
						SetStatusCommand(StatusCmd, TRUE);
						InNumber = FALSE;
						InTfq = Character;
						return;

					case 'F': case 'T':
					case 'f': case 't':
						TfCommandChar = Character;
						ShowChar(Character, TRUE);
						SetStatusCommand(StatusCmd, TRUE);
						InNumber = FALSE;
						InTfq = ';';
						return;

					case '[': case ']':
						BracketCommandChar = Character;
						ShowChar(Character, TRUE);
						SetStatusCommand(StatusCmd, TRUE);
						InNumber = FALSE;
						InTfq = Character;
						return;

					default:
						if (Character != '\033') {
							Ok = DoCompoundCommand(Multiplier, Character,
												   Character, TRUE);
						} else Mode = CommandMode;
						Multiplier = 0;
				}
				if ((CmdFirstChar=='!' && Ok) || Mode!=CommandMode)
					CommandDone = FALSE;
			}
			break;

		case PositionQuote:
			if ((Mode = ModeAfterTfmq) != CommandMode) CommandDone = FALSE;
			if (Character != '\033') {
				ShowChar(Character, FALSE);
				QuoteChar = Character;
				SetStatusCommand(StatusCmd, QuoteCommandChar != 'm');
				if (QuoteCommandChar == 'm') {
					if (isalpha(Character)) MarkPos[Character & 31] = CurrPos;
					else MessageBeep(MB_ICONEXCLAMATION);
				} else Position(Number, QuoteCommandChar, 0);
			}
			break;

		case PositionTf:
			if ((Mode = ModeAfterTfmq) != CommandMode) CommandDone = FALSE;
			TfSearchChar = Character;
			ShowChar(Character, TRUE);
			SetStatusCommand(StatusCmd, TRUE);
			Position(Number, TfCommandChar, 0);
			break;

		case PositionBracket:
			if ((Mode = ModeAfterTfmq) != CommandMode) CommandDone = FALSE;
			if (Character != '\033') {
				ShowChar(Character, TRUE);
				SetStatusCommand(StatusCmd, TRUE);
				if ((INT)Character == BracketCommandChar) {
					Position(Number, BracketCommandChar, 0);
				} else MessageBeep(MB_ICONEXCLAMATION);
			}
			break;

		case GetDblQuoteChar:
			ShowChar(Character, FALSE);
			if (isalnum(Character) && Character > '0') {
				DblQuoteChar = Character;
				SetStatusCommand(StatusCmd, TRUE);
				InNumber = FALSE;
				CommandDone = FALSE;
			} else {
				DblQuoteChar = 0;
				SetStatusCommand(StatusCmd, FALSE);
				if (Character != '\033') MessageBeep(MB_ICONEXCLAMATION);
			}
			Mode = CommandMode;
			break;

		case ReplaceMode:
		case InsertMode:
			CommandDone = FALSE;
			if (Character == '\t' && GetKeyState(VK_CONTROL) < 0
								  && GetKeyState(VK_TAB) < 0) c = -2;
			else {
				c = Character;
				switch (c) {
					case CTRL('a'):  SelectAll();	    return;
					case CTRL('c'):  ClipboardCopy();   return;
					case CTRL('v'):  ClipboardPaste();  break;
					case CTRL('x'):  ClipboardCut();    break;
					case CTRL('z'):  Undo(1);			return;
				}
				if (SelectCount) {
					if (Character != '\033') {
						/*delete selected area...*/
						DeleteSelected(IsSelected ? 17 : 1);
						if (Character == '\b') {
							FindValidPosition(&CurrPos,
											  (WORD)(HexEditMode ? 3 : 1));
							GetXPos(&CurrCol);
							ShowEditCaret();
							ShiftOutPrefix = 0;
							if (UtfEncoding == 16 && CountBytes(&CurrPos) == 0)
								CheckUtf16Lsb();
							break;
						}
						GetXPos(&CurrCol);
					} else {
						Unselect();
						UpdateWindow(hwndMain);
					}
				}
			}
			if (HexEditMode && !HexEditTextSide) {
				switch (c) {
					case '0': case '1': case '2': case '3':
							  case '4': case '5': case '6':
							  case '7': case '8': case '9':
					case 'A': case 'B': case 'C': case 'D':
							  case 'E': case 'F':
					case 'a': case 'b': case 'c': case 'd':
							  case 'e': case 'f':
						c = (Character & 15) + (Character > '9' ? 9 : 0);
						if (HexEditFirstNibble == -1) {
							HexEditFirstNibble = c <<= 4;
							if (Mode == ReplaceMode) {
								INT c2;
								if ((c2 = ByteAt(&CurrPos)) != C_EOF) {
									if (c2 == C_CRLF) c2 = '\r';
									c += c2 & 15;
									if (UtfEncoding == 16) {
										if (!(CountBytes(&CurrPos) & 1)) {
											EnterDeleteForUndo(&CurrPos, 2, 0);
											EnterInsertForUndo(&CurrPos, 2);
										}
									} else {
										EnterDeleteForUndo(&CurrPos, 1, 0);
										EnterInsertForUndo(&CurrPos, 1);
									}
									SetUnsafe();
									assert(CurrPos.p->PageBuf);
									CurrPos.p->PageBuf[CurrPos.i] = c;
									UnsafePage(&CurrPos);
									InvalidateArea(&CurrPos, 1, 9);
									c = -1;
								}
							}
							if (c != -1) {
								SuppressMap = TRUE;
								if (UtfEncoding) {
									if (UtfEncoding == 8) {
										UtfEncoding = 0;
										InsertChar(c);
										UtfEncoding = 8;
									} else {
										if (!(CountBytes(&CurrPos) & 1)) {
											if (!UtfLsbFirst) c <<= 8;
											InsertChar(c);
											GoBack(&CurrPos, 1);
										} else {
											CurrPos.p->PageBuf[CurrPos.i] = c;
											InvalidateArea(&CurrPos, 1, 9);
											Advance(&CurrPos, 1);
										}
									}
								} else InsertChar(c);
								SuppressMap = FALSE;
								GoBack(&CurrPos, 1);
							}
							HideEditCaret();
						} else {
							ByteAt(&CurrPos);
							assert(CurrPos.p->PageBuf);
							CurrPos.p->PageBuf[CurrPos.i] =
								HexEditFirstNibble + c;
							UnsafePage(&CurrPos);
							if (UtfEncoding == 16 && CountBytes(&CurrPos) == 1)
								CheckUtf16Lsb();
							InvalidateArea(&CurrPos, 1, 9);
							Advance(&CurrPos, 1);
							HexEditFirstNibble = -1;
						}
						NewPosition(&CurrPos);
						ShowEditCaret();
						c = -1;
						break;

					case '\b':
					case '\033':
						if (HexEditFirstNibble != -1) {
							Advance(&CurrPos, 1 - (CountBytes(&CurrPos) & 1)
											    + (UtfEncoding == 16));
							HexEditFirstNibble = -1;
						} else if (UtfEncoding==16 && CountBytes(&CurrPos) & 1)
							Advance(&CurrPos, 1);
						break;

					case CTRL('v'):
					case CTRL('x'):
					case -2:
						break;

					default:
						MessageBeep(MB_ICONEXCLAMATION);
						return;
				}
			}
			switch (c) {
				case CTRL('v'):
				case CTRL('x'):
				case -1:
					break;

				case '\033':
					ReplaceSingle = FALSE;
					if (ExecRunning()) break;
					Mode = CommandMode;
					if (Number && !Repeating) {
						INT Count = Number;

						Number = 0;
						Repeating = TRUE;
						while (--Count) {
							LONG Fill = RedoFill;

							GotChar(RepeatCommand, 0);
							ReplayRedo(Fill);
							Mode = CommandMode;
						}
						Repeating = FALSE;
					}
					if (!Repeating) {
						if (Indenting && IndentChars &&
								CharFlags(CharAt(&CurrPos)) & 1) {
							GoBack(&CurrPos, IndentChars);
							Reserve(&CurrPos, -IndentChars, 0);
							CharAt(&LineInfo[CurrLine].Pos);	/*normalize*/
							InvalidateArea(&CurrPos, 1, 9);
						} else if (HexEditMode) {
							GoBack(&CurrPos, 1 + (UtfEncoding == 16));
						} else if ((CharFlags(GoBackAndChar(&CurrPos))&33) == 1)
							CharAndAdvance(&CurrPos);
						FindValidPosition(&CurrPos, 4);
					}
					Indenting = FALSE;
					NewPosition(&CurrPos);
					SwitchCursor(SWC_TEXTCURSOR);
					ShowEditCaret();
					if (!(Flags & 1))
						switch (CharAt(&CurrPos)) {
							case '(': case ')':
							case '[': case ']':
							case '{': case '}':
								ShowMatch(0);
						}
					GetXPos(&CurrCol);
					CommandDone = TRUE;
					ShiftOutPrefix = 0;
					break;

				case '\r':
					if (GetKeyState('M') >= 0 /*'M' key *not* pushed*/
							|| GetKeyState(VK_CONTROL) >= 0) {
						/*translate <enter> key to default newline...*/
						if (Mode==ReplaceMode
								&& !(CharFlags(CharAt(&CurrPos)) & 1))
							DeleteForward(1);
						InsertEol(5, DefaultNewLine);
						break;
					}
					/*FALLTHROUGH*/
				case '\0':
				case CTRL('^'):
				case '\n':
					InsertEol(5, Character);
					break;

				case CTRL('t'):
					HideEditCaret();
					Shift(0, 15);
					ShowEditCaret();
					break;

				case CTRL('d'):
					HideEditCaret();
					if (ShiftOutPrefix=='0' || ShiftOutPrefix=='^') {
						if (!ComparePos(&ShiftOutPos, &CurrPos)) {
							DeleteChar();
							Shift(0, 7+16);
						}
					}
					Shift(0, 7);
					ShowEditCaret();
					break;

				case '\b':
					DeleteChar();
					break;

				case CTRL('w'):
				case 0177:
					{	POSITION pos;
						INT		 c, c2;

						pos = CurrPos;
						while (CharFlags(c = GoBackAndChar(&pos)) & 16)
							DeleteChar();
						c2 = CharFlags(c);
						while (!(CharFlags(c) & 49)) {
							if ((CharFlags(c) ^ c2) & 8) break;
							DeleteChar();
							c = GoBackAndChar(&pos);
						}
					}
					break;

				/*for vi % positioning: { ( [*/
				case ')': case ']': case '}':
					InsertChar(Character);
					if (!(Flags & 1)) ShowMatch(1);
					break;

				case -2:
					if (HexEditMode) {
						HexEditFirstNibble = -1;
						HexEditTextSide ^= TRUE;
						if (UtfEncoding == 16 && CountBytes(&CurrPos) & 1)
							--CurrPos.i;
						HideEditCaret();
						NewPosition(&CurrPos);
						ShowEditCaret();
						break;
					}
					MessageBeep(MB_ICONEXCLAMATION);
					return;

				default:
					/*normal character to insert...*/
					InsertChar(Character);
			}
			ShiftOutPrefix = c;
			ShiftOutPos = CurrPos;
			if (!Repeating) {
				if (RedoFill >= (LONG)WSIZE(RedoChain.Buffer)) {
					do {	/*no loop, for break only*/
						LPREDO r = &RedoChain;
						LONG   n = RedoFill;

						while (r->Next != NULL) {
							r = r->Next;
							if ((n -= (LONG)WSIZE(r->Buffer))
									< (LONG)WSIZE(r->Buffer))
								break;
						}
						if (n >= (LONG)WSIZE(r->Buffer)) {
							HGLOBAL h = GlobalAlloc(GPTR, sizeof(REDO));

							if ((n -= (LONG)WSIZE(r->Buffer))
								   >= (LONG)WSIZE(r->Buffer))
								break;
							if (!h || (r->Next=(LPREDO)GlobalLock(h)) == NULL)
								break;
							r = r->Next;
						}
						r->Buffer[n] = Character;
						++RedoFill;
					} while (FALSE);
				} else RedoChain.Buffer[RedoFill++] = Character;
			}
			if (Mode==ReplaceMode && ReplaceSingle && HexEditFirstNibble==-1) {
				GotChar('\033', 0);
				if (!Repeating) --RedoFill;
			}
			break;
	}
	InNumber = FALSE;
	if (CommandDone) ViCommandDone();
}

void Scroll(INT ScrollOp, INT CurrPosition)
{	POSITION	OldPos = CurrPos;
	extern BOOL	Exiting;
	extern HWND	hwndScroll;
	INT			Flags = (Mode==InsertMode) | 0x402;

	HexEditFirstNibble = -1;
	if (Exiting) {
		assert(!Exiting);
		return;
	}
	if (Mode==InsertMode && GetKeyState(VK_SHIFT) < 0) Flags |= 16;
	switch (ScrollOp) {
		case SB_TOP:			Position(1, 'G', 0);
								break;
		case SB_BOTTOM:			Position(0, 'G', 0);
								break;
		case SB_LINEUP:			Position(1, CTRL('y'), Flags);
								if (CurrPos.i==OldPos.i && CurrPos.p==OldPos.p)
									 Position(1, 'k', Flags);
								ShowEditCaret();
								break;
		case SB_LINEDOWN:		if (!(LineInfo[CrsrLines].Flags & LIF_EOF))
									 Position(1, CTRL('e'), Flags);
								if (CurrPos.i==OldPos.i && CurrPos.p==OldPos.p)
									 Position(1, 'j', Flags);
								ShowEditCaret();
								break;
		case SB_PAGEUP:			Position(1, CTRL('b'), Flags & ~2);
								break;
		case SB_PAGEDOWN:		Position(1, CTRL('f'), Flags & ~2);
								break;
		case SB_THUMBTRACK:		/*dragging the thumb*/
								ScrollJump(CurrPosition);
								break;
	/*	case SB_THUMBPOSITION:	--final position of drag--	*/
								/*has already been handled by SB_THUMBTRACK*/
	}
	if (GetFocus() == hwndScroll) SetFocus(hwndMain);
}

VOID ShowMatch(WORD Flags)
{	/*Flags: 1=handle parentheses at previous position (input mode)
	 */
	extern INT SequenceLen;		/*keyboard macro replacement in progess*/

	if (ShowMatchFlag && !Repeating && SequenceLen <= 0) {
		LONG	 Time;
		POSITION SavePos;
		LONG	 SaveRelativePosition;

		SavePos				 = CurrPos;
		SaveRelativePosition = RelativePosition;
		if (Flags & 1) GoBack(&CurrPos, 1);		/*pos to parenthesis*/
		if (!Position(0, '%', 0x86)) {			/*find matching one*/
			if (Flags & 1) Position(0, 'l', 1);	/*behind parenthesis again*/
			return;
		}
		SelectStart	   = CurrPos;
		if (RelativePosition < 0) GoBack(&SelectStart, -RelativePosition);
		else Advance(&SelectStart, RelativePosition);
		SelectBytePos  = CountBytes(&SelectStart);
		SelectCount	   = 1;
		ShowParenMatch = TRUE;
		InvalidateArea(&SelectStart, 1, 0);
		UpdateWindow(hwndMain);
		SelectCount = 0;
		if (Flags & 1) NewPosition(&SavePos);
		RelativePosition = SaveRelativePosition;
		Time = GetTickCount();
		do {
			static MSG Msg;

			/*any msg except WM_KEYUP and
			 *system timer (for caret) stops 1 sec loop
			 */
			if (!PeekMessage(&Msg, 0, 0, 0, PM_NOREMOVE)) {
				if (RestoreCaret) ShowEditCaret();
				#if defined(WIN32)
					Sleep(1);
				#else
					Yield();
				#endif
				continue;
			}
			if (!PeekMessage(&Msg, 0, WM_KEYUP, WM_KEYUP, PM_REMOVE) &&
				!PeekMessage(&Msg, 0, 0x118, 0x118 /*sys timer*/, PM_REMOVE))
					break;
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		} while (GetTickCount()-Time<1000 && ComparePos(&CurrPos, &SavePos)==0);
		ShowParenMatch = FALSE;
		InvalidateArea(&SelectStart, 1, 1);
		UpdateWindow(hwndMain);
	}
}
