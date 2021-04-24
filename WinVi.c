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
 *     28-Dec-2000	new owner-draw sunken control for about box and colors
 *      1-Jan-2001	some preparations for keyboard mappings (not working yet)
 *     28-May-2002	make email and web address readable in version dialogbox
 *     27-Jul-2002	force an initial redraw of the scrollbar
 *     12-Sep-2002	+EBCDIC quick hack (Christian Franke)
 *     25-Sep-2002	prevent nested ExpandPath calls
 *      6-Oct-2002	settings for new global variable TabExpandDomains (Win32)
 *      8-Nov-2002	prevent Ctrl+C event after Ctrl+Break event
 *     15-Nov-2002	Ctrl+Alt+V as escape character in insert/replace mode
 *     12-Jan-2003	help contexts added
 *     15-Jan-2003	several changes to keyboard handling
 *      4-Apr-2004	keep hourglass until InterCommInit() returned
 *      9-Jan-2005	new email address in about dialog
 *     12-Mar-2005	null-pointer check for dropped files
 *      9-Apr-2006	fixed scroll amount for mouse wheel handling in Windows XP
 *     22-Jun-2007	UTF-8 and UTF-16 Unicode menu entries
 *      5-Jul-2007	WM_UNICHAR handling
 *     22-Jul-2007	several changes for unicode window title handling
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *      7-Sep-2009	interruptung shell processes running in background
 *     31-Aug-2010	DLL load from system directory only
 */

#include <windows.h>
#include <VersionHelpers.h>
#include <shellapi.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#include "myassert.h"
#include "ctl3d.h"
#include "winvi.h"
#include "colors.h"
#include "paint.h"
#include "resource.h"
#include "page.h"
#include "intercom.h"
#include "printh.h"
#include "toolbar.h"
#include "status.h"
#include "exec.h"
#include "tabctrl.h"
#include "map.h"

#if !defined(WM_CONTEXTMENU)
	#define  WM_CONTEXTMENU 0x007B
#endif
#if !defined(WM_UNICHAR)
	#define  WM_UNICHAR     0x0109
#endif
#if !defined(WM_MOUSEWHEEL)
	#define  WM_MOUSEWHEEL  0x020a
#endif
#if !defined(UNICODE_NOCHAR)
	#define  UNICODE_NOCHAR 0xffff
#endif
#if !defined(SPI_GETWHEELSCROLLLINES)
	#define  SPI_GETWHEELSCROLLLINES 104
#endif

PCSTR  ApplName  = "WinVi";
PCWSTR ApplNameW = L"WinVi";
PCWSTR ClassName = L"Ramo's WinVi";
RECT   ClientRect, WholeWindow;
INT    x = 40, y = 60, Width = 687, Height = 500;
BOOL   Maximized;
LONG   PageThreshold = 500;
BOOL   UnsafeSettings = FALSE;
BOOL   RefreshFlag = FALSE;
BOOL   StartupPhase = TRUE;

BOOL AllocStringA(LPSTR *Destination, LPCSTR Source)
{	LPSTR p;

	if ((p = _fcalloc(lstrlen(Source) + sizeof(CHAR), sizeof(CHAR))) == NULL)
		return (FALSE);
	if (*Destination) _ffree(*Destination);
	lstrcpy(p, Source);
	*Destination = p;
	return(TRUE);
}

BOOL AllocStringW(PWSTR *Destination, PCWSTR Source)
{	PWSTR p;

	if ((p = _fcalloc(wcslen(Source) + sizeof(WCHAR), sizeof(WCHAR))) == NULL)
		return (FALSE);
	if (*Destination) _ffree(*Destination);
	wcscpy(p, Source);
	*Destination = p;
	return(TRUE);
}

VOID MakeBackgroundBrushes(VOID)
{	HDC hDC;

	if (SelBkgndBrush) DeleteObject(SelBkgndBrush);
	SelBkgndBrush = CreateSolidBrush(SelBkgndColor);
	if (BkgndBrush) DeleteObject(BkgndBrush);
	hDC = GetDC(NULL);
	if (hDC) {
		BkgndBrush = CreateSolidBrush
					 (GetNearestColor((HDC)hDC, BackgroundColor));
		ReleaseDC(NULL, hDC);
	} else BkgndBrush = CreateSolidBrush(BackgroundColor);
}

extern BOOL	IsFixedFont;

BOOL SelectBkBitmap(HWND hParent)
{	PWSTR				 p;
	static WCHAR		 BitmapDir[260], Filter[50], Title[40];
	static OPENFILENAMEW Ofn = {
		sizeof(OPENFILENAMEW), 0, 0, NULL, NULL, 0, 1,
		BitmapFilename, WSIZE(BitmapFilename),
		NULL, 0, NULL, NULL,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR,
		0, 0, L".bmp", 0, NULL, NULL
	};

	if (*BitmapFilename) wcscpy(BitmapDir, BitmapFilename);
	else GetModuleFileNameW(hInst, BitmapDir, WSIZE(BitmapDir));
	for (p=BitmapDir+wcslen(BitmapDir); p>BitmapDir && *p!='/' && *p!='\\';
										--p);
	*p = '\0';
	if (!*Filter) {
		LOADSTRINGW(hInst, 281, Title,  WSIZE(Title));
		LOADSTRINGW(hInst, 292, Filter, WSIZE(Filter));
		SeparateAllStringsW(Filter);
	}
	Ofn.lpstrInitialDir = *BitmapDir ? BitmapDir : NULL;
	Ofn.hwndOwner	= hParent;
	Ofn.hInstance	= hInst;
	Ofn.lpstrFilter	= Filter;
	Ofn.lpstrTitle	= Title;
	StopMap(TRUE);
	if (!GetOpenFileNameW(&Ofn)) return (FALSE);
	return (TRUE);
}

BOOL GetEditLong(HWND hDlg, INT EditId, INT Min, LONG Max, LONG *Value)
{	LONG i;
	PSTR p;
	static CHAR Buf[30];

	GetDlgItemText(hDlg, EditId, Buf, sizeof(Buf));
	i = strtol(Buf, &p, 10);
	while (*p == ' ') ++p;
	if (*p) {
		LOADSTRING(hInst, 309, Buf, sizeof(Buf));
		MessageBox(hDlg, Buf, NULL, MB_OK | MB_ICONSTOP);
		SetFocus(GetDlgItem(hDlg, EditId));
		return (FALSE);
	}
	if (i < Min || i > Max) {
		LOADSTRING(hInst, 310 + (i >= Min), Buf, sizeof(Buf));
		MessageBox(hDlg, Buf, NULL, MB_ICONSTOP | MB_OK);
		SetFocus(GetDlgItem(hDlg, EditId));
		return (FALSE);
	}
	*Value = i;
	return (TRUE);
}

#define	SETFLAG(f, id)								\
	if (f != (BOOL)IsDlgButtonChecked(hDlg, id)) {	\
		f ^= TRUE;									\
	}
#define	SETFLAG_AND_UPDATE(f, id, cond)				\
	if (f != (BOOL)IsDlgButtonChecked(hDlg, id)) {	\
		f ^= TRUE;									\
		if (cond) Update = TRUE;					\
	}

BOOL DisplaySettingsCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	CHAR Buf[260];
	LONG i;

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			CheckDlgButton(hDlg, IDC_TERSE,		   TerseFlag);
			CheckDlgButton(hDlg, IDC_BREAKLINES,   BreakLineFlag);
			CheckDlgButton(hDlg, IDC_AUTORESTORE,  AutoRestoreFlag);
			CheckDlgButton(hDlg, IDC_SHOWMATCH,	   ShowMatchFlag);
			CheckDlgButton(hDlg, IDC_LINENUMBERS,  NumberFlag);
			CheckDlgButton(hDlg, IDC_HEXADDRESSES, HexNumberFlag);
			CheckDlgButton(hDlg, IDC_REFRESHBKG,   RefreshFlag);
			wsprintf(Buf, "%u", TabStop);
			SetDlgItemText(hDlg, IDC_TABSTOP, Buf);
			wsprintf(Buf, "%u", MaxScroll);
			SetDlgItemText(hDlg, IDC_MAXSCROLL, Buf);
			return (TRUE);

		case WM_COMMAND:
			switch (COMMAND) {
				case IDOK: case IDC_STORE: case ID_APPLY:
					SETFLAG(TerseFlag,		 IDC_TERSE);
					SETFLAG(AutoRestoreFlag, IDC_AUTORESTORE);
					SETFLAG(ShowMatchFlag,	 IDC_SHOWMATCH);
					SETFLAG(RefreshFlag,	 IDC_REFRESHBKG);
					if (BreakLineFlag !=
							(BOOL)IsDlgButtonChecked(hDlg, IDC_BREAKLINES)) {
						BreakLineFlag ^= TRUE;
						InvalidateText();
					}
					{	INT Update = FALSE;

						SETFLAG_AND_UPDATE(NumberFlag,	 IDC_LINENUMBERS,
														 !HexEditMode);
						SETFLAG_AND_UPDATE(HexNumberFlag,IDC_HEXADDRESSES,
														 HexEditMode);
						if (!GetEditLong(hDlg, IDC_TABSTOP, 1, 80, &i))
							return (FALSE);
						if (i != TabStop) {
							TabStop = (INT)i;
							Update = TRUE;
						}
						if (!GetEditLong(hDlg, IDC_MAXSCROLL, 0, 80, &i))
							return (FALSE);
						if (i != MaxScroll) MaxScroll = (INT)i;
						if (Update) InvalidateText();
					}
					if (COMMAND == IDC_STORE) SaveDisplaySettings();
					DlgResult = TRUE;
					if (COMMAND != IDOK) return (TRUE);
					/*FALLTHROUGH*/
				case IDCANCEL:
					EndDialog(hDlg, 0);
			}
			return (TRUE);
	}
	return (FALSE);
}

BOOL EditSettingsCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	CHAR Buf[260];
	LONG i;

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			CheckDlgButton(hDlg, IDC_AUTOINDENT,	AutoIndentFlag);
			CheckDlgButton(hDlg, IDC_AUTOWRITE,		AutoWriteFlag);
			CheckDlgButton(hDlg, IDC_WRAPSCAN,		WrapScanFlag);
			CheckDlgButton(hDlg, IDC_NOTEPADNL,		WrapPosFlag);
			CheckDlgButton(hDlg, IDC_IGNORECASE,	IgnoreCaseFlag);
			CheckDlgButton(hDlg, IDC_DEFAULTINSERT,	DefaultInsFlag);
			CheckDlgButton(hDlg, IDC_READONLY,		ReadOnlyFlag);
			CheckDlgButton(hDlg, IDC_MAGIC,			MagicFlag);
			CheckDlgButton(hDlg, IDC_FULLROWEXTEND, FullRowFlag);
			if (UndoLimit == (ULONG)-1)
				LOADSTRING(hInst, 336, Buf, sizeof(Buf));
			else wsprintf(Buf, "%lu", UndoLimit/1024);
			SetDlgItemText(hDlg, IDC_UNDOLIMIT, Buf);
			wsprintf(Buf, "%u", ShiftWidth);
			SetDlgItemText(hDlg, IDC_SHIFTWIDTH, Buf);
			#if !defined(WIN32)
				ShowWindow(GetDlgItem(hDlg, IDC_TABEXPDOMAINTITLE), SW_HIDE);
				ShowWindow(GetDlgItem(hDlg, IDC_TABEXPANDDOMAINS),  SW_HIDE);
			#endif
			return (TRUE);

		case WM_COMMAND:
			switch (COMMAND) {

				case IDOK: case IDC_STORE: case ID_APPLY:
					SETFLAG(AutoIndentFlag,	 IDC_AUTOINDENT);
					SETFLAG(AutoWriteFlag,	 IDC_AUTOWRITE);
					SETFLAG(WrapScanFlag,	 IDC_WRAPSCAN);
					SETFLAG(WrapPosFlag,	 IDC_NOTEPADNL);
					SETFLAG(IgnoreCaseFlag,	 IDC_IGNORECASE);
					SETFLAG(DefaultInsFlag,  IDC_DEFAULTINSERT);
					SETFLAG(MagicFlag,		 IDC_MAGIC);
					SETFLAG(FullRowFlag,	 IDC_FULLROWEXTEND);
					if (ReadOnlyFlag !=
							(BOOL)IsDlgButtonChecked(hDlg, IDC_READONLY)) {
						ReadOnlyFlag ^= TRUE;
						/*ReadOnlyFile = ReadOnlyFlag;*/
					}
					if (GetDlgItemText(hDlg, IDC_UNDOLIMIT, Buf, sizeof(Buf))) {
						PSTR p;

						for (p=Buf; *p==' '; ++p);
						if (*p>='0' && *p<='9')
							if ((UndoLimit = strtol(p, NULL, 10)) >= 4194304)
								UndoLimit = (ULONG)-1;
							else UndoLimit *= 1024;
						else UndoLimit = (ULONG)-1;
					}
					if (!GetEditLong(hDlg, IDC_SHIFTWIDTH, 1, 80, &i))
						return (FALSE);
					if (i != ShiftWidth) ShiftWidth = (INT)i;
					if (COMMAND == IDC_STORE) SaveEditSettings();
					DlgResult = TRUE;
					if (COMMAND != IDOK) return (TRUE);
					/*FALLTHROUGH*/
				case IDCANCEL:
					EndDialog(hDlg, 0);
			}
			return (TRUE);
	}
	return (FALSE);
}

BOOL FileSettingsCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	WCHAR BufW[260];
	LONG  i;

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			CheckDlgButton(hDlg, IDC_LOWERCASE,		LcaseFlag);
			CheckDlgButton(hDlg, IDC_MOREFILESQUIT,	QuitIfMoreFlag);
			CheckDlgButton(hDlg, IDC_DEFAULTTMP, DefaultTmpFlag);
			EnableWindow(GetDlgItem(hDlg, IDC_TMPTITLE),  !DefaultTmpFlag);
			EnableWindow(GetDlgItem(hDlg, IDC_TMPFOLDER), !DefaultTmpFlag);
			if (DefaultTmpFlag) {
				GetSystemTmpDir(BufW, WSIZE(BufW));
				AllocStringW(&TmpDirectory, BufW);
			}
			SetDlgItemTextW(hDlg, IDC_TMPFOLDER, TmpDirectory != NULL
												 ? TmpDirectory : NULL);
			SetDlgItemTextW(hDlg, IDC_TAGS, Tags != NULL ? Tags : NULL);
			_snwprintf(BufW, WSIZE(BufW), L"%ld", PageThreshold);
			SetDlgItemTextW(hDlg, IDC_PAGE_THRESHOLD, BufW);
			#if 1
				ShowWindow(GetDlgItem(hDlg, IDC_BKUPTITLE1),		SW_HIDE);
				ShowWindow(GetDlgItem(hDlg, IDC_BKUPTITLE2),		SW_HIDE);
				ShowWindow(GetDlgItem(hDlg, IDC_NUMBER_OF_BACKUPS),	SW_HIDE);
			#endif
			SetDlgItemTextW(hDlg, IDC_TABEXPANDDOMAINS, TabExpandDomains);
			return (TRUE);

		case WM_COMMAND:
			switch (COMMAND) {
				case IDC_DEFAULTTMP:
					{	BOOL Checked = IsDlgButtonChecked(hDlg, IDC_DEFAULTTMP);
						EnableWindow(GetDlgItem(hDlg, IDC_TMPTITLE),  !Checked);
						EnableWindow(GetDlgItem(hDlg, IDC_TMPFOLDER), !Checked);
					}
					break;

				case IDOK: case IDC_STORE: case ID_APPLY:
					SETFLAG(LcaseFlag,		 IDC_LOWERCASE);
					SETFLAG(QuitIfMoreFlag,	 IDC_MOREFILESQUIT);
					SETFLAG(DefaultTmpFlag,  IDC_DEFAULTTMP);
					GetDlgItemTextW(hDlg, IDC_TMPFOLDER, BufW, WSIZE(BufW));
					AllocStringW(&TmpDirectory, BufW);
					GetDlgItemTextW(hDlg, IDC_TAGS, BufW, WSIZE(BufW));
					AllocStringW(&Tags, BufW);
					if (!GetEditLong(hDlg, IDC_PAGE_THRESHOLD, 8, 32767, &i))
						return (FALSE);
					if (i != PageThreshold) PageThreshold = i;
					GetDlgItemTextW(hDlg, IDC_TABEXPANDDOMAINS,
									TabExpandDomains, WSIZE(TabExpandDomains));
					if (COMMAND == IDC_STORE) SaveFileSettings();
					DlgResult = TRUE;
					if (COMMAND != IDOK) return (TRUE);
					/*FALLTHROUGH*/
				case IDCANCEL:
					EndDialog(hDlg, 0);
			}
			return (TRUE);
	}
	return (FALSE);
}

static VOID SetMenuRadioStyle(VOID)
{
	#if defined(WIN32)
		static CHAR Buf[40];
		static MENUITEMINFO miiRadioMenu = {sizeof(MENUITEMINFO), MIIM_TYPE, 0,
											0,0,NULL,NULL,NULL,0,Buf,0};
		HMENU hMenu = GetMenu(hwndMain);

		miiRadioMenu.cch = sizeof(Buf);
		GetMenuItemInfo(hMenu, ID_ANSI,    FALSE, &miiRadioMenu);
		miiRadioMenu.fType |= MFT_RADIOCHECK;
		SetMenuItemInfo(hMenu, ID_ANSI,    FALSE, &miiRadioMenu);

		miiRadioMenu.cch = sizeof(Buf);
		GetMenuItemInfo(hMenu, ID_OEM,     FALSE, &miiRadioMenu);
		miiRadioMenu.fType |= MFT_RADIOCHECK;
		SetMenuItemInfo(hMenu, ID_OEM,     FALSE, &miiRadioMenu);

		miiRadioMenu.cch = sizeof(Buf);
		GetMenuItemInfo(hMenu, ID_UTF8,    FALSE, &miiRadioMenu);
		miiRadioMenu.fType |= MFT_RADIOCHECK;
		SetMenuItemInfo(hMenu, ID_UTF8,    FALSE, &miiRadioMenu);

		miiRadioMenu.cch = sizeof(Buf);
		GetMenuItemInfo(hMenu, ID_UTF16LE, FALSE, &miiRadioMenu);
		miiRadioMenu.fType |= MFT_RADIOCHECK;
		SetMenuItemInfo(hMenu, ID_UTF16LE, FALSE, &miiRadioMenu);

		miiRadioMenu.cch = sizeof(Buf);
		GetMenuItemInfo(hMenu, ID_UTF16BE, FALSE, &miiRadioMenu);
		miiRadioMenu.fType |= MFT_RADIOCHECK;
		SetMenuItemInfo(hMenu, ID_UTF16BE, FALSE, &miiRadioMenu);

		miiRadioMenu.cch = sizeof(Buf);
		GetMenuItemInfo(hMenu, ID_EBCDIC,  FALSE, &miiRadioMenu);
		miiRadioMenu.fType |= MFT_RADIOCHECK;
		SetMenuItemInfo(hMenu, ID_EBCDIC,  FALSE, &miiRadioMenu);
	#endif
}

HMENU hPopupMenu;

VOID ChangeLanguage(UINT NewLanguage)
{
	if (Language != NewLanguage) {
		HMENU NewMenu;

		Language = NewLanguage;
		NewMenu = LoadMenu(hInst, MAKEINTRES(100));
		if (NewMenu) {
			HMENU OldMenu;
			extern VOID	InitFileList(VOID);
			extern CHAR ErrorTitle[];

			*ErrorTitle = '\0';
			OldMenu = GetMenu(hwndMain);
			DestroyMenu(SetMenu(hwndMain, NewMenu) ? OldMenu : NewMenu);
			SetMenuRadioStyle();
			InitFileList();
		}
		if (hPopupMenu) {
			DestroyMenu(hPopupMenu);
			hPopupMenu = 0;
		}
		NewCapsString();
		GenerateTitle(TRUE);
		LoadTips();
	}
}

LPSTR NewLangStr = NULL;

BOOL LanguageCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	static UINT  NewLanguage;
	extern CHAR  ViLang[];

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			if		(ViLang[0] == 'f' && ViLang[1] == 'r') wPar = IDC_FRENCH;
			else if (ViLang[0] == 'e' && ViLang[1] == 's') wPar = IDC_SPANISH;
			else if (ViLang[0] == 'd' && ViLang[1] == 'e') wPar = IDC_GERMAN;
			else if (ViLang[0] == 'e' && ViLang[1] == 'n') wPar = IDC_ENGLISH;
			else										   wPar = IDC_AUTOLANG;
			CheckRadioButton(hDlg, IDC_ENGLISH, IDC_AUTOLANG, wPar);
			/*FALLTHROUGH*/
		case WM_COMMAND:
			switch (COMMAND) {
				case IDC_ENGLISH:
					NewLanguage = 9000;
					NewLangStr  = "en";
					break;
				case IDC_SPANISH:
					NewLanguage = 10000;
					NewLangStr  = "es";
					break;
				case IDC_FRENCH:
					NewLanguage = 12000;
					NewLangStr  = "fr";
					break;
				case IDC_GERMAN:
					NewLanguage = 7000;
					NewLangStr  = "de";
					break;
				case IDC_AUTOLANG:
					NewLanguage = GetDefaultLanguage(TRUE);
					NewLangStr  = NULL;
					break;
				case IDC_STORE:
					SaveLanguage(NewLangStr);
					DlgResult = TRUE;
					break;
				case ID_APPLY:
				case IDOK:
					ChangeLanguage(NewLanguage);
					lstrcpy(ViLang, NewLangStr==NULL ? "" : NewLangStr);
					DlgResult = TRUE;
					if (COMMAND != IDOK) return (TRUE);
					/*FALLTHROUGH*/
				case IDCANCEL:
					EndDialog(hDlg, COMMAND!=IDCANCEL);
			}
			break;

		default:
			return (FALSE);
	}
	return (TRUE);
}

static VOID SelectLanguage(VOID)
{	static DLGPROC DlgProc;

	if (!DlgProc)
		 DlgProc = (DLGPROC)MakeProcInstance((FARPROC)LanguageCallback, hInst);
	DialogBox(hInst, MAKEINTRES(IDD_LANGUAGE), hwndMain, DlgProc);
}

INT WrapScrollStyle, WrapPos = IDC_WRAP_WORD1;
#define WSS_SHOW_SCROLLBAR		 -1
#define WSS_NONE				  0
#define WSS_WRAP_AT_RIGHT_BORDER  1

BOOL CALLBACK WrapScrollCallback
		(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	CHAR Buf[16];
	BOOL SetEnable = FALSE, Enabled;
	static BOOL SaveWrapPos;

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			switch (WrapScrollStyle) {
				case WSS_NONE:
					CheckDlgButton(hDlg, IDC_NO_WRAP_OR_SCROLLBAR, TRUE);
					break;
				case WSS_WRAP_AT_RIGHT_BORDER:
					CheckDlgButton(hDlg, IDC_WRAP_AT_RIGHT_BORDER, TRUE);
					break;
				case WSS_SHOW_SCROLLBAR:
					CheckDlgButton(hDlg, IDC_SHOW_SCROLLBAR,	   TRUE);
					break;
				default:
					CheckDlgButton(hDlg, IDC_WRAP_AT_COLUMN,	   TRUE);
					wsprintf(Buf, "%d", WrapScrollStyle);
					SetDlgItemText(hDlg, IDC_WRAP_COLUMN, Buf);
			}
			if (WrapScrollStyle <= WSS_NONE) {
				EnableWindow(GetDlgItem(hDlg, IDC_WRAP_CHAR),  FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_WRAP_WORD1), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_WRAP_WORD2), FALSE);
			} else CheckDlgButton(hDlg, WrapPos, TRUE);
			SaveWrapPos = WrapPos;
			break;

		case WM_COMMAND:
			switch (COMMAND) {
				static INT NewStyle = 0;
				case IDC_WRAP_COLUMN:
					if (NOTIFICATION != EN_CHANGE) break;
					CheckRadioButton(hDlg, IDC_WRAP_AT_RIGHT_BORDER,
										   IDC_NO_WRAP_OR_SCROLLBAR,
										   IDC_WRAP_AT_COLUMN);
					/*FALLTHROUGH*/
				case IDC_WRAP_AT_RIGHT_BORDER:
				case IDC_WRAP_AT_COLUMN:
					SetEnable = Enabled = TRUE;
					break;

				case IDC_NO_WRAP_OR_SCROLLBAR:
				case IDC_SHOW_SCROLLBAR:
					SetEnable = TRUE;
					Enabled   = FALSE;
					break;

				case IDC_WRAP_CHAR:
				case IDC_WRAP_WORD1:
				case IDC_WRAP_WORD2:
					WrapPos = COMMAND;
					break;

				case IDOK:
					if (IsDlgButtonChecked(hDlg, IDC_NO_WRAP_OR_SCROLLBAR))
						NewStyle = WSS_NONE;
					else if (IsDlgButtonChecked(hDlg, IDC_WRAP_AT_RIGHT_BORDER))
						NewStyle = WSS_WRAP_AT_RIGHT_BORDER;
					else if (IsDlgButtonChecked(hDlg, IDC_SHOW_SCROLLBAR))
						NewStyle = WSS_SHOW_SCROLLBAR;
					else if (IsDlgButtonChecked(hDlg, IDC_WRAP_AT_COLUMN)) {
						GetDlgItemText(hDlg, IDC_WRAP_COLUMN, Buf, sizeof(Buf));
						NewStyle = (INT)strtol(Buf, NULL, 10);
					}
					if (WrapScrollStyle != NewStyle || WrapPos != SaveWrapPos) {
						WrapScrollStyle  = NewStyle;
						InvalidateText();
					}
					BreakLineFlag = WrapScrollStyle==WSS_WRAP_AT_RIGHT_BORDER;
					EndDialog(hDlg, TRUE);
					break;

				case IDCANCEL:
					WrapPos = SaveWrapPos;
					EndDialog(hDlg, FALSE);
			}
			if (SetEnable && IsWindowEnabled(GetDlgItem(hDlg, IDC_WRAP_CHAR))
							 != Enabled) {
				EnableWindow(GetDlgItem(hDlg, IDC_WRAP_CHAR),  Enabled);
				EnableWindow(GetDlgItem(hDlg, IDC_WRAP_WORD1), Enabled);
				EnableWindow(GetDlgItem(hDlg, IDC_WRAP_WORD2), Enabled);
				CheckDlgButton(hDlg, WrapPos, Enabled);
			}
			break;

		default:
			return (FALSE);
	}
	return (TRUE);
}

static VOID SelectWrapScroll(VOID)
{	static DLGPROC DlgProc;

	if (!DlgProc)
		 DlgProc = (DLGPROC)MakeProcInstance((FARPROC)WrapScrollCallback,hInst);
	DialogBox(hInst, MAKEINTRES(IDD_WRAP_SCROLL), hwndMain, DlgProc);
}

LOGFONT AboutLogFont = {
	0,0,0,0,0, 0,0,0,0,0,0,0,0, "Arial"
};

BOOL CALLBACK AboutCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	static HFONT hFont;
	HDC			 hDC;
	//CHAR		 Buf[56], *p1, *p2; // STEPHANIE

	PARAM_NOT_USED(wPar);
	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			#if defined(WIN32)
				SetDlgItemText(hDlg, IDS_VERSIONBITS, "(32 bit)");
			#else
				SetDlgItemText(hDlg, IDS_VERSIONBITS, "(16 bit)");
			#endif
			SetDlgItemTextW(hDlg, IDS_VERSION, GetViVersion());
			#if _MSC_VER	/* did not find a macro to test in rc file */
				SetDlgItemText(hDlg, IDS_COMPILER, "Microsoft Visual C");
			#endif
			/* // STEPHANIE
			if (GetDlgItemText(hDlg, IDS_AUTHOR, Buf, sizeof(Buf))) {
				SYSTEMTIME t;
				CHAR	   year[12];	

				t.wYear = 0;
				GetSystemTime(&t);
				snprintf(year, sizeof(year),
						 t.wYear < 2005 ? "<year>" : "%d", t.wYear);
				for (p1=Buf,p2="ramowinvide"; *p1&&*p2; ++p1) {
					if (*p1 == '-') *p1 = *p2++;
					else if (*p1 == '@' && p1[-1] == 'o') {
						memmove(p1 + strlen(year), p1, strlen(p1) + 1);
						memcpy(p1, year, strlen(year));
						p1 += strlen(year) - 1;
					}
				}
				SetDlgItemText(hDlg, IDS_AUTHOR, Buf);
			}
			*/
			/* // STEPHANIE
			if (GetDlgItemText(hDlg, IDS_WEBADDRESS, Buf, sizeof(Buf))) {
				for (p1=Buf,p2="wwwwinvide"; *p1&&*p2; ++p1)
					if (*p1 == '-') *p1 = *p2++;
				SetDlgItemText(hDlg, IDS_WEBADDRESS, Buf);
			}
			*/
			#if 0	/*this does not work...*/
			{	HWND Frame;

				if (Frame = GetDlgItem(hDlg, IDC_FRAME)) {
					SetWindowLong(hDlg,  GWL_EXSTYLE,
								  GetWindowLong(hDlg,  GWL_EXSTYLE)
								  | WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE);
					SetWindowLong(Frame, GWL_EXSTYLE,
								  GetWindowLong(Frame, GWL_EXSTYLE)
								  | WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE
								  | WS_EX_WINDOWEDGE    | WS_EX_STATICEDGE);
				}
			}
			#endif
			hDC = GetDC(NULL);
			if (hDC) {
				AboutLogFont.lfHeight =
						-MulDiv(18/*pt*/, GetDeviceCaps(hDC, LOGPIXELSY), 72);
				hFont = CreateFontIndirect(&AboutLogFont);
				if (hFont)
					SendDlgItemMessage(hDlg, IDS_VERSIONTITLE,
									   WM_SETFONT, (WPARAM)hFont, 0);
				ReleaseDC(NULL, hDC);
			}
			return (TRUE);

		case WM_COMMAND:
			EndDialog(hDlg, 0);
			return (TRUE);

		case WM_DESTROY:
			if (hFont) {
				DeleteObject(hFont);
				hFont = NULL;
			}
			return (TRUE);
	}
	return (FALSE);
}

VOID About(HWND hwndParent)
{	static DLGPROC Callback;

	if (!Callback)
		 Callback = (DLGPROC)MakeProcInstance((FARPROC)AboutCallback, hInst);
	DialogBox(hInst, MAKEINTRES(IDD_ABOUT), hwndParent, Callback);
}

PSTR SeparateStringA(PSTR *p)
{	PSTR p2 = *p;

	if ((*p = strchr(p2, ',')) != NULL) *(*p)++ = '\0';
	else *p = "";
	return (p2);
}

VOID SeparateAllStringsA(PSTR p)
{
	while (*p) SeparateStringA(&p);
}

PWSTR SeparateStringW(PWSTR *p)
{	PWSTR p2 = *p;

	if ((*p = wcschr(p2, ',')) != NULL) *(*p)++ = '\0';
	else *p = L"";
	return (p2);
}

VOID SeparateAllStringsW(PWSTR p)
{
	while (*p) SeparateStringW(&p);
}

#ifdef __LCC__
  #define MENUITEMINFOW MENUITEMINFO
  #define MIIRADIOMENU_DWTYPEDATA *(LPWSTR*)&miiRadioMenu.dwTypeData
#else
  #define MIIRADIOMENU_DWTYPEDATA miiRadioMenu.dwTypeData
#endif

VOID ContextMenu(HWND hWnd, INT xPos, INT yPos)
{
	if (!hPopupMenu && (hPopupMenu = CreatePopupMenu()) != 0) {
		static WCHAR MenuStrings[160];
		PWSTR		 p;

		LOADSTRINGW(hInst, 904, MenuStrings, WSIZE(MenuStrings));
		p = MenuStrings;
		{	MENUITEMINFOW miiRadioMenu =
				{sizeof(MENUITEMINFO), MIIM_ID | MIIM_TYPE,
				 MFT_RADIOCHECK | MFT_STRING};
			miiRadioMenu.wID = ID_INSMODE;
			MIIRADIOMENU_DWTYPEDATA = SeparateStringW(&p);
			if (InsertMenuItemW(hPopupMenu, 0, TRUE, &miiRadioMenu)) {
				miiRadioMenu.wID = ID_REPMODE;
				MIIRADIOMENU_DWTYPEDATA = SeparateStringW(&p);
				InsertMenuItemW(hPopupMenu, 1, TRUE, &miiRadioMenu);
				miiRadioMenu.wID = ID_CMDMODE;
				MIIRADIOMENU_DWTYPEDATA = SeparateStringW(&p);
				InsertMenuItemW(hPopupMenu, 2, TRUE, &miiRadioMenu);
			} else {
				AppendMenuW(hPopupMenu, MF_STRING, ID_INSMODE,
							MIIRADIOMENU_DWTYPEDATA);
				AppendMenuW(hPopupMenu, MF_STRING, ID_REPMODE,
							SeparateStringW(&p));
				AppendMenuW(hPopupMenu, MF_STRING, ID_CMDMODE,
							SeparateStringW(&p));
			}
		}
		AppendMenuW(hPopupMenu, MF_SEPARATOR, 0, NULL);
		AppendMenuW(hPopupMenu, MF_STRING,	 ID_TAG,	  SeparateStringW(&p));
		AppendMenuW(hPopupMenu, MF_STRING,	 ID_TAGBACK,  SeparateStringW(&p));
		AppendMenuW(hPopupMenu, MF_SEPARATOR, 0, NULL);
		AppendMenuW(hPopupMenu, MF_STRING,	 ID_UNDO,	  SeparateStringW(&p));
		AppendMenuW(hPopupMenu, MF_SEPARATOR, 0, NULL);
		AppendMenuW(hPopupMenu, MF_STRING,	 ID_CUT,	  SeparateStringW(&p));
		AppendMenuW(hPopupMenu, MF_STRING,	 ID_COPY,	  SeparateStringW(&p));
		AppendMenuW(hPopupMenu, MF_STRING,	 ID_PASTE,	  SeparateStringW(&p));
		AppendMenuW(hPopupMenu, MF_STRING,	 ID_DELETE,	  SeparateStringW(&p));
		AppendMenuW(hPopupMenu, MF_SEPARATOR, 0, NULL);
		AppendMenuW(hPopupMenu, MF_STRING,	 ID_SELECTALL,SeparateStringW(&p));
		AppendMenuW(hPopupMenu, MF_STRING,	 ID_REFRESH,  SeparateStringW(&p));
	}
	if (hPopupMenu) {
		POINT p;
		MouseSelectText(WM_LBUTTONUP, 0, 0, FALSE);
		CheckMenuItem(hPopupMenu, ID_INSMODE,
					  Mode==InsertMode  ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(hPopupMenu, ID_REPMODE,
					  Mode==ReplaceMode ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(hPopupMenu, ID_CMDMODE,
					  Mode==CommandMode ? MF_CHECKED : MF_UNCHECKED);
		DisableMenuItems(1, hPopupMenu);
		p.x = xPos;
		p.y = yPos;
		ClientToScreen(hWnd, &p);
		TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON,
					   p.x, p.y, 0, hWnd, NULL);
	}
}

LPARAM PopupLparam;

VOID SwitchMode(MODEENUM NewMode)
{
	if (Mode != NewMode) {
		if (Mode != CommandMode) {
			   GotChar('\033', 0);
			   GotChar(' ',	   0);
		} else GotChar('\033', 0);
		if (NewMode != CommandMode) GotChar(NewMode==InsertMode ? 'i' : 'R', 0);
		if (PopupLparam && !SelectCount) {
			MouseHandle(WM_LBUTTONDOWN,	0, PopupLparam);
			MouseHandle(WM_LBUTTONUP,	0, PopupLparam);
		}
	}
	PopupLparam = 0;
}

extern char  QueryString[150];
extern DWORD QueryTime;

extern INT nCurrFileListEntry, nNumFileListEntries;
BOOL	   RestoreCaret, SizeGrip;
BOOL	   SessionEnding = FALSE;
UINT	   MsWheelRollMsg = 0, WheelAmount = 3, ScrollMsg = 0;
HWND	   WheelHandler;
BOOL	   IsWinNt2kXpEtc = FALSE;

LRESULT MyDefWindowProc(HWND hWnd, UINT uMsg, WPARAM wPar, LPARAM lPar)
{
	if (IsWinNt2kXpEtc)
		 return DefWindowProcW(hWnd, uMsg, wPar, lPar);
	else return DefWindowProc (hWnd, uMsg, wPar, lPar);
}

LRESULT CALLBACK MainProc(HWND hWnd, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	extern LONG xOffset;

	switch (uMsg) {
		case WM_QUERYENDSESSION:
			if (!AskForSave(hwndMain, 0)) break;
			SaveSettings();
			return (TRUE);

		case WM_SYSCOMMAND:
			RemoveStatusTip();
			return MyDefWindowProc(hWnd, uMsg, wPar, lPar);

		case WM_CLOSE:
			if (!AskForSave(hwndMain, 0)) break;
			QuitCmd(TRUE);
			break;

		case WM_ENDSESSION:
			if (!wPar) break;
			RemoveTmpFile();
			SessionEnding = TRUE;
			break;

		case WM_DESTROY:
			ReleasePaintResources();
			ReleaseBmp(0);
			if (hPopupMenu) DestroyMenu(hPopupMenu);
			PathExpCleanup();
			PostQuitMessage(0);
			RemoveTmpFile();
			hwndMain = 0;
			break;

		case WM_MOVE:
			if (!IsIconic(hWnd) && !IsZoomed(hWnd)) {
				GetWindowRect(hWnd, &WholeWindow);
				UnsafeSettings = TRUE;
			}
			break;

		case WM_GETMINMAXINFO:
			{	MINMAXINFO FAR *lpMmi = (MINMAXINFO FAR *)lPar;
				extern INT	   StatusHeight;
				RECT		   r;

				GetWindowRect(hWnd, &r);
				if (r.right-r.left <= 40) return (1);
				if (Disabled && r.bottom > r.top+StatusHeight+33) {
					lpMmi->ptMinTrackSize.x = lpMmi->ptMaxTrackSize.x =
						r.right - r.left;
					lpMmi->ptMinTrackSize.y = lpMmi->ptMaxTrackSize.y =
						r.bottom - r.top;
				} else {
					lpMmi->ptMinTrackSize.x = 271 +
						(r.right - r.left - ClientRect.right);
					lpMmi->ptMinTrackSize.y = StatusHeight + 33 + TextHeight +
						(r.bottom - r.top - ClientRect.bottom);
				}
			}
			break;

		case WM_SIZE:
			if (wPar != SIZE_MINIMIZED) {
				RECT OldRect;
				BOOL ShowCaretAgain = FALSE;

				OldRect = ClientRect;
				GetClientRect(hWnd, &ClientRect);
				SizeGrip = (wPar != SIZE_MAXIMIZED);
				if (ClientRect.right != OldRect.right) {
					HideEditCaret();
					RecalcStatusRow();
					AdjustWindowParts(OldRect.bottom, ClientRect.bottom);
					if (ClientRect.bottom != OldRect.bottom) NewScrollPos();
					ShowCaretAgain = TRUE;
				} else if (ClientRect.bottom != OldRect.bottom) {
					AdjustWindowParts(OldRect.bottom, ClientRect.bottom);
					NewScrollPos();
				}
				if (SizeGrip) {
					RECT NewRect;

					GetWindowRect(hWnd, &NewRect);
					if (	   NewRect.left   != WholeWindow.left
							|| NewRect.top	  != WholeWindow.top
							|| NewRect.right  != WholeWindow.right
							|| NewRect.bottom != WholeWindow.bottom) {
						WholeWindow = NewRect;
						UnsafeSettings = TRUE;
					}
				}
				if (WrapScrollStyle) {
					extern LONG xOffset, ExpandRedraw, ShrinkRedraw;
					extern INT  RightOffset;
					LONG EndX = ClientRect.right + xOffset - RightOffset;
					extern BOOL Exiting;

					if (Exiting) {
						assert(!Exiting);
						break;
					}
					if (EndX >= ExpandRedraw || EndX < ShrinkRedraw) {
						InvalidateText();
						while (LineInfo && ComparePos(&LineInfo[CrsrLines].Pos,
													  &CurrPos) <= 0) {
							HideEditCaret();
							ScrollLine(1);
							ShowCaretAgain = TRUE;
						}
					}
				}
				if (ShowCaretAgain) ShowEditCaret();
			}
			break;

		case WM_SETFOCUS:
			if (hwndCmd) {
				extern BOOL HideCmd;

				if (!HideCmd) SetFocus(hwndCmd);
			} else {
				ShowEditCaret();
				CheckClipboard();
			}
			break;

		case WM_KILLFOCUS:
			MouseInTextArea(0, 0, 0, 0, 0, TRUE);	/*stop scrolling*/
			HideEditCaret();
			break;

		case WM_ACTIVATEAPP:
			if (wPar) {
				GotKeydown(VK_NUMLOCK);	/*update status if keys changed*/
				GotKeydown(VK_CAPITAL);
			} else TooltipHide(-1);
			break;

		case WM_KEYDOWN:
			WmKeydown(wPar);
			break;

		case WM_CHAR:
			if (wPar >= 0x100) {
				CHAR Buf[50];

				snprintf(Buf, sizeof(Buf),
						 "WinVi32: wide WM_CHAR 0x%x\n", wPar);
				OutputDebugString(Buf);
			}
			WmChar(wPar);
			break;

		case WM_UNICHAR:
			if (wPar == UNICODE_NOCHAR) return TRUE;
			if (wPar < 0x10000) {
				CHAR Buf[50];

				snprintf(Buf, sizeof(Buf),
						 "WinVi32: WM_UNICHAR 0x%x\n", wPar);
				OutputDebugString(Buf);
				WmChar(wPar);
			} else if (wPar >= 0x10000 && wPar < 0x10FFFF) {
				CHAR Buf[50];

				snprintf(Buf, sizeof(Buf),
						 "WinVi32: extended WM_UNICHAR 0x%x\n", wPar);
				OutputDebugString(Buf);
				WmChar((wPar >> 10) - 0x40 + 0xd800);
				WmChar((wPar & 0x3ff) + 0xdc00);
			} else {
				CHAR Buf[50];

				snprintf(Buf, sizeof(Buf),
						 "WinVi32: unhandled WM_UNICHAR 0x%x\n", wPar);
				OutputDebugString(Buf);
			}
			break;

		case WM_SYSKEYDOWN:
			TooltipHide(-1);
			if (wPar == VK_F3) {
				PostMessage(hWnd, WM_COMMAND, ID_SEARCH, 0);
				break;
			} else if (wPar == VK_F10 && GetKeyState(VK_SHIFT) < 0) {
				if (GetKeyState(VK_CONTROL) < 0) {
					/*status context menu...*/
					POINT p;

					p.x = 20;
					p.y = ClientRect.bottom;
					ClientToScreen(hwndMain, &p);
					StatusContextMenu(p.x, p.y);
				} else ContextMenu(hwndMain, (INT)(CaretX-xOffset),
											 (INT)(CaretY+CaretHeight));
				break;
			}
			return MyDefWindowProc(hWnd, uMsg, wPar, lPar);

		case WM_SYSCHAR:
			if (wPar == '\b') {
				if (Disabled) MessageBeep(MB_OK);
				else Undo(1);
				break;
			}
			return MyDefWindowProc(hWnd, uMsg, wPar, lPar);

		case WM_VSCROLL:
			if (!Disabled) {
				#if defined(WIN32)
					Scroll(LOWORD(wPar), HIWORD(wPar));
				#else
					Scroll(wPar, LOWORD(lPar));
				#endif
			}
			break;

		case WM_MOUSEACTIVATE:
			MouseActivated = TRUE;
			return MyDefWindowProc(hWnd, uMsg, wPar, lPar);

		case WM_TIMER:
			if (wPar == 100) MouseSelectText(uMsg, wPar, 0, FALSE);
			else TooltipTimer();
			break;

		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
		case WM_MOUSEMOVE:
			MouseHandle(uMsg, wPar, lPar);
			break;

		case WM_RBUTTONDOWN:
			if (HIWORD(lPar) <  (WORD)nToolbarHeight) break;
			if (HIWORD(lPar) >= (WORD)ClientRect.bottom-StatusHeight) {
				MouseHandle(WM_RBUTTONDOWN, wPar, lPar);
				break;
			}
			if (!SelectCount) {
				PopupLparam = lPar;
				MouseHandle(WM_LBUTTONDOWN,	wPar, lPar);
				MouseHandle(WM_LBUTTONUP,	wPar, lPar);
			}
			ContextMenu(hWnd, (short)LOWORD(lPar), (short)HIWORD(lPar));
			break;

		case WM_CONTEXTMENU:
			if (lPar == -1) ContextMenu(hwndMain, (INT)(CaretX-xOffset),
												  (INT)(CaretY+CaretHeight));
			break;

		case WM_DROPFILES:
			{	HANDLE		 hDrop = (HANDLE)wPar;
				INT			 i, iDropped, nNumDropped;
				extern WCHAR FileNameBuf2[260];
				BOOL		 ShiftPressed;

				ShiftPressed = GetKeyState(VK_SHIFT) < 0;
				nNumDropped = DragQueryFile(hDrop, (UINT)-1, NULL, 0);
				if (nNumDropped) {
					for (iDropped = 0; iDropped < nNumDropped; ++iDropped) {
						DragQueryFileW(hDrop, iDropped,
									   FileNameBuf2, WSIZE(FileNameBuf2));
						LowerCaseFnameW(FileNameBuf2);
						if (ShiftPressed) NewViEditCmd(FileNameBuf2);
						else {
							i = FindOrAppendToFileList(FileNameBuf2);
							if (!iDropped) nCurrFileListEntry = i;
						}
					}
					if (!ShiftPressed) {
						PWSTR File = GetFileListEntry(nCurrFileListEntry);

						if (File != NULL)
							Open(File, 0);
					}
				}
				DragFinish(hDrop);
				if (AutoRestoreFlag && !ShiftPressed) {
					if (IsIconic(hWnd)) ShowWindow(hWnd, SW_NORMAL);
					else {
						#if defined(WIN32)
							SetForegroundWindow(hWnd);
						#else
							BringWindowToTop(hWnd);
						#endif
					}
				}
			}
			break;

		case WM_ERASEBKGND:
			if (hwndMain) {
				RECT Rect;

				if (CaretActive && !RestoreCaret) {
					HideEditCaret();
					RestoreCaret = TRUE;
				}
				{	RECT SaveClientRect;

					SaveClientRect = ClientRect;
					GetClientRect(hWnd, &Rect);
					ClientRect = Rect;
					TextAreaOnly(&Rect, FALSE);
					ClientRect = SaveClientRect;
				}
				if (!PaintBmp((HDC)wPar, 0, &Rect, 0,
							  FirstVisible * TextHeight)) {
					UnrealizeObject(BkgndBrush);
					FillRect((HDC)wPar, &Rect, BkgndBrush);
				}
			}
			return (1L);

		case WM_PAINT:
			{	PAINTSTRUCT	Ps;
				HDC			hDC = BeginPaint(hWnd, &Ps);

				if (hwndMain) Paint(hDC, &Ps.rcPaint);
				EndPaint(hWnd, &Ps);
			}
			break;

		case WM_PALETTECHANGED:
			{	RECT Rect;

				Rect = ClientRect;
				TextAreaOnly(&Rect, FALSE);
				InvalidateRect(hWnd, &Rect, TRUE);
			}
			break;

		case WM_COMMAND:
			if (Disabled && COMMAND!=ID_EXIT && COMMAND!=910 && COMMAND!=911)
				MessageBeep(MB_OK);
			else switch (COMMAND) {
				case ID_NEW:		New(hWnd, TRUE);					break;
				case ID_NEWVI:		CommandExec(L":vi");				break;
				case ID_OPEN:		Open(L"", 0);						break;
				case ID_SAVE:		Save(L"");							break;
				case ID_SAVEAS:		Save(NULL);							break;
				case ID_ALTERNATE:	CommandExec(L":e#");				break;
				case ID_READFILE:	ReadCmd(-1, L"");					break;
				case ID_FILELIST:	ArgsCmd();							break;
				case ID_PRINTSETTINGS:
									{	extern INT CurrTab, PrintTab;

										CurrTab = PrintTab;
										Settings();
									}									break;
				case ID_PRINT:		PrintFromMenu(hWnd);				break;
				case ID_EXIT:		PostMessage(hWnd, WM_CLOSE, 0, 0L);	break;

				case ID_INSMODE:	SwitchMode(InsertMode);				break;
				case ID_REPMODE:	SwitchMode(ReplaceMode);			break;
				case ID_CMDMODE:	SwitchMode(CommandMode);			break;
				case ID_UNDO:		Undo(1);							break;
				case ID_CUT:		ClipboardCut();						break;
				case ID_COPY:		ClipboardCopy();					break;
				case ID_PASTE:		ClipboardPaste();					break;
				case ID_DELETE:		if (SelectCount) GotChar('\b', 0);	break;
				case ID_SELECTALL:	SelectAll();						break;
				case ID_REFRESH:	GotKeydown(VK_F5);					break;
				case ID_TAG:		TagCurrentPosition();				break;
				case ID_TAGBACK:	PopTag(TRUE);						break;

				case ID_SEARCH:		SearchDialog();						break;
				case ID_SRCHAGN:	SearchAgain(0);						break;

				case ID_SETTINGS:	Settings();							break;
				case ID_WRAPSCROLL: SelectWrapScroll();					break;
				case ID_LANGUAGE:	SelectLanguage();					break;
				case ID_ANSI:		SetCharSet(CS_ANSI,	   1);			break;
				case ID_OEM:		SetCharSet(CS_OEM,	   1);			break;
				case ID_UTF8:		SetCharSet(CS_UTF8,	   1);			break;
				case ID_UTF16:		SetCharSet(CS_UTF16LE, 5);			break;
				case ID_UTF16LE:	SetCharSet(CS_UTF16LE, 1);			break;
				case ID_UTF16BE:	SetCharSet(CS_UTF16BE, 1);			break;
				case ID_EBCDIC:		SetCharSet(CS_EBCDIC,  1);			break;
				case ID_HEX:		ToggleHexMode();					break;

				case ID_HELP:		{	HELPCONTEXT SavHelp = HelpContext;

										HelpContext = HelpAll;
										Help();
										HelpContext = SavHelp;
									}									break;
				case ID_WEBPAGE:	ShowUrl("https://www.open-source.tech/WinVi-Minimalist-Text-Hex-Editor/");	break;
				case ID_ABOUT:		About(hWnd);						break;

				case ID_COMMAND:	if (hwndCmd &&
										  !GetWindowTextLength(hwndCmd) &&
										  IsWindowVisible(hwndCmd))
										PostMessage(hwndMain,
													WM_USER+1234, 0,0); break;
				case 910:
				case 911:			StatusContextMenuCommand(COMMAND);  break;

				default:			InterCommCommand(COMMAND);
									PathExpCommand  (COMMAND);
									FileListCommand (COMMAND);
			}
			break;

		case WM_USER+1234:
			CommandDone((BOOL)wPar);
			break;

		case WM_USER+1235:
			ExecInput(wPar, lPar);
			break;

		case WM_USER+1236:
			WatchMessage((PWSTR)lPar);
			break;

		case WM_INITMENUPOPUP:
			if (!HIWORD(lPar)) {
				if (LOWORD(lPar)==4) InterCommMenu(hWnd, (HMENU)wPar);
				else {
					extern HMENU PathExpMenu, hStatusPopupMenu;

					DisableMenuItems((INT)LOWORD(lPar), (HMENU)wPar);
					if (!LOWORD(lPar) && (HMENU)wPar!=PathExpMenu &&
										 (HMENU)wPar!=hPopupMenu  &&
										 (HMENU)wPar!=hStatusPopupMenu)
						InsertFileList((HMENU)wPar);
				}
			}
			break;

		case WM_SYSCOLORCHANGE:
			Ctl3dColorChange();
			SystemColors(TRUE);
			if (wUseSysColors) {
				if (wUseSysColors & 1)
					BackgroundColor	= GetSysColor(COLOR_WINDOW);
				if (wUseSysColors & 2)
					TextColor		= GetSysColor(COLOR_WINDOWTEXT);
				if (wUseSysColors & 4)
					SelBkgndColor	= GetSysColor(COLOR_HIGHLIGHT);
				if (wUseSysColors & 8)
					SelTextColor	= GetSysColor(COLOR_HIGHLIGHTTEXT);
				if (wUseSysColors & 3 || SelectCount)
					InvalidateRect(hWnd, NULL, TRUE);
			}
			break;

		default:
			if (uMsg!=MsWheelRollMsg || !uMsg) {
				LRESULT lr = InterCommWindowProc(hWnd, uMsg, wPar, lPar);
				if (lr != (LRESULT)-1) return (lr);
				return MyDefWindowProc(hWnd, uMsg, wPar, lPar);
			}
			/*FALLTHROUGH*/
		case WM_MOUSEWHEEL:
			if (!Disabled) {
				INT			i;
				static UINT	LastWheelMsg = 0;

				if (uMsg != LastWheelMsg) {
					if (LastWheelMsg /*only accept one type*/) break;
					LastWheelMsg = uMsg;
				}
				if (!SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0,
										  &WheelAmount, 0)) {
					if (ScrollMsg)
						WheelAmount	= (INT)SendMessage(WheelHandler,
													   ScrollMsg, 0, 0);
					else WheelAmount = 3;
				}
				if ((INT)WheelAmount >= CrsrLines)
					Scroll(wPar & 0x80008000 ? SB_PAGEDOWN : SB_PAGEUP, 0);
				else if (wPar & 0x80008000)
					 for (i=WheelAmount; i>0; --i) Scroll(SB_LINEDOWN, 0);
				else for (i=WheelAmount; i>0; --i) Scroll(SB_LINEUP,   0);

				/*allow query of uMsg and wPar for debugging...*/
				{
					#if defined(WIN32)
						#define FMT "Wheel scrolling\n"\
									"uMsg=%s, wParam=0x%08x lParam=0x%08x"
					#else
						#define FMT "Wheel scrolling\n"\
									"uMsg=%s, wParam=0x%04x lParam=0x%08lx"
					#endif
					wsprintf(QueryString, FMT,
							 (LPSTR)(uMsg==WM_MOUSEWHEEL ? "WM_MOUSEWHEEL"
							 							 : "MSWHEEL_ROLLMSG"),
							 wPar, lPar);
					QueryTime = GetTickCount();
				}
			}
	}
	return (0);
}

#define TITLESIZE 269
PCWSTR ArgList, CurrArg;
WCHAR  Title[TITLESIZE+1];
BOOL   RefreshTitle = FALSE;

VOID GenerateTitle(BOOL ApplyToWindow)
{	extern CHAR	WindowTitle[40];
	PCWSTR		src;
	PWSTR		dst			= Title;
	PSTR		fmt			= WindowTitle;
	DWORD		HideFileExt	= FALSE;

	if (CurrFile == NULL) return;	/*may occur in initial startup phase*/
	#if defined(WIN32)
		{	DWORD Size = sizeof(HideFileExt);
			HKEY  hKey;

			if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\"
												"Windows\\CurrentVersion\\"
												"Explorer\\Advanced",
							 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
				RegQueryValueEx(hKey, "HideFileExt", NULL, NULL,
								(LPBYTE)&HideFileExt, &Size);
				RegCloseKey(hKey);
			}
		}
	#endif
	src = CurrFile;
	while (*src == ' ') ++src;
	if (*(src = CurrFile) == '\0') {
		static WCHAR unnamed[40];

		LOADSTRINGW(hInst, 901, unnamed, WSIZE(unnamed));
		src = unnamed;
	}
	while (*fmt != '\0' && dst < Title + sizeof(Title) - 1) {
		if (*fmt == '&') {
			extern BOOL	SuppressAsterisk;
			extern INT  IcWindowID(VOID);
			PCWSTR		s1, s2;

			s1 = src;
			switch (*++fmt) {
				case '&':	/*literal '&'*/
							if (dst < Title + TITLESIZE) *dst++ = '&';
							break;

				case 's':	/*short filename (without path)*/
							if ((s2 = wcsrchr(s1, '\\')) != NULL) s1 = s2 + 1;
							if ((s2 = wcsrchr(s1, '/' )) != NULL) s1 = s2 + 1;
							/*FALLTHROUGH*/
				case 'n':	/*full filename (with path)*/
							if (!HideFileExt || (s2 = wcsrchr(s1, '.')) == NULL)
								s2 = s1 + wcslen(s1);
							while (s1 < s2)
								if (dst < Title + TITLESIZE) *dst++ = *s1++;
							break;

				case 'e':	/*extension if stripped in &s/&n...*/
							if (HideFileExt && (s2 = wcsrchr(s1,'.')) != NULL) {
								while (*s2 != '\0')
									if (dst < Title + TITLESIZE) *dst++ = *s2++;
							}
							break;

				case 'm':	/*modification sign ('*' or none)*/
							if (Unsafe && !SuppressAsterisk)
								if (dst < Title + TITLESIZE) *dst++ = '*';
							break;

				case 'w':	/*window identifier*/
							{	INT	id = IcWindowID();

								if (id && dst < Title + TITLESIZE) *dst++ = id;
								else RefreshTitle = TRUE;
							}
							break;
			}
		} else if (dst < Title + TITLESIZE) *dst++ = *fmt;
		++fmt;
	}
	*dst = '\0';
	if (ApplyToWindow) SetWindowTextW(hwndMain, Title);
}

static DWORD GetVersion32(VOID)
{
	#if defined(WIN32)
		return (0);
	#elif LIBW_AVAILABLE
		extern DWORD WINAPI LoadLibraryEx32W
							   (LPCSTR lpszLibFile, DWORD hFile, DWORD dwFlags);
		extern DWORD WINAPI FreeLibrary32W(DWORD hLibModule);
		extern DWORD WINAPI GetProcAddress32W
							   (DWORD hModule, LPCSTR lpszProc);
		extern DWORD FAR	CallProcEx32W(DWORD, DWORD, DWORD, ...);
		HMODULE				hKernel;
		DWORD				h32Dll = 0, p32GetVersion, Result = 0;

		if ((hKernel = GetModuleHandle("KERNEL"))
				&& GetProcAddress(hKernel, "GetProcAddress32W")
				&& (h32Dll = LoadLibraryEx32W(ExpandSys32("KERNEL32"), NULL, 0))
				&& (p32GetVersion = GetProcAddress32W(h32Dll, "GetVersion")))
			Result = CallProcEx32W(0, 0, p32GetVersion);
		if (h32Dll) FreeLibrary32W(h32Dll);
		return (Result);
	#else
		return (0);
	#endif
}

BOOL PrintOnly=FALSE, ReadOnlyParam=FALSE;

BOOL HandleFlag(PCWSTR Arg, INT Len, INT *pnShow)
{
	switch (Arg[1]) {
		case 'R': /*readonly*/
			if (Len != 2) return (FALSE);
			ReadOnlyParam = TRUE;
			break;
		case 'p': case 'P': /*print file*/
			if (Len==2 || Arg[2]==':') {
				if (Len > 2 && !PrintSetPrinter(Arg+3, Len-3)) {
					ErrorBox(MB_ICONSTOP, 248);
					break;
				}
				*pnShow = SW_SHOWMINNOACTIVE;
				PrintOnly = TRUE;
				break;
			}
			/*FALLTHROUGH*/
		default:
			return (FALSE);
	}
	return (TRUE);
}

HGLOBAL SpareMemory;			/*released if memory problems occur*/

BOOL FreeSpareMemory(VOID)
{
	if (SpareMemory == NULL) return (FALSE);
	GlobalFree(SpareMemory);
	SpareMemory = NULL;
	ErrorBox(MB_ICONEXCLAMATION, 313);
	return (TRUE);
}

BOOL NotepadCompatible(VOID)
{	CHAR ModuleFileName[MAX_PATH];
	PSTR p, Tail;

	GetModuleFileName(NULL, ModuleFileName, sizeof(ModuleFileName));
	Tail = ModuleFileName;
	while ((p = strchr(Tail, '\\')) != NULL || (p = strchr(Tail, '/')) != NULL)
		Tail = p+1;
	return (*Tail=='N' || *Tail=='n');
}

extern DWORD MyGetVersion(VOID)
{
	if (!IsWindows7OrGreater()) {
		MessageBoxW(
			NULL,
			(LPCWSTR)L"Only Windows 7 or greater is supported.",
			(LPCWSTR)L"Fatal Error",
			MB_ICONERROR);
		exit(__LINE__);
	}

	return 0;
}

BOOL InitInstance(INT nShow)
{
	if (!(MyGetVersion() & 0x80000000U)) IsWinNt2kXpEtc = TRUE;
	if ((WinVersion = LOWORD(MyGetVersion())) == MAKEWORD(3,10)) {
		WORD Ver32  = LOWORD(GetVersion32());

		if (Ver32) WinVersion = Ver32;
	}
	WinVersion		= HIBYTE(WinVersion) + (LOBYTE(WinVersion) << 8);
	TextColor		= GetSysColor(COLOR_WINDOWTEXT);
	BackgroundColor	= GetSysColor(COLOR_WINDOW);
	MsWheelRollMsg	= RegisterWindowMessage("MSWHEEL_ROLLMSG");
	WheelHandler	= FindWindow("MouseZ", "Magellan MSWHEEL");
	if (WheelHandler)
		ScrollMsg	= RegisterWindowMessage("MSH_SCROLL_LINES_MSG");
	LoadSettings();
	#if !defined(SW_SHOWDEFAULT)
		#define  SW_SHOWDEFAULT 10
	#endif
	if (Maximized && (nShow==SW_SHOWNORMAL || nShow==SW_SHOWDEFAULT))
		nShow = SW_SHOWMAXIMIZED;
	{	WCHAR *CmdLine;

		CmdLine = GetCommandLineW();
		if (*CmdLine == '"')
			do; while (*++CmdLine != '\0' && *CmdLine != '"');
		else if (*CmdLine != '\0')
			do; while (*++CmdLine != '\0' && *CmdLine != ' ');
		do; while (*CmdLine != '\0' && *++CmdLine == ' ');
		#if 0
		{	CHAR  Buffer[1024];
			WCHAR *p;
			INT   n;

			n = snprintf(Buffer, sizeof(Buffer), "WinVi: CmdLine '");
			p = CmdLine;
			while (n < sizeof(Buffer)-2) {
				if (*p == '\0') break;
				if (*p >= ' ' && *p <= '~')
					Buffer[n++] = *p++;
				else if (n < 990)
					n += snprintf(Buffer + n, sizeof(Buffer) - n,
								  "\\x%.4x", *p++);
				else break;
			}
			Buffer[n++]	= '\'';
			Buffer[n]	= '\0';
			OutputDebugString(Buffer);
		}
		#endif
		ArgList = CmdLine;
	}
	if (ArgList) {
		if (NotepadCompatible()) {
			INT c;

			if (ArgList[0]=='/' && ((c=ArgList[1])=='P' || c=='p')
								&& ((c=ArgList[2])==' ' || c=='\0')) {
				nShow = SW_SHOWMINNOACTIVE;
				PrintOnly = TRUE;
				ArgList += 2;
				if (c) ++ArgList;
			}
			AppendToFileList(ArgList, wcslen(ArgList), 1);
		} else if (ArgList) {
			PCWSTR pw, pwStart;

			while (*ArgList == ' ') ++ArgList;
			pwStart = pw = ArgList;
			for (;;) {
				switch (*pw++) {
					case '"':	do; while (*pw && *pw++ != '"');
								continue;
					case ' ':	if (*pwStart!='-' ||
									 !HandleFlag(pwStart, pw-pwStart-1, &nShow))
									AppendToFileList(pwStart, pw-pwStart-1, 1);
								while (*pw == ' ') ++pw;
								pwStart = pw;
								/*FALLTHROUGH*/
					default:	continue;
					case '\0':	/*break*/;
				}
				break;
			}
			if (*pwStart)
				if (*pwStart!='-' || !HandleFlag(pwStart, pw-pwStart-1, &nShow))
					AppendToFileList(pwStart, pw-pwStart-1, 1);
		}
	}
	nCurrFileListEntry = -1;
	do {
		if ((CurrFile = GetFileListEntry(++nCurrFileListEntry)) == NULL) {
			CurrFile = L"";
			break;
		}
	} while (*CurrFile == '+');
	CurrArg = CurrFile;
	GenerateTitle(FALSE);
	hwndMain = CreateWindowW(ClassName, Title, WS_OVERLAPPEDWINDOW,
							 x, y, Width, Height, NULL, NULL, hInst, NULL);
	if (!hwndMain) {
		MessageBox(NULL, "CreateWindow failed!", NULL, MB_OK | MB_ICONSTOP);
		return (FALSE);
	}
	hwndErrorParent = hwndMain;
	DragAcceptFiles(hwndMain, TRUE);
	GetClientRect(hwndMain, &ClientRect);
	SizeGrip = TRUE;
	InitPaint();
	if (*CurrArg) {
		extern BOOL SuppressPaint;

		SuppressPaint = TRUE;
		FreeAllPages();
		SwitchToMatchingProfile(CurrFile = CurrArg);
		SwitchCursor(SWC_HOURGLASS_ON);
		ShowWindow(hwndMain, nShow);
		if (!Open(CurrFile, 0)) {
			SuppressPaint = FALSE;
			InvalidateText();
		} /*else SuppressPaint has already been reset in Open()*/
	} else {
		New(hwndMain, FALSE);
		ShowWindow(hwndMain, nShow);
	}
	for (;;) {
		PWSTR lp = GetFileListEntry(0);

		SwitchCursor(SWC_HOURGLASS_ON);
		if (!lp || *lp != '+') break;
		wcscpy(CommandBuf, lp);
		RemoveFileListEntry(0);
		CommandBuf[0] = ':';
		CommandExec(CommandBuf);
	}
	SetMenuRadioStyle();
	if (hwndMain) UpdateWindow(hwndMain);
	InterCommInit(hwndMain);
	if (RefreshTitle) {
		RefreshTitle = FALSE;
		GenerateTitle(TRUE);
	}
	SwitchCursor(SWC_HOURGLASS_OFF);
	if (DefaultInsFlag) GotChar('i', 0);
	ShowEditCaret();
	SpareMemory = GlobalAlloc(GMEM_MOVEABLE, 1024);
	{	extern HWND hwndScroll;

		/* Some Windows versions have problems initially displaying
		 * the lower line of the upper scrollbar arrow button.
		 * So a redraw should fix it here...
		 */
		InvalidateRect(hwndScroll, NULL, FALSE);
	}
	StartupPhase = FALSE;
	return (TRUE);
}

BOOL InitApplication(VOID)
{	static WNDCLASSW wcw;
	static WNDCLASS  wc;

	wcw.style		 = wc.style		 = CS_DBLCLKS;
	wcw.lpfnWndProc					 = MainProc;
	wcw.cbClsExtra	 = wc.cbClsExtra = 0;
	wcw.cbWndExtra	 = wc.cbWndExtra = 0;
	wcw.hInstance	 = wc.hInstance	 = hInst;
	wcw.hIcon						 = LoadIcon(hInst, MAKEINTRESOURCE(100));
	wcw.hCursor		 = wc.hCursor	 = NULL;
	wcw.hbrBackground				 = (HBRUSH)(COLOR_WINDOW + 1);
	wcw.lpszMenuName				 = MAKEINTRESW(100);
	wcw.lpszClassName				 = ClassName;
	if (!RegisterClassW(&wcw)) {
		MessageBox(NULL, "RegisterClass failed!", NULL, MB_OK | MB_ICONSTOP);
		return (FALSE);
	}
	wcw.lpszMenuName  = NULL;	wc.lpszMenuName  = NULL;
	wcw.hIcon		  =			wc.hIcon		 = NULL;
	wcw.hbrBackground =			wc.hbrBackground = (HBRUSH)NULL;
	#if defined(OWN_TABCTRL)
		wc.lpfnWndProc   = TabCtrlProc;
		wc.lpszClassName = "WinVi - TabCtrl";
		RegisterClass(&wc);
	#endif
	wc.lpfnWndProc	  = ColorProc;
	wc.lpszClassName  = "WinVi - ColorCtrl";
	RegisterClass(&wc);
	wc.lpfnWndProc	  = SunkenBoxProc;
	wc.lpszClassName  = "WinVi - SunkenBox";
	RegisterClass(&wc);
	wcw.lpfnWndProc	  = TooltipProc;
	wcw.cbWndExtra	  = 4 * sizeof(LONG);
	wcw.lpszClassName = L"WinVi - ToolTip";
	RegisterClassW(&wcw);
	return (TRUE);
}

static BOOL	SpecialCtrlVFlag = TRUE;
BOOL		EscapeInput;

INT MessageLoop(BOOL Wait)
{
	for (;;) {
		static BOOL Expanding	   = FALSE;
		static BOOL KeyboardQueued = FALSE;
		static BOOL SaveMode;
		extern BOOL HexEditTextSide;
		extern BOOL MapInterrupted;
		extern HWND	hwndSearch;
		MSG			Msg;
		BOOL		TryToMap = !EscapeInput;

		if ((Wait || MapInterrupted) && (!RefreshBitmap || KeyboardQueued)) {
			if (KeyboardQueued || MapInterrupted) {
				if (!PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
					if (!KeyboardMapDequeue(&Msg)) {
						MapInterrupted = KeyboardQueued = FALSE;
						continue;
					}
					TryToMap = FALSE;
				}
			} else {
				if (RestoreCaret) ShowEditCaret();
				if (!GetMessageW(&Msg, NULL, 0, 0)) return (Msg.wParam);
			}
		} else if (!PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
			if (RefreshBitmap && hwndMain) {
				INT i;

				for (i=0; i<VisiLines; ++i)
					if (LineInfo[i].Flags & LIF_REFRESH_BKGND) {
						RECT r;

						r.left   = 0;
						r.right  = ClientRect.right;
						r.top    = nToolbarHeight + YTEXT_OFFSET + i*TextHeight;
						r.bottom = r.top + TextHeight;
						LineInfo[i].Flags &= ~LIF_REFRESH_BKGND;
						if (RefreshFlag) InvalidateRect(hwndMain, &r, TRUE);
						else i = VisiLines;	/*skip the remaining part*/
						break;
					}
				if (++i >= VisiLines) {
					RefreshBitmap = FALSE;
					ShowEditCaret();
				}
				continue;
			}
			return (0);
		}
		if (TryToMap && IsKeyboardMapMsg(&Msg)) {
			KeyboardQueued = TRUE;
			continue;
		}

		if (Msg.message == WM_CHAR && EscapeInput) {
			EscapeInput = FALSE;
			if (Msg.hwnd == hwndMain) {
				DeleteSelected(SaveMode == InsertMode ? 42 : 10);
				Mode = InsertMode;
				InsertChar(Msg.wParam);
				Enable();
				Mode = SaveMode;
				NewPosition(&CurrPos);
				if (ReplaceSingle) GotChar('\033', 0);
				ShowEditCaret();
			} else {
				CHAR b[8];

				wsprintf(b,
						 (Msg.wParam>=' '&&Msg.wParam<='~') || Msg.hwnd!=hwndCmd
						 ? "%c" : "\\%%%02x", Msg.wParam & 255);
				SendMessage(Msg.hwnd, EM_REPLACESEL, TRUE, (LPARAM)(LPSTR)b);
			}
			continue;
		}
		if ((Msg.message == WM_KEYDOWN || Msg.message == WM_SYSKEYDOWN)
				&& Msg.hwnd) {
			extern HWND hwndSettings;

			switch (Msg.wParam) {
				static HELPCONTEXT SavedHelpContext;

				case VK_NUMLOCK:
				case VK_CAPITAL:
					GotKeydown(Msg.wParam);
					break;

				case VK_F1:
					if (Msg.hwnd != hwndMain) {
						HWND hw = GetParent(Msg.hwnd);

						if (hw == hwndSearch && HelpContext != HelpSearchDlg) {
							SavedHelpContext = HelpContext;
							HelpContext = HelpSearchDlg;
						} else if (hw == hwndCmd && HelpContext < HelpExCommand)
							HelpContext = HelpExCommand;
					} else if (HelpContext == HelpSearchDlg)
							HelpContext = SavedHelpContext;
					Help();
					break;

				case 'V':
					if (Msg.hwnd == hwndMain
							&& (!HexEditMode || HexEditTextSide)
							&& (Mode == InsertMode || Mode == ReplaceMode)
							&& GetKeyState(VK_CONTROL) < 0
							&& GetKeyState(VK_MENU)    < 0 && !EscapeInput) {
						SaveMode = Mode;
						if (SelectCount) DeleteSelected(8);
						InsertChar('^');
						Position(1, 'h', 18);
						EscapeInput = TRUE;
						Mode = ReplaceMode;
						NewPosition(&CurrPos);
						ShowEditCaret();
						Disable(3);
						continue;
					} else if (Msg.hwnd == hwndCmd
							&& GetKeyState(VK_CONTROL) < 0
							&& GetKeyState(VK_MENU)    < 0) {
						DWORD Sel;

						EscapeInput = TRUE;
						SendMessage(hwndCmd, WM_CHAR, '^', 1);
						Sel = SendMessage(hwndCmd, EM_GETSEL, 0,0);
						#if defined(WIN32)
							SendMessage(hwndCmd, EM_SETSEL, LOWORD(Sel)-1,
															LOWORD(Sel));
						#else
							SendMessage(hwndCmd, EM_SETSEL, 1,
										MAKELPARAM(LOWORD(Sel), LOWORD(Sel)-1));
						#endif
						continue;
					}
					break;

				case VK_TAB:
					if (hwndSearch && GetKeyState(VK_CONTROL) < 0
						&& (!hwndSettings || GetParent(Msg.hwnd)==hwndSearch)) {
							SetActiveWindow(GetActiveWindow()!=hwndSearch
											? hwndSearch
											: hwndSettings ? hwndSettings
														   : hwndMain);
							continue;
					}
					break;

				case VK_CANCEL:
					if (Msg.hwnd == hwndCmd) {
						if (Expanding) {
							Interrupted = TRUE;
							continue;
						}
						PostMessage(hwndMain, WM_USER+1234, 1, 0);
						continue;
					}
					break;
			}
		}
		if (Msg.message == WM_CHAR && !EscapeInput) {
			if (Msg.hwnd == hwndCmd) {
				switch (Msg.wParam) {
					case '\t':
						if (GetKeyState(VK_SHIFT)   >= 0) {
							/* prevent nested ExpandPath calls,
							 * MessageLoop may be called recursively
							 * to check server expansion interrupts
							 */
							if (!Expanding) {
								Expanding = TRUE;
								ExpandPath(hwndCmd, 1);
								Expanding = FALSE;
							}
							continue;
						}
						break;

					case CTRL('c'):
					case '\033':
						PostMessage(hwndMain, WM_USER+1234, 1, 0);
						continue;

					case '\r':
						PostMessage(hwndMain, WM_USER+1234, 0, 0);
						continue;

					case CTRL('v'):
						if (SpecialCtrlVFlag) {
							DWORD Sel;

							EscapeInput = TRUE;
							SendMessage(hwndCmd, WM_CHAR, '^', 1);
							Sel = SendMessage(hwndCmd, EM_GETSEL, 0,0);
							#if defined(WIN32)
								SendMessage(hwndCmd, EM_SETSEL,
											LOWORD(Sel)-1, LOWORD(Sel));
							#else
								SendMessage(hwndCmd, EM_SETSEL, 1,
											MAKELPARAM(LOWORD(Sel),
													   LOWORD(Sel)-1));
							#endif
							continue;
						}
				}
			}
		}
		if (Msg.message == WM_QUIT) {
			#if defined(WIN32)
				Interrupted = CTRL_CLOSE_EVENT+1;
			#else
				Interrupted = TRUE;
			#endif
			PostQuitMessage(Msg.wParam);
			return (0);
		}
		if (hwndSearch && IsDialogMessageW(hwndSearch, &Msg)) continue;
		if (IsSettingsDialogMessage(&Msg)) continue;
		if (Msg.message != WM_KEYDOWN || Msg.wParam != VK_CANCEL) {
			/*prevent mapping of Ctrl+Break to Ctrl+C*/
			TranslateMessage(&Msg);
		}
		DispatchMessageW(&Msg);
	}
}

INT WINAPI WinMain(HINST hCurr, HINST hPrev, LPSTR lpCmd, INT nShow)
{	INT Ret;

	hInst = hCurr;
	//ArgList = lpCmd;
	Language = GetDefaultLanguage(FALSE);
	Ctl3dRegister(hInst);
	Ctl3dAutoSubclass(hInst);
	#if defined(WIN32)
		InitCommonControls();
	#endif
	HelpContext = HelpEditTextmode;
	if ((!hPrev && !InitApplication()) || !InitInstance(nShow)) Ret = 1;
	else if (PrintOnly) {
		HelpContext = HelpPrint;
		PrintImmediate(hwndMain);
		Ret = 0;
	} else Ret = MessageLoop(TRUE);
	ExecInterrupt(CTRL_BREAK_EVENT);
	Ctl3dUnregister(hInst);
	return (Ret);
}
