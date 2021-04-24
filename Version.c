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
 *		3-Dec-2000	first publication of source code
 *     23-Jul-2007	version returned as wide character string
 */

#include <windows.h>

static WCHAR Version[] = L"Version 3.3.0.0";

PWCHAR GetViVersion(void) {
	extern unsigned Language;

	Version[5] = (unsigned char)(Language == 10000 ? '\363' : 'o');
	return (Version);
}
