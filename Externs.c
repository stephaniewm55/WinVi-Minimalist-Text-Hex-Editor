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
 *     12-Sep-2002	+EBCDIC quick hack (Christian Franke)
 *      6-Apr-2004	new global data for font glyph availability
 *     11-Jun-2007	new UTF encoding
 */

#include <windows.h>
#include "winvi.h"
#include "page.h"
#include "paint.h"
#include "colors.h"
#include "status.h"
#include "toolbar.h"

/*WinVi.h:*/
MODEENUM Mode;
UINT	 Language;
BOOL	 HexEditMode, TfForward, TfIsFind, ReplaceSingle;
BOOL	 Interruptable, FullRowExtend;
UINT	 Interrupted, CmdFirstChar, Number;
int		 TextHeight;  /*screen line y difference in pixels*/
LONG	 FirstVisible;/*line numbr (starting at 0) for bitmap alignment*/
COLORREF TextColor;
HCURSOR	 hcurCurrent, hcurArrow, hcurIBeam, hcurHourGlass, hcurSizeNwSe;
HFONT	 TextFont, HexFont;
BYTE	 TextGlyphsAvail[32], HexGlyphsAvail[32];
HINST	 hInst;
HWND	 hwndMain, hwndCmd, hwndErrorParent;
WORD	 WinVersion;
PCWSTR	 CurrFile;
BOOL	 IsAltFile, Unsafe, CommandWithExclamation, CaretActive;
BOOL	 MouseActivated, BreakLineFlag, ShowMatchFlag;
BOOL	 ReadOnlyFlag, AutoRestoreFlag, AutoIndentFlag, AutoWriteFlag;
BOOL	 IgnoreCaseFlag, WrapScanFlag, TerseFlag, DefaultInsFlag;
BOOL	 DefaultTmpFlag, QuitIfMoreFlag, WrapPosFlag;
BOOL	 ListFlag, NumberFlag, HexNumberFlag, FullRowFlag;
BOOL	 ViewOnlyFlag;	/*view-only=restrictive read-only w/o editing*/
BOOL	 ReadOnlyFile;	/*read-only for this file only*/
COLUMN	 CurrCol;		/*aspired pixel position in logical line*/
ULONG	 CurrColumn;	/*displayed character offset within line (for status)*/
ULONG	 LinesInFile, SelectCount;
INT		 CurrX, CurrLine;
INT		 MaxScroll, Disabled;
WCHAR	 FileName[260];
PWSTR	 TmpDirectory;
PWSTR	 Tags;
UINT	 CharSet;		/*0=ANSI, 1=PC (OEM), 2=EBCDIC*/
INT		 UtfEncoding;	/*0=Non-UTF, 8=UTF-8, 16=UTF-16*/
BOOL	 UtfLsbFirst;
BOOL	 Indenting;
BOOL	 RefreshBitmap;
WCHAR	 BitmapFilename[260];
BOOL	 DlgResult;

/*Page.h:*/
LPPAGE	 FirstPage, LastPage;
POSITION ScreenStart, CurrPos;
PLINEINFO LineInfo;
POSITION SelectStart;
ULONG	 SelectBytePos;

/*Paint.h:*/
COLORREF SelTextColor, SelBkgndColor;
WCHAR	 NumLock[10], CapsLock[10];

/*Colors.h:*/
COLORREF TextColor, BackgroundColor;
WORD	 wUseSysColors;		/*1:Bg, 2:Fg, 4:SelBg, 8:SelFg, 512:StatusFg*/

/*Status.h:*/
STATUSFIELD	StatusFields[8];
int			StatusTextHeight;

/*ToolBar.h:*/
HFONT	TooltipFont;
int		yButtonOffset, nToolbarHeight;

