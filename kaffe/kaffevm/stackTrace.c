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
buildStackTrace(struct _exceptionFrame* baseframe)
{
	int cnt;
	stackTraceInfo* info;
    exceptionFrame* base;
    exceptionFrame* e;
    vmException *v;

    base =  baseframe == NULL ?
            (exceptionFrame*) (((uintp) &baseframe) - 8)
            :   baseframe;

	cnt = 0;
    e = base;
    switch(Kaffe_JavaVMArgs[0].JITstatus)
    {
        case 10:
        {
            /*
             * Pure Interpreter
             */
            while(e != 0)
            {
                if((v = isIntrpFrame(e->retbp)) != NULL)
                {
                    if(v->meth == (Method*) 1)
                    {
                        break;
                    }

                    cnt++;
                }

                e = Kaffe_ThreadInterface.nextFrame(e);
            }
        }
        break;

        case 20:
        {
            /*
             * Pure JIT
             */
            while(e != 0) {
                e = Kaffe_ThreadInterface.nextFrame(e);
                cnt++;
            }
        }
        break;

        case 30:
        case 40:
        {
            /*
             * Mix modes
             */
            while(e != 0) {
                if((v = isIntrpFrame(e->retbp)) != NULL)
                {
                    if(v->meth == (Method*) 1)
                    {
                        break;
                    }
                }

                e = Kaffe_ThreadInterface.nextFrame(e);
                cnt++;
            }
        }
        break;

        default:
            assert(0);
        break;
    }

	/* Build an array of stackTraceInfo */
	info = gc_malloc(sizeof(stackTraceInfo) * (cnt+1), GC_ALLOC_NOWALK);

	cnt = 0;
    e = base;
    switch(Kaffe_JavaVMArgs[0].JITstatus)
    {
        case 10:
        {
            /*
             * Pure Interpreter
             */
            while(e != 0)
            {
                if((v = isIntrpFrame(e->retbp)) != NULL)
                {
                    if(v->meth == (Method*) 1)
                    {
                        break;
                    }

                    info[cnt].pc = v->pc;
                    info[cnt].meth = v->meth;
                    cnt++;
                }

                e = Kaffe_ThreadInterface.nextFrame(e);
            }
        }
        break;

        case 20:
        {
            /*
             * Pure JIT
             */
            for(; e != 0; e = Kaffe_ThreadInterface.nextFrame(e)) {
                info[cnt].pc = PCFRAME(e);
                info[cnt].meth = findMethodFromPC(PCFRAME(e));
                cnt++;
            }
        }
        break;

        case 30:
        case 40:
        {
            /*
             * Mix modes
             */
            while(e != 0)
            {
                if((v = isIntrpFrame(e->retbp)) != NULL)
                {
                    if(v->meth == (Method*) 1)
                    {
                        break;
                    }

                    info[cnt].pc = v->pc;
                    info[cnt].meth = v->meth;
                }
                else
                {
                    info[cnt].pc = PCFRAME(e);
                    info[cnt].meth = findMethodFromPC(PCFRAME(e));
                }

                e = Kaffe_ThreadInterface.nextFrame(e);
                cnt++;
            }
        }
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
