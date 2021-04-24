/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several (w/ WIN32 support)   *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of source code
 */

BOOL WINAPI Ctl3dRegister(HANDLE);
BOOL WINAPI Ctl3dAutoSubclass(HANDLE);
BOOL WINAPI Ctl3dEnabled(void);
BOOL WINAPI Ctl3dColorChange(void);
BOOL WINAPI Ctl3dUnregister(HANDLE);
