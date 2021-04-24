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
 *     24-Dec-2000	dialogbox for "Insert File" (:r) requires file to exist
 *     22-Jun-2002	explicit directory specification for WinMe and WinXP
 *     20-Jul-2002	allow writing when executing shell command
 *     22-Jul-2002	use of private myassert.h
 *     29-Jul-2002	reduce flicker by earlier graying save button after write
 *     15-Aug-2002	check of ViewOnly flag before inserting file (":r")
 *     12-Sep-2002	+EBCDIC quick hack (Christian Franke)
 *     25-Oct-2002	keep previous Interruptable flag after write operations
 *      8-Jan-2003	not asking for save if file only contains one CR or LF
 *     10-Jan-2003	configurable contents for new files (CR/LF, CR or LF)
 *      2-Jul-2007	new UTF encoding
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *     30-Apr-2010	improved UTF-16 recognition
 */

#define _WIN32_WINNT 0x0400	/*needed for OPENFILENAME definition in commdlg.h*/

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#include "myassert.h"
#include "winvi.h"
#include "map.h"
#include "page.h"

#if !defined(WIN32)
#	include <cderr.h>
#	include <dos.h>
#	include <direct.h>
#	include <ctype.h>
#	include <io.h>
#endif

#define TEST_DRIVE_IO FALSE

extern LONG   xOffset;
extern CHAR   QueryString[150];
extern DWORD  QueryTime;
extern BOOL   ReadOnlyParam;

OPENFILENAMEW Ofn = {0};
CHAR		  Buffer[300];
WCHAR		  BufferW[300];
WCHAR		  AltFileName[260];
static WCHAR  CustFilter[262] = L"\0*.*\0";
static WCHAR  CurrFileName[262] = L".\\";
static WCHAR  FileTitle[260];
static WCHAR  SmallBuf[40];
CONST WCHAR	  *CrLfAttribute = L"";
LONG		  AltFilePos;
UINT		  AltFileCset;
BOOL		  NotEdited = FALSE;
PWSTR		  Brackets = L"\253%s\273";
POSITION	  ReadPos;

void SetUnsafe(void)
{
	if (!Unsafe) {
		Unsafe = TRUE;
		EnableToolButton(IDB_SAVE, TRUE);
		GenerateTitle(TRUE);
	}
}

void SetSafe(BOOL FileChanged)
{
	if (Unsafe) {
		Unsafe = FALSE;
		GenerateTitle(TRUE);
		if (FileChanged) SetSafeForUndo(TRUE);
	}
}

#if defined(WIN32)
	FILETIME LastTime, TmpTime;
    #define _lclose(hf) CloseHandle((HANDLE)(hf))
#else
	OFSTRUCT LastOf,   TmpOf;
#endif

void CheckReadOnly(void)
{
	if (ReadOnlyParam || ReadOnlyFlag || ViewOnlyFlag) ReadOnlyFile = TRUE;
	else if (!NotEdited && *CurrFile) {
		#if defined(WIN32)
			HANDLE hf;

			if ((hf = CreateFileW(CurrFile, GENERIC_READ,
								  FILE_SHARE_READ | FILE_SHARE_WRITE,
								  NULL, OPEN_EXISTING,
								  FILE_ATTRIBUTE_NORMAL,
								  NULL)) != INVALID_HANDLE_VALUE) {
				BY_HANDLE_FILE_INFORMATION FileInfo;
				if (GetFileInformationByHandle(hf, &FileInfo))
					ReadOnlyFile = 0 !=
						(FileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY);
				CloseHandle(hf);
			}
		#else
			unsigned Attr;

			/*WARNING: CurrFile MUST be a pointer into DS!*/
			if (_dos_getfileattr(*(PSTR*)&CurrFile, &Attr)==0)
				if (Attr & _A_RDONLY) ReadOnlyFile = TRUE;
		#endif
	}
}

BOOL AskForSave(HWND hwndParent, WORD Flags)
	/*Flags: 1=check auto-write
	 *		 2=called from ":cd", different MessageBox if unsafe
	 */
{	INT			 i, c;
	PWSTR		 p;
	POSITION	 StartPos;
	static WCHAR Str[120];

	if (!Unsafe || CommandWithExclamation) return (TRUE);
	StartPos.p = FirstPage;
	StartPos.i = 0;
	StartPos.f = 0;
	c = CharAndAdvance(&StartPos);
	i = (c == C_EOF || c == C_CRLF || c == '\r' || c == '\n')
		&& CharAt(&StartPos) == C_EOF ? IDNO : IDCANCEL;
	CheckReadOnly();
	if (ReadOnlyFile) {
		LOADSTRINGW(hInst, (Flags & 2 ? 244 : 207) + (*CurrFile=='\0'),
				    Str, WSIZE(Str));
		p = _fcalloc(c = wcslen(Str) + wcslen(CurrFile) + 2, sizeof(WCHAR));
		if (p != NULL) {
			_snwprintf(p, c, Str, CurrFile);
			if (i != IDNO) {
			   i = MessageBoxW(hwndParent, p, L"WinVi",
							   MB_OKCANCEL | MB_ICONEXCLAMATION | MB_DEFBUTTON2);
			   if (i == IDOK) i = IDNO;
			}
		}
	} else {
		if (Flags&1 && AutoWriteFlag && *CurrFile!='\0')
			return (WriteCmd(-1, -1, L"", 0));
		LOADSTRINGW(hInst, 205 + (*CurrFile=='\0'), Str, WSIZE(Str));
		p = _fcalloc(c = wcslen(Str) + wcslen(CurrFile) + 2, sizeof(WCHAR));
		if (p != NULL) {
			_snwprintf(p, c, Str, CurrFile);
			if (i != IDNO)
				i = MessageBoxW(hwndParent, p, L"WinVi",
							    MB_YESNOCANCEL | MB_ICONQUESTION);
		}
	}
	if (p != NULL) _ffree(p);
	return (i==IDNO || (i==IDYES && WriteCmd(-1, -1, L"", 0)));
}

void LowerCaseFnameA(PSTR Fname)
{
	if (!LcaseFlag) return;
	CharLowerBuffA(Fname, strlen(Fname));
}

void LowerCaseFnameW(PWSTR Fname)
{
	if (!LcaseFlag) return;
	CharLowerBuffW(Fname, wcslen(Fname));
}

extern WCHAR FileNameBuf2[260];

void InitOfn(HWND hParent, WORD Flags)
	/*Flags: 1=Save dialog instead of open,
	 *		 2=Read command (no CREATEPROMPT)
	 */
{	static WCHAR AllFilter[30];

	if (!(Flags & 1)) *FileName = '\0';
	if (!*AllFilter) {
		LOADSTRINGW(hInst, 291, AllFilter, WSIZE(AllFilter));
		SeparateAllStringsW(AllFilter);
	}
	GetCurrentDirectoryW(WSIZE(FileNameBuf2), FileNameBuf2);
	Ofn.lStructSize 	  = sizeof(OPENFILENAME);
	Ofn.hwndOwner		  = hParent;
	Ofn.hInstance		  = hInst;
	Ofn.lpstrFilter 	  = AllFilter;
	Ofn.lpstrCustomFilter = CustFilter;
	Ofn.nMaxCustFilter	  = WSIZE(CustFilter);
	Ofn.nFilterIndex	  = 1;
	Ofn.lpstrFile		  = FileName;
	Ofn.nMaxFile		  = WSIZE(FileName);
	Ofn.lpstrFileTitle	  = FileTitle;
	Ofn.nMaxFileTitle	  = WSIZE(FileTitle);
	Ofn.lpstrInitialDir	  = FileNameBuf2;	/*new: explicit dir specification*/
											/*    required in WinMe and WinXP*/
	if (Flags & 2) {
		static WCHAR Title[30];

		if (!*Title) LOADSTRINGW(hInst, 280, Title, WSIZE(Title));
		Ofn.lpstrTitle	  = *Title ? Title : (PWSTR)NULL;
	} else Ofn.lpstrTitle = NULL;
	if (Flags & 1)
		Ofn.Flags		  = OFN_HIDEREADONLY  | OFN_OVERWRITEPROMPT;
	else {
		Ofn.Flags = Flags & 2
					? OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR
					: OFN_PATHMUSTEXIST | OFN_CREATEPROMPT;
		if (ReadOnlyFlag || ViewOnlyFlag) Ofn.Flags |= OFN_READONLY;
	}
	Ofn.lpstrDefExt 	  = L"";
}

void SetAltFileName(PCWSTR FName, PPOSITION Pos)
{
	wcscpy(AltFileName, FName);
	AltFilePos  = Pos ? CountBytes(Pos) : 0;
	AltFileCset = CharSet;
}

void WatchMessage(PWSTR FileName)
{
	#if defined(WIN32)
		WCHAR Message[340], Format[80];

		if (!LOADSTRINGW(hInst, 253, Format, WSIZE(Format)))
			_snwprintf(Format, WSIZE(Format), L"%s", L"Modified on disk: %s");
		_snwprintf(Message, WSIZE(Message), Format, FileName);
		MessageBoxW(hwndMain, Message, L"WinVi", MB_OK | MB_ICONEXCLAMATION);
	#endif
}

#if defined(WIN32)

static WCHAR  WatchName[MAX_PATH+2];
static HANDLE WatchEvents[2] = {INVALID_HANDLE_VALUE, 0};
							   /*0: ChangeNotification, 1:QuitEvent*/

DWORD WINAPI WatchFileThread(LPVOID Unused)
{	PWSTR	 p1, p2;
	FILETIME FileTime;
	BOOL	 Done;

	PARAM_NOT_USED(Unused);
	p1 = wcsrchr(WatchName, '\\');
	p2 = wcsrchr(WatchName, '/' );
	if (p1 != NULL || p2 != NULL) {
		if (p1 == NULL || (p2 != NULL && p2 > p1)) p1 = p2;
		*p1++ = '\0';
	} else {
		memmove(p1=WatchName+2, WatchName, 2*wcslen(WatchName)+2);
		WatchName[0] = '.';
		WatchName[1] = '\0';
	}
	WatchEvents[0] = FindFirstChangeNotificationW(WatchName, FALSE,
					   FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
	if (WatchEvents[0] == INVALID_HANDLE_VALUE) return (1);
	p1[-1] = '\\';
	Done = FALSE;
	do {
		HANDLE hFile;
		DWORD  WaitResult;
		INT    i;

		WaitResult = WaitForMultipleObjects(2, WatchEvents, FALSE, INFINITE);
		switch (WaitResult) {
			case WAIT_OBJECT_0:
				/*directory modified*/
				break;
			default:
				/*quit signal or error*/
				Done = TRUE;
				break;
		}
		if (Done) break;
		for (i=0; i<3; ++i) {
			hFile = CreateFileW(WatchName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
							    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
							    NULL);
			if (hFile != INVALID_HANDLE_VALUE) break;
			Sleep(200);	/*try again after 200 ms...*/
		}
		if (hFile != INVALID_HANDLE_VALUE) {
			BOOL Ok = GetFileTime(hFile, NULL, NULL, &FileTime);

			CloseHandle(hFile);
			if (Ok && (FileTime.dwLowDateTime  != LastTime.dwLowDateTime ||
					   FileTime.dwHighDateTime != LastTime.dwHighDateTime)) {
				static BOOL FlashState;

				FlashState = FALSE;
				while (!CaretActive) {
					INT i;

					FlashWindow(hwndMain, FlashState ^= TRUE ^ FALSE);
					for (i=0; i<5 && !CaretActive; ++i) Sleep(100);
				}
				if (FlashState) FlashWindow(hwndMain, FlashState = FALSE);
				PostMessage(hwndMain, WM_USER+1236, 0, (LPARAM)p1);
				break;
			}
			Sleep(200);	/*avoid drawbacks from change notification bursts*/
		}
	} while (FindNextChangeNotification(WatchEvents[0]));
	FindCloseChangeNotification(WatchEvents[0]);
	ExitThread(0);		/*...not necessary to warn once more*/
	return (0);
}

#endif

BOOL WatchFile(const WCHAR *FileName)
{
	#if defined(WIN32)
		static HANDLE hThread = 0;
		static DWORD  dwThreadId;

		if (WatchEvents[1] == 0) {
			WatchEvents[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (WatchEvents[1] == 0) return (FALSE);
		}
		if (hThread != 0) {
			SetEvent(WatchEvents[1]);
			if (WaitForSingleObject(hThread, 2000) == WAIT_TIMEOUT)
				TerminateThread(hThread, 2);
			CloseHandle(hThread);
		}
		ResetEvent(WatchEvents[1]);
		if (FileName == NULL ||
				(LastTime.dwHighDateTime==0 && LastTime.dwLowDateTime==0)) {
			hThread = 0;
			return (TRUE);
		}
		wcsncpy(WatchName, FileName, WSIZE(WatchName) - 2);
		hThread = CreateThread(NULL, 1024, WatchFileThread, (LPVOID)NULL,
							   0, &dwThreadId);
		return (hThread != 0);
	#else
		return (FALSE);
	#endif
}

INT		   CountedDefaultNl = 1;	/*1=CR/LF, 2=LF, 3=CR*/
extern INT SelectDefaultNewline;
extern INT SelectDefaultCharSet;
extern INT DefaultNewLine;

VOID SetDefaultNl(VOID)
{	CONST INT	DefaultNls[3]   = {C_CRLF, '\n',	 '\r'};
	CONST WCHAR	*NlIndicator[3] = {L"",	   L" [Lf]", L" [Cr]"};
	INT			Nl;

	Nl = (SelectDefaultNewline ? SelectDefaultNewline : CountedDefaultNl) - 1;
	if (DefaultNewLine != DefaultNls[Nl]) {
		DefaultNewLine = DefaultNls [Nl];
		CrLfAttribute  = NlIndicator[Nl];
		InvalidateText();
	}
}

void New(HWND hParent, BOOL SaveAltFile)
{
	PARAM_NOT_USED(hParent);
	if (!AskForSave(hwndMain, 0)) return;
	Unsafe = FALSE;
	EnableToolButton(IDB_SAVE, FALSE);
	DisableUndo();
	NewStatus(0, L"", NS_NORMAL);
	if (SaveAltFile && CurrFile && *CurrFile)
		SetAltFileName(CurrFile, &CurrPos);
	CurrFileName[2] = '\0';
	CurrFile = CurrFileName+2;
	NotEdited = FALSE;
	FreeAllPages();
	SwitchToMatchingProfile(L"");
	if (SelectDefaultNewline > 1) {
		FirstPage->PageBuf[0] = DefaultNewLine;
		FirstPage->Fill = 1;
	} else {
		lstrcpy(FirstPage->PageBuf, "\r\n");
		FirstPage->Fill = 2;
	}
	FirstPage->Newlines = 1;
	LinesInFile = 1;
	if (UtfEncoding) {
		if (UtfEncoding == 8) {
			FirstPage->PageBuf[3] = FirstPage->PageBuf[0];
			FirstPage->PageBuf[4] = FirstPage->PageBuf[1];
			FirstPage->PageBuf[0] = 0xef;
			FirstPage->PageBuf[1] = 0xbb;
			FirstPage->PageBuf[2] = 0xbf;
			FirstPage->Fill += 3;
		} else if (UtfLsbFirst) {
			FirstPage->PageBuf[2] = FirstPage->PageBuf[0];
			FirstPage->PageBuf[4] = FirstPage->PageBuf[1];
			FirstPage->PageBuf[0] = C_BOM & 0xff;
			FirstPage->PageBuf[1] = C_BOM >> 8;
			FirstPage->PageBuf[3] =
			FirstPage->PageBuf[5] = '\0';
			FirstPage->Fill += FirstPage->Fill + 2;
		} else {
			FirstPage->PageBuf[3] = FirstPage->PageBuf[0];
			FirstPage->PageBuf[5] = FirstPage->PageBuf[1];
			FirstPage->PageBuf[0] = C_BOM >> 8;
			FirstPage->PageBuf[1] = C_BOM & 0xff;
			FirstPage->PageBuf[2] =
			FirstPage->PageBuf[4] = '\0';
			FirstPage->Fill += FirstPage->Fill + 2;
		}
	} else if (CharSet == CS_EBCDIC) {
		FirstPage->PageBuf[0] = MapAnsiToEbcdic[FirstPage->PageBuf[0]];
		FirstPage->PageBuf[1] = MapAnsiToEbcdic[FirstPage->PageBuf[1]];
	}
	CurrCol.ScreenLine = CurrCol.PixelOffset = 0;
	if (!(CountedDefaultNl = SelectDefaultNewline)) CountedDefaultNl = 1;
	SetDefaultNl();
	ReadOnlyFile = ReadOnlyFlag || ViewOnlyFlag;
	BeginOfFile(&CurrPos);
	ScreenStart = CurrPos;
	InvalidateText();
	GenerateTitle(TRUE);
	NewPosition(&CurrPos);	/*for updating line/col status fields*/
	NewScrollPos();
	WatchFile(NULL);
	ShowEditCaret();
}

void ShowFileName(void)
{
	*BufferW = Brackets[0];
	if (*CurrFile) wcscpy(BufferW+1, CurrFile);
	else LOADSTRINGW(hInst, 901, BufferW+1, WSIZE(BufferW) - 2);
	wcscat(BufferW, Brackets+3);
	CheckReadOnly();
	if (ReadOnlyFile) {
		LOADSTRINGW(hInst, 282, SmallBuf, WSIZE(SmallBuf));
		wcscat(BufferW, SmallBuf);
	}
	if (NotEdited)	{
		LOADSTRINGW(hInst, 283, SmallBuf, WSIZE(SmallBuf));
		wcscat(BufferW, SmallBuf);
	}
	if (Unsafe) {
		LOADSTRINGW(hInst, 284, SmallBuf, WSIZE(SmallBuf));
		wcscat(BufferW, SmallBuf);
	}
	wcscat(BufferW, CrLfAttribute);
	LOADSTRINGW(hInst, 288, SmallBuf, WSIZE(SmallBuf));
	_snwprintf(BufferW + wcslen(BufferW), WSIZE(BufferW) - wcslen(BufferW),
			   SmallBuf,
			   CountLines(&CurrPos) + 1, LinesInFile + IncompleteLastLine());
	NewStatus(0, BufferW, NS_NORMAL);
}

void FileError(PCWSTR FName, INT DosErrorCode, INT Range)
{	static WCHAR ErrStr[60];

	if (DosErrorCode > 0 && DosErrorCode <= Range &&
			LOADSTRINGW(hInst, DosErrorCode+400, ErrStr, WSIZE(ErrStr)) &&
			LOADSTRINGW(hInst, 114, SmallBuf, WSIZE(SmallBuf)))
		_snwprintf(BufferW, WSIZE(BufferW), SmallBuf, FName, ErrStr);
	else {
		if (!LOADSTRINGW(hInst, 115, SmallBuf, WSIZE(SmallBuf)))
			wcscpy(SmallBuf, L"%s: file error %d");
		_snwprintf(BufferW, WSIZE(BufferW), SmallBuf, FName, DosErrorCode);
	}
	NewStatus(0, BufferW, NS_ERROR);
}

#if defined(WIN32)

static void File32Error(PCWSTR FName)
{	PWSTR ErrorMessage = NULL;
	DWORD ErrorCode    = GetLastError();

	if (LOADSTRINGW(hInst, 114, SmallBuf, WSIZE(SmallBuf))
		&& FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
						  FORMAT_MESSAGE_FROM_SYSTEM,
						  NULL, ErrorCode,
						  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						  (LPWSTR)&ErrorMessage, 0, NULL) &&
		ErrorMessage) {
			PWSTR p;
			INT   MaxLen = WSIZE(BufferW) - wcslen(FName) - wcslen(SmallBuf);

			if ((INT)wcslen(ErrorMessage) > MaxLen) ErrorMessage[MaxLen] = '\0';
			while ((p = wcschr(ErrorMessage, '\r')) != NULL ||
				   (p = wcschr(ErrorMessage, '\n')) != NULL)
				*p = (p[1]=='\n' && p[2]=='\0') || p[1]=='\0' ? '\0' : ' ';
			_snwprintf(BufferW, WSIZE(BufferW),
					   SmallBuf, (LPCSTR)FName, (LPSTR)ErrorMessage);
			LocalFree(ErrorMessage);
	}
	if (ErrorMessage == NULL) {
		*SmallBuf = '\0';
		if (ErrorCode == ERROR_BAD_PATHNAME)
			LOADSTRINGW(hInst, 116, SmallBuf, WSIZE(SmallBuf));
		if (!*SmallBuf && !LOADSTRINGW(hInst, 115, SmallBuf, WSIZE(SmallBuf)))
			wcscpy(SmallBuf, L"%s: file error %d");
		_snwprintf(BufferW, WSIZE(BufferW), SmallBuf, FName, ErrorCode);
	}
	NewStatus(0, BufferW, NS_ERROR);
}

#endif

VOID JumpAbsolute(ULONG SkipBytes)
{
	CurrPos.p = FirstPage;
	if (SkipBytes) {
		do {
			if (SkipBytes < CurrPos.p->Fill) {
				CurrPos.i = (INT)SkipBytes;
				break;
			}
			SkipBytes -= CurrPos.p->Fill;
			CurrPos.p  = CurrPos.p->Next;
			if (CurrPos.p == NULL) {
				CurrPos.p = LastPage;
				CurrPos.i = CurrPos.p->Fill;
				break;
			}
		} while (SkipBytes);
		FindValidPosition(&CurrPos, 4);
	}
	Jump(&CurrPos);
}

#if TEST_DRIVE_IO
INT MyRead(INT hFile, PBYTE Buf, INT Size)
{	BYTE  pw[8192], *p;
	DWORD n = 0;

	if (Size != 4096)
		return _lread(hFile, Buf, Size);
	p = (PBYTE)(((LONG)pw + 4095) & ~4095);
	if (!ReadFile((HANDLE)hFile, p, 4096, &n, NULL)) return -1;
	if (n > 0) memcpy(Buf, p, n);
	return n;
}
#else
#define MyRead(h,b,n) _lread(h,b,n)
#endif

static ULONG SkipBytes;
extern BOOL  SuppressPaint;
extern INT	 nCurrFileListEntry;

BOOL OpenFileName(BOOL DontCreate)
{
	InitOfn(hwndMain, (WORD)(DontCreate ? 2 : 0));
	StopMap(TRUE);
	if (!GetOpenFileNameW(&Ofn)) {
		static WCHAR Buf[40];
		DWORD		 dwError;

		switch (dwError = CommDlgExtendedError()) {
			case 0:  /*normal cancel*/
				break;
			default: /*error*/
				if (FreeSpareMemory()) break;
				if (!LOADSTRINGW(hInst, 290, SmallBuf, WSIZE(SmallBuf)))
					wcscpy(SmallBuf, L"Unexpected error %04lx");
				_snwprintf(Buf, WSIZE(Buf), SmallBuf, dwError);
				MessageBoxW(hwndMain, Buf, NULL, MB_OK | MB_ICONSTOP);
		}
		return (FALSE);
	}
	LowerCaseFnameW(FileName);
	return (TRUE);
}

BOOL Open(PCWSTR FName, WORD Flags)
	/* Flags: 1=read and insert into current file (command ":read")
	 */
{	INT  hf, i;
	LONG NlCnt[9];
	UINT AltCset = AltFileCset;
	BOOL ReadPosAfterScreenStart, LowerCaseFlag;
	BOOL IsReadOnly = ReadOnlyParam || ReadOnlyFlag || ViewOnlyFlag;

	if (FName != NULL && wcslen(FName) >= MAX_PATH) {
		Error(411);
		return (FALSE);
	}
	memset(NlCnt, 0, sizeof(NlCnt));
	if (Flags & 1) {
		if (!FName || !*FName) {
			if (ViewOnlyFlag) {
				Error(236);
				return (FALSE);
			}
			if (!OpenFileName(TRUE)) return (FALSE);
		} else wcscpy(FileName, FName);
		FName = FileName;
		if (!wcschr(FileName, '\\') && !wcschr(FileName, '/')) {
			FName += 2;
			memmove(FileName+2, FileName, 2*wcslen(FileName)+2);
			FileName[0] = '.';
			FileName[1] = '\\';
		}
		SetAltFileName(FName, NULL);
		if (ViewOnlyFlag) {
			Error(236);
			return (FALSE);
		}
	} else {
		MoreFilesWarned = FALSE;
		if (ExecRunning()) {
			Error(236);
			if (*FName) SetAltFileName(FName, NULL);
			return (FALSE);
		}
		if (!AskForSave(hwndMain, 1)) {
			if (*FName) SetAltFileName(FName, NULL);
			return (FALSE);
		}
		LowerCaseFlag = !*FName;
		if (LowerCaseFlag) {
			if (!OpenFileName(FALSE)) {
				#if defined(WIN32)
					SetCurrentDirectoryW(FileNameBuf2);
				#else
					isalpha(FileNameBuf2[0]) && FileNameBuf2[1]==':'
						? _chdir(FileNameBuf2) || _chdrive(*FileNameBuf2 & 31)
						: _chdir(FileNameBuf2);
				#endif
				return (FALSE);
			}
			IsReadOnly = (Ofn.Flags & OFN_READONLY) != 0;
			nCurrFileListEntry = FindOrAppendToFileList(FileName);
			FName = FileName;
		}
		Unsafe = NotEdited = FALSE;
		EnableToolButton(IDB_SAVE, FALSE);
		DisableUndo();
		if (FName != CurrFile && (CurrFileName[2] || IsAltFile))
			SetAltFileName(CurrFileName+2, &CurrPos);
		wcscpy(CurrFileName+2, FName);
		if (!LowerCaseFlag) LowerCaseFnameW(CurrFileName+2);
		CurrFile = CurrFileName+2;
		GenerateTitle(TRUE);
		FName = CurrFile;
		if (CurrFile[1] != ':' && !wcschr(CurrFile, '\\') &&
								  !wcschr(CurrFile, '/'))
			CurrFile = CurrFileName;
	}
	DragAcceptFiles(hwndMain, FALSE);
	i = _snwprintf(BufferW, WSIZE(BufferW), Brackets, FName);
	LOADSTRINGW(hInst, 286, BufferW+i, WSIZE(BufferW)-i);
	NewStatus(0, BufferW, NS_BUSY);
	if (Flags & 1) {
		#if defined(WIN32)
			hf = (HFILE)
				 CreateFileW(FileName, GENERIC_READ,
				 			 FILE_SHARE_READ | FILE_SHARE_WRITE,
							 NULL, OPEN_EXISTING,
							 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
							 NULL);
		#else
			hf = OpenFile(FileName, &TmpOf, OF_READ);
		#endif
	} else {
		WatchFile(NULL);
		FreeAllPages();
		UtfEncoding = 0;
		UtfLsbFirst = FALSE;
		SwitchToMatchingProfile(FName);
		SetCharSet(SelectDefaultCharSet, 2);
		#if defined(WIN32)
			hf = (HFILE)
				 CreateFileW(CurrFile, GENERIC_READ,
				 			 FILE_SHARE_READ | FILE_SHARE_WRITE,
							 NULL, OPEN_EXISTING,
#if TEST_DRIVE_IO
							 wcslen(CurrFile) == 6 ? FILE_FLAG_NO_BUFFERING :
#endif
							 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
							 NULL);
		#else
			hf = OpenFile(CurrFile, &LastOf, OF_READ);
		#endif
	}
	if (hf != HFILE_ERROR) do {		/*no loop, for break only*/
		LPPAGE Page, NewPg;
		BOOL   IsFirst = TRUE;
		DWORD  CountFlags = 0;
		INT	   PageNo;
		LONG   Length  = _llseek(hf, 0L, 2);
		LONG   ToRead  = Length;
		LONG   FileNls = 0, PageNls, ReadNls;

#if TEST_DRIVE_IO
		if (wcslen(CurrFile) == 6 && wcsncmp(CurrFile, L"\\\\.\\", 4) == 0
								  && CurrFile[5] == ':')
			ToRead = Length = 4096;
#endif
		_llseek(hf, 0L, 0);
		if (Flags & 1) {
			CurrPos = ReadPos;
			ReadPosAfterScreenStart = ComparePos(&ReadPos, &ScreenStart) > 0;
			if (CurrPos.i) {
				if (CurrPos.i < CurrPos.p->Fill) {
					CharAt(&CurrPos);
					Page = NewPage();
					if (Page == NULL) {
						_lclose(hf);
						Error(313);
						DragAcceptFiles(hwndMain, TRUE);
						return (FALSE);
					}
					assert(Page->PageBuf);
					assert(CurrPos.p->PageBuf);
					_fmemcpy(Page->PageBuf, CurrPos.p->PageBuf + CurrPos.i,
							 Page->Fill  =  CurrPos.p->Fill    - CurrPos.i);
					CurrPos.p->Fill = CurrPos.i;
					Page->Next = CurrPos.p->Next;
					if (Page->Next != NULL) Page->Next->Prev = Page;
					else LastPage = Page;
					CurrPos.p->Next = Page;
					Page->Prev = CurrPos.p;
					Page->Newlines = CurrPos.p->Newlines;
					CurrPos.p->Newlines -=
						CountNewlinesInBuf(Page->PageBuf, Page->Fill, NULL,
										   NULL /*TODO: check prev char*/);
				}
				PageNls = CurrPos.p->Newlines;
			} else PageNls = CurrPos.p->Prev ? CurrPos.p->Prev->Newlines : 0;
			PageNo = CurrPos.p->PageNo;
			if (Length) {
				Page = NewPage();
				if (Page == NULL) {
					_lclose(hf);
					Error(313);
					DragAcceptFiles(hwndMain, TRUE);
					return (FALSE);
				}
				Page->Fill = 0;
				if (CurrPos.i) {
					Page->Next = CurrPos.p->Next;
					if (Page->Next != NULL) Page->Next->Prev = Page;
					else LastPage = Page;
					CurrPos.p->Next = Page;
					Page->Prev = CurrPos.p;
				} else {
					--PageNo;
					Page->Prev = CurrPos.p->Prev;
					if (Page->Prev != NULL) Page->Prev->Next = Page;
					else FirstPage = Page;
					CurrPos.p->Prev = Page;
					Page->Next = CurrPos.p;
				}
				CurrPos.p = Page;
				CurrPos.i = 0;
			} else Page = CurrPos.p;
		} else {
			#if defined(WIN32)
				GetFileTime((HANDLE)hf, NULL, NULL, &LastTime);
				WatchFile(CurrFile);
			#endif
			if (!IsReadOnly) {
				#if defined(WIN32)
					BY_HANDLE_FILE_INFORMATION FileInfo;
					if (GetFileInformationByHandle((HANDLE)hf, &FileInfo) &&
							FileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
						IsReadOnly = TRUE;
				#else
					unsigned Attr;
					/*WARNING: CurrFile MUST be a pointer into DS!*/
					if (_dos_getfileattr(*(PSTR*)&CurrFile, &Attr)==0)
						if (Attr & _A_RDONLY) IsReadOnly = TRUE;
				#endif
			}
			ReadOnlyFile = IsReadOnly;
			LinesInFile = 0;
			if (IsAltFile) {
				SetCharSet(AltCset, 2);
				UpdateWindow(hwndMain);
			}
			PageNls = PageNo = 0;
			Page = FirstPage;
			CurrFile = CurrFileName+2;
		}
		xOffset = 0;
		Interrupted = FALSE;
		Disable(Length > 50000L);
		SuppressPaint = TRUE;
		while (ToRead > 0) {
			INT len;

			if (IsFirst) NewPg = Page;	/*used as error flag*/
			else {
				NewPg = NewPage();
				if (NewPg == NULL) break;
				NewPg->Next = Page->Next;
				if (NewPg->Next != NULL) NewPg->Next->Prev = NewPg;
				else LastPage = NewPg;
				Page->Next = NewPg;
				NewPg->Prev = Page;
				Page = NewPg;
			}
			Page->PageBuf[0] = '\r';
			Page->PageNo = ++PageNo;
			len = MyRead(hf, Page->PageBuf, PAGESIZE);
			if (len <= 0) {
				/*read error*/
				Page->Fill = 0;
				Page->Newlines = PageNls;
				break;
			}
			ToRead -= len;
			Page->Fill = len;
			if (IsFirst) {
				IsFirst = FALSE;
				if (!(Flags & 1)) {
					INT skip = 0;

					switch (SelectDefaultCharSet) {
						case CS_AUTOENCODING:
							if (len >= 2 && Page->PageBuf[0] == 0xfe
										 && Page->PageBuf[1] == 0xff) {
								UtfEncoding	= 16;
								skip		= 2;
							}
							if (len >= 2 && Page->PageBuf[0] == 0xff
										 && Page->PageBuf[1] == 0xfe) {
								UtfEncoding = 16;
								UtfLsbFirst = TRUE;
								skip		= 2;
							} else if (len >= 3 && Page->PageBuf[0] == 0xef
												&& Page->PageBuf[1] == 0xbb
												&& Page->PageBuf[2] == 0xbf) {
								UtfEncoding = 8;
								skip		= 3;
							}
							break;

						case CS_UTF8:
							if (len >= 3 && Page->PageBuf[0] == 0xef
										 && Page->PageBuf[1] == 0xbb
										 && Page->PageBuf[2] == 0xbf)
								skip		= 3;
							break;

						case CS_UTF16LE:
							if (len >= 2 && Page->PageBuf[0] == 0xff
										 && Page->PageBuf[1] == 0xfe)
								skip		= 2;
							break;

						case CS_UTF16BE:
							if (len >= 2 && Page->PageBuf[0] == 0xfe
										 && Page->PageBuf[1] == 0xff)
								skip		= 2;
							break;
					}
					if (!HexEditMode) ScreenStart.i = CurrPos.i = skip;
				}
			}
			FileNls += ReadNls = CountNewlinesInBuf(Page->PageBuf, len, NlCnt,
													&CountFlags);
			Page->Newlines = PageNls += ReadNls;
			MessageLoop(FALSE);
			if (!hwndMain) {
				_lclose(hf);
				DragAcceptFiles(hwndMain, TRUE);
				return (FALSE);
			}
			if (Interrupted) {
				#if 0
				//	if (MessageBox(hwndMain,"Continue?","Interrupted",MB_YESNO)
				//		== IDNO) break;
				//	Interrupted = FALSE;
				#endif
				break;
			}
		}
		SuppressPaint = FALSE;
		Length -= ToRead;
		_lclose(hf);
		if (!UtfEncoding && SelectDefaultCharSet == CS_AUTOENCODING) {
			if (CountFlags & CNLB_UTF8_COUNT) {
				UtfEncoding = 8;
			}
		}
		_snwprintf(BufferW, WSIZE(BufferW), Brackets, FName);
		if (!(Flags & 1)) {
			if (ReadOnlyFile && !ExecRunning()) {
				LOADSTRINGW(hInst, 282, SmallBuf, WSIZE(SmallBuf));
				wcscat(BufferW, SmallBuf);
			}
			CountedDefaultNl = NlCnt[0] > 0		   ? 1 :
							   NlCnt[1] > NlCnt[2] ? 3 :
							   NlCnt[2] > 0		   ? 2 : 1;
			SetDefaultNl();
			wcscat(BufferW, CrLfAttribute);
			if (SelectDefaultCharSet == CS_AUTOENCODING)
				if (!UtfEncoding) {
					if (NlCnt[6] == 0 && (Length & 1) == 0
									  && (NlCnt[4] + NlCnt[5] > Length/500+2)) {
						UtfEncoding = 16;
						UtfLsbFirst = (NlCnt[4] <= NlCnt[5]);
					}
				} else if (UtfEncoding == 16 && (Length & 1) != 0) {
					/* UTF-16 files must not have an odd number of bytes... */
					UtfEncoding = 0;
					UtfLsbFirst = FALSE;
				}
		}
		if (TerseFlag) wcscpy(SmallBuf, L" %ld/%ld");
		else LOADSTRINGW(hInst, 289, SmallBuf, WSIZE(SmallBuf));
		_snwprintf(BufferW + wcslen(BufferW), WSIZE(BufferW) - wcslen(BufferW),
				   SmallBuf, FileNls, Length);
		NewStatus(0, BufferW, NS_NORMAL);
		if (ToRead > 0) {
			if (NewPg == NULL) {
				/*serious error, error box already confirmed*/
				New(hwndMain, FALSE);
				Page = FirstPage;
			} else if (Interrupted) {
				LOADSTRINGW(hInst, 218, BufferW, WSIZE(BufferW));
				NewStatus(0, BufferW, NS_ERROR);
			} else MessageBox(hwndMain, "Read Error", NULL, MB_OK|MB_ICONSTOP);
			if (!(Flags & 1)) NotEdited = TRUE;
		}
		while ((Page = Page->Next) != NULL) {
			Page->Newlines += FileNls;
			Page->PageNo = ++PageNo;
		}
		LinesInFile += FileNls;
		if (Flags & 1) EnterInsertForUndo(&CurrPos, Length);
		else {
			AddToFileList(FName);
			SwitchToMatchingProfile(FName);
		}
	} while (FALSE);
	else if (Flags & 1) {
		#if defined(WIN32)
			File32Error(FName);
		#else
			FileError(FName, TmpOf.nErrCode, 88);
		#endif
	} else {
		#if defined(WIN32)
			DWORD ErrorValue = GetLastError();
		#else
			DWORD ErrorValue = LastOf.nErrCode;
		#endif

		NotEdited = TRUE;
		CurrPos.p->Fill = 2;
		lstrcpy(CurrPos.p->PageBuf, "\r\n");
		LinesInFile = 1;
		DefaultNewLine = C_CRLF;
		CrLfAttribute = L"";
		CurrFile = CurrFileName+2;
		if (DefaultNewLine != C_CRLF) {
			*CurrPos.p->PageBuf = DefaultNewLine;
			--CurrPos.p->Fill;
		}
		if (ErrorValue == 2) {
			i = _snwprintf(BufferW, WSIZE(BufferW), Brackets, (LPSTR)CurrFile);
			LOADSTRINGW(hInst, 285, BufferW+i, WSIZE(BufferW)-i);
			i += wcslen(BufferW+i);
			ReadOnlyFile = IsReadOnly;
			if (ReadOnlyFile && !ExecRunning())
				LOADSTRINGW(hInst, 282, BufferW+i, WSIZE(BufferW)-i);
			wcscat(BufferW, CrLfAttribute);
			NewStatus(0, BufferW, NS_NORMAL);
		} else {
			#if defined(WIN32)
				File32Error(CurrFile);
			#else
				FileError(CurrFile, LastOf.nErrCode, 88);
			#endif
		}
		SwitchToMatchingProfile(CurrFile);
	}
	if (Flags & 1) {
		if (ReadPosAfterScreenStart && GoBackAndChar(&CurrPos) != C_EOF) {
			InvalidateArea(&CurrPos, 1, 15);
			UpdateWindow(hwndMain);
			CharAndAdvance(&CurrPos);
		} else Jump(&CurrPos);
	} else JumpAbsolute(SkipBytes);
	NewScrollPos();
	if (Disabled) Enable();
	if (Mode != CommandMode && !DefaultInsFlag) {
		GotChar('\033', 1);
		//Mode = CommandMode;
		//SwitchCursor(SWC_TEXTCURSOR);
	}
	if (!HexEditMode) Position(1, '^', 0);	/*position to first non-space*/
	else {
		NewPosition(&CurrPos);
		ShowEditCaret();
	}
	DragAcceptFiles(hwndMain, TRUE);
	return (hf != HFILE_ERROR);
}

BOOL EditCmd(PCWSTR FName)
{	BOOL Ok;

	SkipBytes = IsAltFile ? AltFilePos : 0;
	if (!FName || !*FName) FName = CurrFile;
	Ok = Open(FName, 0);
	SkipBytes = 0;
	return (Ok);
}

BOOL ReadCmd(LONG Line, PCWSTR FName)
{
	StartUndoSequence();
	if (Line != -1) {
		ReadPos.i = 0;
		ReadPos.p = FirstPage;
	} else {
		ReadPos = CurrPos;
		if (HexEditMode) {
			/*read at current position...*/
			if (Mode != InsertMode) Advance(&ReadPos, 1);
			Line = 0;
		} else Line = 1;
	}
	while (Line--) {
		INT c;

		do; while (!((c = CharFlags(CharAndAdvance(&ReadPos))) & 1));
		if (c & 0x20) break;
	}
	if (!Open(FName, 1)) return (FALSE);
	SetUnsafe();
	return (TRUE);
}

BOOL NextCmd(PCWSTR FNames)
{	extern INT nCurrFileListEntry;

	while (*FNames == ' ') ++FNames;
	if (*FNames) {
		if (!AskForSave(hwndMain, 1)) return (FALSE);
		ClearFileList();
		do {
			PCWSTR lp = FNames;

			if (*FNames == '"') {
				++FNames;
				do; while (*++lp && *lp != '"');
			} else while (*lp && *lp != ' ') ++lp;
			AppendToFileList(FNames, lp-FNames, 1);
			if (*lp == '"') ++lp;
			while (*lp == ' ') ++lp;
			FNames = lp;
		} while (*FNames);
		FNames = GetFileListEntry(nCurrFileListEntry);
		if (FNames == NULL) return (FALSE);
	} else {
		FNames = GetFileListEntry(nCurrFileListEntry+1);
		if (FNames == NULL) {
			/*error: no more files*/
			Error(220);
			return (FALSE);
		}
		if (!AskForSave(hwndMain, 1)) return (FALSE);
		++nCurrFileListEntry;
	}
	Unsafe = FALSE;		/*already asked for save*/
	Open(FNames, 0);
	return (TRUE);
}

void FilenameCmd(PWSTR FName)
{
	if (*FName) {
		if (wcslen(FName) >= MAX_PATH) {
			Error(411);
			return;
		}
		if (CurrFile && *CurrFile) SetAltFileName(CurrFile, NULL);
		wcscpy(CurrFileName+2, FName);
		CurrFile = CurrFileName + 2;
		GenerateTitle(TRUE);
		EnableToolButton(IDB_SAVE, TRUE);
		NotEdited = TRUE;
		SwitchToMatchingProfile(FName);
		WatchFile(NULL);
	}
	ShowFileName();
}

void Save(PCWSTR FName)
{
	WriteCmd(-1, -1, FName, 0);
}

#if defined(WIN32)
DWORD TimeDiff(FILETIME *q1, FILETIME *q2)
{
	if (q1->dwHighDateTime == q2->dwHighDateTime)
		return (q1->dwLowDateTime >= q2->dwLowDateTime
			  ? q1->dwLowDateTime -  q2->dwLowDateTime
			  : q2->dwLowDateTime -  q1->dwLowDateTime);
	if (q1->dwHighDateTime == q2->dwHighDateTime+1)
		if (q1->dwLowDateTime < q2->dwLowDateTime)
			return (q1->dwLowDateTime - q2->dwLowDateTime);
	if (q2->dwHighDateTime == q1->dwHighDateTime+1)
		if (q2->dwLowDateTime < q1->dwLowDateTime)
			return (q2->dwLowDateTime - q1->dwLowDateTime);
	return (0xffffffff);
}
#endif

BOOL SetSafeAfterWrite;

BOOL WriteCmd(LONG StartLine, LONG EndLine, PCWSTR FName, WORD Flags)
	/*Flags: 1=Write command with '!', don't ask for overwrite
	 */
{	POSITION StartPos, EndPos;
	INT      c = 0;
	BOOL     KeepEnabled = FALSE, AskIfExists = TRUE;
	BOOL     IsCurrFile  = FALSE, Ret = TRUE;
	BOOL	 SetReadOnly = FALSE, ReadOnlyError;
	BOOL	 SavInterruptable = Interruptable;
	PCWSTR   Name = FName;
	LONG     Newlines, Length;
	#if WRITE_WITH_CREAT
		INT	 OpenMode = OF_WRITE | OF_CREATE;
	#else
		INT	 OpenMode = OF_WRITE | OF_REOPEN | OF_VERIFY;
	#endif

	if (FName != NULL && wcslen(FName) >= MAX_PATH) {
		Error(411);
		return (FALSE);
	}
	SetSafeAfterWrite = TRUE;	/*may also be altered by shell execution*/
	if (!Name) KeepEnabled = TRUE;
	else if (!*Name) {
		Name = CurrFile;
		IsCurrFile  = TRUE;
		AskIfExists = NotEdited;
	} else {
		KeepEnabled = TRUE;
		if (Name[0]=='>' && Name[1]=='>') {
			++Name;
			do; while (*++Name == ' ');
			if (!*Name) {
				Name = CurrFile;
				IsCurrFile = TRUE;
			}
			AskIfExists = FALSE;
			OpenMode = OF_WRITE;
		}
	}
	if (Flags & 1) AskIfExists = FALSE;
	HideEditCaret();
	if (!Name || !*Name) {
		DWORD dwError = 0;

		wcscpy(FileName, CurrFileName+2);
		InitOfn(hwndMain, 1);
		StopMap(TRUE);
		while (!GetSaveFileNameW(&Ofn)) {
			static WCHAR Buf[40];
			DWORD		 LastError = dwError;

			switch (dwError = CommDlgExtendedError()) {
				case 0:	/*normal cancel*/
					break;

				case FNERR_INVALIDFILENAME:
					if (!LastError) {
						*FileName = '\0';
						continue;
					}
					/*FALLTHROUGH*/
				default:
					if (FreeSpareMemory()) continue;
					if (!LOADSTRINGW(hInst, 290, SmallBuf, WSIZE(SmallBuf)))
						wcscpy(SmallBuf, L"Unexpected error %04lx");
					_snwprintf(Buf, WSIZE(Buf), SmallBuf, dwError);
					MessageBoxW(hwndMain, Buf, NULL, MB_OK | MB_ICONSTOP);
			}
			SetCurrentDirectoryW(FileNameBuf2);
			ShowEditCaret();
			return(FALSE);
		}
		LowerCaseFnameW(FileName);
		IsCurrFile = TRUE;
		if (FName==NULL && CurrFile && *CurrFile) {
			/*Save as, remember previous file name...*/
			SetAltFileName(CurrFile, &CurrPos);
			SwitchToMatchingProfile(FileName);
		}
		wcscpy(CurrFileName+2, FileName);
		FName = CurrFile = CurrFileName+2;
		GenerateTitle(TRUE);
		AskIfExists = FALSE;
	} else wcscpy(FileName, Name);
	if (!IsCurrFile) SetAltFileName(FileName, NULL);
	if (Mode != CommandMode && !DefaultInsFlag) GotChar('\033', 1);
	ReadOnlyError = (ReadOnlyParam || ReadOnlyFile
								   || ViewOnlyFlag && !ExecRunning())
					&& !wcscmp(FileName, CurrFile);
	if (ReadOnlyError) {
		if (((ReadOnlyFlag || ReadOnlyParam) && !(Flags & 1)) || ViewOnlyFlag) {
			Error(112, (LPSTR)FileName);
			ShowEditCaret();
			return (FALSE);
		}
	}

	StartPos.p = FirstPage;
	StartPos.i = 0;
	StartPos.f = 0;
	Newlines = LinesInFile;
	if (StartLine > 1) {
		LONG i = StartLine-1;

#if 0	/*faster method of skipping pages before specified line range...*/
		while (StartPos.p->Newlines < i && StartPos.p->Next)
			StartPos.p = StartPos.p->Next;
		if (StartPos.p->Prev) i -= StartPos.p->Prev->Newlines;
#else	/*safer method...*/
		do {
			do; while (!(CharFlags(c = CharAndAdvance(&StartPos)) & 1));
			--Newlines;
		} while (--i && c != C_EOF);
#endif
		KeepEnabled = TRUE;
		SetSafeAfterWrite = FALSE;
	} else {
		if (StartLine == -1) EndLine = -2;	/*unspecified range -> write all*/
		StartLine = 1;
	}
	if (EndLine != -2) {
		LONG i = StartLine;

		EndPos = StartPos;
		Newlines = Length = 0;
		do {
			do ++Length; while (!(CharFlags(c = CharAndAdvance(&EndPos)) & 1));
			if (c == C_EOF) break;
			++Newlines;
		} while (++i <= EndLine);
		if (c != C_EOF) {
			KeepEnabled = TRUE;
			SetSafeAfterWrite = FALSE;
		}
	} else {
		Length = StartPos.p->Fill - StartPos.i;
		for (EndPos.p=StartPos.p->Next; EndPos.p!=NULL; EndPos.p=EndPos.p->Next)
			Length += EndPos.p->Fill;
		EndPos.p = LastPage;
		EndPos.i = LastPage->Fill;
		EndPos.f = 0;	/*TODO: set flags*/
	}
	if (c != C_EOF) {
		HFILE  hf;
		BOOL   FileExists;
		INT	   i;
		INT    ErrorVal		= 0;
		PWSTR  DispFileName = FileName;

		if (!wcschr(FileName, '\\') && !wcschr(FileName, '/')) {
			if (wcslen(FileName) < WSIZE(FileName) - 2) {
				memmove(FileName+2, FileName,
						(wcslen(FileName) + 1) * sizeof(WCHAR));
				FileName[0] = '.';
				FileName[1] = '\\';
				DispFileName += 2;
			}
		}
		i = _snwprintf(BufferW, WSIZE(BufferW), Brackets, (LPSTR)DispFileName);
		LOADSTRINGW(hInst, 293, BufferW+i, WSIZE(BufferW)-i);
		NewStatus(0, BufferW, NS_BUSY);
		DragAcceptFiles(hwndMain, FALSE);

		/*open for checking file date/time first...*/
		hf = (HFILE)CreateFileW(FileName, GENERIC_READ, 0, NULL,
								OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
								NULL);
		FileExists = hf!=HFILE_ERROR;
		if (FileExists) {
			if (AskIfExists || (FName!=NULL && !*FName)) {
				INT TextId = 0;

				if (AskIfExists) TextId = 203;
				else if (!NotEdited) {
					#if defined(WIN32)
						do {	/*no loop, for break only*/
							static WORD  TmpDosDate,  TmpDosTime;
							static WORD LastDosDate, LastDosTime;

							if (LastTime.dwHighDateTime==0 &&
								LastTime.dwLowDateTime ==0) break;
							GetFileTime((HANDLE)hf, NULL, NULL, &TmpTime);
							FileTimeToDosDateTime(&TmpTime,
												  &TmpDosDate, &TmpDosTime);
							FileTimeToDosDateTime(&LastTime,
												  &LastDosDate, &LastDosTime);
							if (TmpDosDate == LastDosDate &&
								TmpDosTime == LastDosTime) break;
							TextId = 204;
							#if 1
							{	static CHAR *Mon[] =
									{"Nul", "Jan", "Feb", "Mar",
									 "Apr", "May", "Jun", "Jul",
									 "Aug", "Sep", "Oct", "Nov",
									 "Dec", "m13", "m14", "m15"};
								if (TimeDiff(&TmpTime, &LastTime) <= 20000000) {
									/*2 seconds bug with Samba filesystem*/
									break;
								} else QueryString[0] = '\0';
								wsprintf(QueryString + lstrlen(QueryString),
								  "Original:\t%02d-%s-%02d %02d:%02d:%02d UTC\n"
										   "\t0x%08x.%08x\n\n"
								   "Current:\t%02d-%s-%02d %02d:%02d:%02d UTC\n"
										   "\t0x%08x.%08x",
									LastDosDate & 31,
									(LPSTR)Mon[(LastDosDate >> 5) & 15],
									((LastDosDate >> 9) + 80) % 100,
									LastDosTime >> 11,
									(LastDosTime >> 5) & 63,
									(LastDosTime & 31) << 1,
									LastTime.dwHighDateTime,
									LastTime.dwLowDateTime,
									TmpDosDate & 31,
									(LPSTR)Mon[(TmpDosDate >> 5) & 15],
									((TmpDosDate >> 9) + 80) % 100,
									TmpDosTime >> 11,
									(TmpDosTime >> 5) & 63,
									(TmpDosTime & 31) << 1,
									TmpTime.dwHighDateTime,
									TmpTime.dwLowDateTime);
								QueryTime = GetTickCount();
							}
							#endif
						} while (FALSE);
					#else
						if (memcmp(TmpOf.reserved, LastOf.reserved,
								   sizeof(TmpOf.reserved)))
							TextId = 204;
					#endif
				}
				if (TextId) {
					static CHAR Buf[70];

					LOADSTRING(hInst, TextId, Buf, sizeof(Buf));
					if (MessageBox(hwndMain, Buf, ApplName,
							MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONEXCLAMATION)
							!= IDOK) {
						_lclose(hf);
						ShowEditCaret();
						NewStatus(0, L"", NS_NORMAL);
						DragAcceptFiles(hwndMain, TRUE);
						return (FALSE);
					}
					OpenMode = OF_WRITE | OF_CREATE;
				}
			}
			_lclose(hf);
		} else OpenMode = OF_WRITE | OF_CREATE;
		i = _snwprintf(BufferW, WSIZE(BufferW), Brackets, DispFileName);
		LOADSTRINGW(hInst, 287, BufferW+i, WSIZE(BufferW)-i);
		NewStatus(0, BufferW, NS_BUSY);
		if (IsCurrFile) WatchFile(NULL);

		/*now open for writing...*/
		{	DWORD CreateAttr = FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN;

			if (FileExists && Flags & 1) {
				/*reset read-only attribute in file system...*/
				DWORD Attr = GetFileAttributesW(FileName);

				if (Attr != 0xffffffff && Attr & FILE_ATTRIBUTE_READONLY) {
					SetFileAttributesW(FileName, Attr&~FILE_ATTRIBUTE_READONLY);
					SetReadOnly = TRUE;
					CreateAttr = Attr | FILE_FLAG_SEQUENTIAL_SCAN;
				}
			}
			hf = (HFILE)CreateFileW(FileName, GENERIC_WRITE, 0, NULL,
									#if WRITE_WITH_CREAT
									  OpenMode==OF_WRITE ? OPEN_ALWAYS
									  					 : CREATE_ALWAYS,
									#else
									  OPEN_ALWAYS,
									#endif
									CreateAttr, NULL);
		}
		if (hf != HFILE_ERROR) {
			INT  i, n;

			if (ReadOnlyError) ReadOnlyFile = FALSE;
			Interrupted = FALSE;
			Disable(Length > 50000L);
			Length = 0;
			if (OpenMode == OF_WRITE) _llseek(hf, 0L, 2);
			do {
				while (StartPos.p != EndPos.p) {
					n = StartPos.p->Fill - StartPos.i;
					if (n > 0) {
						if (!StartPos.p->PageBuf && !ReloadPage(StartPos.p)) {
							ErrorVal = 108;
							break;
						}
						i = _lwrite(hf, StartPos.p->PageBuf + StartPos.i, n);
						if (i != n) {
							ErrorVal = 107;
							break;
						}
						Length += i;
					}
					StartPos.p = StartPos.p->Next;
					StartPos.i = 0;
					if (!StartPos.p) {
						ErrorVal = 109;
						break;
					}
					MessageLoop(FALSE);
				}
				n = EndPos.i - StartPos.i;
				if (!ErrorVal && n > 0) {
					if (!StartPos.p->PageBuf && !ReloadPage(StartPos.p)) {
						ErrorVal = 108;
						break;
					}
					i = _lwrite(hf, StartPos.p->PageBuf + StartPos.i, n);
					if (i != n) {
						ErrorVal = 107;
						break;
					}
					Length += i;
				}
			} while (FALSE);
			if (!ErrorVal) {
				#if defined(WIN32)
					SetEndOfFile((HANDLE)hf);
    				i = !CloseHandle((HANDLE)(hf));
				#else
					_chsize(hf, _llseek(hf, 0L, 1));
					i = _lclose(hf);
				#endif
				if (i) ErrorVal = 107;
			} else _lclose(hf);
			AddToFileList(DispFileName);
			Enable();
			Interruptable = SavInterruptable;
		}
		#if 1
			else ErrorVal = 1;
		#else
		//	else ErrorVal = OpenMode==OF_WRITE ? 111 : 110;
		#endif
		if (ErrorVal) {
			PWSTR p;

			if (ErrorVal == 1) {
				#if defined(WIN32)
					#define ERRORVALUE GetLastError()
				#else
					#define ERRORVALUE TmpOf.nErrCode
				#endif
				if (ReadOnlyError && ERRORVALUE==5)
					 Error(112, DispFileName);
				else {
					#if defined(WIN32)
						File32Error(DispFileName);
					#else
						FileError(DispFileName, TmpOf.nErrCode, 88);
					#endif
				}
				p = BufferW;
			} else {
				LOADSTRINGW(hInst, ErrorVal, BufferW, WSIZE(BufferW));
				p = BufferW + wcslen(BufferW) + 1;
				_snwprintf(p, WSIZE(BufferW) - (p-BufferW),
						   BufferW, (LPSTR)DispFileName);
				NewStatus(0, p, NS_ERROR);
			}
			MessageBoxW(hwndMain, p, L"WinVi", MB_OK | MB_ICONEXCLAMATION);
			SetSafeAfterWrite = Ret = FALSE;
		} else {
			if (!KeepEnabled && SetSafeAfterWrite)
				EnableToolButton(IDB_SAVE, FALSE);
			i = _snwprintf(BufferW, WSIZE(BufferW),
						   Brackets, (LPSTR)DispFileName);
			if (!FileExists)
				LOADSTRINGW(hInst, 285, BufferW+i, WSIZE(BufferW)-i);
			wcscat(BufferW, CrLfAttribute);
			if (TerseFlag) wcscpy(SmallBuf, L" %ld/%ld");
			else LOADSTRINGW(hInst, 289, SmallBuf, WSIZE(SmallBuf));
			_snwprintf(BufferW + wcslen(BufferW),
					   WSIZE(BufferW) - wcslen(BufferW),
					   SmallBuf, Newlines, Length);
			NewStatus(0, BufferW, NS_NORMAL);
		}
		if (SetSafeAfterWrite) SetSafe(IsCurrFile);
		else if (IsCurrFile) SetUnsafe();
		if (hf!=HFILE_ERROR) {
			#if defined(WIN32)
				if (SetReadOnly) {
					/*set read-only attribute in file system again...*/
					DWORD Attr = GetFileAttributesW(FileName);

					SetFileAttributesW(FileName,
									   Attr | FILE_ATTRIBUTE_READONLY);
				}
			#endif
			if (IsCurrFile) {
				#if defined(WIN32)
					if (SetReadOnly) ReadOnlyFile = TRUE;
					hf = (HFILE)CreateFileW(FileName,
											GENERIC_READ | GENERIC_WRITE,
											0, NULL, OPEN_EXISTING,
											FILE_ATTRIBUTE_NORMAL, NULL);
					if (hf != HFILE_ERROR) {
						if (GetFileTime((HANDLE)hf, NULL, NULL, &LastTime))
							SetFileTime((HANDLE)hf, NULL, NULL, &LastTime);
						else memset(&LastTime, 0, sizeof(LastTime));
						_lclose(hf);
					} else memset(&LastTime, 0, sizeof(LastTime));
					WatchFile(FileName);
					if (hf == HFILE_ERROR)
						ErrorBox(MB_ICONEXCLAMATION, 333, "open failed");
				#else
					hf = OpenFile(FileName, &LastOf, OF_READ);
					if (hf != HFILE_ERROR) _lclose(hf);
				#endif
				NotEdited = FALSE;
			}
		}
		DragAcceptFiles(hwndMain, TRUE);
	}
	ShowEditCaret();
	return (Ret);
}
