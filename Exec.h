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
 *      3-Dec-2000	first publication of source code
 *     23-Oct-2002	better handling of Ctrl+S/Ctrl+Q (xoff/xon)
 */

VOID ReleaseXOff(VOID);
BOOL ExecPiped(PWSTR, WORD);
VOID ExecInput(WPARAM, LPARAM);
VOID ExecInterrupt(int);
