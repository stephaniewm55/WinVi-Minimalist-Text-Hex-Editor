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
 *      3-Jul-2007	new UTF encoding
 */

#include <windows.h>
#include "winvi.h"
#include "page.h"

/*CharFlags:
 *		1:	line terminating character (NUL, CR, LF, CR_LF, RS)
 *		2:	non-display character (shown with caret or hex: ^A...^_, 7f...9f)
 *		4:	used but no meaning yet
 *		8:	letter/digit/underline (for word detection)
 *	   16:	space (TAB, BLANK)
 *	   32:	sentinel character (EOF, ERROR)
 *	   64:	path separator characters ('\\' and '/')
 *	 0x80:	not used yet
 */
static BYTE CFlags[128] = {
	3,2,2,2,2,2,2,2, 2,18,3,2,2,3,2,2,	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,3,2,
   16,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,64,	8,8,8,8,8,8,8,8, 8,8,0,0,0,0,0,0,
	0,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,	8,8,8,8,8,8,8,8, 8,8,8,0,64,0,0,8,
	0,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,	8,8,8,8,8,8,8,8, 8,8,8,0,0,0,0,2
};

extern int OemCodePage, AnsiCodePage;

BYTE CharFlags(INT c)
{
	if (c < 0x80 && c >= (CharSet == CS_OEM ? ' ' : 0)) return CFlags[c];
	switch (c) {
		case C_UNDEF:	return 2;
		case C_EOF:
		case C_ERROR:	return 32 + 7;
		case C_CRLF:
		case '\r':
		case '\n':		return 3;
		case '\t':		return 18;
		case '\0':		return 16;
	}
	if (c < ' ') return 0;
	if (CharSet == CS_ANSI) {
		CHAR	cc = c;
		WCHAR	wc[2];

		MultiByteToWideChar(AnsiCodePage, 0, &cc, 1, wc, 2);
		c = wc[0];
	} else if (CharSet == CS_OEM) {
		CHAR	cc = c;
		WCHAR	wc[2];

		MultiByteToWideChar(OemCodePage, 0, &cc, 1, wc, 2);
		c = wc[0];
	}
	/*TODO: handle Unicode tables...*/
	if (c >= 0xc0 && c <= 0xff) return 8;
	return 0;
}
