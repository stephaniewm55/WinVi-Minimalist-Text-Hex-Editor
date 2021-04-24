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
 *     31-Aug-2010	DLL load from system directory only
 */

#include <windows.h>
#include "winvi.h"
#include "ctl3d.h"

static HINSTANCE Ctl3dDll;
static FARPROC AutoSubclass, Enabled, ColorChange, Unregister;

BOOL WINAPI Ctl3dRegister(HANDLE hInst)
{
	#if defined(WIN32)
		PARAM_NOT_USED(hInst);
		return (FALSE);
	#else
		UINT PrevState = SetErrorMode(SEM_NOOPENFILEERRORBOX);
		static FARPROC Register;

		if ((UINT)(Ctl3dDll = LoadLibrary(ExpandSys32("ctl3d.dll"))) < 32) {
			Ctl3dDll = NULL;
			SetErrorMode(PrevState);
			return (FALSE);
		}
		SetErrorMode(PrevState);
		Register     = GetProcAddress(Ctl3dDll, "Ctl3dRegister");
		AutoSubclass = GetProcAddress(Ctl3dDll, "Ctl3dAutoSubclass");
		Enabled      = GetProcAddress(Ctl3dDll, "Ctl3dEnabled");
		ColorChange  = GetProcAddress(Ctl3dDll, "Ctl3dColorChange");
		Unregister   = GetProcAddress(Ctl3dDll, "Ctl3dUnregister");
		if (!Register || !AutoSubclass || !Enabled || !ColorChange
					  || !Unregister) {
			FreeLibrary(Ctl3dDll);
			Ctl3dDll = NULL;
			return (FALSE);
		}
		return ((*(BOOL (WINAPI FAR*)(HINSTANCE))Register)(hInst));
	#endif
}

BOOL WINAPI Ctl3dAutoSubclass(HANDLE hInst)
{
	if (!Ctl3dDll) return (FALSE);
	return ((*(BOOL (WINAPI FAR*)(HINSTANCE))AutoSubclass)(hInst));
}

BOOL WINAPI Ctl3dEnabled(void)
{
	if (!Ctl3dDll) return (FALSE);
	return ((*(BOOL (WINAPI FAR*)(VOID))Enabled)());
}

BOOL WINAPI Ctl3dColorChange(void)
{
	if (!Ctl3dDll) return (FALSE);
	return ((*(BOOL (WINAPI FAR*)(VOID))ColorChange)());
}

BOOL WINAPI Ctl3dUnregister(HANDLE hInst)
{	BOOL ret;

	if (!Ctl3dDll) return (FALSE);
	ret = (*(BOOL (WINAPI FAR*)(HINSTANCE))Unregister)(hInst);
	FreeLibrary(Ctl3dDll);
	Ctl3dDll = NULL;
	return (ret);
}
