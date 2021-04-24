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
 *     12-Dec-2000	missing initialization of DWORD i in EnumWindowsProc()
 *      4-Apr-2004	return value of InterCommInit (actually not needed)
 *     21-Jul-2007	window identifier can be included into window title
 */

#include <windows.h>
#include <string.h>
#include <wchar.h>
#include "winvi.h"
#include "intercom.h"

#define MAXDISPLAY 20

extern PCWSTR ClassName;

static UINT	  IcMessage, IcCount, IcUnassigned;
static BOOL	  IcLowest;
static UINT	  IcIndex = MAXDISPLAY;
static HWND	  IcWindows[MAXDISPLAY];

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lPar)
{	WCHAR Buf[20];

	PARAM_NOT_USED(lPar);
	Buf[19] = '\0';
	if (GetClassNameW(hWnd, Buf, WSIZE(Buf)-1) && wcscmp(Buf, ClassName) == 0) {
		DWORD i = MAXDISPLAY;

		#if defined(WIN32)
			#if !defined(SMTO_NORMAL)
				#define SMTO_NORMAL 0x0000
			#endif
			SendMessageTimeout(hWnd, IcMessage, (WPARAM)hWnd, 0,
							   SMTO_NORMAL, 2000, &i);
		#else
			i = SendMessage(hWnd, IcMessage, (WPARAM)hWnd, 0);
		#endif
		if (i < MAXDISPLAY) {
			if (!IcWindows[(int)i]) IcWindows[(int)i] = hWnd;
			#if 0
			//	else OutputDebugString("Entry already in use!\n");
			#endif
		} else ++IcUnassigned;
	}
	return (TRUE);
}

static VOID RebuildWindowList(VOID)
{	static WNDENUMPROC EnumProc;

	/*request window titles from all running WinVis*/
	if (EnumProc == NULL)
		EnumProc=(WNDENUMPROC)MakeProcInstance((FARPROC)EnumWindowsProc, hInst);
	memset((VOID*)IcWindows, 0, sizeof(IcWindows));
	EnumWindows(EnumProc, 0);
}

INT InterCommInit(HWND hWnd)
{	INT i;

	PARAM_NOT_USED(hWnd);
	IcMessage = RegisterWindowMessage("WinVi - Message");
	for (;;) {
		IcLowest = TRUE;
		IcCount = IcUnassigned = 0;
		RebuildWindowList();
		if (IcCount <= 1 /*own window (might already be closed)*/) break;
		if (!IcLowest && IcUnassigned > 1) {
			IcCount = 0;
			for (i=1000; i>0; --i) {
				MessageLoop(FALSE);
				if (IcCount && i > MAXDISPLAY) i = MAXDISPLAY;
			}
		}
	}
	/*search for an empty entry index to use for this WinVi instance...*/
	for (i=0; i<MAXDISPLAY && IcWindows[i]; ++i) {}
	IcIndex = i;
	return (IcIndex);
}

BOOL InterCommCommand(WORD uCmd)
{	HWND hwndThis;

	if ((uCmd -= 2000) < MAXDISPLAY && IsWindow(hwndThis = IcWindows[uCmd])) {
		if (IsIconic(hwndThis)) ShowWindow(hwndThis, SW_RESTORE);
		else {
			#if defined(WIN32)
				BringWindowToTop(hwndThis);
			#else
				SetActiveWindow(hwndThis);
			#endif
		}
		return (TRUE);
	}
	return (FALSE);
}

#ifdef __LCC__
  #define MENUITEMINFOW MENUITEMINFO
  #define MIIRADIOMENU_DWTYPEDATA *(LPWSTR*)&mii.dwTypeData
#else
  #define MIIRADIOMENU_DWTYPEDATA mii.dwTypeData
#endif

VOID InterCommMenu(HWND hWnd, HMENU hMenu)
{	INT  i;
	UINT LastItem = 0;

	if (!IcMessage) return;

	/*clear old list of menu entries*/
	do; while (DeleteMenu(hMenu, 0, MF_BYPOSITION));

	RebuildWindowList();
	for (i=0; i<MAXDISPLAY; ++i) {
		static WCHAR Buf[266];
		PWSTR		 p, p2;

		if (!IcWindows[i]) continue;
		GetWindowTextW(IcWindows[i], Buf+4, WSIZE(Buf)-4);
		if (!wcsncmp(Buf+4, L"WinVi - ", 8)) p = Buf + 8;
		else if (wcscmp(Buf+wcslen(Buf+4)-4, L" - WinVi") == 0) {
			Buf[wcslen(Buf+4)-4] = '\0';
			p = Buf;
		}
		p2 = p+2;
		while ((p2 = wcschr(p2+2, '&')) != NULL)
			memmove(p2+1, p2, 2*wcslen(p2)+2);
		p[0] = '&'; p[1] = i<9 ? '1'+i : 'a'-9+i; p[2] = ' '; p[3] = '\t';
		#if defined(WIN32)
		{	static MENUITEMINFOW mii = {sizeof(MENUITEMINFO),
										MIIM_ID | MIIM_TYPE | MIIM_STATE,
										MFT_RADIOCHECK | MFT_STRING};

			MIIRADIOMENU_DWTYPEDATA	= p;
			mii.wID					= 2000 + i;
			mii.fState				= IcWindows[i]==hWnd ? MFS_CHECKED : 0;
			if (InsertMenuItemW(hMenu, LastItem, TRUE, &mii)) {
				++LastItem;
				continue;
			}
		}
		#endif
		InsertMenuW(hMenu, LastItem++,
				    IcWindows[i]!=hWnd ? MF_BYPOSITION | MF_STRING
									   : MF_CHECKED | MF_BYPOSITION | MF_STRING,
				    2000+i, p);
	}
}

INT IcWindowID(VOID) {
	if (IcIndex == MAXDISPLAY) return 0;
	return IcIndex < '9' ? '1' + IcIndex : 'a' - 9 + IcIndex;
}

BOOL InterCommActivate(BOOL Backwards)
{	INT  i, Start;
	HWND Activate = 0;

	if (!IcMessage) return (FALSE);
	RebuildWindowList();
	for (Start=0; IcWindows[Start]!=hwndMain && Start<MAXDISPLAY; ++Start);
	if (Start >= MAXDISPLAY) return (FALSE);
	i = Start;
	while (++i != Start) {
		if (i >= MAXDISPLAY) {
			i = 0;
			if (!Start) break;
		}
		if (IcWindows[i]) {
			Activate = IcWindows[i];
			if (!Backwards) break;
		}
	}
	if (!Activate) return (FALSE);
	#if defined(WIN32)
		BringWindowToTop(Activate);
	#else
		SetActiveWindow(Activate);
	#endif
	return (TRUE);
}

LRESULT InterCommWindowProc(HWND hWnd, UINT uMsg, WPARAM wPar, LPARAM lPar)
{
	PARAM_NOT_USED(lPar);
	if (!uMsg || uMsg!=IcMessage) return ((LRESULT)-1);
	++IcCount;
	if ((HWND)wPar < hWnd) IcLowest = FALSE;
	return (IcIndex);
}
