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
 *     28-Dec-2000	sunken owner-draw dialog control
 *     12-Mar-2001	suppression of non-basic color choice in font settings
 *     30-Sep-2002	disable/enable Apply button according to saveable changes
 *     19-Nov-2002	new tabsheets "Profile", "File type", "Macros" and "Shell"
 *     12-Jan-2003	several changes for handling profiles
 *     20-Oct-2003	WinXp style with EnableThemeDialogTexture
 *     21-Oct-2003	optional tab expansion with forward slash
 *      6-Apr-2004	saving font glyph availability
 *      5-Jan-2005	reload dialog after switching profiles if language changed
 *                	enable Apply button if profile changed
 *                	restore profile after pushing Cancel if profile changed
 *      5-Feb-2005	saving current settings when creating a profile
 *                	corrected cancel data when removing profiles
 *                	bugfix in tab sheet change after error message
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *     31-Aug-2010	DLL load from system directory only
 */

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <lmcons.h>
//#include <uxtheme.h>
#include <string.h>
#include <stdlib.h>
#include "winvi.h"
#include "colors.h"
#include "printh.h"
#include "paint.h"
#include "tabctrl.h"
#include "resource.h"
#include "page.h"
#include "map.h"

#if !defined(ETDT_ENABLETAB)
#define ETDT_ENABLE 2
#define ETDT_USETABTEXTURE 4
#define ETDT_ENABLETAB (ETDT_ENABLE | ETDT_USETABTEXTURE)
#endif

BOOL ProfileCallback(HWND, UINT, WPARAM, LPARAM);
BOOL FiletypeCallback(HWND, UINT, WPARAM, LPARAM);
BOOL ShellCallback (HWND, UINT, WPARAM, LPARAM);
BOOL FontCallback (HWND, UINT, WPARAM, LPARAM);

HWND		   hwndSettings, hwndSubSet;
static DLGPROC SettingsDlgProc, SubSetDlgProc;
CHAR		   QueryString[150];
DWORD		   QueryTime;
static BOOL	   SuppressFocusChange, FocusOnFirstControl;
COLORREF	   *ForegroundColor;
INT			   SuppressEnableApply = 0;
CHAR           CurrProfile[64]	   = "(default)";
CHAR const	   DefaultProfile[]	   = "(default)";
BOOL		   NoGlyphs = FALSE, NoHexGlyphs = FALSE;

struct tagTABSHEET {
	CHAR	*Label;
	BOOL	(*Callback)(HWND,UINT,WPARAM,LPARAM);
	HWND	hDlg;
	INT		Dialog, Flags, FirstControl, LastControl;
} TabSheets[] = {
	{/*Profile*/	NULL,				 ProfileCallback,		  0, 320, 0,
					IDC_NEWPROFILE,		 IDC_AUTOPERMANENT},
	{/*File type*/	NULL,				 FiletypeCallback,		  0, 321, 0,
					IDC_AUTO,			 IDC_OEMCP},
	{/*Display*/	NULL,				 DisplaySettingsCallback, 0, 322, 0,
					IDC_SHOWMATCH,		 IDC_MAXSCROLL},
	{/*Edit*/		NULL,				 EditSettingsCallback,	  0, 323, 0,
					IDC_DEFAULTINSERT,	 IDC_MAGIC},
	{/*Files*/		NULL,				 FileSettingsCallback,	  0, 324, 0,
					IDC_PAGE_THRESHOLD,	 IDC_LOWERCASE},
	{/*Macros*/		NULL,				 MacrosCallback,		  0, 325, 0,
					IDC_MACROLIST,		 IDC_DELMACRO},
	{/*Shell*/		NULL,				 ShellCallback,			  0, 326, 0,
					IDC_SHELL,			 IDC_HELPURL},
	{/*Colors*/		NULL,				 ColorsCallback,		  0, 327, 0,
					IDC_EDIT_BG,		 IDC_BITMAPBROWSE},
	{/*Font*/		NULL,				 FontCallback,			  0, 328, 0,
					IDC_CHANGEFONT_TEXT, IDC_CHANGEFONT_HEX},
	{/*Print*/		NULL,				 PrintCallback,			  0, 329, 0,
					IDC_LEFTBORDER,		 IDC_PRINTFONT},
	{/*Language*/	NULL,				 LanguageCallback,		  0, 330, 0,
					IDC_ENGLISH,		 IDC_AUTOLANG},
	{				NULL,				 NULL,					  0,   0, 0}
};

#define TAB_PROFILE   0
#define TAB_FILETYPE  1
#define TAB_DISPLAY   2
#define TAB_EDIT	  3
#define TAB_FILES	  4
#define TAB_MACROS	  5
#define TAB_SHELL	  6
#define TAB_COLORS	  7
#define TAB_FONT	  8
#define TAB_PRINT	  9
#define TAB_LANGUAGE 10

#define NUM_SHEETS (sizeof(TabSheets) / sizeof(TabSheets[0]) - 1)

INT CurrTab = 0, PrintTab = TAB_PRINT;

LOGFONT			  TextLogFont, HexLogFont, NewTextLogFont, NewHexLogFont;
HFONT			  NewTextFont, NewHexFont;
INT				  NewTextHeight, NewHexHeight;
static CHOOSEFONT cfFont;

typedef struct {
	HINSTANCE hInst;
	INT		  FontTitle;
} CHOOSEFONTPARAM, FAR *LPCHOOSEFONTPARAM;
CHOOSEFONTPARAM ChooseFontParam;

void EnableApply(void)
{	HWND hApply;

	if (hwndSettings && (hApply = GetDlgItem(hwndSettings, ID_APPLY)) != 0)
		EnableWindow(hApply, TRUE);
}

UINT CALLBACK ChooseFontHook(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{
	PARAM_NOT_USED(wPar);
	if (uMsg==WM_INITDIALOG) {
		LPCHOOSEFONT	  lpCf = (LPCHOOSEFONT)lPar;
		LPCHOOSEFONTPARAM cfp  = (LPCHOOSEFONTPARAM)lpCf->lCustData;

		if (cfp->FontTitle) {
			CHAR Title[48];

			LoadString(cfp->hInst, cfp->FontTitle, Title, sizeof(Title));
			SetWindowText(hDlg, Title);
		}
	}
	return (0);
}

BOOL IsBaseColor(COLORREF Color)
{
	switch ((INT)Color & 0xFFFFFF) {
		case 0x000000: case 0xC0C0C0:
		case 0x000080: case 0x0000FF:
		case 0x008000: case 0x00FF00:
		case 0x008080: case 0x00FFFF:
		case 0x800000: case 0xFF0000:
		case 0x800080: case 0xFF00FF:
		case 0x808000: case 0xFFFF00:
		case 0x808080: case 0xFFFFFF: return (TRUE);
	}
	return (FALSE);
}

static BOOL SelectFont(HWND hWnd, PLOGFONT LogFont)
{	HWND OldFocus = GetFocus();
	BOOL Result;

	ChooseFontParam.hInst = hInst;
	cfFont.lStructSize	= sizeof(CHOOSEFONT);
	cfFont.hwndOwner	= hWnd;
	if (ForegroundColor	== NULL)
		/*may have been invalidated in color initdialog handling*/
		ForegroundColor	= Colors + 0x11;
	cfFont.rgbColors	= *ForegroundColor;
	cfFont.lCustData	= (LPARAM)(LPCHOOSEFONTPARAM)&ChooseFontParam;
	cfFont.lpLogFont	= LogFont;
	if (cfFont.lpfnHook	== NULL)
		cfFont.lpfnHook	= (LPCFHOOKPROC)
						  MakeProcInstance((FARPROC)ChooseFontHook, hInst);
	cfFont.Flags		= CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT
										 | CF_ENABLEHOOK;
	if (IsBaseColor(*ForegroundColor)) cfFont.Flags |= CF_EFFECTS;
	cfFont.hInstance	= hInst;
	if (!*LogFont->lfFaceName) {
		lstrcpy(LogFont->lfFaceName, "Courier New");
		LogFont->lfHeight = 12;
	}
	Result = ChooseFont(&cfFont);
	if (OldFocus) SetFocus(OldFocus);	/*for 16-bit*/
	if (Result) {
		extern UINT CurrColor;

		*ForegroundColor = cfFont.rgbColors;
		wUseSysColors &= ~2;
		if (TabSheets[TAB_COLORS].hDlg && TabSheets[TAB_COLORS].Callback
									   && CurrColor==0x11)
			SendMessage(TabSheets[TAB_COLORS].hDlg, WM_COMMAND, IDC_COLORS, 0);
	}
	return (Result);
}

LPARAM ChooseFontCustData(INT FontTitle)
{
	ChooseFontParam.hInst	  = hInst;
	ChooseFontParam.FontTitle = FontTitle+Language;
	return ((LPARAM)(LPCHOOSEFONTPARAM)&ChooseFontParam);
}

static BOOL ShowFont(VOID)
{	extern BOOL IsFixedFont;
	extern INT	HexTextHeight, TextTextHeight;
	BOOL        Repaint = FALSE;

	UpdateWindow(hwndMain);
	if (TextFont && NewTextFont) {
		DeleteObject(TextFont);
		TextFont = NewTextFont;
		NewTextFont = 0;
		if (!HexEditMode) {
			TextHeight = TextTextHeight = NewTextHeight;
			Repaint = TRUE;
		}
		TextLogFont = NewTextLogFont;
	}
	if (HexFont && NewHexFont) {
		DeleteObject(HexFont);
		HexFont = NewHexFont;
		NewHexFont = 0;
		if (HexEditMode) {
			TextHeight = HexTextHeight = NewHexHeight;
			Repaint = TRUE;
		}
		HexLogFont = NewHexLogFont;
	}
	if (ForegroundColor!=NULL && *ForegroundColor!=TextColor) {
		TextColor = *ForegroundColor;
		Repaint = TRUE;
	}
	if (Repaint) {
		AveCharWidth = 0;		/*will be set again in first paint operation*/
		IsFixedFont	 = FALSE;	/*...*/
		AdjustWindowParts(ClientRect.bottom, ClientRect.bottom);
		InvalidateText();
	}
	ShowEditCaret();
	return (TRUE);
}

static VOID DisplayFont(PLOGFONT pLogFont, HFONT Font, HWND hDlg,
						INT NameControl, INT SampleControl)
{	CHAR Buf[64];
	INT  PointSize;
	HDC  hDC = GetDC(hwndMain);

	if (!hDC) return;
	lstrcpyn(Buf, pLogFont->lfFaceName, 44);
	PointSize = -MulDiv(pLogFont->lfHeight, 72, GetDeviceCaps(hDC, LOGPIXELSY));
	ReleaseDC(hwndMain, hDC);
	wsprintf(Buf+lstrlen(Buf), ",  %d pt.", PointSize);
	SetDlgItemText(hDlg, NameControl, Buf);
	SendMessage(GetDlgItem(hDlg, SampleControl), WM_SETFONT, (WPARAM)Font, 0);
}

static INT  nDefId;
static HWND hDefParent;

LRESULT CALLBACK SunkenBoxProc(HWND hWnd, UINT uMsg, WPARAM wPar, LPARAM lPar)
{
	switch(uMsg) {
		case WM_PAINT:
			{	PAINTSTRUCT	Ps;
				HDC			hDC = BeginPaint(hWnd, &Ps);
				RECT		r;
				HPEN		OldPen;

				GetClientRect(hWnd, &r);
				OldPen = SelectObject(hDC, DlgLitePen);
				MoveTo(hDC, 0, r.bottom-1);
				LineTo(hDC, r.right-1, r.bottom-1);
				LineTo(hDC, r.right-1, 0);
				SelectObject(hDC, ShadowPen);
				MoveTo(hDC, 0, r.bottom);
				LineTo(hDC, 0, 0);
				LineTo(hDC, r.right, 0);
				SelectObject(hDC, OldPen);
				EndPaint(hWnd, &Ps);
			}
			break;

		default:
			return (DefWindowProc(hWnd, uMsg, wPar, lPar));
	}
	return (0);
}

VOID CheckDefButton(VOID)
{	INT			d1, d2;
	HWND		h, hParent;
	INT			i;
	static BOOL	IsActive = FALSE;
	static HWND PrevH = 0, hLastDef = 0;

	if (IsActive) return;
	IsActive = TRUE;
	h = GetFocus();
	if (h == PrevH) {
		IsActive = FALSE;
		return;
	}
	if (!h || ((hParent=GetParent(h))!=hwndSubSet && hParent!=hwndSettings)) {
		if (hLastDef)
			SendMessage(hLastDef, BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, 1);
		PrevH = 0;
		IsActive = FALSE;
		return;
	}
	PrevH = h;
	d1 = d2 = 102;
	switch (i = GetDlgCtrlID(h)) {
		case IDOK:
		case IDCANCEL:
		case ID_APPLY:
			nDefId = d1 = i;
			hDefParent = hwndSettings;
			break;

		case IDC_NEWPROFILE:
		case IDC_DELPROFILE:
		case IDC_SETMACRO:
		case IDC_DELMACRO:
		case IDC_CHOOSECOLOR:
		case IDC_BITMAPBROWSE:
		case IDC_CHANGEFONT_TEXT:
		case IDC_CHANGEFONT_HEX:
		case IDC_PRINTFONT:
			nDefId = d2 = i;
			hDefParent = hwndSubSet;
			break;

		default:
			nDefId = d1 = IDOK;
			hDefParent = hwndSettings;
			h = GetDlgItem(hwndSettings, IDOK);
	}
	if (h != hLastDef) {
		if (nDefId == d2) SendMessage(hwndSettings,	DM_SETDEFID, d1, 0);
		SendMessage					 (hwndSubSet,	DM_SETDEFID, d2, 0);
		if (nDefId == d1) SendMessage(hwndSettings,	DM_SETDEFID, d1, 0);
		if (hLastDef)
			SendMessage(hLastDef, BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON,	 1);
		if (nDefId != 102)
			SendMessage(h,		  BM_SETSTYLE, (WPARAM)BS_DEFPUSHBUTTON, 1);
		hLastDef = nDefId==102 ? (HWND)0 : h;
	}
	IsActive = FALSE;
}

static CHAR GetStringField[64];

BOOL CALLBACK GetStringCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{
	switch (uMsg) {
		case WM_INITDIALOG:
			SetDlgItemText(hDlg, IDC_GETSTRINGSTRING, GetStringField);
			SendDlgItemMessage(hDlg, IDC_GETSTRINGSTRING, EM_LIMITTEXT, 63, 0);
			LOADSTRING(hInst, (UINT)lPar,
					   GetStringField, sizeof(GetStringField));
			SetWindowText(hDlg, GetStringField);
			LOADSTRING(hInst, (UINT)(lPar+1),
					   GetStringField, sizeof(GetStringField));
			SetDlgItemText(hDlg, IDC_GETSTRINGTITLE, GetStringField);
			LOADSTRING(hInst, 340, GetStringField, sizeof(GetStringField));
			SetDlgItemText(hDlg, IDCANCEL, GetStringField);
			break;

		case WM_COMMAND:
			if (COMMAND == IDOK) {
				GetDlgItemText(hDlg, IDC_GETSTRINGSTRING,
							   GetStringField, sizeof(GetStringField));
				EndDialog(hDlg, TRUE);
			} else if (COMMAND == IDCANCEL) EndDialog(hDlg, FALSE);
			return (TRUE);
	}
	return (FALSE);
}

BOOL GetString(HWND hParent, INT Strs, LPSTR Prefill, LPSTR Buffer, INT Size)
{	INT			   Result;
	static DLGPROC DlgProc;

	if (!DlgProc)
		 DlgProc = (DLGPROC)MakeProcInstance((FARPROC)GetStringCallback, hInst);
	if (Prefill == NULL) *GetStringField = '\0';
	else lstrcpyn(GetStringField, Prefill, sizeof(GetStringField));
	Result = DialogBoxParam(hInst, MAKEINTRESOURCE(301), hParent, DlgProc,Strs);
	if (Result && Buffer != NULL) {
		if (Size > lstrlen(GetStringField)) {
			lstrcpy(Buffer, GetStringField);
			return (TRUE);
		}
	}
	return (FALSE);
}

VOID EnableProfileControls(HWND hDlg, BOOL Enable)
{
	EnableWindow(GetDlgItem(hDlg, IDC_PROFILEACTIVATE),	  Enable);
	EnableWindow(GetDlgItem(hDlg, IDC_DRIVEPATHUNCTITLE), Enable);
	EnableWindow(GetDlgItem(hDlg, IDC_DRIVEPATHUNC),	  Enable);
	EnableWindow(GetDlgItem(hDlg, IDC_EXTENSIONSTITLE),   Enable);
	EnableWindow(GetDlgItem(hDlg, IDC_EXTENSIONS),		  Enable);
}

BOOL WalkTabSheets(INT Curr, WPARAM wPar, LPARAM lPar)
{	INT i;

	for (i=0; TabSheets[i].Dialog; ++i)
		if (TabSheets[i].hDlg && TabSheets[i].Callback) {
			CurrTab = i;
			DlgResult = FALSE;
			SendMessage(TabSheets[i].hDlg, WM_COMMAND, wPar, lPar);
			if (!DlgResult) {
				if (i != Curr) {
					HWND hctlTab = GetDlgItem(hwndSettings, IDC_SETTINGSTAB);

					ShowWindow(TabSheets[Curr].hDlg, SW_HIDE);
					ShowWindow(TabSheets[i].hDlg, SW_SHOW);
					hwndSubSet = TabSheets[i].hDlg;
					BringWindowToTop(hwndSubSet);
					TabCtrl_SetCurSel(hctlTab, i);
					CheckDefButton();
					HelpContext = HelpSettingsProfile + i;
				}
				return FALSE;
			}
		}
	CurrTab = Curr;
	return TRUE;
}

CHAR DrivePathUNC[260], Extensions[80];
CHAR RestoreProfile[64];
BOOL AutoPermanent;

BOOL ProfileCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	static BOOL NewPermanent;

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			EnableProfileControls(hDlg, lstrcmp(CurrProfile,DefaultProfile)!=0);
			SetDlgItemText(hDlg, IDC_DRIVEPATHUNC,  DrivePathUNC);
			SetDlgItemText(hDlg, IDC_EXTENSIONS,    Extensions);
			CheckDlgButton(hDlg, IDC_AUTOPERMANENT, AutoPermanent);
			NewPermanent = AutoPermanent;
			return (FALSE);

		case WM_COMMAND:
			switch (COMMAND) {
				case IDC_NEWPROFILE:
					{	CHAR NewProfile[64];

						NewProfile[0] = '\0';
						for (;;) {
							if (!GetString(hDlg, 337, NewProfile,
										   NewProfile, sizeof(NewProfile)))
								break;
							if (NewProfile[0] != '\0'
									&& lstrcmp(NewProfile, DefaultProfile) != 0
									&& strpbrk(NewProfile, "/\\") == NULL) {
								WORD Entry;

								if (!SwitchProfile(ProfileAdd, NewProfile)) {
									ErrorBox(MB_ICONSTOP, 345);
									continue;
								}
								SaveAllSettings();
								SaveProfileSettings();
								Entry =(WORD)SendDlgItemMessage
											 (hwndSettings, IDC_PROFILENAME,
											  CB_ADDSTRING, 0,
											  (LPARAM)(LPCSTR)NewProfile);
								SendDlgItemMessage(hwndSettings,IDC_PROFILENAME,
												   CB_SETCURSEL, Entry, 0);
								EnableProfileControls(hDlg, TRUE);
								WalkTabSheets(0, IDC_STORE, 0);
								break;
							}
							ErrorBox(MB_ICONSTOP, 339);
						}
					}
					break;

				case IDC_DELPROFILE:
					if (Confirm(hDlg, lstrcmp(CurrProfile, DefaultProfile)
									  ? 342 : 343, CurrProfile)) {
						WORD Entry;
						BOOL IsRestoreProfile;

						IsRestoreProfile =
							lstrcmp(RestoreProfile, CurrProfile) == 0;
						Entry = (WORD)
							SendDlgItemMessage(hwndSettings, IDC_PROFILENAME,
											   CB_FINDSTRINGEXACT, -1,
											   (LPARAM)(LPCSTR)CurrProfile);
						if (Entry != (WORD)CB_ERR)
							SendDlgItemMessage(hwndSettings, IDC_PROFILENAME,
											   CB_DELETESTRING, Entry, 0);
						SwitchProfile(ProfileDelete, NULL);
						SwitchToMatchingProfile(CurrFile);
						if (IsRestoreProfile)
							lstrcpy(RestoreProfile, CurrProfile);
						SendDlgItemMessage(hwndSettings, IDC_PROFILENAME,
										   CB_SELECTSTRING, (WPARAM)-1,
										   (LPARAM)(LPSTR)CurrProfile);
						ProfileCallback(hDlg, WM_INITDIALOG, 0, 0);
					}
					break;

				case IDC_AUTOPERMANENT:
					NewPermanent = IsDlgButtonChecked(hDlg, IDC_AUTOPERMANENT);
					if (!AutoPermanent)
						CheckRadioButton(hwndSettings, IDC_SAVEPERMANENTLY,
										 IDC_ONETIMESETTING,
										 IDC_ONETIMESETTING - NewPermanent);
					break;

				case IDC_STORE:
				case ID_APPLY:
				case IDOK:
					AutoPermanent = NewPermanent;
					DlgResult = TRUE;
					GetDlgItemText(hDlg, IDC_DRIVEPATHUNC,
								   DrivePathUNC, sizeof(DrivePathUNC));
					GetDlgItemText(hDlg, IDC_EXTENSIONS,
								   Extensions,	 sizeof(Extensions));
					if (COMMAND == IDC_STORE) SaveProfileSettings();
					if (COMMAND != IDOK) break;
					/*FALLTHROUGH*/
				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;
			}
			return (TRUE);
	}
	return (FALSE);
}

BOOL FiletypeCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	extern INT OemCodePage, AnsiCodePage;
	extern INT SelectDefaultNewline, SelectDefaultHexMode, SelectDefaultCharSet;

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			CheckRadioButton(hDlg, IDC_AUTO, IDC_CR,
											 IDC_AUTO+SelectDefaultNewline);
			CheckRadioButton(hDlg, IDC_TEXT, IDC_HEX,
											 IDC_TEXT+SelectDefaultHexMode);
			switch (SelectDefaultCharSet) {
				case CS_AUTOENCODING:
								 CheckRadioButton(hDlg, IDC_AUTOENCODING,
												  IDC_UTF16BE,IDC_AUTOENCODING);
								 break;
				case CS_ANSI:	 CheckRadioButton(hDlg, IDC_AUTOENCODING,
												  IDC_UTF16BE, IDC_ANSI);
								 break;
				case CS_OEM:	 CheckRadioButton(hDlg, IDC_AUTOENCODING,
												  IDC_UTF16BE, IDC_OEM);
								 break;
				case CS_EBCDIC:	 CheckRadioButton(hDlg, IDC_AUTOENCODING,
												  IDC_UTF16BE, IDC_EBCDIC);
								 break;
				case CS_UTF8:	 CheckRadioButton(hDlg, IDC_AUTOENCODING,
												  IDC_UTF16BE, IDC_UTF8);
								 break;
				case CS_UTF16LE: CheckRadioButton(hDlg, IDC_AUTOENCODING,
												  IDC_UTF16BE, IDC_UTF16LE);
								 break;
				case CS_UTF16BE: CheckRadioButton(hDlg, IDC_AUTOENCODING,
												  IDC_UTF16BE, IDC_UTF16BE);
								 break;
			}
			#if defined(WIN32)
			{	CHAR Buf[12];

				if (AnsiCodePage != CP_ACP)
					wsprintf(Buf, "%d", AnsiCodePage);
				else LOADSTRING(hInst, 344, Buf, sizeof(Buf));
				SetDlgItemText(hDlg, IDC_ANSICP, Buf);
				if (OemCodePage != CP_OEMCP)
					wsprintf(Buf, "%d", OemCodePage);
				else LOADSTRING(hInst, 344, Buf, sizeof(Buf));
				SetDlgItemText(hDlg, IDC_OEMCP,  Buf);
			}
			#else
				ShowWindow(GetDlgItem(hDlg, IDC_CODEPAGESTITLE), SW_HIDE);
				ShowWindow(GetDlgItem(hDlg, IDC_ANSICPTITLE),	 SW_HIDE);
				ShowWindow(GetDlgItem(hDlg, IDC_ANSICP),		 SW_HIDE);
				ShowWindow(GetDlgItem(hDlg, IDC_OEMCPTITLE),	 SW_HIDE);
				ShowWindow(GetDlgItem(hDlg, IDC_OEMCP),			 SW_HIDE);
				TabSheets[TAB_FILETYPE].LastControl = IDC_EBCDIC;
			#endif
			return (FALSE);

		case WM_COMMAND:
			switch (COMMAND) {
				extern CHAR	*CrLfAttribute;
				extern INT	DefaultNewLine;

				case IDC_STORE:
				case ID_APPLY:
				case IDOK:
					DlgResult = TRUE;
					if		(IsDlgButtonChecked(hDlg, IDC_AUTO))
						 SelectDefaultNewline = 0;
					else if	(IsDlgButtonChecked(hDlg, IDC_CRLF))
						 SelectDefaultNewline = 1;
					else if	(IsDlgButtonChecked(hDlg, IDC_LF))
						 SelectDefaultNewline = 2;
					else if	(IsDlgButtonChecked(hDlg, IDC_CR))
						 SelectDefaultNewline = 3;
					if		(IsDlgButtonChecked(hDlg, IDC_TEXT))
						 SelectDefaultHexMode = 0;
					else if	(IsDlgButtonChecked(hDlg, IDC_HEX))
						 SelectDefaultHexMode = 1;
					if		(IsDlgButtonChecked(hDlg, IDC_AUTOENCODING))
						 SelectDefaultCharSet = CS_AUTOENCODING;
					else if	(IsDlgButtonChecked(hDlg, IDC_ANSI))
						 SelectDefaultCharSet = CS_ANSI;
					else if	(IsDlgButtonChecked(hDlg, IDC_OEM))
						 SelectDefaultCharSet = CS_OEM;
					else if	(IsDlgButtonChecked(hDlg, IDC_EBCDIC))
						 SelectDefaultCharSet = CS_EBCDIC;
					else if	(IsDlgButtonChecked(hDlg, IDC_UTF8))
						 SelectDefaultCharSet = CS_UTF8;
					else if	(IsDlgButtonChecked(hDlg, IDC_UTF16LE))
						 SelectDefaultCharSet = CS_UTF16LE;
					else if	(IsDlgButtonChecked(hDlg, IDC_UTF16BE))
						 SelectDefaultCharSet = CS_UTF16BE;
					#if defined(WIN32)
					{	CHAR Buf[12];

						GetDlgItemText(hDlg, IDC_ANSICP, Buf, sizeof(Buf));
						AnsiCodePage = strtol(Buf, NULL, 0);
						if (AnsiCodePage == 0) OemCodePage = CP_ACP;
						GetDlgItemText(hDlg, IDC_OEMCP,  Buf, sizeof(Buf));
						OemCodePage  = strtol(Buf, NULL, 0);
						if (OemCodePage  == 0) OemCodePage = CP_OEMCP;
					}
					#endif
					SetDefaultNl();
					if (COMMAND == IDC_STORE) SaveFiletypeSettings();
					if (COMMAND != IDOK) break;
					/*FALLTHROUGH*/
				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;
			}
			return (TRUE);
	}
	return (FALSE);
}

BOOL ShellCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	CHAR		 Buf[256];
	extern LPSTR Shell, ShellCommand, HelpURL;
	extern BOOL  TabExpandWithSlash;

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			SendDlgItemMessage(hDlg, IDC_SHELL,		   EM_LIMITTEXT,
							   sizeof(Buf) - 1, 0);
			SendDlgItemMessage(hDlg, IDC_SHELLCOMMAND, EM_LIMITTEXT,
							   sizeof(Buf) - 1, 0);
			SetDlgItemText	  (hDlg, IDC_SHELL,		   Shell);
			SetDlgItemText	  (hDlg, IDC_SHELLCOMMAND, ShellCommand);
			CheckDlgButton    (hDlg, IDC_TABEXPANDWITHSLASH,
													   TabExpandWithSlash);
			if (HelpURL != NULL)
				SetDlgItemText(hDlg, IDC_HELPURL,	   HelpURL);
			return (FALSE);
			
		case WM_COMMAND:
			switch (COMMAND) {
				case IDC_STORE:
				case ID_APPLY:
				case IDOK:
					GetDlgItemText(hDlg, IDC_SHELL,		   Buf, sizeof(Buf));
					AllocStringA(&Shell, Buf);
					GetDlgItemText(hDlg, IDC_SHELLCOMMAND, Buf, sizeof(Buf));
					AllocStringA(&ShellCommand, Buf);
					TabExpandWithSlash =
						IsDlgButtonChecked(hDlg, IDC_TABEXPANDWITHSLASH);
					GetDlgItemText(hDlg, IDC_HELPURL,	   Buf, sizeof(Buf));
					AllocStringA(&HelpURL, Buf);
					DlgResult = TRUE;
					if (COMMAND == IDC_STORE) SaveShellSettings();
					if (COMMAND != IDOK) break;
					/*FALLTHROUGH*/
				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;
			}
			return (TRUE);

		default:
			return (FALSE);
	}
}

BOOL FontCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	BOOL		Result = TRUE;
	static CHAR Sample[16], Buffer[128];
	static CHAR	*TextSample = "%s  AaBbCcXxYyZz\r\n"
							  "The quick brown fox jumps over a lazy dog.",
				*HexSample  = "%s  AaBbCcXxYyZz\r\n"
							  "14 38 7c ff  |.8|.|\r\n"
							  "25 49 8d 00  |%%I..|\r\n"
							  "36 5a 9e 11  |6Z..|";

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			LOADSTRING(hInst, 257, Sample, sizeof(Sample));
			DisplayFont(&TextLogFont, TextFont, hDlg,
						IDC_FONTNAME_TEXT, IDC_FONTSAMPLE_TEXT);
			DisplayFont(&HexLogFont,  HexFont,  hDlg,
						IDC_FONTNAME_HEX, IDC_FONTSAMPLE_HEX);
			wsprintf(Buffer, TextSample, Sample);
			SetDlgItemText(hDlg, IDC_FONTSAMPLE_TEXT, Buffer);
			wsprintf(Buffer, HexSample, Sample);
			SetDlgItemText(hDlg, IDC_FONTSAMPLE_HEX,  Buffer);
			NewTextLogFont = TextLogFont;
			NewHexLogFont  = HexLogFont;
			if (TabSheets[TAB_COLORS].hDlg && TabSheets[TAB_COLORS].Callback)
				/*color dialog already called...*/
				ForegroundColor = Colors + 0x11;
			else {
				static COLORREF NewForegroundColor;

				NewForegroundColor = TextColor;
				ForegroundColor = &NewForegroundColor;
			}
			cfFont.rgbColors = *ForegroundColor;
			Result = FALSE;
			break;

		case WM_COMMAND:
			switch (COMMAND) {
				case IDC_CHANGEFONT_TEXT:
					ChooseFontParam.FontTitle = 254+Language;
					if (SelectFont(hDlg, &NewTextLogFont)) {
						NoGlyphs = TRUE;	/* glyph bitmap not used anymore */
						NewFont(&NewTextFont, &NewTextLogFont, &NewTextHeight,
								TextGlyphsAvail);
						DisplayFont(&NewTextLogFont, NewTextFont, hDlg,
									IDC_FONTNAME_TEXT, IDC_FONTSAMPLE_TEXT);
						EnableApply();
						InvalidateRect(GetDlgItem(hDlg, IDC_FONTSAMPLE_TEXT),
									   NULL, TRUE);
					}
					break;

				case IDC_CHANGEFONT_HEX:
					ChooseFontParam.FontTitle = 255+Language;
					if (SelectFont(hDlg, &NewHexLogFont)) {
						NoHexGlyphs = TRUE;	/* glyph bitmap not used anymore */
						NewFont(&NewHexFont, &NewHexLogFont, &NewHexHeight,
								HexGlyphsAvail);
						DisplayFont(&NewHexLogFont, NewHexFont, hDlg,
									IDC_FONTNAME_HEX, IDC_FONTSAMPLE_HEX);
						EnableApply();
						InvalidateRect(GetDlgItem(hDlg, IDC_FONTSAMPLE_HEX),
									   NULL, TRUE);
					}
					break;

				case IDC_STORE:
					ShowFont();
					SaveFontSettings();
					DlgResult = TRUE;
					break;

				case ID_APPLY:
					/*FALLTHROUGH*/
				case IDOK:
					ShowFont();
					DlgResult = TRUE;
					if (COMMAND != IDOK) break;
					/*FALLTHROUGH*/
				case IDCANCEL:
					EndDialog(hDlg, 0);
					if (NewTextFont) {
						DeleteObject(NewTextFont);
						NewTextFont = 0;
					}
					if (NewHexFont) {
						DeleteObject(NewHexFont);
						NewHexFont = 0;
					}
					break;
			}
			break;

		default:
			Result = FALSE;
	}
	return (Result);
}

//static HBRUSH TransparentBrush = NULL;

BOOL CALLBACK SubSetCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	BOOL Result = FALSE, GotResult = FALSE;

	if (TabSheets[CurrTab].Callback != NULL) {
		Result = (*TabSheets[CurrTab].Callback)(hDlg, uMsg, wPar, lPar);
		GotResult = TRUE;
	}
	switch (uMsg) {
		case WM_INITDIALOG:
			if (!GotResult) Result = TRUE;
			break;

		case WM_COMMAND:
			if (!FocusOnFirstControl && NOTIFICATION==EN_SETFOCUS)
				switch (COMMAND) {
					case 100:
						SetFocus(GetDlgItem(hwndSettings, IDC_SETTINGSTAB));
						break;
					case 101:
						{	INT nRadio = IsDlgButtonChecked
											(hwndSettings, IDC_SAVEPERMANENTLY)
										 ? IDC_SAVEPERMANENTLY
										 : IDC_ONETIMESETTING;

							SetFocus(GetDlgItem(hwndSettings, nRadio));
							CheckDefButton();
						}
				}
			if (!GotResult) Result = TRUE;
			if (!SuppressEnableApply) {
				switch (NOTIFICATION) {
					case BN_CLICKED:
						switch (COMMAND) {
							case IDC_NEWPROFILE:
							case IDC_DELPROFILE:
							case IDC_SETMACRO:
							case IDC_DELMACRO:
							case IDC_CHANGEFONT_TEXT:
							case IDC_CHANGEFONT_HEX:
							case IDC_BITMAPBROWSE:
							case IDC_CHOOSECOLOR:
							case IDC_PRINTFONT:
								break;
							default:
								EnableApply();
						}
						break;
					case EN_CHANGE:
						EnableApply();
				}
			}
	}
	return (Result);
}

VOID SetTabDlg(HWND hwndSubSet)
{
	#if defined(WIN32)
		typedef HRESULT			 (WINAPI *ENABLETHEMEDIALOGTEXTURE)(HWND,DWORD);
		ENABLETHEMEDIALOGTEXTURE func;
		HINSTANCE				 hDll;
		#if !defined CCM_GETVERSION
		# define CCM_GETVERSION (CCM_FIRST+8)
		#endif

		//if (SendMessage(hwndSubSet, CCM_GETVERSION, 0, 0) < 6) return;
		if ((hDll = LoadLibrary(ExpandSys32("uxtheme.dll"))) != NULL) {
			func = (ENABLETHEMEDIALOGTEXTURE)
					GetProcAddress(hDll, "EnableThemeDialogTexture");
			if (func != NULL) {
				func(hwndSubSet, ETDT_ENABLETAB);
			}
			FreeLibrary(hDll);
		}
	#endif
}

static VOID SwitchTab(HWND hctlTab, INT NewTab)
{
	if (FocusOnFirstControl) SuppressFocusChange = FALSE;
	else SuppressFocusChange = GetFocus() == hctlTab;
	if (NewTab != CurrTab) {
		HWND NewFocus;

		ShowWindow(hwndSubSet, SW_HIDE);
		CurrTab = NewTab;
		hwndSubSet = TabSheets[CurrTab].hDlg;
		if (!hwndSubSet) {
			++SuppressEnableApply;
			hwndSubSet = TabSheets[CurrTab].hDlg
				= CreateDialog(hInst, MAKEINTRES(TabSheets[CurrTab].Dialog),
							   /*hctlTab*/ hwndSettings, SubSetDlgProc);
			SetTabDlg(hwndSubSet);
			--SuppressEnableApply;
		} else {
			/*reset default pushbuttons in Profile, Colors, Print tabsheets...*/
			switch (CurrTab) {
				case TAB_PROFILE:
					SendMessage(hwndSubSet, DM_SETDEFID, 102, 0);
					SendDlgItemMessage(hwndSubSet, IDC_NEWPROFILE,
									   BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, 0);
					SendDlgItemMessage(hwndSubSet, IDC_DELPROFILE,
									   BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, 0);
					break;

				case TAB_COLORS:
					SendMessage(hwndSubSet, DM_SETDEFID, 102, 0);
					SendDlgItemMessage(hwndSubSet, IDC_BITMAPBROWSE,
									   BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, 0);
					SendDlgItemMessage(hwndSubSet, IDC_CHOOSECOLOR,
									   BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, 0);
					break;

				case TAB_PRINT:
					SendMessage(hwndSubSet, DM_SETDEFID, 102, 0);
					SendDlgItemMessage(hwndSubSet, IDC_PRINTFONT,
									   BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, 0);
					break;
			}
		}
		ShowWindow(hwndSubSet, SW_SHOW);
		BringWindowToTop(hwndSubSet);
		if (SuppressFocusChange) NewFocus = hctlTab;
		else {
			INT MaxLoop	= 6;
			INT Ctl		= TabSheets[CurrTab].FirstControl;

			do {
				NewFocus = GetDlgItem(hwndSubSet, Ctl++);
				if (GetWindowLong(NewFocus, GWL_STYLE) & WS_TABSTOP) break;
			} while (--MaxLoop);
		}
		SetFocus(NewFocus);
		SuppressFocusChange = FALSE;
		CheckDefButton();
		HelpContext = HelpSettingsProfile + NewTab;
	}
}

HWND GetCurrentTabSheet(VOID)
{
	return (TabSheets[CurrTab].hDlg);
}

static BOOL SelectProfile(HWND hDlg, CHAR *Buf)
{	UINT OldLang = Language;
	INT  i;

	PARAM_NOT_USED(hDlg);
	SwitchProfile(ProfileSwitchAndLoad, Buf);
	for (i=0; i<NUM_SHEETS; ++i)
		if (TabSheets[i].hDlg != 0)
			(*TabSheets[i].Callback)(TabSheets[i].hDlg, WM_INITDIALOG, 0, 1);
	CheckRadioButton(hwndSettings, IDC_SAVEPERMANENTLY,
					 IDC_ONETIMESETTING,
					 IDC_ONETIMESETTING-AutoPermanent);
	EnableApply();
	return Language != OldLang;
}

RECT DlgRect = {-10000};

BOOL CALLBACK SettingsCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	INT				   i, Ctl, Inc;
	static BOOL		   Restart = FALSE, FocusOnProfileName = FALSE;
	static HFONT	   hFont;
	static HELPCONTEXT SavHelpContext;

	switch (uMsg) {
		case WM_SETFONT:
			hFont = (HFONT)wPar;
			break;

		case WM_INITDIALOG:
			#if defined(OWN_TABCTRL)
				TabCtrlInit(hDlg, hFont);
			#endif
			if (DlgRect.left != -10000)
				SetWindowPos(hDlg, 0, DlgRect.left, DlgRect.top, 0, 0,
							 SWP_NOZORDER | SWP_NOSIZE | SWP_NOREDRAW);
			AddAllProfileNames(GetDlgItem(hDlg, IDC_PROFILENAME));
			SendDlgItemMessage(hDlg, IDC_PROFILENAME,
							   CB_SELECTSTRING, (WPARAM)-1,
							   (LPARAM)(LPSTR)CurrProfile);
			hDefParent = hDlg;
			nDefId = IDOK;
			{	TC_ITEM		TcItem = {TCIF_TEXT};
				HWND		hctlTab = GetDlgItem(hDlg, IDC_SETTINGSTAB);
				static UINT	LabelsLang;

				TabCtrl_SetPadding(hctlTab, 9, 3);
				if (LabelsLang != Language) {
					PSTR		p, pNext;
					static CHAR	Labels[128];

					LOADSTRING(hInst, 912, Labels, sizeof(Labels));
					LabelsLang = Language;
					pNext = Labels;
					i = 0;
					while (*(p = SeparateStringA(&pNext)))
						TabSheets[i++].Label = p;
				}
				i = 0;
				while (TabSheets[i].Dialog) {
					TcItem.pszText = TabSheets[i].Label;
					TcItem.cchTextMax = lstrlen(TcItem.pszText);
					TabCtrl_InsertItem(hctlTab, i, &TcItem);
					++i;
				}
				TabCtrl_SetCurSel(hctlTab, CurrTab);
				hwndSubSet = TabSheets[CurrTab].hDlg =
					CreateDialog(hInst, MAKEINTRES(TabSheets[CurrTab].Dialog),
								 /*hctlTab*/ hDlg, SubSetDlgProc);
				SetTabDlg(hwndSubSet);
				CheckDlgButton(hDlg, AutoPermanent ? IDC_SAVEPERMANENTLY
												   : IDC_ONETIMESETTING, TRUE);
				ShowWindow(hwndSubSet, SW_SHOW);
				BringWindowToTop(hwndSubSet);
			}
			{	HWND hCtl;
				INT  MaxLoop = 6;

				Ctl = TabSheets[CurrTab].FirstControl;
				do {
					hCtl = GetDlgItem(hwndSubSet, Ctl);
					if (GetWindowLong(hCtl, GWL_STYLE) & WS_TABSTOP) break;
					++Ctl;
				} while (--MaxLoop);
				PostMessage(hDlg, WM_COMMAND, 4567, (LPARAM)(UINT)hCtl);
				if (!FocusOnProfileName) {
					hCtl = GetDlgItem(hDlg, ID_APPLY);
					EnableWindow(hCtl, FALSE);
				}
			}
			if (Restart) Restart = FALSE;
			else SavHelpContext = HelpContext;
			if (FocusOnProfileName) {
				FocusOnProfileName = FALSE;
				PostMessage(hDlg, WM_COMMAND, 4567,
							(LPARAM)GetDlgItem(hDlg, IDC_PROFILENAME));
			} else lstrcpy(RestoreProfile, CurrProfile);
			HelpContext = HelpSettingsProfile + CurrTab;
			return (FALSE);

		case WM_COMMAND:
			Ctl = 0;
			switch (COMMAND) {
				case 4567:
					SetFocus((HWND)lPar);
					CheckDefButton();
					break;

				case 4568:
					CheckDefButton();
					return (TRUE);

				case IDC_PROFILENAME:
					if (NOTIFICATION == CBN_SELCHANGE) {
						CHAR Buf[64];

						GetDlgItemText(hDlg, IDC_PROFILENAME, Buf, sizeof(Buf));
						if (*Buf != '\0' && lstrcmp(Buf, CurrProfile) != 0) {
							if (SelectProfile(hDlg, Buf)) {
								/*language changed, reload dialog...*/
								Restart = FocusOnProfileName = TRUE;
								PostMessage(hDlg, WM_CLOSE, 0, 0);
							}
						}
					}
					break;

				case 102:
					/*should not occur, only happens in NT 3.51*/
					QueryTime = GetTickCount();
					wsprintf(QueryString, "[102]");
					wPar = IDOK;
					/*FALLTHROUGH*/
				case ID_APPLY:
				case IDOK:
					{	BOOL Save=IsDlgButtonChecked(hDlg, IDC_SAVEPERMANENTLY);
						UINT OldLang = Language;

						if (Save && !WalkTabSheets(CurrTab, IDC_STORE, 0))
							return FALSE;
						if (!WalkTabSheets(CurrTab, wPar, lPar)) return FALSE;
						lstrcpy(RestoreProfile, CurrProfile);
						if (COMMAND == ID_APPLY) {
							HWND hApply;

							if ((hApply = GetDlgItem(hDlg, ID_APPLY)) != NULL) {
								if (GetFocus() == hApply) {
									SendMessage(hDlg, DM_SETDEFID, IDOK, 0);
									SetFocus(GetDlgItem(hDlg, IDOK));
								}
								EnableWindow(hApply, FALSE);
							}
							if (Language == OldLang) return (TRUE);
							Restart = TRUE;
						}
					}
					/*FALLTHROUGH*/
				case IDCANCEL:
					hwndSubSet = 0;
					{	INT Curr = CurrTab;

						for (i=0; TabSheets[i].Dialog; ++i) {
							if (TabSheets[i].hDlg) {
								CurrTab = i;
								if (COMMAND == IDCANCEL)
									SendMessage(TabSheets[i].hDlg, uMsg, wPar,
																		 lPar);
								DestroyWindow(TabSheets[i].hDlg);
								TabSheets[i].hDlg = 0;
							}
						}
						CurrTab = Curr;
					}
					hFont = 0;
					ForegroundColor = NULL;
					PostMessage(hDlg, WM_CLOSE, 0, 0);
					if (!Restart) HelpContext = SavHelpContext;
					/*FALLTHROUGH*/
				default:
					return (TRUE);
				case IDC_SAVEPERMANENTLY:
					EnableApply();
					return (TRUE);

				case 100:
					Ctl = TabSheets[CurrTab].LastControl;
					Inc = -1;
					break;
				case 101:
					Ctl = TabSheets[CurrTab].FirstControl;
					Inc = 1;
			}
			if (hwndSubSet && Ctl) {
				HWND hCtl;
				INT  MaxLoop = 6;

				do {
					hCtl = GetDlgItem(hwndSubSet, Ctl);
					if (IsWindowEnabled(hCtl)
						&& GetWindowLong(hCtl, GWL_STYLE) & WS_TABSTOP) break;
					Ctl += Inc;
				} while (--MaxLoop);
				if (Ctl == IDC_BITMAP)
					hCtl = GetDlgItem(hwndSubSet, IDC_BITMAPBROWSE);
				/*...special hack: bitmap radio button is not the last control
				 *   on colors sub-dialog, set focus to following pushbutton.
				 */
				SetFocus(hCtl);
				CheckDefButton();
			}
			return (TRUE);

		case WM_NOTIFY:
			{	LPNMHDR	pNmHdr = (LPNMHDR)lPar;
				INT		NewTab;

				if (pNmHdr->code == TCN_SELCHANGE) {
					/*Tab sheet change...*/
					NewTab =
						(INT)SendMessage(pNmHdr->hwndFrom, TCM_GETCURSEL, 0, 0);
					SwitchTab(pNmHdr->hwndFrom, NewTab);
				}
			}
			break;

		case WM_ACTIVATE:
			PostMessage(hDlg, WM_COMMAND, 4568, 0);
			break;

		case WM_CLOSE:
			if (hwndSubSet) SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
			if (hwndSettings) {
				EnableWindow(hwndMain, TRUE);
				SetFocus(hwndMain);
				GetWindowRect(hDlg, &DlgRect);
				DestroyWindow(hDlg);
				hwndSettings = 0;
				if (Restart) PostMessage(hwndMain, WM_COMMAND, ID_SETTINGS, 0);
				else if (*RestoreProfile
						 && lstrcmp(RestoreProfile, CurrProfile) != 0) {
					SwitchProfile(ProfileSwitchAndLoad, RestoreProfile);
				}
			}
			return (TRUE);
	}
	return (FALSE);
}

void Settings(void)
{
	if (!SubSetDlgProc) {
		 SubSetDlgProc =
			(DLGPROC)MakeProcInstance((FARPROC)SubSetCallback, hInst);
		 SettingsDlgProc =
			(DLGPROC)MakeProcInstance((FARPROC)SettingsCallback, hInst);
	}
	hwndSettings = CreateDialog(hInst, MAKEINTRES(IDD_SETTINGS),
								hwndMain, SettingsDlgProc);
	if (!hwndSettings) {
		return;
	}
	ShowWindow(hwndSettings, SW_SHOW);
	EnableWindow(hwndMain, FALSE);
}

BOOL KeyboardSwitchTab(BOOL Backwards)
{	HWND hctlTab = GetDlgItem(hwndSettings,IDC_SETTINGSTAB);
	BOOL FocusOnTab;
	INT  NewTab;

	if (!hctlTab) return (FALSE);
	if (!(FocusOnTab = GetFocus()==hctlTab)) FocusOnFirstControl = TRUE;
	if (Backwards) {
		   if ((NewTab = CurrTab-1) == -1)		   NewTab = NUM_SHEETS - 1;
	} else if ((NewTab = CurrTab+1) == NUM_SHEETS) NewTab = 0;
	TabCtrl_SetCurSel(hctlTab, NewTab);
	SwitchTab(hctlTab, NewTab);
	PostMessage(hwndSettings, WM_COMMAND, FocusOnTab ? 4567 : 4568,
				(LPARAM)(UINT)hctlTab);
	FocusOnFirstControl = FALSE;
	return (TRUE);
}

BOOL IsSettingsDialogMessage(PMSG pMsg)
{	HWND hParent;

	if (!hwndSettings || !hwndSubSet || (hParent = pMsg->hwnd)==0)
		return (FALSE);
	if (hParent!=hwndSettings && hParent!=hwndSubSet) {
		hParent = GetParent(hParent);
		if (hParent!=hwndSettings && hParent!=hwndSubSet) {
			if (hParent != 0) hParent = GetParent(hParent);
			if (hParent!=hwndSettings && hParent!=hwndSubSet) return (FALSE);
		}
	}
	switch (pMsg->message) {
		case WM_KEYDOWN:
			{	WPARAM wPar = pMsg->wParam;

				if (GetKeyState(VK_CONTROL) < 0
					&& (wPar==VK_PRIOR || wPar==VK_NEXT)
					&& KeyboardSwitchTab(wPar == VK_PRIOR))
						return (TRUE);
			}
			/*FALLTHROUGH*/
		case WM_CHAR:
			{	WPARAM		 wPar = pMsg->wParam;
				extern char  QueryString[150];
				extern DWORD QueryTime;

				if (wPar == VK_TAB) {
					if (GetKeyState(VK_CONTROL)<0)
						if (KeyboardSwitchTab(GetKeyState(VK_SHIFT) < 0))
							return (TRUE);
					PostMessage(hwndSettings, WM_COMMAND, 4568, 0);
				}
				if (wPar != VK_RETURN && wPar != VK_ESCAPE) break;
				QueryTime = GetTickCount();
				if (wPar == VK_RETURN) {
					hParent = hwndSettings;
					wPar = LOWORD(SendMessage(hParent, DM_GETDEFID, 0, 0));
					if (wPar == 102) {
						hParent = hwndSubSet;
						wPar = LOWORD(SendMessage(hParent, DM_GETDEFID, 0, 0));
						wsprintf(QueryString, "[4] message=0x%04x, wPar=0x%x",
								 pMsg->message, wPar);
					} else wsprintf(QueryString,
									"[5] message=0x%04x, wPar=%d",
									pMsg->message, wPar);
					switch (wPar) {
						case IDC_NEWPROFILE:
						case IDC_DELPROFILE:
						case IDC_SETMACRO:
						case IDC_DELMACRO:
						case IDC_CHOOSECOLOR:
						case IDC_BITMAPBROWSE:
						case IDC_CHANGEFONT_TEXT:
						case IDC_CHANGEFONT_HEX:
						case IDC_PRINTFONT:
							hParent = hwndSubSet;
							break;
						default:
							hParent	= hDefParent;
							wPar	= nDefId;
					}
				} else {
					hParent = hwndSettings;
					wPar = IDCANCEL;
					wsprintf(QueryString,
							 "[3] message=0x%04x, wPar=0x%02x, WM_COMMAND, "
							 "wPar=%d", pMsg->message, pMsg->wParam, wPar);
				}
				PostMessage(hParent, WM_COMMAND, wPar, 0);
			}
			return (TRUE);

		case WM_SYSCHAR:
			{	static CHAR Accelerators[8];
				static UINT AccelLang;

				if (AccelLang != Language) {
					LOADSTRING(hInst, 913, Accelerators, sizeof(Accelerators));
					AccelLang = Language;
				}
				if (strchr(Accelerators,
						  (INT)(DWORD)AnsiUpper((LPSTR)(INT)(BYTE)pMsg->wParam))
						  != NULL) {
					if (hParent==hwndSubSet) {
						hParent = hwndSettings;
						pMsg->hwnd = GetDlgItem(hwndSettings, IDOK);
					}
				} else if (hParent==hwndSettings) {
					hParent = hwndSubSet;
					pMsg->hwnd =
						GetDlgItem(hwndSubSet, TabSheets[CurrTab].FirstControl);
				}
			}
			break;

		case WM_KEYUP:
		case WM_LBUTTONDOWN:
		case WM_SETFOCUS:
		case WM_KILLFOCUS:
			PostMessage(hwndSettings, WM_COMMAND, 4568, 0);
	}
	return (IsDialogMessageW(hParent, pMsg));
}
