/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2010 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of source code
 *     15-Nov-2002	tag back in same file with NewPosition() (uses scrolling)
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *     10-Jan-2009	fixed identifier extraction after multi-byte UTF-8 chars
 *     12-Nov-2009	better check of Ctrl+Break interrupts when tagging
 */

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#include "winvi.h"
#include "page.h"

static PWSTR TagsFile;
static WCHAR TagCommand[264];

typedef struct tagTAGSTACK {
	struct tagTAGSTACK FAR *Next;
	ULONG					Pos;
	WCHAR					FName[1];
} TAGSTACK, FAR *LPTAGSTACK;
static LPTAGSTACK TagStack = NULL;

static VOID PushTag(VOID)
{	LPTAGSTACK SavePos;

	if (CurrFile == NULL || !*CurrFile) return;
	SavePos = _fcalloc(sizeof(TAGSTACK) + wcslen(CurrFile)*sizeof(WCHAR), 1);
	if (SavePos == NULL) return;
	SavePos->Next = TagStack;
	SavePos->Pos  = CountBytes(&CurrPos);
	wcscpy(SavePos->FName, CurrFile);
	TagStack = SavePos;
}

VOID PopTag(BOOL Jump)
{
	if (TagStack != NULL) {
		LPTAGSTACK ToFree;

		if (Jump) {
			if (wcsicmp(TagStack->FName, CurrFile) != 0) {
				wcscpy(TagCommand, L":e ");
				wcscat(TagCommand, TagStack->FName);
				if (!CommandExec(TagCommand)) return;
				JumpAbsolute(TagStack->Pos);
				NewScrollPos();
			} else {
				POSITION p;

				p.p = FirstPage;
				while (p.p->Next && TagStack->Pos > p.p->Fill) {
					TagStack->Pos -= p.p->Fill;
					p.p = p.p->Next;
				}
				if (TagStack->Pos > p.p->Fill) p.i = p.p->Fill;
				else p.i = (short)TagStack->Pos;
				p.f = 0;
				NewPosition(&p);
			}
			if (!HexEditMode) Position(1, '^', 0);	/*position to first non-space*/
			else ShowEditCaret();					/*show caret only*/
		}
		ToFree   = TagStack;
		TagStack = TagStack->Next;
		_ffree(ToFree);
	} else Error(330);
}

BOOL IsTagStackEmpty(VOID)
{
	return (TagStack == NULL);
}

static HANDLE OpenNextTagsFile(VOID)
{	PWSTR  p;
	INT    c;
	HANDLE File;

	for (p=TagsFile; *p && *p!=',' && *p!=';'; ++p);
	c = *p;
	*p = '\0';
	if (*TagsFile) {
		File = CreateFileW(TagsFile, GENERIC_READ,
						   FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
						   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (c) {
			*p = c;
			TagsFile = ++p;
		} else TagsFile = p;
	} else File = INVALID_HANDLE_VALUE;
	return (File);
}

static INT Fill;

static INT NextChar(HFILE hf)
{	static CHAR	Buf[128];
	static INT	Index;

	if (Index >= Fill) {
		Index = 0;
		if ((Fill = _lread(hf, Buf, sizeof(Buf))) <= 0) {
			Fill = 0;
			return ('\0');
		}
	}
	return (Buf[Index++]);
}

static VOID CombinePath(PWSTR Buffer, INT Size)
{	PWSTR p1 = TagsFile, p2;

	while (p1 > Tags) {
		switch (*--p1) {
			case ',': case ';':
				if (p1 == TagsFile-1) continue;
				/*FALLTHROUGH*/
			case '/': case '\\':
				break;
			default:
				continue;
		}
		break;
	}
	if (*p1=='/' || *p1=='\\') {
		INT i;

		for (p2=p1; p2>Tags && p2[-1]!=',' && p2[-1]!=';'; --p2);
		if ((i = p1 - p2 + 1) < (INT)(Size - wcslen(Buffer))) {
			_fmemmove(Buffer + i, Buffer, 2*wcslen(Buffer) + 2);
			_fmemcpy (Buffer, p2, 2*i);
		}
	}
}

extern WCHAR BufferW[300];

BOOL Tag(PWSTR Identifier, WORD Flags)
	/* Flags: 1=change file even if modified (command ":tag! ...")
	 *		  2=display tag command in status line
	 */
{	HFILE hf;
	PWSTR  p;

	if (Flags & 2) {
		_snwprintf(BufferW, sizeof(BufferW), L":tag%s %s",
			Flags & 1 ? L"!" : L"", Identifier);
		NewStatus(0, BufferW, NS_BUSY);
	}
	TagsFile = Tags;
	StartInterruptCheck();
	while (!Interrupted && *TagsFile) {
		if ((hf = (HFILE)OpenNextTagsFile()) != HFILE_ERROR) {
			INT c, i, Delimiter;

			Fill = 0;
			do {
				p = Identifier;
				for (;;) {
					if (!(c = NextChar(hf))) break;
					if (c==' ' || c=='\t') {
						BOOL ClearStatus = FALSE;

						if (p==Identifier) continue;
						if (*p) break;

						/*matched, extract file name...*/
						do; while ((c=NextChar(hf)) == ' ' || c=='\t');
						wcscpy(TagCommand, L":e ");
						i = 3;
						if (Flags & 1) TagCommand[2] = '!';
						{	BOOL WithinQuotes = FALSE, Done = FALSE;

							do {
								switch (c) {
									case ' ':
									case '\t': if (WithinQuotes) break;
											   /*FALLTHROUGH*/
									case '\0':
									case '\r':
									case '\n': Done = TRUE;
											   break;
									case '"':  WithinQuotes ^= TRUE^FALSE;
								}
								if (Done) break;
								TagCommand[i] = c;
								c = NextChar(hf);
							} while (++i < WSIZE(TagCommand)-1);
						}
						TagCommand[i] = '\0';

						PushTag();
						if (TagCommand[3]!='\\' && TagCommand[3]!='/'
												&& TagCommand[4]!=':')
							CombinePath(TagCommand+3, sizeof(TagCommand) -
													  3*sizeof(TagCommand[0]));
						if (wcsicmp(TagCommand+3, CurrFile)) {
							if (!CommandExec(TagCommand)) {
								_lclose(hf);
								NewStatus(0, L"", NS_NORMAL);
								Enable();
								PopTag(FALSE);
								ShowEditCaret();
								return (FALSE);
							}
						} else ClearStatus = Flags & 2;

						/*extract positioning command now...*/
						while (c==' ' || c=='\t') c = NextChar(hf);
						i = 1;
						Delimiter = '\0';
						while (c && c!='\r' && c !='\n') {
							if (c == Delimiter) Delimiter = '\0';
							else if (Delimiter == '\0') {
								if (c=='/' || c=='?') Delimiter = c;
								else if (c==';') break;
							}
							TagCommand[i] = c;
							if (++i == sizeof(TagCommand)-1) break;
							if (c == '\\') {
								c = NextChar(hf);
								if (c!='\0' && c!='\r' && c!='\n') {
									TagCommand[i] = c;
									if (++i == sizeof(TagCommand)-1) break;
									c = NextChar(hf);
								}
							} else c = NextChar(hf);
						}
						_lclose(hf);
						TagCommand[i] = '\0';
						{	BOOL SaveMagic		= MagicFlag;
							BOOL SaveIgnoreCase	= IgnoreCaseFlag;
							BOOL SaveWrapScan	= WrapScanFlag;

							SaveMatchList();
							MagicFlag	   =
							IgnoreCaseFlag = FALSE;
							WrapScanFlag   = TRUE;
							if (!CommandExec(TagCommand)) ClearStatus = FALSE;
							WrapScanFlag   = SaveWrapScan;
							IgnoreCaseFlag = SaveIgnoreCase;
							MagicFlag	   = SaveMagic;
							RestoreMatchList();
						}
						Enable();
						ShowEditCaret();
						if (ClearStatus) NewStatus(0, L"", NS_NORMAL);
						return (TRUE);
					}
					if (c != *p) break;
					++p;
					continue;
				}
				while (c && c!='\n') c = NextChar(hf);
				if (CheckInterrupt()) {
					TagsFile = L"";
					break;
				}
			} while (c);
			_lclose(hf);
		}
	}
	Enable();
	ShowEditCaret();
	Error(Interrupted ? 218 : 329);
	return (FALSE);
}

PWSTR ExtractIdentifier(PPOSITION pPos)
{	PWSTR		p = TagCommand;
	INT			c;
	POSITION	Pos;

	Pos = CurrPos;
	if (!(CharFlags(c = CharAt(&Pos)) & 8)) return (NULL);
	do {
		if (pPos != NULL) *pPos = Pos;
		if ((c = GoBackAndChar(&Pos)) == C_EOF) {
			*p++ = CharAt(&Pos);
			break;
		}
	} while (CharFlags(c) & 8);
	while (CharFlags(c = AdvanceAndChar(&Pos)) & 8)
		if (p-TagCommand < WSIZE(TagCommand)-1) *p++ = c;
	*p = '\0';
	return (*TagCommand ? TagCommand : NULL);
}

VOID TagCurrentPosition(VOID)
{	PWSTR p;

	if ((p = ExtractIdentifier(NULL)) != NULL) Tag(p, 2);
}
