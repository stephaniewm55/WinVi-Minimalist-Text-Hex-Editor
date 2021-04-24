/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2007 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *     11-Jan-2003	extracted from paint.c
 *     12-Jan-2003	added FormatShell and Help
 */

#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include "winvi.h"

static WCHAR ErrBuf1[200], ErrBuf2[280];
extern WCHAR BufferW[300];

void Error(INT StringNo, ...)
{
	LOADSTRINGW(hInst, StringNo, ErrBuf1, WSIZE(ErrBuf1));
	if (!*ErrBuf1) return;
	_vsnwprintf(BufferW, WSIZE(BufferW), ErrBuf1, (char*)(&StringNo+1));
	NewStatus(0, BufferW, NS_ERROR);
}

WCHAR ErrorTitle[16] = {'\0'};

INT ErrorBox(INT Flags, INT StringNo, ...)
{
	LOADSTRINGW(hInst, StringNo, ErrBuf1, WSIZE(ErrBuf1));
	if (!*ErrBuf1 /*error string not defined in resource string table*/)
		return(IDCANCEL);
	if (!*ErrorTitle) LOADSTRINGW(hInst, 314, ErrorTitle, WSIZE(ErrorTitle));
	_vsnwprintf(ErrBuf2, WSIZE(ErrBuf2), ErrBuf1, (char*)(&StringNo+1));
	return (MessageBoxW(hwndErrorParent, ErrBuf2,
			Flags & MB_ICONINFORMATION ? L"WinVi" : ErrorTitle, MB_OK | Flags));
}

BOOL Confirm(HWND hParent, INT nQuestion, ...)
{	CHAR Title[20];
	CHAR Format[80];
	CHAR Question[256];
	INT	 Answer;

	LOADSTRING(hInst, 341,		 Title,  sizeof(Title));
	LOADSTRING(hInst, nQuestion, Format, sizeof(Format));
	wvsprintf(Question, Format, (char*)(&nQuestion+1));
	Answer = MessageBox(hParent, Question, Title, MB_YESNO | MB_ICONQUESTION);
	return (Answer == IDYES);
}

INT FormatShell(PSTR Result, LPSTR FormatString, LPSTR Cmd0, PSTR Cmd1)
{	PSTR p;
	INT   Flags = 0, Length = 0;
	BOOL  Cmd1Done = FALSE;

	for (;;) {
		switch (*FormatString++) {
			case '\0':
				if (!Cmd1Done) {
					if (Result != NULL) {
						*Result++ = ' ';
						strcpy(Result, Cmd1);
					}
					Length += strlen(Cmd1) + 1;
				} else if (Result != NULL) *Result = '\0';
				return (Length);

			case '%':
				switch (*FormatString++) {
					case '0':	p = Cmd0;
								break;

					case '1':	p = Cmd1;
								Cmd1Done = TRUE;
								break;

					case '%':	++FormatString;
								/*FALLTHROUGH*/
					default:	--FormatString;
								if (Result != NULL) *Result++ = '%';
								++Length;
								continue;
				}
				while (*p != '\0') {
					if (   (*p == '\'' && Flags & 1)
						|| (*p == '"'  && Flags & 2)
						|| (*p == '\\' && Flags)) {
							if (Result != NULL) *Result++ = '\\';
							++Length;
					}
					if (Result != NULL) *Result++ = *p;
					++Length;
					++p;
				}
				break;

			case '\'':
				Flags ^= 1;
				if (Result != NULL) *Result++ = '\'';
				++Length;
				break;

			case '"':
				Flags ^= 2;
				/*FALLTHROUGH*/
			default:
				if (Result != NULL) *Result++ = FormatString[-1];
				++Length;
		}
	}
}

INT FormatShellW(PWSTR Result, LPSTR FormatString, LPSTR Cmd0, PWSTR Cmd1)
{	LPSTR lp;
	PWSTR pw;
	INT   Flags = 0, Length = 0;
	BOOL  Cmd1Done = FALSE;

	for (;;) {
		switch (*FormatString++) {
			case '\0':
				if (!Cmd1Done) {
					if (Result != NULL) {
						*Result++ = ' ';
						wcscpy(Result, Cmd1);
					}
					Length += wcslen(Cmd1) + 1;
				} else if (Result != NULL) *Result = '\0';
				return (Length);

			case '%':
				switch (*FormatString++) {
					case '0':	lp = Cmd0;
								while (*lp != '\0') {
									if ((*lp == '\'' && Flags & 1) ||
										(*lp == '"'  && Flags & 2) ||
										(*lp == '\\' && Flags)) {
											if (Result != NULL)
												*Result++ = '\\';
											++Length;
									}
									if (Result != NULL) *Result++ = *lp;
									++Length;
									++lp;
								}
								break;

					case '1':	pw = Cmd1;
								Cmd1Done = TRUE;
								while (*pw != '\0') {
									if ((*pw == '\'' && Flags & 1) ||
										(*pw == '"'  && Flags & 2) ||
										(*pw == '\\' && Flags)) {
											if (Result != NULL)
												*Result++ = '\\';
											++Length;
									}
									if (Result != NULL) *Result++ = *pw;
									++Length;
									++pw;
								}
								break;

					case '%':	++FormatString;
								/*FALLTHROUGH*/
					default:	--FormatString;
								if (Result != NULL) *Result++ = '%';
								++Length;
								continue;
				}
				break;

			case '\'':
				Flags ^= 1;
				if (Result != NULL) *Result++ = '\'';
				++Length;
				break;

			case '"':
				Flags ^= 2;
				/*FALLTHROUGH*/
			default:
				if (Result != NULL) *Result++ = FormatString[-1];
				++Length;
		}
	}
}

extern CHAR  QueryString[150];
extern DWORD QueryTime;

BOOL ShowUrl(PSTR Url)
{	
	CHAR  Command[260];
	HKEY  Key;
	DWORD Type;
	CHAR  Value[180];
	DWORD Size;

	do {	/*no loop, for break only*/
		Key = INVALID_HANDLE_VALUE;
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, ".htm", 0, KEY_QUERY_VALUE, &Key)
			!= ERROR_SUCCESS) break;
		Size = sizeof(Value);
		if (RegQueryValueEx(Key, NULL, NULL, &Type, (LPBYTE)Value, &Size)
			!= ERROR_SUCCESS) break;
		RegCloseKey(Key);
		Key = INVALID_HANDLE_VALUE;
		if (strlen(Value) > sizeof(Value) - 20) break;
		strcat(Value, "\\shell\\open\\command");
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, Value, 0, KEY_QUERY_VALUE, &Key)
			!= ERROR_SUCCESS) break;
		Size = sizeof(Value);
		if (RegQueryValueEx(Key, NULL, NULL, &Type, (LPBYTE)Value, &Size)
			!= ERROR_SUCCESS) break;
		RegCloseKey(Key);
		if (FormatShell(NULL, Value, "", Url) >= WSIZE(Command))
				break;
		FormatShell(Command,  Value, "", Url);
		WinExec(Command, SW_NORMAL);
		if (strlen(Command) < WSIZE(QueryString)) {
			strcpy(QueryString, Command);
			QueryTime = GetTickCount();
		}
		return (TRUE);
	} while (FALSE);
	if (Key != INVALID_HANDLE_VALUE) RegCloseKey(Key);
	return (FALSE);
}

PCSTR HelpAnchors[] = {
	/*HelpAll*/				NULL,
	/*HelpEditTextmode*/	"EditTextmode",
	/*HelpEditHexmode*/		"EditHexmode",
	/*HelpInsertmode*/		"Insertmode",
	/*HelpReplacemode*/		"Replacemode",
	/*HelpEditC*/			"EditC",
	/*HelpEditD*/			"EditD",
	/*HelpEditY*/			"EditY",
	/*HelpEditZ*/			"EditZ",
	/*HelpEditMark*/		"EditMark",
	/*HelpEditFT*/			"EditFT",
	/*HelpEditZZ*/			"EditZZ",
	/*HelpEditShift*/		"EditShift",
	/*HelpEditShell*/		"EditShell",
	/*HelpEditDoublequote*/	"EditDoublequote",
	/*HelpPrint*/			"Print",
	/*HelpSearch*/			"Search",
	/*HelpSearchDlg*/		"SearchDlg",
	/*HelpSettingsProfile*/	"SettingsProfile",
	/*HelpSettingsFiletype*/"SettingsFiletype",
	/*HelpSettingsDisplay*/	"SettingsDisplay",
	/*HelpSettingsEdit*/	"SettingsEdit",
	/*HelpSettingsFiles*/	"SettingsFiles",
	/*HelpSettingsMapping*/	"SettingsMapping",
	/*HelpSettingsShell*/	"SettingsShell",
	/*HelpSettingsColors*/	"SettingsColors",
	/*HelpSettingsFonts*/	"SettingsFonts",
	/*HelpSettingsPrint*/	"SettingsPrint",
	/*HelpSettingsLanguage*/"SettingsLang",
	/*HelpExCommand*/		"ExCommand",
	/*HelpExRead*/			"ExRead",
	/*HelpExEdit*/			"ExEdit",
	/*HelpExNext*/			"ExNext",
	/*HelpExRewind*/		"ExRewind",
	/*HelpExWrite*/			"ExWrite",
	/*HelpExFilename*/		"ExFilename",
	/*HelpExArgs*/			"ExArgs",
	/*HelpExVi*/			"ExVi",
	/*HelpExCd*/			"ExCd",
	/*HelpExTag*/			"ExTag",
	/*HelpExShell*/			"ExShell",
	/*HelpExSubstitute*/	"ExSubstitute"
};
PCSTR		HelpUrlPrefix = "file://c:/Bin/WinViNfo.htm";
HELPCONTEXT	HelpContext;

VOID Help(VOID)
{	extern LPSTR HelpURL;

	if (HelpContext >= 0
			&& HelpContext < sizeof(HelpAnchors) / sizeof(HelpAnchors[0])) {
		CHAR Url[260], *p;

		strncpy(Url, HelpURL, WSIZE(Url));
		while ((p = strstr(Url, "%EXEPATH")) != NULL) {
			PSTR Tail = NULL;

			if (!AllocStringA(&Tail, p + 8)) break;
			if (!GetModuleFileName(hInst, p, WSIZE(Url) - (p - Url)))
				break;
			while (*p != '\0') {
				if (*p == '\\') *p = '/';
				++p;
			}
			do; while (--p > Url && *p != '/' && *p != '\\');
			strncpy(p, Tail, WSIZE(Url) - (p - Url));
			_ffree(Tail);
		}
		while ((p = strstr(Url, "%LANG")) != NULL) {
			PSTR p2;

			p2 = Language ==  7000 ? "de" :
				 Language == 10000 ? "es" :
				 Language == 12000 ? "fr" : "en";
			strcpy(p, p2);
			memmove(p + 2, p + 5, strlen(p + 4));
		}
		if (HelpContext != HelpAll
				&& strlen(HelpAnchors[HelpContext]) + strlen(Url) + 1
				   < WSIZE(Url))
			strcat(Url, HelpAnchors[HelpContext]);
		else {
			INT Len = strlen(Url);

			if (Len && Url[--Len] == '#') Url[Len] = '\0';
		}
		ShowUrl(Url);
	}
}
