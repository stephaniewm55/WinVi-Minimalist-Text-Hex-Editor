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
 *      3-Dec-2000	first publication of source code
 *     22-Jul-2002	use of private myassert.h
 *     22-Sep-2002	tabexpand server and share names
 *     22-Sep-2002	new choice for ":set ebcdimode" or shorter ":set em"
 *      3-Oct-2002	corrected handling of :<num><tab> and :<num><letter><tab>
 *      3-Oct-2002	corrected handling of :w>><tab>
 *      6-Oct-2002	corrected handling of :s///<tab>
 *      8-Dec-2002	skipping multiple ":" at start of command line
 *     10-Jan-2003	new variable "shell"
 *     22-Oct-2003	optional tab expansion with forward slash
 *     22-Dec-2004	show at least one more character in shortened menus
 *      1-Jan-2005	bugfix in PatternMatch matching '?' in one pattern of a list
 *     12-Mar-2005	bugfix in PatternMatch when matching a list of patterns
 *      9-Aug-2007	handling wide character strings
 *     16-Dec-2007	passed correct size to wcsncpy() and WNetEnumResourceW()
 *                	WNetEnumResource() changed to WNetEnumResourceW()
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 */

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#include "myassert.h"
#include "winvi.h"
#include "status.h"
#include "pathexp.h"

HANDLE MyFindFirst(PWSTR,  FIND_DATA *);
BOOL   MyFindNext (HANDLE, FIND_DATA *);
BOOL   MyFindClose(HANDLE);
BOOL   IsDirectory(FIND_DATA *);
BOOL   IsHidden   (FIND_DATA *);
PCWSTR FoundName  (FIND_DATA *);

HMENU PathExpMenu;

static WCHAR Source[280], Result[280], *pTail, *pMultiLastName;
static UINT	 Id;
static struct {
	WCHAR *Name;
	WORD  Flags;
} *pCmd, Commands[] = {
	{L"!",			   4},	/*4=expand commands with path*/
	{L"=",			   0},	/*0=nothing to expand*/
	{L"args",		   0},
	{L"cd",			   2},	/*2=expand directories only*/
	{L"chdir",		   2},
	{L"edit",		   1},	/*1=expand normal files*/
	{L"filename",	   1},
	{L"next",		   1},
	{L"quit",		   0},
	{L"read",		   1},
	{L"rewind",		   0},
	{L"set",		   8},	/*8=expand variable names*/
	{L"substitute",	  16},	/*16=expand previous search pattern*/
	{L"tag",		   0},
	{L"version",	   0},
	{L"vi",			   1},
	{L"wq",			   1},
	{L"write",		   1},
	{L"xit",		   1},
	{NULL,			   0}
}, Variables[] = {
	{L"ai",			  32},	/*32=suppress menu choice*/
	{L"all",		   0},
#if defined(WIN32)
	{L"ansicodepage=",128},
	{L"ansicp=",	 160},
#endif
	{L"am",			  32},
	{L"ansimode",	   0},
	{L"autoindent",	   0},
	{L"autowrite",	   0},
	{L"aw",			  32},
	{L"columns=",	 128},	/*128=value*/
	{L"dm",			  32},
	{L"dosmode",	   0},
	{L"em",			  32},
	{L"ebcdimode",	   0},
	{L"hexmode",	   0},
	{L"hm",			  32},
	{L"ic",			  32},
	{L"ignorecase",	   0},
	{L"lines=",		 128},
	{L"list",		   0},
	{L"magic",		   0},
	{L"noai",		  96},
	{L"noautoindent", 64},	/*64="no" prefix*/
	{L"noautowrite",  64},
	{L"noam",		  96},
	{L"noansimode",	  96},
	{L"noaw",		  96},
	{L"nodm",		  96},
	{L"nodosmode",	  96},
	{L"nohexmode",	  64},
	{L"nohm",		  96},
	{L"noic",		  96},
	{L"noignorecase", 64},
	{L"nolist",		  64},
	{L"nomagic",	  64},
	{L"nonumber",	  64},
	{L"noreadonly",	  64},
	{L"noro",		  96},
	{L"noshowmatch",  64},
	{L"nosm",		  96},
	{L"noterse",	  64},
	{L"noviewonly",	  64},
	{L"novo",		  96},
	{L"nowrapscan",	  64},
	{L"nows",		  96},
	{L"number",		   0},
#if defined(WIN32)
	{L"oemcodepage=",128},
	{L"oemcp=",		 160},
#endif
	{L"readonly",	   0},
	{L"ro",			  32},
	{L"scroll=",	 128},
	{L"shell=",		 128},
	{L"shiftwidth=", 128},
	{L"showmatch",	   0},
	{L"sm",			  32},
	{L"sw=",		 160},
	{L"tabstop=",	 128},
	{L"tags=",		 128},
	{L"terse",		   0},
	{L"tmpdirectory=",128},
	{L"ts=",		 160},
	{L"viewonly",	   0},
	{L"vo",			  32},
	{L"wrapscan",	   0},
	{L"ws",			  32},
	{NULL,			   0}
}, ShellCommands[] = {
	{L"break",		  32},	/*not meaningful as a single command*/
	{L"call",		   0},
	{L"cd",			  32},
	{L"chcp",		  32},
	{L"chdir",		  32},
	{L"cls",		  32},
	{L"copy",		   0},
	{L"ctty",		   0},
	{L"date",		   0},
	{L"del",		   0},
	{L"dir",		   0},
	{L"echo",		   0},
	{L"erase",		   0},
	{L"exit",		  32},
	{L"for",		  32},
	{L"goto",		  32},
	{L"if",			   0},
	{L"lfnfor",		 544},
	{L"lh",			   0},
	{L"loadhigh",	   0},
	{L"lock",		 512},	/*Windows 95 or Windows NT 4.0 only (not Win 3.x)*/
	{L"md",			   0},
	{L"mkdir",		   0},
	{L"path",		  32},
	{L"pause",		  32},
	{L"prompt",		  32},
	{L"rd",			   0},
	{L"rem",		   0},
	{L"ren",		   0},
	{L"rename",		   0},
	{L"rmdir",		   0},
	{L"set",		  32},
	{L"shift",		  32},
	{L"time",		   0},
	{L"truename",	   0},
	{L"type",		   0},
	{L"unlock",		 512},
	{L"ver",		   0},
	{L"verify",		  32},
	{L"vol",		   0},
	{NULL,			   0}
};
int nMenuItemHght, nMenuHeight, nMenuItems;
static int Columns, MaxLen;

typedef struct tagPOPUPLIST {
	BYTE   MenuGroup;	/*0=internal/shell internal, 1=directory, 2=file*/
	WCHAR  Name[1];
} POPUPLIST, FAR* LPPOPUPLIST, FAR* FAR* LPLPPOPUPLIST;
LPLPPOPUPLIST PopupList = NULL;
int PopupInUse=0, PopupAllocated=0;

extern BOOL TabExpandWithSlash;

static void StartPopupMenu(void)
{
	if (!nMenuItems) {
		nMenuItemHght = GetSystemMetrics(SM_CYMENU);
		nMenuHeight = GetSystemMetrics(SM_CYSCREEN);
		#if defined(WIN32)
			++nMenuItemHght;
		#else
			if (WinVersion >= MAKEWORD(95,3)) {
				nMenuItemHght += 2;
				nMenuHeight -= 10;
			}
		#endif
	}
	while (PopupInUse > 0) _ffree(PopupList[--PopupInUse]);
	if (PopupAllocated) {
		_ffree(PopupList);
		PopupAllocated = 0;
	}
	nMenuItems = MaxLen = 0;
}

static void AppendMenuItemW(INT MenuGroup, PWSTR pString)
{	LPPOPUPLIST NewItem;

	if (PopupInUse+1 /*NULL element at end*/ >= PopupAllocated) {
		LPLPPOPUPLIST np;

		if ((np = _fcalloc(PopupAllocated+100, sizeof(LPPOPUPLIST))) == NULL)
			return;
		if (PopupInUse) {
			_fmemcpy(np, PopupList, PopupInUse*sizeof(LPPOPUPLIST));
			_ffree(PopupList);
		}
		PopupList = np;
		PopupAllocated += 100;
	}
	NewItem = _fcalloc(1, sizeof(POPUPLIST) + wcslen(pString)*sizeof(WCHAR));
	if (NewItem != NULL) {
		int i;

		wcscpy(NewItem->Name, pString);
		if ((i = wcslen(pString)) > MaxLen) MaxLen = i;
		NewItem->MenuGroup = (BYTE)MenuGroup;
		/*binary array search...*/
		{	int i, d;

			if (PopupInUse) {
				BOOL Less;

				for (d=PopupInUse-1; d & (d-1); d &= d-1);
				/*d = largest power of 2 less than PopupInUse*/
				i = d;
				if (i) i += i-1;
				for (;;) {
					Less = i < PopupInUse
						   && ( PopupList[i]->MenuGroup <  MenuGroup ||
							   (PopupList[i]->MenuGroup == MenuGroup
								&& lstrcmpiW(PopupList[i]->Name, pString) < 0));
					if (Less) i += d;
					else i -= d;
					if (!d) break;
					d >>= 1;
				}
				if (Less) ++i;
			} else i = 0;
			if (i < PopupInUse)
				_fmemmove(PopupList+i+1, PopupList+i,
						  (PopupInUse-i)*sizeof(LPPOPUPLIST));
			assert(i >= 0);
			assert(i < PopupAllocated);
			++PopupInUse;
			PopupList[i] = NewItem;
		}
		++nMenuItems;
	}
}

static BOOL ShowPopupMenu(int x, int y)
{	LPLPPOPUPLIST p;
	int			  MenuGroup, Separator = 0;

	if (PopupList==NULL || *PopupList==NULL) return (FALSE);
	MenuGroup = (*PopupList)->MenuGroup;
	if (PathExpMenu) DestroyMenu(PathExpMenu);
	PathExpMenu = CreatePopupMenu();
	if (!PathExpMenu) {
		MessageBeep(MB_ICONEXCLAMATION);
		return (FALSE);
	}
	if ((LONG)nMenuItems*nMenuItemHght > 3L*nMenuHeight) {
		/*too many files for menu (more than 5 columns), split name...*/
		LPLPPOPUPLIST p2;
		int			  i, MatchLen[21];

		memset(MatchLen, 0, sizeof(MatchLen));
		for (p=PopupList, p2=p+1; *p2!=NULL; ++p, ++p2) {
			PWSTR pN1, pN2;

			if ((*p)->MenuGroup == 0) continue;
			i = 0;
			pN1 = (*p)->Name;
			pN2 = (*p2)->Name;
			while (pN1[i] && CharUpperW((PWSTR)(INT)(BYTE)pN1[i])==
							 CharUpperW((PWSTR)(INT)(BYTE)pN2[i])) ++i;
			if (i > 20) i = 20;
			++MatchLen[i];
		}
		for (MaxLen=i=0; MaxLen<=20; ++MaxLen)
			if ((i+=MatchLen[MaxLen]) > 20) break;
		if (i == MatchLen[MaxLen]) ++MaxLen;
		else if (MaxLen>1 && i*nMenuItemHght > 8*nMenuHeight) --MaxLen;
	}
	nMenuItems = 0;
	p = PopupList;
	while (*p != NULL) {
		WORD wFlags = MF_STRING;

		if (((long)++nMenuItems-(Separator>>1))*nMenuItemHght > nMenuHeight) {
			wFlags |= MF_MENUBARBREAK;
			nMenuItems = 1;
			++Columns;
			MenuGroup = (*p)->MenuGroup;
			Separator = 0;
		} else if (MenuGroup != (*p)->MenuGroup) {
			if (((long)++nMenuItems-(Separator>>1))*nMenuItemHght>nMenuHeight) {
				wFlags |= MF_MENUBARBREAK;
				nMenuItems = 1;
				++Columns;
				Separator = 0;
			} else {
				AppendMenu(PathExpMenu, MF_SEPARATOR, 0, NULL);
				if (WinVersion >= MAKEWORD(95,3)) ++Separator;
			}
			MenuGroup = (*p)->MenuGroup;
		}
		if (MenuGroup > 0 && (INT)wcslen((*p)->Name) > MaxLen+(MenuGroup==1)) {
			int			c1, c2, len;
			LPLPPOPUPLIST p2;

			if (*(p2=p+1) != NULL && (INT)wcslen((*p2)->Name) > MaxLen) {
				c1 = (*p)->Name[MaxLen];
				c2 = (*p2)->Name[MaxLen];
				(*p)->Name[MaxLen] = (*p2)->Name[len=MaxLen] = '\0';
				while (lstrcmpiW((*p)->Name, (*p2)->Name) == 0
					   && (*p)->MenuGroup == (*p2)->MenuGroup) {
					(*p2)->Name[MaxLen] = c2;
					++p2;
					if (*p2==NULL || (len=wcslen((*p2)->Name))<MaxLen) break;
					c2 = (*p2)->Name[MaxLen];
					(*p2)->Name[MaxLen] = '\0';
				}
				if (*p2!=NULL && len>=MaxLen)
					(*p2)->Name[MaxLen] = c2;
				if (p2 != p+1) {
					WCHAR ps[26];  /*wcslen((*p)->Name) is not larger than 20*/

					wcscpy(ps, (*p)->Name);
					if (c1 == '&') {
						/*check for & to double at end of string...*/
						int n = 1, i=MaxLen;

						while ((*p)->Name[++i] == '&') ++n;
						if (n & 1) wcscat(ps, L"&");
					}
					wcscat(ps, L" ...");
					AppendMenuW(PathExpMenu, wFlags, Id++, ps);
					(*p)->Name[MaxLen] = c1;
					p = p2;
					continue;
				}
				(*p)->Name[MaxLen] = c1;
			}
		}
		AppendMenuW(PathExpMenu, wFlags, Id++, (*p)->Name);
		++p;
	}
	TrackPopupMenu(PathExpMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON,
				   x, y, 0, hwndMain, NULL);
	StartPopupMenu();	/*remove memory only*/
	return (TRUE);
}

static INT InsertHexTab(HWND hwndEdit, INT Position, PWSTR *Start, INT nSep)
	/*return values:	0:	insufficient space in buffer,
	 *					1:	tab inserted,
	 *					2:	position out of search/replace fields.
	 */
{	PWSTR pTabpos = Source + Position, p;

	if (nSep) {
		/*check Position to be within string separator *Start...*/
		for (p = *Start+1; *p && p < pTabpos; ++p) {
			if (*p == **Start && !--nSep) {
				*Start = p + 1;
				return (2);
			}
			if (*p == '\\' && !*++p) break;
		}
	}
	if (wcslen(Source) >= WSIZE(Source) - 4) return (0);
	memmove(pTabpos+4, pTabpos, (wcslen(pTabpos)+1) * sizeof(WCHAR));
	memcpy(pTabpos, L"\\%09", 8);
	SetWindowTextW(hwndEdit, Source);
	Position += 4;
	#if defined(WIN32)
		SendMessage(hwndEdit, EM_SETSEL, Position, Position);
	#else
		SendMessage(hwndEdit, EM_SETSEL, 0, MAKELPARAM(Position, Position));
	#endif
	return (1);
}

BOOL ExpandPath(HWND hwndEdit, WORD wFlags)
{
	/*wFlags: 1=skip command part (command window in status line)
	 */
	PWSTR	pName = Source, pPos;
	LRESULT	CurrSel;
	int		nLen;
	BOOL	ShowInternal = FALSE,	DirectoriesOnly	= FALSE;
	BOOL	Multiple	 = FALSE,	TraversePath	= FALSE;
	PWSTR	*SuffixList	 = NULL;
	PWSTR	DirSeparator = L"\\";
	WORD	SuffixMask	 = 0;

	pMultiLastName = NULL;
	GetWindowTextW(hwndEdit, Source, WSIZE(Source));
	CurrSel = SendMessage(hwndEdit, EM_GETSEL, 0, 0L);
	if (LOWORD(CurrSel) != HIWORD(CurrSel)
			|| (ULONG)(CurrSel &= 0xffff) > wcslen(Source)) {
		MessageBeep(MB_ICONEXCLAMATION);
		return (FALSE);
	}
	if (wFlags & 1) {
		BOOL Skip = TRUE;

		if (*pName == '/' || *pName == '?')
			return (InsertHexTab(hwndEdit, (short)CurrSel, &pName, 1));
		if (*pName != ':' && *pName != '!') {
			MessageBeep(MB_ICONEXCLAMATION);
			return (FALSE);
		}
		if (*pName == ':') {
			do; while (*++pName == ':');
			for (;;) {
				switch (*pName) {
					case ' ': case ',': case '%': case '$':
					case '+': case '-': case '.':
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9':
						++pName;
						continue;
				}
				break;
			}
		}
		switch (LCASE(*pName)) {
			case 'c': DirectoriesOnly = TRUE;
					  break;

			case '!': Skip			  = FALSE;
					  TraversePath	  = TRUE;
					  if (TabExpandWithSlash) DirSeparator = L"/";
					  /*FALLTHROUGH*/
			case 'n': Multiple		  = TRUE;
					  break;

			case 's':
				if (LCASE(*++pName) == 'e') {
					if (LCASE(*++pName) == 't') ++pName;
					if (*pName == ' ') {
						for (;;) {
							do; while (*++pName == ' ');
							for (pPos=pName; *pPos && *pPos!=' '; ++pPos)
								if (*pPos == '"')
									do; while (*++pPos && *pPos != '"');
							if (!*pPos) break;
							pName = pPos;
						}
						ShowInternal = TRUE;
						pCmd = Variables;
						Skip = FALSE;
					}
				} else {
					PSTR p = "substitute";

					while (*pName && LCASE(*pName) == *++p) ++pName;
					if (!*pName) break;
					while (*pName == ' ') ++pName;
					if (*pName && (*pName<'A' || *pName>'Z')
							   && (*pName<'a' || *pName>'z')) {
						switch (InsertHexTab(hwndEdit, (short)CurrSel,
											 &pName, 2)) {
							case 0:	return (FALSE);
							case 1:	return (TRUE);
						 /*	case 2: break; */
						}
					}
					{	static PWSTR s[] = {L"/", L"c", L"g"};

						SuffixList = s;
						SuffixMask = pName[-1]!=' ' ? 6 : 7;
						while (*pName) {
							switch (LCASE(*pName++)) {
								case ' ':	continue;

								case 'c':	SuffixMask &= ~3;
											break;

								case 'g':	SuffixMask &= ~5;
											break;

								default:	SuffixMask = 0;
							}
							if (!SuffixMask) break;
						}
						Skip = FALSE;
					}
					if (SuffixMask && !*pName) break;
					MessageBeep(MB_ICONEXCLAMATION);
					return (FALSE);
				}
				break;

			case '.':
				if (pName[1] != '.' && pName[1] != '/' && pName[1] != '\\' &&
						*Source == ':') {
					++pName;
					break;
				}
				/*FALLTHROUGH*/
			case '\\':
			case '?': case '/':
				if (*pName == '\\' || *pName == '.' || *Source != ':') {
					MessageBeep(MB_ICONEXCLAMATION);
					return (FALSE);
				}
				{	INT c = *pName;

					while (*++pName != c) {
						if (*pName == '\\') ++pName;
						else if (pName-Source == CurrSel)
							return (InsertHexTab(hwndEdit, (short)CurrSel,
												 NULL, 0));
						if (!*pName) return(FALSE);
					}
					do; while (*pName && *++pName == ' ');
					if (*pName != ',') break;
					do; while (*++pName == ' ');
					if ((c = *pName) != '/' && c != '?') break;
					while (*++pName != c) {
						if (*pName == '\\') ++pName;
						else if (pName-Source == CurrSel)
							return (InsertHexTab(hwndEdit, (short)CurrSel,
												 NULL, 0));
						if (!*pName) return(FALSE);
					}
					++pName;
				}
		}
		if (Skip) {
			while (*pName >= 'A' && *pName <= 'z') ++pName;
			if (!*pName) {
				ShowInternal = TRUE;
				pCmd = Commands;
				while (pName[-1] > '9' && pName[-1] != ':') --pName;
			}
		}
		while (*pName == ' ' || *pName == '!' || *pName == '>') ++pName;
		if (Multiple) {
			for (;;) {
				if (*pName == '"') {
					++pName;
					pPos = wcsstr(pName, L"\" ");
					if (pPos == NULL) break;
					pName = pPos + 2;
					TraversePath = FALSE;
				} else if ((pPos = wcschr(pName, ' ')) != NULL) {
					pName = pPos + 1;
					TraversePath = FALSE;
				} else {
					if (TraversePath)
						if (wcschr(pName, '\\') || wcschr(pName, '/'))
							TraversePath = FALSE;
					break;
				}
			}
		}
	}
	{	PWSTR p;

		for (p=pName; (p=wcsstr(p, L"##")) != NULL; ++p)
			memmove(p, p+1, 2*wcslen(p));
		for (p=pName; (p=wcsstr(p, L"%%")) != NULL; ++p)
			memmove(p, p+1, 2*wcslen(p));
	}
	pPos = pName + wcslen(pName);
	wcscpy(Result, Source);
	Id = 4000;
	StartPopupMenu();
	if (SuffixMask) {
		pTail = Result + (pName - Source);
		while (SuffixMask) {
			if (SuffixMask & 1) {
				wcscpy(pTail, *SuffixList);
				AppendMenuItemW(0, pTail);
			}
			SuffixMask >>= 1;
			++SuffixList;
		}
	} else if (ShowInternal) {
		pTail = Result + (pName - Source);
		*pTail = '\0';
		for (; pCmd->Name; ++pCmd) {
			PWSTR p1 = pName, p2 = pCmd->Name;

			while (*p1 && LCASE(*p1) == *p2++) ++p1;
			if (*p1 || (*p2 && *p2!='=' && pCmd->Flags & 32)) continue;
			wcscpy(pTail, pCmd->Name);
			if (/*PopupInUse == 1 &&*/ pCmd->Flags & 27)
				wcscat(pTail, pCmd->Flags & 16 ? L"/" : L" ");
			AppendMenuItemW(0, pTail);
		}
		if (!*pTail) {
			MessageBeep(MB_ICONEXCLAMATION);
			return (FALSE);
		}
	} else {
		FIND_DATA FindData;
		HANDLE	  hFindHandle;

		for (pTail=Result+(pPos-Source);
			 --pTail>=Result+(pName-Source) && *pTail!='\\' && *pTail!='/';)
			{}
		if ((pPos-pName==2 && pName[1]==':') || !lstrcmpW(pTail+1, L".."))
			wcscat(Result, DirSeparator);
		else {
			PWSTR Path = NULL;
			#if !defined(WIN32)
				BOOL AppendExtension = !strchr(pTail+1, '.');
			#endif

			*++pTail = '\0';
			if (TraversePath && pTail >= Result+(pName-Source)) {
				/*include COMMAND.COM intrinsic commands and path commands...*/
				for (pCmd=ShellCommands; pCmd->Name; ++pCmd) {
					PWSTR p1 = pName, p2 = pCmd->Name;

					while (*p1 && LCASE(*p1) == *p2++) ++p1;
					if (*p1 || pCmd->Flags & 32) continue;
					wcscpy(pTail, pCmd->Name);
					AppendMenuItemW(0, pTail);
				}
				#if defined(WIN32)
				{	INT Len;

					Len = GetEnvironmentVariableW(L"Path", NULL, 0) + 1;
					Path = _fcalloc(Len, sizeof(WCHAR));
					if (Path) GetEnvironmentVariableW(L"Path", Path, Len);
				}
				#endif
			}
			wcscpy(pPos, L"*");
			hFindHandle = MyFindFirst(pName, &FindData);
			if (hFindHandle != INVALID_HANDLE_VALUE) {
				do {
					if (*FoundName(&FindData) == '.' &&
								(!lstrcmpW(FoundName(&FindData), L".")
							  || !lstrcmpW(FoundName(&FindData), L"..")))
						/*TODO: make last compare (..) configurable*/
						continue;
					if (IsHidden(&FindData)) continue;
					if (DirectoriesOnly && !IsDirectory(&FindData)) continue;
					TO_ANSI_STRCPY(pTail, FoundName(&FindData));
					LowerCaseFnameW(pTail);
					{	PWSTR p = pTail;

						while ((p = wcschr(p, '&')) != NULL) {
							/*double '&' to prevent handling as accelerator...*/
							memmove(p+1, p, 2*wcslen(p)+2);
							p += 2;
						}
					}
					if (IsDirectory(&FindData)) {
						wcscat(pTail, DirSeparator);
						AppendMenuItemW(1, pTail);
					} else AppendMenuItemW(2, pTail);
				} while (MyFindNext(hFindHandle, &FindData));
				MyFindClose(hFindHandle);
			}
			if (!*pTail) {
				MessageBeep(MB_ICONEXCLAMATION);
				return (FALSE);
			}
			if (Multiple) do {	/*no loop, for break only*/
				pMultiLastName = Result + (pName-Source);
				if (pName[-1] != '"') {
					if (PopupInUse != 1 || !wcschr(pTail, ' ')) break;
					memmove(pMultiLastName+1, pMultiLastName,
							2*wcslen(pMultiLastName)+2);
					*pMultiLastName = '"';
				} else --pMultiLastName;
				if (!wcsspn(pTail + wcslen(pTail) - 1, L"/\\"))
					wcscat(pTail, L"\"");
			} while (FALSE);
			if (Path) _ffree(Path);
		}
	}
	{	PWSTR p, p2;

		*pPos = '\0';
		p = pName;
		while ((p2 = wcschr(p, '#')) != NULL) {
			memmove(p2+1, p2, 2*wcslen(p2)+2);
			p = p2 + 2;
			++pPos;
		}
		p = pName;
		while ((p2 = wcschr(p, '%')) != NULL) {
			memmove(p2+1, p2, 2*wcslen(p2)+2);
			p = p2 + 2;
			++pPos;
		}
		p = Result + (pName-Source);
		while ((p2 = wcschr(p, '#')) != NULL) {
			memmove(p2+1, p2, 2*wcslen(p2)+2);
			p = p2 + 2;
			if (p2 < pTail) ++pTail;
		}
		p = Result + (pName-Source);
		while ((p2 = wcschr(p, '%')) != NULL) {
			memmove(p2+1, p2, 2*wcslen(p2)+2);
			p = p2 + 2;
			if (p2 < pTail) ++pTail;
		}
	}
	if (PopupInUse > 1) {
		RECT Pos;
		HDC  hDC;

		GetWindowRect(hwndEdit, &Pos);
		hDC = GetDC(hwndMain);
		if (hDC) {
			SIZE  Size;
			HFONT hfontOld = SelectObject(hDC, StatusFont);

			Pos.left += StatusFields[0].x;
			if (GetTextExtentPointW(hDC, Source, pPos-Source, &Size)) {
				if (Size.cx >= StatusFields[0].Width)
					 Pos.left += StatusFields[0].Width - 1;
				else Pos.left += Size.cx;
			}
			SelectObject(hDC, hfontOld);
			ReleaseDC(hwndMain, hDC);
		}
		ShowPopupMenu(Pos.left, Pos.top);
	} else {
		while ((pTail = wcsstr(pTail, L"&&")) != NULL) {
			memmove(pTail, pTail+1, 2*wcslen(pTail));
			++pTail;
		}
		SetWindowTextW(hwndEdit, Result);
		nLen = wcslen(Result);
		#if defined(WIN32)
			SendMessage(hwndEdit, EM_SETSEL, nLen, nLen);
		#else
			SendMessage(hwndEdit, EM_SETSEL, 0, MAKELPARAM(nLen, nLen));
		#endif
		pTail = NULL;
	}
	return (TRUE);
}

void PathExpCommand(UINT Command)
{	int nLen;

	if (pTail!=NULL && hwndCmd && PathExpMenu && Command>=4000 && Command<Id) {
		BOOL TabAgain = FALSE;

		GetMenuStringW(PathExpMenu, Command, pTail,
					   WSIZE(Result) - (pTail-Result), MF_BYCOMMAND);
		{	PWSTR p, p2;

			for (p=pTail; (p=wcsstr(p, L"&&")) != NULL; ++p)
				/*strip second ampersand (necessary for correct menu display)*/
				memmove(p, p+1, 2*wcslen(p));
			for (p=pTail; (p2=wcschr(p,'#'))  != NULL; p=p2+2)
				/*double these '#' and '%' to avoid filename substitution...*/
				memmove(p2+1, p2, 2*wcslen(p2)+2);
			for (p=pTail; (p2=wcschr(p,'%'))  != NULL; p=p2+2)
				memmove(p2+1, p2, 2*wcslen(p2)+2);
		}
		if ((nLen=wcslen(pTail))>4 && lstrcmpW(pTail+(nLen-=4), L" ...")==0) {
			pTail[nLen] = '\0';
			TabAgain = TRUE;
		}
		if (pMultiLastName) {
			if (*pMultiLastName!='"' && wcschr(pTail, ' ')) {
				memmove(pMultiLastName+1, pMultiLastName,
						2*wcslen(pMultiLastName)+2);
				*pMultiLastName = '"';
			}
			if (!TabAgain && *pMultiLastName == '"')
				if (!wcsspn(pTail + wcslen(pTail) - 1, L"/\\"))
					wcscat(pTail, L"\"");
		}
		SetWindowTextW(hwndCmd, Result);
		nLen = wcslen(Result);
		#if defined(WIN32)
			SendMessage(hwndCmd, EM_SETSEL, nLen, nLen);
		#else
			SendMessage(hwndCmd, EM_SETSEL, 0, MAKELPARAM(nLen, nLen));
		#endif
		pTail = NULL;
		DestroyMenu(PathExpMenu);
		PathExpMenu = NULL;
		if (TabAgain) ExpandPath(hwndCmd, 1);
	}
}

void PathExpCleanup(void)
{
	if (PathExpMenu) {
		DestroyMenu(PathExpMenu);
		PathExpMenu = NULL;
	}
}

/*-----------------------------------------------------------------
 * now some extensions to FindFirstFile/FindNextFile
 * for handling network enumerations
 */

#if defined(WIN32)

BOOL PatternMatch(PCWSTR String, PCWSTR Pattern)
{	PCWSTR		s = String, p = Pattern;
	static BOOL	Recursive = FALSE;

	for (;;) {
		switch (*p) {
			case '?':
				if (!*s) {
					if (Recursive || (p = wcspbrk(p, L",;")) == NULL)
						return (FALSE);
					s = String;
				} else ++s;
				++p;
				break;

			case '*':
				{	BOOL SaveRecursive = Recursive;

					Recursive = TRUE;
					++p;
					do {
						if (PatternMatch(s, p)) {
							Recursive = SaveRecursive;
							return (TRUE);
						}
					} while (*s++);
					Recursive = SaveRecursive;
					if (Recursive || (p = wcspbrk(p, L",;")) == NULL)
						return (FALSE);
					s = String;
					++p;
				}
				continue;

			case '\0':
			case ',':
			case ';':
				if (!*s) return (TRUE);
				if (!*p || Recursive) return (FALSE);
				/*FALLTHROUGH*/
			default:
				if (LOWER_CMP(*s, *p)) {
					if (Recursive || (p = wcspbrk(p, L",;")) == NULL)
						return (FALSE);
					s = String;
				} else ++s;
				++p;
		}
	}
}

#ifdef __LCC__
typedef struct {
	DWORD  dwScope;
	DWORD  dwType;
	DWORD  dwDisplayType;
	DWORD  dwUsage;
	LPWSTR lpLocalName;
	LPWSTR lpRemoteName;
	LPWSTR lpComment;
	LPWSTR lpProvider;
} NETRESOURCEW;
#endif

static NETRESOURCEW *NetResource[4] = {NULL, NULL, NULL, NULL};
static DWORD        NrSizes[4];
static HANDLE       hEnum[4] =
	   {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
	    INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
/* The first 3 elements of each of the above arrays are used for the different
 * levels of enumerating network servers (top level, domains, and servers).
 * The 4th element (index 3) is used for enumerating the shares of a server.
 *															25-Sep-2002 Ramo
 */
WCHAR TabExpandDomains[256];

static INT EnumNetRes(INT i, INT iMax, NETRESOURCEW *nr)
{
	if (hEnum[i] == INVALID_HANDLE_VALUE) {
		if (WNetOpenEnumW(RESOURCE_GLOBALNET, RESOURCETYPE_ANY,
   						  0, nr, &hEnum[i]) != NO_ERROR) {
			return (-1);
		}
	}
	for (;;) {
		DWORD count, size;

		if (NetResource[i] == NULL) {
			NetResource[i] = malloc(sizeof(NETRESOURCEW));
			if (NetResource[i] == NULL) return (-1);
			NrSizes[i] = sizeof(NETRESOURCEW);
		}
		if (i >= iMax || hEnum[i+1] == INVALID_HANDLE_VALUE) {
			DWORD rc;

			count = 1;
			size  = NrSizes[i];
			if (CheckInterrupt()) rc = ERROR_REQUEST_ABORTED;
			else rc = WNetEnumResourceW(hEnum[i], &count, NetResource[i],&size);
			if (rc == ERROR_MORE_DATA) {
				NETRESOURCEW *nr = realloc(NetResource[i], size);

				if (nr == NULL) return (-1);
				NetResource[i] = nr;
				NrSizes[i]     = size;
				count          = 1;
				rc = WNetEnumResourceW(hEnum[i], &count, NetResource[i], &size);
			}
			if (rc != NO_ERROR) {
				WNetCloseEnum(hEnum[i]);
				hEnum[i] = INVALID_HANDLE_VALUE;
				return (-1);
			}
			if (NetResource[i]->lpRemoteName != NULL &&
					wcsspn(NetResource[i]->lpRemoteName, L"\\/") == 2 &&
					NetResource[i]->dwDisplayType == RESOURCEDISPLAYTYPE_SERVER)
				return (i);
		}
		if (i < iMax && NetResource[i]->dwUsage & RESOURCEUSAGE_CONTAINER) {
			INT Res;

			if (NetResource[i]->dwDisplayType != RESOURCEDISPLAYTYPE_DOMAIN ||
				PatternMatch(NetResource[i]->lpRemoteName, TabExpandDomains)) {
					Res = EnumNetRes(i+1, iMax, NetResource[i]);
					if (Res >= 0) return(Res);
			}
		}
	}
}

static HANDLE EnumServers(PCWSTR Name, FIND_DATA *FindData)
{	INT n;
	static PWSTR Compare;

	if (Name != NULL) {
		if (Compare != NULL) free(Compare);
		if ((Compare = malloc((wcslen(Name) + 1) * sizeof(WCHAR))) == NULL)
			return (INVALID_HANDLE_VALUE);
		wcscpy(Compare, Name);
	}
	while ((n = EnumNetRes(0, 2, NULL)) >= 0) {
		if (PatternMatch(NetResource[n]->lpRemoteName, Compare)) {
			wcsncpy(FindData->cFileName,
			        NetResource[n]->lpRemoteName + 2,
			        WSIZE(FindData->cFileName));
			FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
			return((HANDLE)(NetResource+n));
		}
	}
	MyFindClose((HANDLE)NetResource);
	return (INVALID_HANDLE_VALUE);
}

static HANDLE EnumShares(PWSTR Name, PWSTR NameSep, FIND_DATA *FindData)
{	DWORD		 rc, count, size;
	static PWSTR Compare;

	if (NetResource[3] == NULL) {
		NetResource[3] = malloc(sizeof(NETRESOURCEW));
		if (NetResource[3] == NULL) return (INVALID_HANDLE_VALUE);
		NrSizes[3] = sizeof(NETRESOURCEW);
	}
	if (hEnum[3] == INVALID_HANDLE_VALUE) {
		static NETRESOURCEW Root;
		INT					Separator;

		if (Name == NULL) return (INVALID_HANDLE_VALUE);
		Root.dwScope       = RESOURCE_GLOBALNET;
		Root.dwType        = RESOURCETYPE_ANY;
		Root.dwDisplayType = RESOURCEDISPLAYTYPE_SERVER;
		Root.dwUsage       = RESOURCEUSAGE_CONTAINER;
		Root.lpRemoteName  = Name;
		Separator = *NameSep;
		*NameSep  = '\0';
		if (Compare != NULL) free(Compare);
		if ((Compare = malloc(wcslen(NameSep))) == NULL)
			return (INVALID_HANDLE_VALUE);
		wcscpy(Compare, NameSep+1);
		if (WNetOpenEnumW(RESOURCE_GLOBALNET, RESOURCETYPE_ANY,
   						  0, &Root, &hEnum[3]) != NO_ERROR) {
			*NameSep = Separator;
			return (INVALID_HANDLE_VALUE);
		}
		*NameSep = Separator;
	}
	for (;;) {
		count = 1;
		size  = NrSizes[3];
		rc = WNetEnumResourceW(hEnum[3], &count, NetResource[3], &size);
		if (rc == ERROR_MORE_DATA) {
			NETRESOURCEW *nr;

			nr = realloc(NetResource[3], size);
			if (nr == NULL) {
				WNetCloseEnum(hEnum[3]);
				return (hEnum[3] = INVALID_HANDLE_VALUE);
			}
			NetResource[3] = nr;
			NrSizes[3]     = size;
			count          = 1;
			rc = WNetEnumResourceW(hEnum[3], &count, NetResource[3], &size);
		}
		if (rc != NO_ERROR) {
			WNetCloseEnum(hEnum[3]);
			return (hEnum[3] = INVALID_HANDLE_VALUE);
		}
		if (NetResource[3]->dwType == RESOURCETYPE_DISK &&
				NetResource[3]->lpRemoteName != NULL &&
				wcslen(NetResource[3]->lpRemoteName) > 3) {
			PCWSTR p = wcspbrk(NetResource[3]->lpRemoteName+2, L"\\/");

			if (p++ != NULL && Compare != NULL && PatternMatch(p, Compare)) {
				wcsncpy(FindData->cFileName, p, WSIZE(FindData->cFileName));
				FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
				return((HANDLE)(NetResource+3));
			}
		}
	}
}

#endif

static HANDLE FindFileHandle = INVALID_HANDLE_VALUE;

HANDLE MyFindFirst(PWSTR Name, FIND_DATA *FindData)
{
	#if defined(WIN32)
		if (wcsspn(Name, L"\\/") == 2) do {	/*no loop, for break only*/
			PWSTR p;
			HANDLE Result;

			StartInterruptCheck();
			if ((p = wcspbrk(Name+2, L"\\/")) == NULL)
				Result = EnumServers(Name, FindData);
			else if (wcspbrk(p+1, L"\\/") == NULL)
				Result = EnumShares(Name, p, FindData);
			else break;
			if (Result == INVALID_HANDLE_VALUE)
				MyFindClose((HANDLE)(p==NULL ? NetResource : NetResource+3));
			return (Result);
		} while (FALSE);
	#endif
	/*
	 * For enumerating normal filesystem directories, a pointer to the
	 * "real" handle is used instead of the handle returned by the system.
	 * I chose this method to avoid possible but unlikely handle value
	 * conflicts with the server and share enumeration handles.
	 * Currently, there is only one memory location for such handles, so
	 * only one MyFindFirst/MyFindNext/MyFindClose sequence may be in
	 * progress at any time.							25-Sep-2002 Ramo
	 */
	assert(FindFileHandle == INVALID_HANDLE_VALUE);
	FindFileHandle = FIND_FIRST(Name, FindData);
	return (FindFileHandle!=INVALID_HANDLE_VALUE ? &FindFileHandle
												 : INVALID_HANDLE_VALUE);
}

BOOL MyFindNext(HANDLE Handle, FIND_DATA *FindData)
{
	#if defined(WIN32)
		if ((NETRESOURCEW **)Handle >= NetResource &&
			(NETRESOURCEW **)Handle <= NetResource+3) {
			HANDLE Result;

			if ((NETRESOURCEW **)Handle < NetResource+3)
				 Result = EnumServers(NULL, FindData);
			else Result = EnumShares (NULL, NULL, FindData);
			return (Result != INVALID_HANDLE_VALUE);
		}
	#endif
	assert(Handle == (HANDLE)&FindFileHandle);
	return (FIND_NEXT(FindFileHandle, FindData) != 0);
}

BOOL MyFindClose(HANDLE Handle)
{	BOOL Result;

	#if defined(WIN32)
		if ((NETRESOURCEW **)Handle >= NetResource &&
		    (NETRESOURCEW **)Handle <= NetResource+3) {
			INT i, i1, i2;

			if ((NETRESOURCEW **)Handle < NetResource+3) {
				i1 = 0;
				i2 = 2;
			} else i1 = i2 = 3;
			for (i = i2; i >= i1; --i) {
				if (hEnum[i] != INVALID_HANDLE_VALUE) {
					WNetCloseEnum(hEnum[i]);
					hEnum[i] = INVALID_HANDLE_VALUE;
				}
			}
			Enable();
			SetFocus(hwndCmd);
			return (TRUE);
		}
	#endif
	assert(Handle == (HANDLE)&FindFileHandle);
	Result = FIND_CLOSE(FindFileHandle);
	FindFileHandle = INVALID_HANDLE_VALUE;
	return (Result);
}

BOOL IsDirectory(FIND_DATA *FindData)
{
	return (IS_DIRECTORY(FindData));
}

BOOL IsHidden(FIND_DATA *FindData)
{
	return (IS_HIDDEN(FindData));
}

PCWSTR FoundName(FIND_DATA *FindData)
{
	return (FOUND_NAME(FindData));
}
