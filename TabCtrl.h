/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2009 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of source code
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 */

#if !defined(WIN32) && !defined(_WIN32)
# define OWN_TABCTRL 1
#endif

#if !defined(RC_INVOKED) && defined(OWN_TABCTRL)

#if defined(WIN32)
# include <commctrl.h>
#endif

#if defined(TabCtrl_InsertItem)
# undef TabCtrl_InsertItem
# undef TabCtrl_SetCurSel
#endif
#define TabCtrl_InsertItem(hwnd,iItem,pitem) TabInsertItem(hwnd, iItem, pitem)
#define TabCtrl_SetCurSel(hwnd,iItem) TabSetCurSel(hwnd, iItem)

#if !defined(WIN32)
# define WM_NOTIFY	   0x004E
# define TCM_GETCURSEL 0x1311
# define TCN_SELCHANGE 0xfdd9
# define TCIF_TEXT     0x0001

  typedef struct tagNMHDR {
	HWND   hwndFrom;
	UINT   idFrom;
	UINT   code;
  } NMHDR, FAR *LPNMHDR;

  typedef struct tagTC_ITEM {
	UINT   mask;
	DWORD  dwState;
	DWORD  dwStateMask;
	LPSTR  pszText;
	int    cchTextMax;
	int    iImage;
	LPARAM lParam;
  } TC_ITEM;
#endif

BOOL			 TabInsertItem(HWND, INT, TC_ITEM*);
BOOL			 TabSetCurSel (HWND, INT);
BOOL			 TabCtrlPaint (HWND, HDC, PAINTSTRUCT*);
LRESULT CALLBACK TabCtrlProc  (HWND, UINT, WPARAM, LPARAM);
BOOL			 TabCtrlInit  (HWND, HFONT);

#endif
