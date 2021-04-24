/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 2002      by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *     22-Jul-2002	source file created
 */

#ifdef NDEBUG
 #define assert(p) ((void)0)
#else

 #define assert(p) ((p) ? (void)0: (void)AssertionFail(#p, __FILE__, __LINE__ ))
 void AssertionFail(char *Expression, char *FileName, int Line);

#endif
