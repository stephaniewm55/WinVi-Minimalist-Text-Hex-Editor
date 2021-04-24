/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several (w/ WIN32 support)   *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2007 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of source code
 */

#ifdef __LCC__
  #define FindFirstFileW(n,wfd) FindFirstFileW((n),(LPWIN32_FIND_DATA)(wfd))
  #define FindNextFileW(h,wfd)  FindFirstFileW((h),(LPWIN32_FIND_DATA)(wfd))
#endif

#if defined(WIN32)
  #define FIND_DATA				  WIN32_FIND_DATAW
  #define FIND_FIRST(Name, Data)  FindFirstFileW(Name, Data)
  #define FIND_NEXT(Handle, Data) FindNextFileW(Handle, Data)
  #define FIND_CLOSE(Handle)	  FindClose(Handle)
  #define FOUND_NAME(Data)		  (Data)->cFileName
  #define IS_HIDDEN(Data)		  ((Data)->dwFileAttributes \
												& FILE_ATTRIBUTE_HIDDEN)
  #define IS_DIRECTORY(Data)	  ((Data)->dwFileAttributes \
  												& FILE_ATTRIBUTE_DIRECTORY)
  #define TO_ANSI_STRCPY(Dst,Src) wcscpy(Dst, Src)
#else
  #include <dos.h>
  #if !defined(INVALID_HANDLE_VALUE)
	#define INVALID_HANDLE_VALUE  (HANDLE)0
  #endif
  #define FIND_DATA struct _find_t
  #define FIND_FIRST(Name, Data)  \
		  (_dos_findfirst(Name, _A_SUBDIR | _A_SYSTEM, Data) == 0 ? \
		   (HANDLE)~(int)INVALID_HANDLE_VALUE : INVALID_HANDLE_VALUE)
  #define FIND_NEXT(Handle, Data) (_dos_findnext(Data) == 0)
  #define FIND_CLOSE(Handle)	  TRUE
  #define FOUND_NAME(Data)		  (Data)->name
  #define IS_HIDDEN(Data)		  0 /*hidden files have not been enumerated*/
  #define IS_DIRECTORY(Data)	  ((Data)->attrib & _A_SUBDIR)
  #define TO_ANSI_STRCPY(Dst,Src) OemToAnsi(Src, Dst)
#endif
