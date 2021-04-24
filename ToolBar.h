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

#define TOOLS_HEIGHT	  29
#define XBUTTON_OFFSET	  12
#define YBUTTON_OFFSET	   3

extern HBITMAP	Buttons;
extern HFONT	TooltipFont;
extern int		yButtonOffset, nToolbarHeight;

void PrepareBitmap(HDC);
INT  AbsByte(INT);
