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
 *     29-Dec-2000	check for working dir on network path (Win32 on Win9x/ME)
 *     20-Jul-2002	new function ExecRunning 
 *     22-Jul-2002	use of private myassert.h
 *     24-Jul-2002	disable "new" and "open" toolbar buttons / menu items
 *     15-Aug-2002	check of ViewOnly flag before executing
 *     16-Aug-2002	prevent setting safe after writing when inserting
 *     29-Oct-2002	better handling of Ctrl+S/Ctrl+Q (xoff/xon)
 *     30-Oct-2002	corrected inherit attrs for shell pipes, now EOF works
 *      1-Nov-2002	Ctrl+C/Ctrl+Break handling for Windows XP
 *     10-Jan-2003	configurable shell for use of bash
 *     11-Jan-2003	problem with shell pipes fixed for win95/98/me
 *     11-Jan-2003	shell: hide for winnt family, minimize for win95 family
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *     31-Aug-2010	DLL load from system directory only
 */

#include <windows.h>
#include <shellapi.h>
#include <malloc.h>
#include <wchar.h>
#include "myassert.h"
#include "winvi.h"
#include "page.h"
#include "exec.h"

#if defined(WIN32)

extern PWSTR LastCmd;
extern CHAR  QueryString[150];
extern DWORD QueryTime;

static LPSTR lpMemData;
static DWORD dwMemSize;

DWORD WINAPI InputThread(LPVOID lpParam)
	/*this thread handles command input*/
{	HANDLE Pipe = (HANDLE)lpParam;
	DWORD  Written;

	if (lpMemData) WriteFile(Pipe, lpMemData, dwMemSize, &Written, NULL);
	CloseHandle(Pipe);
	return (0);
}

static HANDLE			   hSemaContinueReading, hSemaWaitXOff;
static PROCESS_INFORMATION pi;
static volatile LPSTR	   pError = "No problem";
static volatile BOOL	   Done, GotFromPipe, WaitingForPaint;
static volatile HANDLE	   hThread1, hThread2, hThread3;
static volatile BOOL	   XOffWaiting1, XOffWaiting2;
static BOOL				   DiscardOutput;

BOOL XOffState = FALSE;

DWORD WINAPI OutputThread(LPVOID lpParam)
	/*this thread handles command output, i.e. WinVi input*/
{	HANDLE PipeVcv = (HANDLE)lpParam;
	BOOL   Ok	   = TRUE;

	for (;;) {
		static CHAR	Buf[2048];
		DWORD		Read;

		if (!DiscardOutput) {
			WaitingForPaint = TRUE;
			WaitForSingleObject(hSemaContinueReading, INFINITE);
			WaitingForPaint = FALSE;
		}
		PeekNamedPipe(PipeVcv, NULL, 0, NULL, &Read, NULL);
		if (Read <= 0 && Ok && !Done) Read = 1;
		else if (Read > sizeof(Buf)) Read = sizeof(Buf);
		if (Read && !(Ok = ReadFile(PipeVcv, Buf, Read, &Read, NULL))) Read = 0;
		GotFromPipe = TRUE;
		if (Read <= 0) {
			CloseHandle(PipeVcv);
			PostMessage(hwndMain, WM_USER+1235, 0, 0);
			return (0);
		}
		XOffWaiting1 = TRUE;
		if (XOffState) {
			XOffWaiting2 = TRUE;
			WaitForSingleObject(hSemaWaitXOff, INFINITE);
		}
		XOffWaiting1 = FALSE;
		if (!DiscardOutput)
			PostMessage(hwndMain, WM_USER+1235, (WPARAM)Read, (LPARAM)Buf);
	}
}

VOID ReleaseXOff(VOID)
{
	while (XOffWaiting1) {
		if (XOffWaiting2) {
			XOffWaiting2 = FALSE;
			ReleaseSemaphore(hSemaWaitXOff, 1, NULL);
			return;
		}
		Sleep(10);
	}
}

DWORD WINAPI WatchThread(LPVOID lpParam)
{
	PARAM_NOT_USED(lpParam);
	WaitForSingleObject((HANDLE)pi.hProcess, INFINITE);
	Done = TRUE;
	if (hThread1 && WaitForSingleObject(hThread1, 0)==WAIT_TIMEOUT)
		TerminateThread(hThread1, 0);
	if (XOffState) WaitForSingleObject(hThread2, INFINITE);
	if (!WaitingForPaint && ViewOnlyFlag) {
		GotFromPipe = FALSE;
		Sleep(1000);
		if (!GotFromPipe && !WaitingForPaint) {
			if (hThread2) TerminateThread(hThread2, 0);
			if (ViewOnlyFlag) PostMessage(hwndMain, WM_USER+1235, 0, 0);
		}
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	pi.hProcess = NULL;
	return (0);
}

POSITION	ExecInsertPos;
static BOOL	ConsoleAttached = FALSE;

#endif

VOID ExecInput(WPARAM Read, LPARAM pBuf)
{
	#if defined(WIN32)
		if (Read) {
			LPSTR		Buf = (LPSTR)pBuf;
			INT			nActPos;
			POSITION	OldPos;
			INT			AdvanceAmount = CurrPos.i;
			extern BOOL SetSafeAfterWrite;
			PWSTR		Buf2 = NULL;
			BOOL		LastWasCr  = Buf[Read-1] == '\r';

			nActPos = ComparePos(&CurrPos, &ExecInsertPos);
			if (nActPos != 0) {
				OldPos = CurrPos;
				if (nActPos > 0) {
					while (OldPos.p->PageNo > ExecInsertPos.p->PageNo) {
						OldPos.p = OldPos.p->Prev;
						AdvanceAmount += OldPos.p->Fill;
					}
					AdvanceAmount -= ExecInsertPos.i;
				}
				CurrPos = ExecInsertPos;
			}
			SetSafeAfterWrite = FALSE; /*write operation may be in progress*/
			ViewOnlyFlag      = FALSE;
			if (UtfEncoding) {
				Buf2 = malloc(Read * sizeof(WCHAR));
				if (Buf2 == NULL) {
					/*TODO: display error*/
				} else {
					extern INT OemCodePage;
					INT		   m, n;

					n = MultiByteToWideChar(OemCodePage, 0, Buf, Read,
											Buf2, Read);
					if (UtfEncoding == 8) {
						LPSTR Buf3;

						m = WideCharToMultiByte(CP_UTF8, 0, Buf2, n,
												NULL, 0, NULL, NULL);
						Buf3 = malloc(m);
						if (Buf3 == NULL) {
							/*TODO: display error*/
						} else {
							n = WideCharToMultiByte(CP_UTF8, 0, Buf2, n,
													Buf3, m, NULL, NULL);
							free(Buf2);
							Buf2 = (PWSTR)Buf3;
						}
					} else {
						if (!UtfLsbFirst) {
							for (m=0; m<n; ++m)
								Buf2[m] = (Buf2[m] << 8) | (Buf2[m] >> 8);
						}
						n *= sizeof(WCHAR);
					}
					Buf  = (LPSTR)Buf2;
					Read = n;
				}
			}
			InsertBuffer(Buf, Read, 0);
			ExecInsertPos = CurrPos;
			if (Buf2 != NULL) free(Buf2);
			if (LastWasCr) {
				GoBackAndChar(&ExecInsertPos);
				Advance(&ExecInsertPos, UtfEncoding == 16 ? 2 : 1);
			}
			if (nActPos > 0) {
				OldPos = ExecInsertPos;
				Advance(&OldPos, AdvanceAmount);
			}
			NewPosition(nActPos == 0 ? &ExecInsertPos : &OldPos);
			ShowEditCaret();
			ViewOnlyFlag  = TRUE;
			ReleaseSemaphore(hSemaContinueReading, 1, NULL);
		} else if (ViewOnlyFlag) {
			ViewOnlyFlag = FALSE;
			if (ConsoleAttached) {
				FreeConsole();
				ConsoleAttached = FALSE;
			}
			GotChar('\033', 0);
			FindValidPosition(&CurrPos, 0);
			ShowEditCaret();
			CloseHandle(hSemaContinueReading);
			CloseHandle(hSemaWaitXOff);
			Interruptable = FALSE;
			EnableToolButton(IDB_NEW, TRUE);
			EnableToolButton(IDB_OPEN, TRUE);
			DragAcceptFiles(hwndMain, TRUE);
			CheckClipboard();
			if (pError) MessageBox(hwndMain, pError, "Test", MB_OK);
			if (hThread1) {CloseHandle(hThread1); hThread1 = 0;}
			if (hThread2) {CloseHandle(hThread2); hThread2 = 0;}
			if (hThread3) {CloseHandle(hThread3); hThread3 = 0;}
		}
	#endif
}

BOOL CALLBACK MyControlHandler(DWORD dwCtrlType)
{
	/*ignore event*/
	PARAM_NOT_USED(dwCtrlType);
	return (TRUE);
}

PSTR ExpandSys32(PSTR Dllname)
{	static CHAR Path[MAX_PATH];
	INT			len;

	len = GetSystemDirectory(Path, sizeof(Path));
	if (len > 0 && len + strlen(Dllname) + 2 <= sizeof(Path)) {
		snprintf(Path + len, sizeof(Path) - len - 1, "%s%s",
				 Path[len-1] == '\\' ? "" : "\\", Dllname);
		return Path;
	}
	return "";	/*better prevent DLL loading than doing it unsafe*/
}

VOID ExecInterrupt(INT InterruptEvent)
{
	#if defined(WIN32)
		if (pi.hProcess) {
			/*first try to use [AttachConsole ->] GenerateConsoleCtrlEvent...*/
			typedef BOOL (*ATTACHCONSOLEPROC)(DWORD);
			static HINST hKernel;
			static ATTACHCONSOLEPROC AttachConsole;
			BOOL Result;
			CHAR LastError[32];

			DiscardOutput = TRUE;
			XOffState = FALSE;
			ReleaseXOff();
			if (!ConsoleAttached) {
				if (!hKernel)
					if (!(hKernel = LoadLibrary(ExpandSys32("kernel32.dll"))))
						return;
				if (AttachConsole == NULL) {
					AttachConsole = (ATTACHCONSOLEPROC)
									GetProcAddress(hKernel, "AttachConsole");
					if (AttachConsole == NULL) return;
				}
				if (!AttachConsole(pi.dwProcessId)) return;
			}
			SetConsoleCtrlHandler(MyControlHandler, TRUE);
			#if 1	/*CTRL_C_EVENT does not work, so enforce a CTRL_BREAK*/
				InterruptEvent = CTRL_BREAK_EVENT;
			#endif
			Result = GenerateConsoleCtrlEvent(InterruptEvent, pi.dwProcessId);
			wsprintf(LastError, Result ? "ok" : "failed (LastError=%d)",
					 GetLastError());
			if (!ConsoleAttached) FreeConsole();
			wsprintf(QueryString, "GenerateConsoleCtrlEvent(%d) %s",
								  InterruptEvent, LastError);
			QueryTime = GetTickCount();
			if (Result) return;
		}
	#endif
}

extern DWORD MyGetVersion(VOID);

#if defined(WIN32)
VOID CreateConsole(VOID)
{
	CHAR Title[40];
	HWND ConsoleWindow;
	INT  i;

	if (WinVersion < MAKEWORD(1,5)) {
		/*only for non-WinXP, in WinXP AttachConsole is used*/
		wsprintf(Title, "WinVi 0x%08x", GetCurrentProcessId());
		AllocConsole();
		SetConsoleCtrlHandler(MyControlHandler, TRUE);
		SetConsoleTitle(Title);
		for (i=20; i; --i) {
			if ((ConsoleWindow = FindWindow(NULL, Title)) != NULL) {
				ShowWindow(ConsoleWindow,
					MyGetVersion() & 0x80000000U ? SW_SHOWMINNOACTIVE : SW_HIDE);
				break;
			}
			Sleep(1);
		}
		ConsoleAttached = TRUE;
		SetForegroundWindow(hwndMain);
	}
}
#endif

BOOL ExecRunning(VOID)
{
	#if defined(WIN32)
		return (ViewOnlyFlag && pi.hProcess != 0);
	#else
		return (FALSE);
	#endif
}

BOOL ExecPiped(PWSTR Cmd, WORD Flags)
	/*Flags: 1=use input  pipe (selected part piped from winvi)
	 *		 2=use output pipe (insert command output into current file)
	 */
{
	#if defined(WIN32)
		HANDLE					   PipeVvc, PipeCvc, PipeVcv, PipeCcv;
		/*PipeXyz =	pipe to be used in X for data flow from y to z,
		 *			V/v:	vi end of pipe,
		 *			C/c:	command end of pipe.
		 */
		static STARTUPINFOW	si;
		DWORD				dwFlags;
		PWSTR				Cmd2 = NULL, NewCmd;

		if (ViewOnlyFlag || pi.hProcess) {
			Error(236);
			return (FALSE);
		}
		if (*Cmd == '!') {
			if (!LastCmd) {
				Error(235);
				return (FALSE);
			}
			NewCmd = malloc((wcslen(LastCmd) + wcslen(Cmd)) * sizeof(WCHAR));
			if (NewCmd == NULL) {
				Error(300, 3);
				return (FALSE);
			}
			wcscpy(NewCmd, LastCmd);
			wcscat(NewCmd, Cmd+1);
		} else {
			NewCmd = malloc((wcslen(Cmd) + 2) * sizeof(WCHAR));
			if (NewCmd == NULL) {
				Error(300, 4);
				return (FALSE);
			}
			*NewCmd = '!';
			wcscpy(NewCmd+1, Cmd);
		}
		if (LastCmd != NULL) free(LastCmd);
		LastCmd = NewCmd;
		NewStatus(0, LastCmd, NS_NORMAL);
		Cmd = LastCmd + 1;
		#if 0 && defined(WIN32)
		//	if (MyGetVersion() & 0x80000000U) {
		//		/*Win9x/ME, check for working directory on network path,
		//		 *command.com refuses any action on such a drive...
		//		 */
		//		CHAR Cwd[MAX_PATH];
		//
		//		if (GetCurrentDirectory(sizeof(Cwd), Cwd)
		//				&& Cwd[0]=='\\' && Cwd[1]=='\\') {
		//			Error(416);
		//			return (FALSE);
		//		}
		//	}
		#endif

		if (!CreatePipe(&PipeCvc, &PipeVvc, NULL, 0)) return (FALSE);
		if (!CreatePipe(&PipeVcv, &PipeCcv, NULL, 0)) {
			CloseHandle(PipeCvc);
			CloseHandle(PipeVvc);
			return (FALSE);
		}
		Interrupted = FALSE;
		if (MyGetVersion() & 0x80000000U) {
			/*Win95/98/Me*/
			DuplicateHandle(GetCurrentProcess(),  PipeCvc,
							GetCurrentProcess(), &PipeCvc, 0, TRUE,
							DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
			DuplicateHandle(GetCurrentProcess(),  PipeCcv,
							GetCurrentProcess(), &PipeCcv, 0, TRUE,
							DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
		} else {
			/*WinNT/2K/XP*/
			SetHandleInformation(PipeCvc,
				HANDLE_FLAG_INHERIT | HANDLE_FLAG_PROTECT_FROM_CLOSE,
				HANDLE_FLAG_INHERIT);
			SetHandleInformation(PipeCcv,
				HANDLE_FLAG_INHERIT | HANDLE_FLAG_PROTECT_FROM_CLOSE,
				HANDLE_FLAG_INHERIT);
		}
		dwFlags = CREATE_NEW_PROCESS_GROUP;
		si.cb		  = sizeof(si);
		si.dwFlags	  = STARTF_USESTDHANDLES;
		si.hStdInput  = PipeCvc;
		si.hStdOutput =
		si.hStdError  = PipeCcv;
		if (Flags & 2) {
			extern PSTR Shell, ShellCommand;
			INT			Length;

			Length = FormatShellW(NULL, ShellCommand, Shell, Cmd);
			if ((Cmd2 = malloc(2*Length + 2)) != NULL) {
				FormatShellW(Cmd2, ShellCommand, Shell, Cmd);
				Cmd = Cmd2;
			}
			si.wShowWindow = MyGetVersion() & 0x80000000U /*Win95/98/Me*/
			 				 ? SW_SHOWMINNOACTIVE : SW_HIDE;
			si.dwFlags |= STARTF_USESHOWWINDOW;
		}
		QueryTime = 0;
		CreateConsole();
		/*OutputDebugStringW(Cmd);*/
		if (CreateProcessW(NULL,	Cmd,  NULL, NULL, TRUE,
						   dwFlags, NULL, NULL, &si,  &pi)) {
			extern HGLOBAL YankHandles[27];
			extern ULONG   YankLength[27];
			extern INT	   YankIndex;

			CloseHandle(PipeCvc);
			CloseHandle(PipeCcv);
			if (!SelectCount) Flags &= ~1;
			if (SelectCount) DeleteSelected(25);
			if (Flags & 1) {
				DWORD TId;

				lpMemData = (LPSTR)GlobalLock(YankHandles[YankIndex]);
				dwMemSize = YankLength[YankIndex];
				hThread1 = CreateThread(NULL, 0, InputThread, PipeVvc, 0, &TId);
			} else {
				CloseHandle(PipeVvc);
				hThread1 = NULL;
			}
			ExecInsertPos = CurrPos;
			Interruptable = TRUE;
			if (Flags & 2) {
				DWORD TId2, TId3;

				Mode = InsertMode;
				NewPosition(&CurrPos);
				SwitchCursor(SWC_TEXTCURSOR);
				ShowEditCaret();
				pError = NULL;
				if (Cmd2) free(Cmd2);
				XOffState = Done = WaitingForPaint = DiscardOutput
						  = XOffWaiting1 = XOffWaiting2 = FALSE;
				hSemaContinueReading = CreateSemaphore(NULL, 1, 1, NULL);
				hSemaWaitXOff		 = CreateSemaphore(NULL, 0, 1, NULL);
				ViewOnlyFlag = TRUE;
				DragAcceptFiles(hwndMain, FALSE);
				EnableToolButton(IDB_NEW, FALSE);
				EnableToolButton(IDB_OPEN, FALSE);
				CheckClipboard();
				hThread2 = CreateThread(NULL, 0, OutputThread,PipeVcv,0, &TId2);
				hThread3 = CreateThread(NULL, 0, WatchThread, NULL,   0, &TId3);
				return (TRUE);
			}
			hThread2 = hThread3 = NULL;
			ShowEditCaret();
			if (Flags & 1) {
				if (hThread1) {
					WaitForSingleObject(hThread1, INFINITE);
					CloseHandle(hThread1);
				}
				if (lpMemData) {
					GlobalUnlock(YankHandles[YankIndex]);
					lpMemData = NULL;
				}
			}
			CloseHandle(PipeVcv);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			Interruptable = FALSE;
			return (TRUE);
		}
		wsprintf(QueryString,
				 "CreateProcess failed, error %ld", GetLastError());
		QueryTime = GetTickCount();
		if (ConsoleAttached) {
			FreeConsole();
			ConsoleAttached = FALSE;
		}
		if (Cmd2) free(Cmd2);
		CloseHandle(PipeVcv);
		CloseHandle(PipeCcv);
		CloseHandle(PipeVvc);
		CloseHandle(PipeCvc);
		Interruptable = FALSE;
	#else
		Error(125);
	#endif
	return (FALSE);
}
