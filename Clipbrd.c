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
 *	   13-Mar-2003	clipboard paste with codepage conversion
 *     15-Jul-2007	support for UTF-16 and UTF-8
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 */

#include <windows.h>
#include <memory.h>
#include <wchar.h>
#include <stdlib.h>
#include "winvi.h"
#include "page.h"

extern INT OemCodePage, AnsiCodePage;

CHAR BinaryFormat[] = "WinVi bin %lu:";
CHAR BinaryPart[24];

static VOID EbcdicConvert(char huge *dst, LPSTR src, INT n)
{
	while (n--) {
		INT orig   = (BYTE)*src++;
		INT mapped = MapEbcdicToAnsi[orig];

		*dst++ = mapped != 127 ? mapped : orig;
	}
}

extern DWORD MyGetVersion(VOID);

BOOL ClipboardCopy(void)
{	HGLOBAL	  hMem;
	char huge *pMem;
	LONG	  lSize;
	BOOL	  WideCharConvert = FALSE;
	UINT	  Codepage = 0;

	/*prepare resources...*/
	if (!SelectCount) {
		Error(209);
		return (FALSE);
	}
	if (!OpenClipboard(hwndMain)) {
		ErrorBox(MB_ICONEXCLAMATION, 302);
		return (FALSE);
	}
	lSize = SelectCount + sizeof(BinaryPart);
	if (!(MyGetVersion() & 0x80000000U))
		if (UtfEncoding || CharSet == CS_OEM || AnsiCodePage != CP_ACP) {
			/*copy as wide chars...*/
			Codepage = CharSet == CS_OEM ? OemCodePage : AnsiCodePage;
			if (UtfEncoding != 16)
				lSize <<= 1;
			WideCharConvert = TRUE;
		}
	hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, lSize);
	if (!hMem) {
		CloseClipboard();
		ErrorBox(MB_ICONEXCLAMATION, 304);
		return (FALSE);
	}
	pMem = GlobalLock(hMem);
	if (pMem == NULL) {
		CloseClipboard();
		GlobalFree(hMem);
		ErrorBox(MB_ICONEXCLAMATION, 305);
		return (FALSE);
	}

	/*copy into memory...*/
	{	POSITION Pos;
		ULONG    Bytes = SelectCount;
		UINT     i;
		BOOL	 NullbyteFound = FALSE;

		Pos = SelectStart;
		if (UtfEncoding && WideCharConvert) {
			POSITION EndPos = Pos;
			WCHAR	 *pw = (WCHAR *)pMem;

			Advance(&EndPos, Bytes);
			//if (CountBytes(&Pos) != 0 || (i = CharAndAdvance(&Pos)) == C_BOM)
			i = CharAndAdvance(&Pos);
			for (;;) {
				if (ComparePos(&Pos, &EndPos) > 0)
					break;
				if (i == C_EOF) break;
				if (i == C_CRLF) {
					if ((char *)pw + 6 > pMem + lSize)
						break;
					*pw++ = '\r';
					i = '\n';
				}
				if ((char *)pw + 4 > pMem + lSize)
					break;
				*pw++ = i;
				i = CharAndAdvance(&Pos);
			}
			*pw = '\0';
		} else while (Bytes) {
			LPSTR p, pNull;

			CharAt(&Pos);	/*normalize position and enforce page load*/
			i = Pos.p->Fill - Pos.i;
			if (i > Bytes) i = (UINT)Bytes;
			p = Pos.p->PageBuf + Pos.i;
			if (!NullbyteFound && (pNull = _fmemchr(p, '\0', i)) != NULL) {
				if (ErrorBox(MB_ICONINFORMATION|MB_OKCANCEL, 328) == IDCANCEL) {
					CloseClipboard();
					GlobalUnlock(hMem);
					GlobalFree(hMem);
					return (FALSE);
				}
				NullbyteFound = TRUE;
				i = pNull - p + 1;
				if (CharSet == CS_EBCDIC) EbcdicConvert(pMem, p, i);
				else if (WideCharConvert) {
					MultiByteToWideChar(Codepage, 0, p, i, (WCHAR*)pMem, i);
					pMem += i;
				} else hmemcpy(pMem, p, i);
				pMem  += i;
				Bytes -= --i;
				Pos.i += i;
				i = wsprintf(BinaryPart, BinaryFormat, Bytes);
				if (WideCharConvert) {
					MultiByteToWideChar(Codepage, 0, BinaryPart, i,
													 (WCHAR*)pMem, i);
					pMem += i;
				} else hmemcpy(pMem, BinaryPart, i);
				pMem += i;
				continue;
			}
			if (CharSet == CS_EBCDIC)
				EbcdicConvert(pMem, Pos.p->PageBuf + Pos.i, i);
			else if (WideCharConvert) {
				MultiByteToWideChar(Codepage, 0, Pos.p->PageBuf + Pos.i, i,
												 (WCHAR*)pMem, i);
				if (CharSet == CS_OEM) {
					extern WCHAR const MapLowOemToUtf16[33];
					PWSTR			   pw = (WCHAR*)pMem;
					INT				   j;

					for (j=i; j>0; --j) {
						if (*pw < ' ') {
							switch (*pw) {
							case '\0':
							case '\t':
							case '\n':
							case '\r':	break;
							default:	*pw = MapLowOemToUtf16[*pw];
							}
						}
						++pw;
					}
				}
				pMem += i;
			} else hmemcpy(pMem, Pos.p->PageBuf + Pos.i, i);
			pMem  += i;
			Bytes -= i;
			Pos.i += i;
		}
	}

	/*unlock...*/
	GlobalUnlock(hMem);

	/*clear previous clipboard contents...*/
	if (!EmptyClipboard()) {
		CloseClipboard();
		GlobalFree(hMem);
		ErrorBox(MB_ICONEXCLAMATION, 303);
		return (FALSE);
	}

	/*finally, publish...*/
	SetClipboardData(WideCharConvert ? CF_UNICODETEXT
									 : CharSet == CS_OEM ? CF_OEMTEXT
														 : CF_TEXT, hMem);
	CloseClipboard();
	CheckClipboard();
	return (TRUE);
}

BOOL ClipboardCut(void)
{
	if (!ClipboardCopy()) return (FALSE);
	DeleteSelected(17);
	GetXPos(&CurrCol);
	ShowEditCaret();
	return (TRUE);
}

BOOL ClipboardPaste(void)
{
	HGLOBAL  hMem = 0;
	LPSTR	 lpMem;
	UINT	 uFormat = CharSet == CS_OEM ? CF_OEMTEXT : CF_TEXT;
	LONG	 lSize;
	MODEENUM SaveMode;
	BOOL	 FreeConversionMemory = FALSE;

	if (IsViewOnly()) return(FALSE);

	/*prepare resources...*/
	if (!OpenClipboard(hwndMain)) {
		ErrorBox(MB_ICONEXCLAMATION, 302);
		return (FALSE);
	}

	/*first check for binary data containing null bytes...*/
	if (UtfEncoding != 16 && (hMem = GetClipboardData(uFormat)) != 0) {
		lSize = GlobalSize(hMem);
		if ((lpMem = GlobalLock(hMem)) != NULL) {
			LPSTR lp;
			LONG  lRemain;

			lp = _fmemchr(lpMem, '\0', (size_t)lSize);
			if (lp != NULL && (lp-lpMem+sizeof(BinaryPart)) <= (size_t)lSize
						   && lp[1]==BinaryFormat[0]
						   && _fmemchr(lp+1, '\0', lSize-(lp+1-lpMem)) != NULL
						   && _fmemcmp(lp+1, BinaryFormat, 10) == 0) {
				/*insert first part of null containing binary data...*/
				SaveMode = Mode;
				if (Mode != InsertMode && Mode != ReplaceMode) {
					StartUndoSequence();
					Mode = InsertMode;
				}
				if (SelectCount) DeleteSelected(17);
				else EnterDeleteForUndo(&CurrPos, 0, 1); /*force new undo elem*/
				Mode = SaveMode;
				HideEditCaret();
				if (*lpMem) InsertBuffer(lpMem, (UINT)(lp-lpMem), 0);
				lRemain = strtoul(lp+11, &lp, 10);
				if (*lp != ':') lSize = 0;
				else {
					if (lSize >= (lp - lpMem) + lRemain) lSize = lRemain;
					else lSize = 0;
					lpMem = lp + 1;
				}
				if (lSize) InsertBuffer(lpMem, (UINT)lSize, 1);
				GetXPos(&CurrCol);
				ShowEditCaret();

				/*clean up...*/
				GlobalUnlock(hMem);
				CloseClipboard();
				return (TRUE);
			}
			GlobalUnlock(hMem);
			hMem = 0;
		}
	}

	/*then, if running on Windows NT, try to get wide chars...*/
	if (!(MyGetVersion() & 0x80000000U) && CharSet != CS_EBCDIC) {
		UINT Cp;

		if (UtfEncoding == 8)
			 Cp = CP_UTF8;
		else if (CharSet == CS_ANSI)
			 Cp = AnsiCodePage;
		else Cp = OemCodePage;
		if (Cp) {
			HGLOBAL	hWideCharMem = GetClipboardData(CF_UNICODETEXT);
			WCHAR	*lpWideCharMem;

			if (hWideCharMem) {
				lSize		  = GlobalSize(hWideCharMem);
				lpWideCharMem = GlobalLock(hWideCharMem);
				if (lpWideCharMem != NULL) {
					INT	   nSizeRequired;
					PWCHAR wp;

					lSize >>= 1;
					wp = wmemchr(lpWideCharMem, '\0', lSize);
					if (wp != NULL) lSize = wp - lpWideCharMem;
					if (UtfEncoding == 16) {
						lSize <<= 1;
						if (UtfLsbFirst) {
							hMem  = hWideCharMem;
							lpMem = (LPSTR)lpWideCharMem;
						} else {
							hMem  = GlobalAlloc(GMEM_MOVEABLE, lSize);
							if (hMem) {
								lpMem = GlobalLock(hMem);
								if (lpMem != NULL) {
									INT i = 0;

									while ((i += 2) <= lSize) {
										lpMem[i-2] = *lpWideCharMem >> 8;
										lpMem[i-1] = *lpWideCharMem++ & 255;
									}
									FreeConversionMemory = TRUE;
								} else {
									GlobalFree(hMem);
									hMem = 0;
								}
							}
						}
					} else {
						nSizeRequired = WideCharToMultiByte
										(Cp, 0, lpWideCharMem, lSize,
										 NULL, 0, NULL, NULL);
						hMem = GlobalAlloc(GMEM_MOVEABLE, nSizeRequired);
						if (hMem) {
							lpMem = GlobalLock(hMem);
							if (lpMem != NULL) {
								lSize = WideCharToMultiByte
										(Cp, 0, lpWideCharMem, lSize,
										 lpMem, nSizeRequired, NULL, NULL);
								FreeConversionMemory = TRUE;
							} else {
								GlobalFree(hMem);
								hMem = 0;
							}
						}
					}
				}
				if (!hMem) GlobalUnlock(hWideCharMem);
			}
		}
	}

	/*next, get text clipboard data in non-wide format...*/
	if (!hMem) {
		hMem = GetClipboardData(uFormat);
		if (!hMem) {
			CloseClipboard();
			Error(306);
			return (FALSE);
		}
		lSize = GlobalSize(hMem);
		#if !defined(WIN32)
			if (lSize > 65535) {
				CloseClipboard();
				ErrorBox(MB_ICONSTOP, 307);
				return (FALSE);
			}
		#endif
		lpMem = GlobalLock(hMem);
		if (lpMem == NULL) {
			CloseClipboard();
			ErrorBox(MB_ICONEXCLAMATION, 305);
			return (FALSE);
		}
		{	LPSTR p;

			if ((p = _fmemchr(lpMem, '\0', lSize)) != NULL)
				lSize = p - lpMem;
		}
		if (CharSet == CS_EBCDIC) {
			HGLOBAL	hConvMem = GlobalAlloc(GMEM_MOVEABLE, lSize);

			if (hConvMem) {
				LPBYTE lpConvMem = GlobalLock(hConvMem);

				if (lpConvMem != NULL) {
					INT	   i;
					LPBYTE lpMem2 = lpMem;

					lpMem = lpConvMem;
					for (i = lSize; i > 0; --i)
						*lpConvMem++ = MapAnsiToEbcdic[*lpMem2++];
					GlobalUnlock(hMem);
					hMem = hConvMem;
					FreeConversionMemory = TRUE;
				} else GlobalFree(hConvMem);
			}
		}
	}
	SaveMode = Mode;
	if (Mode != InsertMode && Mode != ReplaceMode) {
		StartUndoSequence();
		Mode = InsertMode;
	}
	if (SelectCount) DeleteSelected(17);
	else EnterDeleteForUndo(&CurrPos, 0, 1);	/*force new undo element*/
	Mode = SaveMode;
	HideEditCaret();

#if 0
//	if (lSize && UtfEncoding != 16) {
//		LPSTR lp;
//		LONG  lRemain;
//
//		lp = _fmemchr(lpMem, '\0', (size_t)lSize);
//		if (lp != NULL && (lp-lpMem+sizeof(BinaryPart)) <= (size_t)lSize
//					   && lp[1]==BinaryFormat[0]
//					   && _fmemcmp(lp+1, BinaryFormat, 10) == 0) {
//			/*insert first part of null containing binary data...*/
//			if (*lpMem) InsertBuffer(lpMem, (UINT)(lp-lpMem), 0);
//			lRemain = strtoul(lp+11, &lp, 10);
//			if (*lp != ':') lSize = 0;
//			else {
//				if (lSize >= (lp - lpMem) + lRemain) lSize = lRemain;
//				else lSize = 0;
//				lpMem = lp + 1;
//			}
//		} else if (lp) lSize = lp-lpMem;
//	}
#endif

	/*insert clipboard now...*/
	if (lSize) InsertBuffer(lpMem, (UINT)lSize, 1);
	GetXPos(&CurrCol);
	ShowEditCaret();

	/*clean up...*/
	GlobalUnlock(hMem);
	if (FreeConversionMemory) GlobalFree(hMem);
	CloseClipboard();
	return (TRUE);
}
