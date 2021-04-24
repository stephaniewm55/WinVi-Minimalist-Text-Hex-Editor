/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2004 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      3-Dec-2000	first publication of source code
 *      4-Apr-2004	return value of InterCommInit (actually not needed)
 */

INT		InterCommInit(HWND);
BOOL	InterCommCommand(WORD);
VOID	InterCommMenu(HWND, HMENU);
BOOL	InterCommActivate(BOOL);
LRESULT	InterCommWindowProc(HWND, UINT, WPARAM, LPARAM);
