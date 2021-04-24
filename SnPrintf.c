/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 2009      by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      1-Jan-2009	initial version
 */

#if _MSC_VER

#include <windows.h>
#include <stdio.h>
#include "WinVi.h"

/* This module defines a new snprintf() wrapper avoiding the need 
 * for having special work-arounds when using Microsoft's compiler.
 */
/*
int snprintf(char *buf, const size_t size, const char *format, ...)
{	va_list	arglist;
	int		result;

	va_start(arglist, format);
	result = _vsnprintf(buf, size, format, arglist);
	if (size > 0) buf[size-1] = '\0';
	if (result < 0) result = _vscprintf(format, arglist);
	va_end(arglist);
	return result;
}
*/
#endif
