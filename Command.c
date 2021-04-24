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
 *      3-Dec-2000	first publication of source code
 *     21-Jun-2002	recalculating pixel position after changing tabstop variable
 *     22-Jul-2002	use of private myassert.h
 *      4-Nov-2002	substitute command displayed in status line while executing
 *     10-Jan-2003	new variable "shell"
 *     24-Jul-2007	several changes for unicode handling
 *     12-Jan-2011	bugfix: cd command with ##/%% works again
 */

#include <windows.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <direct.h>
#include <malloc.h>
#include <wchar.h>
#include "myassert.h"
#include "resource.h"
#include "winvi.h"
#include "page.h"
#include "status.h"
#include "toolbar.h"
#include "paint.h"
#include "exec.h"

#if !defined(_MSC_VER)
#define _chdir chdir	/*for Borland compiler*/
#endif

extern INT	 nCurrFileListEntry, nNumFileListEntries;
extern INT	 OemCodePage, AnsiCodePage;
extern LPSTR Shell, ShellCommand;

PWSTR		 Args;
INT 		 CmdMask;
static INT	 ExecError;
WCHAR		 FileNameBuf2[260];	/*used in [command.c]Compare:Substitute[#%],
								 *		  [command.c]CdCmd:_getcwd(),
								 *		  [winvi.c]MainProc:WM_DROPFILES and
								 *		  [file.c]Open/WriteCmd:OPENFILENAME
								 */

PWSTR Compare(PWSTR Cmd, PWSTR CompareWith, INT MinLength, INT CheckMask)
	/* CheckMask: 1 = line allowed
	 *			  2 = line range allowed
	 *			  4 = arguments allowed
	 *			  8 = command may be forced with '!'
	 *		   0x10 = substitute '#'/'%' in arguments
	 */
{
	while (*Cmd == ' ') ++Cmd;
	IsAltFile = FALSE;
	for (;;) {
		INT c = *Cmd++ ^ *CompareWith++;

		if (c && (c & ~('a'-'A') || CompareWith[-1] < 'a')) return (NULL);
		--MinLength;
		if (!iswalpha(*Cmd) || !iswalpha(CompareWith[-1])) {
			if (MinLength > 0) return (NULL);
			if (*Cmd == '!' && CheckMask & 8) {
				CommandWithExclamation = TRUE;
				++Cmd;
			}
			while (*Cmd == ' ') ++Cmd;
			{	INT m = CmdMask;

				if (*Cmd) m |= 4;
				if (m &= ~CheckMask) {
					if		(m & 1) ExecError = 104;
					else if (m & 2) ExecError = 105;
					else if (m & 4) ExecError = 106;
					return (NULL);
				}
			}
			if (CheckMask & 0x10) {
				PWSTR p, p1, p2;

				for (p = Cmd+wcslen(Cmd)-1; p>=Cmd && *p==' '; --p)
					*p = '\0';
				p = Cmd;
				if (!wcscmp(p, L"#")) IsAltFile = TRUE;
				for (;;) {
					PCWSTR lp;
					INT    len;

					p1 = wcschr(p, '#');
					p2 = wcschr(p, '%');
					if (p1!=NULL) p = p1;
					else if (p2==NULL) break;
					if (p2!=NULL && (p1==NULL || p2<p)) p = p2;
					if (Cmd != FileNameBuf2) {
						wcscpy(FileNameBuf2, Cmd);
						p = FileNameBuf2 + (p-Cmd);
						Cmd = FileNameBuf2;
					}
					if (p[0] == p[1]) {
						/*special case:
						 *  "##" or "%%" treated as single character
						 *  which should not be converted (2.72 28-Feb-99)
						 */
						_fmemmove(p, p+1, wcslen(p)*2);
						++p;
						continue;
					}
					if	   (*p == '#')	 lp = AltFileName;
					else /*(*p == '%')*/ lp = CurrFile;
					if (!*lp) {
						ExecError = *p=='#' ? 113 : 233;
						return (NULL);
					}
					len = wcslen(lp);
					_fmemmove(p+len, p+1, wcslen(p)*2);
					_fmemcpy(p, lp, len*2);
					p += len;
				}
			}
			return (Cmd);
		}
	}
}

#if defined(WIN32)
BOOL CALLBACK SetForegroundWndProc(HWND hWnd, LPARAM lParam)
{
	PARAM_NOT_USED(lParam);
	if (GetParent(hWnd) == NULL) SetForegroundWindow(hWnd);
	return (TRUE);
}
#endif

UINT WinExecW(PWSTR Cmd, WORD Mode)
{	static PROCESS_INFORMATION	pi;
	static STARTUPINFOW			si;

	si.cb		   = sizeof(si);
	si.dwFlags	   = STARTF_FORCEONFEEDBACK | STARTF_USESHOWWINDOW;
	si.wShowWindow = Mode;
	if (!CreateProcessW(NULL, Cmd, NULL, NULL, FALSE,
					    CREATE_DEFAULT_ERROR_MODE, NULL, NULL,
					    &si, &pi))
		return 0;
	WaitForInputIdle(pi.hProcess, 500);
	EnumThreadWindows(pi.dwThreadId, SetForegroundWndProc, 0);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return (UINT)pi.hProcess;
}

BOOL NewViEditCmd(PWSTR Args)
{	PWSTR p;
	UINT  Err = 0;
	INT   Size;

	Size = wcslen(Args) + 266;
	if ((p = malloc(Size*sizeof(WCHAR))) != NULL) {
		*p = '"';
		if (GetModuleFileNameW(hInst, p+1, 260) > 0) {
			_snwprintf(p+wcslen(p), Size-wcslen(p), L"\" \"%s\"", Args);
			if (*Args) SetAltFileName(Args, NULL);
			if ((Err = WinExecW(p, SW_NORMAL)) < 32)
				FileError(p, (int)Err+300, 332);
		}
		free(p);
	} else Error(300, 1);		/*fatal: no memory*/
	return (Err >= 32);
}

static BOOL RewindCmd(void)
{	PWSTR lp;

	if (nNumFileListEntries && (lp = GetFileListEntry(0), lp != NULL)) {
		if (!AskForSave(hwndMain, 1)) return (FALSE);
		Unsafe = FALSE;		/*already asked for save*/
		nCurrFileListEntry = 0;
		Open(lp, 0);
		return (TRUE);
	}
	Error(221);		/*no file list error*/
	return (FALSE);
}

BOOL MoreFilesWarned = FALSE, Exiting = FALSE;

BOOL QuitCmd(BOOL AskIfMoreFiles)
{
	if (nCurrFileListEntry < nNumFileListEntries-1 && !QuitIfMoreFlag) {
		if (!CommandWithExclamation && !MoreFilesWarned) {
			MoreFilesWarned = TRUE;
			if (!AskIfMoreFiles) {
				Error(232);
				return (FALSE);
			}
			ErrorBox(MB_ICONEXCLAMATION, 315);
			return (FALSE);
		}
	}
	SaveSettings();
	Exiting = TRUE;
	DestroyWindow(hwndMain);
	assert(hwndMain == 0);
	return (TRUE);
}

extern CHAR  Buffer[300];
extern WCHAR BufferW[300];
PWSTR		 PreviousWorkingDir = NULL;

static BOOL CdCmd(PWSTR Dir)
{	PWSTR p;

	if (*Dir) {
		extern BOOL NotEdited;
		BOOL		OtherLocation = FALSE;

		/*first check if current file has full path...*/
		if (GetCurrentDirectoryW(WSIZE(BufferW), BufferW))
			 p = BufferW;
		else p = NULL;
		if (Dir[0] == '-' && Dir[1] == '\0' && PreviousWorkingDir != NULL)
			Dir = PreviousWorkingDir;
		do {	/*no loop, for break only*/
			if (!CurrFile || !*CurrFile) break;
			if (CharFlags(*CurrFile) & 64) {
				if (CharFlags(CurrFile[1]) & 64) break;
				if (CharFlags(Dir[0]) & 64) {
					if (!(CharFlags(Dir[1]) & 64)) break;
				} else {
					if (Dir[1] != ':') break;
					/*check if same drive...*/
					if (p != NULL && p[1]==':' && *p==*Dir) break;
				}
			} else if (CurrFile[1] == ':' && CharFlags(CurrFile[2]) & 64)
				break;
			if (Unsafe && !AskForSave(hwndMain, 2)) return (FALSE);
			OtherLocation = TRUE;
		} while (FALSE);
		if (!SetCurrentDirectoryW(Dir)) {
			Error(227+TerseFlag, (LPSTR)Dir);
			return (FALSE);
		}
		if (PreviousWorkingDir != NULL) free(PreviousWorkingDir);
		if (p != NULL) {
			PreviousWorkingDir = malloc(wcslen(p) * 2 + 2);
			if (PreviousWorkingDir != NULL) wcscpy(PreviousWorkingDir, p);
		} else PreviousWorkingDir = NULL;
		NotEdited = OtherLocation;
		SwitchToMatchingProfile(CurrFile);
	}
	if (!GetCurrentDirectoryW(WSIZE(FileNameBuf2), p = FileNameBuf2)) {
		Error(229);
		return (FALSE);
	}
	{	static WCHAR Fmt[30];

		LowerCaseFnameW(p);
		if (!*Fmt) LOADSTRINGW(hInst, 237, Fmt, WSIZE(Fmt));
		_snwprintf(BufferW, WSIZE(BufferW), Fmt, p);
	}
	NewStatus(0, BufferW, NS_NORMAL);
	return (TRUE);
}

INT LinesVar, ColumnsVar;

void ResizeWithLinesAndColumns(void)
{	INT  Width, Height;
	RECT r;

	GetWindowRect(hwndMain, &r);
	Width  = r.right  - r.left;
	Height = r.bottom - r.top;
	if (LinesVar) {
		Height -= ClientRect.bottom - ClientRect.top;	/*height of nc area*/
		Height += StatusHeight + TOOLS_HEIGHT + 1;		/*height of non-text*/
		Height += 2*YTEXT_OFFSET + LinesVar*TextHeight;	/*new height*/
	}
	if (ColumnsVar) {
		Width -= ClientRect.right - ClientRect.left;	/*width of nc area*/
		Width += GetSystemMetrics(SM_CXVSCROLL);		/*width of non-text*/
		Width += XTEXT_OFFSET-1 + StartX(FALSE) + ColumnsVar*AveCharWidth;
														/*new width*/
	}
	SetWindowPos(hwndMain, 0,0,0, Width, Height, SWP_NOMOVE | SWP_NOZORDER);
	if (ColumnsVar && BreakLineFlag) InvalidateText();
}

static BOOL MatchRight(LPSTR LongerString, LPSTR ShorterString)
{	INT LLen, SLen;

	LLen = lstrlen(LongerString);
	SLen = lstrlen(ShorterString);
	return (LLen > SLen
			&& lstrcmpi(LongerString + (LLen - SLen), ShorterString) == 0);
}

#define VF_BOOLEAN 1
#define VF_DECIMAL 2
#define VF_STRING  4

static BOOL SetCmd(PWSTR Args)
{	static struct {
		WORD	VarFlags;
		INT		Min, Max;
		PWSTR	Name, Abbrev;
		INT		*Variable;
	} VarDescr[] = {
		{0,			 0, 0,	   L"all",		   L"all",	 NULL},
		{VF_BOOLEAN, 0, 0,	   L"ansimode",	   L"am",	 (INT*)&CharSet},
		{VF_DECIMAL, 0, 65001, L"ansicodepage",L"ansicp",(INT*)&AnsiCodePage},
		{VF_BOOLEAN, 0, 0,	   L"autoindent",  L"ai",	 (INT*)&AutoIndentFlag},
		{VF_BOOLEAN, 0, 0,	   L"autowrite",   L"aw",	 (INT*)&AutoWriteFlag},
		{VF_DECIMAL, 8, 500,   L"columns",	   L"co",	 &ColumnsVar},
		{VF_BOOLEAN, 0, 0,	   L"dosmode",	   L"dm",	 (INT*)&CharSet},
		{VF_BOOLEAN, 0, 0,	   L"ebcdimode",   L"em",	 (INT*)&CharSet},
		{VF_BOOLEAN, 0, 0,	   L"hexmode",	   L"hm",	 (INT*)&HexEditMode},
		{VF_BOOLEAN, 0, 0,	   L"ignorecase",  L"ic",	 (INT*)&IgnoreCaseFlag},
		{VF_DECIMAL, 1, 200,   L"lines",	   L"li",	 &LinesVar},
		{VF_BOOLEAN, 0, 0,	   L"list",		   L"list",  (INT*)&ListFlag},
		{VF_BOOLEAN, 0, 0,	   L"magic",	   L"magic", (INT*)&MagicFlag},
		{VF_BOOLEAN, 0, 0,	   L"number",	   L"nu",	 (INT*)&NumberFlag},
		{VF_DECIMAL, 0, 2000,  L"oemcodepage", L"oemcp", (INT*)&OemCodePage},
		{VF_BOOLEAN, 0, 0,	   L"readonly",	   L"ro",	 (INT*)&ReadOnlyFlag},
		{VF_DECIMAL, 0, 80,	   L"scroll",	   L"sc",	 &MaxScroll},
		{VF_STRING,  0, 0,	   L"shell",	   L"sh",	 (INT*)&Shell},
		{VF_DECIMAL, 1, 128,   L"shiftwidth",  L"sw",	 &ShiftWidth},
		{VF_BOOLEAN, 0, 0,	   L"showmatch",   L"sm",	 (INT*)&ShowMatchFlag},
		{VF_DECIMAL, 1, 128,   L"tabstop",	   L"ts",	 &TabStop},
		{VF_STRING,  0, 0,	   L"tags",		   L"tags",  (INT*)&Tags},
		{VF_BOOLEAN, 0, 0,	   L"terse",	   L"terse", (INT*)&TerseFlag},
		{VF_STRING,  0, 0,	   L"tmpdirectory",L"tmp",	 (INT*)&TmpDirectory},
		{VF_BOOLEAN, 0, 0,	   L"viewonly",	   L"vo",	 (INT*)&ViewOnlyFlag},
		{VF_BOOLEAN, 0, 0,	   L"wrapscan",	   L"ws",	 (INT*)&WrapScanFlag},
		{0,			 0, 0,	   NULL,		   NULL,	 NULL}
	};
	static WCHAR NameBuf[16];
	INT			 i;

	LinesVar = ColumnsVar = 0;
	if (!*Args) PostMessage(hwndMain, WM_COMMAND, ID_SETTINGS, 0);
	else do {
		BOOL Off = FALSE;

		if (LCASE(Args[0])=='n' && LCASE(Args[1])=='o') {
			Args += 2;
			Off = TRUE;
		}
		for (i=0; i<WSIZE(NameBuf)-1 && isalpha(Args[i]); ++i) {
			NameBuf[i] = Args[i];
		}
		NameBuf[i] = '\0';
		Args += i;
		for (i=0; VarDescr[i].Name && wcsicmp(NameBuf, VarDescr[i].Name)
								   && wcsicmp(NameBuf, VarDescr[i].Abbrev);
				  ++i);
		if (VarDescr[i].Name) {
			if (VarDescr[i].VarFlags & VF_BOOLEAN) {
				if ((BOOL*)VarDescr[i].Variable == &HexEditMode) {
					if (*(BOOL*)VarDescr[i].Variable != !Off) ToggleHexMode();
				} else if ((UINT*)VarDescr[i].Variable == &CharSet) {
					INT NewCharSet;

					switch (*VarDescr[i].Name) {
						case 'a':	/*ANSI*/
							NewCharSet = Off ? 1 : 0;
							break;
						case 'd':	/*OEM*/
							NewCharSet = Off ? 0 : 1;
							break;
						case 'e':	/*EBCDIC*/
							NewCharSet = Off ? 0 : 4;
							break;
					}
					if (*(BOOL*)VarDescr[i].Variable != NewCharSet)
						SetCharSet(NewCharSet, 3);
				} else {
					*(BOOL*)VarDescr[i].Variable = !Off;
					if		((BOOL*)VarDescr[i].Variable == &ReadOnlyFlag)
						ReadOnlyFile = ReadOnlyFlag;
					else if ((BOOL*)VarDescr[i].Variable == &ViewOnlyFlag)
						CheckClipboard();
					else if ((BOOL*)VarDescr[i].Variable == &ListFlag ||
							 (BOOL*)VarDescr[i].Variable == &NumberFlag)
						if (!HexEditMode) InvalidateText();
				}
			} else if (VarDescr[i].VarFlags & VF_DECIMAL) {
				if (Off || *Args++!='=') Error(222);	/*not a boolean*/
				else {
					LONG Value;

					Value = wcstol(Args, &Args, 10);
					if (Value < VarDescr[i].Min || Value > VarDescr[i].Max)
						Error(223, VarDescr[i].Min, VarDescr[i].Max);
					else if (*VarDescr[i].Variable != (INT)Value) {
						*VarDescr[i].Variable = (INT)Value;
						if (VarDescr[i].Variable == &TabStop) {
							HideEditCaret();
							InvalidateArea(&ScreenStart, 0, 7);
							NewPosition(&CurrPos);
							if (CurrCol.PixelOffset < 2147480000)
								GetXPos(&CurrCol);
							ShowEditCaret();
						} else if (VarDescr[i].Variable == &OemCodePage ||
								   VarDescr[i].Variable == &AnsiCodePage) {
							extern VOID SetNewCodePage(VOID);
							SetNewCodePage();
						}
					}
				}
			} else if (VarDescr[i].VarFlags & VF_STRING) {
				PWSTR p;
				INT  c;

				if (Off || *Args++!='=') Error(222);	/*not a boolean*/
				if (*Args=='"') {
					c = '"';
					++Args;
				} else c = '\0';
				for (p=Args; *Args && *Args!=c; ++Args);
				if (!*Args) c = '\0';
				else *Args = '\0';
				AllocStringW((PWSTR*)VarDescr[i].Variable, p);
				*Args = c;
				if (c) ++Args;
				DefaultTmpFlag = TmpDirectory==NULL || !*TmpDirectory;
				if ((LPSTR*)VarDescr[i].Variable == &Shell) {
					/*set appropriate format for shell commands...*/
					if (MatchRight(Shell, "sh.exe") || MatchRight(Shell, "sh"))
						 AllocStringA(&ShellCommand, "%0 -c \"%1\"");
					else AllocStringA(&ShellCommand, "%0 /c %1");
				}
			} else {
				/*set all*/
				if (Off || *Args=='=') {
					Error(226, (LPSTR)NameBuf);
					return (FALSE);
				}
				PostMessage(hwndMain, WM_COMMAND, ID_SETTINGS, 0);
			}
		} else {
			Error(226, (LPSTR)NameBuf);	 /*no such option*/
			return (FALSE);
		}
		while (*Args == ' ') ++Args;
	} while (*Args);
	if (*Args) {
		Error(106);
		return (FALSE);
	}
	ResizeWithLinesAndColumns();
	return (TRUE);
}

static void DeleteCmd(LONG StartLine, LONG EndLine, PWSTR Args)
{
	PARAM_NOT_USED(Args);
	SelectCount = 0;
	if (StartLine > 0 && EndLine >= StartLine) {
		Position(StartLine, 'G', 11);
		Position(1, '0', 11);
		Position(EndLine-StartLine+1, 'j', 31);
		DeleteSelected(1);
		GotChar('\033', 0);
		FindValidPosition(&CurrPos, 0);
		Position(1, '^', 0);
	}
}

PWSTR LastCmd;

static BOOL ExecCmd(PWSTR Args)
{	UINT  Err;
	PWSTR NewCmd;

	do {	/*no loop, for break in case of errors only*/
		if (*Args == '!') {
			if (!LastCmd) {
				Error(235);
				return (FALSE);
			}
			NewCmd = malloc((wcslen(LastCmd) + wcslen(Args)) * 2);
			if (NewCmd == NULL) break;
			wcscpy(NewCmd, LastCmd);
			wcscat(NewCmd, Args+1);
		} else {
			NewCmd = malloc(wcslen(Args) * 2 + 4);
			if (NewCmd == NULL) break;
			*NewCmd = '!';
			wcscpy(NewCmd+1, Args);
		}
		if (LastCmd != NULL) free(LastCmd);
		LastCmd = NewCmd;
		NewStatus(0, LastCmd, NS_NORMAL);
		if ((Err = WinExecW(LastCmd+1, SW_NORMAL)) < 32)
			FileError(LastCmd, (INT)Err+300, 332);
		return (TRUE);
	} while (FALSE);
	Error(300, 2);
	return (FALSE);
}

static void VersionCmd(void)
{
	_snwprintf(BufferW, WSIZE(BufferW), L"WinVi - %s", GetViVersion());
	NewStatus(0, BufferW, NS_NORMAL);
}

static PWSTR GetAddr(PWSTR Line, ULONG *Ptr)
{
	CmdMask = 0;
	switch (*Line) {
		case '.': *Ptr = CountLines(&CurrPos) + 1;
				  /*FALLTHROUGH*/
		case '$': if (*Line == '$') *Ptr = LinesInFile + IncompleteLastLine();
				  ++Line;
				  CmdMask |= 1;
				  break;

		case '\'':/*line of marker position...*/
				  {	  extern POSITION MarkPos[27];
					  INT			  c;

					  if ((c = LCASE(Line[1])) < 'a' || c > 'z') {
						  Error(331);
						  return (NULL);
					  }
					  if (MarkPos[c &= 31].p == NULL) {
						  Error(332);
						  return (NULL);
					  }
					  Line += 2;
					  CmdMask |= 1;
					  *Ptr = CountLines(&MarkPos[c]) + 1;
				  }
				  break;
		case '/':
		case '?': {   POSITION    p;
					  INT		  Forward;

					  p = CurrPos;
					  Forward = *Line=='/' ? 1 : 0;
					  Line = BuildMatchList(Line, *Line, NULL);
					  if (!StartSearch()) return (NULL);
					  if (Forward)
					  	   do; while (!(CharFlags(CharAndAdvance(&p)) & 1));
					  else do; while (!(CharFlags(GoBackAndChar (&p)) & 1));
					  if (!SearchIt(&p, Forward | 2)) {
						  Error(Interrupted ? 218 : Forward ? 210 : 212);
						  if (Disabled) {
							  Enable();
							  ShowEditCaret();
						  }
						  return (NULL);
					  }
					  {	  BOOL SaveHexEdit = HexEditMode;

						  HexEditMode = FALSE;
						  *Ptr = CountLines(&p) + 1;
						  HexEditMode = SaveHexEdit;
					  }
					  if (Disabled) Enable();
				  }
				  CmdMask |= 1;
				  break;
	}
	while (*Line == ' ') ++Line;
	{	INT  Sign;

		if (*Line == '+' || *Line == '-') {
			if (!(CmdMask & 1)) *Ptr = CountLines(&CurrPos) + 1;
			do {
				Sign = *Line=='-' ? -1 : 1;
				do; while (*++Line == ' ');
				if (*Line >= '0' && *Line <= '9') break;
				*Ptr += Sign;
			} while (*Line=='+' || *Line=='-');
			CmdMask |= 1;
		} else if (*Line >= '0' && *Line <= '9') {
			if (CmdMask & 1) return (Line); /*invalid*/
			CmdMask |= 1;
			*Ptr = 0;
			Sign = 1;
		} else return (Line);
		{	LONG n;

			n = 0;
			while (*Line >= '0' && *Line <= '9') n = 10 * n + *Line++ - '0';
			*Ptr += Sign * n;
		}
		while (*Line == ' ') ++Line;
	}
	return (Line);
}

BOOL CommandExec(PWSTR Cmd)
{	BOOL  Ok = FALSE;
	ULONG StartLine, EndLine;
	WCHAR *CmdStart = Cmd;
	/* StartLine:					EndLine:
	 *		-1: unspecified,			-1: unspecified,
	 *		 0: first line (0, %),		-2: last line ($, %)
	 *		>0:	specified line		  0,>0: specified line
	 */

	ExecError = 102;
	if (*Cmd != ':') {
		if (*Cmd=='/' || *Cmd=='?') {
			BuildMatchList(Cmd, *Cmd, NULL);
			return (TRUE);
		}
		if (*Cmd=='!') {
			CmdMask = 0;
			Args = Compare(Cmd, L"!", 1, 0x14);	/*...replace '#' and '%'*/
			if (Args != NULL) Ok = ExecPiped(Args, 3);
			else Error(ExecError);
		} else Error(103);
		return (Ok);
	}
	do; while (*++Cmd==' ' || *Cmd==':');
	if (!*Cmd) return (TRUE);
	StartLine = EndLine = (ULONG)-1L;
	if (*Cmd == '%') {
		++Cmd;
		StartLine = 0;
		EndLine = (ULONG)-2L;
		CmdMask = 3;
	} else {
		Cmd = GetAddr(Cmd, &StartLine);
		if (Cmd != NULL && *Cmd == ',') {
			EndLine = CountLines(&CurrPos) + 1;
			if (!(CmdMask & 1)) StartLine = EndLine;
			Cmd = GetAddr(Cmd+1, &EndLine);
			if (EndLine >= LinesInFile + IncompleteLastLine() &&
				EndLine != (ULONG)-1L)
					EndLine = (ULONG)-2L;
			CmdMask = 3;
		}
		if (Cmd == NULL) return (FALSE);
	}
	if		  ((Args = Compare(Cmd, L"write",      1, 0x1f)) != NULL) {
		Ok = WriteCmd(StartLine, EndLine, Args, (WORD)CommandWithExclamation);
	} else if ((Args = Compare(Cmd, L"edit",	   1, 0x1c)) != NULL) {
		Ok = EditCmd(Args);
	} else if ((Args = Compare(Cmd, L"vi",		   2, 0x1c)) != NULL) {
		Ok = NewViEditCmd(Args);
	} else if ((Args = Compare(Cmd, L"rewind",     3,	 8)) != NULL) {
		Ok = RewindCmd();
	} else if ((Args = Compare(Cmd, L"args",	   2,	 8)) != NULL) {
		ArgsCmd();
		Ok = TRUE;
	} else if ((Args = Compare(Cmd, L"filename",   1, 0x1c)) != NULL) {
		FilenameCmd(Args);
		Ok = TRUE;
	} else if ((Args = Compare(Cmd, L"read",	   1, 0x15)) != NULL) {
		Ok = ReadCmd(StartLine, Args);
	} else if ((Args = Compare(Cmd, L"next",	   1, 0x1c)) != NULL) {
		Ok = NextCmd(Args);
	} else if ((Args = Compare(Cmd, L"wq",		   2, 0x1f)) != NULL) {
		if ((StartLine!=(ULONG)-1L && (StartLine>1U || EndLine!=(ULONG)-2L))
				&& !CommandWithExclamation)
			Error(224);
		else if (WriteCmd(StartLine, EndLine, Args,
						  (WORD)CommandWithExclamation)) {
			Unsafe = FALSE;
			Ok = QuitCmd(FALSE);
		}
	} else if ((Args = Compare(Cmd, L"xit",		   1, 0x1f)) != NULL) {
		if (StartLine!=(ULONG)-1L && (StartLine>1U || EndLine!=(ULONG)-2L))
			Error(225);
		else if (!Unsafe || WriteCmd(StartLine, EndLine, Args,
									 (WORD)CommandWithExclamation)) {
			Unsafe = FALSE;
			Ok = QuitCmd(FALSE);
		}
	} else if (((Args = Compare(Cmd, L"cd",		   2, 0x1c)) != NULL) ||
			   ((Args = Compare(Cmd, L"chdir",	   3, 0x1c)) != NULL)) {
		Ok = CdCmd(Args);
	} else if ((Args = Compare(Cmd, L"tag",		   2,	12)) != NULL) {
		Ok = Tag(Args, (WORD)(CommandWithExclamation | 2));
	} else if ((Args = Compare(Cmd, L"quit",	   1,	 8)) != NULL) {
		if (AskForSave(hwndMain, 0)) {
			Ok = QuitCmd(FALSE);
			if (Ok) Unsafe = FALSE;
		}
	} else if ((Args = Compare(Cmd, L"set",		   2,	 4)) != NULL) {
		Ok = SetCmd(Args);
	} else if ((Args = Compare(Cmd, L"substitute", 1, 7)) != NULL) {
		wcsncpy(BufferW, CmdStart, WSIZE(BufferW));
		NewStatus(0, BufferW, NS_BUSY);
		if (!(CmdMask & 1)) StartLine = CountLines(&CurrPos) + 1;
		if (!(CmdMask & 2)) EndLine   = StartLine;
		Ok = SubstituteCmd(StartLine, EndLine, Args);
	} else if ((Args = Compare(Cmd, L"delete",	   1,	 3)) != NULL) {
		if (!(CmdMask & 1)) StartLine = CountLines(&CurrPos) + 1;
		else if (StartLine == 0) StartLine = 1;
		if (!(CmdMask & 2)) EndLine = StartLine;
		else if (EndLine == (ULONG)-2) EndLine = 2147000000;
		DeleteCmd(StartLine, EndLine, Args);
		Ok = TRUE;
	#if 0
	//	} else if ((Args = Compare(Cmd, L"yank",   1,	 3)) != NULL) {
	//	} else if ((Args = Compare(Cmd, L"global", 1,	 3)) != NULL) {
	//	} else if ((Args = Compare(Cmd, L"v",	   1,	 3)) != NULL) {
	#endif
	} else if ((Args = Compare(Cmd, L"version",	   2,	 0)) != NULL) {
		VersionCmd();
		Ok = TRUE;
	} else if ((Args = Compare(Cmd, L"=",		   1,	 1)) != NULL) {
		_snwprintf(BufferW, WSIZE(BufferW),
				   L"%ld", CmdMask&1 ? StartLine : CountLines(&CurrPos)+1);
		NewStatus(0, BufferW, NS_NORMAL);
		Ok = TRUE;
	} else if ((Args = Compare(Cmd, L"!",		   1, 0x14)) != NULL) {
		Ok = ExecCmd(Args);
	} else if (*Cmd) {
		for (Args=Cmd; *Args && *Args != ' '; ++Args);
		if (*Args != '\0') *Args = '\0';
		Error(ExecError, (LPSTR)Cmd);
		Ok = FALSE;
	} else {
		if (!StartLine) StartLine = 1;
		Ok = Position(StartLine, 'G', 0);
	}
	CommandWithExclamation = IsAltFile = FALSE;
	return (Ok);
}
