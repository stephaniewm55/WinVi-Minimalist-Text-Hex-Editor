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
 *     15-Aug-2002	check of running shell when changing to another file
 *      6-Oct-2002	settings for new global variable TabExpandDomains (Win32)
 *     27-Nov-2002	multiple profiles, automatic profile selection
 *     11-Jan-2003	mappings/abbreviations, configurable permanent settings
 *     21-Oct-2003	optional tab expansion with forward slash
 *      6-Apr-2004	saving and restoring font glyph availability
 *      6-Jan-2005	setting system default colors before loading profile
 *     19-Apr-2005	hexmode and charset changed in SwitchMatchingProfile
 *                	even if profile does not change
 *      7-Mar-2006	read caret shapes from registry
 *     21-Jul-2007	new registry entry for window title
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 */

#include <windows.h>
#include <commdlg.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <direct.h>
#include <wchar.h>
#if !defined(__LCC__)
# include <dos.h>
#endif
#include <stdio.h>
#include "winvi.h"
#include "paint.h"
#include "colors.h"
#include "page.h"
#include "map.h"

extern PCSTR	   ApplName;
extern RECT		   WholeWindow;
extern INT		   x, y, Width, Height;
extern BOOL		   Maximized, RefreshFlag, AutoPermanent;
extern INT		   OemCodePage, AnsiCodePage;
extern CHAR		   CurrProfile[64];
extern CHAR const  DefaultProfile[];
extern PMAP		   FirstMapping;
extern COLORREF	   CustomColors[16];
extern signed char CaretProps[20];
extern BOOL		   NoGlyphs, NoHexGlyphs;

CHAR			   WindowTitle[40];
BOOL			   LcaseFlag;
BOOL			   UseRegistry = TRUE;

static LPCSTR	   ProfileSection		= "Ramo's WinVi";
static LPCSTR	   pfDrivePathUNC		= "ActivateDrivePathUNC";
static LPCSTR	   pfExtensions			= "ActivateExtensions";
static LPCSTR	   pfLanguage			= "Language";
static LPCSTR	   pfFontFace	 		= "FontName";
static LPCSTR	   pfFontSize	 		= "FontSize";
static LPCSTR	   pfFontAttr	 		= "FontAttributes";
static LPCSTR	   pfFontCharSet		= "FontCharSet";
static LPCSTR	   pfFontGlyphsAvail	= "FontGlyphsAvailable";
static LPCSTR	   pfHexFontFace 		= "HexFontName";
static LPCSTR	   pfHexFontSize 		= "HexFontSize";
static LPCSTR	   pfHexFontAttr		= "HexFontAttributes";
static LPCSTR	   pfHexFontCharSet		= "HexFontCharSet";
static LPCSTR	   pfHexFontGlyphsAvail	= "HexFontGlyphsAvailable";
static LPCSTR	   pfXPos 				= "X-Position";
static LPCSTR	   pfYPos 				= "Y-Position";
static LPCSTR	   pfWidth				= "Width";
static LPCSTR	   pfHeight				= "Height";
static LPCSTR	   pfMaximized			= "Maximized";
static LPCSTR	   pfCaretProps			= "CaretProperties";
static LPCSTR	   pfNewline			= "Newline";
static LPCSTR	   pfHexMode			= "HexMode";
static LPCSTR	   pfCharSet			= "CharacterSet";
static LPCSTR	   pfFgColor			= "TextColor";
static LPCSTR	   pfBkColor			= "BackgroundColor";
static LPCSTR	   pfSelFgColor			= "SelectedText";
static LPCSTR	   pfSelBkColor			= "SelectedBackground";
static LPCSTR	   pfShmFgColor			= "ShowMatchText";
static LPCSTR	   pfShmBkColor			= "ShowMatchBackground";
static LPCSTR	   pfEolColor			= "EolColor";
static LPCSTR	   pfCustomColors		= "CustomColors";
static LPCSTR	   pfTerse				= "Terse";
static LPCSTR	   pfLcaseFnames		= "LowerCaseFnames";
static LPCSTR	   pfReadOnly			= "ReadOnly";
static LPCSTR	   pfAutoRestore		= "AutoRestore";
static LPCSTR	   pfAutoIndent			= "AutoIndent";
static LPCSTR	   pfAutoWrite			= "AutoWrite";
static LPCSTR	   pfShowMatch			= "ShowMatch";
static LPCSTR	   pfWrapScan			= "WrapScan";
static LPCSTR	   pfWrapPos			= "WrapPos";
static LPCSTR	   pfIgnoreCase			= "IgnoreCase";
static LPCSTR	   pfMagic				= "Magic";
static LPCSTR	   pfQuitIfMore			= "QuitIfMoreFiles";
static LPCSTR	   pfTabStop			= "TabStop";
static LPCSTR	   pfUndoLimit			= "UndoLimit";
static LPCSTR	   pfShiftWidth			= "ShiftWidth";
static LPCSTR	   pfDefaultInsert		= "DefaultInsert";
static LPCSTR	   pfLineNumber			= "LineNumber";
static LPCSTR	   pfHexLineNumber		= "HexModeLineNumber";
static LPCSTR	   pfTags				= "Tags";
static LPCSTR	   pfTmpDirectory		= "TmpDirectory";
static LPCSTR	   pfPageThreshold		= "PageThreshold";
static LPCSTR	   pfBitmap				= "Bitmap";
static LPCSTR	   pfRefreshBkgnd		= "RefreshBackground";
static LPCSTR	   pfFullRowExtend		= "FullRowExtend";
static LPCSTR	   pfBreakLine			= "BreakLine";
static LPCSTR	   pfSearchBoxX			= "SearchBox-X";
static LPCSTR	   pfSearchBoxY			= "SearchBox-Y";
static LPCSTR	   pfOemCodePage		= "OemCodePage";
static LPCSTR	   pfAnsiCodePage		= "AnsiCodePage";
static LPCSTR	   pfRecentFiles		= "RecentFiles";
static LPCSTR	   pfRecentFile			= "RecentFile%d";
static LPCSTR	   pfRecentDir			= "RecentDir%d";
static LPCSTR	   pfMaxScroll			= "MaxScroll";
static LPCSTR	   pfShell				= "Shell";
static LPCSTR	   pfShellCommand		= "ShellCommand";
static LPCSTR	   pfTabExpWithSlash	= "TabExpandWithSlash";
static LPCSTR	   pfHelpUrl			= "HelpURL";
static LPCSTR	   pfMapping			= "MapAbbrev%d";
static LPCSTR	   pfMappingName		= "DisplayName";
static LPCSTR	   pfMappingInput		= "Input";
static LPCSTR	   pfMappingReplace		= "Replace";
static LPCSTR	   pfMappingFlags		= "Flags";
static LPCSTR	   pfPrintBorderLeft	= "PrintBorderLeft";
static LPCSTR	   pfPrintBorderTop		= "PrintBorderTop";
static LPCSTR	   pfPrintBorderRight	= "PrintBorderRight";
static LPCSTR	   pfPrintBorderBottom	= "PrintBorderBottom";
static LPCSTR	   pfPrintNumbers		= "PrintNumbers";
static LPCSTR	   pfPrintFontFace		= "PrintFontFace";
static LPCSTR	   pfPrintFontAttr		= "PrintFontAttributes";
static LPCSTR	   pfPrintFontCharSet	= "PrintFontCharSet";
static LPCSTR	   pfPrintFontSize1		= "PrintFontSize1";
static LPCSTR	   pfPrintFontSize2		= "PrintFontSize2";
static LPCSTR	   pfPrintFrame			= "PrintFrame";
static LPCSTR	   pfPrintFrame2		= "PrintFrame2";
static LPCSTR	   pfPrintFrame3		= "PrintFrame3";
static LPCSTR	   pfPrintHeader		= "PrintHeader";
static LPCSTR	   pfPrintFooter		= "PrintFooter";
static LPCSTR	   pfPrintOrientation	= "PrintOrientation";
static LPCSTR	   pfoPortrait			= "Portrait";
static LPCSTR	   pfoLandscape			= "Landscape";
static LPCSTR	   pfoDoubleColumn		= "DoubleColumn";
static LPCSTR	   pfAutoPermanent		= "PermanentSettings";
static LPCSTR	   pfWindowTitle		= "WindowTitle";
#if defined(WIN32)
	static LPCSTR  pfTabExpandDomains	= "TabExpandDomains";
#endif
static PCWSTR	   pfwDrivePathUNC		= L"ActivateDrivePathUNC";
static PCWSTR	   pfwExtensions		= L"ActivateExtensions";

static CHAR		   KeyPrefix[]			= "Software\\Ramo\\WinVi";
static HKEY		   hKey					= INVALID_HANDLE_VALUE;
static CHAR		   ProfileBuffer[76];

BOOL SwitchProfile(PROFILEOP Operation, PSTR Profile)
{	BOOL Result = TRUE;

	#if defined(WIN32)
		switch (Operation) {
			case ProfileDelete:
				{	HKEY hKey2;

					if (RegOpenKeyEx(HKEY_CURRENT_USER, KeyPrefix, 0,
							KEY_CREATE_SUB_KEY, &hKey2) == ERROR_SUCCESS) {
						LONG ErrNo;
						INT  i;

						i = 0;
						do wsprintf(ProfileBuffer, "%s\\MapAbbrev%d",
									CurrProfile, ++i);
						while (RegDeleteKey(hKey2, ProfileBuffer)
							   == ERROR_SUCCESS);
						ErrNo = RegDeleteKey(hKey2, CurrProfile);
						RegCloseKey(hKey2);
						if (ErrNo != ERROR_SUCCESS) {
							ErrorBox(MB_ICONSTOP, 115, CurrProfile, ErrNo);
							Result = FALSE;
						}
					}
				}
				lstrcpy(CurrProfile, DefaultProfile);
				break;

			case ProfileSaveAndClear:
				lstrcpy(Profile, CurrProfile);
				*CurrProfile = '\0';
				break;

			case ProfileRestore:
				lstrcpy(CurrProfile, Profile);
				break;

			case ProfileAdd:
				{	HKEY hKey2;

					wsprintf(ProfileBuffer, "%s\\%s", KeyPrefix, Profile);
					if (RegOpenKeyEx(HKEY_CURRENT_USER, ProfileBuffer, 0,
							KEY_QUERY_VALUE, &hKey2) == ERROR_SUCCESS) {
						RegCloseKey(hKey2);
						return(FALSE);
					}
				}
				/*FALLTHROUGH*/
			case ProfileSwitchAndLoad:
				UseRegistry = TRUE;
				if (Profile != CurrProfile) lstrcpy(CurrProfile, Profile);
				break;
		}
		if (hKey != INVALID_HANDLE_VALUE) {
			RegCloseKey(hKey);
			hKey = INVALID_HANDLE_VALUE;
		}
		if (Operation == ProfileSwitchAndLoad) LoadSettings();
	#endif
	return (Result);
}

static INT MatchDrivePathUNC(PWSTR MatchAgainst, PCWSTR Filename)
{	WCHAR Name[300];
	PWSTR p;
	INT	  Result = -1;

	if (Filename == NULL) Filename = L"";
	if (*Filename=='\0' || (Filename[1]!=':' && wcsspn(Filename, L"\\/")==0)) {
		/*match working directory and relative filename...*/
		if (!GetCurrentDirectoryW(WSIZE(Name) - 1, Name)) *Name = '\0';
		p = Name + wcslen(Name);
		if (*Filename != '\0' && p > Name && p[-1] != '\\' && p[-1] != '/')
			*p++ = '\\';
		wcsncpy(p, Filename, WSIZE(Name) - (p-Name));
	} else if (Filename[1] == ':' && wcsspn(Filename + 2, L"\\/") == 0) {
		/*match working directory on another drive and filename*/
		INT olddrive = _getdrive();

		_chdrive(*Filename & 31);
		if (!GetCurrentDirectoryW(WSIZE(Name), Name)) *Name = '\0';
		_chdrive(olddrive);
		p = Name + wcslen(Name);
		if (*Filename != '\0' && p > Name && p[-1] != '\\' && p[-1] != '/')
			*p++ = '\\';
		wcsncpy(p, Filename + 2, WSIZE(Name) - (p-Name));
	} else if (wcsspn(Filename, L"\\/") == 1) {
		/*match drive or share of working directory and filename...*/
		p = Name;
		if (!GetCurrentDirectoryW(WSIZE(Name), Name)) *Name = '\0';
		if (Name[0] != '\0' && Name[1] == ':') p = Name + 2;
		else {
			INT  Slashes = 4;

			for (;;) {
				if (*p == '\0') break;
				if ((*p == '\\' || *p == '/') && --Slashes == 0) break;
				++p;
			}
			if (Slashes > 1) return (-1);
		}
		wcsncpy(p, Filename, WSIZE(Name) - (p-Name));
	} else {
		/*match filename only...*/
		wcsncpy(Name, Filename, WSIZE(Name));
	}
	p = Name;
	while ((p = wcsstr(p, L"..")) != NULL && p > Name) {
		if (	   (p[-1] == '\\' || p[-1] == '/')
				&& (p[2]  == '\\' || p[2]  == '/' || p[2] == '\0')) {
			PWSTR p2 = p-1;

			do; while (--p2 > Name && *p2 != '\\' && *p2 != '/');
			memmove(p2, p + 2, 2*wcslen(p + 1));
			p = p2;
		} else ++p;
	}
#if 0
	return PatternMatch(Name, MatchAgainst);
#else
	p = MatchAgainst;
	for (;;) {
		PWSTR p2, p3 = Name;
		BOOL  Failed = FALSE;

		if ((p2 = wcspbrk(p, L",;")) == NULL) p2 = p + wcslen(p);
		if ((*p == '\\' || *p == '/') && p[1] != '\\' && p[1] != '/') {
			/*no drive or share, match against path only...*/
			if (p3[0] != '\0' && p3[1] == ':') p3 += 2;
			else if ((*p3=='\\' || *p3=='/') && (p3[1]=='\\' || p3[1]=='/')) {
				++p3;
				do; while (*++p3 != '\0' && *p3   != '\\' && *p3 != '/');
				do;	while (*p3   != '\0' && *++p3 != '\\' && *p3 != '/');
			}
		}
		while (p < p2) {
			switch (*p) {
				case '\\':
				case '/':
					if (*p3 == '\\' || *p3 == '/') break;
					if (*p3 == '\0' && p+1 == p2) --p3;
					else Failed = TRUE;
					break;

				default:
					if ((INT)(DWORD)AnsiUpper((LPSTR)(INT)(BYTE)*p) !=
						(INT)(DWORD)AnsiUpper((LPSTR)(INT)(BYTE)*p3))
							Failed = TRUE;
			}
			if (Failed) break;
			++p;
			++p3;
		}
		if (*p3 != '\0' && *p3     != '\\' && *p3     != '/' &&
			 p3 > Name  &&  p3[-1] != '\\' &&  p3[-1] != '/') Failed = TRUE;
		if (!Failed && p3-Name > Result) Result = p3-Name;
		if (*p2 == '\0') break;
		p = p2 + 1;
	}
	return (Result);
#endif
}

static BOOL MatchExtension(PCWSTR MatchAgainst, PCWSTR Filename)
{	PCWSTR p = MatchAgainst, p2;
	UINT  n;

	if (Filename == NULL) return (FALSE);
	for (;;) {
		if ((p2 = wcspbrk(p, L",;")) == NULL) p2 = p + wcslen(p);
		n = p2 - p;
		if (n > 0 && wcslen(Filename) >= n
				  && _wcsnicmp(p, Filename + wcslen(Filename) - n, n) == 0)
			return (TRUE);
		if (*p2 == '\0') return (FALSE);
		p = p2 + 1;
	}
}

INT SelectDefaultNewline = 0;		/*0=autodetect, 1=CR/LF, 2=LF, 3=CR*/
INT SelectDefaultHexMode = 0;		/*0=text, 1=hex*/
INT SelectDefaultCharSet = CS_AUTOENCODING;

VOID SwitchToMatchingProfile(PCWSTR Filename)
{	HKEY hKey2, hKey3;
	INT  i = 0, n;
	INT  MaxMatchDrivePathUNC = 0;
	BOOL MatchedExtension = FALSE;
	CHAR NewProfile[64];

	*NewProfile = '\0';
	#if defined(WIN32)
	if (RegOpenKeyEx(HKEY_CURRENT_USER, KeyPrefix, 0,
					 KEY_ENUMERATE_SUB_KEYS, &hKey2) == ERROR_SUCCESS) {
		for (;;) {
			DWORD	 Size1 = sizeof(ProfileBuffer);
			FILETIME Time;
			CHAR	 KeyName[sizeof(KeyPrefix) + 64];
			DWORD	 dwType;
			WCHAR	 Buf[260];
			DWORD	 Size2 = sizeof(Buf);

			if (RegEnumKeyEx(hKey2, i++, ProfileBuffer, &Size1, NULL,
							 NULL, NULL, &Time) != ERROR_SUCCESS) break;
			if (lstrcmp(ProfileBuffer, DefaultProfile) == 0) continue;
			lstrcpy(KeyName, KeyPrefix);
			lstrcat(KeyName, "\\");
			lstrcat(KeyName, ProfileBuffer);
			if (RegOpenKeyEx(HKEY_CURRENT_USER, KeyName, 0, KEY_QUERY_VALUE,
							 &hKey3) != ERROR_SUCCESS) continue;
			if (RegQueryValueExW(hKey3, pfwDrivePathUNC, NULL, &dwType,
								 (LPBYTE)Buf, &Size2) != ERROR_SUCCESS)
				*Buf = '\0';
			if (((n = *Buf) == 0 || (n = MatchDrivePathUNC(Buf, Filename)) != 0)
					&& n >= MaxMatchDrivePathUNC) {
				BOOL b;

				Size2 = sizeof(Buf);
				if (RegQueryValueExW(hKey3, pfwExtensions, NULL, &dwType,
									 (LPBYTE)Buf, &Size2) != ERROR_SUCCESS)
					*Buf = '\0';
				if ((!(b = *Buf!='\0')
							|| (b = MatchExtension(Buf, Filename)) != 0)
						&& (b || !MatchedExtension)) {
					if (n >  MaxMatchDrivePathUNC ||
					   (n == MaxMatchDrivePathUNC && b && !MatchedExtension)) {
							MaxMatchDrivePathUNC = n;
							MatchedExtension	 = b;
							lstrcpy(NewProfile, ProfileBuffer);
					}
				}
			}
			RegCloseKey(hKey3);
		}
		RegCloseKey(hKey2);
	}
	if (hKey != INVALID_HANDLE_VALUE) {
		RegCloseKey(hKey);
		hKey = INVALID_HANDLE_VALUE;
	}
	#endif
	if (*NewProfile == '\0') lstrcpy(NewProfile, DefaultProfile);
	if (lstrcmp(CurrProfile, NewProfile) != 0) {
		lstrcpy(CurrProfile, NewProfile);
		LoadSettings();
	} else {
		INT	CharSet;

		/*choose hex mode and character set*/
		if (HexEditMode != SelectDefaultHexMode) ToggleHexMode();
		if (SelectDefaultCharSet == CS_AUTOENCODING) {
			if (UtfEncoding) {
				if (UtfEncoding == 8) CharSet = CS_UTF8;
				else CharSet = UtfLsbFirst ? CS_UTF16LE : CS_UTF16BE;
			} else CharSet = CS_ANSI;
		} else CharSet = SelectDefaultCharSet;
		SetCharSet(CharSet, 0);
	}
}

VOID AddAllProfileNames(HWND hwndCombobox)
{
	SendMessage(hwndCombobox, CB_ADDSTRING, 0, (LPARAM)DefaultProfile);
	SendMessage(hwndCombobox, CB_SETCURSEL, 0, 0);
	#if defined(WIN32)
	{	HKEY	 hKey2;
		FILETIME Time;
		INT		 i = 0;

		if (RegOpenKeyEx(HKEY_CURRENT_USER, KeyPrefix, 0,
						 KEY_ENUMERATE_SUB_KEYS, &hKey2) == ERROR_SUCCESS) {
			for (;;) {
				DWORD Size = sizeof(ProfileBuffer);

				if (RegEnumKeyEx(hKey2, i++, ProfileBuffer, &Size, NULL,
								 NULL, NULL, &Time) != ERROR_SUCCESS)
					break;
				if (lstrcmp(ProfileBuffer, DefaultProfile) != 0)
					SendMessage(hwndCombobox, CB_ADDSTRING,
								0, (LPARAM)ProfileBuffer);
			}
			RegCloseKey(hKey2);
		}
	}
	#endif
}

CHAR Subkey[24];

static INT MyGetProfileString(LPCSTR Key, LPCSTR Default, LPSTR Buf, INT Size)
{
	#if defined(WIN32)
		if (UseRegistry) {
			if (hKey == INVALID_HANDLE_VALUE) {
				CHAR KeyName[sizeof(KeyPrefix) + 90];

				lstrcpy(KeyName, KeyPrefix);
				if (*CurrProfile != '\0') {
					lstrcat(KeyName, "\\");
					lstrcat(KeyName, CurrProfile);
				}
				if (*Subkey != '\0') {
					lstrcat(KeyName, "\\");
					lstrcat(KeyName, Subkey);
				}
				if (RegOpenKeyEx(HKEY_CURRENT_USER, KeyName, 0,
								 KEY_QUERY_VALUE | KEY_SET_VALUE,
								 &hKey) != ERROR_SUCCESS) {
					hKey = INVALID_HANDLE_VALUE;
					if (*CurrProfile == '\0') UseRegistry = FALSE;
				}
			}
			if (UseRegistry /*may have changed*/) {
				DWORD dwSize = Size;
				DWORD dwType;
				LONG  dwValue;

				if (hKey != INVALID_HANDLE_VALUE &&
						RegQueryValueEx(hKey, Key, NULL, &dwType, Buf, &dwSize)
						== ERROR_SUCCESS) {
					if (dwType == REG_DWORD && dwSize == sizeof(LONG)) {
						memcpy(&dwValue, Buf, sizeof(LONG));
						snprintf(Buf, Size, "%ld", dwValue);
					}
				} else lstrcpyn(Buf, Default, Size);
				return (lstrlen(Buf));
			}
		}
	#endif
	{	INT Result;

		Result = GetProfileString(ProfileSection, Key, Default, Buf, Size);
		#if defined(WIN32)
			if (!UseRegistry && lstrcmp(Buf, Default) != 0) {
				/*first use after switch to registry, save key to registry...*/
				if (hKey == INVALID_HANDLE_VALUE) {
					CHAR  KeyName[sizeof(KeyPrefix) + 64];
					DWORD Disposition;

					lstrcpy(KeyName, KeyPrefix);
					if (*CurrProfile != '\0') {
						lstrcat(KeyName, "\\");
						lstrcat(KeyName, CurrProfile);
					}
					if (RegCreateKeyEx(HKEY_CURRENT_USER, KeyName, 0, NULL,
									   REG_OPTION_NON_VOLATILE,
									   KEY_QUERY_VALUE | KEY_SET_VALUE, NULL,
									   &hKey, &Disposition) != ERROR_SUCCESS)
						hKey = INVALID_HANDLE_VALUE;
				}
				if (hKey != INVALID_HANDLE_VALUE)
					RegSetValueEx(hKey, Key, 0, REG_SZ, Buf, lstrlen(Buf) + 1);
			}
		#endif
		return (Result);
	}
}

static INT MyGetProfileStringW(LPCSTR Key, PCWSTR Default, PWSTR Buf, INT Size)
{
	if (UseRegistry) {
		if (hKey == INVALID_HANDLE_VALUE) {
			CHAR KeyName[sizeof(KeyPrefix) + 90];

			lstrcpy(KeyName, KeyPrefix);
			if (*CurrProfile != '\0') {
				lstrcat(KeyName, "\\");
				lstrcat(KeyName, CurrProfile);
			}
			if (*Subkey != '\0') {
				lstrcat(KeyName, "\\");
				lstrcat(KeyName, Subkey);
			}
			if (RegOpenKeyEx(HKEY_CURRENT_USER, KeyName, 0,
							 KEY_QUERY_VALUE | KEY_SET_VALUE,
							 &hKey) != ERROR_SUCCESS) {
				hKey = INVALID_HANDLE_VALUE;
				if (*CurrProfile == '\0') UseRegistry = FALSE;
			}
		}
		if (UseRegistry /*may have changed*/) {
			DWORD dwSize = Size;
			DWORD dwType;
			LONG  dwValue;
			WCHAR wName[40];
			INT	  n;

			n = MultiByteToWideChar(CP_ACP, 0, Key, strlen(Key),
									wName, WSIZE(wName));
			wName[n] = '\0';
			if (hKey != INVALID_HANDLE_VALUE &&
					RegQueryValueExW(hKey, wName, NULL, &dwType,
									 (LPBYTE)Buf, &dwSize) == ERROR_SUCCESS) {
				if (dwType == REG_DWORD && dwSize == sizeof(LONG)) {
					memcpy(&dwValue, Buf, sizeof(LONG));
					_snwprintf(Buf, Size/sizeof(WCHAR), L"%ld", dwValue);
				}
			} else wcsncpy(Buf, Default, Size/sizeof(WCHAR));
			return wcslen(Buf);
		}
	}
	#if 1
	if (Size >= sizeof(WCHAR)) *Buf = '\0';
	return 0;
	#else	/*TODO*/
	{	INT Result;

		Result = GetProfileString(ProfileSection, Key, Default, Buf, Size);
		#if defined(WIN32)
			if (!UseRegistry && lstrcmp(Buf, Default) != 0) {
				/*first use after switch to registry, save key to registry...*/
				if (hKey == INVALID_HANDLE_VALUE) {
					CHAR  KeyName[sizeof(KeyPrefix) + 64];
					DWORD Disposition;

					lstrcpy(KeyName, KeyPrefix);
					if (*CurrProfile != '\0') {
						lstrcat(KeyName, "\\");
						lstrcat(KeyName, CurrProfile);
					}
					if (RegCreateKeyEx(HKEY_CURRENT_USER, KeyName, 0, NULL,
									   REG_OPTION_NON_VOLATILE,
									   KEY_QUERY_VALUE | KEY_SET_VALUE, NULL,
									   &hKey, &Disposition) != ERROR_SUCCESS)
						hKey = INVALID_HANDLE_VALUE;
				}
				if (hKey != INVALID_HANDLE_VALUE)
					RegSetValueEx(hKey, Key, 0, REG_SZ, Buf, lstrlen(Buf) + 1);
			}
		#endif
		return (Result);
	}
	#endif
}

static DWORD MyGetProfileLong(LPCSTR Key, DWORD Default, WORD SysDefaultMask)
{	CHAR Buf[16];

	MyGetProfileString(Key, "", Buf, sizeof(Buf));
	if (!*Buf) {
		wUseSysColors |= SysDefaultMask;
		return (Default);
	}
	return (strtol(Buf, NULL, 0));
}

static INT MyGetProfileInt(LPCSTR Key, INT Default)
{
	return ((INT)MyGetProfileLong(Key, Default, 0));
}

static VOID MyGetProfileNewString(LPCSTR Key, LPCSTR Default, LPSTR *pResult)
{	CHAR Buf[260];

	MyGetProfileString(Key, Default, Buf, sizeof(Buf));
	AllocStringA(pResult, Buf);
}

static VOID MyGetProfileNewStringW(LPCSTR Key, PCWSTR Default, PWSTR *pResult)
{	WCHAR Buf[260];

	MyGetProfileStringW(Key, Default, Buf, sizeof(Buf));
	AllocStringW(pResult, Buf);
}

static BOOL MyGetProfileBool(LPCSTR Key, BOOL Default)
{	CHAR Buf[4];

	MyGetProfileString(Key, "", Buf, sizeof(Buf));
	if (!*Buf) return (Default);
	return (*Buf == '1' || (*Buf = LCASE(*Buf)) == 'y' || *Buf == 't');
}

#if defined(WIN32)
static BOOL RegKey(VOID)
{
	if (hKey == INVALID_HANDLE_VALUE) {
		CHAR  KeyName[sizeof(KeyPrefix) + 90];
		DWORD Disposition;

		lstrcpy(KeyName, KeyPrefix);
		if (*CurrProfile != '\0') {
			lstrcat(KeyName, "\\");
			lstrcat(KeyName, CurrProfile);
		}
		if (*Subkey != '\0') {
			lstrcat(KeyName, "\\");
			lstrcat(KeyName, Subkey);
		}
		if (RegCreateKeyEx(HKEY_CURRENT_USER, KeyName, 0, NULL,
						   REG_OPTION_NON_VOLATILE,
						   KEY_QUERY_VALUE | KEY_SET_VALUE,
						   NULL, &hKey, &Disposition) != ERROR_SUCCESS) {
			hKey = INVALID_HANDLE_VALUE;
			UseRegistry = FALSE;
		}
	}
	return (hKey != INVALID_HANDLE_VALUE);
}
#endif

static VOID MyWriteProfileString(LPCSTR Key, PCSTR Value)
{
	#if defined(WIN32)
		if (RegKey()) {
			if (Value == NULL) RegDeleteValue(hKey, Key);
			else RegSetValueEx(hKey, Key, 0, REG_SZ, (LPBYTE)Value,
							   lstrlen(Value) + 1);
			return;
		}
	#endif
	WriteProfileString(ProfileSection, Key, Value);
}

static VOID MyWriteProfileStringW(LPCSTR Key, PCWSTR Value)
{	WCHAR Name[20];
	INT	  n;

	n = MultiByteToWideChar(CP_ACP, 0, Key, strlen(Key), Name, WSIZE(Name));
	Name[n] = '\0';
	#if defined(WIN32)
		if (RegKey()) {
			if (Value == NULL) RegDeleteValue(hKey, Key);
			else RegSetValueExW(hKey, Name, 0, REG_SZ, (LPBYTE)Value,
								2 * wcslen(Value) + 2);
			return;
		}
	#endif
	//WriteProfileStringW(ProfileSection, Name, Value);
}

static VOID MyWriteProfileInt(LPCSTR Key, PCSTR Format, LONG Value)
{	//WCHAR Buf[16];

	#if defined(WIN32)
		if (RegKey()) {
			if (RegSetValueEx(hKey, Key, 0, REG_DWORD,
							  (PBYTE)&Value, sizeof(LONG)) == ERROR_SUCCESS)
				return;
		}
	#endif
	//_snwprintf(Buf, WSIZE(Buf), Format, Value);
	//MyWriteProfileString(Key, Buf);
}

static VOID MyWriteProfileBool(LPCSTR Key, BOOL Value)
{
	MyWriteProfileString(Key, Value ? "Yes" : "No");
}

/*  ^^^  elementary functions handling win.ini and registry  ^^^  */
/*----------------------------------------------------------------*/
/*  vvv  functions using the elementary functions above      vvv  */

PCSTR LdFormat  = "%ld";
PCSTR L6xFormat = "0x%06lx";
BOOL  SearchBoxPosition;
INT   SearchBoxX, SearchBoxY;

VOID SaveSettings(VOID)
{
	if (!UnsafeSettings) return;
	SwitchProfile(ProfileSaveAndClear, ProfileBuffer);
	if (  (UINT)((WholeWindow.left + WholeWindow.right) / 2)
				< (UINT)GetSystemMetrics(SM_CXSCREEN) &&
		  (UINT)((WholeWindow.top + WholeWindow.bottom) / 2)
				< (UINT)GetSystemMetrics(SM_CYSCREEN)) {
		MyWriteProfileInt(pfXPos,	LdFormat, WholeWindow.left);
		MyWriteProfileInt(pfYPos,	LdFormat, WholeWindow.top);
	}
	MyWriteProfileInt(pfWidth,	LdFormat, WholeWindow.right -WholeWindow.left);
	MyWriteProfileInt(pfHeight, LdFormat, WholeWindow.bottom-WholeWindow.top);
	MyWriteProfileBool(pfMaximized, IsZoomed(hwndMain));
	if (SearchBoxPosition) {
		MyWriteProfileInt(pfSearchBoxX, LdFormat, SearchBoxX);
		MyWriteProfileInt(pfSearchBoxY, LdFormat, SearchBoxY);
	}
	SwitchProfile(ProfileRestore, ProfileBuffer);
}

extern CHAR DrivePathUNC[260], Extensions[80];

VOID SaveProfileSettings(VOID)
{
	MyWriteProfileString(pfDrivePathUNC,  *DrivePathUNC ? DrivePathUNC : NULL);
	MyWriteProfileString(pfExtensions,	  *Extensions   ? Extensions   : NULL);
	MyWriteProfileBool  (pfAutoPermanent, AutoPermanent);
}

VOID SaveColorSettings(VOID)
{	INT  i, n;
	CHAR Buf[16*9], *p;

	if (wUseSysColors & 2)
		MyWriteProfileString(pfFgColor,	   NULL);
	else MyWriteProfileInt(pfFgColor,	   L6xFormat,	 TextColor);
	if (wUseSysColors & 1)
		MyWriteProfileString(pfBkColor,	   NULL);
	else MyWriteProfileInt(pfBkColor,	   L6xFormat,	 BackgroundColor);
	if (wUseSysColors & 8)
		MyWriteProfileString(pfSelFgColor, NULL);
	else MyWriteProfileInt(pfSelFgColor,   L6xFormat,	 SelTextColor);
	if (wUseSysColors & 4)
		MyWriteProfileString(pfSelBkColor, NULL);
	else MyWriteProfileInt(pfSelBkColor,   L6xFormat,	 SelBkgndColor);
	MyWriteProfileInt	  (pfShmFgColor,   L6xFormat,	 ShmTextColor);
	MyWriteProfileInt	  (pfShmBkColor,   L6xFormat,	 ShmBkgndColor);
	MyWriteProfileInt	  (pfEolColor,	   L6xFormat,	 EolColor);
	MyWriteProfileStringW (pfBitmap, *BitmapFilename ? BitmapFilename : NULL);
	for (n = 15; n >= 0; --n)
		if (CustomColors[n] != 0) break;
	p = Buf;
	for (i = 0;;) {
		if (CustomColors[i] != 0)
			p += snprintf(p, 9, L6xFormat, CustomColors[i] & 0xffffffUL);
		if (++i > n) break;
		*p++ = ',';
	}
	MyWriteProfileString(pfCustomColors, n >= 0 ? Buf : NULL);
}

VOID SaveFiletypeSettings(VOID)
{	static CHAR *NewlineStrings[] = {NULL, "crlf", "lf", "cr"};
	static CHAR *HexModeStrings[] = {NULL, "hex"};

	MyWriteProfileString(pfNewline, NewlineStrings[SelectDefaultNewline]);
	MyWriteProfileString(pfHexMode, HexModeStrings[SelectDefaultHexMode]);
	switch (SelectDefaultCharSet) {
		case CS_AUTOENCODING: MyWriteProfileString(pfCharSet, NULL);	  break;
		case CS_ANSI:		  MyWriteProfileString(pfCharSet, "ansi");	  break;
		case CS_OEM:		  MyWriteProfileString(pfCharSet, "oem");	  break;
		case CS_EBCDIC:		  MyWriteProfileString(pfCharSet, "ebcdic");  break;
		case CS_UTF8:		  MyWriteProfileString(pfCharSet, "utf-8");	  break;
		case CS_UTF16LE:	  MyWriteProfileString(pfCharSet, "utf-16le");break;
		case CS_UTF16BE:	  MyWriteProfileString(pfCharSet, "utf-16be");break;
	}
	#if defined(WIN32)
		if (AnsiCodePage == CP_ACP)
			 MyWriteProfileString(pfAnsiCodePage, NULL);
		else MyWriteProfileInt	 (pfAnsiCodePage, LdFormat, AnsiCodePage);
		if (OemCodePage  == CP_OEMCP)
			 MyWriteProfileString(pfOemCodePage,  NULL);
		else MyWriteProfileInt	 (pfOemCodePage,  LdFormat, OemCodePage);
	#endif
}

static VOID WriteFontAttributes(LPCSTR ProfileKey, PLOGFONT LogFont)
{	CHAR Buf[20];

	snprintf(Buf, sizeof(Buf)-7, "%d", LogFont->lfWeight);
	if (LogFont->lfItalic)	  lstrcat(Buf, ",i");
	if (LogFont->lfUnderline) lstrcat(Buf, ",u");
	if (LogFont->lfStrikeOut) lstrcat(Buf, ",s");
	MyWriteProfileString(ProfileKey, Buf);
}

static VOID SaveFont(PLOGFONT LogFont, LPCSTR FaceKey, LPCSTR HeightKey,
									   LPCSTR AttrKey, LPCSTR CharsetKey)
{
	if (*LogFont->lfFaceName) {
		MyWriteProfileString(FaceKey,				 LogFont->lfFaceName);
		MyWriteProfileInt	(HeightKey,	 LdFormat,   LogFont->lfHeight);
		MyWriteProfileInt	(CharsetKey, LdFormat,   LogFont->lfCharSet);
		WriteFontAttributes(AttrKey, LogFont);
	}
}

static VOID SaveGlyphsAvail(LPCSTR Key, BOOL NoGlyphs, BYTE GlyphsAvail[32])
{
#if 0
	CHAR AvailBuffer[68], *p;
	INT	 i;
#endif

	if (NoGlyphs) {
		MyWriteProfileString(Key, NULL);
		return;
	}
#if 0
	for (p = AvailBuffer, i = 0; i < 32; ++i) {
		snprintf(p, 3, "%02x", *GlyphsAvail++);
		p += 2;
	}
	MyWriteProfileString(Key, AvailBuffer);
#endif
}

extern LOGFONT TextLogFont, HexLogFont;

VOID SaveFontSettings(VOID)
{
	if (wUseSysColors & 2)
		MyWriteProfileString(pfFgColor,	   NULL);
	else MyWriteProfileInt(pfFgColor,	   L6xFormat,	 TextColor);
	SaveFont(&TextLogFont, pfFontFace, pfFontSize, pfFontAttr, pfFontCharSet);
	SaveFont(&HexLogFont,  pfHexFontFace, pfHexFontSize, pfHexFontAttr,
						   pfHexFontCharSet);
	SaveGlyphsAvail(pfFontGlyphsAvail,	  NoGlyphs,	   TextGlyphsAvail);
	SaveGlyphsAvail(pfHexFontGlyphsAvail, NoHexGlyphs, HexGlyphsAvail);
}

VOID SaveEditSettings(VOID)
{
	MyWriteProfileBool(pfReadOnly,				  ReadOnlyFlag);
	MyWriteProfileBool(pfAutoIndent,			  AutoIndentFlag);
	MyWriteProfileBool(pfAutoWrite,				  AutoWriteFlag);
	MyWriteProfileBool(pfWrapScan,				  WrapScanFlag);
	MyWriteProfileBool(pfWrapPos,				  WrapPosFlag);
	MyWriteProfileBool(pfIgnoreCase,			  IgnoreCaseFlag);
	MyWriteProfileBool(pfMagic,					  MagicFlag);
	MyWriteProfileBool(pfQuitIfMore,			  QuitIfMoreFlag);
	MyWriteProfileInt (pfUndoLimit,		LdFormat, UndoLimit);
	MyWriteProfileInt (pfShiftWidth,	LdFormat, ShiftWidth);
	MyWriteProfileBool(pfDefaultInsert,			  DefaultInsFlag);
	MyWriteProfileBool(pfFullRowExtend,			  FullRowFlag);
	MyWriteProfileStringW(pfTmpDirectory, DefaultTmpFlag ? NULL : TmpDirectory);
	MyWriteProfileStringW(pfTags,
						  Tags!=NULL && wcscmp(Tags, L"tags") ? Tags : NULL);
	MyWriteProfileInt (pfPageThreshold, LdFormat, PageThreshold);
}

VOID SaveDisplaySettings(VOID)
{
	MyWriteProfileBool(pfTerse,					  TerseFlag);
	MyWriteProfileBool(pfAutoRestore,			  AutoRestoreFlag);
	MyWriteProfileBool(pfShowMatch,				  ShowMatchFlag);
	MyWriteProfileInt (pfTabStop,		LdFormat, TabStop);
	MyWriteProfileInt (pfMaxScroll,		LdFormat, MaxScroll);
	MyWriteProfileBool(pfLineNumber,			  NumberFlag);
	MyWriteProfileString(pfHexLineNumber, HexNumberFlag ? "hexadecimal" : NULL);
	MyWriteProfileBool(pfRefreshBkgnd,			  RefreshFlag);
}

VOID SaveFileSettings(VOID)
{
	MyWriteProfileBool(pfLcaseFnames,			  LcaseFlag);
	MyWriteProfileBool(pfQuitIfMore,			  QuitIfMoreFlag);
	MyWriteProfileStringW(pfTmpDirectory, DefaultTmpFlag ? NULL : TmpDirectory);
	MyWriteProfileStringW(pfTags,
						  Tags!=NULL && wcscmp(Tags, L"tags") ? Tags : NULL);
	#if defined(WIN32)
		MyWriteProfileStringW(pfTabExpandDomains, wcscmp(TabExpandDomains,L"*")
												   ? TabExpandDomains : NULL);
	#endif
	MyWriteProfileInt(pfPageThreshold, LdFormat, PageThreshold);
}

LPSTR Shell, ShellCommand;
BOOL  TabExpandWithSlash;
PSTR  HelpURL = NULL;
PSTR  DefaultHelpURL = "file://%EXEPATH/WinVi-%LANG.htm#";

VOID SaveShellSettings(VOID)
{
	MyWriteProfileString(pfShell,		 Shell);
	MyWriteProfileString(pfShellCommand, ShellCommand);
	MyWriteProfileBool  (pfTabExpWithSlash, TabExpandWithSlash);
	MyWriteProfileString(pfHelpUrl,
						 HelpURL == NULL || lstrcmp(HelpURL, DefaultHelpURL)==0
						 ? NULL : HelpURL);
}

VOID SaveMappingSettings(VOID)
{	PMAP		This;
	INT			i = 0;

	#if defined(WIN32)
		if (hKey != INVALID_HANDLE_VALUE) {
			RegCloseKey(hKey);
			hKey = INVALID_HANDLE_VALUE;
		}
		for (This = FirstMapping; This != NULL; This = This->Next) {
			wsprintf(Subkey, pfMapping, ++i);
			if (RegKey()) {
				MyWriteProfileString(pfMappingName,	   This->Name);
				MyWriteProfileString(pfMappingInput,   This->InputDisplay);
				MyWriteProfileString(pfMappingReplace, This->Replace);
				MyWriteProfileInt   (pfMappingFlags,   LdFormat, This->Flags);
				RegCloseKey(hKey);
				hKey = INVALID_HANDLE_VALUE;
			}
		}
		*Subkey = '\0';
		/*remove any remaining old mappings/abbreviations now...*/
		if (RegKey()) {
			CHAR Name[24];

			for (;;) {
				wsprintf(Name, pfMapping, ++i);
				if (RegDeleteKey(hKey, Name) != ERROR_SUCCESS) break;
			}
		}
	#endif
}

extern INT		  nPrtLeft, nPrtTop, nPrtRight, nPrtBottom;
extern BOOL		  LineNumbering, Frame, Frame2, Frame3;
extern BOOL		  OverrideOrientation, DoubleColumn;
extern INT		  nOrientation;
extern PWSTR	  Header, Footer;
extern LOGFONT	  lf;
extern CHOOSEFONT cf;

VOID SavePrintSettings(VOID)
{	static PSTR Fmt = "%d";

	MyWriteProfileInt	(pfPrintBorderLeft,   Fmt, nPrtLeft);
	MyWriteProfileInt	(pfPrintBorderTop,    Fmt, nPrtTop);
	MyWriteProfileInt	(pfPrintBorderRight,  Fmt, nPrtRight);
	MyWriteProfileInt	(pfPrintBorderBottom, Fmt, nPrtBottom);
	MyWriteProfileString(pfPrintOrientation,
		OverrideOrientation ? (DoubleColumn ? pfoDoubleColumn :
							   (nOrientation ? pfoLandscape : pfoPortrait))
							: NULL);
	MyWriteProfileBool(pfPrintNumbers,		LineNumbering);
	if (lf.lfHeight) {
		WriteFontAttributes	(pfPrintFontAttr,   &lf);
		MyWriteProfileString(pfPrintFontFace,		 lf.lfFaceName);
		MyWriteProfileInt	(pfPrintFontCharSet,Fmt, lf.lfCharSet);
		MyWriteProfileInt	(pfPrintFontSize1,  Fmt, lf.lfHeight);
		MyWriteProfileInt	(pfPrintFontSize2,  Fmt, cf.iPointSize);
	}
	MyWriteProfileBool	 (pfPrintFrame,  Frame);
	MyWriteProfileBool	 (pfPrintFrame2, Frame2);
	MyWriteProfileBool	 (pfPrintFrame3, Frame3);
	MyWriteProfileStringW(pfPrintHeader, Header ? Header : L"");
	MyWriteProfileStringW(pfPrintFooter, Footer ? Footer : L"");
}

VOID SaveLanguage(LPSTR pLang)
{
	MyWriteProfileString(pfLanguage, pLang);
}

CHAR ViLang[4];

VOID SaveAllSettings(VOID)
{	extern LPSTR NewLangStr;

	SaveSettings();
	SaveColorSettings();
	SaveFontSettings();
	SaveEditSettings();
	SaveDisplaySettings();
	SaveFileSettings();
	SaveShellSettings();
	SaveMappingSettings();
	SavePrintSettings();
	SaveLanguage(NewLangStr != NULL ? NewLangStr : ViLang);
}

/*  ^^^  functions for saving settings   ^^^  */
/*--------------------------------------------*/
/*  vvv  functions for loading settings  vvv  */

static VOID GetFontAttributes(LPCSTR ProfileKey, PLOGFONT LogFont)
{	CHAR Buf[16], *p;

	if (MyGetProfileString(ProfileKey, "", Buf, sizeof(Buf))) {
		LogFont->lfWeight = (INT)strtol(Buf, &p, 10);
		--p;
		while (*++p == ',') {
			switch (*++p) {
				case 'i':	LogFont->lfItalic	 = TRUE;	continue;
				case 'u':	LogFont->lfUnderline = TRUE;	continue;
				case 's':	LogFont->lfStrikeOut = TRUE;	continue;
			}
			break;
		}
	}
}

static VOID LoadPrintSettings(VOID)
{	static WCHAR PrintBuf[82];

	nPrtLeft   = (INT)MyGetProfileLong(pfPrintBorderLeft,   nPrtLeft,   0);
	nPrtTop    = (INT)MyGetProfileLong(pfPrintBorderTop,    nPrtTop,    0);
	nPrtRight  = (INT)MyGetProfileLong(pfPrintBorderRight,  nPrtRight,  0);
	nPrtBottom = (INT)MyGetProfileLong(pfPrintBorderBottom, nPrtBottom, 0);
	MyGetProfileStringW(pfPrintOrientation, L"", PrintBuf, WSIZE(PrintBuf));
	switch (*PrintBuf) {
		case 'd': case 'D': /*double column*/
			OverrideOrientation = nOrientation = DoubleColumn = TRUE;
			break;

		case 'l': case 'L': /*landscape*/
			OverrideOrientation = nOrientation = TRUE;
			DoubleColumn = FALSE;
			break;

		case 'p': case 'P': /*portrait*/
			OverrideOrientation = TRUE;
			nOrientation = DoubleColumn = FALSE;
			break;

		default:
			OverrideOrientation = nOrientation = DoubleColumn = FALSE;
			break;
	}
	LineNumbering =		 MyGetProfileBool(pfPrintNumbers,	   LineNumbering);
	MyGetProfileString(pfPrintFontFace, "Courier New",
					   lf.lfFaceName, sizeof(lf.lfFaceName));
	GetFontAttributes(pfPrintFontAttr, &lf);
	if (cf.iPointSize == 0) cf.iPointSize = 100;
	lf.lfCharSet  = (INT)MyGetProfileLong(pfPrintFontCharSet, lf.lfCharSet,	 0);
	lf.lfHeight   = (INT)MyGetProfileLong(pfPrintFontSize1,	  lf.lfHeight,	 0);
	cf.iPointSize = (INT)MyGetProfileLong(pfPrintFontSize2,	  cf.iPointSize, 0);
	Frame		  =		 MyGetProfileBool(pfPrintFrame,		  Frame);
	Frame2		  =		 MyGetProfileBool(pfPrintFrame2,	  Frame2);
	Frame3		  =		 MyGetProfileBool(pfPrintFrame3,	  Frame3);
	MyGetProfileStringW(pfPrintHeader, L"&n", PrintBuf, WSIZE(PrintBuf));
	if (Header) _ffree(Header);
	if (!*PrintBuf) Header = NULL;
	else if ((Header = _fcalloc(wcslen(PrintBuf) + 1, sizeof(WCHAR))) != NULL)
		wcscpy(Header, PrintBuf);
	{	WCHAR Def[10];

		LOADSTRINGW(hInst, 247, Def, WSIZE(Def));
		MyGetProfileStringW(pfPrintFooter, Def, PrintBuf, WSIZE(PrintBuf));
	}
	if (Footer) _ffree(Footer);
	if (!*PrintBuf) Footer = NULL;
	else if ((Footer = _fcalloc(wcslen(PrintBuf) + 1, sizeof(WCHAR))) != NULL)
		wcscpy(Footer, PrintBuf);
}

static VOID LoadFontInfo(LPCSTR FaceKey, LPCSTR HeightKey, LPCSTR AttrKey,
						 LPCSTR CharsetKey, PLOGFONT LogFont,
						 LPCSTR GlyphsAvailKey, BYTE GlyphsAvail[32])
{	extern INT HexTextHeight, TextTextHeight;
#if 0
	CHAR	   AvailBuffer[68], *p;
	INT		   i;
#endif

	if (MyGetProfileString(FaceKey, "Courier New",
						   LogFont->lfFaceName, sizeof(LogFont->lfFaceName))) {
		LogFont->lfHeight  = MyGetProfileInt(HeightKey, 0);
		if (!LogFont->lfHeight) {
			HDC hDC = GetDC(NULL);

			if (hDC) {
				LogFont->lfHeight =
					-MulDiv(10/*pt*/, GetDeviceCaps(hDC, LOGPIXELSY), 72);
				ReleaseDC(NULL, hDC);
			} else LogFont->lfHeight = 10;
		}
		GetFontAttributes(AttrKey, LogFont);
		LogFont->lfCharSet = MyGetProfileInt(CharsetKey, DEFAULT_CHARSET);
										  /*don't care ansi, etc.*/
#if 0
		if (MyGetProfileString(GlyphsAvailKey, "",
							   AvailBuffer, sizeof(AvailBuffer))) {
			for (i = 0, p = AvailBuffer; i < 32 && p[0] && p[1]; ++i) {
				*GlyphsAvail = *p & 15;
				if (*p > '9') *GlyphsAvail += 9;
				*GlyphsAvail <<= 4;
				*GlyphsAvail += *++p & 15;
				if (*p++ > '9') *GlyphsAvail += 9;
				++GlyphsAvail;
			}
			if ((LogFont == &TextLogFont) == !HexEditMode) {
				GlyphsAvail -= 32;
				for (i = 0; i <= 255; ++i)
					if (GlyphsAvail[i >> 3] & (1 << (i & 7)))
						 CharFlags(i) &= ~2;
					else CharFlags(i) |=  2;
			}
			GlyphsAvail = NULL;
		}
#endif
		if (LogFont == &TextLogFont) {
			if (TextFont) DeleteObject(TextFont);
			NewFont(&TextFont, LogFont, &TextTextHeight, GlyphsAvail);
			if (!HexEditMode) TextHeight = TextTextHeight;
		} else {
			if (HexFont) DeleteObject(HexFont);
			NewFont(&HexFont,  LogFont, &HexTextHeight, GlyphsAvail);
			if (HexEditMode) TextHeight = HexTextHeight;
		}
#if 0
		if (GlyphsAvail)
			SaveGlyphsAvail(GlyphsAvailKey, FALSE, GlyphsAvail);
#endif
	}
}

UINT GetDefaultLanguage(BOOL SysDefault)
{	UINT uLanguage;
	CHAR SysLang[16];
	CHAR *p = SysLang;

	GetProfileString("intl", "sLanguage", "", SysLang, sizeof(SysLang));
	MyGetProfileString(pfLanguage, "", ViLang, sizeof(ViLang));
	if (!SysDefault && *ViLang) p = ViLang;
	p[0] = LCASE(p[0]);
	p[1] = LCASE(p[1]);
	if		(p[0]=='d' && p[1]=='e') /*German*/  uLanguage =  7000;
	else if (p[0]=='e' && p[1]=='s') /*Spanish*/ uLanguage = 10000;
	else if (p[0]=='f' && p[1]=='r') /*French*/  uLanguage = 12000;
	else				    /*default: English*/ uLanguage =  9000;
	return (uLanguage);
}

static VOID LoadFontSettings(VOID)
{	extern BOOL	IsFixedFont;

	LoadFontInfo(pfFontFace, pfFontSize, pfFontAttr, pfFontCharSet,
				 &TextLogFont, pfFontGlyphsAvail, TextGlyphsAvail);
	LoadFontInfo(pfHexFontFace, pfHexFontSize, pfHexFontAttr, pfHexFontCharSet,
				 &HexLogFont, pfHexFontGlyphsAvail, HexGlyphsAvail);
	AveCharWidth = 0;		/*will be set again in first paint operation*/
	IsFixedFont	 = FALSE;	/*...*/
	AdjustWindowParts(ClientRect.bottom, ClientRect.bottom);
}

static VOID LoadMappingSettings(VOID)
{	INT  i = 0;
	CHAR Value[256];
	PMAP ThisMapping, *MapPtr;

	FreeMappings(FirstMapping);
	FirstMapping = NULL;
	#if defined(WIN32)
		MapPtr = &FirstMapping;
		if (hKey != INVALID_HANDLE_VALUE) {
			RegCloseKey(hKey);
			hKey = INVALID_HANDLE_VALUE;
		}
		for (;;) {
			wsprintf(Subkey, pfMapping, ++i);
			if (!MyGetProfileString(pfMappingName, "", Value, sizeof(Value)))
				break;
			ThisMapping = calloc(1, sizeof(*ThisMapping));
			if (ThisMapping == NULL) break;
			AllocStringA(&ThisMapping->Name, Value);
			MyGetProfileString(pfMappingInput, "", Value, sizeof(Value));
			if (!TranslateMapping(&ThisMapping->InputMatch, Value)) continue;
			AllocStringA(&ThisMapping->InputDisplay, Value);
			MyGetProfileString(pfMappingReplace, "", Value, sizeof(Value));
			if (!TranslateMapping(&ThisMapping->ReplaceMap, Value)) continue;
			AllocStringA(&ThisMapping->Replace, Value);
			ThisMapping->Flags = MyGetProfileLong(pfMappingFlags, 0, 0);
			RegCloseKey(hKey);
			hKey = INVALID_HANDLE_VALUE;
			*MapPtr = ThisMapping;
			MapPtr = &ThisMapping->Next;
		}
		if (hKey != INVALID_HANDLE_VALUE) {
			RegCloseKey(hKey);
			hKey = INVALID_HANDLE_VALUE;
		}
		*Subkey = '\0';
	#endif
}

COLORREF ShmTextColor  = RGB(255,255,255);
COLORREF ShmBkgndColor = RGB(255,000,000);
COLORREF EolColor	   = RGB(128,000,000);
BOOL	 SuppressAsterisk;

static VOID GetIndexList(VOID);

extern DWORD MyGetVersion(VOID);

VOID LoadSettings(VOID)
{
	SwitchProfile(ProfileSaveAndClear, ProfileBuffer);
	x				= MyGetProfileInt(pfXPos,	   x);
	y				= MyGetProfileInt(pfYPos,	   y);
	Width			= MyGetProfileInt(pfWidth,	   Width);
	Height			= MyGetProfileInt(pfHeight,	   Height);
	Maximized		= MyGetProfileBool(pfMaximized, FALSE);
	memset(CaretProps, 0, sizeof(CaretProps));
	MyGetProfileString(pfCaretProps, "", CaretProps, sizeof(CaretProps));
	MyGetProfileString(pfWindowTitle, "", WindowTitle, sizeof(WindowTitle));
	if (*WindowTitle == '\0')
		snprintf(WindowTitle, sizeof(WindowTitle) - 1,
				 WinVersion >= MAKEWORD(95,3) ? "&s&m - %s" : "%s - &s&m",
				 ApplName);
	WholeWindow.left   = x;
	WholeWindow.top    = y;
	WholeWindow.right  = x + Width;
	WholeWindow.bottom = y + Height;
	SearchBoxX		= MyGetProfileInt(pfSearchBoxX, -500);
	if (SearchBoxX != -500) {
		SearchBoxY	= MyGetProfileInt(pfSearchBoxY, 50);
		SearchBoxPosition = TRUE;
	}
	SwitchProfile(ProfileRestore, ProfileBuffer);
	LoadFontSettings();
	wUseSysColors	= 0;
	TabStop			= MyGetProfileInt(pfTabStop,		TabStop);
	ShiftWidth		= MyGetProfileInt(pfShiftWidth,		ShiftWidth);
	MaxScroll		= MyGetProfileInt(pfMaxScroll,		MaxScroll);
	TextColor		= GetSysColor(COLOR_WINDOWTEXT);
	BackgroundColor	= GetSysColor(COLOR_WINDOW);
	SelTextColor	= GetSysColor(COLOR_HIGHLIGHTTEXT);
	SelBkgndColor	= GetSysColor(COLOR_HIGHLIGHT);
	TextColor		= MyGetProfileLong(pfFgColor,		TextColor,		 2);
	BackgroundColor	= MyGetProfileLong(pfBkColor,		BackgroundColor, 1);
	SelTextColor	= MyGetProfileLong(pfSelFgColor,	SelTextColor,	 8);
	SelBkgndColor	= MyGetProfileLong(pfSelBkColor,	SelBkgndColor,	 4);
	ShmTextColor	= MyGetProfileLong(pfShmFgColor,	ShmTextColor,	 0);
	ShmBkgndColor	= MyGetProfileLong(pfShmBkColor,	ShmBkgndColor,	 0);
	EolColor		= MyGetProfileLong(pfEolColor,		EolColor,		 0);
	UndoLimit		= MyGetProfileLong(pfUndoLimit,		UndoLimit,		 0);
	{	CHAR Buf[16*9];

		memset(CustomColors, 0, sizeof(CustomColors));
		if (MyGetProfileString(pfCustomColors, "", Buf, sizeof(Buf))) {
			CHAR *p = Buf;
			INT  i  = 0;

			do {
				if (*p++ != '0' || LCASE(*p++) != 'x') break;
				CustomColors[i] = strtol(p, &p, 16);
				if (*p != ',') break;
				do ++i; while (*++p == ',');
			} while (i < 16);
		}
	}
	MyGetProfileString(pfDrivePathUNC, "", DrivePathUNC,sizeof(DrivePathUNC));
	MyGetProfileString(pfExtensions,   "", Extensions,	sizeof(Extensions));
	MyGetProfileStringW(pfBitmap,  L"", BitmapFilename,	sizeof(BitmapFilename));
	MyGetProfileNewString(pfShell,		  "",			&Shell);
	MyGetProfileNewString(pfShellCommand, "%0 /c %1",	&ShellCommand);
	TabExpandWithSlash = MyGetProfileBool(pfTabExpWithSlash, FALSE);
	MyGetProfileNewString(pfHelpUrl,	DefaultHelpURL,	&HelpURL);
	if (*Shell == '\0') {
		static PSTR	 CmdName = "c:\\command.com";
		static HFILE hf		 = (HFILE)INVALID_HANDLE_VALUE;

		if (hf == (HFILE)INVALID_HANDLE_VALUE) {
			if (MyGetVersion() & 0x80000000U) {
				/*not NT...*/
				OFSTRUCT TmpOf;

				hf = OpenFile(CmdName, &TmpOf, OF_READ);
				if (hf != (HFILE)INVALID_HANDLE_VALUE) _lclose(hf);
				else CmdName += 3;	/*can hopefully be found in path*/
			} else	 CmdName = "cmd.exe";
			hf = 0;	/*not used as handle anymore, prevent further checks only*/
		}
		AllocStringA(&Shell, CmdName);
	}
	{	static CHAR	Value[12];
		extern CHAR	*CrLfAttribute;
		INT			CharSet;

		MyGetProfileString(pfNewline, "", Value, sizeof(Value));
		switch (LCASE(Value[0])) {
			case 'c': SelectDefaultNewline =
						 Value[1] != '\0' && LCASE(Value[2]) == 'l' ? 1 : 3;
					  break;

			case 'l': SelectDefaultNewline = 2;
					  break;

			default:  SelectDefaultNewline = 0;
		}
		SetDefaultNl();

		MyGetProfileString(pfHexLineNumber, "", Value, sizeof(Value));
		HexNumberFlag		 = LCASE(Value[0]) == 'h';
		MyGetProfileString(pfHexMode, "", Value, sizeof(Value));
		SelectDefaultHexMode = LCASE(Value[0]) == 'h';
		if (HexEditMode != SelectDefaultHexMode) ToggleHexMode();

		MyGetProfileString(pfCharSet, "", Value, sizeof(Value));
		switch (LCASE(Value[0])) {
			case 'o':
			case 'p': SelectDefaultCharSet = CS_OEM;
					  break;

			case 'e': SelectDefaultCharSet = CS_EBCDIC;
					  break;

			case 'u': if (Value[4] == '8')
						   SelectDefaultCharSet = CS_UTF8;
					  else if (Value[6] == 'b')
						   SelectDefaultCharSet = CS_UTF16BE;
					  else SelectDefaultCharSet = CS_UTF16LE;
					  break;

			case 'a': if (Value[1] == 'n') {
						   SelectDefaultCharSet = CS_ANSI;
						   break;
					  }
					  /*FALLTHROUGH*/
			default:  SelectDefaultCharSet = CS_AUTOENCODING;
		}
		if (SelectDefaultCharSet == CS_AUTOENCODING) {
			if (UtfEncoding) {
				if (UtfEncoding == 8) CharSet = CS_UTF8;
				else CharSet = UtfLsbFirst ? CS_UTF16LE : CS_UTF16BE;
			} else CharSet = CS_ANSI;
		} else CharSet = SelectDefaultCharSet;
		SetCharSet(CharSet, 0);
	}
	ReleaseBmp(0);
	#if defined(WIN32)
		MyGetProfileStringW(pfTabExpandDomains, L"*",
						    TabExpandDomains, sizeof(TabExpandDomains));
	#endif
	TerseFlag		= MyGetProfileBool(pfTerse, 		FALSE);
	#if defined(WIN32)
		LcaseFlag	= MyGetProfileBool(pfLcaseFnames,	FALSE);
	#else
		LcaseFlag	= MyGetProfileBool(pfLcaseFnames,	TRUE);
	#endif
	ReadOnlyFlag	= MyGetProfileBool(pfReadOnly,		FALSE);
	AutoRestoreFlag	= MyGetProfileBool(pfAutoRestore,	TRUE);
	AutoIndentFlag	= MyGetProfileBool(pfAutoIndent,	FALSE);
	AutoWriteFlag	= MyGetProfileBool(pfAutoWrite,		FALSE);
	ShowMatchFlag	= MyGetProfileBool(pfShowMatch,		FALSE);
	WrapScanFlag	= MyGetProfileBool(pfWrapScan,		TRUE);
	WrapPosFlag		= MyGetProfileBool(pfWrapPos,		FALSE);
	IgnoreCaseFlag	= MyGetProfileBool(pfIgnoreCase,	TRUE);
	MagicFlag		= MyGetProfileBool(pfMagic,			TRUE);
	QuitIfMoreFlag	= MyGetProfileBool(pfQuitIfMore,	FALSE);
	DefaultInsFlag	= MyGetProfileBool(pfDefaultInsert,	TRUE);
	NumberFlag		= MyGetProfileBool(pfLineNumber,	FALSE);
	RefreshFlag		= MyGetProfileBool(pfRefreshBkgnd,	FALSE);
	FullRowFlag		= MyGetProfileBool(pfFullRowExtend,	TRUE);
	BreakLineFlag	= MyGetProfileBool(pfBreakLine,		FALSE);
	SuppressAsterisk= MyGetProfileBool("SuppressCaptionAsterisk", FALSE);
	AutoPermanent	= MyGetProfileBool(pfAutoPermanent,FALSE);
	MyGetProfileNewStringW(pfTmpDirectory, L"", &TmpDirectory);
	MyGetProfileNewStringW(pfTags, L"tags", &Tags);
	DefaultTmpFlag	= !*TmpDirectory;
	PageThreshold	= MyGetProfileLong(pfPageThreshold,	PageThreshold, 0);
	MakeBackgroundBrushes();
	AnsiCodePage	= MyGetProfileInt(pfAnsiCodePage, CP_ACP);
	OemCodePage		= MyGetProfileInt(pfOemCodePage,  CP_OEMCP);
	if (AnsiCodePage == 0) AnsiCodePage = CP_ACP;
	if (OemCodePage  == 0) OemCodePage  = CP_OEMCP;
	{	extern VOID SetNewCodePage(VOID);

		SetNewCodePage();
	}
	LoadPrintSettings();
	LoadMappingSettings();
	ChangeLanguage(GetDefaultLanguage(FALSE));
	#if defined(WIN32)
		if (!UseRegistry) {
			/*just for copying win.ini file entries to registry...*/
			SwitchProfile(ProfileSaveAndClear, ProfileBuffer);
			GetIndexList();
			SwitchProfile(ProfileRestore, ProfileBuffer);
			SaveAllSettings();
		}
	#endif
	if (hwndMain) InvalidateRect(hwndMain, NULL, TRUE);
}

/******** list of recently used files... ********/

#define NUMFILES 6

static INT  Index[NUMFILES] = {-1,-1,-1,-1,-1,-1};
static CHAR Buff[264];
static INT  AlreadyInsertedLruFiles;

static VOID GetIndexList(VOID)
{	INT  i;
	CHAR *p;
	BOOL Free[NUMFILES];

	MyGetProfileString(pfRecentFiles, "", Buff, sizeof(Buff));
	for (i=0; i<NUMFILES; ++i) Free[i] = TRUE;
	for (i=0, p=Buff; *p; ++p) {
		INT n = strtol(p, &p, 10);

		if (n>=0 && n<NUMFILES && Free[n]) {
			Index[i++] = n;
			Free[n] = FALSE;
		}
		if (*p == '\0') break;
	}
	#if defined(WIN32)
		if (!UseRegistry) {
			/*first use after switch to registry, dup entries by loading...*/
			for (i=0; i<NUMFILES; ++i) {
				PSTR Value;

				Value = Buff + 1 + snprintf(Buff, sizeof(Buff), pfRecentDir, i);
				MyGetProfileString(Buff, "", Value, sizeof(Buff)-(Value-Buff));
				Value = Buff + 1 + snprintf(Buff, sizeof(Buff), pfRecentFile,i);
				MyGetProfileString(Buff, "", Value, sizeof(Buff)-(Value-Buff));
			}
		}
	#endif
}

static VOID WriteIndexList(VOID)
{	CHAR *p = Buff;
	INT  i;

	Buff[1] = '\0';
	for (i=0; i<NUMFILES; ++i)
		if (Index[i] != -1) {
			*p++ = ',';
			p += snprintf(p, sizeof(Buff)-(p-Buff), "%d", Index[i]);
		}
	MyWriteProfileString(pfRecentFiles, Buff+1 /*skip initial comma*/);
}

VOID InitFileList(VOID)
{
	AlreadyInsertedLruFiles = 0;
}

VOID InsertFileList(HMENU hMenu)
{	INT   i;
	CHAR  Key[16];
	WCHAR wBuff[264];

	SwitchProfile(ProfileSaveAndClear, ProfileBuffer);
	GetIndexList();
	for (i=0; i<NUMFILES; ++i) {
		if (Index[i] == -1) break;
		snprintf(Key,  sizeof(Key),  pfRecentFile, Index[i]);
		_snwprintf(wBuff, WSIZE(wBuff), L"&%d   ", i+1);
		if (!MyGetProfileStringW(Key, L"", wBuff+5, sizeof(wBuff)-10))
			continue;
		if (wcslen(wBuff) > 32) {
			PWSTR p = wBuff + wcslen(wBuff) - 32;

			while (p>wBuff && *p!='\\' && *p!='/') --p;
			if (p > wBuff+14) {
				memmove(wBuff+8, p, 2*wcslen(p)+2);
				wBuff[5] = wBuff[6] = wBuff[7] = '.';
			}
		}
		{	PWSTR p = wcschr(wBuff+5, '&');

			while (p != NULL && wcslen(wBuff) < WSIZE(wBuff) - 2) {
				memmove(p+1, p, 2*wcslen(p)+2);
				p = wcschr(p+2, '&');
			}
		}
		if (i >= AlreadyInsertedLruFiles) {
			if (!AlreadyInsertedLruFiles) {
				/*insert another separator...*/
				InsertMenu(hMenu, 13, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
			}
			InsertMenuW(hMenu, 13+i, MF_BYPOSITION | MF_STRING, 3000+i, wBuff);
			++AlreadyInsertedLruFiles;
		} else {
			ModifyMenuW(hMenu, 13+i, MF_BYPOSITION | MF_STRING, 3000+i, wBuff);
		}
	}
	SwitchProfile(ProfileRestore, ProfileBuffer);
}

VOID AddToFileList(PCWSTR FileName)
{	INT  i, j;
	CHAR Key[16];

	if (!*FileName) return;
	SwitchProfile(ProfileSaveAndClear, ProfileBuffer);
	GetIndexList();
	for (i=0; i<NUMFILES; ++i) {
		if (Index[i] != -1) {
			WCHAR wBuff[264];

			snprintf(Key, sizeof(Key), pfRecentFile, Index[i]);
			MyGetProfileStringW(Key, L"", wBuff, sizeof(wBuff));
			if (wcsicmp(wBuff, FileName) == 0) {
				INT  j, k = Index[i];
				BOOL Differs = wcscmp(wBuff, FileName) != 0;

				snprintf(Key, sizeof(Key), pfRecentDir, Index[i]);
				if (MyGetProfileString(Key, "", Buff, sizeof(Buff))) {
					CHAR Dir[260];

					_getcwd(Dir, sizeof(Dir));
					if (lstrcmpi(Dir, Buff) != 0) {
						if ( (FileName[1]!=':' ||
							 (FileName[2]!='\\' && FileName[2]!='/')) &&
							((FileName[0]!='\\' && FileName[0]!='/') ||
							  FileName[1]!=FileName[0]))
							continue;
					}
					if (lstrcmp(Dir, Buff) != 0)
						MyWriteProfileString(Key, Dir);
				}
				if (Differs) {
					snprintf(Key, sizeof(Key), pfRecentFile, Index[i]);
					MyGetProfileString(Key, "", Buff, sizeof(Buff));
					MyWriteProfileStringW(Key, FileName);
				}
				for (j=i; j>0; --j) Index[j] = Index[j-1];
				Index[0] = k;
				WriteIndexList();
				SwitchProfile(ProfileRestore, ProfileBuffer);
				return;
			}
		}
	}
	{	BOOL Occupied[NUMFILES];

		for (i=0; i<NUMFILES; ++i) Occupied[i] = FALSE;
		for (i=0; i<NUMFILES; ++i) {
			if (Index[i] == -1) break;
			Occupied[Index[i]] = TRUE;
		}
		if (i == NUMFILES) j = Index[--i];
		else for (j=0; j<NUMFILES && Occupied[j]; ++j);
	}
	while (i > 0) {
		Index[i] = Index[i-1];
		--i;
	}
	Index[0] = j;
	WriteIndexList();
	snprintf(Key, sizeof(Key), pfRecentFile, j);
	MyWriteProfileStringW(Key, FileName);
	snprintf(Key, sizeof(Key), pfRecentDir, j);
	_getcwd(Buff, sizeof(Buff));
	MyWriteProfileString(Key, Buff);
	SwitchProfile(ProfileRestore, ProfileBuffer);
}

VOID FileListCommand(INT Command)
{
	if (Command>=3000 && Command<3000+NUMFILES && Index[Command-=3000]!=-1) {
		CHAR  Key[16];
		WCHAR wBuff[264];

		if (ExecRunning()) {
			ErrorBox(MB_ICONSTOP, 236);
			return;
		}
		if (!AskForSave(hwndMain, 1)) return;
		SwitchProfile(ProfileSaveAndClear, ProfileBuffer);
		snprintf(Key, sizeof(Key), pfRecentDir, Index[Command]);
		wcscpy(wBuff, L":cd ");
		if (MyGetProfileStringW(Key, L"", wBuff+4, sizeof(wBuff)-8)) {
			NewStatus(0, wBuff, NS_BUSY);
			SetCurrentDirectoryW(wBuff+4);
		}
		snprintf(Key, sizeof(Key), pfRecentFile, Index[Command]);
		if (MyGetProfileStringW(Key, L"", wBuff, sizeof(wBuff))) {
			SwitchProfile(ProfileRestore, ProfileBuffer);
			wcsncpy(CommandBuf, wBuff, WSIZE(CommandBuf));
			NewStatus(0, L"", NS_NORMAL);
			CommandWithExclamation = TRUE;	/*prevent another AskForSave()*/
			Open(CommandBuf, 0);
			CommandWithExclamation = FALSE;
		} else {
			SwitchProfile(ProfileRestore, ProfileBuffer);
			Error(412);
		}
	}
}

/************* Debug settings... ****************/

BOOL DebugMessages(VOID)
{
	return (MyGetProfileBool("Debug", FALSE));
}
