/*
 * exception.c
 * Handle exceptions for the interpreter or translator.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */
#include "debug.h"

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
#include "locks.h"
#include "stackTrace.h"

static void nullException(EXCEPTIONPROTO);
static void floatingException(EXCEPTIONPROTO);
static void dispatchException(Hjava_lang_Throwable*, struct _exceptionFrame*) __NORETURN__;

Hjava_lang_Object* buildStackTrace(struct _exceptionFrame*);

extern Hjava_lang_Object* exceptionObject;
extern uintp Kaffe_JNI_estart;
extern uintp Kaffe_JNI_eend;
extern void Kaffe_JNIExceptionHandler(void);

extern void Tspinoffall(void);

/*
 * Throw an internal exception.
 */
void
throwException(Hjava_lang_Object* eobj)
{
	if (eobj != 0) {
		unhand((Hjava_lang_Throwable*)eobj)->backtrace = buildStackTrace(0);
	}
	throwExternalException(eobj);
}

/*
 * Throw an exception.
 */
void
throwExternalException(Hjava_lang_Object* eobj)
{
    exceptionFrame frame = *((exceptionFrame*)(((uintp)&(eobj))-8));

    if (eobj == 0) {
        fprintf(stderr, "Exception thrown on null object ... abort\n");
        ABORT();
        EXIT(1);
    }

    dispatchException((Hjava_lang_Throwable*) eobj, &frame);
}

void
throwOutOfMemory(void)
{
	Hjava_lang_Object* err;

	err = OutOfMemoryError;
	if (err != NULL) {
		throwException(err);
	}
	fprintf(stderr, "(Insufficient memory)\n");
	EXIT(-1);
}

vmException* isIntrpFrame(uintp);

static
void
dispatchException(Hjava_lang_Throwable* eobj, struct _exceptionFrame* baseframe)
{
	Hjava_lang_Class* class = OBJECT_CLASS(&eobj->base);
	char* cname = CLASS_CNAME(class);
	Hjava_lang_Object* obj;
	iLock* lk;
	Hjava_lang_Thread* ct = getCurrentThread();

    exceptionInfo einfo;
    vmException* v;
    exceptionFrame* e;

    bool res;

	/* Release the interrupts (in case they were held when this
	 * happened - and hope this doesn't break anything).
	 * XXX - breaks the thread abstraction model !!!
	 */
	Tspinoffall();

	/* Save exception object */
	unhand(ct)->exceptObj = (struct Hkaffe_util_Ptr*)eobj;

	/* Search down exception stack for a match */
    if(Kaffe_JavaVMArgs[0].JITstatus == 10)
	{
        /*
         * Pure interpreter
         */
		for(e = baseframe; e != 0; e = Kaffe_ThreadInterface.nextFrame(e))
        {
            if((v = isIntrpFrame(e->retbp)) != NULL)
            {
                if(v->meth == (Method*)1)
                {
                    /* Don't allow JNI to catch thread death
                     * exceptions.  Might be bad but its going
                     * 1.2 anyway.
                     */
                    if(strcmp(cname, THREADDEATHCLASS) != 0)
                    {
                        unhand(ct)->exceptPtr = (struct Hkaffe_util_Ptr*) v;
                        Kaffe_JNIExceptionHandler();
                    }
                }

                /* Look for handler */
                res = findExceptionBlockInMethod(   v->pc,
                                                    eobj->base.Idtable->class,
                                                    v->meth,
                                                    &einfo,
                                                    false);

                /* Find the sync. object */
                if( einfo.method == 0 ||
                    (einfo.method->accflags & ACC_SYNCHRONISED) == 0) {
                    obj = 0;
                }
                else if (einfo.method->accflags & ACC_STATIC) {
                    obj = &einfo.class->head;
                }
                else {
                    obj = v->mobj;
                }

                /* If handler found, call it */
                if (res == true) {
                    v->pc = einfo.handler;
                    longjmp(v->jbuf, 1);
                }

                /* If not here, exit monitor if synchronised. */
                lk = getLock(obj);
                if( lk != 0 &&
                    lk->holder == (*Kaffe_ThreadInterface.currentNative)()) {
                    unlockMutex(obj);
                }
            }
		}
	}
    else if(Kaffe_JavaVMArgs[0].JITstatus == 20)
	{
        /*
         * Pure JIT
         */
		for(e = baseframe; e != 0; e = Kaffe_ThreadInterface.nextFrame(e))
        {
			findExceptionInMethod(PCFRAME(e), class, &einfo);

            if( einfo.method == 0 &&
                PCFRAME(e) >= Kaffe_JNI_estart &&
                PCFRAME(e) < Kaffe_JNI_eend)
            {
                /* Don't allow JNI to catch thread death
                 * exceptions.  Might be bad but its going
                 * 1.2 anyway.
                 */
                if (strcmp(cname, THREADDEATHCLASS) != 0) {
                    Kaffe_JNIExceptionHandler();
                }
            }

			/* Find the sync. object */
			if( einfo.method == 0 ||
                (einfo.method->accflags & ACC_SYNCHRONISED) == 0) {
				obj = 0;
			}
			else if (einfo.method->accflags & ACC_STATIC) {
				obj = &einfo.class->head;
			}
			else {
				obj = FRAMEOBJECT(e);
			}

			/* Handler found - dispatch exception */
			if (einfo.handler != 0) {
				unhand(ct)->exceptObj = 0;
				CALL_KAFFE_EXCEPTION(e, einfo, eobj);
			}

			/* If method found and synchronised, unlock the lock */
			lk = getLock(obj);
			if( lk != 0 &&
                lk->holder == (*Kaffe_ThreadInterface.currentNative)()) {
				unlockMutex(obj);
			}
		}
	}
    else if(Kaffe_JavaVMArgs[0].JITstatus == 30 ||
            Kaffe_JavaVMArgs[0].JITstatus == 40)
	{
        /*
         * Mix modes
         */
		for(e = baseframe; e != 0; e = Kaffe_ThreadInterface.nextFrame(e))
        {
            if((v = isIntrpFrame(e->retbp)) != NULL)
            {
                if(v->meth == (Method*)1)
                {
                    /* Don't allow JNI to catch thread death
                     * exceptions.  Might be bad but its going
                     * 1.2 anyway.
                     */
                    if(strcmp(cname, THREADDEATHCLASS) != 0)
                    {
                        unhand(ct)->exceptPtr = (struct Hkaffe_util_Ptr*) v;
                        Kaffe_JNIExceptionHandler();
                    }
                }

                /* Look for handler */
                res = findExceptionBlockInMethod(   v->pc,
                                                    eobj->base.Idtable->class,
                                                    v->meth,
                                                    &einfo,
                                                    false);

                /* Find the sync. object */
                if( einfo.method == 0 ||
                    (einfo.method->accflags & ACC_SYNCHRONISED) == 0) {
                    obj = 0;
                }
                else if (einfo.method->accflags & ACC_STATIC) {
                    obj = &einfo.class->head;
                }
                else {
                    obj = v->mobj;
                }

                /* If handler found, call it */
                if (res == true) {
                    v->pc = einfo.handler;
                    longjmp(v->jbuf, 1);
                }

                /* If not here, exit monitor if synchronised. */
                lk = getLock(obj);
                if( lk != 0 &&
                    lk->holder == (*Kaffe_ThreadInterface.currentNative)()) {
                    unlockMutex(obj);
                }
            }
            else
            {
                findExceptionInMethod(PCFRAME(e), class, &einfo);

                /* Find the sync. object */
                if( einfo.method == 0 ||
                    (einfo.method->accflags & ACC_SYNCHRONISED) == 0) {
                    obj = 0;
                }
                else if (einfo.method->accflags & ACC_STATIC) {
                    obj = &einfo.class->head;
                }
                else {
                    obj = FRAMEOBJECT(e);
                }

                /* Handler found - dispatch exception */
                if (einfo.handler != 0) {
                    unhand(ct)->exceptObj = 0;
                    CALL_KAFFE_EXCEPTION(e, einfo, eobj);
                }

                /* If method found and synchronised, unlock the lock */
                lk = getLock(obj);
                if( lk != 0 &&
                    lk->holder == (*Kaffe_ThreadInterface.currentNative)()) {
                    unlockMutex(obj);
                }
            }
		}
	}
    else
    {
        assert(0);
    }

	/* Clear held exception object */
	unhand(ct)->exceptObj = 0;

	/* We must catch 'java.lang.ThreadDeath' exceptions now and
	 * kill the thread rather than the machine.
	 */
	if (strcmp(cname, THREADDEATHCLASS) == 0) {
		exitThread();
	}

	/* If all else fails we call the the uncaught exception method
	 * on this thread's group.  Note we must set a flag so we don't do
	 * this again while in the handler.
	 */
	if (unhand(ct)->dying == false) {
		unhand(ct)->dying = true;
		do_execute_java_method(unhand(
            (*Kaffe_ThreadInterface.currentJava)())->group,
            "uncaughtException",
            "(Ljava/lang/Thread;Ljava/lang/Throwable;)V", 0, 0,
            (*Kaffe_ThreadInterface.currentJava)(), eobj);
	}
	exitThread();
}

/*
 * Setup the internal exceptions.
 */
void
initExceptions(void)
{
DBG(INIT,	printf("initExceptions()\n");			)
	if (DBGEXPR(EXCEPTION, false, true)) {
	/* Catch signals we need to convert to exceptions */
#if defined(SIGSEGV)
		catchSignal(SIGSEGV, nullException);
#endif
#if defined(SIGBUS)
		catchSignal(SIGBUS, nullException);
#endif
#if defined(SIGFPE)
		catchSignal(SIGFPE, floatingException);
#endif
#if defined(SIGPIPE)
		catchSignal(SIGPIPE, SIG_IGN);
#endif
	}
}

/*
 * Null exception - catches bad memory accesses.
 */
static void
nullException(EXCEPTIONPROTO)
{
	Hjava_lang_Throwable* npe;
    exceptionFrame frame;

    EXCEPTIONFRAME(frame, ctx);

    /* don't catch the signal if debugging exceptions */
    if (DBGEXPR(EXCEPTION, false, true))
        catchSignal(sig, nullException);

    npe = (Hjava_lang_Throwable*)NullPointerException;
    unhand(npe)->backtrace = buildStackTrace(&frame);
    dispatchException(npe, &frame);
}

/*
 * Division by zero.
 */
static void
floatingException(EXCEPTIONPROTO)
{
	Hjava_lang_Throwable* ae;
    exceptionFrame frame;

    EXCEPTIONFRAME(frame, ctx);

    /* don't catch the signal if debugging exceptions */
    if (DBGEXPR(EXCEPTION, false, true))
        catchSignal(sig, floatingException);

    ae = (Hjava_lang_Throwable*)ArithmeticException;
    unhand(ae)->backtrace = buildStackTrace(&frame);
    dispatchException(ae, &frame);
}

/*
 * Setup a signal handler.
 */
void
catchSignal(int sig, void* handler)
{
	sigset_t nsig;

#if defined(HAVE_SIGACTION)

	struct sigaction newact;

	newact.sa_handler = (SIG_T)handler;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;
#if defined(SA_SIGINFO)
	newact.sa_flags |= SA_SIGINFO;
#endif
	sigaction(sig, &newact, NULL);

#elif defined(HAVE_SIGNAL)

	signal(sig, (SIG_T)handler);

#else
	ABORT();
#endif

	/* Unblock this signal */
	sigemptyset(&nsig);
	sigaddset(&nsig, sig);
	sigprocmask(SIG_UNBLOCK, &nsig, 0);
}
