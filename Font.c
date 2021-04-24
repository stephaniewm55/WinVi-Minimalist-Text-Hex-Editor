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
 *      3-Dec-2000	first publication of source code
 *      6-Apr-2004	CheckGlyphs() only called if a new font is selected from
 *              	the Settings dialog.  In Windows XP, the calls to
 *              	GetGlyphOutline() makes the system unstable if three or
 *              	more WinVi instances are calling this API at the same time.
 *              	A blue screen crash stops the system sooner or later,
 *              	in most cases immediately.
 *     29-Apr-2004	setting Defaultvalues if CheckGlyphs() fails
 *     20-Jul-2007	codepage setting of UTF-8
 */

#include <windows.h>
#include <memory.h>
#include "winvi.h"
#include "page.h"

extern BOOL  IsFixedFont;
extern CHAR  QueryString[150];
extern DWORD QueryTime;
extern BYTE  CaseConvert[256];

static BOOL CheckGlyphs(HDC hDC, BYTE GlyphsAvail[32])
{
#if 0
	INT			 c;
	DWORD		 n, nn;
	GLYPHMETRICS gm;
	static MAT2	 mat2 = {{0,1},{0,0},{0,0},{0,1}};
	CHAR		 *Buf, *Bufn;

	nn = GetGlyphOutline(hDC, 127, GGO_NATIVE, &gm, 0, NULL, &mat2);
	if ((LONG)nn <= 0) return FALSE;
	Bufn = (CHAR *)LocalAlloc(LPTR, (UINT)(2*nn + 1));
	if (!Bufn) return FALSE;
	Buf = Bufn + nn;
	GetGlyphOutline(hDC, 127, GGO_NATIVE, &gm, 2*nn, Bufn, &mat2);
	for (c=0; c<=255; ++c) {
		n = GetGlyphOutline(hDC, c, GGO_NATIVE, &gm, nn+1, Buf, &mat2);
		if (gm.gmCellIncX == 0 || (n == nn && !memcmp(Buf, Bufn, (size_t)n))) {
			CharFlags[c] |=  2;
			GlyphsAvail[c >> 3] &= ~(1 << (c & 7));
		} else {
			CharFlags[c] &= ~2;
			GlyphsAvail[c >> 3] |= 1 << (c & 7);
		}
	}
	LocalFree((HLOCAL)Bufn);
#endif
	return TRUE;
}

#if defined(WIN32)

VOID NewCharmap(UINT OemCp, UINT Charset)
{
	#if 0
	//	static CHARSETINFO	csi;
	//	INT					c;
	//	CHAR				c2;
	//	WCHAR				wc;
	//	UINT				r, n, f1, f2, f3;
	//
	//	SetLastError(0);
	//	csi.ciACP = csi.ciCharset = n = f1 = f2 = f3 = 0;
	//	r = TranslateCharsetInfo(&Charset, &csi, TCI_SRCCHARSET);
	//	if (csi.ciACP) {
	//		for (c=0; c<256; ++c) {
	//			if (!(CharFlags[c] & 1)) {
	//				c2 = c;
	//				if (MultiByteToWideChar(OemCp, 0, &c2, 1, &wc, 1) == 1) {
	//					if (WideCharToMultiByte(csi.ciACP, WC_DISCARDNS, &wc, 1,
	//											&c2, 1, "", NULL) == 1) {
	//						if (c2) {
	//							MapTable[0][c] = c2;
	//							++n;
	//						} else ++f3;
	//					} else ++f2;
	//				} else ++f1;
	//			}
	//		}
	//	}
	//	#if 1
	//		wsprintf(QueryString, "%d characters remapped, %d+%d+%d fail,\n"
	//				 "OemCP=%d, CharSet=%d,\n"
	//				 "TciRet=%d, ciCharset=%d, ciACP=%d,\n"
	//				 "LastError=%d",
	//				 n,f1,f2,f3, OemCp,Charset, r,csi.ciCharset,csi.ciACP,
	//				 GetLastError());
	//		QueryTime = GetTickCount();
	//	#endif
	#else
		PARAM_NOT_USED(OemCp);
		PARAM_NOT_USED(Charset);
	#endif
}

#endif

INT	 HexTextHeight, TextTextHeight;
BYTE DefaultAvail[32] = {  0,   0,   0,   0, 255, 255, 255, 255,
						 255, 255, 255, 255, 255, 255, 255, 127,
						   0,   0,   6,   0, 254, 255, 255, 255,
						 255, 255, 255, 255, 255, 255, 255, 255};

BOOL NewFont(HFONT *Font, PLOGFONT LogFont, PINT pHeight, BYTE GlyphsAvail[32])
{	HFONT hFont, hOldFont;
	HDC   hDC;
	INT   Height;

	hFont = CreateFontIndirect(LogFont);
	if (!hFont) return (FALSE);
	*Font = hFont;
	hDC = GetDC(NULL);
	if (!hDC) {
		*pHeight = 8;
		return (TRUE);
	}
	hOldFont = SelectObject(hDC, hFont);
	{	SIZE Size;

		Height = GetTextExtentPoint(hDC, "X", 1, &Size) ? Size.cy : 8;
		if (!Height) Height = 8;
		*pHeight = Height;
	}
	AveCharWidth   = 0;		/*set again in first paint operation*/
	IsFixedFont	   = FALSE;	/*...*/
	if (GlyphsAvail != NULL && WinVersion >= MAKEWORD(95,3))
		if (!CheckGlyphs(hDC, GlyphsAvail))
			memcpy(GlyphsAvail, DefaultAvail, sizeof(DefaultAvail));
	#if defined(WIN32)
		NewCharmap(GetOEMCP(), LogFont->lfCharSet);
	#endif
	#if 0
	//	{	CHAR Buf[256], OutBuf[1024], *p = OutBuf;
	//		INT i;
	//
	//		i = GetFontData(hDC, *(LPDWORD)(LPSTR)"cmap", 0, Buf, sizeof(Buf));
	//		if (i == sizeof(Buf)) {
	//			for (i = 0; i < sizeof(Buf); ++i) {
	//				wsprintf(p, "%02x ", (BYTE)Buf[i]);
	//				p += lstrlen(p);
	//				if ((i & 15) == 15) p[-1] = '\n';
	//			}
	//		} else wsprintf(OutBuf, "GetFontData returned %d\n"
	//								"GetLastError returned %d",
	//								i, GetLastError());
	//		MessageBox(NULL, OutBuf, "Test Message", MB_OK);
	//	}
	#endif
	SelectObject(hDC, hOldFont);
	ReleaseDC(NULL, hDC);
	if (Font == &TextFont) {
		extern HFONT SpecialFont;
		static LOGFONT LogSpecialFont = {
			12, 0, 0, 0, 400, 0, 0, 0,
			SYMBOL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "WingDings"
		};

		if (SpecialFont) DeleteObject(SpecialFont);
		if (LogFont->lfHeight)
			LogSpecialFont.lfHeight = LogFont->lfHeight * 6 / 7;
		SpecialFont = CreateFontIndirect(&LogSpecialFont);
	}
	return (TRUE);
}

INT OemCodePage = CP_OEMCP, AnsiCodePage = CP_ACP;

VOID SetNewCodePage(VOID)
{
#if 0
	#if defined(WIN32)
		INT	  c, Acp;
		CHAR  c2;
		WCHAR wc;
		UINT  n, f1, f2, f3, f4, f5, f6, f7, f8;

		n = f1 = f2 = f3 = f4 = f5 = f6 = f7 = f8 = 0;
		SetLastError(0);
		if (!OemCodePage)  OemCodePage  = CP_OEMCP;
		if (!AnsiCodePage) AnsiCodePage = CP_ACP;
		for (c=0; c<256; ++c) {
			if (CharFlags[c] & 1) continue;
			c2 = c;
			if (MultiByteToWideChar(OemCodePage, 0, &c2, 1, &wc, 1) != 1) {
				++f1;
				continue;
			}
			if (WideCharToMultiByte(AnsiCodePage, 0, &wc, 1,
									&c2, 1, "", NULL) != 1) {
				++f2;
				continue;
			}
			if (!c2) {
				++f3;
				continue;
			}
			MapTable[0][c] = c2;
			++n;
		}
		#if 1
		{	CHAR DefaultOemCP[8], DefaultACP[8];
			if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTCODEPAGE,
							  DefaultOemCP, sizeof(DefaultOemCP)) <= 0)
				lstrcpy(DefaultOemCP, "unknown");
			if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE,
							  DefaultACP, sizeof(DefaultACP)) <= 0)
				lstrcpy(DefaultACP, "unknown");
			wsprintf(QueryString,
					 "%d characters remapped, %d+%d+%d+%d+%d+%d+%d+%d fail,\n"
					 "OemCP=%d, ACP=%d, LastError=%d,\n"
					 "DefaultOemCP=%s, DefaultACP=%s",
					 n, f1, f2, f3, f4, f5, f6, f7, f8,
					 OemCodePage, AnsiCodePage, GetLastError(),
					 DefaultOemCP, DefaultACP);
			QueryTime = GetTickCount();
		}
		#endif
		if (n) InvalidateText();
		if (!(Acp = AnsiCodePage)) {
			CHAR DefaultACP[8];
			if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE,
							  DefaultACP, sizeof(DefaultACP)) > 0)
				Acp = (INT)strtol(DefaultACP, NULL, 10);
		}
		{	CHAR *AlphaBits = NULL;
			switch (Acp) {
				case 1250:	/*Central European (merely Eastern European)*/
							AlphaBits =	"\0\364\0\364\50\204\10\326"
										"\377\377\177\377\377\377\177\177";
							break;
				case 1251:	/*Cyrillic*/
							AlphaBits =	"\13\364\1\364\56\205\34\365"
										"\377\377\377\377\377\377\377\377";
							break;
				case 1252:	/*Latin 1*/
				case 1254:	/*Turkish*/
							AlphaBits =	"\0\24\0\224\0\0\0\0"
										"\377\377\177\377\377\377\177\377";
							break;
				case 1253:	/*Greek*/
							AlphaBits =	"\0\24\0\224\4\0\0\327"
										"\377\377\373\377\377\377\377\177";
							break;
			}
			if (AlphaBits != NULL) {
				for (c=0; c<128; ++c) {
					if (AlphaBits[c>>3] & (1 << (c & 7)))
						 CharFlags[c+128] |=  8;
					else CharFlags[c+128] &= ~8;
				}
			}
		}
		#if 1
		{	static struct {
				BYTE Position, CaseConv[5];
				/*central european, cyrillic, latin1, greek, turkish*/
			} Convert[] = {
				{'I',  { 'i',  'i',  'i',  'i', 0xfd}},
				{'i',  { 'I',  'I',  'I',  'I', 0xdd}},
				{0x81, {   0, 0x83,    0,    0,    0}},
				{0x83, {   0, 0x81,    0,    0,    0}},
				{0x8d, {0x9d, 0x9d,    0,    0,    0}},
				{0x8e, {0x9e, 0x9e,    0,    0,    0}},
				{0x8f, {0x9f, 0x9f,    0,    0,    0}},
				{0x90, {   0, 0x80,    0,    0,    0}},
				{0x9d, {0x8d, 0x8d,    0,    0,    0}},
				{0x9e, {0x8e, 0x8e,    0,    0,    0}},
				{0x9f, {0x8f, 0x8f, 0xff,    0, 0xff}},
				{0xa1, {   0, 0xa2,    0,    0,    0}},
				{0xa2, {   0, 0xa1,    0,    0,    0}},
				{0xa5, {   0, 0xb4,    0,    0,    0}},
				{0xa8, {   0, 0xb8,    0,    0,    0}},
				{0xaa, {0xba, 0xba,    0,    0,    0}},
				{0xaf, {0xbf, 0xbf,    0,    0,    0}},
				{0xb4, {   0, 0xa5,    0,    0,    0}},
				{0xb8, {   0, 0xa8,    0,    0,    0}},
				{0xba, {0xaa, 0xaa,    0,    0,    0}},
				{0xbc, {0xbe,    0,    0,    0,    0}},
				{0xbe, {0xbc,    0,    0,    0,    0}},
				{0xbf, {0xaf, 0xaf,    0,    0,    0}},
				{0xdc, {0xfc, 0xfc, 0xfc,    0, 0xfc}},
				{0xdd, {0xfd, 0xfd, 0xfd,    0,  'i'}},
				{0xde, {0xfe, 0xfe, 0xfe,    0, 0xfe}},
				{0xdf, {   0, 0xff,    0,    0,    0}},
				{0xfc, {0xdc, 0xdc, 0xdc,    0, 0xdc}},
				{0xfd, {0xdd, 0xdd, 0xdd,    0,  'I'}},
				{0xfe, {0xde, 0xde, 0xde,    0, 0xde}},
				{0xff, {   0, 0xdf, 0x9f,    0, 0x9f}},
				{0}
			};

			if (Acp >= 1250 && Acp <= 1254)
				for (c=0; Convert[c].Position; ++c) {
					BYTE Old = Convert[c].Position;
					BYTE New = Convert[c].CaseConv[Acp-1250];

					CaseConvert[Old] = New ? New : Old;
				}
		}
		#else
		//	{	CHAR  cUpLo[2];
		//		WCHAR wcUpLo[2];
		//
		//		for (c=0; c<256; ++c) {
		//			if (MultiByteToWideChar(Acp, 0, &c2, 1, &wc, 1) != 1) {
		//				++f4;
		//				continue;
		//			}
		//			wcUpLo[0] = (WCHAR)CharUpperW((LPWSTR)(DWORD)(WORD)wc);
		//			wcUpLo[1] = (WCHAR)CharLowerW((LPWSTR)(DWORD)(WORD)wc);
		//			if (wcUpLo[0] != wcUpLo[1]) {
		//				if (WideCharToMultiByte(Acp, 0, wcUpLo, 2,
		//										cUpLo, 2, "", NULL) != 2) {
		//					++f5;
		//					continue;
		//				}
		//				if (cUpLo[0] == cUpLo[1]) {
		//					++f6;
		//					continue;
		//				}
		//				if (!cUpLo[0] || !cUpLo[1]) {
		//					++f7;
		//					continue;
		//				}
		//				if (cUpLo[0]!=c2 && cUpLo[1]!=c2) {
		//					++f8;
		//					continue;
		//				}
		//				CaseConvert[(BYTE)cUpLo[0]] = cUpLo[1];
		//				CaseConvert[(BYTE)cUpLo[1]] = cUpLo[0];
		//			}
		//		}
		//	}
		#endif
	#endif
#endif
}
