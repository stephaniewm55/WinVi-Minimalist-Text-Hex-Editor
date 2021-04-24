/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2009 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      3-Dec-2000	first publication of source code
 *     22-Jul-2002	use of private myassert.h
 *     27-Jul-2002	new file srchdlg.c containing dialog specific parts
 *     27-Jul-2002	CheckFlag for interactive substitute confirmation
 *     30-Sep-2002	EBCDIC support
 *      1-Oct-2002	display and crash problems solved for :s/^$/xxx/c
 *     31-Jul-2007	several changes for Unicode character handling
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 */

#include <windows.h>
#include <shellapi.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include "myassert.h"
#include "winvi.h"
#include "page.h"

#define NEW_SEARCH TRUE

#if NEW_SEARCH

#define NUM_RANGES 48							/* must be at least 4	   */

#else

#define W 255
#define INCLUDE(m,c,i) if((i)<0x100) (m)[c&3][((i)&255)>>3] |=  (1<<((i)&7))
#define EXCLUDE(m,c,i) if((i)<0x100) (m)[c&3][((i)&255)>>3] &= ~(1<<((i)&7))
#define TEST(m,c,i)      ((i)<0x100&&(m)[c&3][((i)&255)>>3] &   (1<<((i)&7)))

typedef BYTE MASK[4][33];

MASK NoneMask = {{0}}, AnyMask = {
{W-1,W-4-32,W,W-64,W,W,W,W,W,W,W,W,W,W,W,W, W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W, 0},
{W-1,W-4-32,W,W-64,W,W,W,W,W,W,W,W,W,W,W,W, W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W, 0},
{W-1,W-4-32,W,W-64,W,W,W,W,W,W,W,W,W,W,W,W, W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W, 0},
};

#endif

typedef struct tagSEARCHPART {
	struct tagSEARCHPART *Next, *Prev;
	enum {SrchChoice, SrchString, SrchSpecial} Type;
	union {
#if NEW_SEARCH
		struct {
			BOOL  ContinuedInNextSP;   /*same set cont'd in next SEARCHPART*/
			BOOL  ContinuedInPrevSP;   /*same set cont'd in prev SEARCHPART*/
			BOOL  Inverse;			   /*TRUE for [^...] search operations*/
			BYTE  MatchMulti;		   /*appended '*' in pattern*/
			INT	  Fill;				   /*number of used Range[] entries*/
			WORD  ConvCSetChoice[(NUM_RANGES + 15) / 16];
			WCHAR Range[2][NUM_RANGES];/*start/end pairs, case sensitive*/
		} NewChoice;
#else
		struct {			/*not used anymore*/
			MASK  Mask;				   /*Mask[0]=ANSI, [1]=OEM, [2]=EBCDIC*/
			WCHAR SingleCase[32];	   /*relating to ANSI charset*/
			BYTE  Inverted;			   /*choice built with [^...]*/
			BYTE  MatchMulti;		   /*appended '*' in pattern*/
		} ChoiceMatch;
#endif
		struct {
			WORD  ConvertCSet[6];	   /*(sizeof(String)-1)/(8*sizeof(WORD))+1*/
			WORD  AllowCaseConvert[6]; /*(sizeof(String)-1)/(8*sizeof(WORD))+1*/
			WCHAR FirstUcase, FirstLcase;
			BYTE  Length;
			WCHAR String[96];
		} StringMatch;
		struct {
			CHAR     Char;			   /*one of: '(', '|', ')', '<', '>'*/
			POSITION Pos;			   /*matched position of "\(" / "\)"*/
			struct tagSEARCHPART *NextBranch;
			   /*circular forward pointing list of substrings divided by "\|",*/
			   /*\) entry points back to \( entry, NULL if only one choice*/
		} SpecialMatch;
	} u;
} SEARCHPART, *PSEARCHPART;
PSEARCHPART FirstMatch, FirstString, LastMatch;
BOOL		MatchAtStartOfLine, MatchAtEndOfLine, MatchForward, MatchValid;
INT			RecursiveBranch;

BOOL CheckFwd (PPOSITION, PSEARCHPART);
BOOL CheckBkwd(PPOSITION, PSEARCHPART);

extern INT OemCodePage, AnsiCodePage;

INT SingleCharAndAdvance(PPOSITION Pos) {
	INT c;

	if ((c = CharAndAdvance(Pos)) != C_CRLF)
		return c;
	GoBack(Pos, UtfEncoding == 16 ? 2 : 1);
	return '\r';
}

INT GoBackAndSingleChar(PPOSITION Pos) {
	INT c;

	if (UtfEncoding == 8 && GoBack(Pos, 1)) {
		if ((c = ByteAt(Pos)) >= 0 && c < 0x80) return c;
		Advance(Pos, 1);
		return GoBackAndChar(Pos);
	}
	if (!GoBack(Pos, UtfEncoding == 16 ? 2 : 1)) c = C_EOF;
	else if ((c = CharAt(Pos)) == C_CRLF) c = '\r';
	return c;
}

BOOL MatchSpecial(PSEARCHPART *pSrch, PPOSITION Pos, BOOL Backward) {
	INT         c;
	PSEARCHPART Srch = *pSrch;

	switch (Srch->u.SpecialMatch.Char) {
		case '<':
			if (!(CharFlags(c = CharAt(Pos))		& 8)) return FALSE;
			if (  CharFlags(c = GoBackAndChar(Pos))	& 8 ) return FALSE;
			if (c != C_EOF) CharAndAdvance(Pos);
			break;

		case '>':
			if (  CharFlags(c = CharAt(Pos))		& 8 ) return FALSE;
			if (!(CharFlags(c = GoBackAndChar(Pos))	& 8)) return FALSE;
			CharAndAdvance(Pos);
			break;

		case '|':
			while (Srch->u.SpecialMatch.NextBranch != NULL &&
				   Srch->u.SpecialMatch.Char != (Backward ? '(' : ')'))
				Srch = Srch->u.SpecialMatch.NextBranch;
			*pSrch = Srch;
			/*FALLTHROUGH*/
		case '(':
		case ')':
			Srch->u.SpecialMatch.Pos = *Pos;
			if (Srch->u.SpecialMatch.NextBranch != NULL) {
				POSITION PosSav;

				PosSav = *Pos;
				if (Backward) {
					if (Srch->u.SpecialMatch.Char == '(') break;
					Srch = Srch->u.SpecialMatch.NextBranch;
					assert(Srch != NULL);
					assert(Srch->u.SpecialMatch.Char == '(');
					Srch = Srch->u.SpecialMatch.NextBranch;
					assert(Srch != NULL);
					assert(Srch->u.SpecialMatch.Char == '|');
					++RecursiveBranch;
					while (!CheckBkwd(Pos, Srch)) {
						Srch = Srch->u.SpecialMatch.NextBranch;
						assert(Srch != NULL);
						if (Srch->u.SpecialMatch.Char == '(') {
							--RecursiveBranch;
							return FALSE;
						}
						*Pos = PosSav;
					}
				} else {
					if (Srch->u.SpecialMatch.Char == ')') break;
					++RecursiveBranch;
					while (!CheckFwd(Pos, Srch->Next)) {
						Srch = Srch->u.SpecialMatch.NextBranch;
						assert(Srch != NULL);
						if (Srch->u.SpecialMatch.Char == ')') {
							--RecursiveBranch;
							return FALSE;
						}
						*Pos = PosSav;
					}
				}
				--RecursiveBranch;
				while (Srch->u.SpecialMatch.Char != ')')
					Srch = Srch->u.SpecialMatch.NextBranch;
				if (Backward)
					Srch = Srch->u.SpecialMatch.NextBranch;
				*pSrch = Srch;
			}
			break;
	}
	return TRUE;
}

extern WCHAR const MapLowOemToUtf16[33];

WCHAR CharSetToUnicode(WCHAR c)
{	BYTE ch;

	switch (CharSet) {
		case CS_EBCDIC:	c = c >= 0x100 ? c : MapEbcdicToAnsi[c];
						break;

		case CS_ANSI:	if (c >= 0x80) {
							ch = (BYTE)c;
							MultiByteToWideChar(AnsiCodePage, 0, &ch, 1, &c, 1);
						}
						break;

		case CS_OEM:	if (c >= ' ' && c <= '~') break;
						if (c > 0 && c < ' ') c = MapLowOemToUtf16[c];
						else {
							ch = (BYTE)c;
							MultiByteToWideChar(OemCodePage, 0, &ch, 1, &c, 1);
						}
						break;
	}
	return c;
}

BYTE MapUnicodeToOem(WCHAR Char)
{	BYTE   c;
	PWCHAR p;

	if (Char >= ' ') {
		if (Char <= '~') return (BYTE)Char;
		if ((p = wcschr(MapLowOemToUtf16, Char)) != NULL)
			return p - MapLowOemToUtf16;
	}
	if (WideCharToMultiByte(OemCodePage, 0, &Char, 1, &c, 1, NULL, NULL) != 1)
		return '?';
	return c;
}

BYTE TestBit(PSEARCHPART pS, INT Char, BOOL Back)
{
#if NEW_SEARCH
	INT		i;
	DWORD	Cp;
	CHAR	c;
	WCHAR	wc;

	if (Char < 0)
		return FALSE;
	c  = Char;
	Cp = AnsiCodePage;
	switch (CharSet) {
		case CS_OEM:		wc = CharSetToUnicode(Char);
							break;
		case CS_EBCDIC:
		case CS_ANSI:		MultiByteToWideChar(Cp, 0, &c, 1, &wc, 1);
							break;
		case CS_UTF8:
		case CS_UTF16LE:
		case CS_UTF16BE:	wc = Char;
							break;
	}
	do {
		for (i=0; i<pS->u.NewChoice.Fill; ++i) {
			if (pS->u.NewChoice.ConvCSetChoice[i>>4] & (i&15)) {
				if (wc >= pS->u.NewChoice.Range[0][i] &&
					wc <= pS->u.NewChoice.Range[1][i])
						return !pS->u.NewChoice.Inverse;
			} else {
				if (Char >= pS->u.NewChoice.Range[0][i] &&
					Char <= pS->u.NewChoice.Range[1][i])
						return !pS->u.NewChoice.Inverse;
			}
		}
		if (Back) {
			if (!pS->u.NewChoice.ContinuedInPrevSP)
				break;
			pS = pS->Prev;
		} else {
			if (!pS->u.NewChoice.ContinuedInNextSP)
				break;
			pS = pS->Next;
		}
	} while (pS != NULL && pS->Type == SrchChoice);
	return pS->u.NewChoice.Inverse;
#else
	BYTE		 Result;
	static WCHAR c[2];

	if (Char < 255) {
		switch (CharSet) {
			case CS_OEM:
				if ((Result = TEST(pS->u.ChoiceMatch.Mask, 1, Char) != 0)
						!= pS->u.ChoiceMatch.Inverted)
					return Result;
				break;
			case CS_EBCDIC:
				if ((Result = TEST(pS->u.ChoiceMatch.Mask, 2, Char) != 0)
						!= pS->u.ChoiceMatch.Inverted)
					return Result;
				break;
		}
	}
	c[0] = CharSetToUnicode(Char);
	if (c[0] > 255) return pS->u.ChoiceMatch.Inverted;
	if ((Result = TEST(pS->u.ChoiceMatch.Mask, 0, c[0]) != 0)
			!= pS->u.ChoiceMatch.Inverted || !IgnoreCaseFlag)
		return Result;
	if (IsCharLowerW(c[0]))	CharUpperBuffW(c, 1);
	else					CharLowerBuffW(c, 1);
	if (c[0] > 255) return 0;
	if (pS->u.ChoiceMatch.SingleCase[c[0] >> 3] & (1 << (c[0] & 7)))
		return Result;
	return TEST(pS->u.ChoiceMatch.Mask, 0, c[0]);
#endif
}

BOOL CheckFwd(PPOSITION Pos, PSEARCHPART Srch)
{
	while (Srch) {
		switch (Srch->Type) {
			case SrchString:
				{	PWCHAR pCmp	= Srch->u.StringMatch.String;
					INT    Len	= Srch->u.StringMatch.Length;
					INT    i	= 0;

					while (Len--) {
						INT c;

						if (Srch->u.StringMatch.ConvertCSet[i>>4] &
									(1 << (i & 15))) {
							c = SingleCharAndAdvance(Pos);
							if (CharSet == CS_ANSI || CharSet == CS_OEM)
								c = CharSetToUnicode(c);
						} else {
							c = ByteAndAdvance(Pos);
							if (c >= 0 && UtfEncoding == 16)
								if (UtfLsbFirst)
									 c |= ByteAndAdvance(Pos) << 8;
								else c = (c << 8) | ByteAndAdvance(Pos);
						}
						if (c < 0) return FALSE;
						if (c != *pCmp) {
							if (!(Srch->u.StringMatch.AllowCaseConvert[i>>4] &
									   (1 << (i & 15))) ||
									!IgnoreCaseFlag || LOWER_CMP(c, *pCmp))
								return FALSE;
						}
						++i;
						++pCmp;
					}
				}
				break;

#if NEW_SEARCH
			case SrchChoice:
				if (Srch->u.NewChoice.MatchMulti) {
					INT i = 0, c;

					if ((c = CharAt(Pos)) == C_CRLF) c = '\r';
					while (TestBit(Srch, c, FALSE)) {
						++i;
						if (c == '\r') {
							Advance(Pos, UtfEncoding == 16 ? 2 : 1);
							c = CharAt(Pos);
						} else c = AdvanceAndChar(Pos);
						if (c == C_CRLF) c = '\r';
					}
					do {
						POSITION SavePos;

						SavePos = *Pos;
						if (!Srch->Next || CheckFwd(Pos, Srch->Next))
							return (!MatchAtEndOfLine ||
									CharFlags(CharAt(Pos)) & 0x21);
						*Pos = SavePos;
						GoBack(Pos, UtfEncoding == 16 ? 2 : 1);
					} while (i--);
					return (FALSE);
				} else {
					INT c = SingleCharAndAdvance(Pos);

					if (!TestBit(Srch, c, FALSE)) return FALSE;
				}
				break;
#else
			case SrchChoice:
				if (Srch->u.ChoiceMatch.MatchMulti) {
					INT i = 0, c;

					if ((c = CharAt(Pos)) == C_CRLF) c = '\r';
					while (TestBit(Srch, c, FALSE)) {
						++i;
						Advance(Pos, UtfEncoding == 16 ? 2 : 1);
						if ((c = CharAt(Pos)) == C_CRLF) c = '\r';
					}
					do {
						POSITION SavePos;

						SavePos = *Pos;
						if (!Srch->Next || CheckFwd(Pos, Srch->Next))
							return (!MatchAtEndOfLine ||
									CharFlags(CharAt(Pos)) & 0x21);
						*Pos = SavePos;
						GoBack(Pos, UtfEncoding == 16 ? 2 : 1);
					} while (i--);
					return (FALSE);
				} else {
					INT c = SingleCharAndAdvance(Pos);

					if (!TestBit(Srch, c, FALSE)) return FALSE;
				}
				break;
#endif
			case SrchSpecial:
				{	BOOL RecursivelyCalled = Srch->u.SpecialMatch.Char!='(';

					if (!MatchSpecial(&Srch, Pos, FALSE)) return FALSE;
					if (RecursiveBranch && Srch->u.SpecialMatch.NextBranch)
						if (RecursivelyCalled) return TRUE;
				}
				break;
		}
		Srch = Srch->Next;
	}
	{	INT c;

		return !MatchAtEndOfLine || (c = CharFlags(CharAt(Pos)) & 0x21) == 1
			   || (c & 1 &&
				   (GoBackAndChar(Pos), !(CharFlags(CharAndAdvance(Pos)) & 1)));
	}
}

BOOL MinimumMatchBkwd;

BOOL CheckBkwd(PPOSITION Pos, PSEARCHPART Srch)
{
	/*Current Srch need not be checked, check preceeding chain elements only.
	 */
	if (Srch) while ((Srch = Srch->Prev) != NULL) {
		INT c;

		switch (Srch->Type) {
			case SrchString:
				{	PWCHAR pCmp = Srch->u.StringMatch.String;
					INT    Len  = Srch->u.StringMatch.Length;

					pCmp += Len-1;
					while (Len--) {
						if (Srch->u.StringMatch.ConvertCSet[Len>>4] &
									(1 << (Len & 15))) {
							c = GoBackAndSingleChar(Pos);
							if (CharSet == CS_ANSI || CharSet == CS_OEM)
								c = CharSetToUnicode(c);
						} else {
							c = GoBackAndByte(Pos);
							if (c >= 0 && UtfEncoding == 16)
								if (UtfLsbFirst)
									 c = (c << 8) | GoBackAndByte(Pos);
								else c |= GoBackAndByte(Pos) << 8;
						}
						if (c < 0) return FALSE;
						if (c != *pCmp) {
							if (!(Srch->u.StringMatch.AllowCaseConvert[Len>>4] &
									   (1 << (Len & 15))) ||
									!IgnoreCaseFlag || LOWER_CMP(c, *pCmp))
								return FALSE;
						}
						--pCmp;
					}
				}
				break;
#if NEW_SEARCH
			case SrchChoice:
				if (Srch->u.NewChoice.MatchMulti) {
					INT		 i = 0;
					POSITION SavePos;

					if (MinimumMatchBkwd) {
						for (;;) {
							SavePos = *Pos;
							if (CheckBkwd(Pos, Srch)) return TRUE;
							*Pos = SavePos;
							c = GoBackAndSingleChar(Pos);
							if (!TestBit(Srch, c, TRUE)) return FALSE;
						}
					}
					for (;;) {
						c = GoBackAndSingleChar(Pos);
						if (!TestBit(Srch, c, TRUE)) break;
						++i;
					}
					do {
						if (c == C_EOF) c = 0;
						else Advance(Pos, UtfEncoding == 16 ? 2 : 1);
						SavePos = *Pos;
						if (CheckBkwd(Pos, Srch)) return TRUE;
						*Pos = SavePos;
					} while (i--);
					return (FALSE);
				} else {
					c = GoBackAndSingleChar(Pos);
					if (!TestBit(Srch, c, TRUE)) return FALSE;
				}
				break;
#else
			case SrchChoice:
				if (Srch->u.ChoiceMatch.MatchMulti) {
					INT		 i = 0;
					POSITION SavePos;

					if (MinimumMatchBkwd) {
						for (;;) {
							SavePos = *Pos;
							if (CheckBkwd(Pos, Srch)) return TRUE;
							*Pos = SavePos;
							c = GoBackAndSingleChar(Pos);
							if (!TestBit(Srch, c, TRUE)) return FALSE;
						}
					}
					for (;;) {
						c = GoBackAndSingleChar(Pos);
						if (!TestBit(Srch, c, TRUE)) break;
						++i;
					}
					do {
						if (c == C_EOF) c = 0;
						else Advance(Pos, UtfEncoding == 16 ? 2 : 1);
						SavePos = *Pos;
						if (CheckBkwd(Pos, Srch)) return TRUE;
						*Pos = SavePos;
					} while (i--);
					return (FALSE);
				} else {
					c = GoBackAndSingleChar(Pos);
					if (!TestBit(Srch, c, TRUE)) return FALSE;
				}
				break;
#endif
			case SrchSpecial:
				{	BOOL RecursivelyCalled = Srch->u.SpecialMatch.Char!=')';

					if (!MatchSpecial(&Srch, Pos, TRUE)) return FALSE;
					if (RecursiveBranch && Srch->u.SpecialMatch.NextBranch)
						if (RecursivelyCalled) return TRUE;
				}
				break;
		}
	}
	if (MatchAtStartOfLine) {
		INT c;

		if (Pos->i && Pos->p->PageBuf) {
			c = (BYTE)Pos->p->PageBuf[Pos->i-1];
			if ((c=='\r' && CharAt(Pos)=='\n') ||
				(Pos->i==Pos->p->Fill && CharAt(Pos)==C_EOF)) return FALSE;
		} else {
			if (GoBack(Pos, UtfEncoding == 16 ? 2 : 1) != 1) return TRUE;
			c = CharAt(Pos);
			Advance(Pos, UtfEncoding == 16 ? 2 : 1);
			if (c==C_CRLF /*position inbetween*/ || CharAt(Pos)==C_EOF)
				return FALSE;
		}
		if (!(CharFlags(c) & 0x21)) return FALSE;
	}
	return TRUE;
}

LONG StartTime;

BOOL CheckInterrupt(void) {
	LONG Time;

	Time = GetCurrentTime();
	if (Disabled) MessageLoop(FALSE);
	else if (Time-StartTime > 200) Disable(1);
	return (Interrupted);
}

VOID StartInterruptCheck(void) {
	Interrupted = FALSE;
	StartTime = GetCurrentTime();
}

INT		 OffsetTo1stString;
BOOL	 RescanMaximumMatch, Wrapped;
POSITION StartPos, EndMatch;

#if defined(__LCC__)
wchar_t* wmemchr(const wchar_t *s, wchar_t c, size_t n)
{	PBYTE	p1, p2;

	if (n == 0) return NULL;
	p1 = (PBYTE)s;
	n += n - 1;
	do {
		if ((p2 = _fmemchr(p1, c, n)) == NULL) return NULL;
		if (!((p2 - (PBYTE)s) & 1) && p2[1] == (c >> 8))
			return (wchar_t *)p2;
		n -= p2 - p1 + 1;
		p1 = p2 + 1;
	} while (n > 0);
	return NULL;
}
#endif

BOOL SearchIt(PPOSITION pPos, INT Flags) {
	/*pPos:  on input, it specifies the start position for search
	 *		 on return, it marks the start of the area found
	 *Flags: 1=forward
	 *		 2=don't advance position before search
	 *		 4=don't pre-advance 2 positions at end of line (insert mode)
	 */
	POSITION Pos;

	Wrapped = FALSE;
	if (CheckInterrupt()) return FALSE;
	Pos = *pPos;
	MinimumMatchBkwd = FALSE;
	{	INT c;
		BOOL Done = FALSE;
		INT (*Next)(PPOSITION) =
			Flags & 1 ? SingleCharAndAdvance : GoBackAndSingleChar;

		if (!(Flags & 2)) {
			if (CharFlags((c = (*Next)(&Pos))) & 0x21) {
				if (c == C_EOF || c == C_ERROR || c == C_UNDEF || FirstString) {
					if (!(Flags & 1) && c != C_EOF)
						Advance(&Pos, UtfEncoding == 16 ? 2 : 1);
					Done = TRUE;
				}
				if (c=='\r' && CharAt(&Pos)=='\n') CharAndAdvance(&Pos);
			} else if ((Flags & 5) == 1 && CharFlags(CharAt(&Pos)) & 1)
				CharAndAdvance(&Pos);
		}
		if (!Done) for (;;) {
			EndMatch = Pos;
			if (CheckFwd(&EndMatch,FirstMatch) && CheckBkwd(&Pos,FirstMatch)) {
				*pPos = Pos;
				return TRUE;
			}
			if (Done) break;
			if (CharFlags(c = (*Next)(&Pos)) & 0x21) {
				if (CheckInterrupt()) return FALSE;
				if (c == C_EOF || c == C_ERROR || c == C_UNDEF || FirstString)
					Done = TRUE;
			}
		}
		if (c == C_EOF || c == C_ERROR || c == C_UNDEF) {
			if (c != C_EOF || !WrapScanFlag) return FALSE;
			Wrapped = TRUE;
			if (Flags & 1) {
				Pos.p = FirstPage;
				Pos.i = 0;
			} else {
				Pos.p = LastPage;
				Pos.i = LastPage->Fill;
			}
			CharAt(&Pos);
			if (!FirstString) for (;;) {
				if (Pos.p == CurrPos.p && Pos.i == CurrPos.i) return (FALSE);
				EndMatch = Pos;
				if (CheckFwd(&EndMatch, FirstMatch) &&
						CheckBkwd(&Pos, FirstMatch)) {
					*pPos = Pos;
					return TRUE;
				}
				if (CharFlags(c = (*Next)(&Pos)) & 0x21)
					if (CheckInterrupt()) return FALSE;
			}
		}
	}
	{	WCHAR Ucase = FirstString->u.StringMatch.FirstUcase;
		WCHAR Lcase = FirstString->u.StringMatch.FirstLcase;

		if (UtfEncoding == 16 && !UtfLsbFirst) {
			Ucase = (Ucase << 8) | (Ucase >> 8);
			Lcase = (Lcase << 8) | (Lcase >> 8);
		}
		CharAt(&Pos);	/*page must be present in memory*/
		if (Flags & 1) {
			/*search forward*/
			LPSTR lpUc, lpLc;

			for (;;) {
				if (Pos.i >= Pos.p->Fill) {
					Pos.i = 0;
					if ((Pos.p = Pos.p->Next) == NULL) {
						if (!WrapScanFlag) return FALSE;
						Pos.p = FirstPage;
						Wrapped = TRUE;
					}
					if (CheckInterrupt()) return FALSE;
					if (!Pos.p->PageBuf && !ReloadPage(Pos.p)) return FALSE;
				}
				if (UtfEncoding == 16)
					 lpUc = (LPSTR)wmemchr((PWSTR)(Pos.p->PageBuf + Pos.i),
					 					   Ucase,
					 					   (Pos.p->Fill - Pos.i)/sizeof(WCHAR));
				else lpUc = _fmemchr(Pos.p->PageBuf + Pos.i,
								     Ucase, Pos.p->Fill - Pos.i);
				if (Lcase == Ucase) lpLc = NULL;
				else if (UtfEncoding == 16)
					 lpLc = (LPSTR)wmemchr((PWSTR)(Pos.p->PageBuf + Pos.i),
					 					   Lcase,
					 					   (Pos.p->Fill - Pos.i)/sizeof(WCHAR));
				else lpLc = _fmemchr(Pos.p->PageBuf + Pos.i,
									 Lcase, Pos.p->Fill - Pos.i);
				if (lpUc || lpLc) {
					do {
						LPSTR lp;

						if (lpUc && lpLc) lp = lpUc < lpLc ? lpUc : lpLc;
						else lp = lpUc ? lpUc : lpLc;
						Pos.i = (LPBYTE)lp - Pos.p->PageBuf;
						if (Wrapped && Pos.i >= CurrPos.i && Pos.p == CurrPos.p)
							return FALSE;
						EndMatch = Pos;
						if (CheckFwd(&EndMatch, FirstString)) {
							POSITION ChkBack;

							ChkBack = Pos;
							if (CheckBkwd(&ChkBack, FirstString)) {
								if (Wrapped==(ComparePos(&EndMatch, pPos)<=0)) {
									*pPos = ChkBack;
									if (!RescanMaximumMatch) return TRUE;
									EndMatch = ChkBack;
									return (CheckFwd(&EndMatch, FirstMatch));
								}
							}
						}
						if ((Pos.i += UtfEncoding == 16 ? 2 : 1) >= Pos.p->Fill)
							if (lp == lpUc) lpUc = NULL;
							else lpLc = NULL;
						else if (lp == lpUc) {
							if (UtfEncoding == 16)
								 lpUc = (LPSTR)wmemchr((PWSTR)
								 					   (Pos.p->PageBuf + Pos.i),
													   Ucase,
													   (Pos.p->Fill - Pos.i)
														   /sizeof(WCHAR));
							else lpUc = _fmemchr(Pos.p->PageBuf + Pos.i,
												 Ucase,
												 Pos.p->Fill - Pos.i);
						} else if (UtfEncoding == 16)
							 lpLc = (LPSTR)wmemchr((PWSTR)
							 					   (Pos.p->PageBuf + Pos.i),
												   Lcase,
												   (Pos.p->Fill - Pos.i)
													   /sizeof(WCHAR));
						else lpLc = _fmemchr(Pos.p->PageBuf + Pos.i,
											 Lcase,
											 Pos.p->Fill - Pos.i);
					} while (lpUc || lpLc);
					Pos.i += UtfEncoding == 16 ? 2 : 1;
				} else {
					if (Pos.p == CurrPos.p && Wrapped) return FALSE;
					Pos.i = Pos.p->Fill;
				}
			}
		} else {
			/*search backward*/
			LPSTR lp;

			MinimumMatchBkwd = TRUE;
			for (;;) {
				BYTE c;

				while (!Pos.i) {
					if ((Pos.p = Pos.p->Prev) == NULL) {
						if (!WrapScanFlag) return FALSE;
						Pos.p = LastPage;
						Wrapped = TRUE;
					}
					Pos.i = Pos.p->Fill;
					if (CheckInterrupt()) return FALSE;
				}
				if (!Pos.p->PageBuf && !ReloadPage(Pos.p)) return FALSE;
				lp = NULL;
				do {
					c = Pos.p->PageBuf[--Pos.i];
					if (c == Ucase || c == Lcase) {
						lp = Pos.p->PageBuf + Pos.i;
						break;
					}
				} while (Pos.i);
				if (lp) {
					Pos.i = (LPBYTE)lp - Pos.p->PageBuf;
					if (Wrapped && Pos.i <= CurrPos.i && Pos.p == CurrPos.p)
						return FALSE;
					EndMatch = Pos;
					if (CheckFwd(&EndMatch, FirstString)) {
						POSITION ChkBack;

						ChkBack = Pos;
						if (CheckBkwd(&ChkBack, FirstString)) {
							if (Wrapped == (ComparePos(&ChkBack, pPos)>=0)) {
								*pPos = ChkBack;
								if (!RescanMaximumMatch) return TRUE;
								EndMatch = ChkBack;
								return CheckFwd(&EndMatch, FirstMatch);
							}
						}
					}
				} else if (Pos.p == CurrPos.p && Wrapped) return FALSE;
			}
		}
	}
}

INT SearchErrNo = 216;

BOOL StartSearch(void) {
	if (!MatchValid) {
		if (SearchErrNo == (TerseFlag ? 216 : 217)) SearchErrNo ^= 216^217;
		Error(SearchErrNo);
		return (FALSE);
	}
	StartInterruptCheck();
	return (TRUE);
}

WCHAR SrchDispBuf[100];

BOOL SearchAndStatus(PPOSITION Pos, WORD Flags) {
	/*Flags: 1=Reverse search
	 *		 2=don't advance position before search
	 */
	BOOL Return, Forward = MatchForward ^ (Flags & 1);

	if (!StartSearch()) return (FALSE);
	if (*SrchDispBuf != (Forward ? '/' : '?')) {
		INT	  c, len;
		PWSTR p = SrchDispBuf;

		c = *SrchDispBuf ^= '/' ^ '?';
		for (;;) {
			do if (*p++=='\\' && *p) ++p;
			while (*p && *p!=c);
			if (!*p) break;
			len = wcslen(p) + 1;
			if (len >= (INT)WSIZE(SrchDispBuf) - (p-SrchDispBuf)) {
				--len;
				wcscpy(SrchDispBuf + WSIZE(SrchDispBuf) - 5, L"...");
			}
			_fmemmove(p+1, p, len*sizeof(WCHAR));
			*p = '\\';
		}
	}
	NewStatus(0, SrchDispBuf, NS_NORMAL);
	Flags &= 2;
	if (Forward) Flags |= Mode==InsertMode ? 5 : 1;
	DragAcceptFiles(hwndMain, FALSE);
	if (!SearchIt(Pos, Flags)) {
		Error(Interrupted  ? 218 :
			  WrapScanFlag ? 214 :
			  Forward	   ? 210 :
							 212);
		if (Disabled) Enable();
		ShowEditCaret();
		Return = FALSE;
	} else {
		BOOL OldSupp = SuppressStatusBeep;

		SuppressStatusBeep = TRUE;
		if (Wrapped) Error(Forward ? 258 : 259);
		SuppressStatusBeep = OldSupp;
		if (Disabled) Enable();
		StartPos = *Pos;
		Return = TRUE;
	}
	DragAcceptFiles(hwndMain, TRUE);
	EnableToolButton(IDB_SRCHAGN, TRUE);
	FindValidPosition(Pos, (WORD)((Mode==InsertMode) | 4));
	return (Return);
}

PSEARCHPART NewSearchPart(void)
{	PSEARCHPART p;

	if ((p = calloc(sizeof(SEARCHPART), 1)) == NULL) {
		MatchValid = FALSE;
		return (NULL);
	}
	if (LastMatch) {
		LastMatch->Next = p;
		p->Prev = LastMatch;
		LastMatch = p;
	} else {
		p->Prev = NULL;
		FirstMatch = LastMatch = p;
	}
	p->Next = NULL;
	return (p);
}

#if NEW_SEARCH

PSEARCHPART CreateChoice(void)
{	PSEARCHPART p;

	if (!MatchValid || (p = NewSearchPart()) == NULL)
		return NULL;
	p->Type = SrchChoice;
	p->u.NewChoice.ContinuedInNextSP =
	p->u.NewChoice.ContinuedInPrevSP = FALSE;
	p->u.NewChoice.MatchMulti = p->u.NewChoice.Inverse = 0;
	p->u.NewChoice.Fill = 0;
	return p;
}

#else

void NewChoice(MASK Mask)
{	PSEARCHPART p;

	if (!MatchValid) return;
	if ((p = NewSearchPart()) == NULL) return;
	p->Type = SrchChoice;
	memcpy(p->u.ChoiceMatch.Mask, Mask, sizeof(p->u.ChoiceMatch.Mask));
	memset(p->u.ChoiceMatch.SingleCase, -1,sizeof(p->u.ChoiceMatch.SingleCase));
	p->u.ChoiceMatch.MatchMulti = p->u.ChoiceMatch.Inverted = 0;
}

#endif

BOOL MagicFlag = TRUE;
INT  Level;

void EnterMatch(WCHAR Char, PWSTR Asterisk, UINT Flags)
	/*values of Flags:
	 *	0:	take byte literally,
	 *	1:	do character set conversion
	 *	2:	allow upper and lower case match if IgnoreCase is set
	 *	3:	character set conversion and both upper and lower case
	 */
{	WCHAR ch[2];

	if (!MatchValid) return;
	ch[0] = ch[1] = Char;
	if (MagicFlag ? Asterisk[1]=='*'
				  : Asterisk[1]=='\\' && Asterisk[2]=='*') {
#if NEW_SEARCH
		if (CreateChoice() != NULL) {
			LastMatch->u.NewChoice.Range[0][0] =
			LastMatch->u.NewChoice.Range[1][0] = Char;
			++LastMatch->u.NewChoice.Fill;
			if (Flags & 2) {
				CharUpperBuffW(ch, 1);
				if (ch[0] == Char)
					CharLowerBuffW(ch, 1);
				if (ch[0] != Char) {
					LastMatch->u.NewChoice.Range[0][1] =
					LastMatch->u.NewChoice.Range[1][1] = ch[0];
					++LastMatch->u.NewChoice.Fill;
				}
			}
			LastMatch->u.NewChoice.MatchMulti = 1;
			LastMatch->u.NewChoice.ConvCSetChoice[0] |= Flags & 1;
		}
#else
		NewChoice(NoneMask);
		INCLUDE(LastMatch->u.ChoiceMatch.Mask, 0, Char);
		if (Flags & 1) {
			ch[0] = MapUnicodeToOem(ch[0]);
			ch[1] = MapAnsiToEbcdic[Char & 255];
		}
		INCLUDE(LastMatch->u.ChoiceMatch.Mask, 1, ch[0]);
		INCLUDE(LastMatch->u.ChoiceMatch.Mask, 2, ch[1]);
		if (Flags & 2)
			LastMatch->u.ChoiceMatch.SingleCase[(Char & 255) >> 3] = 0;
		LastMatch->u.ChoiceMatch.MatchMulti = 1;
#endif
		return;
	}
	if (!LastMatch || LastMatch->Type != SrchString ||
					  LastMatch->u.StringMatch.Length >=
					  WSIZE(LastMatch->u.StringMatch.String)-1) {
		PSEARCHPART p;

		if ((p = NewSearchPart()) == NULL) return;
		p->Type = SrchString;
		p->u.StringMatch.Length = 0;
		{	WORD *pc = p->u.StringMatch.ConvertCSet;

			*pc = 0; *++pc = 0; *++pc = 0; *++pc = 0; *++pc = 0; *++pc = 0;
			pc = p->u.StringMatch.AllowCaseConvert;
			*pc = 0; *++pc = 0; *++pc = 0; *++pc = 0; *++pc = 0; *++pc = 0;
		}
		if (Flags & 2) {
			CharUpperBuffW(ch+0, 1);
			CharLowerBuffW(ch+1, 1);
		}
		if (Flags & 1) {
			BYTE c[2];

			switch (CharSet) {
				case CS_OEM:
					ch[0] = MapUnicodeToOem(ch[0]);
					ch[1] = MapUnicodeToOem(ch[1]);
					break;

				case CS_EBCDIC:
					if (ch[0] < 0x100) ch[0] = MapAnsiToEbcdic[ch[0]];
					if (ch[1] < 0x100) ch[1] = MapAnsiToEbcdic[ch[1]];
					break;

				case CS_ANSI:
					WideCharToMultiByte(AnsiCodePage, 0, ch,2, c,2, NULL, NULL);
					ch[0] = c[0] & 255;
					ch[1] = c[1] & 255;
					break;

				case CS_UTF8:
					{	BYTE c2[8];

						WideCharToMultiByte(CP_UTF8, 0, ch,1, c2,8, NULL, NULL);
						ch[0] = c2[0];
						WideCharToMultiByte(CP_UTF8, 0, ch+1,1,c2,8,NULL, NULL);
						ch[1] = c2[0];
					}
					break;
			}
		}
		p->u.StringMatch.FirstUcase = ch[0];
		p->u.StringMatch.FirstLcase = ch[1];
		if (!FirstString && !Level) {
			FirstString = p;
			while ((p = p->Prev) != NULL)
#if NEW_SEARCH
				if (!p->u.NewChoice.MatchMulti) ++OffsetTo1stString;
				else RescanMaximumMatch = TRUE;
#else
				if (!p->u.ChoiceMatch.MatchMulti) ++OffsetTo1stString;
				else RescanMaximumMatch = TRUE;
#endif
		}
	}
	{	INT Index = LastMatch->u.StringMatch.Length >> 4;
		INT Value = 1 << (LastMatch->u.StringMatch.Length & 15);

		if (Flags & 1)
			LastMatch->u.StringMatch.ConvertCSet[Index] |= Value;
		if (Flags & 2)
			LastMatch->u.StringMatch.AllowCaseConvert[Index] |= Value;
	}
	LastMatch->u.StringMatch.String[LastMatch->u.StringMatch.Length++] = Char;
	LastMatch->u.StringMatch.String[LastMatch->u.StringMatch.Length]   = '\0';
}

static PWSTR pDisp;

void SetSrchDisp(INT c)
{
	if (pDisp < SrchDispBuf + WSIZE(SrchDispBuf) - 4) *pDisp++ = c;
	else if (pDisp == SrchDispBuf + WSIZE(SrchDispBuf) - 4) {
		wcscpy(pDisp, L"...");
		pDisp += 3;
	}
}

PWSTR EnterChoice(PWSTR Cmd)
{	BOOL Inverse;

	Inverse = *++Cmd=='^';
#if NEW_SEARCH
	if (CreateChoice() == NULL) {
		MatchValid = 0;
		return L"";
	}
	if (Inverse) {
		LastMatch->u.NewChoice.Inverse = Inverse;
		SetSrchDisp(*Cmd);
		++Cmd;
		LastMatch->u.NewChoice.Range[0][0] = 
		LastMatch->u.NewChoice.Range[1][0] = '\n';
		LastMatch->u.NewChoice.Range[0][1] = 
		LastMatch->u.NewChoice.Range[1][1] = '\r';
		LastMatch->u.NewChoice.Range[0][2] = 
		LastMatch->u.NewChoice.Range[1][2] = '\0';
		LastMatch->u.NewChoice.Range[0][3] = 
		LastMatch->u.NewChoice.Range[1][3] = '\036';
		LastMatch->u.NewChoice.Fill		   = 4;
	}
#else
	if (Inverse) {
		NewChoice(AnyMask);
		SetSrchDisp(*Cmd);
		++Cmd;
		LastMatch->u.ChoiceMatch.Inverted = 1;
	} else NewChoice(NoneMask);
#endif
	for (;;) {
		WCHAR cw, cd, ce;	/*character in Windows, DOS and EBCDI charset*/
#if !NEW_SEARCH
		MASK  *m;
#endif
		BOOL  AllowCase = TRUE;
		BOOL  AllowCSet = TRUE;

		SetSrchDisp(cw = cd = *Cmd);
		CharToOemBuffW(&cd, (LPSTR)&cd, 1);
		ce = MapAnsiToEbcdic[cw & 255];
		switch (cw) {
			case '\0':
				return (Cmd);

			case '\\':
				SetSrchDisp(cw = (BYTE)*++Cmd);
				if (!cw) return (Cmd);
				if (cw>='0' && cw<'8') {
					cw -= '0';
					if (Cmd[1]>='0' && Cmd[1]<'8') {
						cw = (cw << 3) + *++Cmd - '0';
						SetSrchDisp(*Cmd);
					}
					if (cw<040 && Cmd[1]>='0' && Cmd[1]<'8') {
						cw = (cw << 3) + *++Cmd - '0';
						SetSrchDisp(*Cmd);
					}
					ce = cd = cw;
					AllowCSet = FALSE;
				} else if (cw == '%') {
					cw = Cmd[1];
					if (ISHEX(cw)) {
						++Cmd;
						SetSrchDisp(cw);
						if (cw >= 'A') cw += (BYTE)(10-'A');
						cw &= 15;
						if (ISHEX(Cmd[1])) {
							cw <<= 4;
							cw  += *++Cmd & 15;
							if (*Cmd >= 'A') cw += (10-'A') & 15;
							SetSrchDisp(*Cmd);
							if (ISHEX(Cmd[1]) && ISHEX(Cmd[2])) {
								cw <<= 4;
								cw  += *++Cmd & 15;
								if (*Cmd >= 'A') cw += (10-'A') & 15;
								SetSrchDisp(*Cmd);
								cw <<= 4;
								cw  += *++Cmd & 15;
								if (*Cmd >= 'A') cw += (10-'A') & 15;
								SetSrchDisp(*Cmd);
							}
						}
					} else cw = *Cmd;
					ce = cd = cw;
					AllowCSet = FALSE;
				} else {
					if (!MagicFlag && cw==']') return (Cmd);
					cd = cw;
					CharToOemBuffW(&cd, (LPSTR)&cd, 1);
					ce = MapAnsiToEbcdic[cw];
				}
				AllowCase = FALSE;
				/*FALLTHROUGH*/
			case ']':
				if (MagicFlag && cw==']')
					if (AllowCase)	/*otherwise fallthrough from '\\'*/
						return (Cmd);
				/*FALLTHROUGH*/
			default:
#if NEW_SEARCH
				if (LastMatch->u.NewChoice.Fill >= NUM_RANGES) {
					LastMatch->u.NewChoice.ContinuedInNextSP = TRUE;
					CreateChoice();
					LastMatch->u.NewChoice.ContinuedInPrevSP = TRUE;
				}
				{	static BOOL	WithinRange;
					INT			i;

					i = LastMatch->u.NewChoice.Fill;
					if (WithinRange) {
						LastMatch->u.NewChoice.Range[1][i] = cw;
						++LastMatch->u.NewChoice.Fill;
						WithinRange = FALSE;
					} else {
						LastMatch->u.NewChoice.Range[0][i] =
						LastMatch->u.NewChoice.Range[1][i] = cw;
						if (Cmd[1] == '-' && Cmd[2] != '\0' &&
								(MagicFlag ? Cmd[2] != ']'
										   : Cmd[2] != '\\' || Cmd[3] != ']')) {
							WithinRange = TRUE;
							SetSrchDisp(*++Cmd);
						} else ++LastMatch->u.NewChoice.Fill;
					}
					if (AllowCSet) LastMatch->u.NewChoice.ConvCSetChoice[i >> 4]
								   |= 1 << (i & 15);
					if (!WithinRange && AllowCase) {
						/* add case-converted character/range... */
						WCHAR	c[4];

						c[0] = c[2] = LastMatch->u.NewChoice.Range[0][i];
						c[1] = c[3] = cw;
						CharUpperBuffW(c+2, 2);
						if (c[2] == c[0] || c[3] == c[1]) {
							CharLowerBuffW(c+2, 2);
						}
						if (c[2] != c[0] && c[3] != c[1]) {
							if (LastMatch->u.NewChoice.Fill >= NUM_RANGES) {
								LastMatch->u.NewChoice.ContinuedInNextSP = TRUE;
								CreateChoice();
								LastMatch->u.NewChoice.ContinuedInPrevSP = TRUE;
							}
							i = LastMatch->u.NewChoice.Fill;
							LastMatch->u.NewChoice.Range[0][i] = c[2];
							LastMatch->u.NewChoice.Range[1][i] = c[3];
							++LastMatch->u.NewChoice.Fill;
						}
					}
				}
#else
				m = &LastMatch->u.ChoiceMatch.Mask;
				if (AllowCase)
					LastMatch->u.ChoiceMatch.SingleCase[(cw & 255) >> 3]
						&= ~(1 << (cw & 7));
				if (Inverse) {
					EXCLUDE(*m, 0, cw);
					EXCLUDE(*m, 1, cd);
					EXCLUDE(*m, 2, ce);
				} else {
					INCLUDE(*m, 0, cw);
					INCLUDE(*m, 1, cd);
					INCLUDE(*m, 2, ce);
				}
				{	static BOOL WithinRange;
					static BYTE WinRngStart, DosRngStart, EbcdiRngStart;

					if (!WithinRange && Cmd[1] == '-' && Cmd[2] != '\0' &&
						   (MagicFlag ? Cmd[2] != ']'
									  : Cmd[2] != '\\' || Cmd[3] != ']')) {
						WithinRange   = TRUE;
						WinRngStart   = cw;
						DosRngStart   = cd;
						EbcdiRngStart = ce;
						SetSrchDisp(*++Cmd);
					} else if (WithinRange) {
						INT ca = cw;

						WithinRange = FALSE;
						if (AllowCase) while (ca > WinRngStart) {
							--ca;
							LastMatch->u.ChoiceMatch.SingleCase[(ca & 255) >> 3]
								&= ~(1 << (ca & 7));
						}
						if (Inverse) {
							while (cw > WinRngStart)   {--cw; EXCLUDE(*m,0,cw);}
							while (cd > DosRngStart)   {--cd; EXCLUDE(*m,1,cd);}
							while (ce > EbcdiRngStart) {--ce; EXCLUDE(*m,2,ce);}
						} else {
							while (cw > WinRngStart)   {--cw; INCLUDE(*m,0,cw);}
							while (cd > DosRngStart)   {--cd; INCLUDE(*m,1,cd);}
							while (ce > EbcdiRngStart) {--ce; INCLUDE(*m,2,ce);}
						}
					}
				}
#endif
		}
		++Cmd;
	}
}

PWSTR BuildMatchList(PWSTR Cmd, INT Delimiter, INT *ErrNo)
{	PWSTR		Start = Cmd;
	BOOL		ForcedMagic = FALSE;
	PSEARCHPART	FirstBranch[10], LastBranch[10];

	Level = 0;
	FirstBranch[0] = LastBranch[0] = NULL;
	pDisp = SrchDispBuf;
	MatchForward = (*pDisp++ = *Cmd++) == '/';
	if (*Cmd && *Cmd != Delimiter) {
		BOOL Done = FALSE;

		while (FirstMatch) {
			LastMatch = FirstMatch->Next;
			free(FirstMatch);
			FirstMatch = LastMatch;
		}
		FirstString = NULL;
		OffsetTo1stString = 0;
		RescanMaximumMatch = MatchAtStartOfLine = MatchAtEndOfLine = FALSE;
		if (*Cmd == '^') {
			MatchAtStartOfLine = TRUE;
			SetSrchDisp(*Cmd++);
		}
		MatchValid = TRUE;
		for (;;) {
			if (*Cmd == Delimiter) {
				++Cmd;
				break;
			}
			if (!ForcedMagic) SetSrchDisp(*Cmd);
			switch (*Cmd) {
				case '\0':
					Done = TRUE;
					break;
				case '.':
					if (MagicFlag || ForcedMagic) {
						ForcedMagic = FALSE;
#if NEW_SEARCH
						CreateChoice();
						LastMatch->u.NewChoice.Range[0][0] = '\036' + 1;
						LastMatch->u.NewChoice.Range[1][0] = '\0'	- 1;
						LastMatch->u.NewChoice.Range[0][1] = '\0'	+ 1;
						LastMatch->u.NewChoice.Range[1][1] = '\n'	- 1;
						LastMatch->u.NewChoice.Range[0][2] = '\n'	+ 1;
						LastMatch->u.NewChoice.Range[1][2] = '\r'	- 1;
						LastMatch->u.NewChoice.Range[0][3] = '\r'	+ 1;
						LastMatch->u.NewChoice.Range[1][3] = '\036'	- 1;
						LastMatch->u.NewChoice.Fill		   = 4;
						if (MagicFlag ? Cmd[1]=='*' :
										Cmd[1]=='\\' && Cmd[2]=='*')
							LastMatch->u.NewChoice.MatchMulti = 1;
#else
						NewChoice(AnyMask);
						if (MagicFlag ? Cmd[1]=='*' :
										Cmd[1]=='\\' && Cmd[2]=='*')
							LastMatch->u.ChoiceMatch.MatchMulti = 1;
#endif
					} else EnterMatch(*Cmd, Cmd, 1);
					break;
				case '[':
#if NEW_SEARCH
					if (MagicFlag || ForcedMagic) {
						ForcedMagic = FALSE;
						if (!*(Cmd = EnterChoice(Cmd))) Done = TRUE;
						else if (MagicFlag ? Cmd[1]=='*' :
											 Cmd[1]=='\\' && Cmd[2]=='*')
							LastMatch->u.NewChoice.MatchMulti = 1;
					} else EnterMatch(*Cmd, Cmd, 1);
#else
					if (MagicFlag || ForcedMagic) {
						ForcedMagic = FALSE;
						if (!*(Cmd = EnterChoice(Cmd))) Done = TRUE;
						else if (MagicFlag ? Cmd[1]=='*' :
											 Cmd[1]=='\\' && Cmd[2]=='*')
							LastMatch->u.ChoiceMatch.MatchMulti = 1;
					} else EnterMatch(*Cmd, Cmd, 1);
#endif
					break;
				case '/':
				case '?':
					if (*Cmd==*Start && pDisp[-1]==*Cmd) {
						pDisp[-1] = '\\';
						SetSrchDisp(*Cmd);
					}
					EnterMatch(*Cmd, Cmd, 1);
					break;
				case '$':
					if (!Cmd[1] || Cmd[1]==Delimiter) {
						MatchAtEndOfLine = TRUE;
						break;
					}
					EnterMatch(*Cmd, Cmd, 1);
					break;
				case '\\':
					SetSrchDisp(*++Cmd);
					switch (*Cmd) {
						WCHAR c;

						case '(':
							if (Level >= 9) {
								if (ErrNo) *ErrNo = 251;
								SearchErrNo = 251;
								MatchValid = FALSE;
								Done = TRUE;
								break;
							}
							++Level;
							FirstBranch[Level] = LastBranch[Level] = NULL;
							/*FALLTHROUGH*/
						case ')': case '|':
						case '<': case '>':
							if (!NewSearchPart()) break;
							LastMatch->Type = SrchSpecial;
							LastMatch->u.SpecialMatch.Char  = (CHAR)*Cmd;
							LastMatch->u.SpecialMatch.Pos.p = NULL;
							LastMatch->u.SpecialMatch.NextBranch = NULL;
							if (*Cmd=='|') {
								if (!Level) {
									if (ErrNo) *ErrNo = 252;
									SearchErrNo = 252;
									MatchValid = FALSE;
									Done = TRUE;
									break;
								}
								if (LastBranch[Level]!=NULL) {
									LastBranch[Level]->u.SpecialMatch.NextBranch
										= LastMatch;
									LastBranch[Level] = LastMatch;
								}
							} else if (*Cmd == '(') {
								FirstBranch[Level] = LastBranch[Level] =
									LastMatch;
							} else if (*Cmd == ')') {
								if (LastBranch[Level] != NULL &&
									LastBranch[Level] != FirstBranch[Level]) {
										LastBranch[Level]->u.SpecialMatch.
														 NextBranch = LastMatch;
										LastMatch->u.SpecialMatch.NextBranch =
											FirstBranch[Level];
								}
								if (!Level) {
									if (ErrNo) *ErrNo = 250;
									SearchErrNo = 250;
									MatchValid = FALSE;
									Done = TRUE;
									break;
								}
								--Level;
							}
							break;

						case '0': case '4':
						case '1': case '5':
						case '2': case '6':
						case '3': case '7':
							c = *Cmd - '0';
							if (Cmd[1] >= '0' && Cmd[1] < '8') {
								c = (c << 3) + *++Cmd - '0';
								SetSrchDisp(*Cmd);
							}
							if (c < 040 && Cmd[1] >= '0' && Cmd[1] < '8') {
								c = (c << 3) + *++Cmd - '0';
								SetSrchDisp(*Cmd);
							}
							EnterMatch(c, Cmd, 0);
							break;

						case '%':	/*WinVi extension: hex*/
							c = Cmd[1];
							if (ISHEX(c)) {
								++Cmd;
								SetSrchDisp(c);
								if (c >= 'A') c += (BYTE)(10-'A');
								c &= 15;
								if (ISHEX(Cmd[1])) {
									c <<= 4;
									c  += *++Cmd & 15;
									if (*Cmd >= 'A') c += (10-'A') & 15;
									SetSrchDisp(*Cmd);
									if (UtfEncoding == 16) {
										if (ISHEX(Cmd[1]) && ISHEX(Cmd[2])) {
											c <<= 4;
											c  += *++Cmd & 15;
											if (*Cmd >= 'A') c += (10-'A') & 15;
											SetSrchDisp(*Cmd);
											c <<= 4;
											c  += *++Cmd & 15;
											if (*Cmd >= 'A') c += (10-'A') & 15;
											SetSrchDisp(*Cmd);
										}
									}
								}
								EnterMatch(c, Cmd, 0);
							} else EnterMatch(*Cmd, Cmd, 1);
							break;

						case '\0':
							EnterMatch(*--Cmd, L"-", 0);
							Done = TRUE;
							break;

						case '.': case '[':
							if (!MagicFlag) {
								ForcedMagic = TRUE;
								continue;
							}
							/*FALLTHROUGH*/
						default:
							EnterMatch(*Cmd, Cmd, 1);
					}
					break;
				default:
					EnterMatch(*Cmd, Cmd, 3);
			}
			if (Done) break;
			if (MagicFlag) {
				if (*++Cmd == '*') SetSrchDisp(*Cmd++);
			} else if (*++Cmd == '\\' && Cmd[1] == '*') {
				SetSrchDisp(*Cmd++);
				SetSrchDisp(*Cmd++);
			}
		}
		*pDisp = '\0';
	} else {
		if (*Cmd) ++Cmd;
		if (!MatchValid) {
			SearchAndStatus(&CurrPos, 0);	/*error display only*/
			return (Cmd);
		}
	}
	if (MatchValid && Level) {
		if (ErrNo) *ErrNo = 249;
		SearchErrNo = 249;
		MatchValid = FALSE;
	}
	if (!MatchValid) MessageBeep(MB_ICONEXCLAMATION);
	return (Cmd);
}

typedef struct tagSEARCH_DESCR {
	BOOL		MatchValid, MatchForward;
	BOOL		RescanMaximumMatch, MatchAtStartOfLine, MatchAtEndOfLine;
	PSEARCHPART	FirstMatch, FirstString, LastMatch;
	WCHAR		DisplayBuffer[1];
} SEARCH_DESCR;
SEARCH_DESCR FAR *SavedSearch;

BOOL SaveMatchList(VOID)
{	INT Size = sizeof(SEARCH_DESCR) + wcslen(SrchDispBuf) * sizeof(WCHAR);

	if ((SavedSearch = _fcalloc(Size, 1)) == NULL) return (FALSE);
	wcscpy(SavedSearch->DisplayBuffer, SrchDispBuf);
	SavedSearch->FirstMatch			= FirstMatch;
	SavedSearch->FirstString		= FirstString;
	SavedSearch->LastMatch			= LastMatch;
	SavedSearch->RescanMaximumMatch	= RescanMaximumMatch;
	SavedSearch->MatchAtStartOfLine	= MatchAtStartOfLine;
	SavedSearch->MatchAtEndOfLine	= MatchAtEndOfLine;
	SavedSearch->MatchForward		= MatchForward;
	SavedSearch->MatchValid			= MatchValid;
	FirstMatch = FirstString = LastMatch = NULL;
	*SrchDispBuf = '\0';
	return (TRUE);
}

BOOL RestoreMatchList(VOID)
{
	if (SavedSearch == NULL) return (FALSE);
	while (FirstMatch) {
		LastMatch = FirstMatch->Next;
		free(FirstMatch);
		FirstMatch = LastMatch;
	}
	wcscpy(SrchDispBuf,  SavedSearch->DisplayBuffer);
	FirstMatch			= SavedSearch->FirstMatch;
	FirstString			= SavedSearch->FirstString;
	LastMatch			= SavedSearch->LastMatch;
	RescanMaximumMatch	= SavedSearch->RescanMaximumMatch;
	MatchAtStartOfLine	= SavedSearch->MatchAtStartOfLine;
	MatchAtEndOfLine	= SavedSearch->MatchAtEndOfLine;
	MatchForward		= SavedSearch->MatchForward;
	MatchValid			= SavedSearch->MatchValid;
	_ffree(SavedSearch);
	SavedSearch = NULL;
	return (TRUE);
}

REPLIST   RepList;
LPREPLIST lpLastRep = &RepList;
INT       nRepListCount = 0;

VOID FreeRepList(VOID)
{	LPREPLIST ToDel;

	while ((ToDel = RepList.Next) != NULL) {
		RepList.Next = ToDel->Next;
		_ffree(ToDel);
	}
	lpLastRep = &RepList;
	nRepListCount = 0;
}

PWCHAR ExpandRepList(PWCHAR Prev)
{
	if (Prev==NULL || ++Prev < lpLastRep->Buf + WSIZE(lpLastRep->Buf))
		return (Prev);
	if ((lpLastRep->Next = _fcalloc(sizeof(REPLIST), 1)) == NULL)
		return (NULL);
	++nRepListCount;
	lpLastRep = lpLastRep->Next;
	return (lpLastRep->Buf);
}

PWSTR pLastReplace;
INT	  nParenPairs;

BOOL CheckParenPairs(VOID)
{	PSEARCHPART	pSp;
	INT			Level;

	Level = nParenPairs = 0;
	for (pSp=FirstMatch; pSp; pSp=pSp->Next) {
		if (pSp->Type == SrchSpecial) {
			switch (pSp->u.SpecialMatch.Char) {
				case '(': ++nParenPairs;
						  ++Level;
						  continue;
				case ')': if (--Level < 0) break;
						  /*FALLTHROUGH*/
				default:  continue;
			}
			break;
		}
	}
	if (Level) {
		Error(231);
		return (FALSE);
	}
	return (TRUE);
}

BOOL BuildReplaceString
	 (PWSTR Source, LONG *DestLen, PPOSITION Pos, LONG Len, WORD Flags)
	/*Flags: 1=hex input flag checked in dialog box
	 *		 2=display error in box
	 *		 4=double any \'s (nomagic)
	 */
{	PWCHAR		pRep = RepList.Buf-1;
	PSEARCHPART	pSp;
	INT			c;

	while (*Source) {
		switch (*Source) {
			case '\\':
				if (Flags & 4) {
					if ((pRep = ExpandRepList(pRep)) != NULL) *pRep = '\\';
					break;
				}
				switch (c = *++Source) {
					case '\0':
						if ((pRep = ExpandRepList(pRep)) != NULL)
							*pRep = *--Source;
						break;
					case '1': case '2': case '3': case '4': case '5':
					case '6': case '7': case '8': case '9':
						if (c-'0' <= nParenPairs) {
							/*extract \(...\)*/
							POSITION	p;
							INT			i  = c-'0';

							pSp = FirstMatch;
							for (;;) {
								while (pSp && (pSp->Type != SrchSpecial ||
									   pSp->u.SpecialMatch.Char != '('))
									pSp = pSp->Next;
								if (!pSp || --i <= 0) break;
								pSp = pSp->Next;
							}
							assert(pSp);
							if (pSp) {
								p = pSp->u.SpecialMatch.Pos;
								while ((pSp = pSp->Next) != NULL) {
									if (pSp->Type == SrchSpecial) {
										switch (pSp->u.SpecialMatch.Char) {
											case '(': ++i;
													  continue;
											case ')': if (--i < 0) break;
													  /*FALLTHROUGH*/
											default:  continue;
										}
										break;
									}
								}
							}
							assert(pSp);
							if (pSp) {
								while (ComparePos(&p,
											 &pSp->u.SpecialMatch.Pos) < 0) {
									c = SingleCharAndAdvance(&p);
									if ((pRep = ExpandRepList(pRep)) != NULL)
										*pRep = c;
								}
							}
							break;
						}
						if (*Source >= '8') {
							Error(230);
							if (Flags & 2) ErrorBox(MB_ICONSTOP, 230);
							if (Disabled) Enable();
							FreeRepList();
							return (FALSE);
						}
						/*FALLTHROUGH*/
					case '0':
						if ((pRep = ExpandRepList(pRep)) != NULL) {
							*pRep = *Source - '0';
							if (Source[1] >= '0' && Source[1] < '8') {
								*pRep = (*pRep<<3) + *++Source - '0';
								if (*pRep < '\040')
									if (Source[1] >= '0' && Source[1] < '8')
										*pRep = (*pRep<<3) + *++Source - '0';
							}
						}
						break;
					case '%':
						if ((pRep = ExpandRepList(pRep)) != NULL) {
							if (ISHEX(Source[1])) {
								*pRep = *++Source;
								if (*Source >= 'A')
									*pRep += (BYTE)(10-'A');
								*pRep &= 15;
								if (ISHEX(Source[1])) {
									*pRep <<= 4;
									*pRep += *++Source & 15;
									if (*Source >= 'A')
										*pRep += (BYTE)((10-'A') & 15);
								}
								if (UtfEncoding == 16) {
									if (ISHEX(Source[1]) && ISHEX(Source[2])) {
										*pRep <<= 4;
										*pRep += *++Source & 15;
										if (*Source >= 'A')
											*pRep += (BYTE)((10-'A') & 15);
										*pRep <<= 4;
										*pRep += *++Source & 15;
										if (*Source >= 'A')
											*pRep += (BYTE)((10-'A') & 15);
									}
								}
							} else *pRep = *Source;
						}
						break;
					case '&':
						if (!MagicFlag) goto HandleAmpersand;
						/*FALLTHROUGH*/
					default:
						if ((pRep = ExpandRepList(pRep)) != NULL)
							*pRep = *Source;
				}
				break;
			case '&':
				if (MagicFlag && !(Flags & 4)) {
					POSITION p;
					LONG	 n;

				HandleAmpersand:
					p = *Pos;
					n = Len;
					if (UtfEncoding == 16 && n & 1) {
						/*TODO: show error message*/
						--n;
					}
					while (n) {
						if ((c = CharAndAdvance(&p)) == C_CRLF) {
							if (n) {
								if ((pRep = ExpandRepList(pRep)) != NULL)
									*pRep = '\r';
								c = '\n';
								n -= UtfEncoding == 16 ? 2 : 1;
							} else c = '\r';
						}
						if ((pRep = ExpandRepList(pRep)) != NULL) *pRep = c;
						n -= UtfEncoding == 16 ? 2 : 1;
					}
					break;
				}
				/*FALLTHROUGH*/
			default:
				if (Flags & 1) {
					if (ISHEX(*Source)) {
						if ((pRep = ExpandRepList(pRep)) != NULL) {
							if ((*pRep = *Source) >= 'A')
								*pRep += (BYTE)(10-'A');
							*pRep &= 15;
							if (ISHEX(Source[1])) {
								*pRep <<= 4;
								*pRep  += *++Source & 15;
								if (*Source >= 'A') *pRep += (10-'A') & 15;
								if (UtfEncoding == 16) {
									if (ISHEX(Source[1]) && ISHEX(Source[2])) {
										*pRep <<= 4;
										*pRep += *++Source & 15;
										if (*Source >= 'A')
											*pRep += (BYTE)((10-'A') & 15);
										*pRep <<= 4;
										*pRep += *++Source & 15;
										if (*Source >= 'A')
											*pRep += (BYTE)((10-'A') & 15);
									}
								}
							}
						}
					} else if (*Source != ' ') {
						Error(239);
						if (Flags & 2) ErrorBox(MB_ICONSTOP, 239);
						FreeRepList();
						return (FALSE);
					}
				} else if ((pRep = ExpandRepList(pRep)) != NULL) {
			//		PBYTE pc;
			//
			//		if (CharSet == CS_OEM)
			//			*pRep = UnicodeToOemChar(*Source);
			//		else if (CharSet == CS_EBCDIC)
			//			*pRep = *Source>0xff ? 0x1a : MapAnsiToEbcdic[*Source];
			//		else *pRep = *Source;
					*pRep = *Source;
				}
				break;
		}
		++Source;
	}
	if (pRep == NULL) {
		Error(121);
		if (Flags & 2) ErrorBox(MB_ICONSTOP, 121);
		FreeRepList();
		return (FALSE);
	}
	*DestLen = nRepListCount * WSIZE(lpLastRep->Buf) + (pRep+1-lpLastRep->Buf);
	return (TRUE);
}

void InsertSubstChar(PPOSITION Pos, INT Len, WORD Flags, PWSTR Buf)
{
	if (UtfEncoding == 16) {
		Reserve(Pos, Len * sizeof(WCHAR), Flags);
		assert(Pos->p->PageBuf);
		while (Len--)
			if (UtfLsbFirst) {
				Pos->p->PageBuf[Pos->i++] = (BYTE)*Buf;
				Pos->p->PageBuf[Pos->i++] = *Buf++ >> 8;
			} else {
				Pos->p->PageBuf[Pos->i++] = *Buf >> 8;
				Pos->p->PageBuf[Pos->i++] = (BYTE)*Buf++;
			}
	} else {
		CHAR Buf2[8];
		CHAR *p = Buf2;

		Len = WideCharToMultiByte(UtfEncoding == 8 ? CP_UTF8 :
								  CharSet==CS_OEM ? OemCodePage : AnsiCodePage,
								  0, Buf, Len, Buf2, sizeof(Buf2), NULL, NULL);
		Reserve(Pos, Len, Flags);
		assert(Pos->p->PageBuf);
		while (Len--)
			Pos->p->PageBuf[Pos->i++] = *p++;
	}
}

BOOL SubstituteCmd(LONG StartLine, LONG EndLine, PWSTR Args)
{	PWSTR		pSearch = NULL;
	PWSTR		pReplace = NULL;
	INT			c;
	BOOL		SaveWrapFlag, ReportLineCount, GlobalFlag = FALSE;
	BOOL		DoSetUnsafe = FALSE, AtByte0 = FALSE, ConfirmFlag = FALSE;
	LONG		lMatched;
	POSITION	Pos, Start, SkipNullMatch;
	LONG		LineCount = 0, LastSubstLine = -1, DiffLines = 0, SaveDiff;
	LONG		RepaintLength = 0;
	POSITION	RepaintPosition;
	UINT		RepaintFlags = 1;

	if (IsViewOnly()) return (FALSE);

	/*unassemble arguments...*/
	if ((c = *Args) > ' ' && !(CharFlags(c) & 8)) {
		pSearch = ++Args;
		while (*Args && *Args != c)
			if (*Args++ == '\\' && *Args) ++Args;
		if (*Args) {
			*Args++ = '\0';
			pReplace = Args;
			while (*Args && *Args != c)
				if (*Args++ == '\\' && *Args) ++Args;
			if (*Args) *Args++ = '\0';
		}
	} else if (!pLastReplace) {
		Error(234);
		return (FALSE);
	}
	for (;;) {
		switch (*Args++) {
			case 'c':	ConfirmFlag = TRUE;
						continue;
			case 'g':	GlobalFlag  = TRUE;
						/*FALLTHROUGH*/
			case ' ':	continue;
			case '\0':	break;
			default:	Error(106);
						return (FALSE);
		}
		break;
	}

	/*set up search structure...*/
	if (pSearch) {
		if (*pSearch) {
			*--pSearch = '/';
			pSearch = BuildMatchList(pSearch, c, NULL);
			assert(*pSearch == '\0');
		}
		if (!pReplace) pReplace = L"";
		if (pLastReplace) free(pLastReplace);
		if ((pLastReplace = malloc(2*wcslen(pReplace)+2)) == NULL) {
			Error(312);
			return (FALSE);
		}
		wcscpy(pLastReplace, pReplace);
	}
	if (!StartSearch()) return (FALSE);
	Start.p = NULL;

	if (!CheckParenPairs()) return (FALSE);

	/*jump to start line...*/
	if (StartLine != EndLine) {
		SwitchCursor(SWC_HOURGLASS_ON);
		ReportLineCount = GlobalFlag;
	} else ReportLineCount = FALSE;
	Pos.p = FirstPage;
	Pos.i = 0;
	Pos.f = 0;
	while (--StartLine > 0) {
		do; while (!(CharFlags(CharAndAdvance(&Pos)) & 1));
	}

	/*set up some variables...*/
	SaveWrapFlag = WrapScanFlag;
	lMatched = 0;
	WrapScanFlag = Interrupted = FALSE;
	StartUndoSequence();
	SkipNullMatch.p = NULL;
	DragAcceptFiles(hwndMain, FALSE);

	/*and finally, do the substitutions...*/
	while (!(CharFlags(CharAt(&Pos)) & 0x20)) {
		LONG Length, CurrLine, ReplaceLength, Lines;
		BOOL ContinueOnSameLine = GlobalFlag;

		/*search...*/
		if (!SearchIt(&Pos, 3)) break;
		if (SkipNullMatch.p != NULL) {
			if (ComparePos(&EndMatch, &SkipNullMatch) == 0) {
				if (!Advance(&Pos, UtfEncoding == 16 ? 2 : 1)) break;
				SkipNullMatch.p = NULL;
				continue;
			}
			SkipNullMatch.p = NULL;
		}
		CurrLine = CountLines(&Pos);
		if (EndLine >= 0 && CurrLine >= EndLine+DiffLines) break;
		Length = CountBytes(&EndMatch) - CountBytes(&Pos);
		if (CurrLine != LastSubstLine) {
			LastSubstLine = CurrLine;
			++LineCount;
		}

		if (ConfirmFlag) {
			SEARCHCONFIRMRESULT	ConfirmResult;

			if (SelectCount) InvalidateArea(&SelectStart, SelectCount, 1);
			SelectCount   = Length;
			SelectStart   = Pos;
			SelectBytePos = CountBytes(&SelectStart);
			if (RepaintLength)
				InvalidateArea(&RepaintPosition, RepaintLength, RepaintFlags);
			InvalidateArea(&SelectStart, SelectCount, 0);
			NewPosition(&SelectStart);
			HorizontalScrollIntoView();
			RepaintPosition = SelectStart;
			RepaintLength   = SelectCount;
			ConfirmResult = ConfirmSubstitute();
			if (ConfirmResult == ConfirmAbort) {
				SelectCount = 0;
				InvalidateText();
				break;
			}
			if (ConfirmResult == ConfirmAll)  ConfirmFlag = FALSE;
			if (ConfirmResult == ConfirmSkip) {
				Advance(&Pos, SelectCount);
				if (!GlobalFlag)
					do; while (!(CharFlags(CharAndAdvance(&Pos)) & 1));
				SelectCount = 0;
				RepaintFlags = 1;
				continue;
			}
			c = GoBackAndSingleChar(&RepaintPosition);
			if (c != C_EOF && CharFlags(c) & 1)
				Advance(&RepaintPosition, UtfEncoding == 16 ? 2 : 1);
			++RepaintLength;
			RepaintFlags = 9;
		}

		/*build replace buffer...*/
		if (!BuildReplaceString(pLastReplace, &ReplaceLength, &Pos,Length, 0)) {
			WrapScanFlag = SaveWrapFlag;
			DragAcceptFiles(hwndMain, TRUE);
			ShowEditCaret();
			SwitchCursor(SWC_HOURGLASS_OFF);
			return (FALSE);
		}

		if (!lMatched++) {
			Start = Pos;
			if (CountBytes(&Start) == 0) AtByte0 = TRUE;
		}
		Lines = CountLines(&EndMatch) - CurrLine;
		SaveDiff = DiffLines;
		if (Lines) {
			if (CharAt(&EndMatch)=='\n' && EndMatch.i && EndMatch.p->PageBuf &&
					EndMatch.p->PageBuf[EndMatch.i-1] == '\r') --Lines;
			if (Lines) {
				DiffLines -= Lines;
				ContinueOnSameLine = TRUE;
			}
		}

		/*replace...*/
		SelectCount = Length;
		if (SelectCount) {
			SelectStart = Pos;
			SelectBytePos = CountBytes(&SelectStart);
			DeleteSelected(6);
			DoSetUnsafe = TRUE;
		}
		CurrPos = Pos;
		if (ReplaceLength) {
			LPREPLIST CurrRep = &RepList;
			PWCHAR	  p = RepList.Buf;
			BOOL	  AddCr = FALSE;

			DoSetUnsafe = TRUE;
			while (ReplaceLength-- > 0) {
				if (AddCr && *p=='\n') {
					InsertSubstChar(&Pos, 2, 1, L"\r\n");
					AddCr = FALSE;
					++DiffLines;
				} else {
					if (AddCr) {
						InsertSubstChar(&Pos, 1, 1, L"\r");
						AddCr = FALSE;
						++DiffLines;
					}
					if (CharFlags(*p) & 1) {
						if (*p=='\r' && ReplaceLength>0) AddCr = TRUE;
						else {
							if (*p=='\r' && CharAt(&Pos)=='\n') {
								Reserve(&Pos, -1, 1);
								InsertSubstChar(&Pos, 2, 1, L"\r\n");
								GoBack(&Pos, UtfEncoding == 16 ? 2 : 1);
							} else {
								InsertSubstChar(&Pos, 1, 1, p);
								++DiffLines;
							}
						}
					} else {
						InsertSubstChar(&Pos, 1, 0, p);
					}
				}
				if (++p >= CurrRep->Buf + WSIZE(CurrRep->Buf)) {
					if ((CurrRep = CurrRep->Next) == NULL) break;
					p = CurrRep->Buf;
				}
			}
		}
		FreeRepList();

		if (DiffLines != SaveDiff) RepaintFlags = 15;
		if (!ContinueOnSameLine)
			do; while (!(CharFlags(CharAndAdvance(&Pos)) & 1));
		else SkipNullMatch = Pos;
	}
	if (RepaintLength)
		InvalidateArea(&RepaintPosition, RepaintLength, RepaintFlags);
	WrapScanFlag = SaveWrapFlag;
	if (DoSetUnsafe) SetUnsafe();
	if (Interrupted) Error(218);
	else if (!lMatched) Error(214);
	else {
		/*display number of matches...*/
		extern WCHAR BufferW[300];
		static WCHAR Fmt[36];

		if (ReportLineCount && lMatched>1)
			 LOADSTRINGW(hInst, 242 + (LineCount!=1), Fmt, WSIZE(Fmt));
		else LOADSTRINGW(hInst, 240 + (lMatched!=1),  Fmt, WSIZE(Fmt));
		_snwprintf(BufferW, WSIZE(BufferW), Fmt, lMatched, LineCount);
		NewStatus(0, BufferW, NS_NORMAL);
	}
	if (lMatched) {
		if (AtByte0 && LineInfo[0].Pos.p==Start.p && LineInfo[0].Pos.i==0) {
			ScreenStart.p = FirstPage;
			ScreenStart.i = 0;
			InvalidateText();
		} else {
			GoBackAndChar(&Start);
			InvalidateArea(&Start, CountBytes(&Pos) - CountBytes(&Start), 15);
		}
		UpdateWindow(hwndMain);
		NewPosition(&CurrPos);
		FindValidPosition(&CurrPos, (WORD)(Mode==InsertMode));
	}
	if (Disabled) Enable();
	DragAcceptFiles(hwndMain, TRUE);
	ShowEditCaret();
	SwitchCursor(SWC_HOURGLASS_OFF);
	return (TRUE);
}

void SearchNewCset(void)
	/* CharSet value has changed,
	 * recalculate new FirstUcase/FirstLcase characters
	 * for fast scan in forthcoming search operations...
	 */
{
	if (MatchValid) {
		PSEARCHPART p;

		for (p=FirstMatch; p; p=p->Next) {
			if (p->Type == SrchString) {
				WCHAR ch[2];

				ch[0] = ch[1] = *p->u.StringMatch.String;
				if (p->u.StringMatch.AllowCaseConvert[0] & 1) {
					CharUpperBuffW(ch+0, 1);
					CharLowerBuffW(ch+1, 1);
				}
				if (p->u.StringMatch.ConvertCSet[0] & 1) {
					CHAR  c[4];

					switch (CharSet) {
						case CS_OEM:
							ch[0] = MapUnicodeToOem(ch[0]);
							ch[1] = MapUnicodeToOem(ch[1]);
							break;

						case CS_EBCDIC:
							ch[0] = MapAnsiToEbcdic[ch[0] & 255];
							ch[1] = MapAnsiToEbcdic[ch[1] & 255];
							break;

						case CS_ANSI:
							WideCharToMultiByte(AnsiCodePage, 0, ch, 2,
												c, sizeof(c), NULL, NULL);
							ch[0] = c[0] & 255;
							ch[1] = c[1] & 255;
							break;

						case CS_UTF8:
							WideCharToMultiByte(CP_UTF8, 0, ch, 1,
												c, sizeof(c), NULL, NULL);
							ch[0] = c[0] & 255;
							WideCharToMultiByte(CP_UTF8, 0, ch+1, 1,
												c, sizeof(c), NULL, NULL);
							ch[1] = c[0] & 255;
							break;
					}
				}
				p->u.StringMatch.FirstUcase = ch[0];
				p->u.StringMatch.FirstLcase = ch[1];
			}
		}
	}
}
