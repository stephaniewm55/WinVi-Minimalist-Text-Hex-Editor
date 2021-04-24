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
 *		3-Dec-2000	first publication of source code
 *     23-Sep-2002	enable apply button
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *     22-Jun-2010	suppress BOM in print-out
 */

#include <windows.h>
#include <commdlg.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#if !defined(WIN32)
# include <print.h>
#endif
#include "resource.h"
#include "winvi.h"
#include "printh.h"
#include "map.h"
#include "page.h"

#if defined(__LCC__)
typedef struct {
	INT		cbSize;
	LPCWSTR	lpszDocName;
	LPCWSTR	lpszOutput;
	LPCWSTR	lpszDatatype;
	DWORD	fwType;
} DOCINFOW;
#endif

INT			nPrtLeft=40, nPrtTop=50, nPrtRight=40, nPrtBottom=50;
/*...measures half millimeters*/
BOOL		iMeasure, LineNumbering, Frame, Frame2, Frame3;
BOOL		OverrideOrientation, DoubleColumn;
INT			nOrientation;
PWSTR		Header, Footer;

static UINT	PageNo;
static WCHAR DecimalPoint[2], Buf[300];

void FillField(HWND hDlg, INT nCtl, INT Value)
{
	if (iMeasure)
		 _snwprintf(Buf, WSIZE(Buf), L"%d%s%d",
		 			Value/50, DecimalPoint, Value/5 % 10);
	else _snwprintf(Buf, WSIZE(Buf), L"%d%s%d",
					Value/20, DecimalPoint, Value/2 % 10);
	SetDlgItemTextW(hDlg, nCtl, Buf);
}

INT GetBorderWidth(HWND hDlg, INT nCtl)
{	PWSTR p = Buf;
	INT	  Decimals = 0, Value = 0;

	GetDlgItemTextW(hDlg, nCtl, Buf, WSIZE(Buf));
	while (*p && Value <= 3200 && Decimals <= 3200) {
		if (*p >= '0' && *p <= '9') {
			Value = 10 * Value + *p - '0';
			if (Decimals) Decimals *= 10;
		}
		if (*p++ == *DecimalPoint) Decimals = 1;
	}
	if (!Decimals) Decimals = 1;
	return (MulDiv(Value, iMeasure ? 50 : 20, Decimals));
}

extern UINT CALLBACK ChooseFontHook(HWND, UINT, WPARAM, LPARAM);

PRINTDLG   pd;
LOGFONT	   lf = {0};
CHOOSEFONT cf = {
	sizeof(CHOOSEFONT), NULL, NULL, &lf, 100,
	CF_PRINTERFONTS | CF_INITTOLOGFONTSTRUCT | CF_ENABLEHOOK
};

BOOL PrintCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{
	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			iMeasure = GetProfileInt("Intl", "iMeasure", 0);
			GetProfileStringW(L"Intl", L"sDecimal", L".",
							  DecimalPoint, WSIZE(DecimalPoint));
			if (iMeasure && LOADSTRINGW(hInst, 246, Buf, sizeof(Buf)) > 0) {
				SetDlgItemTextW(hDlg, IDC_UNIT1, Buf);
				SetDlgItemTextW(hDlg, IDC_UNIT2, Buf);
				SetDlgItemTextW(hDlg, IDC_UNIT3, Buf);
				SetDlgItemTextW(hDlg, IDC_UNIT4, Buf);
			}
			FillField(hDlg, IDC_LEFTBORDER,   nPrtLeft);
			FillField(hDlg, IDC_TOPBORDER,    nPrtTop);
			FillField(hDlg, IDC_RIGHTBORDER,  nPrtRight);
			FillField(hDlg, IDC_BOTTOMBORDER, nPrtBottom);
			{	INT Orientation;

				if (DoubleColumn) {
					Orientation = IDC_DOUBLECOLUMN;
					nOrientation = 1;
				} else {
					EnableWindow(GetDlgItem(hDlg, IDC_DIVIDINGLINE), FALSE);
					Orientation = OverrideOrientation
						? IDC_PORTRAIT+nOrientation : IDC_DEFAULTORIENTATION;
				}
				CheckRadioButton(hDlg, IDC_DEFAULTORIENTATION, IDC_DOUBLECOLUMN,
								 Orientation);
			}
			SetDlgItemTextW(hDlg, IDC_HEADER, Header);
			SetDlgItemTextW(hDlg, IDC_FOOTER, Footer);
			SendDlgItemMessage(hDlg, IDC_HEADER, EM_LIMITTEXT, 80, 0);
			SendDlgItemMessage(hDlg, IDC_FOOTER, EM_LIMITTEXT, 80, 0);
			{	CHAR Buf[260];

				if (!*lf.lfFaceName) *Buf = 0;
				else snprintf(Buf, sizeof(Buf), "%s,  %d pt.",
							  lf.lfFaceName, cf.iPointSize/10);
				SetDlgItemText(hDlg, IDC_FONTNAME, Buf);
			}
			CheckDlgButton(hDlg, IDC_LINENUMBER,   LineNumbering);
			CheckDlgButton(hDlg, IDC_PRINTFRAME,   Frame);
			CheckDlgButton(hDlg, IDC_PRINTFRAME2,  Frame2);
			CheckDlgButton(hDlg, IDC_DIVIDINGLINE, Frame3);
			break;

		case WM_COMMAND:
			switch (COMMAND) {
				case IDC_DEFAULTORIENTATION:
				case IDC_PORTRAIT:
				case IDC_LANDSCAPE:
					CheckDlgButton		   (hDlg, IDC_DIVIDINGLINE,  FALSE);
					EnableWindow(GetDlgItem(hDlg, IDC_DIVIDINGLINE), FALSE);
					break;

				case IDC_DOUBLECOLUMN:
					EnableWindow(GetDlgItem(hDlg, IDC_DIVIDINGLINE), TRUE);
					break;

				case IDC_PRINTFONT:
					{	PSTR p1, p2, p3;
						CHAR Buf[260];
						HDC	 hDC;

						if (pd.hDevNames) {
							LPDEVNAMES p;

							p  = (LPDEVNAMES)GlobalLock(pd.hDevNames);
							p1 = (LPSTR)p + p->wDriverOffset;
							p2 = (LPSTR)p + p->wDeviceOffset;
							p3 = (LPSTR)p + p->wOutputOffset;
						} else {
							GetProfileString("windows", "device", "",
											 Buf, sizeof(Buf));
							p2 = Buf;
							for (p1=Buf; *p1 && *p1!=','; ++p1);
							if (*p1) *p1++ = '\0';
							for (p3=p1;  *p3 && *p3!=','; ++p3);
							if (*p3) *p3++ = '\0';
						}
						if (lf.lfHeight == 0 && (hDC = GetDC(NULL)) != NULL) {
							INT Lpy = GetDeviceCaps(hDC, LOGPIXELSY);

							/* lfHeight needed for setting up
							 * ChooseFont Win32 dialog only.
							 * Windows does not use the printer DC
							 * for calculating font size. Don't know why.
							 */
							lf.lfHeight = -MulDiv(cf.iPointSize, Lpy, 720);
							ReleaseDC(NULL, hDC);
							#if 0
							{	CHAR Buf[160];

								_snprintf(Buf, sizeof(Buf),
										  "WinVi: driver=\"%s\" "
										  		 "device=\"%s\" "
												 "output=\"%s\" "
												 "size=-%d*%d/%d=%d\n",
										  p1, p2, p3,
										  cf.iPointSize, Lpy, 720, lf.lfHeight);
								OutputDebugString(Buf);
							}
							#endif
						}
						cf.hDC = CreateIC(p1, p2, p3, NULL);
					}
					cf.hwndOwner = hDlg;
					cf.hInstance = hInst;
					cf.nFontType = PRINTER_FONTTYPE;
					cf.lCustData = ChooseFontCustData(256);
					if (cf.lpfnHook == NULL)
						cf.lpfnHook = (LPCFHOOKPROC)
							MakeProcInstance((FARPROC)ChooseFontHook, hInst);
					{	HWND OldFocus = GetFocus();

						if (ChooseFont(&cf)) {
							#if 0
							{	CHAR Buf[160];

								_snprintf(Buf, sizeof(Buf),
										  "WinVi: size=%d/%d\n",
										  cf.iPointSize, lf.lfHeight);
								OutputDebugString(Buf);
							}
							#endif
							EnableApply();
						}
						if (OldFocus) SetFocus(OldFocus);	/*for 16-bit*/
					}
					DeleteDC(cf.hDC);
					if (pd.hDevNames) GlobalUnlock(pd.hDevNames);
					{	CHAR Buf[260];

						if (!*lf.lfFaceName) *Buf = 0;
						else snprintf(Buf, sizeof(Buf), "%s,  %d pt.",
									  lf.lfFaceName, cf.iPointSize/10);
						SetDlgItemText(hDlg, IDC_FONTNAME, Buf);
					}
					break;

				case IDC_STORE:
				case ID_APPLY:
				case IDOK:
					LineNumbering = IsDlgButtonChecked(hDlg, IDC_LINENUMBER);
					nPrtLeft   = GetBorderWidth(hDlg, IDC_LEFTBORDER);
					nPrtTop    = GetBorderWidth(hDlg, IDC_TOPBORDER);
					nPrtRight  = GetBorderWidth(hDlg, IDC_RIGHTBORDER);
					nPrtBottom = GetBorderWidth(hDlg, IDC_BOTTOMBORDER);
					Frame  = IsDlgButtonChecked(hDlg, IDC_PRINTFRAME);
					Frame2 = IsDlgButtonChecked(hDlg, IDC_PRINTFRAME2);
					Frame3 = IsDlgButtonChecked(hDlg, IDC_DIVIDINGLINE);
					OverrideOrientation = nOrientation =
						!IsDlgButtonChecked(hDlg, IDC_DEFAULTORIENTATION);
					if (OverrideOrientation)
						nOrientation = !IsDlgButtonChecked(hDlg, IDC_PORTRAIT);
					DoubleColumn = IsDlgButtonChecked(hDlg, IDC_DOUBLECOLUMN);
					GetDlgItemTextW(hDlg, IDC_HEADER, Buf, WSIZE(Buf));
					if (!Header || wcscmp(Buf, Header)) {
						if (Header) _ffree(Header);
						if (!*Buf) Header = NULL;
						else {
							Header = _fcalloc(wcslen(Buf)+1, sizeof(WCHAR));
							if (Header != NULL) wcscpy(Header, Buf);
						}
					}
					GetDlgItemTextW(hDlg, IDC_FOOTER, Buf, WSIZE(Buf));
					if (!Footer || wcscmp(Buf, Footer)) {
						if (Footer) _ffree(Footer);
						if (!*Buf) Footer = NULL;
						else {
							Footer = _fcalloc(wcslen(Buf)+1, sizeof(WCHAR));
							if (Footer != NULL) wcscpy(Footer, Buf);
						}
					}
					DlgResult = TRUE;
					if (COMMAND == IDC_STORE) {
						SavePrintSettings();
						break;
					}
					if (COMMAND != IDOK) return (TRUE);
					/*FALLTHROUGH*/
				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;
			}
			break;

		default:
			return (FALSE);
	}
	return (TRUE);
}

VOID PrintSettings(HWND hwndParent)
{	static DLGPROC Callback;

	if (!Callback)
		 Callback = (DLGPROC)MakeProcInstance((FARPROC)PrintCallback, hInst);
	DialogBox(hInst, MAKEINTRES(IDD_PRINTSETTINGS), hwndParent, Callback);
}

INT PrintWidth(HDC hDC, PSTR Buffer, INT Len)
{	SIZE Size;

	if (GetTextExtentPoint(hDC, Buffer, Len, &Size))
		return (Size.cx);
	return (0);
}

INT PrintWidthW(HDC hDC, PWSTR Buffer, INT Len)
{	SIZE Size;

	if (GetTextExtentPointW(hDC, Buffer, Len, &Size))
		return (Size.cx);
	return (0);
}

WCHAR BufW[300];

VOID PrintHeaderFooter(HDC hDC, PWSTR Text, INT LeftX, INT RightX, INT Y)
{	PWSTR sp = Text;
	PWSTR dp = BufW;

	for (;;) {
		while ((*dp = *sp++) != '\0' && *dp != '&') ++dp;
		if (!*dp) break;
		switch (*sp++) {
			case 'n':	/*file name*/
				if (dp-BufW+wcslen(CurrFile)+wcslen(sp) > WSIZE(BufW)-1) {
					PWSTR p1, p2;

					p1 = wcsrchr(CurrFile, '/');
					p2 = wcsrchr(CurrFile, '\\');
					if (!p1 || (p2 && p1 < p2)) p1 = p2;
					if (!p1 || dp-BufW+wcslen(p1)+wcslen(sp) > WSIZE(BufW)-1)
						continue;
					wcscpy(dp, p1);
				} else wcscpy(dp, CurrFile);
				dp += wcslen(dp);
				continue;

			case 'p':	/*page number*/
				if (dp-BufW+wcslen(sp) > WSIZE(BufW)-6) continue;
				dp += _snwprintf(dp, 6, L"%u", PageNo);
				continue;

			default:	/*preserve '&'*/
				--sp;
				/*FALLTHROUGH*/
			case '&':	/*escaped '&'*/
				++dp;
				continue;
		}
	}
	TextOutW(hDC, LeftX + ((RightX-LeftX-PrintWidthW(hDC,BufW,dp-BufW))>>1), Y,
			 BufW, dp-BufW);
}

DOCINFOW   di;
TEXTMETRIC tm;
HWND	   hwndPrintCancel;
BOOL	   PrintCancelled;

VOID PrintIt(HDC hDC, INT nCopies, BOOL fStartDoc, BOOL fEndDoc)
{	PCWSTR		 Src = CurrFile;
	static WCHAR DocTitle[300];
	PWSTR		 pw, DstStart;
	POSITION	 Pos, StartPos;
	INT			 c, LineStart, LineWidth, HalfWidth, PageStart, PageHeight;
	INT			 LineNoWidth = 0, LinesPerPage = 0, nCopy = 0;
	long		 LineNo = 0, StartLineNo;
	BOOL		 NewLine = TRUE, StartNewLine, CountLines = TRUE;
	HFONT		 hFont = NULL, hOldFont = NULL;
	INT			 Lpx, Lpy, FrameDist;

	if (fStartDoc) {
		di.cbSize	   = sizeof(DOCINFO);
		di.lpszDocName = DstStart = pw = DocTitle;
		while (*Src) {
			if (*Src=='/' || *Src=='\\') pw = DstStart;
			else if (pw < DocTitle + WSIZE(DocTitle)-1) *pw++ = *Src;
			++Src;
		}
		if (pw - DocTitle >= (INT)(sizeof(DocTitle) - wcslen(ApplNameW) - 1))
			*pw = '\0';
		else {
			if (pw > DocTitle) {
				wcscpy(pw, L" - ");
				pw += 3;
			}
			wcscpy(pw, ApplNameW);
		}
		if (StartDocW(hDC, &di) == SP_ERROR) {
			AbortDoc(hDC);
			if (!PrintCancelled) {
				PrintCancelled = TRUE;
				Error(320, L"StartDoc");
			}
			return;
		}
	}
	Pos.p = FirstPage;
	Pos.i = 0;
	Pos.f = 0;
	PageNo = 0;
	if ((c = CharAt(&Pos)) == C_BOM) CharAndAdvance(&Pos);
	Lpx   = GetDeviceCaps(hDC, LOGPIXELSX);
	Lpy   = GetDeviceCaps(hDC, LOGPIXELSY);
	LineWidth	= GetDeviceCaps(hDC, HORZRES);
	PageHeight	= GetDeviceCaps(hDC, VERTRES);
	PageStart	= MulDiv(nPrtTop,	 Lpy, 50);
	PageHeight -= MulDiv(nPrtBottom, Lpy, 50) + 2;
	LineStart	= MulDiv(nPrtLeft,	 Lpx, 50);
	LineWidth  -= MulDiv(nPrtRight,	 Lpx, 50) + 2;
	HalfWidth   = (LineWidth - LineStart) >> 1;
	FrameDist	= Lpx >> 2;
	if (!Frame) HalfWidth += FrameDist;
	if (cf.iPointSize) {
		c = lf.lfHeight;
		lf.lfHeight = -MulDiv(cf.iPointSize, Lpy, 720);
		hFont = CreateFontIndirect(&lf);
		lf.lfHeight = c;
	}
	do {
		INT  x, y, Start, Col;
		INT  HeaderY   = PageStart, FooterY	  = PageHeight;
		INT  TextStart = PageStart, TextHeight = PageHeight;
		BOOL FirstColumn = TRUE;
		BOOL RealPrint;

		++PageNo;
		RealPrint = (pd.nFromPage == 0 || PageNo	 >= (UINT)pd.nFromPage) &&
					(pd.nToPage   == 0 || pd.nToPage == 0xffff ||
										  PageNo	 <= (UINT)pd.nToPage);
		if (RealPrint && StartPage(hDC) <= 0) {
			ErrorBox(MB_ICONSTOP, 320, "StartPage");
			break;
		}
		if (hwndPrintCancel) {
			WCHAR Buf[12];

			_snwprintf(Buf, WSIZE(Buf), L"%d", PageNo);
			SetDlgItemTextW(hwndPrintCancel, IDC_PAGESPRINTED, Buf);
		}
		if (hFont) hOldFont = SelectObject(hDC, hFont);
		if (!LineNoWidth) {
			GetTextMetrics(hDC, &tm);
			LineNoWidth = PrintWidth(hDC, "000:  ", 6);
		}

		if (Frame || Frame2) {
			if (RealPrint && Frame) {
				MoveToEx(hDC, LineStart, PageStart, NULL);
				LineTo  (hDC, LineWidth, PageStart);
				LineTo  (hDC, LineWidth, PageHeight);
				LineTo  (hDC, LineStart, PageHeight);
				LineTo  (hDC, LineStart, PageStart);
			}
			if (Frame2) {
				if (Header) {
					TextStart += tm.tmHeight + Lpy / 3;
					if (RealPrint) {
						MoveToEx(hDC, LineStart, PageStart, NULL);
						LineTo  (hDC, LineWidth, PageStart);
						LineTo  (hDC, LineWidth, TextStart);
						LineTo  (hDC, LineStart, TextStart);
						LineTo  (hDC, LineStart, PageStart);
					}
				}
				if (Footer) {
					TextHeight -= tm.tmHeight + Lpy / 3;
					if (RealPrint) {
						MoveToEx(hDC, LineStart, TextHeight, NULL);
						LineTo	(hDC, LineWidth, TextHeight);
						LineTo  (hDC, LineWidth, PageHeight);
						LineTo  (hDC, LineStart, PageHeight);
						LineTo  (hDC, LineStart, TextHeight);
					}
				}
				HeaderY += Lpy / 6;
				FooterY -= Lpy / 6;
			} else {
				if (Header) TextStart  += 3 * tm.tmHeight;
				if (Footer) TextHeight -= 3 * tm.tmHeight;
				HeaderY += Lpy / 5;
				FooterY -= Lpy / 5;
			}
			if (Frame) {
				LineStart += FrameDist;
				LineWidth -= FrameDist;
			}
			TextStart  += Lpy / 5;
			TextHeight -= Lpy / 5;
		} else {
			if (Header) TextStart  += 3 * tm.tmHeight;
			if (Footer) TextHeight -= 3 * tm.tmHeight;
		}
		FooterY -= tm.tmHeight;
		if (RealPrint) {
			if (Header)
				PrintHeaderFooter(hDC, Header, LineStart, LineWidth, HeaderY);
			if (Footer)
				PrintHeaderFooter(hDC, Footer, LineStart, LineWidth, FooterY);
		}

		/*save position for multiple uncollated copies...*/
		StartPos	 = Pos;
		StartNewLine = NewLine;
		StartLineNo	 = LineNo;

		if (DoubleColumn) {
			LineWidth -= HalfWidth;
			if (Frame3 && RealPrint) {
				x = LineStart + HalfWidth - FrameDist;
				MoveToEx(hDC, x, TextStart, NULL);
				LineTo  (hDC, x, TextHeight);
			}
		}

		for (;;) {
			Start = LineStart;
			if (LineNumbering) Start += LineNoWidth;
			x = Start;
			y = TextStart;
			do {
				INT  NewX;
				BOOL NewPage = FALSE;

				if (NewLine) {
					NewLine = FALSE;
					x = Start;
					++LineNo;
					if (LineNumbering && RealPrint) {
						INT Len = _snwprintf(Buf,WSIZE(Buf), L"%ld:  ", LineNo);

						TextOutW(hDC, x-PrintWidthW(hDC,Buf,Len), y, Buf, Len);
					}
					Col = 0;
					MessageLoop(FALSE);
				}
				pw = Buf;
				do {
					c = CharAndAdvance(&Pos);
					if (c == CTRL('l')) {
						/*formfeed...*/
						if (CharFlags(CharAt(&Pos)) & 1)
							 c = CharAndAdvance(&Pos);/*skip following newline*/
						else c = C_CRLF;			  /*fake newline after ^L*/
						NewPage = TRUE;
					}
					if (CharFlags(c) & 1) {
						NewLine = TRUE;
						NewX = Start;
						if (CharAt(&Pos) == C_EOF) c = C_EOF;
						break;
					}
					if (c == '\t') {
						INT NextNewX;

						Col += TabStop;
						Col -= Col % TabStop;
						NextNewX = Start + Col * tm.tmAveCharWidth;
						if (NextNewX >= LineWidth) {
							NewX = Start;
							Col = 0;
						} else if (NextNewX > x) NewX = NextNewX;
						else NewX = x;
						break;
					}
					if (CharSet == CS_OEM) {
						WCHAR c2 = c;

						UnicodeConvert(&c2, &c2, 1);
						c = c2;
					}
					*pw++ = c;
					NewX = x + PrintWidthW(hDC, Buf, pw-Buf);
					if (NewX > LineWidth) {
						GoBack(&Pos, UtfEncoding == 16 ? 2 : 1);
						--pw;
						NewX = Start;
						Col = 0;
						break;
					}
					++Col;
				} while (pw < Buf+WSIZE(Buf));
				if (pw > Buf && RealPrint) TextOutW(hDC, x, y, Buf, pw-Buf);
				if (PrintCancelled) {
					AbortDoc(hDC);
					return;
				}
				x = NewX;
				if (NewX == Start) {
					if (CountLines) ++LinesPerPage;
					y += tm.tmHeight + tm.tmInternalLeading;
					if (y + tm.tmHeight > TextHeight || NewPage) break;
				}
			} while (c != C_EOF);
			CountLines = FALSE;
			if (!DoubleColumn || (FirstColumn ^= TRUE | FALSE) || c == C_EOF) {
				if (DoubleColumn) {
					if (FirstColumn) LineStart -= HalfWidth;
					else LineWidth += HalfWidth;
				}
				break;
			}
			LineStart += HalfWidth;
			LineWidth += HalfWidth;
		}
		if (++nCopy >= nCopies) nCopy = 0;
		else {
			Pos		= StartPos;
			NewLine	= StartNewLine;
			LineNo	= StartLineNo;
			c		= CharAt(&Pos);
		}
		{	LONG n;

			if ((n = LineNo + LinesPerPage) >= 1000) {
				if (n < 10000) LineNoWidth = PrintWidth(hDC, "0000:  ",  7);
				else		   LineNoWidth = PrintWidth(hDC, "00000:  ", 8);
			}
		}
		if (Frame) {
			LineStart -= FrameDist;
			LineWidth += FrameDist;
		}
		if (hOldFont) SelectObject(hDC, hOldFont);
		{	INT  ErrorString, ErrorValue;

			if (!RealPrint || (ErrorValue = EndPage(hDC)) >= 0) continue;
			switch (ErrorValue) {
				case SP_ERROR:		 ErrorString = 321; break;
				case SP_APPABORT:	 ErrorString = 322; break;
				case SP_USERABORT:	 ErrorString = 323; break;
				case SP_OUTOFDISK:	 ErrorString = 324; break;
				case SP_OUTOFMEMORY: ErrorString = 325; break;
				default:			 ErrorString = 326; break;
			}
			ErrorBox(MB_ICONSTOP, ErrorString, ErrorValue);
		}
		break;
	} while (c != C_EOF);
	if (hFont) DeleteObject(hFont);
	if (fEndDoc) EndDoc(hDC);
}

BOOL WINAPI PrintCancelCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{
	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_COMMAND:
			if (wPar == IDCANCEL) {
				PrintCancelled = TRUE;
				PostMessage(hDlg, WM_CLOSE, 0, 0);
			}
			break;

		case WM_CLOSE:
			DestroyWindow(hDlg);
			break;

		default:
			return (FALSE);
	}
	return (TRUE);
}

BOOL PrintDoc(HWND hwndParent, BOOL WithDlg)
{
	pd.lStructSize = sizeof(PRINTDLG);
	pd.hwndOwner   = hwndParent;
	if (!pd.Flags) {
		pd.Flags	 = PD_ALLPAGES | PD_COLLATE | PD_NOSELECTION;
		pd.nCopies	 = 1;
		pd.nFromPage = pd.nMinPage = 1;
		pd.nToPage	 = pd.nMaxPage = 0xffff;
	}
	if (!WithDlg || (StopMap(TRUE), PrintDlg(&pd))) {
		LPDEVMODE  lpdm;
		LPDEVNAMES lpdn;
		HDC		   hDC;
		DLGPROC	   CancelDlgProc;

		UpdateWindow(hwndMain);
		CancelDlgProc = (DLGPROC)MakeProcInstance((FARPROC)PrintCancelCallback,
												  hInst);
		if (!(pd.Flags & PD_PAGENUMS)) {
			pd.nFromPage = 1;
			pd.nToPage	 = 0xffff;
		}
		PrintCancelled = FALSE;
		if (CancelDlgProc)
			hwndPrintCancel = CreateDialog(hInst,
										   MAKEINTRES(IDD_PRINTCANCEL),
										   hwndMain, CancelDlgProc);
		if (pd.hDevMode
				&& (lpdm = (LPDEVMODE) GlobalLock(pd.hDevMode)) != NULL
				&& pd.hDevNames
				&& (lpdn = (LPDEVNAMES)GlobalLock(pd.hDevNames)) != NULL) {
			LPSTR p1, p2, p3;

			if (OverrideOrientation) {
				lpdm->dmFields	   |= DM_ORIENTATION;
				lpdm->dmOrientation = nOrientation ? DMORIENT_LANDSCAPE :
													 DMORIENT_PORTRAIT;
			}
			nOrientation = lpdm->dmOrientation == DMORIENT_LANDSCAPE;
			p1 = (LPSTR)lpdn + lpdn->wDriverOffset;
			p2 = (LPSTR)lpdn + lpdn->wDeviceOffset;
			p3 = (LPSTR)lpdn + lpdn->wOutputOffset;
			hDC = CreateDC(p1, p2, p3, lpdm);
			GlobalUnlock(pd.hDevMode);
			GlobalUnlock(pd.hDevNames);
			if (pd.Flags & PD_COLLATE) {
				WORD i;

				for (i=pd.nCopies; i && !PrintCancelled; --i)
					PrintIt(hDC, 1,			 i==pd.nCopies,	i==1);
			} else	PrintIt(hDC, pd.nCopies, TRUE,			TRUE);
			DeleteDC(hDC);
			if (hwndPrintCancel) DestroyWindow(hwndPrintCancel);
			return (TRUE);
		}
	}
	return (FALSE);
}

VOID PrintFromMenu(HWND hwndParent)
{
	PrintDoc(hwndParent, TRUE);
}

HGLOBAL hDevNames;

VOID PrintImmediate(HWND hwndParent)
{	LPDEVMODE  lpdm;
	LPDEVNAMES lpdn;
	PSTR	   p1, p2, p3;
	BOOL	   Ok = FALSE;

	pd.hDevMode = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DEVMODE));
	if (pd.hDevMode) {
		if ((lpdm = GlobalLock(pd.hDevMode)) != NULL) {
			lpdm->dmSpecVersion	= 0x30a;
			lpdm->dmSize		= sizeof(DEVMODE);
			lpdm->dmFields		= DM_PRINTQUALITY;
			lpdm->dmPrintQuality= DMRES_MEDIUM;
			if (hDevNames) {
				pd.hDevNames = hDevNames;
				Ok = PrintDoc(hwndParent, FALSE);
			} else {
				CHAR Buf[260];

				if (!GetProfileString("windows", "device", "",
									  Buf, sizeof(Buf))) {
					ErrorBox(MB_ICONSTOP, 327);
					return;
				}
				pd.hDevNames = GlobalAlloc(GMEM_MOVEABLE,
										   strlen(Buf) + 1 + sizeof(DEVNAMES));
				if (pd.hDevNames) {
					if ((lpdn = GlobalLock(pd.hDevNames)) != NULL) {
						strcpy(p2 = (LPSTR)lpdn + sizeof(DEVNAMES), Buf);
						for (p1=p2; *p1 && *p1!=','; ++p1);
						if (*p1) *p1++ = '\0';
						for (p3=p1; *p3 && *p3!=','; ++p3);
						if (*p3) *p3++ = '\0';
						lpdn->wDriverOffset = (LPSTR)p1 - (LPSTR)lpdn;
						lpdn->wDeviceOffset = (LPSTR)p2 - (LPSTR)lpdn;
						lpdn->wOutputOffset = (LPSTR)p3 - (LPSTR)lpdn;
						lpdn->wDefault = DN_DEFAULTPRN;
						strncpy(lpdm->dmDeviceName, p2, CCHDEVICENAME);
						Ok = PrintDoc(hwndParent, FALSE);
						GlobalUnlock(pd.hDevNames);
					}
				}
				GlobalFree(pd.hDevNames);
				pd.hDevNames = NULL;
			}
			GlobalUnlock(pd.hDevMode);
		}
		GlobalFree(pd.hDevMode);
		pd.hDevMode = NULL;
	}
	if (!Ok) ErrorBox(MB_ICONSTOP, 325);
}

VOID PrintFromToolbar(HWND hwndParent) {
	PrintDoc(hwndParent, TRUE);
}

BOOL PrintSetPrinter(PCWSTR Arg, INT Len)
{	INT  i;
	BOOL Quoted = FALSE;

	hDevNames = GlobalAlloc(GMEM_MOVEABLE, Len + 1+sizeof(DEVNAMES));
	if (hDevNames) {
		LPDEVNAMES lpdn;
		if ((lpdn = GlobalLock(hDevNames)) != NULL) {
			LPSTR p = (LPSTR)lpdn + sizeof(DEVNAMES);

			lpdn->wDeviceOffset = sizeof(DEVNAMES);
			lpdn->wOutputOffset = lpdn->wDriverOffset = 0;
			for (i=0; i<Len; ++i) {
				switch (Arg[i]) {
					case '"':
						Quoted ^= TRUE ^ FALSE;
						continue;

					case ',':
						if (!Quoted) {
							*p++ = '\0';
							if  (lpdn->wDriverOffset) {
								if (lpdn->wOutputOffset) return (FALSE);
								lpdn->wOutputOffset = p - (LPSTR)lpdn;
							} else {
								lpdn->wDriverOffset = p - (LPSTR)lpdn;
							}
							continue;
						}
						/*FALLTHROUGH*/
					default:
						if ((*p++ = (CHAR)Arg[i]) != '\0') continue;
						break;
				}
				break;
			}
			*p = '\0';
			if (!lpdn->wOutputOffset) return (FALSE);
			#if 0
				MessageBox(NULL,(LPSTR)lpdn+lpdn->wDeviceOffset,"Device",MB_OK);
				MessageBox(NULL,(LPSTR)lpdn+lpdn->wDriverOffset,"Driver",MB_OK);
				MessageBox(NULL,(LPSTR)lpdn+lpdn->wOutputOffset,"Output",MB_OK);
			#endif
			GlobalUnlock(hDevNames);
		} else hDevNames = NULL;
	}
	return (hDevNames != NULL);
}
