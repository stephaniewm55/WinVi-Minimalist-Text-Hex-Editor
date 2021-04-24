/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2001 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of source code
 */

extern COLORREF	TextColor, BackgroundColor, EolColor, StatusErrorBackground;
extern COLORREF Colors[];
extern COLORREF *ForegroundColor;	/*used in font settings dialog*/
extern BOOL		UnsafeSettings;
extern WORD		wUseSysColors;		/*1:Bg, 2:Fg, 4:SelBg, 8:SelFg*/

/*--------------- function prototypes... -----------------*/
/*winvi.c:*/
BOOL SelectBkBitmap(HWND);

/*colors.c:*/
LRESULT CALLBACK ColorProc(HWND, UINT, WPARAM, LPARAM);
BOOL			 ColorsCallback(HWND, UINT, WPARAM, LPARAM);
