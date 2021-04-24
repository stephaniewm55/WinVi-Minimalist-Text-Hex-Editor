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
 *		6-Dec-2000	color selectbox with double click on first 6 color boxes
 *	   12-Mar-2001	new custom color choice "user defined"
 *     23-Sep-2002	enable apply button
 *     11-Jan-2002	color refresh after profile switch
 */

#include <windows.h>
#include <commdlg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "resource.h"
#include "winvi.h"
#include "colors.h"
#include "status.h"
#include "paint.h"
#include "ctl3d.h"

UINT	 CurrColor;
COLORREF Colors[0x22] = {
	 RGB(000,000,000), RGB(128,000,000), RGB(000,128,000), RGB(128,128,000),
	 RGB(000,000,128), RGB(128,000,128), RGB(000,128,128), RGB(128,128,128),
	 RGB(192,192,192), RGB(255,000,000), RGB(000,255,000), RGB(255,255,000),
	 RGB(000,000,255), RGB(255,000,255), RGB(000,255,255), RGB(255,255,255),
};

static INT	SysColors[4] = {COLOR_WINDOW,	 COLOR_WINDOWTEXT,
							COLOR_HIGHLIGHT, COLOR_HIGHLIGHTTEXT};
static HWND CurrFrame, CurrColorHwnd;
static INT  CurrBmpInx;

static VOID ColorRadioButton(HWND hParentDlg, UINT uColor, BOOL WithFocus)
{	INT		 i;
	COLORREF c;

	if (uColor==0x10 && *BitmapFilename) {
		CheckRadioButton(hParentDlg, IDC_COLORS, IDC_BITMAP, IDC_BITMAP);
		if (WithFocus) SetFocus(GetDlgItem(hParentDlg, IDC_BITMAPBROWSE));
		return;
	}
	if (wUseSysColors & (1 << (uColor-0x10))) {
		CheckRadioButton(hParentDlg, IDC_COLORS, IDC_BITMAP, IDC_SYSTEM);
		return;		/*focus need not be set in this case*/
	}
	c = Colors[CurrColor];
	for (i=15; i>=0; --i) if (Colors[i] == c) break;
	if (i < 0) {
		if (!WithFocus) {
			CheckRadioButton(hParentDlg, IDC_COLORS, IDC_BITMAP, IDC_CUSTOM);
			return;
		}
		++i;
	}
	CheckRadioButton(hParentDlg, IDC_COLORS, IDC_BITMAP, IDC_COLORS);
	if (WithFocus) SetFocus(GetDlgItem(hParentDlg, IDC_COLOR0+i));
}

COLORREF CustomColors[16] = {0};

static VOID EditColor(UINT uColor)
{	static CHOOSECOLOR Cc;
	static COLORREF	   CustColors[16];
	HWND			   OldFocus = GetFocus();

	Cc.lStructSize	= sizeof(Cc);
	Cc.hwndOwner	= GetParent(CurrColorHwnd);
	Cc.hInstance	= (HWND)hInst;
	Cc.lpCustColors	= CustColors;
	Cc.rgbResult	= Colors[uColor];
	Cc.Flags		= CC_FULLOPEN | CC_RGBINIT;
	memcpy(CustColors, CustomColors, sizeof(CustColors));
	if (ChooseColor(&Cc)) {
		Colors[uColor] = Cc.rgbResult;
		wUseSysColors &= ~(1 << (uColor - 0x10));
		if (uColor == 0x10) *BitmapFilename = '\0';
		ColorRadioButton(Cc.hwndOwner, uColor, FALSE);
		InvalidateRect(CurrColorHwnd, NULL, TRUE);
		EnableApply();
		memcpy(CustomColors, CustColors, sizeof(CustomColors));
	}
	if (OldFocus) SetFocus(OldFocus);
}

LRESULT CALLBACK ColorProc(HWND hWnd, UINT uMsg, WPARAM wPar, LPARAM lPar)
	/*Color control window callback function*/
{	static CHAR	Title[20];
	UINT		uColor;

	switch (uMsg) {
		case WM_KILLFOCUS:
			ShowWindow(hWnd, SW_HIDE);
			UpdateWindow(hWnd);
			ShowWindow(hWnd, SW_SHOW);
			break;

		case WM_SETFOCUS:
			InvalidateRect(hWnd, NULL, TRUE);
			if (!GetWindowText(hWnd, Title, sizeof(Title)))   break;
			if ((uColor = (UINT)strtol(Title, 0, 16)) >= 0x22) break;
			{	PSTR p;

				switch (uColor) {
					case 0x10:	p = "COLOR_WINDOW";			break;
					case 0x11:	p = "COLOR_WINDOWTEXT";		break;
					case 0x12:	p = "COLOR_HIGHLIGHT";		break;
					case 0x13:	p = "COLOR_HIGHLIGHTTEXT";	break;
					case 0x17:	p = "COLOR_BTNFACE";
								#if !defined(WIN32)
									if (!Ctl3dEnabled()) p = "COLOR_WINDOW";
								#endif
															break;
					case 0x19:	p = "COLOR_BTNTEXT";		break;
					case 0x14:
					case 0x15:
					case 0x16:
					case 0x18:
					case 0x20:
					case 0x21:	p = "";						break;
					default:
						CheckRadioButton(GetParent(hWnd),
										 IDC_COLORS, IDC_BITMAP, IDC_COLORS);
						EnableApply();
						p = NULL;
						break;
				}
				if (p) {
					BOOL EnableBitmap = uColor == 0x10;

					SetDlgItemText(GetParent(hWnd), IDC_SYSCOLOR, p);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDC_SYSTEM),
								 *p != '\0');
					EnableWindow(GetDlgItem(GetParent(hWnd), IDC_BITMAP),
								 EnableBitmap);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDC_BITMAPTITLE),
								 EnableBitmap);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDC_BITMAPEDIT),
								 EnableBitmap);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDC_BITMAPBROWSE),
								 EnableBitmap);
					if (CurrFrame) ShowWindow(CurrFrame, SW_HIDE);
					CurrFrame = GetDlgItem(GetParent(hWnd),
										   IDC_FRAME0-0x10+uColor);
					CurrColor = uColor;
					SetWindowLong(CurrColorHwnd, GWL_STYLE,
						GetWindowLong(CurrColorHwnd, GWL_STYLE) & ~WS_TABSTOP);
					CurrColorHwnd = hWnd;
					SetWindowLong(CurrColorHwnd, GWL_STYLE,
						GetWindowLong(CurrColorHwnd, GWL_STYLE) |  WS_TABSTOP);
					ShowWindow(CurrFrame, SW_SHOW);
					ColorRadioButton(GetParent(hWnd), uColor, FALSE);
				} else {
					if (CurrColor == 0x10) *BitmapFilename = '\0';
					wUseSysColors &= ~(1 << (CurrColor - 0x10));
					Colors[CurrColor] = Colors[uColor];
					InvalidateRect(CurrColorHwnd, NULL, TRUE);
				}
			}
			break;

		case WM_LBUTTONDOWN:
			SetFocus(hWnd);
			break;

		case WM_LBUTTONDBLCLK:
			SetFocus(hWnd);
			if (!GetWindowText(hWnd, Title, sizeof(Title)))  break;
			if ((uColor = (UINT)strtol(Title, 0, 16)) < 0x10) break;
			EditColor(uColor);
			break;

		case WM_GETDLGCODE:
			return (DLGC_WANTARROWS);

		case WM_KEYDOWN:
			if (!GetWindowText(hWnd, Title, sizeof(Title)))		break;
			if ((uColor = (UINT)strtol(Title, 0, 16)) >= 0x22)	break;
			{	CHAR n;

				switch (wPar) {
					case VK_LEFT:	n = "\000\377\377\377\377\377\377\377"
										"\377\377\377\377\377\377\377\377"
										"\000\003\376\376\376\376\377"[uColor];
									break;
					case VK_RIGHT:	n = "\001\001\001\001\001\001\001\001"
										"\001\001\001\001\001\001\001\000"
										"\002\002\002\002\375\001\000"[uColor];
									break;
					case VK_UP:		n = "\000\007\007\007\007\007\007\007"
										"\370\370\370\370\370\370\370\370"
										"\000\377\377\377\377\377\377"[uColor];
									break;
					case VK_DOWN:	n = "\010\010\010\010\010\010\010\010"
										"\371\371\371\371\371\371\371\000"
										"\001\001\001\001\001\001\000"[uColor];
									break;
					default:		n = 0;
				}
				if (n)
					SetFocus(GetDlgItem(GetParent(hWnd), IDC_COLOR0+uColor+n));
			}
			break;

		case WM_ERASEBKGND:
			{	HBRUSH hbrBkgnd;
				RECT   Rect;

				GetClientRect(hWnd, &Rect);
				Rect.left+=1; Rect.top+=1; Rect.right-=1; Rect.bottom-=1;
				if (GetFocus() == hWnd) {
					FillRect((HDC)wPar, &Rect, SelBkgndBrush);
				}
				Rect.left+=2; Rect.top+=2; Rect.right-=2; Rect.bottom-=2;
				IntersectClipRect((HDC)wPar, Rect.left,  Rect.top,
											 Rect.right, Rect.bottom);
				if (!GetWindowText(hWnd, Title, sizeof(Title)))   break;
				if ((uColor = (UINT)strtol(Title, 0, 16)) >= 0x22) break;
				if (uColor==0x10 && *BitmapFilename)
					PaintBmp((HDC)wPar, CurrBmpInx, &Rect, 0, 0);
				else {
					hbrBkgnd = CreateSolidBrush(Colors[uColor]);
					if (hbrBkgnd) {
						FillRect((HDC)wPar, &Rect, hbrBkgnd);
						DeleteObject(hbrBkgnd);
					}
				}
			}
			return (TRUE);

		default:
			return (DefWindowProc(hWnd, uMsg, wPar, lPar));
	}
	return (0);
}

BOOL FileExists(PWSTR Filename)
{	PWSTR  p;
	HANDLE h;

	/*for more speed, check for incomplete network path...*/
	if (wcsspn(Filename, L"\\/")==2
		&& ((p=wcspbrk(Filename+2,L"\\/"))==NULL || wcspbrk(p+1,L"\\/")==NULL))
			return (FALSE);
	if ((h = CreateFileW(Filename, 0, 0, NULL, OPEN_EXISTING,
						 FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
		return FALSE;
	CloseHandle(h);
	return TRUE;
}

BOOL ColorsCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
	/*Color selection dialogbox callback function*/
{	static COLORREF	SaveFg, SaveBk, SaveSelFg, SaveSelBk, SaveShmFg, SaveShmBk,
					SaveEol,
					SaveStatN, SaveStatD, SaveStatB, SaveStatE, SaveStatEB;
	static WCHAR	SaveBm[260];
	static WORD		SaveSysColors;
	static BOOL		Applied, BitmapChanged;

	switch (uMsg) {
		case WM_INITDIALOG:
			if (ForegroundColor != NULL) {
				SaveFg = *ForegroundColor;
				/*invalidate foreground color in font selection dialog...*/
				ForegroundColor = NULL;
			} else SaveFg = TextColor;
			wcscpy(SaveBm, BitmapFilename);
			Colors[0x10]  = SaveBk	  = BackgroundColor;
			Colors[0x11]  = SaveFg;
			Colors[0x12]  = SaveSelBk = SelBkgndColor;
			Colors[0x13]  = SaveSelFg = SelTextColor;
			Colors[0x14]  = SaveShmBk = ShmBkgndColor;
			Colors[0x15]  = SaveShmFg = ShmTextColor;
			Colors[0x16]  = SaveEol	  = EolColor;
			Colors[0x17]  = SaveStatEB=	StatusErrorBackground;
			Colors[0x18]  = SaveStatE =	StatusStyle[NS_ERROR].Color;
			Colors[0x19]  = SaveStatN =	StatusStyle[NS_NORMAL].Color;
			Colors[0x20]  = SaveStatD =	StatusStyle[NS_GRAY].Color;
			Colors[0x21]  = SaveStatB =	StatusStyle[NS_BUSY].Color;
			SaveSysColors = wUseSysColors;
			if (lPar == 0)  CurrColor = 0x10;
			CurrColorHwnd = GetDlgItem(hDlg, IDC_EDIT_BG + CurrColor - 0x10);
			CurrBmpInx	  = 0;
			Applied		  = FALSE;
			{	INT i;

				for (i = IDC_EDIT_BG; i <= IDC_NEWLINE_FG; ++i)
					InvalidateRect(GetDlgItem(hDlg, i), NULL, TRUE);
			}
			if (*BitmapFilename) {
				LowerCaseFnameW(BitmapFilename);
				SetDlgItemTextW(hDlg, IDC_BITMAPEDIT, BitmapFilename);
			}
			ColorRadioButton(hDlg, CurrColor, FALSE);
			return (TRUE);

		case WM_COMMAND:
			if (NOTIFICATION != EN_KILLFOCUS) switch (COMMAND) {
				case IDC_COLORS:
					wUseSysColors &= ~(1 << (CurrColor - 0x10));
					if (CurrColor == 0x10) *BitmapFilename = '\0';
					InvalidateRect(CurrColorHwnd, NULL, TRUE);
					ColorRadioButton(hDlg, CurrColor, TRUE);
					SendMessage(hDlg, DM_SETDEFID, 102, 0);
					SendMessage(GetParent(hDlg), DM_SETDEFID, IDOK, 0);
					break;

				case IDC_SYSTEM:
					wUseSysColors |= 1 << (CurrColor - 0x10);
					if (CurrColor == 0x10) *BitmapFilename = '\0';
					Colors[CurrColor] = GetSysColor(SysColors[CurrColor-0x10]);
					InvalidateRect(CurrColorHwnd, NULL, TRUE);
					SendMessage(hDlg, DM_SETDEFID, 102, 0);
					SendMessage(GetParent(hDlg), DM_SETDEFID, IDOK, 0);
					break;

				case IDC_CUSTOM:
					wUseSysColors &= ~(1 << (CurrColor - 0x10));
					if (CurrColor == 0x10) *BitmapFilename = '\0';
					InvalidateRect(CurrColorHwnd, NULL, TRUE);
					SendMessage(hDlg, DM_SETDEFID, IDC_CHOOSECOLOR, 0);
					SendMessage(GetParent(hDlg), DM_SETDEFID, 102, 0);
					SetFocus(GetDlgItem(hDlg, IDC_CHOOSECOLOR));
					break;

				case IDC_CHOOSECOLOR:
					EditColor(CurrColor);
					break;

				case IDC_BITMAP:
					GetDlgItemTextW(hDlg, IDC_BITMAPEDIT,
								    BitmapFilename, WSIZE(BitmapFilename));
					CurrBmpInx = 1;
					ReleaseBmp(1);
					InvalidateRect(CurrColorHwnd, NULL, TRUE);
					SendMessage(hDlg, DM_SETDEFID, IDC_BITMAPBROWSE, 0);
					SendMessage(GetParent(hDlg), DM_SETDEFID, 102, 0);
					SetFocus(GetDlgItem(hDlg, IDC_BITMAPBROWSE));
					break;

				case IDC_BITMAPBROWSE:
					GetDlgItemTextW(hDlg, IDC_BITMAPEDIT,
								    BitmapFilename, WSIZE(BitmapFilename));
					{	HWND OldFocus = GetFocus();

						SetFocus(GetDlgItem(hDlg, IDC_BITMAPEDIT));
						CheckRadioButton(hDlg,IDC_COLORS,IDC_BITMAP,IDC_BITMAP);
						if (SelectBkBitmap(hDlg)) {
							LowerCaseFnameW(BitmapFilename);
							SetDlgItemTextW(hDlg,IDC_BITMAPEDIT,BitmapFilename);
							BitmapChanged = TRUE;
							CurrBmpInx = 1;
							ReleaseBmp(1);
							InvalidateRect(CurrColorHwnd, NULL, TRUE);
							OldFocus = GetDlgItem(hDlg, IDC_BITMAPEDIT);
							SendMessage(hDlg, DM_SETDEFID, 102, 0);
							SendMessage(GetParent(hDlg), DM_SETDEFID, IDOK, 0);
						} else if (OldFocus==GetDlgItem(hDlg, IDC_BITMAPBROWSE))
							SendMessage(hDlg, DM_SETDEFID, IDC_BITMAPBROWSE, 0);
						SetFocus(OldFocus);
					}
					break;

				case IDC_BITMAPEDIT:
					if (NOTIFICATION == EN_CHANGE) {
						GetDlgItemTextW(hDlg, IDC_BITMAPEDIT,
									    BitmapFilename, WSIZE(BitmapFilename));
						if (*BitmapFilename) {
							CheckRadioButton(hDlg, IDC_COLORS, IDC_BITMAP,
											 IDC_BITMAP);
							BitmapChanged = TRUE;
							if (FileExists(BitmapFilename)) {
								CurrBmpInx = 1;
								ReleaseBmp(1);
								InvalidateRect(CurrColorHwnd, NULL, TRUE);
							}
						}
						SetFocus(GetDlgItem(hDlg, IDC_BITMAPEDIT));
					}
					break;

				case ID_APPLY:
					Applied = TRUE;
					/*FALLTHROUGH*/
				case IDC_STORE:
				case IDOK:
					CurrBmpInx = 0;
					ReleaseBmp(1);
					if (BitmapChanged || *SaveBm != *BitmapFilename)
						ReleaseBmp(0);
					BackgroundColor	= SaveBk	 = Colors[0x10];
					TextColor		= SaveFg	 = Colors[0x11];
					SelBkgndColor	= SaveSelBk	 = Colors[0x12];
					SelTextColor	= SaveSelFg	 = Colors[0x13];
					ShmBkgndColor	= SaveShmBk	 = Colors[0x14];
					ShmTextColor	= SaveShmFg	 = Colors[0x15];
					EolColor		= SaveEol	 = Colors[0x16];
					StatusErrorBackground
									= SaveStatEB = Colors[0x17];
					StatusStyle[NS_ERROR].Color	 = SaveStatE = Colors[0x18];
					StatusStyle[NS_NORMAL].Color = SaveStatN = Colors[0x19];
					StatusStyle[NS_GRAY].Color	 = SaveStatD = Colors[0x20];
					StatusStyle[NS_BUSY].Color	 = SaveStatB = Colors[0x21];
					SaveSysColors	= wUseSysColors;
					MakeBackgroundBrushes();
					if (hwndMain) InvalidateRect(hwndMain, NULL, TRUE);
					DlgResult = TRUE;
					BitmapChanged = FALSE;
					wcscpy(SaveBm, BitmapFilename);
					switch (COMMAND) {
						case IDC_STORE:
							SaveColorSettings();
							break;
						case IDOK:
							EndDialog(hDlg, TRUE);
							break;
					}
					break;

				case IDCANCEL:
					ReleaseBmp(1);
					if (BitmapChanged || *BitmapFilename != *SaveBm) {
						ReleaseBmp(0);
						BitmapChanged = FALSE;
						Applied = TRUE;
						wcscpy(BitmapFilename, SaveBm);
					}
					if (Applied) {
						BackgroundColor	= SaveBk;
						TextColor		= SaveFg;
						SelBkgndColor	= SaveSelBk;
						SelTextColor	= SaveSelFg;
						ShmBkgndColor	= SaveShmBk;
						ShmTextColor	= SaveShmFg;
						EolColor		= SaveEol;
						StatusErrorBackground				  = SaveStatEB;
						StatusStyle[NS_ERROR].Color			  = SaveStatE;
						StatusStyle[NS_NORMAL].Color		  = SaveStatN;
						StatusStyle[NS_GRAY].Color			  = SaveStatD;
						StatusStyle[NS_BUSY].Color			  = SaveStatB;
						wUseSysColors	= SaveSysColors;
						MakeBackgroundBrushes();
						if (hwndMain) InvalidateRect(hwndMain, NULL, TRUE);
					}
					EndDialog(hDlg, FALSE);
			}
			return (TRUE);
	}
	return (FALSE);
}
