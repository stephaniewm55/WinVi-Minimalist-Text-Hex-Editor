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

#define XFIELD_OFFSET 6

typedef struct {
	INT	  x, Width;
	INT	  Style;
	BOOL  Centered;
	PWSTR Text;
	INT   TextExtent;
} STATUSFIELD;
extern STATUSFIELD StatusFields[8];
/*Vi command line, visual command, Caps, Num, %, line, col, 0*/

typedef struct tagSTATUSSTYLE {
	COLORREF Color;
	BOOL LoweredFrame;
} STATUSSTYLE;
extern STATUSSTYLE StatusStyle[5];

extern HFONT StatusFont;
extern int   StatusHeight, StatusTextHeight;

/*mouse.c:*/
VOID RemoveStatusTip(VOID);
BOOL StatusContextMenuCommand(WORD);

/*status.c:*/
VOID StatusContextMenu(int, int);
VOID NewCapsString(VOID);
