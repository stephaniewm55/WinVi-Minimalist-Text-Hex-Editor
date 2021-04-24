/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several (w/ WIN32 support)   *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2002 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of source code
 *     28-Dec-2002	implementation of mappings
 *     11-Jan-2003	prototypes for new mappings/abbreviations
 */

typedef struct tagMAP {
	struct tagMAP *Next;
	LPSTR		  Name, InputDisplay, Replace;
	LPWORD		  InputMatch, ReplaceMap;
	DWORD		  Flags;
} *PMAP;

#define M_ABBREVIATION 1		/*entry is an abbreviation, otherwise...*/
#define M_INSERTMODE   2		/*map in insert or replace mode*/
#define M_COMMANDMODE  4		/*map in command mode (edit mode)*/
#define M_COMMANDLINE  8		/*map in ex command line mode*/

extern PMAP FirstMapping;

VOID FreeMappings(PMAP);
BOOL TranslateMapping(PWORD*, PCSTR);
BOOL MacrosCallback(HWND, UINT, WPARAM, LPARAM);
VOID StopMap(BOOL);
BOOL IsKeyboardMapMsg(PMSG);
BOOL KeyboardMapDequeue(PMSG);
VOID WmChar(WPARAM);
VOID WmKeydown(WPARAM);
