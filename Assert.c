/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several (w/ WIN32 support)   *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2000 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      3-Dec-2000	first publication of source code
 *     22-Jul-2002	use of private myassert.h
 */

#include <windows.h>
#include "myassert.h"

static char AssertBuf[160];

static void BufCat(LPSTR p)
{
	if (lstrlen(AssertBuf) + lstrlen(p) < sizeof(AssertBuf))
		lstrcat(AssertBuf, p);
}

void AssertionFail(char *Expression, char *FileName, int Line)
{	extern HWND hwndMain;
	static char NumBuf[16];
	char        *p, *fn;

	for (p=fn=(char*)FileName; *p; ++p)
		if (*p=='/' || *p=='\\') fn = p+1;
	*AssertBuf = '\0';
	BufCat("Assertion failed!\n\nExpression:\t");
	BufCat(Expression);
	BufCat("\nFile:\t\t");
	BufCat(fn);
	wsprintf(NumBuf, " (%d)", Line);
	BufCat(NumBuf);
	#if defined(_DEBUG)
		if (MessageBox(hwndMain, AssertBuf, "WinVi - Assertion Botch",
					   MB_OKCANCEL | MB_ICONSTOP) == IDOK)
			return;
		*p /= *p;	/*enforce a trap (*p=='\0')*/
	#else
		MessageBox(hwndMain, AssertBuf, "WinVi - Assertion Botch",
				   MB_OK | MB_ICONSTOP);
	#endif
}
