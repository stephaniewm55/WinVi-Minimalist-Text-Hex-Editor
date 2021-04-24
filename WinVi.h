/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2012 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      3-Dec-2000	first publication of source code
 *      6-Dec-2000	correct cursor position after pixel 32767 with 16-bit ints
 *     27-Jul-2002	source file search.c now split into search.c and srchdlg.c
 *     12-Sep-2002	+EBCDIC quick hack (Christian Franke)
 *      6-Oct-2002	new global variable TabExpandDomains (Win32 only)
 *      3-Nov-2002	NS_READING_WRITING renamed to NS_BUSY
 *     11-Jan-2003	prototypes for new file misc.c
 *     12-Jan-2003	help contexts added
 *      6-Apr-2004	new global data for font glyph availability
 *     15-Mar-2005	avoid sign extension in ANSILOWER_CMP for MinGW gcc
 *      7-Mar-2006	new calculation of caret dimensions
 *     11-Jun-2007	new UTF encoding
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *     31-Aug-2010	DLL load from system directory only
 */

#define HINST HINSTANCE
#if !defined(ULONG)
# define ULONG unsigned long
#endif
#if !defined(MAKEWORD)
# define MAKEWORD(lo,hi) (WORD)((WORD)((WORD)(hi) << 8) + (BYTE)(lo))
#endif

#if defined(WIN32)
# define COMMAND	  LOWORD(wPar)
# define NOTIFICATION HIWORD(wPar)
# define _fmemmove	  memmove
# define _fmemcpy	  memcpy
# define _fmemcmp	  memcmp
# define _fmemchr	  memchr
# define _fmemset	  memset
# define _fstrchr	  strchr
# define _fstrrchr	  strrchr
# define _fstrncmp	  strncmp
# define hmemcpy	  memcpy
# define huge
# define _fcalloc	  calloc
# define _frealloc	  realloc
# define _ffree		  free
# define LOWER_CMP(c1,c2)					\
		(CharLowerW((PWSTR)(DWORD)c1) !=	\
		 CharLowerW((PWSTR)(DWORD)c2))
# define MoveTo(h,x,y) MoveToEx(h,x,y,NULL)
#else
# define COMMAND	  wPar
# define NOTIFICATION HIWORD(lPar)
# define INT          int
# define CHAR         char
# if !defined(MAX_PATH)
#  define MAX_PATH	  129
# endif
# define LOWER_CMP(c1,c2) \
((char)(DWORD)AnsiLower(MAKELP(0, c1)) != (char)(DWORD)AnsiLower(MAKELP(0, c2)))
  typedef UINT (CALLBACK *LPCFHOOKPROC)(HWND, UINT, WPARAM, LPARAM);
  typedef CHAR const  *PCSTR;
#endif

#define wcsicmp _wcsicmp

#if !defined(MAKEINTRESOURCEW)
# define MAKEINTRESOURCEW(i) (LPWSTR)((DWORD)((WORD)(i)))
#endif
#if !defined(WSIZE)
# define WSIZE(w) (sizeof(w)/sizeof(w[0]))
#endif

#if !defined(CP_UTF8)
# define CP_UTF8 65001
#endif

#define LOADSTRING(a,b,c,d)	  LoadString(a,(b)+Language,c,d)
#define LOADSTRINGW(a,b,c,d)  LoadStringW(a,(b)+Language,c,d)
#define MAKEINTRES(a)		  MAKEINTRESOURCE((a)+Language)
#define MAKEINTRESW(a)		  MAKEINTRESOURCEW((a)+Language)

#define CTRL(c)		((c) & 31)
#define LCASE(c)	((c) | ('a'-'A'))

#if !defined(__LCC__)
# define PARAM_NOT_USED(p) (VOID)p
#else
# define PARAM_NOT_USED(p)
#endif

/*toolbar index values...*/
#define IDB_NEW		0
#define IDB_OPEN	1
#define IDB_SAVE	2
#define IDB_PRINT	3
#define IDB_CUT		4
#define IDB_COPY	5
#define IDB_PASTE	6
#define IDB_UNDO	7
#define IDB_SEARCH	8
#define IDB_SRCHAGN	9
#define IDB_ANSI   10
#define IDB_OEM	   11
#define IDB_UTF8   12
#define IDB_UTF16  13
#define IDB_HEX	   14

/*SwitchCursor flags...*/
#define SWC_STATUS_OR_TOOLBAR 1		/*mouse in non-text area*/
#define SWC_TEXTAREA		  2		/*mouse in text area*/
#define SWC_SIZEGRIP		  4		/*mouse in right lower corner*/
#define SWC_TEXTCURSOR		  8		/*text cursor switch between IBeam & Arrow*/
#define SWC_HOURGLASS_ON	 16		/*working, show hourglass*/
#define SWC_HOURGLASS_OFF	 32		/*switch back*/

/*NewStatus styles*/
#define NS_NORMAL	0
#define NS_GRAY		1
#define NS_ERROR	2
#define NS_BUSY		3
#define NS_TOOLTIP	4

/*CharSet values*/
#define CS_AUTOENCODING	-1
#define CS_ANSI			 0
#define CS_OEM			 1
#define CS_UTF8			 2
#define CS_UTF16LE		 3
#define CS_UTF16BE		 4
#define CS_EBCDIC		 5

#define ISHEX(x) (((x) >= '0' && (x) <= '9') || \
				  (((x) | ('a'-'A')) >= 'a' && ((x) | ('a'-'A')) <= 'f'))

typedef enum {HelpAll,				  HelpEditTextmode,	 HelpEditHexmode,
			  HelpInsertmode,		  HelpReplacemode,
			  HelpEditC,  HelpEditD,  HelpEditY,	 HelpEditZ,	 HelpEditMark,
			  HelpEditFT, HelpEditZZ, HelpEditShift, HelpEditShell,
			  HelpEditDoublequote,	  HelpPrint,	 HelpSearch, HelpSearchDlg,
			  HelpSettingsProfile, HelpSettingsFiletype, HelpSettingsDisplay,
			  HelpSettingsEdit,    HelpSettingsFiles,    HelpSettingsMacros,
			  HelpSettingsShell,   HelpSettingsColors,   HelpSettingsFonts,
			  HelpSettingsPrint,   HelpSettingsLanguage,
			  HelpExCommand, HelpExRead,  HelpExEdit,	  HelpExNext,
			  HelpExRewind,  HelpExWrite, HelpExFilename, HelpExArgs,
			  HelpExVi,		 HelpExCd,	  HelpExTag,	  HelpExShell,
			  HelpExSubstitute}
		HELPCONTEXT;

typedef enum {ConfirmReplace, ConfirmSkip, ConfirmAll, ConfirmAbort}
		SEARCHCONFIRMRESULT;

typedef enum {ProfileSaveAndClear, ProfileRestore, ProfileSwitchAndLoad,
			  ProfileAdd,		   ProfileDelete,  ProfileClose}
		PROFILEOP;

typedef struct {
	LONG ScreenLine;	/*0 for first line, 1 and above for continuation lines*/
	LONG PixelOffset;	/*number of pixels from left border of leftmost char*/
} COLUMN, *PCOLUMN;

typedef struct tagREPLIST {
	struct tagREPLIST FAR *Next;
	WCHAR				  Buf[40];
} REPLIST, FAR *LPREPLIST;
extern REPLIST  RepList;

typedef enum {CommandMode, CmdGotFirstChar, GetDblQuoteChar, PositionTf,
			  PositionQuote, PositionBracket, InsertMode, ReplaceMode} MODEENUM;
extern MODEENUM Mode;

typedef enum {COMMAND_CARET, COMMAND_CARET_EMPTYLINE, COMMAND_CARET_DOUBLEHEX,
			  INSERT_CARET, REPLACE_CARET, REPLACE_CARET_DOUBLEHEX} CARET_TYPE;

extern UINT		Language;
extern BOOL		HexEditMode, TfForward, TfIsFind, ReplaceSingle;
extern BOOL		Interruptable, XOffState, FullRowExtend;
extern UINT		Interrupted, CmdFirstChar, Number;
extern INT		TextHeight;  /*screen line y difference in pixels*/
extern INT		VisiLines;	 /*number of fully or partly displayed text lines*/
extern LONG		FirstVisible;/*line numbr (starting at 0) for bitmap alignment*/
extern INT		AveCharWidth;
extern COLORREF TextColor;
extern HCURSOR	hcurCurrent, hcurArrow, hcurIBeam, hcurHourGlass, hcurSizeNwSe;
extern HFONT	TextFont, HexFont;
extern BYTE		TextGlyphsAvail[32], HexGlyphsAvail[32];
extern HINST	hInst;
extern HWND		hwndMain, hwndCmd, hwndErrorParent;
extern WORD		WinVersion;
extern RECT		ClientRect;
extern PCWSTR	CurrFile, CurrArg;
extern BOOL		IsAltFile, Unsafe, CommandWithExclamation, CaretActive;
extern BOOL		MouseActivated, LcaseFlag, BreakLineFlag, ShowMatchFlag;
extern BOOL		ReadOnlyFlag, AutoRestoreFlag, AutoIndentFlag, AutoWriteFlag;
extern BOOL		IgnoreCaseFlag, WrapScanFlag, TerseFlag, DefaultInsFlag;
extern BOOL		DefaultTmpFlag, QuitIfMoreFlag, MagicFlag, WrapPosFlag;
extern BOOL		ListFlag, NumberFlag, HexNumberFlag, FullRowFlag;
extern BOOL		MoreFilesWarned;
extern BOOL		ViewOnlyFlag;	/*view-only=restrictive read-only w/o editing*/
extern BOOL		ReadOnlyFile;	/*read-only for this file only*/
extern COLUMN	CurrCol;		/*aspired pixel position in logical line*/
extern ULONG	CurrColumn;	/*displayed character offset within line (for status)*/
extern ULONG	LinesInFile, SelectCount;
extern INT		CrsrLines;		/*number of positionable displayed text lines*/
							/*CrsrLines==VisiLines || CrsrLines==VisiLines-1*/
extern INT		CurrX, CurrLine;
extern LONG		CaretX;
extern INT		CaretY, CaretWidth, CaretHeight;
CARET_TYPE		CaretType;
extern INT		TabStop, ShiftWidth, MaxScroll, Disabled;
extern WCHAR	FileName[260];
extern PWSTR	TmpDirectory;
extern PWSTR	Tags;
extern LONG		PageThreshold;
extern ULONG	UndoLimit;
extern PCSTR	ApplName;
extern PCWSTR	ApplNameW;
extern UINT		CharSet;	/*see CS_... constants above*/
extern INT		UtfEncoding;/*0=no UTF, 8=UTF-8, 16=UTF-16*/
extern BOOL		UtfLsbFirst;
extern BOOL		LButtonPushed;
extern LONG     RelativePosition;
extern BOOL		Indenting;
extern INT		LinesVar, ColumnsVar;	/*used in ResizeWithLinesAndColumns()*/
extern BOOL		RefreshBitmap;
extern WCHAR	BitmapFilename[260];
extern WCHAR	AltFileName[260];
extern WCHAR	CommandBuf[1024];
extern BOOL		SuppressStatusBeep, DlgResult;
extern HELPCONTEXT HelpContext;

#if defined(WIN32)
extern WCHAR	TabExpandDomains[256];
#endif

/*------ FUNCTION PROTOTYPES... ------*/
/*winvi.c*/
BOOL AllocStringA(LPSTR*, LPCSTR);
BOOL AllocStringW(PWSTR*, PCWSTR);
PSTR SeparateStringA(PSTR*);
void SeparateAllStringsA(PSTR);
PWSTR SeparateStringW(PWSTR*);
void SeparateAllStringsW(PWSTR);
void ContextMenu(HWND, INT, INT);
void GenerateTitle(BOOL);
BOOL FreeSpareMemory(void);
INT  MessageLoop(BOOL);
BOOL DisplaySettingsCallback(HWND, UINT, WPARAM, LPARAM);
BOOL EditSettingsCallback(HWND, UINT, WPARAM, LPARAM);
BOOL FileSettingsCallback(HWND, UINT, WPARAM, LPARAM);
VOID ChangeLanguage(UINT);
BOOL LanguageCallback(HWND, UINT, WPARAM, LPARAM);

/*profile.c*/
BOOL SwitchProfile(PROFILEOP, PSTR);
VOID SwitchToMatchingProfile(PCWSTR);
VOID AddAllProfileNames(HWND);
void SaveSettings(void);
VOID SaveProfileSettings(VOID);
VOID SaveFiletypeSettings(VOID);
void SaveColorSettings(void);
void SaveFontSettings(void);
void SaveEditSettings(void);
void SaveDisplaySettings(void);
void SaveFileSettings(void);
VOID SaveShellSettings(VOID);
VOID SaveMappingSettings(VOID);
VOID SavePrintSettings(VOID);
VOID SaveAllSettings(VOID);
UINT GetDefaultLanguage(BOOL);
void LoadSettings(void);
void InsertFileList(HMENU);
void AddToFileList(PCWSTR);
void FileListCommand(INT);
VOID SaveLanguage(LPSTR);
BOOL DebugMessages(VOID);

/*paint.c*/
void InitPaint(void);
void ReleasePaintResources(void);
void AdjustWindowParts(INT, INT);
void NewScrollPos(void);
void ExtendSelection(void);
void ScrollJump(INT);
INT  GetTextWidth(HDC, WCHAR*, INT);
BOOL ScrollLine(INT);
void InvalidateText(void);
VOID UnicodeConvert(WCHAR*, WCHAR*, INT);
INT  UnicodeToOemChar(WCHAR);
void TextAreaOnly(RECT*, BOOL);
INT  StartX(BOOL);
void TextLine(HDC, INT, INT);
void Paint(HDC, PRECT);
INT  GetPixelLine(INT, BOOL);
void GetXPos(PCOLUMN);
void CommandEdit(UINT);
void CommandDone(BOOL);

/*misc.c*/
void Error(INT, ...);
INT  ErrorBox(INT, INT, ...);
BOOL Confirm(HWND, INT, ...);
INT  FormatShell(PSTR, LPSTR, LPSTR, PSTR);
INT  FormatShellW(PWSTR, LPSTR, LPSTR, PWSTR);
BOOL ShowUrl(PSTR);
VOID Help(VOID);

/*status.c*/
void RecalcStatusRow(void);
void PaintStatus(HDC, PRECT, PRECT);
void NewStatus(INT, PWSTR, INT);
void SetStatusCommand(PWSTR, BOOL);

/*toolbar.c*/
void LoadTips(void);
void PaintTools(HDC, PRECT);
void Disable(WORD);
void Enable(void);
BOOL IsToolButtonEnabled(INT);
void EnableToolButton(INT, BOOL);
void ToggleHexMode(void);
void DisableMenuItems(INT, HMENU);
void CheckClipboard(void);
void SetCharSet(UINT, UINT);
BOOL MouseForToolbar(UINT, WPARAM, LPARAM);
void TooltipTimer(void);
void TooltipHide(INT);
LRESULT CALLBACK TooltipProc(HWND, UINT, WPARAM, LPARAM);

/*font.c*/
BOOL NewFont(HFONT*, PLOGFONT, PINT, BYTE[32]);

/*file.c*/
void SetUnsafe(void);
void SetSafe(BOOL);
BOOL AskForSave(HWND, WORD);
void LowerCaseFnameA(PSTR);
void LowerCaseFnameW(PWSTR);
VOID SetDefaultNl(VOID);
void New(HWND, BOOL);
void WatchMessage(PWSTR);
void ShowFileName(void);
void FileError(PCWSTR, INT, INT);
VOID JumpAbsolute(ULONG);
BOOL Open(PCWSTR, WORD);
void FilenameCmd(PWSTR);
void Save(PCWSTR);
BOOL EditCmd(PCWSTR);
BOOL ReadCmd(LONG, PCWSTR);
BOOL NextCmd(PCWSTR);
BOOL WriteCmd(LONG, LONG, PCWSTR, WORD);

/*insdel.c*/
INT  CharSize(INT);
void InsertChar(INT);
BOOL DeleteChar(void);
void InsertEol(UINT, INT);
void DeleteEol(BOOL);
void DeleteForward(UINT);
void Join(void);
void DeleteSelected(INT);
BOOL Put(LONG, BOOL);
BOOL Shift(LONG, WORD);

/*undo.c*/
void StartUndoSequence(void);
void DisableUndo(void);
void SetSafeForUndo(BOOL);
void Undo(WORD);

/*input.c*/
BOOL IsViewOnly(void);
void HorizontalScrollIntoView(void);
BOOL HideEditCaret(void);
void ShowEditCaret(void);
void SwitchCursor(WORD);
void GotKeydown(WPARAM);
void ShowSelect(void);
void SetNormalHelpContext(void);
void CommandLineDone(BOOL, PWSTR);
void Unselect(void);
VOID SelectAll(VOID);
void GotChar(INT, WORD);
void Scroll(INT, INT);
VOID ShowMatch(WORD);

/*mouse.c*/
void MouseInTextArea(UINT, WPARAM, INT, INT, INT, BOOL);
void MouseSelectText(UINT, WPARAM, LONG, BOOL);
BOOL MouseHandle(UINT, WPARAM, LPARAM);

/*command.c*/
BOOL NewViEditCmd(PWSTR);
BOOL QuitCmd(BOOL);
void ResizeWithLinesAndColumns(void);
BOOL CommandExec(PWSTR);

/*exec.c*/
PSTR ExpandSys32(PSTR);
BOOL ExecRunning(VOID);

/*filelist.c*/
BOOL  RemoveFileListEntry(INT);
void  ClearFileList(void);
INT   AppendToFileList(PCWSTR, INT, INT);
PWSTR GetFileListEntry(INT);
void  ArgsCmd(void);
INT   FindOrAppendToFileList(PCWSTR);

/*page.c*/
void  RemoveTmpFile(void);

/*search.c*/
WCHAR CharSetToUnicode(WCHAR);
BOOL  CheckInterrupt(void);
VOID  StartInterruptCheck(void);
BOOL  StartSearch(void);
PWSTR BuildMatchList(PWSTR, INT, INT *);
BOOL  SaveMatchList(VOID);
BOOL  RestoreMatchList(VOID);
VOID  FreeRepList(VOID);
BOOL  CheckParenPairs(VOID);
BOOL  SubstituteCmd(LONG, LONG, PWSTR);
void  SearchNewCset(void);

/*srchdlg.c*/
BOOL SearchAgain(WORD);
void SearchDialog(void);
SEARCHCONFIRMRESULT ConfirmSubstitute(void);

/*clipbrd.c*/
BOOL ClipboardCopy(void);
BOOL ClipboardCut(void);
BOOL ClipboardPaste(void);

/*position.c*/
BOOL Position(LONG, INT, INT);

/*tags.c*/
VOID PopTag(BOOL);
BOOL IsTagStackEmpty(VOID);
BOOL Tag(PWSTR, WORD);
VOID TagCurrentPosition(VOID);

/*bmp.c*/
void ReleaseBmp(INT);
BOOL PaintBmp(HDC, INT, PRECT, LONG, LONG);

/*pathexp.c*/
void PathExpCommand(UINT);
BOOL ExpandPath(HWND, WORD);
void PathExpCleanup(void);

/*settings.c*/
void EnableApply(void);
LRESULT CALLBACK SunkenBoxProc(HWND, UINT, WPARAM, LPARAM);
VOID CheckDefButton();
HWND GetCurrentTabSheet(VOID);
void Settings(void);
BOOL IsSettingsDialogMessage(PMSG);

/*c99snprintf.c*/
#if _MSC_VER
int snprintf(char *buf, const size_t size, const char *format, ...);
#endif

/*version.c*/
PWSTR GetViVersion(void);
