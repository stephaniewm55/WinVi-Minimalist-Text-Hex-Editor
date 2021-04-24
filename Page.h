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
 *		3-Dec-2000	first publication of this source file
 *      1-Jul-2007	handling of page overlapping CR-LF
 */

#define PAGESIZE	 4096

typedef struct tagPAGE {
	LPBYTE		   PageBuf;				 /*character buffer of page			*/
	struct tagPAGE FAR *Next, FAR *Prev; /*page chaining					*/
	ULONG		   LastAccess;			 /*last access (used for paging)	*/
	LONG		   PageFilePos;			 /*position on paging file			*/
	UINT		   Fill;				 /*used amount of bytes in PageBuf	*/
	UINT		   PageNo;				 /*length of chain in Prev			*/
	ULONG		   Newlines;			 /*no of newline character sequences*/
	DWORD		   Flags;				 /*page status flags				*/
} PAGE, *PPAGE, FAR *LPPAGE;
extern LPPAGE FirstPage, LastPage;

#define ISSAFE	   1					 /*bits in PAGE.Flags*/
#define CHECK_CRLF 2

typedef struct tagPOSITION {
	LPPAGE p;
	UINT   i;
	UINT   f;	/*flags see below*/
} POSITION, *PPOSITION;
extern POSITION ScreenStart, CurrPos;

#define POSITION_PREVENT_CRLF_COMBINATION	1
#define POSITION_UTF8_HIGH_SURROGATE_DONE	2

#define UTF8_80_BF				3
#define UTF8_1_80_BF			1
#define UTF8_2_80_BF			2
#define UTF8_3_80_BF			3
#define UTF8_COMPLETE_COUNT(x)	((x) >> 2)
#define UTF8_COMPLETE_INCR		4
#define UTF8_MISMATCH			(-1)

typedef struct {
	POSITION Pos;
	ULONG	 ByteCount;
	ULONG	 Pixels;			/*line width needed for horizontal scrollbar*/
	INT 	 nthContdLine;
	INT 	 Flags;
} LINEINFO, *PLINEINFO;
extern PLINEINFO LineInfo;
/*LineInfo:		   describes visible screen lines and one invisible line*/

/*Flags used in LineInfo.Flags...*/
#define LIF_VALID			  1	/*position is correct						  */
#define LIF_CONTD_NEXTLINE	  2	/*line has no eol character (continued or EOF)*/
#define LIF_EMPTY_LINE		  4	/*special cursor for empty lines			  */
#define LIF_EOF 			  8	/*line displayed as eof ('~')				  */
#define LIF_REFRESH_LINE	 16	/*line must be repainted					  */
#define LIF_REFRESH_BKGND	 32	/*moved background should be refreshed		  */

/*Flags for CountNewlinesInBuf...*/
#define CNLB_LASTWASCR		  1	/*last character was \r						  */
#define CNLB_LASTWASNUL		  2	/*last character was \r						  */
#define CNLB_UTF8_1FOLLOWING  4	/*1 following bytes still missing			  */
#define CNLB_UTF8_2FOLLOWING  8	/*2 following bytes still missing			  */
#define CNLB_UTF8_3FOLLOWING 12	/*3 following bytes still missing			  */
#define CNLB_UTF8_MISMATCH	 16	/*not correctly encoded as UTF-8			  */
#define CNLB_UTF8_INCREMENT	 32	/*UTF-8 sequence increment (2 bytes or more)  */
#define CNLB_UTF8_COUNT	 0xffe0	/*count of UTF-8 sequences (2 bytes or more)  */

extern BYTE const MapEbcdicToAnsi[256];
extern BYTE const MapAnsiToEbcdic[256];
#define C_BOM	0xfeff
#define C_EOF	(-1)
#define C_ERROR (-2)
#define C_UNDEF (-3)
#define C_CRLF	(-4)
#define C_MIN	C_CRLF

extern POSITION SelectStart;
extern BOOL		Selecting;
extern ULONG	SelectBytePos;
extern BOOL		SessionEnding, AdditionalNullByte;

/*paint.c*/
void   Jump(PPOSITION);
long   NewPosition(PPOSITION);
int    ComparePos(PPOSITION, PPOSITION);
void   InvalidateArea(PPOSITION, LONG, UINT);

/*file.c*/
void   SetAltFileName(PCWSTR, PPOSITION);

/*input.c*/
void   FindValidPosition(PPOSITION, WORD);
void   AdvanceToCurr(PPOSITION, /*INT,*/ INT);

/*search.c*/
BOOL   SearchIt(PPOSITION, INT);
BOOL   SearchAndStatus(PPOSITION, WORD);
BOOL   BuildReplaceString(PWSTR, LONG *, PPOSITION, LONG, WORD);

/*page.c*/
void   GetSystemTmpDir(PWSTR, INT);
BOOL   IncompleteLastLine(void);
LPPAGE NewPage(void);
BOOL   ReloadPage(LPPAGE);
void   FreeAllPages(void);
void   UnsafePage(PPOSITION);
BOOL   Reserve(PPOSITION, INT, WORD);
INT    ByteAt(PPOSITION);
INT    CharAt(PPOSITION);
INT    ByteAndAdvance(PPOSITION);
INT    CharAndAdvance(PPOSITION);
INT    AdvanceAndByte(PPOSITION);
INT    AdvanceAndChar(PPOSITION);
ULONG  GoBack(PPOSITION, ULONG);
INT    GoBackAndByte(PPOSITION);
INT    GoBackAndChar(PPOSITION);
ULONG  Advance(PPOSITION, ULONG);
void   BeginOfFile(PPOSITION);
void   EndOfFile(PPOSITION);
INT    CountNewlinesInBuf(LPBYTE, INT, LONG[9], DWORD *);
void   CorrectLineInfo(LPPAGE, UINT, LPPAGE, UINT, ULONG, INT);
ULONG  CountBytes(PPOSITION);
ULONG  CountLines(PPOSITION);
void   VerifyPageStructure(void);

/*insdel.c*/
BOOL   InsertBuffer(LPBYTE, ULONG, WORD);
BOOL   Yank(PPOSITION, ULONG, WORD);
BOOL   EnterDeleteForUndo(PPOSITION, ULONG, WORD);
BOOL   EnterInsertForUndo(PPOSITION, ULONG);

/*tags.c*/
PWSTR  ExtractIdentifier(PPOSITION);

/*charflags.c*/
BYTE CharFlags(INT);
