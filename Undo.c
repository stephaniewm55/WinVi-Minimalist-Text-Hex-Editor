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
 *      3-Dec-2000	first publication of this source file
 *     22-Jul-2002	use of private myassert.h
 *      9-Apr-2006	fixed positioning from 2nd hexadecimal nibble
 */

#include <windows.h>
#include <malloc.h>
#include <memory.h>
#include "myassert.h"
#include "winvi.h"
#include "page.h"

typedef struct tagUNDO {
	struct tagUNDO	FAR *Prev, FAR *Next, FAR *UndoRef;
	ULONG			Pos;
	ULONG			DelFill;
	ULONG			Inserted;
	HGLOBAL			DelMem;
	BYTE			Flags;
} UNDO, FAR *LPUNDO;
#define UD_NEWOP	  1	/*flag for separating undo operations*/
#define UD_SAFE		  2	/*file is safe when undone*/
#define UD_BYUNDO	  4	/*operation was undo*/
#define UD_SELECT	  8	/*deleted area was selected*/
#define UD_START	 16	/*undo operation started here (for undo repeat '.')*/
#define UD_GLOBALMEM 32 /*GlobalAlloc() used instead of _fmalloc()*/

LPUNDO FirstUndo, LastUndo, NextSequenceUndo;
int    UndoSequences, GlobalUndoFlags = UD_SAFE;
ULONG  UndoLimit = (ULONG)-1, UndoMemUsed = 0;

static BOOL NewUndo(void)
{	LPUNDO lpUndo;

	if ((lpUndo = (LPUNDO)_fcalloc(sizeof(UNDO), 1)) == NULL)
		return (FALSE);
	UndoMemUsed += sizeof(UNDO);
	if (LastUndo) {
		LastUndo->Next = lpUndo;
		lpUndo->Prev = LastUndo;
	} else FirstUndo = lpUndo;
	lpUndo->Pos   = (ULONG)-1;
	lpUndo->Flags = GlobalUndoFlags;
	LastUndo = NextSequenceUndo = lpUndo;
	return (TRUE);
}

void StartUndoSequence(void);

void CheckForUndoneToRelease(BOOL EnterNewOp)
{
	if (LastUndo != NextSequenceUndo) {
		while (LastUndo!=NULL && LastUndo!=NextSequenceUndo) {
			LPUNDO ToDelete = LastUndo;

			if (LastUndo->Flags & UD_NEWOP && UndoSequences) --UndoSequences;
			LastUndo = LastUndo->Prev;
			if (ToDelete->DelFill) {
				if (ToDelete->Flags & UD_GLOBALMEM) {
					UndoMemUsed -= GlobalSize(ToDelete->DelMem);
					GlobalFree(ToDelete->DelMem);
				} else UndoMemUsed -= ToDelete->DelFill;
			}
			UndoMemUsed -= sizeof(UNDO);
			_ffree(ToDelete);
		}
		if (LastUndo != NULL) LastUndo->Next = NULL;
		else FirstUndo = NULL;
		NextSequenceUndo = LastUndo;
		if (EnterNewOp) StartUndoSequence();
	}
}

static BOOL StartWithRemove, Redoing;

void StartUndoSequence(void)
{
	if (ViewOnlyFlag) return;
	if (!(GlobalUndoFlags & UD_BYUNDO) && Redoing) {
		Redoing = FALSE;
		if (LastUndo!=NULL && LastUndo->UndoRef!=NULL)
			NextSequenceUndo = LastUndo->UndoRef;
	}
	if (StartWithRemove) CheckForUndoneToRelease(FALSE);
	if (UndoLimit!=(ULONG)-1 && UndoMemUsed>UndoLimit && FirstUndo!=NULL) {
		do {
			do {
				LPUNDO ToDelete = FirstUndo;

				if ((FirstUndo = FirstUndo->Next) == NULL) {
					LastUndo = NextSequenceUndo = NULL;
					UndoSequences = 1;
				}
				if (ToDelete->DelFill) {
					if (ToDelete->Flags & UD_GLOBALMEM) {
						UndoMemUsed -= GlobalSize(ToDelete->DelMem);
						GlobalFree(ToDelete->DelMem);
					} else UndoMemUsed -= ToDelete->DelFill;
				}
				UndoMemUsed -= sizeof(UNDO);
				_ffree(ToDelete);
			} while (FirstUndo && !(FirstUndo->Flags & UD_NEWOP));
		} while (FirstUndo!=NULL && UndoMemUsed>UndoLimit);
		--UndoSequences;
		if (FirstUndo) FirstUndo->Prev = NULL;
	}
	if (!LastUndo || !(LastUndo->Flags & UD_NEWOP)
				  ||   LastUndo->Pos != (ULONG)-1) {
		if (!NewUndo()) {
			NextSequenceUndo = NULL;
			return;
		}
		++UndoSequences;
	}
	LastUndo->Flags |= UD_NEWOP | GlobalUndoFlags;
	GlobalUndoFlags &= UD_BYUNDO;
	NextSequenceUndo = LastUndo;
}

void DisableUndo(void)
{
	while (FirstUndo) {
		LastUndo = FirstUndo->Next;
		if (FirstUndo->Flags&UD_GLOBALMEM && FirstUndo->DelMem)
			GlobalFree(FirstUndo->DelMem);
		_ffree(FirstUndo);
		FirstUndo = LastUndo;
	}
	FirstUndo = LastUndo = NextSequenceUndo = NULL;
	EnableToolButton(IDB_UNDO, FALSE);
	UndoSequences = 0;
	UndoMemUsed	  = 0;
	Redoing		  = FALSE;
	SetSafeForUndo(FALSE);
}

static void EnableUndo(void)
{
	if (GlobalUndoFlags & UD_NEWOP) StartUndoSequence();
	EnableToolButton(IDB_UNDO, TRUE);
}

BOOL EnterDeleteForUndo(PPOSITION Pos, ULONG Length, WORD Flags)
	/*Flags: 1=enter as selected, reselect after undo
	 *		 2=remove last undo information, cannot be undone later
	 */
{	LPBYTE   NewBuf = NULL;
	HGLOBAL  NewMem = 0;
	POSITION p;
	ULONG	 Inx;
	ULONG	 ByteCount = CountBytes(Pos);
	ULONG	 OldSize;
	LPUNDO	 Reallocated = NULL;

	if (Flags & 2) {
		if (!LastUndo->Inserted) return (FALSE);
		--LastUndo->Inserted;
		return (TRUE);
	}
	CheckForUndoneToRelease(TRUE);
	EnableUndo();
	if (!LastUndo ||
		(LastUndo->Pos != (ULONG)-1
		 && (LastUndo->Pos != ByteCount - LastUndo->Inserted
			 || (int)(Flags & 1) != ((LastUndo->Flags & UD_SELECT) != 0)
			 || !(LastUndo->Flags & UD_GLOBALMEM
				  && LastUndo->DelFill+Length > 64000))))
		if (!NewUndo()) return (FALSE);
	if (LastUndo->Pos == (ULONG)-1) LastUndo->Pos = ByteCount;
	if (!LastUndo->DelFill) {
		OldSize = 0;
		if (Length > 64000) {
			LastUndo->Flags |= UD_GLOBALMEM;
			NewMem = GlobalAlloc(GMEM_MOVEABLE, Length);
			if (NewMem) NewBuf = GlobalLock(NewMem);
		} else {
			assert(!(LastUndo->Flags & UD_GLOBALMEM));
			LastUndo->Flags &= ~UD_GLOBALMEM;
			Reallocated = (LPUNDO)_frealloc(LastUndo,
											(size_t)(sizeof(UNDO)+Length));
			if (Reallocated == NULL) return (FALSE);
			NewBuf = (LPBYTE)Reallocated + sizeof(UNDO);
		}
	} else {
		if (LastUndo->Flags & UD_GLOBALMEM) {
			OldSize = GlobalSize(LastUndo->DelMem);
			if (LastUndo->DelFill + Length > OldSize) {
				NewMem = GlobalReAlloc(LastUndo->DelMem,
									   LastUndo->DelFill + Length,
									   GMEM_MOVEABLE);
			} else NewMem = LastUndo->DelMem;
			if (NewMem) NewBuf = GlobalLock(NewMem);
		} else {
			Reallocated = (LPUNDO)_frealloc(LastUndo,
											(size_t)(sizeof(UNDO) + Length
														  + LastUndo->DelFill));
			if (Reallocated == NULL) return (FALSE);
			NewBuf = (LPBYTE)Reallocated + sizeof(UNDO);
			OldSize = 0;
		}
	}
	UndoMemUsed -= OldSize;
	if (NewBuf == NULL) {
		if (NewMem) GlobalFree(NewMem);
		return (FALSE);
	}
	if (Reallocated!=NULL && Reallocated!=LastUndo) {
		/*correct any references to this object...*/
		if (NextSequenceUndo == LastUndo) NextSequenceUndo = Reallocated;
		if (Reallocated->Prev != NULL) Reallocated->Prev->Next = Reallocated;
		if (FirstUndo == LastUndo) FirstUndo = Reallocated;
		LastUndo = Reallocated;
	}
	Inx = LastUndo->DelFill;
	if (NewMem) {
		UndoMemUsed += GlobalSize(NewMem);
		LastUndo->DelMem = NewMem;
	} else UndoMemUsed += Length;
	LastUndo->DelFill += Length;
	if (Flags & 1) LastUndo->Flags |= UD_SELECT;
	p = *Pos;
	while (Length) {
		UINT n, c = CharAt(&p);

		if (c == C_EOF) {
			/*should not occur!...*/
			assert(c != C_EOF);		/*!?*/
			LastUndo->DelFill -= Length;
			break;
		}
		if (Length > p.p->Fill - p.i) n = p.p->Fill - p.i;
		else n = (UINT)Length;
		assert(p.p->PageBuf);
		assert(n > 0);
		assert(n <= PAGESIZE);
		assert(p.i+n <= PAGESIZE);
		hmemcpy(NewBuf + Inx, p.p->PageBuf + p.i, n);
		Inx += n;
		p.i += n;
		Length -= n;
	}
	if (NewMem) GlobalUnlock(NewMem);
	return (TRUE);
}

void SetSafeForUndo(BOOL FileChanged)
{	LPUNDO lpUndo;

	GlobalUndoFlags = UD_SAFE;
	if (FileChanged)
		for (lpUndo=LastUndo; lpUndo; lpUndo=lpUndo->Prev)
			lpUndo->Flags &= ~UD_SAFE;
	if (LastUndo != NULL && LastUndo->Flags & UD_BYUNDO)
		if (NextSequenceUndo != NULL && NextSequenceUndo->Next != NULL)
			NextSequenceUndo->Next->Flags |= UD_SAFE;
		#if 0
		//	else if (FirstUndo != NULL) FirstUndo->Flags |= UD_SAFE;
		#endif
}

BOOL EnterInsertForUndo(PPOSITION Pos, ULONG Length)
{	ULONG ByteCount = CountBytes(Pos);

	CheckForUndoneToRelease(TRUE);
	EnableUndo();
	if (!LastUndo || (LastUndo->Pos != (ULONG)-1 &&
					  LastUndo->Pos != ByteCount - LastUndo->Inserted))
		if (!NewUndo()) return (FALSE);
	if (LastUndo->Pos == (ULONG)-1) LastUndo->Pos = ByteCount;
	LastUndo->Inserted += Length;
	return (TRUE);
}

extern char	 QueryString[150];
extern DWORD QueryTime;

void Undo(WORD Flags)
	/* Flags:	0 =	called as vi-like undo:
	 *				undo of whole last operation,
	 *				a second undo reverts the undo operation,
	 *				entered as character 'u';
	 *			1 =	called as Windows-like undo:
	 *				undo last change at one position,
	 *				repeated undos go further back in history,
	 *				entered as <Alt+Bksp>, menu, or tool button;
	 *			2 = repeated vi-like undo ('.'):
	 *				can repeat both undo and redo;
	 *			4 = vi-like undo of whole line:
	 *				not yet implemented,
	 *				entered as character 'U';
	 */
{	LPUNDO	   lpUndo;
	BOOL	   fUndone = FALSE;
	ULONG	   StartSkip = (ULONG)-1;
	extern INT HexEditFirstNibble;

	if (Flags & 3) lpUndo = NextSequenceUndo;
	else {
		lpUndo = NextSequenceUndo = LastUndo;
		if (LastUndo != NULL) {
			LastUndo->Flags |= UD_START;
			if (LastUndo->Flags & UD_BYUNDO) {
				if (Redoing && LastUndo->UndoRef!=NULL)
					lpUndo = LastUndo->UndoRef;
				Redoing ^= TRUE^FALSE;
			} else Redoing = FALSE;
		}
	}
	if (lpUndo==NULL || (Redoing && !(lpUndo->Flags & UD_BYUNDO))) {
		Error(219);
		return;
	}
	if (IsViewOnly()) return;
	if (UtfEncoding == 16) {
		/*any undo ops must be double-byte, check before doing anything...*/
		LPUNDO lp = lpUndo;
		ULONG  Skip;

		while (lp != NULL) {
			if ((Skip = lpUndo->Pos) != (ULONG)-1) {
				if (lp->DelFill & 1 || lp->Inserted & 1) {
					ErrorBox(MB_ICONSTOP, 260);
					return;
				}
				if (StartSkip == (ULONG)-1) StartSkip = Skip;
				if (Flags & 1) break;
			}
			if (lp->Flags & UD_NEWOP && StartSkip != (ULONG)-1) break;
			lp = lp->Prev;
		}
		StartSkip = (ULONG)-1;
	}
	HideEditCaret();
	wsprintf(QueryString, "%s %s, used mem %lu",
			 Redoing ? "redoing" : "undoing",
			 lpUndo->Flags & UD_BYUNDO ? "undo" : "non-undo",
			 UndoMemUsed);
	QueryTime = GetTickCount();
	GlobalUndoFlags |= UD_BYUNDO;
	StartWithRemove = lpUndo->Flags & UD_START;
	StartUndoSequence();
	if (FirstUndo == NULL || (FirstUndo->Next == NULL && FirstUndo != lpUndo))
		lpUndo = NULL;
	StartWithRemove = TRUE;
	HexEditFirstNibble = -1;
	while (lpUndo != NULL) {
		POSITION Pos;
		ULONG	 Skip;

		if ((Skip = lpUndo->Pos) != (ULONG)-1) {
			if (StartSkip == (ULONG)-1) StartSkip = Skip;
			Pos.p = FirstPage;
			while (Skip >= Pos.p->Fill) {
				if (!Pos.p->Next) {
					if (Skip != Pos.p->Fill) {
						/*must not occur*/
						assert(Skip == Pos.p->Fill);	/*!?*/
						return;
					}
					break;
				}
				Skip -= Pos.p->Fill;
				Pos.p = Pos.p->Next;
			}
			if (SelectCount) {
				InvalidateArea(&SelectStart, SelectCount, 1);
				SelectCount = 0;
				UpdateWindow(hwndMain);
			}
			Pos.i = (UINT)Skip;
			Pos.f = 0;	/*TODO: check flags to be filled here*/
			if (lpUndo->Inserted) {
				SelectStart = Pos;
				SelectCount = lpUndo->Inserted;
				SelectBytePos = CountBytes(&SelectStart);
				DeleteSelected(3);
				fUndone   = TRUE;
				Indenting = FALSE;
			}
			if (lpUndo->DelFill) {
				LPSTR Buf;
				LONG  ByteCount = CountBytes(&Pos);

				if (lpUndo->Flags & UD_GLOBALMEM)
					 Buf = GlobalLock(lpUndo->DelMem);
				else Buf = (LPBYTE)lpUndo + sizeof(UNDO);
				if (Buf != NULL) {
					NewPosition(&Pos);
					InsertBuffer(Buf, lpUndo->DelFill, 0);
					fUndone = TRUE;
					if (lpUndo->Flags & UD_GLOBALMEM)
						GlobalUnlock(lpUndo->DelMem);
					/*reposition to current position...*/
					Pos.p = FirstPage;
					Pos.i = 0;
					Advance(&Pos, ByteCount);
					if (lpUndo->Flags & UD_SELECT && Flags & 1) {
						SelectStart = Pos;
						SelectCount = lpUndo->DelFill;
						SelectBytePos = CountBytes(&SelectStart);
						InvalidateArea(&SelectStart, SelectCount, 0);
						CheckClipboard();
					}
				} else ErrorBox(MB_ICONSTOP, 313);
			}
			if (Flags & 1) break;
		}
		if (lpUndo->Flags & UD_NEWOP && StartSkip != (ULONG)-1) break;
		lpUndo = lpUndo->Prev;
	}
	GlobalUndoFlags = 0;
	if (lpUndo != NULL) {
		if (lpUndo->Flags & UD_SAFE) {
			EnableToolButton(IDB_SAVE, FALSE);
			SetSafe(FALSE);
			GlobalUndoFlags = UD_SAFE;
		} else if (fUndone) SetUnsafe();
		LastUndo->UndoRef = NextSequenceUndo = lpUndo->Prev;
		#if 0
		//	if (NextSequenceUndo!=NULL && lpUndo->Flags & UD_BYUNDO
		//							   && !(NextSequenceUndo->Flags&UD_BYUNDO))
		//		NextSequenceUndo->Flags &= ~UD_START;
		#endif
	} else {
		if (fUndone) SetUnsafe();
		NextSequenceUndo = NULL;
	}
	if (Redoing) CheckForUndoneToRelease(FALSE);
	/*...undo information about redo and previous undo is not needed anymore
	 *   because the original undo information will be used instead.
	 */
	if (StartSkip != (ULONG)-1) {
		CurrPos.p = FirstPage;
		CurrPos.i = 0;
		Advance(&CurrPos, StartSkip);
		FindValidPosition(&CurrPos,
						  (WORD)(Mode==InsertMode || Mode==ReplaceMode));
		GetXPos(&CurrCol);
	}
	if (lpUndo==NULL || NextSequenceUndo==NULL)
		EnableToolButton(IDB_UNDO, FALSE);
	ShowEditCaret();
}
