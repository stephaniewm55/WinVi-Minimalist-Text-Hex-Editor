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
 *     22-Jul-2002	use of private myassert.h
 *     12-Sep-2002	+EBCDIC quick hack (Christian Franke)
 *     30-Oct-2002	corrected counting of number of lines in InsertBuffer
 *      1-Jul-2007	handling of page overlapping CR-LF
 *      5-Jul-2007	handling of Unicode characters
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *     23-Nov-2009	some fixes deleting chars and inserting newlines in UTF-16
 *     10-Jan-2010	changed positioning after insert from clpbrd in replace mode
 *     12-Jan-2010	another line counting bugfix in InsertBuffer
 *      8-Apr-2010	fixed removing of indent in UTF-16 files
 */

#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include "myassert.h"
#include "winvi.h"
#include "page.h"

INT  IndentChars;
LONG InsertCount;
BOOL SuppressMap = FALSE;

INT CharSize(INT c)
{
	if (UtfEncoding) {
		if (UtfEncoding == 16)
			return (c >= 0xd800 && c < 0xdc00) || c == C_CRLF ? 4 : 2;
		if (c >= 0x10000) return 4;
		if (c >= 0x800)	  return 3;
		if (c >= 0x80)	  return 2;
	}
	return 1 + (c == C_CRLF);
}

void InsertChar(INT Char) {
	INT space = CharSize(Char);

	if (IsViewOnly()) return;
	SetUnsafe();
	if (Mode != ReplaceMode || (HexEditMode ? CharAt(&CurrPos) == C_EOF
											: CharFlags(CharAt(&CurrPos)) & 1))
		Reserve(&CurrPos, space, 0);
	else {
		INT oldspace = CharSize(CharAt(&CurrPos));

		if (space != oldspace) {
			Reserve(&CurrPos, -oldspace, 0);
			Reserve(&CurrPos, space, 0);
		} else {
			EnterDeleteForUndo(&CurrPos, oldspace, 0);
			EnterInsertForUndo(&CurrPos, space);
		}
		ByteAt(&CurrPos);
	}
	++InsertCount;
	if (!SuppressMap) switch (CharSet) {
		case CS_EBCDIC:
			if ((unsigned)Char >= 0x100) Char = 0x1a;
			else Char = MapAnsiToEbcdic[Char];
			break;

		case CS_OEM:
			Char = UnicodeToOemChar(Char);
			break;
	}
	assert(CurrPos.p->PageBuf);
	if (space > 1) {
		POSITION Pos = CurrPos;

		UnsafePage(&CurrPos);
		if (Char == C_CRLF) {
			if (UtfEncoding == 16) {
				assert((Pos.i & 1) == 0 && Pos.i + 1 < Pos.p->Fill);
				if (UtfLsbFirst) {
					Pos.p->PageBuf[Pos.i]	= '\r';
					Pos.p->PageBuf[Pos.i+1]	= '\0';
					Advance(&Pos, 2);
					Pos.p->PageBuf[Pos.i]	= '\n';
					Pos.p->PageBuf[Pos.i+1]	= '\0';
				} else {
					Pos.p->PageBuf[Pos.i]	= '\0';
					Pos.p->PageBuf[Pos.i+1]	= '\r';
					Advance(&Pos, 2);
					Pos.p->PageBuf[Pos.i]	= '\0';
					Pos.p->PageBuf[Pos.i+1]	= '\n';
				}
				assert(Pos.i + 1 < Pos.p->Fill);
			} else {
				Pos.p->PageBuf[Pos.i] = '\r';
				Advance(&Pos, 1);
				Pos.p->PageBuf[Pos.i] = '\n';
			}
		} else if (UtfEncoding == 16) {
			assert(Pos.i + 1 < Pos.p->Fill);
			if (UtfLsbFirst) {
				Pos.p->PageBuf[Pos.i]	= Char;
				Pos.p->PageBuf[Pos.i+1]	= Char >> 8;
			} else {
				Pos.p->PageBuf[Pos.i]	= Char >> 8;
				Pos.p->PageBuf[Pos.i+1]	= Char;
			}
		} else {
			assert(UtfEncoding == 8);
			if (space == 4) {
				assert(Char >= 0x10000 && Char <= 0x10FFFF);
				Pos.p->PageBuf[Pos.i] = 0xf0 +  (Char >> 18);
				Advance(&Pos, 1);
				Pos.p->PageBuf[Pos.i] = 0x80 + ((Char >> 12) & 0x3f);
				UnsafePage(&Pos);
				Advance(&Pos, 1);
				Pos.p->PageBuf[Pos.i] = 0x80 + ((Char >>  6) & 0x3f);
				UnsafePage(&Pos);
				Advance(&Pos, 1);
				Pos.p->PageBuf[Pos.i] = 0x80 + ( Char        & 0x3f);
			} else if (space == 3) {
				assert(Char >= 0x800 && Char < 0x10000);
				Pos.p->PageBuf[Pos.i] = 0xe0 +  (Char >> 12);
				Advance(&Pos, 1);
				Pos.p->PageBuf[Pos.i] = 0x80 + ((Char >>  6) & 0x3f);
				UnsafePage(&Pos);
				Advance(&Pos, 1);
				Pos.p->PageBuf[Pos.i] = 0x80 + ( Char        & 0x3f);
			} else {
				assert(space == 2);
				assert(Char >= 0x80 && Char < 0x800);
				Pos.p->PageBuf[Pos.i] = 0xc0 +  (Char >>  6);
				Advance(&Pos, 1);
				Pos.p->PageBuf[Pos.i] = 0x80 + ( Char        & 0x3f);
			}
		}
	} else {
		if (Char >= 0x100) {
			extern INT AnsiCodePage;
			WCHAR	   SrcChar;
			BYTE	   ConvChars[4];
			UINT	   Cp;
			INT		   n;

			if ((Cp = AnsiCodePage) == 0) Cp = CP_ACP;
			SrcChar = Char;
			n = WideCharToMultiByte(Cp, 0, &SrcChar, 1,
									ConvChars, sizeof(ConvChars), NULL, NULL);
			if (n == 0) Char = '\177';
			else Char = ConvChars[0];
		}
		CurrPos.p->PageBuf[CurrPos.i] = Char;
	}
	UnsafePage(&CurrPos);
	{	POSITION InvalidatePos;

		InvalidatePos = CurrPos;
		CharAndAdvance(&CurrPos);
		if (UtfEncoding == 16 && CharAt(&CurrPos) == C_EOF)
			AdditionalNullByte = FALSE;
		if (ComparePos(&LineInfo[CurrLine].Pos, &CurrPos) == 0) {
			/*insert at start of line*/
			LineInfo[CurrLine].Pos = InvalidatePos;
		}
		InvalidateArea(&InvalidatePos, 1, HexEditMode ? 7 : 9);
	}
	if (!ReplaceSingle) {
		HideEditCaret();
		UpdateWindow(hwndMain);
		NewPosition(&CurrPos);
		if (HexEditMode) NewScrollPos();
		ShowEditCaret();
		GetXPos(&CurrCol);
		if (Indenting) {
			if (CharFlags(Char) & 16) ++IndentChars;
			else Indenting = FALSE;
		}
	}
}

BOOL DeleteChar(void) {
	INT		 c;
	POSITION EndPos = CurrPos;

	if (IsViewOnly()) return (FALSE);
	if (HexEditMode) {
		if (!GoBack(&CurrPos, 1 + (UtfEncoding == 16))) return (FALSE);
		if ((c = CharAt(&CurrPos)) == C_CRLF) c = ' ';
	} else if (CharFlags(c = GoBackAndChar(&CurrPos)) & 1) {
		if (c != C_EOF) {
			if (WrapPosFlag) {
				GotKeydown(VK_DELETE);
				return (TRUE);
			}
			CharAndAdvance(&CurrPos);
		}
		return (FALSE);
	}
	if (InsertCount) --InsertCount;
	#if 0
	//	else return (FALSE);
	#endif
	SetUnsafe();
	HideEditCaret();
	NewPosition(&CurrPos);
	Reserve(&CurrPos, CountBytes(&CurrPos) - CountBytes(&EndPos),
			(WORD)(CharFlags(c) & 1));
	if (CharAt(&CurrPos) == C_EOF) {
		if (!HexEditMode && !(CharFlags(GoBackAndChar(&CurrPos)) & 1))
			AdvanceAndChar(&CurrPos);
		AdditionalNullByte = FALSE;
	}
	InvalidateArea(&CurrPos, 0, HexEditMode ? 7 : 9);
	UpdateWindow(hwndMain);
	NewPosition(&CurrPos);
	if (HexEditMode) NewScrollPos();
	ShowEditCaret();
	GetXPos(&CurrCol);
	if (Indenting && IndentChars) --IndentChars;
	return (TRUE);
}

void SetCurrPos(VOID) {
	LineInfo[CurrLine+1].Pos		  = CurrPos;
	LineInfo[CurrLine+1].ByteCount	  = CountBytes(&CurrPos);
	LineInfo[CurrLine+1].nthContdLine = 0;
	LineInfo[CurrLine+1].Flags		  = LIF_VALID;
}

void ScrollDisplay(INT Lines, WORD Flags) {
	/*Flags: 1=redraw current line
	 */
	INT			From = Lines>0 ? CurrLine+1 : CurrLine+2;
	INT			To   = Lines>0 ? CurrLine+2 : CurrLine+1;
	INT			ScrolledLines = VisiLines - CurrLine;
	INT			UpdateLino = 0;
	HDC			hDC;
	extern BOOL Exiting;

	if (Exiting) {
		assert(!Exiting);
		return;
	}
	if (NumberFlag && LineInfo[VisiLines].Flags & LIF_EOF) {
		for (UpdateLino=VisiLines;
			 UpdateLino && LineInfo[--UpdateLino].Flags & LIF_EOF;
			);
		if (Lines > 0) ++UpdateLino;
	}
	memmove(LineInfo+To, LineInfo+From, ScrolledLines * sizeof(LINEINFO));
	if (Lines > 0) SetCurrPos();
	HideEditCaret();
	if (hwndMain && (hDC = GetDC(hwndMain)) != 0) {
		extern LONG	xOffset;
		RECT		r1, r2;
		INT			x1;

		r1.left   = r1.top = 0;
		r1.right  = ClientRect.right;
		r1.bottom = ClientRect.bottom;
		TextAreaOnly(&r1, TRUE);
		x1 = r1.left;
		if (NumberFlag)
			r1.left = (INT)(StartX(FALSE) - xOffset - (AveCharWidth>>1));
		r1.top += GetPixelLine(CurrLine+1, TRUE);
		ScrollDC(hDC, 0, Lines * TextHeight, &r1, &r1, NULL, &r2);
		ReleaseDC(hwndMain, hDC);
		if (Flags & 1) r2.top -= TextHeight;
		InvalidateRect(hwndMain, &r2, TRUE);
		if (*BitmapFilename) {
			/*refresh scrolled background bitmap (correct moved origin)*/
			while (To < VisiLines) LineInfo[To++].Flags |= LIF_REFRESH_BKGND;
			RefreshBitmap = TRUE;
		}
		UpdateWindow(hwndMain);
		if (UpdateLino) {
			r1.right = r1.left;
			r1.left  = x1;
			r1.bottom = (r1.top = GetPixelLine(UpdateLino, FALSE)) + TextHeight;
			InvalidateRect(hwndMain, &r1, TRUE);
			UpdateWindow(hwndMain);
		}
	}
}

void InsertEol(UINT Flags, INT Char) {
	/*Flags: 1=redraw current line
	 *		 2=indent from next line (count autoindent in following line)
	 *		 4=newline while in insert mode, remove empty autoindent
	 *		 8=insert without autoindent
	 */
	INT OldCurrLine = CurrLine;

	if (IsViewOnly()) return;
	SetUnsafe();
	++InsertCount;
	if (CurrPos.i == 0 && (CurrPos.p == NULL || CurrPos.p->Fill == 0))
		/*repaint necessary when line numbers are on and buffer is empty*/
		InvalidateArea(&CurrPos, 1, 9);
	if (Char == '\n') {
		POSITION prev;
		INT		 c;

		prev = CurrPos;
		if (GoBack(&prev, UtfEncoding == 16 ? 2 : 1) &&
				((c = CharAt(&prev)) == '\r' || c == C_CRLF)) {
			CurrPos = prev;
			if (CurrLine) --CurrLine;
			else FindValidPosition(&CurrPos, 1);
		//	Position(1, '\b', 0x223);
			InvalidateArea(&CurrPos, 1, 3);
			Reserve(&CurrPos, UtfEncoding == 16 ? -2 : -1, c == '\r');
			--InsertCount;
			Char = C_CRLF;
		}
	}
	if (Char == C_CRLF) {
		++InsertCount;
		if (UtfEncoding == 16) {
			INT i;

			Reserve(&CurrPos, 4, 1);
			assert(CurrPos.p->PageBuf);
			assert(CurrPos.i + 4 <= CurrPos.p->Fill);
			i = CurrPos.i;
			if (!UtfLsbFirst) CurrPos.p->PageBuf[i++] = '\0';
			CurrPos.p->PageBuf[i++] = '\r';
			CurrPos.p->PageBuf[i++] = '\0';
			CurrPos.p->PageBuf[i++] = '\n';
			if (UtfLsbFirst) CurrPos.p->PageBuf[i++] = '\0';
		} else {
			Reserve(&CurrPos, 2, 1);
			assert(CurrPos.p->PageBuf);
			CurrPos.p->PageBuf[CurrPos.i]   = '\r';
			CurrPos.p->PageBuf[CurrPos.i+1] = CharSet==CS_EBCDIC ? 0x25 : '\n';
		}
	} else {
		if (Char=='\r' && CharAt(&CurrPos)=='\n') {
			/*glue CR and LF, must be in same buffer...*/
			UINT InvalidateFlags = 9;

			assert(LineInfo[CurrLine].ByteCount  <= CountBytes(&CurrPos));
			assert(LineInfo[CurrLine+1].ByteCount > CountBytes(&CurrPos));
			if (UtfEncoding == 16) {
				INT i;

				if (CurrPos.i >= 2 &&
						CurrPos.p->PageBuf[CurrPos.i-1-UtfLsbFirst] == '\r') {
					Reserve(&CurrPos, -2, 0);
					++CurrLine;
					InvalidateFlags = 7;
				} else Reserve(&CurrPos, -2, 1);
				Reserve(&CurrPos, 4, 1);
				i = CurrPos.i;
				if (!UtfLsbFirst) CurrPos.p->PageBuf[i++] = '\0';
				CurrPos.p->PageBuf[i++] = Char;
				CurrPos.p->PageBuf[i++] = '\0';
				CurrPos.p->PageBuf[i++] = '\n';
				if (UtfLsbFirst) CurrPos.p->PageBuf[i++] = '\0';
				LineInfo[CurrLine+1].ByteCount = CountBytes(&CurrPos) + 4;
			} else {
				if (CurrPos.i >= 1 && CurrPos.p->PageBuf[CurrPos.i-1] == '\r') {
					Reserve(&CurrPos, -1, 0);
					++CurrLine;
					InvalidateFlags = 7;
				} else Reserve(&CurrPos, -1, 1);
				Reserve(&CurrPos, 2, 1);
				assert(CurrPos.p->PageBuf && CurrPos.i+1 < CurrPos.p->Fill);
				CurrPos.p->PageBuf[CurrPos.i]   = Char;
				CurrPos.p->PageBuf[CurrPos.i+1] = '\n';
				LineInfo[CurrLine+1].ByteCount = CountBytes(&CurrPos) + 2;
			}
			LineInfo[CurrLine+1].Pos = CurrPos;
			Advance(&LineInfo[CurrLine+1].Pos, UtfEncoding == 16 ? 2 : 1);
			InvalidateArea(&CurrPos, 0, InvalidateFlags);
			UpdateWindow(hwndMain);
			ShowEditCaret();
			Advance(&CurrPos, UtfEncoding == 16 ? 2 : 1);
			return;
		}
		Reserve(&CurrPos, 1 + (UtfEncoding == 16), 1);
		assert(CurrPos.p->PageBuf);
		if (UtfEncoding == 16) {
			if (UtfLsbFirst) {
				CurrPos.p->PageBuf[CurrPos.i]	= Char;
				CurrPos.p->PageBuf[CurrPos.i+1]	= '\0';
			} else {
				CurrPos.p->PageBuf[CurrPos.i]	= '\0';
				CurrPos.p->PageBuf[CurrPos.i+1]	= Char;
			}
		} else CurrPos.p->PageBuf[CurrPos.i] = Char;
	}
	if (HexEditMode) InvalidateArea(&CurrPos, 1, 7);
	CharAndAdvance(&CurrPos);
	if (!HexEditMode) ScrollDisplay(1, (WORD)(Flags & 1));
	else SetCurrPos();
	if (!(Flags & 8) && AutoIndentFlag) {
		INT		 Indent = 0, c;
		POSITION Pos;

		/*find start of autoindent reference line...*/
		Pos = CurrPos;
		if (Flags & 2) GoBackAndChar(&CurrPos);
		else {
			GoBackAndChar(&Pos);
			do; while (!(CharFlags(c = GoBackAndChar(&Pos)) & 1));
			if (c != C_EOF) CharAndAdvance(&Pos);
		}

		/*count indent...*/
		IndentChars = -1;
		for (;;) {
			++IndentChars;
			switch (c = CharAndAdvance(&Pos)) {
				case ' ':	++Indent;							  continue;
				case '\t':	Indent += TabStop - Indent % TabStop; continue;
			}
			break;
		}
		if (Flags & 4 && CharFlags(c) & 1 && Indent) {
			/*remove indent in empty line...*/
			INT IndentBytes = IndentChars;

			if (UtfEncoding == 16) IndentBytes <<= 1;
			GoBackAndChar(&Pos);
			GoBack(&CurrPos, IndentBytes);
			GoBack(&Pos,     IndentBytes);
			Reserve(&Pos,   -IndentBytes, 0);
			if ((InsertCount -= IndentChars) < 0) InsertCount = 0;
			if (CharAt(&Pos) == '\n' && GoBackAndChar(&Pos) == C_CRLF)
				/* no additional line, redraw scrolled area... */
				InvalidateArea(&Pos, 1, 7);
			else if (Flags & 1) InvalidateArea(&Pos, 2, 9);
		}

		/*insert indent now...*/
		IndentChars = 0;
		Indenting = TRUE;
		{	MODEENUM SaveMode = Mode;

			Mode = InsertMode;	/*insert indent even in replace mode*/
			if (TabStop > 1) {
				while (Indent >= TabStop) {
					InsertChar('\t');
					Indent -= TabStop;
				}
			}
			while (Indent--) InsertChar(' ');
			Mode = SaveMode;
		}
		if (Flags & 2) CharAndAdvance(&CurrPos);
	}
	NewPosition(&CurrPos);
	if (UtfEncoding == 16 && CharAt(&CurrPos) == C_EOF)
		AdditionalNullByte = FALSE;
	if (hwndMain) {
		if (OldCurrLine >= CrsrLines-1) {
			/*screen has been scrolled up by NewPosition(),*/
			/*invalid area has also moved, invalidate again...*/
			RECT r;
			r = ClientRect;
			TextAreaOnly(&r, TRUE);
			r.top += GetPixelLine(CurrLine-1, TRUE);
			InvalidateRect(hwndMain, &r, TRUE);
		}
		UpdateWindow(hwndMain);
	}
	NewScrollPos();
	ShowEditCaret();
	GetXPos(&CurrCol);
}

void DeleteEol(BOOL RedrawCurrline) {
	INT c = CharAt(&CurrPos);

	if (IsViewOnly()) return;
	switch (c) {
		case C_EOF:		return;

		case C_CRLF:	Reserve(&CurrPos, UtfEncoding == 16 ? -4 : -2, 1);
						--InsertCount;
						break;

		default:		Reserve(&CurrPos, UtfEncoding == 16 ? -2 : -1, 1);
						break;
	}
	--InsertCount;
	ScrollDisplay(-1, 0);
	if (hwndMain && RedrawCurrline) {
		RECT r;

		r = ClientRect;
		TextAreaOnly(&r, TRUE);
		r.top += GetPixelLine(CurrLine, TRUE);
		r.bottom = r.top + TextHeight;
		InvalidateRect(hwndMain, &r, TRUE);
		UpdateWindow(hwndMain);
	}
	NewScrollPos();
}

void DeleteForward(UINT Number) {
	INT      i = 1;
	POSITION Pos;

	Pos = CurrPos;
	if (!Number) ++Number;
	do {
		if (!HexEditMode && CharFlags(CharAndAdvance(&Pos)) & 1) break;
		--i;
	} while (Number==(UINT)-1 || --Number);
	if (i <= 0) {
		if (i < 0) Reserve(&CurrPos, i, 0);
		CharAndAdvance(&CurrPos);
		++InsertCount;
		DeleteChar();
	}
	if (Mode == CommandMode && (HexEditMode || CharFlags(CharAt(&CurrPos)) & 1))
		Position(1, 'h', 0);
}

void Join(void) {
	INT  c, LastC = C_UNDEF;
	BOOL InsertSpace = TRUE;

	if (IsViewOnly()) return;
	while (!(CharFlags(c = CharAt(&CurrPos)) & 1))
		LastC = CharAndAdvance(&CurrPos);
	if (c == C_EOF) {
		if (CharFlags(LastC) & 1) GoBackAndChar(&CurrPos);
	} else {
		SetUnsafe();
		DeleteEol(TRUE);
		while ((c = CharAt(&CurrPos)) == '\t' || c == ' ')
			Reserve(&CurrPos, UtfEncoding == 16 ? -2 : -1, 0);
		InvalidateArea(&CurrPos, 1, 9);
		switch (LastC) {
			case '(': case '{': case '[':
			case ' ': case '\t':
			case C_UNDEF:
				InsertSpace = FALSE;
		}
		switch (c) {
			case ')': case '}': case ']':
			case '.': case ',': case ';': case ':':
				InsertSpace = FALSE;
		}
		if (!(CharFlags(c) & 1)) {
			if (InsertSpace) {
				InsertChar(' ');
				GoBackAndChar(&CurrPos);
			}
		} else if (LastC != C_UNDEF && !(CharFlags(LastC) & 1))
			if (c != C_EOF)
				if (CharFlags(c = GoBackAndChar(&CurrPos)) & 1 && c != C_EOF)
					CharAndAdvance(&CurrPos);
	}
	#if 0
	//	NewPosition(&CurrPos);
	//	ShowEditCaret();
	#endif
}

void DeleteSelected(INT Flags) {
	/*Flags: 1=Show updated window
	 *		 2=No yank (called from undo, substitute or ^V)
	 *		 4=Suppress SetUnsafe()
	 *		 8=Suppress cursor setting to displayable position
	 *		16=Area was really selected (reselect when undoing)
	 *		32=cannot be undone (called for replacing ^ after typing ^V)
	 */
	ULONG	 Newlines = SelectStart.p->Prev ? SelectStart.p->Prev->Newlines : 0;
	ULONG	 SelectBytePos;
	INT		 n, InvalidateFlags = 3;
	POSITION EndSel;

	if (!SelectCount) return;
	if (IsViewOnly()) return;
	SelectBytePos = CountBytes(&SelectStart);
	EnterDeleteForUndo(&SelectStart, SelectCount, (WORD)((Flags >> 4) & 3));
	EndSel = SelectStart;
	Advance(&EndSel, SelectCount);
	if (!(Flags & 2)) Yank(&SelectStart, SelectCount,
						   (WORD)(CharFlags(GoBackAndChar(&EndSel)) & 1));
	if (ComparePos(&SelectStart, &ScreenStart) < 0) {
		/*move start of (currently invisible) SelectStart line into view...*/
		INT c;

		ScreenStart = SelectStart;
		if (HexEditMode) {
			GoBack(&ScreenStart, ((INT)CountBytes(&ScreenStart) & 15) + 32);
		} else {
			for (n=3; n; --n)
				do; while (!(CharFlags(c = GoBackAndChar(&ScreenStart)) & 1));
			if (c != C_EOF) CharAndAdvance(&ScreenStart);
			FirstVisible = CountLines(&ScreenStart);
		}
		InvalidateFlags |= 4;
	} else if (HexEditMode) InvalidateFlags |= 4;
	if (Flags & 1) InvalidateArea(&SelectStart, SelectCount, InvalidateFlags);
	while (SelectStart.i + SelectCount > SelectStart.p->Fill) {
		n = SelectStart.p->Fill - SelectStart.i;
		if (n) {
			CorrectLineInfo(0, 0, 0, 0, SelectBytePos, -n);
			SelectCount -= n;
			if (SelectStart.i &&
					(SelectStart.p->PageBuf || ReloadPage(SelectStart.p))) {
				Newlines = SelectStart.p->Newlines - CountNewlinesInBuf
									(SelectStart.p->PageBuf + SelectStart.i,
									 SelectStart.p->Fill    - SelectStart.i,
									 NULL, NULL /*TODO: check prev char*/);
			}
			UnsafePage(&SelectStart);
		}
		SelectStart.p->Newlines = Newlines;
		SelectStart.p->Fill = SelectStart.i;
		SelectStart.p = SelectStart.p->Next;
		if (SelectStart.p == NULL) {
			SelectStart.p = LastPage;
			SelectCount = 0;
			break;
		}
		SelectStart.i = 0;
	}
	n = SelectStart.p->Fill - SelectStart.i - (UINT)SelectCount;
	if (n) {
		ByteAt(&SelectStart);	/*ensure page to be loaded*/
		assert(SelectStart.p->PageBuf);
		_fmemmove(SelectStart.p->PageBuf + SelectStart.i,
				  SelectStart.p->PageBuf + SelectStart.i + (UINT)SelectCount,
				  n);
		UnsafePage(&SelectStart);
	}
	CorrectLineInfo(SelectStart.p, SelectStart.i + (UINT)SelectCount,
					SelectStart.p, SelectStart.i,
					SelectBytePos, -(INT)SelectCount);
	SelectStart.p->Fill	-= (UINT)SelectCount;
	if (SelectStart.p->PageBuf == NULL) ReloadPage(SelectStart.p);
	assert(SelectStart.p->PageBuf);
	Newlines = SelectStart.p->Newlines -
		(CountNewlinesInBuf(SelectStart.p->PageBuf, SelectStart.p->Fill, NULL,
						    NULL /*TODO: check prev char*/) + Newlines);
	SelectCount = 0;
	if (Newlines) {
		LPPAGE p = SelectStart.p;

		while (p) {
			p->Newlines -= Newlines;
			p = p->Next;
		}
		LinesInFile -= Newlines;
	}
	if (UtfEncoding == 16 && CharAt(&CurrPos) == C_EOF)
		AdditionalNullByte = FALSE;
	if (Flags & 1) {
		UpdateWindow(hwndMain);
		if (ComparePos(&CurrPos, &SelectStart) || Newlines)
			NewPosition(&SelectStart);
		if (!(Flags & 8))
			FindValidPosition(&CurrPos, (WORD)(Mode==InsertMode ? 3 : 0));
		NewScrollPos();
	} else CurrPos = SelectStart;
	if (!(Flags & 4)) SetUnsafe();
	CheckClipboard();
}

BOOL InsertBuffer(LPBYTE Buffer, ULONG Length, WORD Flags) {
	/*Flags: 1=call to FindValidPosition()
	 *		 2=Buffer points to WCHAR array
	 */
	ULONG    i = Length;
	POSITION InsertStart;
	UINT     wFlags = 3;
	BOOL     DoAdvance = FALSE, Disabled = FALSE;
	INT		 LineFeed = '\n';

	if (!Length || !hwndMain) return (FALSE);
	if (IsViewOnly()) return (FALSE);
	InsertStart = CurrPos;
	if (GoBackAndChar(&InsertStart) != C_EOF) ++DoAdvance;
	Interrupted = FALSE;
	assert(UtfEncoding != 16 || !(Length & 1));
	if (CharSet == CS_EBCDIC) LineFeed = MapAnsiToEbcdic[LineFeed];
	while (i--) {
		INT c, cc;

		c = *Buffer++;
		if ((UtfEncoding == 16 && i) || Flags & 2)
			if (UtfLsbFirst || Flags & 2) c |= *Buffer++ << 8;
			else c = (c << 8) | *Buffer++;
		cc = CharSet == CS_EBCDIC ? MapEbcdicToAnsi[c] : c;
		if (CharFlags(cc) & 1) {
			if ((UtfEncoding == 16 && i) || Flags & 2) {
				INT c2;

				if (i >= 3) {
					c2 = *Buffer;
					if (UtfLsbFirst || Flags & 2) c2 |= Buffer[1] << 8;
					else c2 = (c2 << 8) | Buffer[1];
				} else c2 = 0;
				if (c == '\r' && c2 == LineFeed) {
					Buffer += 2;
					i -= 2;
					Reserve(&CurrPos, 4, 1);
					++InsertCount;
					assert(CurrPos.p->PageBuf);
					if (UtfLsbFirst) {
						CurrPos.p->PageBuf[CurrPos.i++] = '\r';
						CurrPos.p->PageBuf[CurrPos.i++] = '\0';
					} else {
						CurrPos.p->PageBuf[CurrPos.i++] = '\0';
						CurrPos.p->PageBuf[CurrPos.i++] = '\r';
					}
					c = LineFeed;
				} else if (c == LineFeed) {
					if (CharAt(&CurrPos) == '\n') {
						Reserve(&CurrPos, 2, 1);
						/*TODO: assert(PrevChar!='\r' || CurrPos.i>0);*/
					} else {
						ULONG n = GoBack(&CurrPos, 2);
						if (n && CharAt(&CurrPos) == '\r') {
							/*cr/lf combination...*/
							#if 0	/*did not work this way (29-Jul-98)*/
							//	Reserve(&CurrPos, -1, 1);
							//	Reserve(&CurrPos,  2, 1);
							//	CurrPos.p->PageBuf[CurrPos.i++] = '\r';
							#else
								Advance(&CurrPos, n);
								/*TODO: assert(CurrPos.i > 0);*/
								Reserve(&CurrPos, 2, 0);
								if (!GoBack(&InsertStart, 2)) DoAdvance = FALSE;
							#endif
						} else {
							Advance(&CurrPos, n);
							Reserve(&CurrPos, 2, 1);
						}
					}
				} else if (c=='\r' && CharAt(&CurrPos)=='\n') {
					Reserve(&CurrPos, 2, 0);
					/*TODO: assert(CurrPos.i+1 < CurrPos.Len);*/
				} else Reserve(&CurrPos, 2, 1);
			} else {
				if (i && c == '\r' && *Buffer == LineFeed) {
					++Buffer;
					--i;
					Reserve(&CurrPos, 2, 1);
					++InsertCount;
					assert(CurrPos.p->PageBuf);
					CurrPos.p->PageBuf[CurrPos.i++] = '\r';
					c = LineFeed;
				} else if (c == LineFeed) {
					if (CharAt(&CurrPos) == '\n') {
						Reserve(&CurrPos, 1, 1);
						/*TODO: assert(PrevChar!='\r' || CurrPos.i>0);*/
					} else {
						ULONG n = GoBack(&CurrPos, 1);

						if (n && CharAt(&CurrPos) == '\r') {
							/*cr/lf combination...*/
							#if 0	/*did not work this way (29-Jul-98)*/
							//	Reserve(&CurrPos, -1, 1);
							//	Reserve(&CurrPos,  2, 1);
							//	CurrPos.p->PageBuf[CurrPos.i++] = '\r';
							#else
								Advance(&CurrPos, 1);
								/*TODO: assert(CurrPos.i > 0);*/
								Reserve(&CurrPos, 1, 0);
								if (!GoBack(&InsertStart, 1)) DoAdvance = FALSE;
							#endif
						} else {
							Advance(&CurrPos, n);
							Reserve(&CurrPos, 1, 1);
						}
					}
				} else if (c=='\r' && CharAt(&CurrPos)=='\n') {
					Reserve(&CurrPos, 1, 0);
					/*TODO: assert(CurrPos.i+1 < CurrPos.Len);*/
				} else Reserve(&CurrPos, 1, 1);
			}
			wFlags |= 4;
		} else {
			INT csize = UtfEncoding == 16 ? 2 : 1;

			if (CharAt(&CurrPos) == '\n') {
				POSITION p2;

				p2 = CurrPos;
				if (GoBack(&p2, csize) == csize && CharAt(&p2) == C_CRLF) {
					Reserve(&p2, -csize, 0);
					Reserve(&p2,  csize, 1);
					if (UtfEncoding == 16) {
						if (UtfLsbFirst) {
							p2.p->PageBuf[p2.i]   = '\r';
							p2.p->PageBuf[p2.i+1] = '\0';
						} else {
							p2.p->PageBuf[p2.i]   = '\0';
							p2.p->PageBuf[p2.i+1] = '\r';
						}
					} else  p2.p->PageBuf[p2.i]   = '\r';
				}
			}
			Reserve(&CurrPos, csize, 0);
		}
		++InsertCount;
		assert(CurrPos.p->PageBuf);
		if (UtfEncoding == 16 && i) {
			assert (CurrPos.i + 1 < CurrPos.p->Fill);
			if (UtfLsbFirst) {
				CurrPos.p->PageBuf[CurrPos.i]	= c;
				CurrPos.p->PageBuf[CurrPos.i+1]	= c >> 8;
			} else {
				CurrPos.p->PageBuf[CurrPos.i]	= c >> 8;
				CurrPos.p->PageBuf[CurrPos.i+1]	= c;
			}
			ByteAndAdvance(&CurrPos);
			--i;
		} else CurrPos.p->PageBuf[CurrPos.i] = c;
		ByteAndAdvance(&CurrPos);
		if (!((Length-i) & 8191)) {
			if (!Disabled) {
				Disable(Disabled = TRUE);
				SetUnsafe();
			}
			MessageLoop(FALSE);
			if (Interrupted) {
				static WCHAR BufferW[80];

				LOADSTRINGW(hInst, 218, BufferW, WSIZE(BufferW));
				NewStatus(0, BufferW, NS_ERROR);
				break;
			}
		}
	}
	if (Disabled) Enable();
	SetUnsafe();
	if (DoAdvance) CharAndAdvance(&InsertStart);
	else {
		InsertStart.p = FirstPage;
		assert(InsertStart.i == 0 ||
			   UtfEncoding >= 8 && InsertStart.i == 4 - (UtfEncoding >> 3));
		CharAt(&InsertStart);	/*...normalize position pointer*/
		ScreenStart = InsertStart;
	}
	if (UtfEncoding == 16 && CharAt(&CurrPos) == C_EOF)
		AdditionalNullByte = FALSE;
	if (HexEditMode) wFlags |= 4;
	InvalidateArea(&InsertStart, Length, wFlags);
	UpdateWindow(hwndMain);
	if (Flags & 1)
		FindValidPosition(&CurrPos, (WORD)((Mode == InsertMode)  |
										   (Mode == ReplaceMode) |
										   (HexEditMode << 1)));
	NewScrollPos();
	Indenting = FALSE;
	return (TRUE);
}

extern INT DblQuoteChar;
HGLOBAL    YankHandles[36];
ULONG      YankLength[36];
INT		   YankSubIndex[10];
INT        YankIndex = 0;
/*Meaning of elements in the arrays above:
 *	0:	   yank/delete buffer used if none of the following apply,
 *	1-9:   automatically assigned for delete row operations
 *		   (rotated, 1=most recent, 9=least recent, buffer names "1 to "9)
 *		   may be a copy of one of the following entries,
 *		   then YankSubIndex[] contains the corresponding index,
 *	10-35: buffers used for named buffers "a through "z.
 *		   If using "A to "Z, the text will be appended.
 */

BOOL Yank(PPOSITION Pos, ULONG Length, WORD Flags) {
	/*Flags: 1: Enter in YankHandles[1...9] (delete row(s) operation)
	 */
	POSITION  YankPos;
	HGLOBAL   YankHandle;
	CHAR huge *YankMem;
	ULONG	  Expand = 0;
	INT		  NumericIndex;

	YankPos = *Pos;
	if (DblQuoteChar) {
		if (isdigit(DblQuoteChar)) YankIndex = DblQuoteChar & 15;
		else if (!isalpha(DblQuoteChar)
				 || (YankIndex = (DblQuoteChar & 31) + 9) >= 36)
		   YankIndex = 0;
	} else YankIndex = 0;
	if (Flags & 1) {

		if (isdigit(DblQuoteChar)) NumericIndex = YankIndex;
		else {
			/*exchange YankHandles 1 and 9 and shift...*/
			HGLOBAL TempHandle;
			ULONG	TempLength;
			INT		TempSubIndex;

			TempHandle	 = YankHandles[9];
			TempLength	 = YankLength[9];
			TempSubIndex = YankSubIndex[9];
			memmove((VOID*)	/*cast should not be necessery,
							 *but VC 1.5 does not understand it without*/
					(YankHandles+2), YankHandles+1,  8*sizeof(YankHandles[1]));
			memmove(YankLength+2,	 YankLength+1,   8*sizeof(YankLength[1]));
			memmove(YankSubIndex+2,	 YankSubIndex+1, 8*sizeof(YankSubIndex[1]));
			YankHandles[1]	= TempHandle;
			YankLength[1]	= TempLength;
			YankSubIndex[1]	= TempSubIndex;
			NumericIndex	= 1;
		}
		if (!YankSubIndex[NumericIndex] && YankHandles[NumericIndex] != NULL)
			GlobalFree(YankHandles[NumericIndex]);
		YankHandles [NumericIndex] = NULL;
		YankSubIndex[NumericIndex] = 0;
		if (!YankIndex) YankIndex = NumericIndex;
	} else NumericIndex = 0;
	if (YankIndex > 9) {
		INT i;

		for (i=1; i<=9; ++i)
			if (YankSubIndex[i] == YankIndex) {
				/*don't free, still in use...*/
				YankHandles[YankIndex] = NULL;
				YankSubIndex[i] = 0;
			}
	}
	if ((YankHandle = YankHandles[YankIndex]) != NULL) {
		if (DblQuoteChar<'A' || DblQuoteChar>'Z') {
			GlobalFree(YankHandle);
			YankHandle = NULL;
		} else Expand = YankLength[YankIndex];
	}
	if (Length) {
		CHAR huge *p;

		if (YankHandle != NULL) {
			HGLOBAL h = GlobalReAlloc(YankHandle, Expand+Length, GMEM_MOVEABLE);
			if (!h) {
				ErrorBox(MB_ICONSTOP, 308);
				ShowEditCaret();
				return (FALSE);
			}
			YankHandle = h;
		} else YankHandle = GlobalAlloc(GMEM_MOVEABLE, Length);
		if (YankHandle == NULL || (YankMem = GlobalLock(YankHandle)) == NULL) {
			if (YankHandle != NULL) GlobalFree(YankHandle);
			YankHandles[YankIndex] = NULL;
			ErrorBox(MB_ICONSTOP, 308);
			ShowEditCaret();
			return (FALSE);
		}
		YankLength[YankIndex] = Expand + Length;
		p = YankMem + Expand;
		while (Length) {
			UINT n;

			CharAt(&YankPos);	/*...normalize pos and enforce page load*/
			n = YankPos.p->Fill - YankPos.i;
			if ((ULONG)n > Length) n = (UINT)Length;
			assert(n > 0);
			assert(n <= PAGESIZE);
			if (n == 0 || n > PAGESIZE) {
				/*TODO:  Should not but seems to occur.  Investigate!*/
				extern WCHAR ErrorTitle[16];

				wcscpy(ErrorTitle, L"Yank Error");
				ErrorBox(MB_ICONSTOP, 290, n);
				ErrorTitle[0] = '\0';
				YankLength[YankIndex] -= Length;
				break;
			}
			assert(YankPos.p->PageBuf != NULL);
			hmemcpy(p, YankPos.p->PageBuf + YankPos.i, n);
			Length	  -= n;
			YankPos.i += n;
			p		  += n;
		}
		GlobalUnlock(YankHandle);
	}
	YankHandles[YankIndex] = YankHandle;
	if (NumericIndex) {
		YankHandles [NumericIndex] = YankHandle;
		YankLength  [NumericIndex] = YankLength[YankIndex];
		YankSubIndex[NumericIndex] = YankIndex > 9 ? YankIndex : 0;
	}
	return (TRUE);
}

BOOL Put(long Number, BOOL Behind) {
	INT		  c, Index;
	BOOL	  LineFlag;
	POSITION  SavePos;
	BYTE huge *YankMem;

	if (isdigit(DblQuoteChar)) Index = DblQuoteChar & 15;
	else if (!isalpha(DblQuoteChar) || (Index = (DblQuoteChar & 31) + 9) >= 36)
		Index = YankIndex;
	if (!YankHandles[Index] || IsViewOnly()) return (FALSE);
	HideEditCaret();
	YankMem = GlobalLock(YankHandles[Index]);
	if (!YankMem) return (FALSE);
	LineFlag = CharFlags(YankMem[YankLength[Index]-1]) & 1;
	if (LineFlag) {
		/*insert as lines...*/
		if (Behind) {
			do; while (!(CharFlags(CharAndAdvance(&CurrPos)) & 1));
		} else {
			do; while (!(CharFlags(c = GoBackAndChar(&CurrPos)) & 1));
			if (c != C_EOF) CharAndAdvance(&CurrPos);
		}
	} else if (Behind && !(CharFlags(CharAt(&CurrPos)) & 1))
		CharAndAdvance(&CurrPos);
	SavePos = CurrPos;
	c = GoBackAndChar(&SavePos);
	do InsertBuffer(YankMem, YankLength[Index], 0);
	while (--Number > 0);
	GlobalUnlock(YankHandles[Index]);
	if (UtfEncoding == 16 && CharAt(&CurrPos) == C_EOF)
		AdditionalNullByte = FALSE;
	CurrPos = SavePos;
	if (c != C_EOF || CharFlags(CharAt(&CurrPos)) & 16)
		do; while (CharFlags(c = AdvanceAndChar(&CurrPos)) & 16 && LineFlag);
	FindValidPosition(&CurrPos, 0);
	ShowEditCaret();
	return (TRUE);
}

BOOL Shift(LONG Amount, WORD Flags) {
	/*Flags: 1=cursor may be positioned behind last character of line
	 *		 2=retain spacing if line is empty
	 *		 4=position at previous location
	 *		 8=shift in (else shift out)
	 *		16=remove all indenting chars
	 */
	INT		 (*Advance)(PPOSITION);
	INT		 c, Indent, nOldChars, LineCount = 0;
	BOOL	 More = TRUE;
	POSITION EolPos, LineStartPos, p;
	INT		 SaveCurrLine = CurrLine;

	/*to be implemented: Disable(), <Ctrl/Break>*/

	if (IsViewOnly()) return (FALSE);
	EolPos = CurrPos;
	while (!(CharFlags(c = GoBackAndChar(&EolPos)) & 1)) ++Amount;
	if (Amount < 0) {
		++Amount;
		if (c == C_CRLF) ++Amount;
		Amount  = -Amount;
		Advance = GoBackAndChar;
	} else {
		if (c == C_EOF) --Amount;
		Advance = AdvanceAndChar;
	}
	if (UtfEncoding == 16) Amount /= 2;
	do {
		LineStartPos = EolPos;
		if (CharFlags(c) & 0x20) {
			c = CharAt(&LineStartPos);
			if (Advance != AdvanceAndChar) More = FALSE;
		} else {
			c = AdvanceAndChar(&LineStartPos);
			if (Amount < 0) More = FALSE;
		}
		p = LineStartPos;
		Indent = nOldChars = 0;
		for (;;) {
			switch (c) {
				case '\t': Indent += TabStop - Indent % TabStop;
						   ++nOldChars;
						   c = AdvanceAndChar(&p);
						   continue;
				case ' ':  ++Indent;
						   ++nOldChars;
						   c = AdvanceAndChar(&p);
						   continue;
				default:   break;
			}
			break;
		}
		if (CharFlags(c) & 1 && !(Flags & 2)) Indent = 0;
		else if (Flags & 8) Indent += ShiftWidth;
		else if (Flags & 16 || (Indent -= ShiftWidth) < 0) Indent = 0;
		if (Advance == AdvanceAndChar)
			Amount += Indent/TabStop + Indent%TabStop - nOldChars;
		if (nOldChars > 0 || Indent > 0) SetUnsafe();
		Reserve(&LineStartPos, UtfEncoding==16 ? 2*-nOldChars : -nOldChars, 0);
		p = LineStartPos;
		if (TabStop > 1) {
			while (Indent >= TabStop) {
				assert(p.p->PageBuf);
				Reserve(&p, 1 + (UtfEncoding == 16), 0);
				if (UtfEncoding == 16) {
					if (UtfLsbFirst) {
						p.p->PageBuf[p.i]	= '\t';
						p.p->PageBuf[p.i+1]	= '\0';
					} else {
						p.p->PageBuf[p.i]	= '\0';
						p.p->PageBuf[p.i+1]	= '\t';
					}
				} else p.p->PageBuf[p.i] = '\t';
				Indent -= TabStop;
				AdvanceAndChar(&p);
				if (!p.i)
					for (c=0; c<VisiLines; ++c)
						if (!LineInfo[c].Pos.i && LineInfo[c].Pos.p==p.p) {
							do LineInfo[c].Pos.p = LineInfo[c].Pos.p->Prev;
							while (!LineInfo[c].Pos.p->Fill);
							LineInfo[c].Pos.i = LineInfo[c].Pos.p->Fill-1;
						}
			}
		}
		while (Indent--) {
			assert(p.p->PageBuf);
			Reserve(&p, 1 + (UtfEncoding == 16), 0);
			if (UtfEncoding == 16) {
				if (UtfLsbFirst) {
					p.p->PageBuf[p.i]	= ' ';
					p.p->PageBuf[p.i+1]	= '\0';
				} else {
					p.p->PageBuf[p.i]	= '\0';
					p.p->PageBuf[p.i+1]	= ' ';
				}
			} else p.p->PageBuf[p.i] = ' ';
			AdvanceAndChar(&p);
		}
		++LineCount;
		if (LineStartPos.p != EolPos.p) {
			LineStartPos = EolPos;
			AdvanceAndChar(&LineStartPos);
		}
		if (ComparePos(&LineStartPos, &ScreenStart) >= 0)
			InvalidateArea(&LineStartPos, 1, 9);
		UpdateWindow(hwndMain);
		c = 0;
		while (More) {
			INT cnt;

			if (UtfEncoding) cnt = CountBytes(&EolPos);
			if (Amount-- < 0 && Advance == AdvanceAndChar) More = FALSE;
			else if (CharFlags(c = (*Advance)(&EolPos)) & 1) {
				if (Advance == AdvanceAndChar) {
					if (Amount < 0) More = FALSE;
					++CurrLine;
					/*...CurrLine update for Reserve()->CorrectLineInfo()...*/
				} else if (CurrLine > 0) --CurrLine;
				break;
			} else if (UtfEncoding == 8)
				Amount -= abs(CountBytes(&EolPos) - cnt) - 1;
		}
		if (c == C_CRLF && UtfEncoding != 8) --Amount;
	} while (More);
	CurrLine = SaveCurrLine;
	if (Flags & 4) {
		while (ComparePos(&EolPos, &LineStartPos) < 0) CharAndAdvance(&EolPos);
		NewPosition(&EolPos);
	} else {
		CurrPos = LineInfo[CurrLine].Pos;
		Position(1, '^', (Flags & 1) | 0x400);
	}
	if (LineCount > 1) {
		extern WCHAR BufferW[300];

		LOADSTRINGW(hInst, 334 + !(Flags&8), BufferW+100, WSIZE(BufferW)-100);
		if (BufferW[100]) {
			_snwprintf(BufferW, 100, BufferW+100, LineCount);
			NewStatus(0, BufferW, NS_NORMAL);
		}
	}
	return (TRUE);
}
