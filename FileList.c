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
 *     28-Jul-2007	no removal of '+' parameters in startup phase
 */

#include <windows.h>
#include <string.h>
#include <wchar.h>
#include "resource.h"
#include "winvi.h"
#include "pathexp.h"
#include "myassert.h"

typedef struct tagFILELIST {
	struct tagFILELIST *Next;
	BYTE			   Flags;
	WCHAR			   FileName[1];
} FILELIST, *PFILELIST;
PFILELIST pFileList, pLastFile;

int nCurrFileListEntry, nNumFileListEntries;

BOOL RemoveFileListEntry(INT n)
{	PFILELIST p;
	HLOCAL	  hFile;

	if (nNumFileListEntries == 0) return FALSE;
	--nNumFileListEntries;
	if (nCurrFileListEntry) --nCurrFileListEntry;
	p = pFileList;
	if (p == NULL) {
		assert(pFileList != NULL);
		nNumFileListEntries =
		nCurrFileListEntry  = 0;
		return FALSE;
	}
	pFileList = pFileList->Next;
	hFile = LocalHandle(p);
	LocalUnlock(hFile);
	LocalFree(hFile);
	return (TRUE);
}

void ClearFileList(void)
{	extern BOOL	StartupPhase;
	PWSTR		lp;
	INT			i = 0;

	while (i < nNumFileListEntries) {
		if (!StartupPhase || *(lp = GetFileListEntry(0)) != '+')
			RemoveFileListEntry(i);
		else ++i;
	}
	nCurrFileListEntry = i;
}

static int AppendSingle(PCWSTR lp1, PCWSTR lp2, int nLen)
{	HLOCAL	  hFile;
	PFILELIST pFile;

	hFile = LocalAlloc(LMEM_FIXED, (wcslen(lp1) + nLen) * sizeof(WCHAR)
								   + sizeof(FILELIST));
	if (!hFile) return (-1);
	pFile = (PFILELIST)LocalLock(hFile);
	if (pFile == NULL) {
		LocalFree(hFile);
		return (-1);
	}
	pFile->Next  = NULL;
	pFile->Flags = 0;
	wcscpy(pFile->FileName, lp1);
	_fmemcpy(pFile->FileName + wcslen(lp1), lp2, nLen*sizeof(WCHAR));
	pFile->FileName[wcslen(lp1) + nLen] = '\0';
	if (pFileList) pLastFile->Next = pFile;
	else pFileList = pFile;
	pLastFile = pFile;
	return (nNumFileListEntries++);
}

INT AppendToFileList(PCWSTR pwFile, INT nLen, INT Flags)
	/*Flags: 1=initial parameter list or next command, expand wildcards
	 */
{	WCHAR  FNameBuf[260];
	WCHAR  *p	  = FNameBuf, *PathEnd = FNameBuf;
	PCWSTR pw	  = pwFile;
	INT	   nRet	  = -1;
	BOOL   Expand = FALSE;

	while (nLen-- && p-FNameBuf < sizeof(FNameBuf)-1) {
		switch (*p++ = *pw++) {
			case '"':
				--p;
				break;
			case '*': case '?':
				Expand = TRUE;
				break;
			case '/': case '\\':
				PathEnd = p;
				Expand = FALSE;
				break;
			case '\0':	/*should not occur*/
				nLen = 0;
		}
	}
	*p = '\0';
	if (!++nLen && Flags & 1 && Expand) {
		FIND_DATA Data;
		HANDLE    FindHandle;

		FindHandle = FIND_FIRST(FNameBuf, &Data);
		if (FindHandle != INVALID_HANDLE_VALUE) {
			*PathEnd = '\0';
			do {
				if (!IS_DIRECTORY(&Data) && !IS_HIDDEN(&Data)) {
					nRet = AppendSingle(FNameBuf, FOUND_NAME(&Data),
										wcslen(FOUND_NAME(&Data)));
					if (nRet == -1) break;
				}
			} while (FIND_NEXT(FindHandle, &Data));
			FIND_CLOSE(FindHandle);
			return (nRet);
		}
	}
	return (AppendSingle(FNameBuf, pw, nLen));
}

PWSTR GetFileListEntry(INT nEntry)
{	PFILELIST p;

	p = pFileList;
	if (p == NULL) return (NULL);
	while (nEntry--) {
		p = p->Next;
		if (p == NULL) return (NULL);
	}
	return (p->FileName);
}

BOOL CALLBACK ArgsCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	int   i;
	PWSTR lp;
	HWND  hwndListBox;

	PARAM_NOT_USED(lPar);
	switch (uMsg) {
		case WM_INITDIALOG:
			i = 0;
			hwndListBox = GetDlgItem(hDlg, IDC_LISTBOX);
			if (hwndListBox) {
				while ((lp = GetFileListEntry(i++)) != NULL)
					SendMessageW(hwndListBox, LB_ADDSTRING, 0, (LPARAM)lp);
				SendMessage(hwndListBox, LB_SETCURSEL, nCurrFileListEntry, 0);
			}
			return (TRUE);

		case WM_COMMAND:
			if (COMMAND == IDC_LISTBOX) {
				if (NOTIFICATION == LBN_DBLCLK) wPar = IDOK;
				else if (NOTIFICATION == LBN_SELCHANGE)
					SendMessage(hDlg, DM_SETDEFID, IDOK, 0);
			}
			if (COMMAND == IDOK) {
				i = (int)SendDlgItemMessage(hDlg,IDC_LISTBOX,LB_GETCURSEL,0,0);
				if (i==LB_ERR || ((lp=GetFileListEntry(i)) == NULL)
							  || !AskForSave(hDlg, 1))
					return (TRUE);
				nCurrFileListEntry = i;
				Unsafe = FALSE;		/*already asked for save*/
				Open(lp, 0);
			}
			if (COMMAND==IDOK || COMMAND==IDCANCEL)
				EndDialog(hDlg, COMMAND==IDOK);
			return (TRUE);
	}
	return (FALSE);
}

void ArgsCmd(void)
{	static DLGPROC Callback;

	if (!Callback)
		 Callback = (DLGPROC)MakeProcInstance((FARPROC)ArgsCallback, hInst);
	DialogBoxW(hInst, MAKEINTRESW(IDD_ARGS), hwndMain, Callback);
}

INT FindOrAppendToFileList(PCWSTR Fname)
{	int j;

	for (j = 0; j < nNumFileListEntries; ++j) {
		PWSTR pName;

		pName = GetFileListEntry(j);
		if (pName != NULL && !wcsicmp(pName, Fname)) return (j);
	}
	return (AppendToFileList(Fname, wcslen(Fname), 0));
}
