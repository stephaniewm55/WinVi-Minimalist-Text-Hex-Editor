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
 *		3-Dec-2000	first publication of source code
 */

#define XTEXT_OFFSET	6
#define YTEXT_OFFSET	2

extern HPEN		BlackPen, DlgLitePen;
extern HBRUSH	SelBkgndBrush, BkgndBrush;
extern COLORREF SelTextColor, SelBkgndColor, ShmTextColor, ShmBkgndColor;
extern HPEN		HilitePen, ShadowPen;	/*3-D system colors*/
extern HBRUSH	FaceBrush;				/*3-D system colors*/

extern WCHAR	NumLock[10], CapsLock[10];

void SystemColors(BOOL);
void MakeBackgroundBrushes(void);
