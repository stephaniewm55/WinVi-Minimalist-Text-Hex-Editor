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
 *     22-Jul-2002	split from search.c
 *     27-Jul-2002	CheckFlag for interactive substitute confirmation
 *     15-Aug-2002  suppress of MessageBeep when Messagebox is called
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 */

#include <windows.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include "winvi.h"
#include "page.h"
#include "resource.h"

#if !defined(DM_REPOSITION)
# define DM_REPOSITION (DM_SETDEFID+1)
#endif

extern INT		SearchErrNo;
extern BOOL		MatchValid;
extern POSITION	StartPos, EndMatch;
extern WCHAR	SrchDispBuf[100];

BOOL SearchAgain(WORD Flags)
	/*Flags: 1=Reverse (search in opposite direction)
	 *		 2=pop up dialog if not searched before
	 */
{	BOOL  Return;
	ULONG OldLength;

	HideEditCaret();
	if (Flags & 2 && !MatchValid) {
		SearchDialog();
		return (TRUE);
	}
	OldLength = SelectCount;
	if (OldLength) {
		InvalidateArea(&SelectStart, SelectCount, 1);
		SelectCount = 0;
		UpdateWindow(hwndMain);
	}
	SelectStart = CurrPos;
	Return = SearchAndStatus(&SelectStart, (WORD)((Flags & 1) | 2));
	if (Return) {
		if ((OldLength==0 && Mode!=InsertMode) ||
				ComparePos(&SelectStart, &CurrPos) == 0) {
			if (CountBytes(&EndMatch) - CountBytes(&StartPos) == OldLength) {
				SelectStart = CurrPos;
				Return = SearchAndStatus(&SelectStart, (WORD)(Flags & 1));
			}
		}
		if (Return) {
			NewPosition(&SelectStart);
			UpdateWindow(hwndMain);
			SelectStart   = StartPos;
			SelectBytePos = CountBytes(&StartPos);
			SelectCount   = CountBytes(&EndMatch) - SelectBytePos;
			InvalidateArea(&SelectStart, SelectCount, 0);
			UpdateWindow(hwndMain);
		}
	}
	CheckClipboard();
	ShowEditCaret();
	GetXPos(&CurrCol);
	return (Return);
}

BOOL Magic, HexInput;

void ReplaceSearched(HWND hDlg)
{	MODEENUM SaveMode = Mode;
	LONG	 Length;

	Magic	 = IsDlgButtonChecked(hDlg, IDC_MAGIC);
	HexInput = IsDlgButtonChecked(hDlg, IDC_HEXSEARCH);
	GetDlgItemTextW(hDlg, IDC_REPLACESTRING, CommandBuf, WSIZE(CommandBuf));
	hwndErrorParent = hDlg;			/*change parent of message box*/
	if ((Magic && !CheckParenPairs()) ||
		!BuildReplaceString(CommandBuf, &Length,
							&CurrPos, SelectCount,
							(WORD)(HexInput | 2 | (Magic ? 0 : 4)))) {
			hwndErrorParent = hwndMain;			/*change parent of message box*/
			return;
	}
	hwndErrorParent = hwndMain;			/*change parent of message box*/
	Mode = InsertMode;
	StartUndoSequence();
	if (SelectCount) DeleteSelected(19);
	HideEditCaret();
	{	LPREPLIST lpRep = &RepList;

		while (Length > (LONG)sizeof(lpRep->Buf)) {
			InsertBuffer((LPBYTE)lpRep->Buf, sizeof(lpRep->Buf), 2);
			Length -= sizeof(lpRep->Buf);
			if ((lpRep = lpRep->Next) == NULL) break;
		}
		if (Length && lpRep != NULL)
			InsertBuffer((LPBYTE)lpRep->Buf, Length, 2);
		FreeRepList();
	}
	GoBackAndChar(&CurrPos);
	Mode = SaveMode;
	FindValidPosition(&CurrPos, (WORD)(Mode==InsertMode));
	SendMessage(hDlg, DM_SETDEFID, IDOK, 0);
	SetFocus(GetDlgItem(hDlg, IDOK));
	EnableWindow(GetDlgItem(hDlg, IDC_REPLACE), FALSE);
}

WCHAR SearchBuf[256];
BOOL  WholeWord, ReplaceOpen;

PWSTR SetupSearchString(HWND hDlg, INT *Error)
{	PWSTR pS = SearchBuf, pD = CommandBuf;
	BOOL  Ok = TRUE, OpenBracket = FALSE;

	Magic		   = IsDlgButtonChecked(hDlg, IDC_MAGIC);
	HexInput	   = IsDlgButtonChecked(hDlg, IDC_HEXSEARCH);
	WholeWord	   = IsDlgButtonChecked(hDlg, IDC_WHOLEWORD);
	IgnoreCaseFlag = IsDlgButtonChecked(hDlg, IDC_MATCHCASE) == FALSE;
	WrapScanFlag   = IsDlgButtonChecked(hDlg, IDC_WRAPSCAN);
	*CommandBuf	   = IsDlgButtonChecked(hDlg, IDC_FORWARD) ? '/' : '?';
	GetDlgItemTextW(hDlg, IDC_SEARCHSTRING, SearchBuf, WSIZE(SearchBuf));
	if (WholeWord) {
		*++pD = '\\';
		*++pD = '<';
	}
	for (;;) {
		switch (*++pD = *pS++) {
			case '\\':
				if (Magic) {
					switch (*pS) {
						case '\0':
							break;
						case '%':
							*++pD = *pS++;
							if (!ISHEX(*pS)) break;
							*++pD = *pS++;
							if (ISHEX(*pS)) *++pD = *pS++;
							continue;
						case '0': case '1': case '2': case '3':
							*++pD = *pS++;
							if (*pS >= '0' && *pS <= '7') *++pD = *pS++;
							if (*pS >= '0' && *pS <= '7') *++pD = *pS++;
							continue;
						default:
							*++pD = *pS++;
							continue;
					}
				}
				if (HexInput) Ok = FALSE;
				break;
			case '/': case '?':
				if (HexInput) {
					Ok = FALSE;
					break;
				}
				if (*pD != *CommandBuf) continue;
				/*FALLTHROUGH*/
			case '\0':
				break;
			case '[':
				OpenBracket = TRUE;
				/*FALLTHROUGH*/
			case '*': case '.':
				if (HexInput && !Magic) {
					Ok = FALSE;
					break;
				}
				if (!MagicFlag != !Magic) break;
				continue;
			case '^': case '$':
				if (!Magic) {
					if (HexInput) Ok = FALSE;
					break;
				}
				continue;
			case '-': case ']':
				if (Magic && OpenBracket) {
					if (!(OpenBracket = *pD != ']') && !MagicFlag) break;
					continue;
				}
				/*FALLTHROUGH*/
			default:
				if (HexInput) {
					if (ISHEX(*pD)) {
						pD[2] = *pD;
						*pD++ = '\\'; *pD++ = '%';
						if (ISHEX(*pS)) *++pD = *pS++;
						if (UtfEncoding == 16 && ISHEX(pS[0]) && ISHEX(pS[1])) {
							*++pD = *pS++;
							*++pD = *pS++;
						}
					} else if (*pD-- != ' ') {
						Ok = FALSE;
						break;
					}
				}
				continue;
		}
		if (!Ok) {
			if (Error != NULL) *Error = 239;
			return (NULL);
		}
		if (*pD == '\0') break;
		pD[1] = *pD;
		*pD++ = '\\';
	}
	if (WholeWord) wcscpy(pD, L"\\>");
	return BuildMatchList(CommandBuf, *CommandBuf, Error);
}

void SearchOk(HWND hDlg)
{	INT  ErrNo = 0;
	HWND hSearchButton, hFocus;
	BOOL OldSuppress = SuppressStatusBeep;

	SuppressStatusBeep = TRUE;
	hFocus = GetFocus();
	EnableWindow(hSearchButton = GetDlgItem(hDlg, IDOK), FALSE);
	if (SetupSearchString(hDlg, &ErrNo)) {
		if (!MatchValid) ErrNo = SearchErrNo;
		else if (!SearchAgain(0)) ErrNo = 238;
	}
	SuppressStatusBeep = OldSuppress;
	EnableWindow(hSearchButton, TRUE);
	if (hFocus == hSearchButton) SetFocus(hSearchButton);
	if (!ErrNo) {
		extern void HorizontalScrollIntoView(void);

		if (!ReplaceOpen) {
			if (!ViewOnlyFlag)
				EnableWindow(GetDlgItem(hDlg, IDC_SHOWREPLACE), TRUE);
		} else {
			EnableWindow(GetDlgItem(hDlg, IDC_REPLACE), TRUE);
			SendMessage(hDlg, DM_SETDEFID, IDC_REPLACE, 0);
			SendDlgItemMessage(hDlg, IDOK, BM_SETSTYLE,
							   (WPARAM)BS_PUSHBUTTON, TRUE);
		}
		/*main window does not have the focus...*/
		HorizontalScrollIntoView();
	} else {
		if (ReplaceOpen)
			 EnableWindow(GetDlgItem(hDlg, IDC_REPLACE),	 FALSE);
		else EnableWindow(GetDlgItem(hDlg, IDC_SHOWREPLACE), FALSE);
		hwndErrorParent = hDlg;			/*change parent of message box*/
		ErrorBox(ErrNo==216 || ErrNo==217 ? MB_ICONINFORMATION : MB_ICONSTOP,
				 ErrNo);
		hwndErrorParent = hwndMain;		/*reset to normal value*/
	}
}

void EnableControls(HWND hDlg, BOOL Enable)
{
	EnableWindow(GetDlgItem(hDlg, IDC_SEARCHSTRING),  Enable);
	EnableWindow(GetDlgItem(hDlg, IDC_REPLACESTRING), Enable);
	EnableWindow(GetDlgItem(hDlg, IDOK),			  Enable);
	EnableWindow(GetDlgItem(hDlg, IDC_REPLACE),		  Enable);
	EnableWindow(GetDlgItem(hDlg, IDC_REPLACEALL),	  Enable);
}

BOOL ReplacingAll = FALSE;

void GlobalSubst(HWND hDlg)
{	PWSTR p = CommandBuf;
	INT	  n;

	Magic	 = IsDlgButtonChecked(hDlg, IDC_MAGIC);
	HexInput = IsDlgButtonChecked(hDlg, IDC_HEXSEARCH);
	ReplacingAll = TRUE;
	p += n = _snwprintf(p, WSIZE(CommandBuf), L":%%s//");
	n += GetDlgItemTextW(hDlg, IDC_REPLACESTRING, p, WSIZE(CommandBuf) - n);
	while (*p) {
		switch (*p) {
			case '\\':
				if (Magic) {
					if	 (*++p >= '0' && *p <= '7') {
						if (*p >= '0' && *p <= '3') ++p;
						if (*p >= '0' && *p <= '7') ++p;
						if (*p >= '0' && *p <= '7') ++p;
						continue;
					}
					if (*p == '%') {
						if (ISHEX(p[1])) ++p;
						if (ISHEX(p[1])) ++p;
					}
					break;
				}
				/*FALLTHROUGH*/
			case '&':
				if (*p=='&' && !Magic!=MagicFlag) break;
				/*FALLTHROUGH*/
			case '/':
				if (n+1 < WSIZE(CommandBuf)) {
					memmove(p+1, p, 2*wcslen(p)+2);
					*p++ = '\\';
					++n;
				}
				break;
			default:
				if (HexInput) {
					if (!ISHEX(*p)) {
						if (*p != ' ') {
							hwndErrorParent = hDlg;
							ErrorBox(MB_ICONSTOP, 239);
							hwndErrorParent = hwndMain;
							return;
						}
						memmove(p, p+1, 2*wcslen(p));
						--n;
						continue;
					}
					if (n+2 < WSIZE(CommandBuf)) {
						memmove(p+2, p, 2*wcslen(p)+2);
						*p++ = '\\';
						*p++ = '%';
						if (ISHEX(p[1])) ++p;
						n += 2;
					}
				}
		}
		++p;
	}
	if (n+2 < WSIZE(CommandBuf)) wcscpy(p, L"/g");
	SendMessage(hDlg, DM_SETDEFID, IDCANCEL, 0);
	SetFocus(GetDlgItem(hDlg, IDCANCEL));
	EnableControls(hDlg, FALSE);
	CommandExec(CommandBuf);
	EnableControls(hDlg, TRUE);
	ReplacingAll = FALSE;
}

HWND hwndSearch;

BOOL CALLBACK SearchCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	extern BOOL SearchBoxPosition, HexEditTextSide;
	extern INT  SearchBoxX, SearchBoxY;
	static RECT DlgRect;
	static BOOL Enabled;
	static CHAR CloseString[10];

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		RECT r;

		case WM_INITDIALOG:
			GetWindowRect(hDlg, &DlgRect);
			GetWindowRect(GetDlgItem(hDlg, IDC_REPLACE), &r);
			SetWindowPos(hDlg, 0, SearchBoxX, SearchBoxY,
								  DlgRect.right - DlgRect.left,
								  r.top - DlgRect.top,
						 SearchBoxPosition ? SWP_NOZORDER
										   : SWP_NOZORDER | SWP_NOMOVE);
			SendMessage(hDlg, DM_REPOSITION, 0, 0);
			EnableWindow(GetDlgItem(hDlg, IDC_SHOWREPLACE), FALSE);
			if (HexInput != (HexEditMode && !HexEditTextSide)) {
				HexInput ^= TRUE;
				*SearchBuf = '\0';
			}
			if (SelectCount && (HexInput ? 3 : 1) * SelectCount
							   < WSIZE(SearchBuf)) {
				POSITION SelPos;
				INT		 i, c;
				PWSTR	 p = SearchBuf;

				SelPos = SelectStart;
				for (i=(INT)SelectCount; i; --i) {
					if (HexInput) {
						c = ByteAt(&SelPos);
						if (Advance(&SelPos, 1) != 1) break;
						if (UtfEncoding == 16 && i > 1) {
							if (UtfLsbFirst)
								c |= ByteAt(&SelPos) << 8;
							else c = (c << 8) | ByteAt(&SelPos);
							if (Advance(&SelPos, 1) != 1) break;
							--i;
						}
						p += _snwprintf(p, WSIZE(SearchBuf) - (p-SearchBuf),
										UtfEncoding == 16 ? L"%04x " : L"%02x ",
										c);
					} else {
						if ((c = CharAt(&SelPos)) == C_CRLF) c = '\r';
						else if (!UtfEncoding) c = CharSetToUnicode(c);
						if (UtfEncoding == 16) {
							if (i > 1) --i;
							if (Advance(&SelPos, 2) != 2) break;
						} else if (Advance(&SelPos, 1) != 1) break;
						*p++ = c;
					}
				}
				if (HexInput) --p;
				*p = '\0';
				if (*SearchBuf && !ViewOnlyFlag)
					EnableWindow(GetDlgItem(hDlg, IDC_SHOWREPLACE), TRUE);
			} else {
				PWSTR p;

				if ((p = ExtractIdentifier(NULL)) != NULL)
					wcsncpy(SearchBuf, p, WSIZE(SearchBuf));
			}
			Enabled = *SearchBuf!='\0';
			if (Enabled) {
				EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
				SetDlgItemTextW(hDlg, IDC_SEARCHSTRING, SearchBuf);
				SendMessage(hDlg, DM_SETDEFID, IDOK, 0L);
			} else {
				SendMessage(hDlg, DM_SETDEFID, IDCANCEL, 0L);
				EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
			}
			EnableWindow(GetDlgItem(hDlg, IDC_REPLACESTRING), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_REPLACE),       FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_REPLACEALL),    FALSE);
			CheckDlgButton(hDlg, *SrchDispBuf=='?' ? IDC_BACKWARD : IDC_FORWARD,
								 TRUE);
			CheckDlgButton(hDlg, IDC_MATCHCASE,	!IgnoreCaseFlag);
			CheckDlgButton(hDlg, IDC_MAGIC,		Magic);
			CheckDlgButton(hDlg, IDC_HEXSEARCH, HexInput);
			CheckDlgButton(hDlg, IDC_WHOLEWORD,	WholeWord);
			CheckDlgButton(hDlg, IDC_WRAPSCAN,	WrapScanFlag);
			LOADSTRING(hInst, 909, CloseString, sizeof(CloseString));
			ReplaceOpen = FALSE;
			PostMessage(hDlg, WM_COMMAND, 4569, 0);	/*for Wine*/
			return (TRUE);

		case WM_COMMAND:
			switch (COMMAND) {
				case 4569:
					/*Disable/enable again for Wine...*/
					EnableWindow(GetDlgItem(hDlg, IDOK), Enabled);
					break;
				case IDOK:
					SearchOk(hDlg);
					break;
				case IDCANCEL:
					if (ReplacingAll) Interrupted = TRUE;
					PostMessage(hDlg, WM_CLOSE, 0, 0);
					break;
				case IDC_SEARCHSTRING:
					GetDlgItemTextW(hDlg, IDC_SEARCHSTRING, CommandBuf, 4);
					if (Enabled != (*CommandBuf != '\0')) {
						if (Enabled ^= TRUE) {
							EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
							SendMessage(hDlg, DM_SETDEFID, IDOK, 0L);
						} else {
							SendMessage(hDlg, DM_SETDEFID, IDCANCEL, 0L);
							EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
						}
						if (ReplaceOpen) {
						  EnableWindow(GetDlgItem(hDlg,IDC_REPLACE),   Enabled);
						  EnableWindow(GetDlgItem(hDlg,IDC_REPLACEALL),Enabled);
						}
					}
					break;
				case IDC_SHOWREPLACE:
					EnableWindow(GetDlgItem(hDlg, IDC_REPLACESTRING), TRUE);
					EnableWindow(GetDlgItem(hDlg, IDC_REPLACE), SelectCount!=0);
					EnableWindow(GetDlgItem(hDlg, IDC_REPLACEALL),	  TRUE);
					SetWindowPos(hDlg, 0,0,0, DlgRect.right-DlgRect.left,
											  DlgRect.bottom-DlgRect.top,
								 SWP_NOZORDER | SWP_NOMOVE);
					SendMessage(hDlg, DM_REPOSITION, 0, 0);
					SetFocus(GetDlgItem(hDlg, IDC_REPLACESTRING));
					SendMessage(hDlg, DM_SETDEFID, IDC_REPLACE, 0L);
					EnableWindow(GetDlgItem(hDlg, IDC_SHOWREPLACE),  FALSE);
					ReplaceOpen = TRUE;
					break;
				case IDC_REPLACE:
					ReplaceSearched(hDlg);
					SetDlgItemText(hDlg, IDCANCEL, CloseString);
					break;
				case IDC_REPLACEALL:
					SetupSearchString(hDlg, NULL);
					GlobalSubst(hDlg);
					SetDlgItemText(hDlg, IDCANCEL, CloseString);
			}
			return (TRUE);

		case WM_MOVE:
		case WM_CLOSE:
			SearchBoxPosition = TRUE;
			GetWindowRect(hDlg, &r);
			SearchBoxX = r.left;
			SearchBoxY = r.top;
			if (uMsg == WM_CLOSE) {
				DestroyWindow(hDlg);
				hwndSearch = NULL;
			}
			return (TRUE);
	}
	return (FALSE);
}

void SearchDialog(void)
{	static DLGPROC DlgProc;

	if (!DlgProc)
		 DlgProc = (DLGPROC)MakeProcInstance((FARPROC)SearchCallback, hInst);
	if (hwndSearch) BringWindowToTop(hwndSearch);
	else hwndSearch =
		 CreateDialogW(hInst, MAKEINTRESW(IDD_SEARCH), hwndMain, DlgProc);
}

BOOL CALLBACK SubstChkCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	SEARCHCONFIRMRESULT	Result;
	static RECT			Rect;
	static BOOL			RectDefined = FALSE;

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			if (RectDefined)
				SetWindowPos(hDlg, 0, Rect.left, Rect.top, 0, 0,
				                      SWP_NOZORDER | SWP_NOSIZE);
			return (TRUE);

		case WM_COMMAND:
			switch (wPar) {
				case IDOK:
					Result = ConfirmReplace;
					break;
				case 100:
					Result = ConfirmSkip;
					break;
				case IDCANCEL:
					Result = ConfirmAbort;
					break;
			}
			GetWindowRect(hDlg, &Rect);
			RectDefined = TRUE;
			EndDialog(hDlg, Result);
			return (TRUE);
	}
	return (FALSE);
}

SEARCHCONFIRMRESULT ConfirmSubstitute(void)
{	SEARCHCONFIRMRESULT	Res;
	static DLGPROC		DlgProc;

	if (!DlgProc)
		 DlgProc = (DLGPROC)MakeProcInstance((FARPROC)SubstChkCallback, hInst);
	Res = DialogBoxW(hInst, MAKEINTRESW(IDD_SUBSTCONFIRM), hwndMain, DlgProc);
	return (Res);
}
