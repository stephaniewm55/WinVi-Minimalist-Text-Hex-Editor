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
 *      6-Dec-2000	new (darker) pen for tabctrl hilite if backgrnd is bright
 *      6-Dec-2000	correct cursor position after pixel 32767 with 16-bit ints
 *     21-Jun-2002	tmOverhang handling removed
 *     22-Jun-2002	display glitch if selection area is made smaller
 *     22-Jul-2002	use of private myassert.h
 *     12-Sep-2002	+EBCDIC quick hack (Christian Franke)
 *     16-Sep-2002	minimum check for VisiLines and CrsrLines
 *     10-Oct-2002	scrollable scrollbar if first line of file is not visible
 *      2-Nov-2002	fixed caret shape at start of file after removing contents
 *     11-Jan-2003	error functions moved to misc.c
 *      2-Sep-2003	caret height at end of file
 *     15-Mar-2005	null pointer check in TextLine (crash when using profiles)
 *      7-Mar-2006	new calculation of caret dimensions
 *     13-Jun-2007	UTF-8 and UTF-16 special handling
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *      6-Apr-2009	some optimizations in calculating display widths of strings
 *     31-Aug-2010	DLL load from system directory only
 *     19-Jan-2012	fixed text position calculation in first line with BOM
 */

#include <windows.h>
#include <string.h>
#include <wchar.h>
#include "myassert.h"
#include "winvi.h"
#include "resource.h"
#include "page.h"
#include "paint.h"
#include "status.h"
#include "toolbar.h"
#include "ctl3d.h"

#if !defined(WIN32)
# define IsDBCSLeadByteEx(a,b) IsDBCSLeadByte(b)
#endif

HPEN	   BlackPen;
HBRUSH	   SelBkgndBrush, BkgndBrush;
HPEN	   HilitePen, ShadowPen;	/*3d paint tools*/
HBRUSH	   FaceBrush;				/*3d paint tools*/
HPEN	   DlgLitePen;
HBRUSH	   DlgBkBrush;
LONG	   CaretX;
INT		   CaretY, CaretWidth, CaretHeight;
CARET_TYPE CaretType;
INT		   VisiLines;	/*number of visible screen lines*/
INT		   CrsrLines;	/*number of screen lines with positionable cursor*/
HFONT	   SpecialFont = 0;
HWND	   hwndScroll;
INT		   TextAreaHeight;
LONG	   xOffset;

void SystemColors(BOOL New)
{
	if (HilitePen)	{DeleteObject(HilitePen);  HilitePen  = NULL;}
	if (ShadowPen)	{DeleteObject(ShadowPen);  ShadowPen  = NULL;}
	if (FaceBrush)	{DeleteObject(FaceBrush);  FaceBrush  = NULL;}
	if (DlgBkBrush)	{DeleteObject(DlgBkBrush); DlgBkBrush = NULL;}
	if (DlgLitePen) {DeleteObject(DlgLitePen); DlgLitePen = NULL;}
	if (New) {
		HDC hDC;

		HilitePen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
		ShadowPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
		FaceBrush =		CreateSolidBrush  (GetSysColor(COLOR_BTNFACE));
		#if defined(WIN32)
			#define USE_3D_COLORS WinVersion>=MAKEWORD(50,3)
		#else
			#define USE_3D_COLORS Ctl3dEnabled()
		#endif
		if (USE_3D_COLORS) {
			DlgBkBrush = CreateSolidBrush (GetSysColor(COLOR_BTNFACE));
			DlgLitePen = CreatePen(PS_SOLID, 1,
										   GetSysColor(COLOR_BTNHIGHLIGHT));
		} else {
			COLORREF BackColor =		   GetSysColor(COLOR_WINDOW);

			DlgBkBrush = CreateSolidBrush(BackColor);
			DlgLitePen = CreatePen(PS_SOLID, 1,
				GetRValue(BackColor)>230 && GetGValue(BackColor)>230
					? RGB(128, 128, 128) : GetSysColor(COLOR_BTNHIGHLIGHT));
				/*using a dark gray pen if background is too bright,
				 *e.g. for 16-bit Windows 3.1 default (!) settings, defining
				 *both COLOR_WINDOW and COLOR_BTNHIGHLIGHT as pure white.
				 */
		}
		StatusStyle[NS_NORMAL].Color  =
		StatusStyle[NS_TOOLTIP].Color =	   GetSysColor(COLOR_BTNTEXT);
		if (hwndMain && IsWindowVisible(hwndMain)) {
			RECT r;
			r = ClientRect;
			r.top = r.bottom - StatusHeight;
			InvalidateRect(hwndMain, &r, FALSE);
		}
		hDC = GetDC(NULL);
		if (hDC) {
			PrepareBitmap(hDC);
			ReleaseDC(NULL, hDC);
		}
		MakeBackgroundBrushes();
	}
}

INT ScrollX;

void InitPaint(void)
{
	if (!TextHeight) TextHeight = 13;
	if (!TextColor)  TextColor	= RGB(0,0,0);
	LOADSTRINGW(hInst, 902, NumLock,  WSIZE(NumLock));
	LOADSTRINGW(hInst, 903, CapsLock, WSIZE(CapsLock));
	BlackPen   = GetStockObject(BLACK_PEN);
	SystemColors(TRUE);
	#if defined(WIN32)
	{	static NONCLIENTMETRICS ncmInfo;

		ncmInfo.cbSize = sizeof(ncmInfo);
		if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
								  0, &ncmInfo, 0)) {
			HDC hDC;

			lstrcpyn(ncmInfo.lfStatusFont.lfFaceName,
					 "MS Sans Serif", sizeof(ncmInfo.lfStatusFont.lfFaceName));
			hDC = GetDC(NULL);
			if (hDC) {
				ncmInfo.lfStatusFont.lfHeight =
					-MulDiv(ncmInfo.lfStatusFont.lfHeight,
							GetDeviceCaps(hDC, LOGPIXELSY), 72);
				ReleaseDC(NULL, hDC);
			} else ncmInfo.lfStatusFont.lfHeight = 10;
		}
		StatusFont = CreateFontIndirect(&ncmInfo.lfStatusFont);
	}
	#else
	{	static LOGFONT LogStatusFont = {
			16, 0, 0, 0, 400, 0, 0, 0,
			ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Helv"
		};
		HDC hDC = GetDC(NULL);

		if (hDC) {
			LogStatusFont.lfHeight =
				-MulDiv(10/*pt*/, GetDeviceCaps(hDC, LOGPIXELSY), 72);
			ReleaseDC(NULL, hDC);
		}
		StatusFont  = CreateFontIndirect(&LogStatusFont);
	}
	#endif
	RecalcStatusRow();
	TextAreaHeight = ClientRect.bottom - StatusHeight - nToolbarHeight;
	ScrollX = ClientRect.right - GetSystemMetrics(SM_CXVSCROLL);
	if (WinVersion >= MAKEWORD(95,3)) ScrollX -= 2;
	hwndScroll = CreateWindow("ScrollBar", "",
				WS_CHILDWINDOW | WS_VISIBLE | SBS_VERT, ScrollX, nToolbarHeight,
				GetSystemMetrics(SM_CXVSCROLL), TextAreaHeight,
				hwndMain, NULL, hInst, 0L);
	VisiLines = (TextAreaHeight + TextHeight - 1 - 2*YTEXT_OFFSET) / TextHeight;
	CrsrLines = (TextAreaHeight              + 2 - 2*YTEXT_OFFSET) / TextHeight;
	if (VisiLines < 1 || CrsrLines < 1) VisiLines = CrsrLines = 1;
	LineInfo = (PLINEINFO)LocalAlloc(LPTR, (VisiLines+2) * sizeof(LINEINFO));
	if (!LineInfo) ErrorBox(MB_ICONSTOP, 300, 7);
	CheckClipboard();
	hcurArrow	  = LoadCursor(NULL, IDC_ARROW);
	hcurHourGlass = LoadCursor(NULL, IDC_WAIT);
	hcurIBeam	  = LoadCursor(NULL, IDC_IBEAM);
	hcurSizeNwSe  = LoadCursor(NULL, IDC_SIZENWSE);
	hcurCurrent	  = hcurHourGlass;
	#if 0
	//	if (!SystemParametersInfo(SPI_GETCARETWIDTH, 0, &InsertCaretWidth, 0))
	//		/*...available in Windows 2000 only, cannot test*/
	//		InsertCaretWidth = 2;
	#endif
}

void ReleasePaintResources(void)
{
	SystemColors(FALSE);
	if (TextFont)		{DeleteObject(TextFont);	  TextFont		= 0;}
	if (SpecialFont)	{DeleteObject(SpecialFont);	  SpecialFont	= 0;}
	if (StatusFont)		{DeleteObject(StatusFont);	  StatusFont	= 0;}
	if (TooltipFont)	{DeleteObject(TooltipFont);	  TooltipFont	= 0;}
	if (Buttons)		{DeleteObject(Buttons);		  Buttons		= 0;}
	if (SelBkgndBrush)	{DeleteObject(SelBkgndBrush); SelBkgndBrush	= 0;}
	if (BkgndBrush)		{DeleteObject(BkgndBrush);	  BkgndBrush	= 0;}
}

void AdjustWindowParts(INT OldBottom, INT NewBottom)
	/*called if main window changes height or width
	 *or when language or font has changed
	 */
{	INT OldVisiLines = VisiLines;
	INT StartLine = 0;

	TextAreaHeight = NewBottom - StatusHeight - nToolbarHeight;
	if (TextHeight) {
		VisiLines = (TextAreaHeight+TextHeight-1-2*YTEXT_OFFSET) / TextHeight;
		CrsrLines = (TextAreaHeight           +2-2*YTEXT_OFFSET) / TextHeight;
	}
	if (LineInfo && CrsrLines>0) {
		PLINEINFO	NewLineInfo = (PLINEINFO)LocalAlloc
								  (LPTR, (VisiLines+2) * sizeof(LINEINFO));
		INT			NewScrollX;
		static RECT r;

		if (NewLineInfo) {
			if (OldVisiLines) {
				INT LinesToCopy = OldVisiLines;

				if (VisiLines <= OldVisiLines) {
					LinesToCopy = VisiLines;
					while (CurrLine && CurrLine >= CrsrLines) {
						++StartLine;
						--CurrLine;
						CaretY -= TextHeight;
					}
				}
				memcpy(NewLineInfo, LineInfo + StartLine,
					   (LinesToCopy + 2) * sizeof(LINEINFO));
				if (VisiLines > OldVisiLines)
					memset(NewLineInfo + OldVisiLines, 0,
						   (VisiLines - OldVisiLines) * sizeof(LINEINFO));
				if (StartLine) {
					ScreenStart = NewLineInfo[0].Pos;
					FirstVisible += StartLine;
				}
			}
			LocalFree((LOCALHANDLE)LineInfo);
			LineInfo = NewLineInfo;
		} else ErrorBox(MB_ICONSTOP, 300, 8);
		NewScrollX = ClientRect.right - GetSystemMetrics(SM_CXVSCROLL);
		if (WinVersion >= MAKEWORD(95,3)) NewScrollX -= 2;
		SetWindowPos(hwndScroll, 0, NewScrollX, nToolbarHeight,
					 GetSystemMetrics(SM_CXVSCROLL), TextAreaHeight,
					 SWP_NOZORDER);
		if (hwndMain) {
			r.top	 = nToolbarHeight;
			r.bottom = ClientRect.bottom - StatusHeight;
			if (StartLine) {
				r.left	= 0;
				r.right = ClientRect.right - GetSystemMetrics(SM_CXVSCROLL);
				InvalidateRect(hwndMain, &r, TRUE);
			}
			if (WinVersion >= MAKEWORD(95,3)) {
				--r.top;
				++r.bottom;
				if (NewScrollX > ScrollX) {
					r.left = ScrollX;
					r.right = ClientRect.right;
					InvalidateRect(hwndMain, &r, TRUE);
				} else {
					r.left = (r.right = ClientRect.right) - 2;
					InvalidateRect(hwndMain, &r, FALSE);
				}
			}
			r.left   = 0;
			r.top	 = (NewBottom < OldBottom ? NewBottom : OldBottom)
					   - StatusHeight - YTEXT_OFFSET;
			r.bottom = NewBottom;
			r.right  = ClientRect.right;
			InvalidateRect(hwndMain, &r, TRUE);
		}
		ScrollX = NewScrollX;
	}
	if (hwndCmd) SetWindowPos(hwndCmd, 0, StatusFields[0].x+1,
							  ClientRect.bottom-StatusHeight+5,
							  StatusFields[0].Width-1, StatusTextHeight,
							  SWP_NOZORDER);
	if (LineInfo) {
		UpdateWindow(hwndMain);
		if (StartLine) ShowEditCaret();
	}
}

void Jump(PPOSITION NewPos)
{	INT i, c;

	ScreenStart = *NewPos;
	if (HexEditMode) {
		GoBack(&ScreenStart,
			   (CountBytes(&ScreenStart) & 15) + (((VisiLines - 1) >> 1) << 4));
	} else {
		for (i = (VisiLines + 1) >> 1; i; --i)
			do; while (!(CharFlags(c = GoBackAndChar(&ScreenStart)) & 1));
		if (c != C_EOF) CharAndAdvance(&ScreenStart);
	}
	HideEditCaret();
	InvalidateText();
}

INT ScrollShift, ScrollRange, ScrollPos;

void NewScrollPos(void)
{	LONG		Range;
	static BOOL	Disabled;

	if (LineInfo == NULL /*not yet initialized*/) return;
	if (HexEditMode) {
		LPPAGE p;

		Range = 15;
		for (p=FirstPage; p; p=p->Next) Range += p->Fill;
		Range >>= 4;
	} else Range = LinesInFile + IncompleteLastLine();
	Range = Range < (LONG)CrsrLines ? 0 : Range - CrsrLines;
	FirstVisible = FirstPage ? CountLines(&ScreenStart) : 0;
	if (FirstVisible && Range < FirstVisible) Range = FirstVisible;
	ScrollShift	 = 0;
	while (Range > 32000) {
		++ScrollShift;
		Range >>= 1;
	}
	ScrollPos	= (INT)(FirstVisible>>ScrollShift);
	ScrollRange	= (INT)Range;
	if ((ScrollRange || ScrollPos) == Disabled) {
		EnableWindow(hwndScroll, Disabled);
		Disabled ^= TRUE^FALSE;
	}
	if (Disabled) return;
	#if defined(WIN32)
		if (WinVersion >= MAKEWORD(95,3)) {
			#if !defined(SBM_SETSCROLLINFO)
				typedef struct tagSCROLLINFO {
					ULONG cbSize;
					ULONG fMask;
					long  nMin;
					long  nMax;
					ULONG nPage;
					long  nPos;
					long  nTrackPos;
				} SCROLLINFO, FAR *LPSCROLLINFO;
				#define SBM_SETSCROLLINFO	0x00e9
				#define SIF_RANGE			1
				#define SIF_PAGE			2
				#define SIF_POS				4
				#define SIF_DISABLENOSCROLL	8
			#endif
			static SCROLLINFO ScrollInfo;
			INT				  ThumbSize;

			if (!(ThumbSize = CrsrLines >> ScrollShift)) ++ThumbSize;
			ScrollInfo.cbSize	 = sizeof(ScrollInfo);
			ScrollInfo.fMask	 = SIF_PAGE | SIF_POS | SIF_RANGE |
								   SIF_DISABLENOSCROLL;
			ScrollInfo.nMin		 = 0;
			ScrollInfo.nMax		 = ScrollRange + ThumbSize - 1;
			ScrollInfo.nPage	 = ThumbSize;
			ScrollInfo.nPos		 = ScrollPos;
			ScrollInfo.nTrackPos = ScrollPos;
			#if !defined(DIRECT_CALL_SETSCROLLINFO)
			{	DWORD i;

				i = SendMessage(hwndScroll, SBM_SETSCROLLINFO, (WPARAM)TRUE,
								(LPARAM)(LPSCROLLINFO)&ScrollInfo);
				if (i==(DWORD)ScrollPos && sizeof(INT)==4 || i) return;
			}
			#else
				do {	/*no loop, for break only*/
					static DWORD p32SetScrollInfo;
					if (!p32SetScrollInfo) {
						DWORD		h32Dll;
						static BOOL	fAlreadyTried;

						if (fAlreadyTried) break;
						fAlreadyTried = TRUE;
						h32Dll = LoadLibraryEx32W(ExpandSys32("USER32"),NULL,0);
						if (!h32Dll) break;
						p32SetScrollInfo =
							GetProcAddress32W(h32Dll, "SetScrollInfo");
						if (!p32SetScrollInfo) {
							FreeLibrary32W(h32Dll);
							break;
						}
					}
					CallProcEx32W(CPEX_DEST_STDCALL | 4, 4, p32SetScrollInfo,
						/*params...*/ hwndScroll,
									  (DWORD)SB_CTL,
									  (LPSCROLLINFO)&ScrollPos,
									  (DWORD)TRUE);
					return;
				} while (FALSE);
			#endif
		}
	#endif
	SetScrollRange(hwndScroll, SB_CTL, 0, ScrollRange, FALSE);
	SetScrollPos  (hwndScroll, SB_CTL,    ScrollPos,   TRUE);
}

extern POSITION	SelectAnchor;
extern ULONG	SelectAnchorCount;

void ExtendSelection(void)
{	ULONG BytePos = CountBytes(&CurrPos);

	if ((BytePos<SelectAnchorCount) != (SelectBytePos<SelectAnchorCount)) {
		/*reverting select direction from anchor...*/
		if (SelectCount > 0) {
			/*remove old selection totally...*/
			InvalidateArea(&SelectStart, SelectCount, 1);
			SelectCount = 0;
			UpdateWindow(hwndMain);
		}
		if (BytePos < SelectAnchorCount) {
			SelectStart = CurrPos;
			SelectCount = SelectAnchorCount-BytePos;
			SelectBytePos = BytePos;
		} else {
			SelectStart = SelectAnchor;
			SelectCount = BytePos-SelectAnchorCount;
			SelectBytePos = SelectAnchorCount;
		}
		if (SelectCount) InvalidateArea(&SelectStart, SelectCount, 0);
	} else if (BytePos>=SelectBytePos && BytePos<=SelectBytePos+SelectCount) {
		/*reducing previous selection...*/
		if (BytePos < SelectAnchorCount) {
			if (BytePos != SelectBytePos)
				InvalidateArea(&SelectStart, BytePos-SelectBytePos, 1);
			SelectStart = CurrPos;
			SelectCount = SelectAnchorCount-BytePos;
			SelectBytePos = BytePos;
		} else {
			if (BytePos != SelectBytePos+SelectCount)
				InvalidateArea(&CurrPos, SelectBytePos+SelectCount-BytePos, 1);
			SelectCount = BytePos-SelectBytePos;
		}
	} else {
		/*extending selection...*/
		if (BytePos < SelectBytePos) {
			InvalidateArea(&CurrPos, SelectBytePos-BytePos, 0);
			SelectStart = CurrPos;
			SelectCount += SelectBytePos-BytePos;
			SelectBytePos = BytePos;
		} else {
			POSITION Inv = SelectStart;

			Advance(&Inv, SelectCount);
			InvalidateArea(&Inv, BytePos-SelectBytePos-SelectCount, 0);
			SelectCount = BytePos-SelectBytePos;
		}
	}
	UpdateWindow(hwndMain);
}

extern BOOL Exiting;

void ScrollJump(INT Where)
{	ULONG	 Line;
	POSITION Pos;
	INT		 SaveLine, Tmp;
	BOOL	 Selecting;

	if (Exiting) {
		assert(!Exiting);
		return;
	}
	if (Where == ScrollPos) return;
	Selecting = GetKeyState(VK_SHIFT) < 0;
	if (Selecting) {
		if (!SelectCount) {
			SelectStart	  = SelectAnchor	  = CurrPos;
			SelectBytePos = SelectAnchorCount = CountBytes(&CurrPos);
		}
	} else {
		if (SelectCount) {
			InvalidateArea(&SelectStart, SelectCount, 1);
			SelectCount = 0;
			UpdateWindow(hwndMain);
		}
	}
	if (!ScrollRange) Line = 0;
	else Line = ((ULONG)Where << ScrollShift) +
				((1<<ScrollShift) * Where) / ScrollRange;
	BeginOfFile(&Pos);
	if (HexEditMode) {
		Pos.i = 0;
		Advance(&Pos, (Line + CrsrLines) << 4);
		GoBack(&Pos, ((CrsrLines - 1) << 4) + 1);
		Tmp = (INT)((Line = CountBytes(&Pos)) & 15);
		if (Tmp) GoBack(&Pos, Tmp);
		Line >>= 4;
	} else {
		ULONG Max;

		if (Line > (Max = LinesInFile + IncompleteLastLine() - CrsrLines))
			Line = Max;
		while (Pos.p && Line > Pos.p->Newlines) Pos.p = Pos.p->Next;
		if (!Pos.p) {
			/*should not occur, but nobody knows...*/
			assert(0);
			EndOfFile(&Pos);
		} else {
			Pos.i = 0;
			if (Pos.p->Prev) Tmp = (INT)(Line - Pos.p->Prev->Newlines);
			else Tmp = (INT)Line;
			if (CharAt(&Pos) == '\n') {
				POSITION Pos2 = Pos;

				GoBack(&Pos2, 1 + (UtfEncoding == 16));
				if (CharAt(&Pos2) == C_CRLF)
					CharAndAdvance(&Pos);
			}
			while (Tmp) {
				INT c;

				if ((c = CharFlags(CharAndAdvance(&Pos))) & 1) --Tmp;
				if (c & 0x20) break;
			}
		}
	}
	HideEditCaret();

	/*scroll if ScreenStart and Pos are less than CrsrLines apart...*/
	Line -= CountLines(&ScreenStart);
	if ((LONG)Line < CrsrLines && -(LONG)Line < CrsrLines) {
		/*scroll (using single ScrollDC for more speed)...*/
		RECT r1, r2;
		HDC  hDC;

		if (hwndMain && (hDC = GetDC(hwndMain)) != 0) {
			r1 = ClientRect;
			TextAreaOnly(&r1, TRUE);
			Tmp = (INT)Line;
			ScrollDC(hDC, 0, Tmp * -TextHeight, &r1, &r1, NULL, &r2);
			ReleaseDC(hwndMain, hDC);

			if (Tmp > 0) {
				memmove(LineInfo, LineInfo+Tmp,
				 		(VisiLines-Tmp+1) * sizeof(LINEINFO));
				memset(LineInfo+VisiLines-Tmp+1, 0, Tmp * sizeof(LINEINFO));
				r2.bottom += YTEXT_OFFSET;	/*invalidate bitmap border area*/
			} else {
				memmove(LineInfo-Tmp, LineInfo,
						(VisiLines+Tmp+1) * sizeof(LINEINFO));
				r2.top -= YTEXT_OFFSET;		/*invalidate bitmap border area*/
			}
			ScreenStart = Pos;
			SaveLine = CurrLine;
			FirstVisible += Tmp;
			InvalidateRect(hwndMain, &r2, TRUE);
			UpdateWindow(hwndMain);
			if (*BitmapFilename) {
				/*repaint other end of scrolled area...*/
				if (Tmp > 0) {
					r1.top	  = nToolbarHeight;
					r1.bottom = r1.top + YTEXT_OFFSET;
				} else {
					r1.bottom = ClientRect.bottom - StatusHeight;
					r1.top	  = r1.bottom - YTEXT_OFFSET;
				}
				InvalidateRect(hwndMain, &r1, TRUE);
			}
			Tmp = 0;
			while ((CurrPos = LineInfo[SaveLine].Pos).p == NULL) {
				--SaveLine;
				++Tmp;
			}
			NewScrollPos();
			if (Tmp) Position(Tmp, 'j', 0);
			else {
				AdvanceToCurr(&CurrPos, Mode==InsertMode || Mode==ReplaceMode);
				NewPosition(&CurrPos);
				if (Selecting) ExtendSelection();
			}
			ShowEditCaret();
			Indenting = FALSE;
			return;
		}
	}

	ScreenStart = Pos;
	SaveLine = CurrLine;
	InvalidateText();
	CurrPos = LineInfo[SaveLine].Pos;
	NewScrollPos();
	AdvanceToCurr(&CurrPos, Mode==InsertMode || Mode==ReplaceMode);
	NewPosition(&CurrPos);
	if (Selecting) ExtendSelection();
	ShowEditCaret();
	Indenting = FALSE;
}

BOOL ScrollLine(INT Dir)
{	HDC  hDC;
	RECT r1, r2;

	if (Exiting) {
		assert(!Exiting);
		return (FALSE);
	}
	if (Dir > 0) memmove(LineInfo, LineInfo+1, VisiLines * sizeof(LINEINFO));
	else		 memmove(LineInfo+1, LineInfo, VisiLines * sizeof(LINEINFO));
	hDC = GetDC(hwndMain);
	if (hDC) {
		r1 = ClientRect;
		TextAreaOnly(&r1, TRUE);
		ScrollDC(hDC, 0, Dir>0 ? -TextHeight : TextHeight, &r1, &r1, NULL, &r2);
		ReleaseDC(hwndMain, hDC);
	} else {
		r2 = ClientRect;
		TextAreaOnly(&r2, TRUE);
	}
	if (Dir > 0) {
		ScreenStart = LineInfo[0].Pos;
		LineInfo[VisiLines].Flags = 0;
		++FirstVisible;
		r2.bottom += YTEXT_OFFSET;
	} else {
		if (HexEditMode) GoBack(&ScreenStart, 16);
		else {
			INT c;

			GoBackAndChar(&ScreenStart);
			do c = GoBackAndChar(&ScreenStart);
			while (!(CharFlags(c) & 1));
			if (c != C_EOF) CharAndAdvance(&ScreenStart);
		}
		LineInfo[0].Flags = 0;
		LineInfo[1].Flags &= ~LIF_VALID;
		--FirstVisible;
		r2.top -= YTEXT_OFFSET;
	}
	if (hwndMain) {
		InvalidateRect(hwndMain, &r2, TRUE);
		UpdateWindow(hwndMain);
		if (*BitmapFilename) {
			/*repaint other end of scrolled area...*/
			if (Dir > 0) {
				r1.top	  = nToolbarHeight;
				r1.bottom = r1.top + YTEXT_OFFSET;
			} else {
				r1.bottom = ClientRect.bottom - StatusHeight;
				r1.top	  = r1.bottom - YTEXT_OFFSET;
			}
			InvalidateRect(hwndMain, &r1, TRUE);
		}
	}
	if (Dir > 0) {
		if (!LineInfo[VisiLines].Flags)
			/*not painted probably because invisible, fake painting...*/
			TextLine(NULL, VisiLines-1,
					 nToolbarHeight+YTEXT_OFFSET+(VisiLines-1)*TextHeight);
	}
	return (hDC != 0);
}

INT	  AveCharWidth;
WCHAR StatusLineFormat[8]=L"%05lu", StatusColFormat[8]=L"%03lu";

long NewPosition(PPOSITION NewPos)
{	static WCHAR LineBuf[12], ColumnBuf[12];
	static WCHAR PercentBuf[10];
	static ULONG OldLine, OldColumn, OldLines;
	static INT	 OldPercent, ScrollMax;
	POSITION	 ScreenEnd;
	ULONG		 Bytes, Lines, NoOfLines;

	CurrPos = *NewPos;
	Bytes	= CountBytes(NewPos);
	Lines	= CountLines(NewPos);
	if (!(ScrollMax=MaxScroll) && (ScrollMax=CrsrLines-2)<0) ScrollMax = 0;

	if (Exiting) {
		assert(!Exiting);
		return (0);
	}
	/*first, move current position into view...*/
	if (ScreenStart.p && (		 NewPos->p->PageNo <  ScreenStart.p->PageNo
						  || (	 NewPos->p->PageNo == ScreenStart.p->PageNo
							  && NewPos->i		   <  ScreenStart.i))) {
		/*go up in file...*/
		if (FirstVisible - (LONG)Lines > (LONG)ScrollMax)
			Jump(NewPos); /*hard jump*/
		else {
			/*scroll back (rolling down)*/
			HideEditCaret();
			do ScrollLine(-1);
			while (Bytes < LineInfo[0].ByteCount);
		}
		NewScrollPos();
	} else if ((ScreenEnd = LineInfo[CrsrLines].Pos).p
			   && (NewPos->p->PageNo	 >  ScreenEnd.p->PageNo
				   || (NewPos->p->PageNo == ScreenEnd.p->PageNo
					   && NewPos->i		 >= ScreenEnd.i))
			   && !(LineInfo[CrsrLines].Flags & LIF_EOF)
			   && !(LineInfo[1].Flags & LIF_EOF)) {
		/*go down...*/
		if ((long)(Lines - CountLines(&ScreenEnd)) >= (long)ScrollMax)
			Jump(NewPos); /*hard jump*/
		else {
			/*scroll forward (rolling up)*/
			HideEditCaret();
			do ScrollLine(1);
			while (Bytes >= LineInfo[CrsrLines].ByteCount &&
				   !(LineInfo[CrsrLines].Flags & LIF_EOF));
		}
		NewScrollPos();
	} else {
		/*calculate new caret position within screen...*/
		INT i;

		for (i = LineInfo[1].Flags & LIF_EOF ? 0 : CrsrLines;
			 i && ( !(LineInfo[i].Flags & LIF_VALID)
				   || LineInfo[i].ByteCount > Bytes
				   || LineInfo[i].Flags & LIF_EOF);
			 --i) {}
		if (LineInfo[i].ByteCount <= Bytes) {
			INT InitCaretX, CaretUpperY;

			/*set caret pos and dimension just for the case sth goes wrong...*/
			CaretX = InitCaretX = StartX(FALSE);
			CaretY = CaretUpperY = nToolbarHeight + YTEXT_OFFSET + i*TextHeight;
			if (Mode == ReplaceMode)
				 CaretType = REPLACE_CARET;
			else if ((CharFlags(CharAt(&CurrPos)) & 33) == 1)
				 CaretType = COMMAND_CARET_EMPTYLINE;
			else CaretType = COMMAND_CARET;
			CaretHeight = TextHeight;
			CaretWidth = AveCharWidth;
			TextLine(0, i, CaretUpperY);
			while ( !(LineInfo[i+1].Flags & LIF_EOF)
				   && LineInfo[i+1].Flags & LIF_VALID
				   && LineInfo[i+1].ByteCount <= Bytes) {
				if (i+1 >= CrsrLines) {
					ScrollLine(1);
					UpdateWindow(hwndMain);
				} else {
					++i;
					CaretY		+= TextHeight;
					CaretUpperY	+= TextHeight;
					TextLine(0, i, CaretUpperY);
				}
			}
			if (CharAt(&CurrPos)==C_EOF && CaretX==InitCaretX &&
					!(LineInfo[i].Flags & LIF_EOF))
				CaretY += TextHeight;
		}
	}

	/*finally, update status row...*/
	if (HexEditMode) {
		if (LastPage != NULL) {
			ScreenEnd.p = LastPage;
			ScreenEnd.i = LastPage->Fill;
			ScreenEnd.f = 0;	/*TODO: fill flags*/
			NoOfLines = (CountBytes(&ScreenEnd) + 15) >> 4;
		} else NoOfLines = 0;
		CurrColumn = Bytes & 15;
	} else NoOfLines = LinesInFile + IncompleteLastLine();
	++Lines;
	{	static ULONG StatusLineLimit=100000, StatusColLimit=1000;
		static INT	 StatusLineDigits=5,	 StatusColDigits=3;
		PWSTR		 p;

		if (Lines >= StatusLineLimit || CurrColumn >= StatusColLimit) {
			/*enlarge line number and/or column field in status line...*/
			RECT r;

			while (CurrColumn >= StatusColLimit) {
				StatusColLimit *= 10;
				++StatusColDigits;
				OldColumn = 0;
				if (StatusLineLimit < StatusColLimit*100
					&& StatusLineLimit < 1000000000L) {
						/*The field for the current line number should be wider
						 *than the field for the current column if possible.
						 */
						StatusLineLimit *= 10;
						++StatusLineDigits;
						OldLine = 0;
				}
			}
			while (Lines >= StatusLineLimit && StatusLineLimit < 1000000000L) {
				StatusLineLimit *= 10;
				++StatusLineDigits;
				OldLine = 0;
			}
			_snwprintf(StatusLineFormat, WSIZE(StatusLineFormat),
					   p = L"%%0%dlu", StatusLineDigits);
			_snwprintf(StatusColFormat, WSIZE(StatusColFormat),
					   p, StatusColDigits);
			RecalcStatusRow();
			r = ClientRect;
			r.top = r.bottom - StatusHeight + 4;
			InvalidateRect(hwndMain, &r, TRUE);
		}
		if (Lines != OldLine || NoOfLines != OldLines) {
			INT Percent;

			Percent = NoOfLines
					  ? Lines<=21474836L ? (INT)((100 * Lines) / NoOfLines)
										 : (INT)(Lines / (NoOfLines / 100))
					  : 0;
			if (Percent != OldPercent || NoOfLines != OldLines) {
				_snwprintf(PercentBuf, WSIZE(PercentBuf),
						   L"--%d%%--", OldPercent = Percent);
				NewStatus(4, PercentBuf, NS_NORMAL);
				OldLines = NoOfLines;
			}
			if (Lines != OldLine) {
				_snwprintf(LineBuf, WSIZE(LineBuf),
						   StatusLineFormat, OldLine = Lines);
				NewStatus(5, LineBuf, NS_NORMAL);
			}
		}
		if (CurrColumn != OldColumn) {
			_snwprintf(ColumnBuf, WSIZE(ColumnBuf),
					   StatusColFormat, OldColumn = CurrColumn);
			NewStatus(6, ColumnBuf, NS_NORMAL);
		}
	}
	return (0);
}

BOOL PaintAll;
LONG ExpandRedraw, ShrinkRedraw;

void InvalidateText(void)
{	RECT r;

	if (!hwndMain) return;
	StartX(TRUE);
	r = ClientRect;
	TextAreaOnly(&r, FALSE);
	FirstVisible = CountLines(&ScreenStart);
	ValidateRect(hwndMain, &r);
	UpdateWindow(hwndMain);
	memset(LineInfo, 0, (VisiLines+2) * sizeof(LINEINFO));
	PaintAll = TRUE;
	ExpandRedraw = 2000000000L;
	ShrinkRedraw = 0;
	InvalidateRect(hwndMain, &r, TRUE);
	UpdateWindow(hwndMain);
	if (!LineInfo[0].Flags || !LineInfo[CrsrLines].Flags) {
		/*not painted, probably hidden: fake painting...*/
		HDC hDC = GetDC(hwndMain);

		if (hDC) {
			Paint(hDC, &r);
			ReleaseDC(hwndMain, hDC);
		}
	}
}

INT ComparePos(PPOSITION First, PPOSITION Second)
{
	if (First->p == Second->p)
		return ((INT)First->i - (INT)Second->i);
	if (First->p == NULL) {
		/* TODO: Should not occur, but it sometimes happens
		 * when joining multiple lines (e.g. "3000J"),
		 * called from InvalidateArea().  Examine!
		 */
		return 1;
	}
	return (First->p->PageNo < Second->p->PageNo ? -1 : 1);
}

#if !defined(WIN32)
BOOL GetTextExtentExPoint(HDC hDC, LPSTR lpStr, INT nLen, INT nMaxExtent,
						  LPINT lpnFit, LPINT nMustBeNULL, LPSIZE lpSize) {
	INT nTryLen = nLen;

	do {
		if (!GetTextExtentPoint(hDC, lpStr, nTryLen, lpSize)) return (FALSE);
		if (lpSize->cx <= nMaxExtent) {
			*lpnFit = nTryLen;
			return (TRUE);
		}
	} while (--nTryLen >= 0);
	return (FALSE);
}
#endif

extern INT	OemCodePage, AnsiCodePage;
BOOL		IsFixedFont = FALSE;

WCHAR const MapLowOemToUtf16[33] = {
			0x0020, 0x263A, 0x263B, 0x2665,  0x2666, 0x2663, 0x2660, 0x2022,
			0x25D8, 0x25CB, 0x25D9, 0x2642,  0x2640, 0x266A, 0x266B, 0x263C,
			0x25BA, 0x25C4, 0x2195, 0x203C,  0x00B6, 0x00A7, 0x25AC, 0x21A8,
			0x2191, 0x2193, 0x2192, 0x2190,  0x221F, 0x2194, 0x25B2, 0x25BC, 0};

VOID UnicodeConvert(WCHAR *Dst, WCHAR *Src, INT Len)
{	INT	 i;
	CHAR b[160];

	if (UtfEncoding) {
		memmove(Dst, Src, Len * sizeof(WCHAR));
		return;
	}
	for (i = 0; i < Len; ++i) b[i] = (CHAR)Src[i];
	MultiByteToWideChar(CharSet==CS_OEM ? OemCodePage : AnsiCodePage,
						0, b, Len, Dst, Len);
	if (CharSet == CS_OEM) {
		for (i = 0; i < Len; ++i)
			if (Dst[i] < ' ') Dst[i] = MapLowOemToUtf16[Dst[i]];
	}
}

INT UnicodeToOemChar(WCHAR Wchar)
{	BYTE  Result;
	PWSTR p;

	if (Wchar == ' ')
		return ' ';
	if ((p = wcschr(MapLowOemToUtf16, Wchar)) != NULL)
		return p - MapLowOemToUtf16;
	WideCharToMultiByte(OemCodePage, 0, &Wchar, 1, &Result, 1, NULL, NULL);
	return Result;
}

INT GetTextWidth(HDC hDC, WCHAR *String, INT Len)
{	SIZE  Size;
	WCHAR Buf2[160];

	if (IsFixedFont) return (Len * AveCharWidth);
	if (!UtfEncoding) {
		UnicodeConvert(Buf2, String, Len);
		String = Buf2;
	}
	if (!hDC) {
		/*No output, calculating cursor position only*/
		HDC   hDC;
		INT   Ret;
		HFONT Font = HexEditMode ? HexFont : TextFont, OldFont;

		hDC = GetDC(NULL);
		if (!hDC) return (0);
		if (Font) OldFont = SelectObject(hDC, Font);
		Ret = GetTextExtentPointW(hDC, String, Len, &Size) && Size.cx > 0
			  ? Size.cx : 0;
		if (Font) SelectObject(hDC, OldFont);
		ReleaseDC(NULL, hDC);
		return Ret;
	}
	return GetTextExtentPointW(hDC, String, Len, &Size) && Size.cx > 0
		   ? Size.cx : 0;
}

INT GetTextPos(PPOSITION StartParam, PPOSITION End, INT ColParam)
{	LONG         Len = 0, Col = ColParam;
	static WCHAR Buf[160];
	WCHAR        *p = Buf;
	POSITION     Start;

	if (HexEditMode) return (0);
	if (CountBytes(StartParam)<=3 && UtfEncoding && CharAt(StartParam)==C_BOM)
		CharAndAdvance(StartParam);
	Start = *StartParam;
	while (Start.i >= Start.p->Fill && Start.p->Next) {
		Start.i -= Start.p->Fill;
		Start.p  = Start.p->Next;
	}
	while (ComparePos(&Start, End) < 0) {
		INT c = CharAndAdvance(&Start);

		if (CharFlags(c) & 1) {
			Col = 0;
			p = Buf;
		} else switch (c) {
			case '\t':	Col += p - Buf;
						Col = Col + TabStop - Col % TabStop;
						Len = Col * AveCharWidth;
						p = Buf;
						break;

			default:	if (CharFlags(c) & 2) {
							if (c < ' ') {
								*p++ = '^';
								*p++ = c + ('A'-1);
							} else {
								BYTE Buf3[8], *p3;

								wsprintf(Buf3, "\\%02X", c);
								for (p3 = Buf3; *p3 != '\0'; ++p3) *p++ = *p3;
							}
						} else *p++ = c;
		}
		if (p >= Buf + WSIZE(Buf) - 3
			|| p >= Buf + WSIZE(Buf) - 6
			   && IsDBCSLeadByteEx(CharSet == CS_OEM ? CP_OEMCP : CP_ACP,
			   					   (BYTE)CharAt(&Start))) {
			Len += GetTextWidth(NULL, Buf, p-Buf);
			Col += p - Buf;
			p = Buf;
		}
	}
	if (p > Buf) Len += GetTextWidth(NULL, Buf, p-Buf);
	return (Len-xOffset <= ClientRect.right
			? (INT)(Len-xOffset) : ClientRect.right);
}

void InvalidateArea(PPOSITION Start, LONG Length, UINT Flags)
	/*Flags: 1 = erase flag for invalidation
	 *		 2 = invalidate to end of screen if area contains newlines
	 *		 4 = invalidate to end of screen even if doesn't seem necessary
	 *		 8 = invalidate to end of last line
	 */
{	INT		 i = 0, Cmp;
	RECT	 r;
	POSITION End;
	BOOL     NewLineFound = Flags & 4;

	if (!hwndMain) return;
	r = ClientRect;
	TextAreaOnly(&r, TRUE);
	/*find first involved line...*/
	while (i < VisiLines && ComparePos(&LineInfo[i+1].Pos, Start) <= 0
						 && !(LineInfo[i].Flags & LIF_EOF)) {
		++i;
		r.top += TextHeight;
	}
	if (LineInfo[i].Flags & LIF_EOF)
		if (i && ComparePos(&LineInfo[i].Pos, Start) >= 0) {
			--i;
			r.top -= TextHeight;
			Flags |= 8;
		}
	/*advance to first involved character in line...*/
	if (ComparePos(&LineInfo[i].Pos, Start) >= 0) r.left = 0;
	else r.left = StartX(FALSE) - 1 + GetTextPos(&LineInfo[i].Pos, Start, 0);
	/*find end position...*/
	End = *Start;
	while ((long)End.i + Length >= (long)End.p->Fill && End.p->Next) {
		Length -= End.p->Fill - End.i;
		End.p = End.p->Next;
		End.i = 0;
	}
	if ((long)End.i + Length > (long)End.p->Fill)
		Length = End.p->Fill - End.i;
	End.i += (INT)Length;
	/*finally, invalidate all following lines...*/
	while (i < VisiLines) {
		r.bottom = r.top + TextHeight;
		if ((Cmp = ComparePos(&LineInfo[++i].Pos, &End)) >= 0) {
			if ((!NewLineFound && Cmp > 0) || !(Flags & 2)) {
				if (Cmp > 0 && !(Flags & 10) && !HexEditMode)
					r.right = StartX(FALSE) + 1 +
							  GetTextPos(&LineInfo[i-1].Pos, &End, 0);
				InvalidateRect(hwndMain, &r, Flags & 1);
				break;
			}
		}
		InvalidateRect(hwndMain, &r, Flags & 1);
		NewLineFound = TRUE;
		r.left = 0;
		r.top += TextHeight;
	}
}

void TextAreaOnly(RECT *r, BOOL WithinOffsets)
{
	if (WithinOffsets) {
		if (r->top	  < nToolbarHeight + YTEXT_OFFSET)
			r->top	  = nToolbarHeight + YTEXT_OFFSET;
		if (r->bottom > ClientRect.bottom-StatusHeight - YTEXT_OFFSET)
			r->bottom = ClientRect.bottom-StatusHeight - YTEXT_OFFSET;
	} else {
		if (r->top	  < nToolbarHeight)
			r->top	  = nToolbarHeight;
		if (r->bottom > ClientRect.bottom-StatusHeight)
			r->bottom = ClientRect.bottom-StatusHeight;
	}
	if (WinVersion >= MAKEWORD(95,3)) {
		if (r->left		< 2) r->left = 2;
		if (r->right	> ClientRect.right-GetSystemMetrics(SM_CXVSCROLL)-2)
			r->right	= ClientRect.right-GetSystemMetrics(SM_CXVSCROLL)-2;
	} else if (r->right > ClientRect.right-GetSystemMetrics(SM_CXVSCROLL))
			   r->right = ClientRect.right-GetSystemMetrics(SM_CXVSCROLL);
}

static WCHAR Buf[160];
static BOOL	 MeasureCaret, NewCaret;
INT 		 DispLine = -1, TabStop = 8, RightOffset = 0;
extern BOOL	 ShowParenMatch;

static void Flush(HDC hDC, LONG x, INT y, WCHAR *Buf, INT Len, BOOL Selected)
{
	/*----------------------------------------------------------------------*/
	/* called from:	TextLine												*/
	/*----------------------------------------------------------------------*/
	if (hDC && Len && (x-=xOffset) > -20000 && x < ClientRect.right) {
		WCHAR Buf2[160];
		INT	  i, c;

		if (Selected) {
			SetBkMode(hDC, OPAQUE);
			if (ShowParenMatch) {
				SetBkColor	(hDC, ShmBkgndColor);
				SetTextColor(hDC, ShmTextColor);
			} else {
				SetBkColor	(hDC, SelBkgndColor);
				SetTextColor(hDC, SelTextColor);
			}
		} else	SetTextColor(hDC, TextColor);
		if (!UtfEncoding) {
			UnicodeConvert(Buf2, Buf, Len);
			Buf = Buf2;
		}
		if (HexEditMode)
			for (i = 0; i < Len; ++i)
				if ((c = Buf[i]) < 0x100 && (c & 0x60) == 0 || c == 0x7f) {
					switch (c) {
						case '\0':		c = 0x00b0;	break;
						case '\t':		c = 0x203a;	break;
						case '\r':		c = 0x25c4;	break;
						case '\n':		c = 0x25bc;	break;
						case '\177':	c = 0x007f;	break;
						default:		c = 0x00b7;	break;
					}
					Buf[i] = c;
				}
		TextOutW(hDC, (INT)x, y, Buf, Len);
		if (Selected) SetBkMode(hDC, TRANSPARENT);
	}
}

static BOOL EnforcePainting;
BOOL SuppressPaint;

static void PrepareNextLine(PPOSITION p, ULONG ByteCount, BOOL Continued)
{
	if (Continued)
		 LineInfo[DispLine].Flags |=  LIF_CONTD_NEXTLINE;
	else LineInfo[DispLine].Flags &= ~LIF_CONTD_NEXTLINE;
	++DispLine;
	if (LineInfo[DispLine].Pos.p != p->p		  ||
		LineInfo[DispLine].Pos.i != p->i		  ||
		LineInfo[DispLine].ByteCount != ByteCount ||
		LineInfo[DispLine].nthContdLine !=
			 (Continued ? LineInfo[DispLine-1].nthContdLine+1 : 0) ||
		!(LineInfo[DispLine].Flags & LIF_VALID))
	{
		LineInfo[DispLine].Pos = *p;
		LineInfo[DispLine].Flags |= LIF_VALID;
		LineInfo[DispLine].nthContdLine =
							Continued ? LineInfo[DispLine-1].nthContdLine+1 : 0;
		LineInfo[DispLine].ByteCount = ByteCount;
		EnforcePainting = TRUE;
	}
	if (ByteAt(p) == C_EOF) LineInfo[DispLine].Flags |= LIF_EOF;
	else LineInfo[DispLine].Flags &= ~LIF_EOF;
}

INT DefaultNewLine = C_CRLF;

INT StartX(BOOL OkToRecalc)
{	static INT NumberDigits, NumberPower10, PrevNumberDigits;

	if (!NumberFlag || HexEditMode) return (XTEXT_OFFSET);
	if (!NumberDigits || OkToRecalc) {
		NumberDigits = 4;
		NumberPower10 = 1000;
		while (LinesInFile / NumberPower10 >= 9) {
			++NumberDigits;
			NumberPower10 *= 10;
		}
		if (NumberDigits != PrevNumberDigits) {
			PrevNumberDigits = NumberDigits;
			InvalidateText();
		}
	}
	return (XTEXT_OFFSET + (NumberDigits + 1) * AveCharWidth);
}

void TextLine(HDC hDC, INT Line, INT YPos)
	/*----------------------------------------------------------------------*/
	/* calls:		CharAndAdvance	  CountBytes	Flush	 GetTextWidth	*/
	/*	  WinApi:	wsprintf												*/
	/*																		*/
	/* called from:	NewPosition		  Paint									*/
	/*----------------------------------------------------------------------*/
{	static POSITION p;
	INT 			i, c, i2;
	LONG			x, x2, Pos, LineStart;
	ULONG			ByteCount;
	BOOL			WithinSelect = FALSE;

	if (SuppressPaint) return;
	EnforcePainting = FALSE;
	if (!Line || LineInfo[Line].Flags & LIF_VALID) {
		DispLine = Line;
		p = Line ? LineInfo[Line].Pos : ScreenStart;
		if (p.p == NULL) return;
	} else {
		p = ScreenStart;
		if (p.p == NULL) return;
		DispLine = 0;
		while (DispLine != Line) {
			if (HexEditMode) Advance(&p, 16);
			else if (!BreakLineFlag) {
				for (;;) {
					c = CharAndAdvance(&p);
					if (c < 0 || (CharFlags(c) & 1)) break;
				}
				if (c == '\r' && CharAt(&p) == '\n') CharAndAdvance(&p);
			} else {
				/*line too long, must be split*/
			}
			++DispLine;
		}
	}
	x2 = Pos = i = i2 = 0;
	MeasureCaret = FALSE;
	x = StartX(Line==0);
	if (NumberFlag && !HexEditMode) {
		/*display line number...*/

		if (CharAt(&p) != C_EOF) {
			WCHAR NumberBuf[12];
			CHAR  Num[12];
			INT	  Width, Len, i;

			Len = wsprintf (Num, "%lu", CountLines(&p) + 1);
			for (i = 0; i <= Len; ++i) NumberBuf[i] = Num[i];
			Width = GetTextWidth(hDC, NumberBuf, Len);
			Flush(hDC, x - AveCharWidth - Width, YPos, NumberBuf, Len, 0);
		}
	}
	LineStart = x;
	LineInfo[DispLine].Pos = p;
	LineInfo[DispLine].Flags = LIF_VALID;
	LineInfo[DispLine].ByteCount = ByteCount = CountBytes(&p);
	if (SelectCount && ByteCount > SelectBytePos
					&& ByteCount < SelectBytePos+SelectCount) {
		WithinSelect = TRUE;
	}
	ByteAt(&p);
	for (;;) {
		extern BOOL		HexEditTextSide;
		extern COLORREF	EolColor;
		static WCHAR	Buf2[18];	/*right column in hex edit mode*/

//		if (!p.p->Fill && p.p->Next) p.p = p.p->Next;
//		/*...I do not understand this currently.
//		 *Without this line, the input of
//		 *   "i<blank><ctrl+bksp><esc><pgdown><crsr-right><ctrl+home>"
//		 *does not change the cursor position (after <ctrl+home>), though
//		 *in fact, the cursor is at the home position.	8-Aug-97 Ramo
//		 */

		if (p.i==CurrPos.i && p.p==CurrPos.p) {
			if (!Line
				|| (Line <= CrsrLines
					&& (p.i < p.p->Fill
						|| i
						|| x != LineStart
						|| (!HexEditMode
							&& !(LineInfo[Line-1].Flags
								 & (LIF_EOF | LIF_CONTD_NEXTLINE)))))) {
				if (i) {
					INT Width = GetTextWidth(hDC, Buf, i);

					Pos += i;
					if (WithinSelect && HexEditMode &&
							ByteCount >= SelectBytePos+SelectCount && i)
						--i;
					if (x-xOffset < ClientRect.right)
						Flush(hDC, x, YPos, Buf, i, WithinSelect);
					x += Width;
					i = 0;
				}
				CaretX = x;
				if (HexEditMode && HexEditTextSide) {
					if (i2) {
						Flush(hDC, x2, YPos, Buf2, i2, WithinSelect);
						x2 += GetTextWidth(hDC, Buf2, i2);
						i2 = 0;
					}
					CaretX += x2-x;
				}
				if (Mode == ReplaceMode)
					 CaretType = REPLACE_CARET;
				else CaretType = COMMAND_CARET;
				CaretHeight = TextHeight;
				CaretY = YPos;
				NewCaret = MeasureCaret = TRUE;
				CurrLine = Line;
				CurrColumn = Pos + 1;
			}
		}
		if (SelectCount &&
				(WithinSelect ? ByteCount >= SelectBytePos+SelectCount
							  : ByteCount == SelectBytePos)) {
			if (HexEditMode) {
				if (i2) {
					Flush(hDC, x2, YPos, Buf2, i2, WithinSelect);
					x2 += GetTextWidth(hDC, Buf2, i2);
					i2 = 0;
				}
				if (WithinSelect && i) --i;
			}
			if (i) {
				Pos += i;
				if (x-xOffset < ClientRect.right)
					Flush(hDC, x, YPos, Buf, i, WithinSelect);
				x += GetTextWidth(hDC, Buf, i);
				i = 0;
				if (WithinSelect && HexEditMode) Buf[i++] = ' ';
			}
			WithinSelect ^= TRUE;
		}
		++ByteCount;
		if (HexEditMode) {
			INT	c2;

			switch (c = ByteAndAdvance(&p)) {
				case C_EOF:
					if (!i && x==LineStart) {
						LineInfo[DispLine].Flags |= LIF_EOF;
						if (hDC) {
							SetTextColor(hDC, EolColor);
							TextOut(hDC, (INT)x, YPos+2, "~", 1);
						}
					}
					#if 0
					//	else LineInfo[DispLine].Flags |= LIF_CONTD_NEXTLINE;
					#endif
					--ByteCount;
					if (MeasureCaret) {
						CaretWidth = AveCharWidth;
						if (!HexEditTextSide) CaretWidth <<= 1;
						if (Mode == InsertMode) CaretType = INSERT_CARET;
						else {
							if (HexEditTextSide)
								CaretType = Mode == ReplaceMode
													? REPLACE_CARET_DOUBLEHEX
													: COMMAND_CARET_DOUBLEHEX;
							else
								CaretType = Mode == ReplaceMode
													? REPLACE_CARET
													: COMMAND_CARET_EMPTYLINE;
						}
						MeasureCaret = FALSE;
						if (!hDC) return;
					}
					break;

				default:
					if ((ByteCount & 15) == 1 /*already incremented*/) {
						INT  Width;
						BYTE Buf3[12];

						if (HexNumberFlag)
							 wsprintf(Buf3, ByteCount > 0x1000000L ?  "%07lx:"
																   : " %06lx:",
										  ByteCount-1);
						else wsprintf(Buf3, ByteCount > 1000000L   ?  "%07ld:"
																   : " %06ld:",
										  ByteCount-1);
						for (i = 0; Buf3[i] != '\0'; ++i) Buf[i] = Buf3[i];
						Flush(hDC, x, YPos, Buf, i, FALSE);
						i = 0;
						x += Width = 10 * AveCharWidth;
						Buf2[0]    = '|';
						Buf2[i2=1] = '\0';
						x2 = 62 * AveCharWidth;
						if ((WithinSelect || MeasureCaret) && i2) {
							Flush(hDC, x2, YPos, Buf2, i2, FALSE);
							x2 += GetTextWidth(hDC, Buf2, i2);
							i2 = 0;
						}
						if (MeasureCaret) {
							if (HexEditTextSide) CaretX = x2;
							else CaretX += Width;
						}
					}
					switch (UtfEncoding) {
						case 16:
							{	BYTE Buf3[4], *p;

								wsprintf(Buf3, "%02x ", c);
								for (p = Buf3; *p != '\0'; ++p)
									Buf[i++] = *p;
							}
							if (MeasureCaret) {
								extern INT HexEditFirstNibble;

								if (HexEditFirstNibble != -1) {
									CaretX += GetTextWidth(hDC, Buf, 1);
									if (Mode != InsertMode)
										CaretWidth = GetTextWidth(hDC,Buf+1,1);
								} else if (HexEditTextSide)
									CaretWidth = GetTextWidth(hDC, Buf2, 1);
								else {
									BYTE Buf3[4];

									wsprintf(Buf3, "%02x", ByteAt(&p) & 255);
									Buf[i]	 = Buf3[0];
									Buf[i+1] = Buf3[1];
									CaretWidth = GetTextWidth(hDC, Buf, 5);
								}
								MeasureCaret = FALSE;
							}
							if (p.i==CurrPos.i && p.p==CurrPos.p) {
								if (i) {
									INT Width = GetTextWidth(hDC, Buf, i);

									Pos += i;
									if (x-xOffset < ClientRect.right)
										Flush(hDC, x, YPos, Buf, i,
											  WithinSelect);
									x += Width;
									i = 0;
								}
								CaretX = x;
								if (Mode == ReplaceMode)
									 CaretType = REPLACE_CARET;
								else CaretType = COMMAND_CARET;
								CaretHeight = TextHeight;
								CaretY = YPos;
								NewCaret = MeasureCaret = TRUE;
								CurrLine = Line;
								CurrColumn = Pos + 1;
							}
							c2 = ByteAndAdvance(&p);
							if (UtfLsbFirst) {
								c2 = (c2 << 8) | c;
								c  = c2 >> 8;
							} else {
								c2 = (c << 8) | c2;
								c  = c2 & 255;
							}
							++ByteCount;
							break;

						case 8:
							if (c >= 0x80 && c < 0xc0) c2 = 0xb7;
							else if (c < 0x80) c2 = c;
							else {
								GoBack(&p, 1);
								c2 = CharAt(&p);
								Advance(&p, 1);
								if (c2 == C_UNDEF) c2 = 0x95;
							}
							break;

						default:
							if (CharSet == CS_EBCDIC) c2 = MapEbcdicToAnsi[c];
							else c2 = c;
							break;
					}
					Buf2[i2++] = c2;
					{	BYTE Buf3[4], *p;

						wsprintf(Buf3, "%02x", c);
						for (p = Buf3; *p != '\0'; ++p)
							Buf[i++] = *p;
					}
					if (ByteCount & 3) Buf[i++] = ' ';
					if (MeasureCaret) {
						extern INT HexEditFirstNibble;

						if (HexEditTextSide)
							 CaretWidth = GetTextWidth(hDC, Buf2, 1);
						else CaretWidth = GetTextWidth(hDC, Buf,  2);
						if (HexEditFirstNibble != -1) {
							int w = GetTextWidth(hDC, Buf+1, 1);

							CaretX += CaretWidth - w;
							if (Mode != InsertMode) CaretWidth = w;
						}
						MeasureCaret = FALSE;
					}
			}
			if (!(ByteCount & 3) || c == C_EOF) {
				Flush(hDC, x, YPos, Buf, i, WithinSelect);
				x += GetTextWidth(hDC, Buf, i);
				i = 0;
				if (!(ByteCount & 15) || c == C_EOF) {
					if (c != C_EOF && p.i >= p.p->Fill) continue;
					if (i2) {
						Flush(hDC, x2, YPos, Buf2, i2, WithinSelect);
						x2 += GetTextWidth(hDC, Buf2, i2);
					}
					if (x2) Flush(hDC, x2, YPos, L"|", 1, FALSE);
					PrepareNextLine(&p, ByteCount, FALSE);
					return;
				}
				x = (INT)(10 * AveCharWidth + LineStart +
						((51 * AveCharWidth * (ByteCount & 15)) >> 4));
			}
		} else {
			POSITION OldPos = p;
			INT		 c2;

			if (CharSet == CS_EBCDIC) {
				c = c2 = ByteAndAdvance(&p);
				if (c >= 0 && (c = MapEbcdicToAnsi[c]) == '\r'
						   && ByteAt(&p) == MapAnsiToEbcdic['\n']) {
					c = C_CRLF;
					Advance(&p, 1);
				}
			} else {
				c = CharAndAdvance(&p);
				if (UtfEncoding == 16) ++ByteCount;
				else if (UtfEncoding == 8 && c >= 0x80) {
					++ByteCount;
					if (c >= 0x800) {
						++ByteCount;
						if (c >= 0x10000) ++ByteCount;
					}
				}
			}
			if (CharFlags(c) & 1) switch (c) {
				case C_EOF:    /*end of file*/
				case C_ERROR:  /*error*/
					if (!i && x==LineStart) {
						LineInfo[DispLine].Flags |= LIF_EOF;
						if (hDC && xOffset < 32000) {
							SetTextColor(hDC, EolColor);
							TextOut(hDC, (INT)(XTEXT_OFFSET-xOffset), YPos+2,
									"~", 1);
						}
					} else {
						LineInfo[DispLine].Flags |= LIF_CONTD_NEXTLINE;
						if (c == C_EOF) c = C_UNDEF;
					}
					--ByteCount;
					/*FALLTHROUGH*/
				case C_CRLF:
				case '\r':
				case '\n':
				case '\36':	/*Record Separator*/
				case '\0':	/*real Null byte*/
					if (i) {
						INT Width = GetTextWidth(hDC, Buf, i);

						if (x-xOffset < ClientRect.right)
							Flush(hDC, x, YPos, Buf, i, WithinSelect);
						x += Width;
						if (x > ShrinkRedraw) ShrinkRedraw = x;
					}
					if (WithinSelect && hDC) {
						RECT r;

						r.left   = (INT)(x - xOffset);
						r.top    = YPos;
						r.right  = (INT)(ClientRect.right - XTEXT_OFFSET);
						r.bottom = YPos + TextHeight;
						if (r.left < r.right) FillRect(hDC, &r, SelBkgndBrush);
					}
					if (c == C_CRLF) {
						++ByteCount;
						if (UtfEncoding == 16) ++ByteCount;
					}
					if (c != C_EOF && (c != DefaultNewLine || ListFlag) && hDC
								   && x-xOffset > -32000
								   && x-xOffset < ClientRect.right) {
						HFONT OldFont = SelectObject(hDC, SpecialFont);
						INT	  y = YPos;
						BYTE  c2;

						SetTextColor(hDC, EolColor);
						if (x != LineStart) x += 2;
						switch (c) {
							case C_CRLF: c2 = 0311; break;
							case '\r':   c2 = 0357; break;
							case '\n':   c2 = 0362; break;
							case '\36':  c2 = 0150; break;
							case '\0':   c2 = 'w' ; break;
							case C_EOF:  c2 = '~' ; break;
							default:     c2 = 0363; break;
						}
						TextOut(hDC, (INT)(x-xOffset), y, &c2, 1);
						if (OldFont) SelectObject(hDC, OldFont);
						x += GetTextWidth(hDC, L"m", 1);
					}
					LineInfo[DispLine].Pixels = x;
					PrepareNextLine(&p, ByteCount, FALSE);
					if (MeasureCaret) {
						if (Mode != ReplaceMode) {
							if (Mode != InsertMode) {
								CaretWidth = AveCharWidth;
								CaretType = COMMAND_CARET_EMPTYLINE;
							} else CaretType = INSERT_CARET;
						}
						MeasureCaret = FALSE;
					}
					return;
			}
			switch (c) {
				case C_UNDEF:
					GoBack(&p, 1);
					c = ByteAndAdvance(&p);
					{	CHAR b[4];
						INT	 j;
						INT  Width;

						wsprintf(b, "\\%02X", c);
						for (j=0; j<3; ++j)
							Buf[i++] = b[j];
						Width = GetTextWidth(hDC, Buf, i);
						if (MeasureCaret) {
							CaretWidth = Width;
							CaretType = Mode==InsertMode ? INSERT_CARET
														 : COMMAND_CARET;
							MeasureCaret = FALSE;
							if (!hDC) return;
						}
						if (i >= WSIZE(Buf) - 3
							|| ((CharSet == CS_ANSI || CharSet == CS_OEM)
								&& i >= WSIZE(Buf) - 6
								&& IsDBCSLeadByteEx(CharSet == CS_OEM ? CP_OEMCP
																	  : CP_ACP,
													(BYTE)CharAt(&p)))) {
							Pos += i;
							if (x-xOffset < ClientRect.right)
								Flush(hDC, x, YPos, Buf, i, WithinSelect);
							x += Width;
							i = 0;
						}
					}
					break;

				case '\t':
					if (!ListFlag) {
						if (i) {
							Pos += i;
							if (x-xOffset < ClientRect.right)
								Flush(hDC, x, YPos, Buf, i, WithinSelect);
							x += GetTextWidth(hDC, Buf, i);
							if (x > ShrinkRedraw) ShrinkRedraw = x;
							i = 0;
						}
						Pos += TabStop - Pos % TabStop;
						{	LONG NewX;

							NewX = LineStart + Pos * AveCharWidth;
							if (NewX > x) {
								if (MeasureCaret) {
									CaretWidth = (INT)(NewX - x);
									MeasureCaret = FALSE;
									if (!hDC) return;
								}
								if (WithinSelect && hDC) {
									RECT r;

									r.left   = (INT)(x - xOffset);
									r.top    = YPos;
									r.right  = (INT)(NewX - xOffset);
									r.bottom = YPos + TextHeight;
									if (r.left < r.right)
										FillRect(hDC, &r, SelBkgndBrush);
								}
								x = NewX;
							} else if (MeasureCaret) {
								CaretType = Mode==InsertMode
									   ? INSERT_CARET : COMMAND_CARET_EMPTYLINE;
								CaretWidth = AveCharWidth;
								MeasureCaret = FALSE;
								if (!hDC) return;
							}
						}
						if (BreakLineFlag &&
								x-xOffset > ClientRect.right-RightOffset) {
							PrepareNextLine(&p, ByteCount, TRUE);
							if (x < ExpandRedraw) ExpandRedraw = x;
							return;
						}
						break;
					}
					/*FALLTHROUGH*/
				default:
					{	INT   OldI = i;
						INT   Width;
						BYTE  Buf3[8], *p3;

						if (c == C_BOM && CountBytes(&p) <= 3)
							continue;
						if (CharSet == CS_EBCDIC) {
							c  = c2;
							c2 = MapEbcdicToAnsi[c];
						} else c2 = c;
						if (CharFlags(c2) & 2) {
							if (c2 < ' ' && CharSet != CS_EBCDIC)
								 wsprintf(Buf3, "^%c", c + 'A' - 1);
							else wsprintf(Buf3, "\\%02X", c);
							for (p3 = Buf3; *p3 != '\0'; ++p3)
								Buf[i++] = *p3;
						} else  Buf[i++] = c2;
						Width = GetTextWidth(hDC, Buf, i);
						if (MeasureCaret) {
							CaretWidth = Width;
							CaretType = Mode==InsertMode ? INSERT_CARET
														 : COMMAND_CARET;
							MeasureCaret = FALSE;
							if (!hDC) return;
						}
						if (BreakLineFlag && x-xOffset+Width
											 > ClientRect.right-RightOffset) {
							Flush(hDC, x, YPos, Buf, OldI, WithinSelect);
							if (x+Width < ExpandRedraw) ExpandRedraw = x+Width;
							Width = GetTextWidth(hDC, Buf, OldI);
							if (x+Width > ShrinkRedraw) ShrinkRedraw = x+Width;
							//GoBack(&p, 1);	/*TODO: ?-?-?*/
							p = OldPos;
							PrepareNextLine(&p, ByteCount-1, TRUE);
							return;
						}
						if (i >= WSIZE(Buf) - 3
							|| ((CharSet == CS_ANSI || CharSet == CS_OEM)
								&& i >= WSIZE(Buf) - 6
								&& IsDBCSLeadByteEx(CharSet == CS_OEM ? CP_OEMCP
																	  : CP_ACP,
													(BYTE)CharAt(&p)))) {
							Pos += i;
							if (x-xOffset < ClientRect.right)
								Flush(hDC, x, YPos, Buf, i, WithinSelect);
							x += Width;
							i = 0;
						}
					}
					break;
			}
		}
	}
}

void Paint(HDC hDC, PRECT pRect)
	/*----------------------------------------------------------------------*/
	/* calls:		PaintStatus			PaintTools			TextLine		*/
	/*	  WinApi:	GetTextMetrics		GetSystemMetrics	GetClientRect	*/
	/*				IntersectClipRect	SelectObject		SetBkMode		*/
	/*																		*/
	/* called from:	(winvi.c)MainProc(WM_PAINT)								*/
	/*----------------------------------------------------------------------*/
{	extern BOOL SessionEnding;
	INT			y = nToolbarHeight + YTEXT_OFFSET, xStart = 0;
	HFONT		Font = HexEditMode ? HexFont : TextFont, OldFont;

	if (SessionEnding) return;
	if (Font) OldFont = SelectObject(hDC, Font);
	if (!AveCharWidth) {
		static TEXTMETRIC tm;

		GetTextMetrics(hDC, &tm);
		AveCharWidth = tm.tmAveCharWidth /*+tm.tmOverhang*/;
		/*...adding tmOverhang was not correct here,
		 *   produced bad results with Courier Bold (21-Jun-2002 - Ramo)
		 */
		if (!AveCharWidth) AveCharWidth = GetTextWidth(hDC, L"x", 1);
		IsFixedFont = /*UtfEncoding == 0 &&*/
					  GetTextWidth(hDC, L"W", 1) == GetTextWidth(hDC, L"i", 1);
		/* There are flags for fixed pitch, but they do not work correctly */
	}
	if (!RightOffset)
		 RightOffset = XTEXT_OFFSET + GetSystemMetrics(SM_CXVSCROLL) - 2;
	if (!ClientRect.right) GetClientRect(hwndMain, &ClientRect);
	if (pRect->top < nToolbarHeight) PaintTools(hDC, pRect);
	if (pRect->bottom > ClientRect.bottom-StatusHeight)
		PaintStatus(hDC, pRect, &ClientRect);
	if (WinVersion >= MAKEWORD(95,3)) {
		POINT Line[2];
		HPEN  OldPen;

		if (pRect->left < 2) {
			/*paint shadow lines on left side...*/
			Line[0].x = Line[1].x = 0;
			Line[0].y = nToolbarHeight - 1;
			Line[1].y = ClientRect.bottom - StatusHeight + 2;
			OldPen = SelectObject(hDC, ShadowPen);
			Polyline(hDC, Line, 2);
			Line[0].x = Line[1].x = 1;
			++Line[0].y; --Line[1].y;
			SelectObject(hDC, BlackPen);
			Polyline(hDC, Line, 2);
			SelectObject(hDC, OldPen);
			xStart = 2;
		}
		if (pRect->right > ClientRect.right-2) {
			/*paint shadow lines on right of scrollbar*/
			Line[0].x = Line[1].x = ClientRect.right - 2;
			Line[0].y = nToolbarHeight - 1;
			Line[1].y = ClientRect.bottom - StatusHeight + 1;
			OldPen = SelectObject(hDC, ShadowPen);
			Polyline(hDC, Line, 2);
			Line[0].x = ++Line[1].x;
			SelectObject(hDC, HilitePen);
			Polyline(hDC, Line, 2);
			SelectObject(hDC, OldPen);
		}
	}
	if (pRect->bottom > ClientRect.bottom-StatusHeight-YTEXT_OFFSET ||
			xStart || pRect->right > ClientRect.right-2)
		IntersectClipRect(hDC, xStart, 0,
			ClientRect.right  - (RightOffset  - XTEXT_OFFSET + 2),
			ClientRect.bottom - (StatusHeight + YTEXT_OFFSET));
	if (pRect->top < ClientRect.bottom - StatusHeight + 5) {
		SetBkMode(hDC, TRANSPARENT);
		NewCaret = FALSE;
		if (LineInfo) {
			INT line = 0, Lower;

			Lower = ClientRect.bottom - StatusHeight - YTEXT_OFFSET;
			if (Lower > pRect->bottom && !PaintAll && LineInfo &&
								 (LineInfo[VisiLines].Flags & LIF_EOF ||
								  LineInfo[VisiLines].Pos.p != NULL))
				Lower = pRect->bottom;
			PaintAll = FALSE;
			while (y < Lower) {
				if (EnforcePainting || PaintAll || pRect->top <= y+TextHeight)
					TextLine(hDC, line, y);
				++line;
				y += TextHeight;
			}
		} /*else fatal error*/
	}
	if (Font && OldFont) SelectObject(hDC, OldFont);
}

INT GetPixelLine(INT Line, BOOL WithinTextArea)
{	INT i = Line * TextHeight;

	if (!WithinTextArea) i += nToolbarHeight + YTEXT_OFFSET;
	return (i);
}

void GetXPos(PCOLUMN pPos)
{	INT Line = (CaretY - nToolbarHeight - YTEXT_OFFSET) / TextHeight;

	pPos->ScreenLine = 0;
	while (Line > 0 && LineInfo[--Line].Flags & LIF_CONTD_NEXTLINE)
		++pPos->ScreenLine;
	/*TODO: first screen line may be a continuation line*/
	pPos->PixelOffset = CaretX - StartX(FALSE);
}

WCHAR CommandBuf[1024];

void CommandEdit(UINT StartChar)
{
	if (Disabled || hwndCmd) return;
	for (;;) {
		static WCHAR Buf[2];
		DWORD		 LastError;

		Buf[0] = StartChar;
		NewStatus(0, L"", NS_NORMAL);
		hwndCmd = CreateWindowW(L"Edit", Buf,
								WS_CHILD | ES_AUTOHSCROLL | ES_LEFT,
								StatusFields[0].x+1,
								ClientRect.bottom-StatusHeight+5,
								StatusFields[0].Width-1, StatusTextHeight,
								hwndMain, (HMENU)ID_COMMAND, hInst, 0L);
		if (hwndCmd) break;
		#if defined(WIN32)
		{	extern char  QueryString[150];
			extern DWORD QueryTime;
			char		 LastErrString[32];

			if ((LastError = GetLastError()) == ERROR_NOT_ENOUGH_MEMORY)
				 lstrcpy (LastErrString, "ERROR_NOT_ENOUGH_MEMORY");
			else wsprintf(LastErrString, "%ld", LastError);
			wsprintf(QueryString, "CreateWindow failed, LastError=%s",
								  LastErrString);
			QueryTime = GetTickCount();
		}
		#else
			#if !defined(ERROR_NOT_ENOUGH_MEMORY)
				#define  ERROR_NOT_ENOUGH_MEMORY 8
			#endif
			LastError =  ERROR_NOT_ENOUGH_MEMORY;
		#endif
		if (LastError==ERROR_NOT_ENOUGH_MEMORY && FreeSpareMemory()) continue;
		ErrorBox(MB_ICONSTOP, 301);
		return;
	}
	SendMessage(hwndCmd, EM_LIMITTEXT, WSIZE(CommandBuf) - 1, 0L);
	SendMessage(hwndCmd, WM_SETFONT, (WPARAM)StatusFont, 0L);
	#if defined(WIN32)
		SendMessage(hwndCmd, EM_SETSEL, 1, 1);
	#else
		SendMessage(hwndCmd, EM_SETSEL, (WPARAM)FALSE, MAKELPARAM(1,1));
	#endif
	ShowWindow(hwndCmd, SW_SHOW);
	SetFocus(hwndCmd);
	HelpContext = HelpExCommand;
}

void CommandDone(BOOL Escape)
{
	if (hwndCmd) {
		HWND ToDestroy = hwndCmd;

		GetWindowTextW(hwndCmd, CommandBuf, WSIZE(CommandBuf));
		hwndCmd = 0;  /*must be cleared when window disappears*/
		DestroyWindow(ToDestroy);
		if (!Escape) {
			if (*CommandBuf) CommandExec(CommandBuf);
		} else if (SelectCount) {
			InvalidateArea(&SelectStart, SelectCount, 1);
			SelectCount = 0;
		}
		CommandLineDone(Escape, CommandBuf);
		if (hwndMain) {
			ShowEditCaret();
			SetFocus(hwndMain);
		}
		SetNormalHelpContext();
	}
}
