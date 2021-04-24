/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2010 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      3-Dec-2000	first publication of source code
 *      6-Dec-2000	column address ('|') with values greater than maxint(16-bit)
 *     27-Jun-2002	Ctrl+E in hex mode in the near of EOF validates position
 *     29-Sep-2002	% positioning for EBCDIC
 *      4-Oct-2002	new positioning command CTRL('p')
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *      9-Jan-2010	Ctrl+End positioning to end of last line
 */

#include <windows.h>
#include <ctype.h>
#include <string.h>
#include <wchar.h>
#include "myassert.h"
#include "winvi.h"
#include "page.h"

extern INT		TfCommandChar,  TfSearchChar, QuoteChar;
extern BOOL		ShowParenMatch, EscapeInput;
extern POSITION	MarkPos[27];

LONG			RelativePosition;

BOOL Position(LONG Repeat, INT Command, INT Flags)
{	/* Flags:	 1 = cursor may be positioned behind last character of line
	 *			 2 = no display of cursor
	 *			 4 = compound command, no positioning on display
	 *			 8 = cursor may be positioned behind last line
	 *			16 = move and change selection area
	 *		  0x20 = suppress MessageBeep
	 *		  0x40 = 'cw'/'cW': last newline/space not included
	 *		  0x80 = position within screen only (Command=='%' when ShowMatch)
	 *		 0x100 = scroll only (<Ctrl+Y>, <Ctrl+E>), no cursor movement
	 *		 0x200 = wrap around newlines (WrapPosFlag==TRUE)
	 *		 0x400 = suppress ShowMatch
	 *		 0x800 = position to next line if SkipSpaces (yank only)
	 *		0x1000 = position to end of last line (Command=='G', input Ctrl+End)
	 */
	INT 	 /*NewLine = CurrLine,*/ c, c2;
	POSITION OldPos = CurrPos;
	POSITION NewPos = CurrPos;
	BOOL     NewCurrCol = FALSE, SkipSpaces = FALSE, HalfWChar = FALSE;

	RelativePosition = 0;
	FullRowExtend	 = FALSE;
	if (!LineInfo || EscapeInput) return (FALSE);
	if (!Repeat && Command != 'G') Repeat = 1;
	if (UtfEncoding == 16 && NewPos.i & 1) {
		HalfWChar = TRUE;
		--NewPos.i;
	}
	switch (Command) {
		case 'H':
			if (--Repeat >= CrsrLines) return (FALSE);
			if (HexEditMode) NewPos = LineInfo[Repeat].Pos;
			else {
				NewPos = LineInfo[0].Pos;
				if (!Repeat) {
					do; while (!(CharFlags(c = GoBackAndChar(&NewPos)) & 1));
					if (c != C_EOF) AdvanceAndChar(&NewPos);
				} else {
					while (Repeat--) {
						do; while (!(CharFlags(c=CharAndAdvance(&NewPos)) & 1));
					}
					if (c == C_EOF) {
						do; while (!(CharFlags(c=GoBackAndChar(&NewPos)) & 1));
						if (c != C_EOF) CharAndAdvance(&NewPos);
					}
					if (ComparePos(&NewPos, &LineInfo[CrsrLines].Pos) >= 0)
						return (FALSE);
				}
				FullRowExtend = SkipSpaces = NewCurrCol = TRUE;
			}
			break;
		case 'M':
			for (c=CrsrLines; c>0 && LineInfo[c].Flags & LIF_EOF; --c) {}
			NewPos = LineInfo[c>>1].Pos;
			if (!HexEditMode) {
				do; while (!(CharFlags(c = GoBackAndChar(&NewPos)) & 1));
				if (c != C_EOF) CharAndAdvance(&NewPos);
				FullRowExtend = SkipSpaces = NewCurrCol = TRUE;
			}
			break;
		case 'L':
			for (c=CrsrLines; c>0 && LineInfo[c-1].Flags & LIF_EOF; --c) {}
			if (Repeat > c) return (FALSE);
			if (HexEditMode) NewPos = LineInfo[c-Repeat].Pos;
			else {
				c2 = (INT)Repeat;
				NewPos = LineInfo[c-1].Pos;
				do {
					do; while (!(CharFlags(c=GoBackAndChar(&NewPos)) & 1));
				} while (--Repeat);
				if (c != C_EOF) AdvanceAndChar(&NewPos);
				if (c2 > 1 && ComparePos(&NewPos, &LineInfo[0].Pos) < 0)
					return (FALSE);
				FullRowExtend = SkipSpaces = NewCurrCol = TRUE;
			}
			break;
		case 'G':
			if (Repeat) {
				BeginOfFile(&NewPos);
				while (--Repeat) {
					do c = CharAndAdvance(&NewPos);
					while (!(CharFlags(c) & 1));
					if (c == C_EOF) break;
				}
				Repeat = CharAt(&NewPos) != C_EOF;
			}
			if (!Repeat) {
				EndOfFile(&NewPos);
				if (HexEditMode) {
					if (!(Flags & 1)) GoBack(&NewPos, 1);
				} else if (!(Flags & 8)) {
					c = GoBackAndChar(&NewPos);
					if (Flags & 0x1000) {
						/*end of last line...*/
						if (CharFlags(c) & 1) {
							if (!(Flags & 1) &&
								CharFlags(c = GoBackAndChar(&NewPos)) & 1)
									c = AdvanceAndChar(&NewPos);
						} else if (Flags & 1) CharAndAdvance(&NewPos);
					} else {
						/*start of last line...*/
						do c = GoBackAndChar(&NewPos);
						while (!(CharFlags(c) & 1));
						if (c != C_EOF) CharAndAdvance(&NewPos);
					}
				}
				if (c != C_EOF) {
					if (!(Flags & 4)) {
						POSITION Pos = NewPos;

						if (HexEditMode) {
							Pos.p = LastPage;
							Pos.i = LastPage->Fill;
							GoBack(&Pos, 16*(CrsrLines-1) +
										 (((INT)CountBytes(&Pos)+15) & 15) + 1);
						} else {
							INT i = CrsrLines;

							c = 0;
							while (--i >= 0)
								do c = CharFlags(GoBackAndChar(&Pos));
								while (!(c & 1));
							if (!(c & 0x20)) CharAndAdvance(&Pos);
						}
						if (ComparePos(&ScreenStart, &Pos)) {
							HideEditCaret();
							ScreenStart = Pos;
							InvalidateText();
							NewScrollPos();
						}
					}
				}
			}
			FullRowExtend = SkipSpaces = NewCurrCol = TRUE;
			if (!(Flags & 4)) MarkPos[0] = OldPos;
			break;
		case 'e':
			if (!(Flags & 1)) CharAndAdvance(&NewPos);
			/*FALLTHROUGH*/
		case 'w':
			do {	/*no loop, for break only*/
				INT Mask;

				c = CharAt(&NewPos);
				if (CharFlags(c) & 0x11) {
					Mask = Command=='w' && Repeat==1 && Flags&4 ? 0x10 : 0x11;
					do {
						c = AdvanceAndChar(&NewPos);
						if (CharFlags(c) & 0x20) return (FALSE);
					} while (CharFlags(c) & Mask);
					if (Command=='w' && !--Repeat) break;
				}
				do {
					do c2 = AdvanceAndChar(&NewPos);
					while (!((CharFlags(c2) ^ CharFlags(c)) & 0x19));
					if (!--Repeat && (Command=='e' || Flags & 0x40
									  || (Flags & 4 && CharFlags(c2) & 1))) {
						if (!(Flags & 5)) GoBackAndChar(&NewPos);
						break;
					}
					Mask = Command=='w' && !Repeat && Flags&4 ? 0x10 : 0x11;
					while (CharFlags(c2) & Mask) {
						c2 = AdvanceAndChar(&NewPos);
						if (CharFlags(c2) & 0x20) return (FALSE);
					}
					c = c2;
				} while (Repeat);
			} while (FALSE);
			NewCurrCol = TRUE;
			break;
		case 'b':
			c = GoBackAndChar(&NewPos);
			do {
				INT Mask = Repeat==1 && Flags&4 ? 0x10 : 0x11;

				if (CharFlags(c) & 0x11) {
					do {
						c = GoBackAndChar(&NewPos);
						if (CharFlags(c) & 0x20) return (FALSE);
					} while (CharFlags(c) & Mask);
				}
				if (!(CharFlags(c) & 1)) {
					do c2 = GoBackAndChar(&NewPos);
					while (!((CharFlags(c2) ^ CharFlags(c)) & 0x39));
					c = c2;
				}
			} while (--Repeat);
			if (c != C_EOF) CharAndAdvance(&NewPos);
			NewCurrCol = TRUE;
			break;
		case 'W':
			{	INT Mask;

				c = CharAt(&NewPos);
				if (CharFlags(c) & 0x11) {
					Mask = Repeat==1 && Flags&4 ? 0x10 : 0x11;
					do {
						c = AdvanceAndChar(&NewPos);
						if (CharFlags(c) & 0x20) return (FALSE);
					} while (CharFlags(c) & Mask);
					--Repeat;
				}
				while (Repeat--) {
					while (!(CharFlags(c) & 0x11)) c = AdvanceAndChar(&NewPos);
					if (!Repeat && (Flags&0x40 || (Flags&4 && CharFlags(c)&1)))
						break;
					Mask = !Repeat && Flags&4 ? 0x10 : 0x11;
					do {
						c = AdvanceAndChar(&NewPos);
						if (CharFlags(c) & 0x20) return (FALSE);
					} while (CharFlags(c) & Mask);
				}
			}
			NewCurrCol = TRUE;
			break;
		case 'E':
			c = Flags & 1 ? CharAt(&NewPos) : AdvanceAndChar(&NewPos);
			do {
				while (CharFlags(c) & 0x11) {
					c = AdvanceAndChar(&NewPos);
					if (CharFlags(c) & 0x20) return (FALSE);
				}
				do c = AdvanceAndChar(&NewPos); while (!(CharFlags(c) & 0x11));
			} while (--Repeat);
			if (!(Flags & 1)) GoBackAndChar(&NewPos);
			NewCurrCol = TRUE;
			break;
		case 'B':
			do {
				INT Mask = Repeat==1 && Flags&4 ? 0x10 : 0x11;

				do {
					c = GoBackAndChar(&NewPos);
					if (CharFlags(c) & 0x20) return (FALSE);
				} while (CharFlags(c) & Mask);
				if (!(CharFlags(c) & 1)) {
					do c = GoBackAndChar(&NewPos);
					while (!(CharFlags(c) & 0x11));
				}
			} while (--Repeat);
			if (c != C_EOF) CharAndAdvance(&NewPos);
			NewCurrCol = TRUE;
			break;
		case ' ':
		case 'l':
			if (HexEditMode) {
				Advance(&NewPos, UtfEncoding == 16 ? 2 * Repeat : Repeat);
				if (!(Flags & 1) && CharAt(&NewPos) == C_EOF)
					GoBack(&NewPos, UtfEncoding == 16 ? 2 : 1);
			} else if (!(CharFlags(CharAt(&NewPos)) & 1) || Flags & 0x200) {
				while (Repeat--) {
					c = CharAndAdvance(&NewPos);
					if (CharFlags(CharAt(&NewPos)) & 1) {
						if (Flags & 0x200) {
							if (Flags & 1) {
								/*insert mode...*/
								if (CharAt(&NewPos) == C_EOF) {
									if (c!=C_EOF && CharFlags(c) & 1)
										if (!(Flags & 4))
											GoBackAndChar(&NewPos);
									break;
								}
							} else if (AdvanceAndChar(&NewPos) == C_EOF) {
								if (CharFlags(GoBackAndChar(&NewPos))   & 1 &&
									CharFlags(c=GoBackAndChar(&NewPos)) & 1 &&
									c != C_EOF)
										AdvanceAndChar(&NewPos);
								break;
							} else if (CharFlags(c) & 1) GoBackAndChar(&NewPos);
						} else {
							if (!(Flags & 1)) GoBackAndChar(&NewPos);
							Repeat = 0;
						}
					}
					NewCurrCol = TRUE;
				}
			}
			break;
		case '\b':
		case 'h':
			if (HexEditMode) {
				if (!HalfWChar || --Repeat > 0)
					GoBack(&NewPos, UtfEncoding == 16 ? 2 * Repeat : Repeat);
				break;
			}
			do {
				if ((c = GoBackAndChar(&NewPos)) == C_EOF) break;
				if (CharFlags(c) & 1) {
					if (!(Flags & 0x200)) {
						CharAndAdvance(&NewPos);
						break;
					}
					if (!(Flags & 1)) {
						if ((c = GoBackAndChar(&NewPos)) == C_EOF) Repeat = 0;
						else if (CharFlags(c) & 1) CharAndAdvance(&NewPos);
					}
				}
				NewCurrCol = TRUE;
			} while (--Repeat > 0);
			break;
		case CTRL('p'):
		case 'k':
			if (HexEditMode) {
				ULONG i;
				i = GoBack(&NewPos, 16*Repeat) & 15;
				if (i) Advance(&NewPos, i);
				break;
			}
			if (CurrPos.i==0 && CurrPos.p==FirstPage)
				return (FALSE);
			++Repeat;
			Command = 'k';
			/*FALLTHROUGH*/
		case '0':
			if (HexEditMode) {
				GoBack(&NewPos, CountBytes(&OldPos) & 15);
				break;
			}
			FullRowExtend = Command=='k' || Repeat>1;
			while (Repeat-- > 0) {
				do; while (!(CharFlags(c=GoBackAndChar(&NewPos)) & 1));
				if (c == C_EOF) break;
				/*--NewLine;*/
			}
			if (c != C_EOF) CharAndAdvance(&NewPos);
			if (Command == 'k') AdvanceToCurr(&NewPos, /*NewLine,*/ Flags & 1);
			else NewCurrCol = TRUE;
			break;
		case '|':
			do; while (!(CharFlags(c=GoBackAndChar(&NewPos)) & 1));
			if (c != C_EOF) CharAndAdvance(&NewPos);
			c2 = 1;	/*column count*/
			while ((LONG)(unsigned)c2 < Repeat) {
				c = CharAndAdvance(&NewPos);
				if (c == '\t') {
					--c2;
					c2 += TabStop - ((unsigned)c2 % TabStop);
				}
				++c2;
				if (CharFlags(c) & 1 || c2 > Repeat) {
					GoBackAndChar(&NewPos);
					break;
				}
			}
			if (CharFlags(CharAt(&NewPos)) & 1 && !(Flags & 1) && !HexEditMode)
				if (CharFlags(c=GoBackAndChar(&NewPos)) & 1 && c != C_EOF)
					CharAndAdvance(&NewPos);
			NewCurrCol = TRUE;
			break;
		case '$':
			if (HexEditMode) {
				Advance(&NewPos, 16 * Repeat - (CountBytes(&OldPos) & 15));
				if (!(Flags & 1)) GoBack(&NewPos, 1);
			} else {
				FullRowExtend = Repeat>1;
				for (;;) {
					while (!(CharFlags(CharAt(&NewPos)) & 1)) {
						CharAndAdvance(&NewPos);
						NewCurrCol = TRUE;
					}
					if (!--Repeat) break;
					CharAndAdvance(&NewPos);
				}
				if (NewCurrCol && !(Flags & 1)) GoBackAndChar(&NewPos);
				CurrCol.ScreenLine = CurrCol.PixelOffset = 2147480000;
			}
			NewCurrCol = FALSE;
			break;
		case '^':
			--Repeat;
			/*FALLTHROUGH*/
		case '-':
			if (HexEditMode /*&& Command=='-'*/) {
				GoBack(&NewPos, 16 * Repeat + (CountBytes(&OldPos) & 15));
			} else {
				FullRowExtend = Repeat>0;
				while (Repeat-- >= 0) {
					do; while (!(CharFlags(c=GoBackAndChar(&NewPos)) & 1));
					if (c == C_EOF) {
						if (UtfEncoding && CharAt(&NewPos) == C_BOM)
							c = 0;	/*skip BOM*/
						break;
					}
				}
				if (c != C_EOF) CharAndAdvance(&NewPos);
				SkipSpaces = TRUE;
			}
			NewCurrCol = TRUE;
			break;
		case '\r':
			if (HexEditMode) {
				Advance(&NewPos, Repeat*16 - (CountBytes(&OldPos) & 15));
				break;
			}
			/*FALLTHROUGH*/
		case '+':
			while (Repeat-- > 0) {
				do; while (!(CharFlags(c=CharAndAdvance(&NewPos)) & 1));
				if (c == C_EOF) break;
			}
			FullRowExtend = TRUE;
			if ((c = CharAt(&NewPos)) == C_EOF) return (FALSE);
			SkipSpaces = NewCurrCol = TRUE;
			break;
		case '\n':
		case CTRL('n'):
		case 'j':
			if (HexEditMode) {
				INT i = (INT)Advance(&NewPos, 16*Repeat);

				if (i) {
					if (i &= 15) GoBack(&NewPos, i);
					else if (!(Flags & 1) && CharAt(&NewPos) == C_EOF)
						GoBack(&NewPos, 16);
				}
				break;
			}
			FullRowExtend = TRUE;
			while (Repeat-- > 0) {
				do; while (!(CharFlags(c=CharAndAdvance(&NewPos)) & 1));
				if (c == C_EOF) break;
				/*++NewLine;*/
			}
			if (!(Flags & 4) && CharAt(&NewPos) == C_EOF) NewPos = OldPos;
			else AdvanceToCurr(&NewPos, /*NewLine,*/ Flags & 1);
			break;
		case CTRL('e'):	/*one line forward*/
		case CTRL('d'):	/*half screen forward*/
		case CTRL('f'):	/*full screen forward*/
			FullRowExtend = !HexEditMode;
			{	LONG i = VisiLines-1;

				if (Command == CTRL('d')) i = (i + 1) >> 1;
				if (i < 1 || Command == CTRL('e')) i = 1;
				if ((i *= Repeat) < 1) i = 1;
				if (Flags & 4) return (Position(i, 'j', Flags));
				if (LineInfo[CrsrLines].Flags & LIF_EOF) {
					if (Flags & 0x100) return (FALSE);
					if ((i += CurrLine) >= CrsrLines) i = CrsrLines;
					do {
						if (!(LineInfo[i].Flags & LIF_EOF)) break;
					} while (--i > 0);
					CurrLine = (INT)i;
				} else {
					INT ScrollMax, SavCurrLine = CurrLine;

					Repeat = i;
					if (!(ScrollMax=MaxScroll) && (ScrollMax=CrsrLines-2)<0)
						ScrollMax = 0;
					if (i <= ScrollMax) {
						/*scroll...*/
						INT  j;

						HideEditCaret();
						for (j=0; j<Repeat; ++j) {
							if (LineInfo[CrsrLines].Flags & LIF_EOF) {
								if (CurrLine >= CrsrLines-1 || Flags & 0x100)
									break;
								++CurrLine;
							} else ScrollLine(1);
						}
						NewScrollPos();
						if (Command == CTRL('e')) {
							if (Flags & 0x100) Repeat = j;
							if (SavCurrLine > Repeat)
								CurrLine = SavCurrLine - (INT)Repeat;
							else {
								CurrLine = 0;
								NewPos = LineInfo[0].Pos;
								if (HexEditMode)
									Advance(&NewPos, CountBytes(&OldPos) & 15);
								else AdvanceToCurr(&NewPos, /*0,*/ Flags & 1);
							}
						} else {
							NewPos = LineInfo[SavCurrLine].Pos;
							if (HexEditMode)
								Advance(&NewPos, CountBytes(&OldPos) & 15);
							else AdvanceToCurr(&NewPos, /*SavCurrLine,*/
											   Flags & 1);
						}
						break;
					}
					if (i < CrsrLines) ScreenStart = LineInfo[i].Pos;
					else if (CrsrLines > 1) {
						INT c;

						ScreenStart = LineInfo[CrsrLines-1].Pos;
						i -= CrsrLines-1;
						if (HexEditMode) {
							i = Advance(&ScreenStart, i << 4);
							if (i &= 15) GoBack(&ScreenStart, i);
						} else {
							while (i > 0) {
								c = CharAndAdvance(&ScreenStart);
								if (c == C_EOF) break;
								if (CharFlags(c) & 1) --i;
							}
							if (c == C_EOF) {
								GoBackAndChar(&ScreenStart);
								do; while (!(CharFlags(c=GoBackAndChar
														 (&ScreenStart)) & 1));
								if (c != C_EOF) CharAndAdvance(&ScreenStart);
							}
						}
					}
					HideEditCaret();
					InvalidateText();
					CurrLine = SavCurrLine;
					while (CurrLine && LineInfo[CurrLine].Flags & LIF_EOF)
						--CurrLine;
					if (Command == CTRL('e')) {
						while (--Repeat >= 0 && CurrLine > 0) --CurrLine;
					}
					NewScrollPos();
				}
				NewPos = LineInfo[CurrLine].Pos;
				if (HexEditMode) {
					Advance(&NewPos, CountBytes(&OldPos) & 15);
					FindValidPosition(&NewPos, (WORD)(Flags & 1 ? 6 : 4));
				} else AdvanceToCurr(&NewPos, /*CurrLine,*/ Flags & 1);
			}
			break;
		case CTRL('y'):	/*one line backward*/
		case CTRL('u'):	/*half screen backward*/
		case CTRL('b'):	/*full screen backward*/
			FullRowExtend = !HexEditMode;
			{	LONG	 i = VisiLines-1;
				INT		 c, Line = CurrLine, ScrollMax;
				POSITION Pos = ScreenStart;

				if (Command == CTRL('u')) i = (i + 1) >> 1;
				if (i < 1 || Command == CTRL('y')) i = 1;
				i *= Repeat;
				if (Flags & 4) return (Position(i, 'k', Flags));
				if (!Pos.p || (Pos.i==0 && !Pos.p->Prev)) {
					if (Command == CTRL('y') || !CurrLine) return (FALSE);
					return (Position(i, 'k', Flags));
				}
				if (HexEditMode) {
					Repeat = GoBack(&Pos, i << 4) >> 4;
					i -= Repeat;
				} else {
					Repeat = -1;
					++i;
					while (--i >= 0) {
						++Repeat;
						do; while (!(CharFlags(c = GoBackAndChar(&Pos)) & 1));
						if (c == C_EOF) break;
					}
					if (c != C_EOF) CharAndAdvance(&Pos);
				}
				HideEditCaret();
				if (!(ScrollMax=MaxScroll) && (ScrollMax=CrsrLines-2)<0)
					ScrollMax = 0;
				if (Repeat <= ScrollMax) {
					/*scroll...*/
					INT j, ThisLine = CurrLine;
					/*CurrLine may change in ScrollLine()*/

					for (j=0; j<Repeat; ++j) ScrollLine(-1);
					NewScrollPos();
					if (Command == CTRL('y')) {
						if (ThisLine+Repeat < CrsrLines) {
							if (!(Flags & 0x100))
								CurrLine = ThisLine + (INT)Repeat;
							break;
						}
						ThisLine = CrsrLines-1;
					} else if (i > 0) {
						if (ThisLine < i) ThisLine = 0;
						else ThisLine -= (INT)i;
					}
					Line = ThisLine;
				} else {
					/*jump...*/
					ScreenStart = Pos;
					InvalidateText();
					NewScrollPos();
					if (Command == CTRL('y')) {
						if (Line + Repeat < CrsrLines-1) {
							CurrLine += (INT)Repeat;
							break;
						}
						Line = CrsrLines-1;
					} else if (i > 0) {
						if (Line < i) Line = 0;
						else Line -= (INT)i;
					}
				}
				NewPos = LineInfo[Line].Pos;
				if (HexEditMode) Advance(&NewPos, CountBytes(&OldPos) & 15);
				else AdvanceToCurr(&NewPos, /*Line,*/ Flags & 1);
			}
			break;
		case 'n': /*search again in same direction*/
		case 'N': /*search again in opposite direction*/
			do if (!SearchAndStatus(&NewPos, (WORD)(Command == 'N')))
				return (FALSE);
			while (--Repeat);
			NewCurrCol = TRUE;
			if (!(Flags & 4)) MarkPos[0] = OldPos;
			FullRowExtend = TRUE;
			break;
		case 'T':
		case 'F':
			if (Command=='T' && CharFlags(c = GoBackAndChar(&NewPos)) & 1)
				return (FALSE);
			do {
				do if (CharFlags(c = GoBackAndChar(&NewPos))&1) return (FALSE);
				while (c != TfSearchChar);
			} while (--Repeat);
			if (Command == 'T') CharAndAdvance(&NewPos);
			NewCurrCol = TRUE;
			break;
		case 't':
		case 'f':
			if (CharFlags(CharAt(&NewPos)) & 1
				|| (Command=='t' && CharFlags(AdvanceAndChar(&NewPos)) & 1))
					return (FALSE);
			{	INT sc = TfSearchChar;

				switch (CharSet) {
					case CS_EBCDIC:
						if (sc < 0x100) sc = MapAnsiToEbcdic[sc];
						break;
					case CS_OEM:
						sc = UnicodeToOemChar(sc);
						break;
				}
				do {
					do {
						if (CharFlags(c = AdvanceAndChar(&NewPos))&1)
							return (FALSE);
					} while (c != sc);
				} while (--Repeat);
			}
			if (Flags & 4) {
				   if (Command == 'f') CharAndAdvance(&NewPos);
			} else if (Command == 't') GoBackAndChar(&NewPos);
			NewCurrCol = TRUE;
			break;
		case ';':		/*search character again*/
			return (Position(Repeat, TfCommandChar, Flags));
		case ',':		/*search again in opposite direction*/
			return (Position(Repeat,
							 TfCommandChar ? TfCommandChar ^ ('f'-'F') : 0,
							 Flags));
		case '[':
		case ']':
			{	BOOL Result;
				BOOL SaveWrapScan = WrapScanFlag;

				SaveMatchList();
				WrapScanFlag = FALSE;
				BuildMatchList(L"/^{", '\1', NULL); /*}*/
				if (StartSearch()) {
					do Result = SearchIt(&NewPos, (WORD)(Command!='['));
					while (Result && --Repeat);
				}
				WrapScanFlag = SaveWrapScan;
				RestoreMatchList();
				if (!Result)
					return (Position(Command!=']', 'G', Flags));
			}
			FullRowExtend = TRUE;
			break;
		case '\'':
		case '`':
			if (QuoteChar == Command)    NewPos = MarkPos[0];
			else if (isalpha(QuoteChar)) NewPos = MarkPos[QuoteChar & 31];
			else NewPos.p = NULL;
			if (NewPos.p == NULL) {
				if (!(Flags & 0x20)) MessageBeep(MB_ICONEXCLAMATION);
				return (FALSE);
			}
			if (!(Flags & 4)) MarkPos[0] = OldPos;
			if (Command == '\'') {
				do; while (!(CharFlags(c = GoBackAndChar(&NewPos)) & 1));
				if (c != C_EOF) CharAndAdvance(&NewPos);
				SkipSpaces = TRUE;
				FullRowExtend = TRUE;
			}
			NewCurrCol = TRUE;
			break;
		case '%':		/*search matching parenthesis*/
			c2 = 0;
			for (;;) {
				INT c3;

				c = c3 = CharAt(&NewPos);
				switch (c3) {
					case '(': case ')':		c2 = c3 ^ ('('^')');	 break;
					case '{': case '}':		c2 = c3 ^ ('{'^'}');	 break;
					case '[': case ']':		c2 = c3 ^ ('['^']');	 break;
				}
				if (c2) break;
				if (CharFlags(c3) & 1) return (FALSE);
				CharAndAdvance(&NewPos);
			}
			{	INT		  cc, Level = 1;
				PPOSITION pPos;

				for (cc=VisiLines; cc && !(LineInfo[cc].Flags&LIF_VALID); --cc);
				pPos = &LineInfo[cc].Pos;
				for (;;) {
					cc = c<c2 ? AdvanceAndChar(&NewPos) : GoBackAndChar(&NewPos);
					if (cc==c2 && !--Level) {
						if (Flags & 4 && !(Flags & 0x80)) {
							if (c<c2) CharAndAdvance(&NewPos);
							else {
								CharAndAdvance(&CurrPos);
								OldPos = CurrPos;
							}
						}
						break;
					}
					if (Flags & 0x80 &&
							(c<c2 ? ComparePos(&NewPos, pPos)		  >= 0
								  : ComparePos(&NewPos, &ScreenStart) <= 0))
						return (FALSE);
					if (cc == c) ++Level;
					if (CharFlags(cc) & 0x20) return (FALSE);
				}
			}
			if (!(Flags & 0x80)) NewCurrCol = TRUE;
			break;
		case '*':	/*search next occurrence of identifier...*/
		case '#':	/*search previous occurrence of identifier...*/
			{	PWSTR p;
				BOOL  SaveIgnoreCase = IgnoreCaseFlag;

				if ((p = ExtractIdentifier(&NewPos)) == NULL) return (FALSE);
				_snwprintf(CommandBuf, WSIZE(CommandBuf), L"%c\\<%s\\>",
						   Command=='*' ? '/' : '?', p);
				IgnoreCaseFlag = FALSE;
				BuildMatchList(CommandBuf, '\1', NULL);
				do if (!SearchAndStatus(&NewPos, 0)) break;
				while (--Repeat);
				IgnoreCaseFlag = SaveIgnoreCase;
				if (Repeat) return (FALSE);
				NewCurrCol = TRUE;
				if (!(Flags & 4)) MarkPos[0] = OldPos;
				FullRowExtend = TRUE;
			}
			break;
		default:
			if (!(Flags & 0x20)) MessageBeep(MB_ICONEXCLAMATION);
			return (FALSE);
	}
	if (SkipSpaces) {
		if (Flags & 4 && ComparePos(&NewPos, &OldPos) > 0) {
			while (!(CharFlags(c = CharAt(&NewPos)) & 1))
				CharAndAdvance(&NewPos);
			if (Flags & 0x800) CharAndAdvance(&NewPos);
		} else {
			while ((c = CharAt(&NewPos))=='\t' || c==' ')
				CharAndAdvance(&NewPos);
			if (!HexEditMode) FindValidPosition(&NewPos, (WORD)((Flags&1) | 4));
		}
	}
	RelativePosition = NewPos.p == OldPos.p
					   ? (LONG)NewPos.i-(INT)OldPos.i
					   : CountBytes(&NewPos)-CountBytes(&OldPos);
	if (Flags & 16) {
		/*add to selected area or subtract from...*/
		UINT Invalidate;

		if (!SelectCount) {
			SelectStart   = OldPos;
			SelectBytePos = CountBytes(&OldPos);
		}
		if (OldPos.p == SelectStart.p && OldPos.i == SelectStart.i) {
			/*changing start of selection area...*/
			if (RelativePosition > 0) {
				/*positioning forward, selection area gets smaller...*/
				LONG n;

				if ((ULONG)RelativePosition > SelectCount) {
					/*position passes end of selected area*/
					n = SelectCount;
					SelectBytePos += SelectCount;
					SelectCount    = RelativePosition - SelectCount;
					while (n + (short)SelectStart.i >
							   (short)SelectStart.p->Fill
							&& SelectStart.p->Next) {
						n -= SelectStart.p->Fill - SelectStart.i;
						SelectStart.i = 0;
						SelectStart.p = SelectStart.p->Next;
					}
					if (n+(short)SelectStart.i > (short)SelectStart.p->Fill)
						n = SelectStart.p->Fill - SelectStart.i;
					SelectStart.i += (UINT)n;
				} else {
					SelectBytePos += RelativePosition;
					SelectCount   -= RelativePosition;
					SelectStart    = NewPos;
				}
				Invalidate = 1;
			} else {
				SelectBytePos += RelativePosition;
				SelectCount   -= RelativePosition;
				SelectStart    = NewPos;
				Invalidate = 0;
			}
		} else {
			/*changing end of selection area...*/
			if (RelativePosition > 0) {
				SelectCount += RelativePosition;
				Invalidate = 0;
			} else {
				if ((ULONG)-RelativePosition > SelectCount) {
					SelectBytePos -= -RelativePosition - SelectCount;
					SelectCount    = -RelativePosition - SelectCount;
					SelectStart    = NewPos;
				} else SelectCount += RelativePosition;
				Invalidate = 1;
			}
		}
		HideEditCaret();
		if (RelativePosition > 0)
			 InvalidateArea(&OldPos,  RelativePosition, Invalidate);
		else InvalidateArea(&NewPos, -RelativePosition, Invalidate);
		if (SelectCount && Mode != InsertMode) {
			StartUndoSequence();
			Mode = InsertMode;
			SwitchCursor(SWC_TEXTCURSOR);
		}
		CheckClipboard();
		UpdateWindow(hwndMain);
	} else if (!(Flags & 4) && SelectCount) {
		Unselect();
		UpdateWindow(hwndMain);
	}
	if (!(Flags & 4)) {
		NewPosition(&NewPos);
		if (!(Flags & 2)) ShowEditCaret();
		if (ShowMatchFlag && !(Flags & 0x480) &&
				!SelectCount && Mode==CommandMode) {
			/*test and show matching parentheses...*/
			INT c = CharAt(&NewPos);

			switch (c) {
				case '(': case '[': case '{':
				case ')': case ']': case '}':
					ShowMatch(0);
			}
		}
		if (NewCurrCol) GetXPos(&CurrCol);
		Indenting = FALSE;
	}
	return (TRUE);
}
