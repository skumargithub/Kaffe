/*
 * stackTrace.h
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#ifndef __stacktrace_h
#define __stacktrace_h

#include "exception.h"

struct _methods;

typedef struct _stackTraceInfo {
	uintp   pc;
	struct _methods* meth;
} stackTraceInfo;

#define ENDOFSTACK	((struct _methods*)-1)

Hjava_lang_Object*	buildStackTrace(struct _exceptionFrame*);

#endif
