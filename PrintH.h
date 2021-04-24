/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several (w/ WIN32 support)   *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2007 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of source code
 */

VOID PrintSettings(HWND);
VOID PrintFromMenu(HWND);
VOID PrintImmediate(HWND);
VOID PrintFromToolbar(HWND);
BOOL PrintSetPrinter(PCWSTR, int);
BOOL PrintCallback(HWND, UINT, WPARAM, LPARAM);

/*Settings.h:*/
LPARAM ChooseFontCustData(INT);
