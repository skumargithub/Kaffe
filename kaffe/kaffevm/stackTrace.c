/*
 * stackTrace.c
 * Handle stack trace for the interpreter or translator.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#define	DBG(s)

#include "config.h"
#include "config-std.h"
#include "config-signal.h"
#include "config-mem.h"
#include "config-setjmp.h"
#include "jtypes.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "code.h"
#include "exception.h"
#include "baseClasses.h"
#include "lookup.h"
#include "thread.h"
#include "errors.h"
#include "itypes.h"
#include "external.h"
#include "soft.h"
#include "md.h"
#include "stackTrace.h"

vmException *
isIntrpFrame(uintp ebp)
{
    vmException *e = (vmException*)
        unhand(Kaffe_ThreadInterface.currentJava())->exceptPtr;

    while(e != NULL)
    {
        if(e->retbp == ebp)
        {
            return e;
        }

        e = e->prev;
    }

    return NULL;
}

Hjava_lang_Object*
buildStackTrace(struct _exceptionFrame* base)
{
	int cnt;
	stackTraceInfo* info;

	cnt = 0;
    switch(Kaffe_JavaVMArgs[0].JITstatus)
    {
        case 10:
        {
            /*
             * Pure Interpreter
             */
            exceptionFrame* v;
            vmException *vm;

            if(base == NULL)
            {
                v = (exceptionFrame*) (((uintp)&(base))-8);
            }
            else
            {
                v = base;
            }

            while(v != 0)
            {
                if((vm = isIntrpFrame(v->retbp)) != NULL)
                {
                    if(vm->meth == (Method*) 1)
                    {
                        break;
                    }

                    cnt++;
                }

                v = Kaffe_ThreadInterface.nextFrame(v);
            }
        }
        break;

        case 20:
        {
            /*
             * Pure JIT
             */
            struct _exceptionFrame* e;
            if(base == NULL)
            {
                e = (exceptionFrame*) (((uintp)&(base))-8);
            }
            else
            {
                e = base;
            }

            while(e != 0) {
                e = Kaffe_ThreadInterface.nextFrame(e);
                cnt++;
            }
        }
        break;

        case 30:
            assert(0);
        break;

        case 40:
            assert(0);
        break;

        default:
            assert(0);
        break;
    }

	/* Build an array of stackTraceInfo */
	info = gc_malloc(sizeof(stackTraceInfo) * (cnt+1), GC_ALLOC_NOWALK);

	cnt = 0;
    switch(Kaffe_JavaVMArgs[0].JITstatus)
    {
        case 10:
        {
            /*
             * Pure Interpreter
             */
            exceptionFrame* v;
            vmException *vm;

            if(base == NULL)
            {
                v = (exceptionFrame*) (((uintp)&(base))-8);
            }
            else
            {
                v = base;
            }

            while(v != 0)
            {
                if((vm = isIntrpFrame(v->retbp)) != NULL)
                {
                    if(vm->meth == (Method*) 1)
                    {
                        break;
                    }

                    info[cnt].pc = vm->pc;
                    info[cnt].meth = vm->meth;
                    cnt++;
                }

                v = Kaffe_ThreadInterface.nextFrame(v);
            }
        }
        break;

        case 20:
        {
            /*
             * Pure JIT
             */
            struct _exceptionFrame* e;
            if(base == NULL)
            {
                e = (exceptionFrame*) (((uintp)&(base))-8);
            }
            else
            {
                e = base;
            }

            for(; e != 0; e = Kaffe_ThreadInterface.nextFrame(e)) {
                info[cnt].pc = PCFRAME(e);
                info[cnt].meth = findMethodFromPC(PCFRAME(e));
                cnt++;
            }
        }
        break;

        case 30:
            assert(0);
        break;

        case 40:
            assert(0);
        break;

        default:
            assert(0);
        break;
    }

	info[cnt].pc = 0;
	info[cnt].meth = ENDOFSTACK;

	return ((Hjava_lang_Object*)info);
}

void
printStackTrace(struct Hjava_lang_Throwable* o, struct Hjava_lang_Object* p)
{
	int i;
	stackTraceInfo* info;
	Method* meth;
	uintp pc;
	int32 linenr;
	uintp linepc;
	char buf[200];
	int len;
	int j;
	Hjava_lang_Object* str;
	jchar* cptr;

	info = (stackTraceInfo*)unhand(o)->backtrace;
	if (info == 0) {
		return;
	}
	for (i = 0; info[i].meth != ENDOFSTACK; i++) {
		pc = info[i].pc;
		meth = info[i].meth; 
		if (meth != 0) {
			linepc = 0;
			linenr = -1;
			if (meth->lines != 0) {
				for (j = 0; j < meth->lines->length; j++) {
					if (pc >= meth->lines->entry[j].start_pc && linepc < meth->lines->entry[j].start_pc) {
						linenr = meth->lines->entry[j].line_nr;
						linepc = meth->lines->entry[j].start_pc;
					}
				}
			}
			if (linenr == -1) {
				sprintf(buf, "\tat %.80s.%.80s(line unknown, pc %p)",
					CLASS_CNAME(meth->class),
					meth->name->data, (void*)pc);
			}
			else {
				sprintf(buf, "\tat %.80s.%.80s(%d)",
					CLASS_CNAME(meth->class),
					meth->name->data,
					linenr);
			}
			len = strlen(buf);
			str = newArray(TYPE_CLASS(TYPE_Char), len);
			cptr = (jchar*)OBJARRAY_DATA(str);
			for (j = len;  --j >= 0; ) {
				cptr[j] = (unsigned char)buf[j];
			}
			do_execute_java_method(p,"println","([C)V",0,0,str);
		}
	}
	do_execute_java_method(p, "flush", "()V", 0, 0);
}
