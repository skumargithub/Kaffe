/*
 * i386/linux/md.h
 * Linux i386 configuration information.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#ifndef __ki386_linux_md_h
#define __ki386_linux_md_h

#include "i386/common.h"
#include "i386/threads.h"

/* #if defined(__TRANSLATOR__) */
#include "jit-md.h"
/* #endif */

void init_md(void);

/* Linux requires a little initialisation */
#define	INIT_MD()	init_md()

#define GET_TIME(X)                                             \
    {                                                           \
        unsigned int beginL__ = 0, beginH__ = 0;                \
                                                                \
        __asm__ __volatile__ (                                  \
                ".byte 0x0f,0x31"                               \
                :"=a"(beginL__), "=d"(beginH__));               \
                                                                \
       (X)    = (((unsigned long long) beginL__)      ) |       \
                (((unsigned long long) beginH__) << 32);        \
    }

#define BEGIN_TIMER()                                           \
    {                                                           \
        unsigned int beginL__ = 0, beginH__ = 0;                \
        unsigned int endL__ = 0, endH__ = 0;                    \
        unsigned long long begin, end;                          \
                                                                \
        __asm__ __volatile__ (                                  \
                ".byte 0x0f,0x31"                               \
                :"=a"(beginL__), "=d"(beginH__));               \
                                                                \
        begin = (((unsigned long long) beginL__)      ) |       \
                (((unsigned long long) beginH__) << 32);

#define END_TIMER(X)                                            \
        __asm__ __volatile__ (                                  \
            ".byte 0x0f,0x31"                                   \
            :"=a"(endL__), "=d"(endH__));                       \
                                                                \
        end = (((unsigned long long) endL__)      ) |           \
              (((unsigned long long) endH__) << 32);            \
                                                                \
        (X) = end - begin;                                      \
    }

#endif
