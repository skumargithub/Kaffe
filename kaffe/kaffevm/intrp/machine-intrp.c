/*
 * machine.c
 * Java virtual machine interpreter.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */
#include "debug.h"
#define	CDBG(s) 	DBG(INT_VMCALL, s)
#define	RDBG(s) 	DBG(INT_RETURN, s)
#define	NDBG(s) 	DBG(INT_NATIVE, s)
#define	IDBG(s)		DBG(INT_INSTR, s)
#define	CHDBG(s)	DBG(INT_CHECKS, s)

#include "config.h"
#include "config-std.h"
#include "config-math.h"
#include "config-mem.h"
#include "config-setjmp.h"
#include "classMethod.h"
#include "gtypes.h"
#include "bytecode.h"
#include "slots-intrp.h"
#include "icode-intrp.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "gc.h"
#include "machine-intrp.h"
#include "lookup.h"
#include "code-analyse.h"
#include "soft.h"
#include "exception.h"
#include "external.h"
#include "baseClasses.h"
#include "thread.h"
#include "locks.h"
#include "checks-intrp.h"
#include "errors.h"
#include "md.h"
#include "trampolines.h"

/*
 * Define information about this engine.
 */
/* char* engine_name = "Interpreter"; */
/* char* engine_version = KVER; */

#define	define_insn(code)	break;					\
				case code:				\
				IDBG( dprintf("%03d: %s\n", pc, #code); )
#define	define_insn_alias(code)	case code:				\
				IDBG( dprintf("%03d: %s\n", pc, #code); )

/* Define CREATE_NULLPOINTER_CHECKS in md.h when your machine cannot use the
 * MMU for detecting null pointer accesses */
#if defined(CREATE_NULLPOINTER_CHECKS)
#define CHECK_NULL(_i, _s, _n)                                  \
      cbranch_ref_const_ne((_s), 0, reference_label(_i, _n)); \
      softcall_nullpointer();                                 \
      set_label(_i, _n)
#else
#define CHECK_NULL(_i, _s, _n)
#endif

#define	METHOD_RETURN_VALUE()					\
do {								\
	/* Store the return type (if necessary) */		\
	switch (low) {						\
	case 'V':						\
		break;						\
	case 'L':						\
	case '[':						\
		push(1);					\
		return_ref(stack(0));				\
		break;						\
	case 'I':						\
	case 'Z':						\
	case 'S':						\
	case 'B':						\
	case 'C':						\
		push(1);					\
		return_int(stack(0));				\
		break;						\
	case 'F':						\
		push(1);					\
		return_float(stack(0));				\
		break;						\
	case 'J':						\
		push(2);					\
		return_long(stack_long(0));			\
		break;						\
	case 'D':						\
		push(2);					\
		return_double(stack_double(0));			\
		break;						\
	default:						\
		ABORT();					\
	}							\
} while (0)

static
void
intrp_to_jit(Method* meth, callMethodInfo *call)
{
	char* sig = meth->signature->data;
	int i = 0;

	/* If this method isn't static, we must insert the object as
	 * an argument.
 	 */
	if ((meth->accflags & ACC_STATIC) == 0) {
		call->callsize[i] = PTR_TYPE_SIZE / SIZEOF_INT;
		i++;
	}

	sig++;	/* Skip leading '(' */
	for (; *sig != ')'; i++, sig++) {
		switch (*sig) {
		case 'I':
		case 'Z':
		case 'S':
		case 'B':
		case 'C':
		case 'F':
			call->callsize[i] = 1;
			break;

		case 'D':
		case 'J':
			call->callsize[i] = 2;
			i++;
			call->callsize[i] = 0;
			break;

		case '[':
			call->callsize[i] = PTR_TYPE_SIZE / SIZEOF_INT;
			while (*sig == '[') {
				sig++;
			}
			if (*sig == 'L') {
				while (*sig != ';') {
					sig++;
				}
			}
			break;
		case 'L':
			call->callsize[i] = PTR_TYPE_SIZE / SIZEOF_INT;
			while (*sig != ';') {
				sig++;
			}
			break;
		default:
			ABORT();
		}
	}

	/* Call info and arguments */
	call->nrargs = i;
}

void
callVirtualMachine(slots ret, FIXUP_TRAMPOLINE_DECL, int bogus, ...)
{
    Method *meth;
    char *sig;
	slots in[MAXMARGS];
    slots *retval = (slots*) &ret;
    va_list args;
    int i;

    FIXUP_TRAMPOLINE_INIT;
    sig = meth->signature->data;
    i = 0;
    va_start(args, bogus);

	if ((meth->accflags & ACC_STATIC) == 0) {
        in[i].v.taddr = va_arg(args, jref);
        i++;
	}

	sig++;	/* Skip leading '(' */
	for (; *sig != ')'; i++, sig++) {
		switch (*sig) {
		case 'I':
		case 'Z':
		case 'S':
		case 'B':
		case 'C':
			in[i].v.tint = va_arg(args, jint);
			break;
		case 'F':
			in[i].v.tfloat = va_arg(args, jfloat);
			break;
		case 'D':
			in[i].v.tdouble = va_arg(args, jdouble);
			i++;
			break;
		case 'J':
			in[i].v.tlong = va_arg(args, jlong);
			i++;
			break;
		case '[':
			in[i].v.taddr = va_arg(args, jref);
			while (*sig == '[') {
				sig++;
			}
			if (*sig == 'L') {
				while (*sig != ';') {
					sig++;
				}
			}
			break;
		case 'L':
			in[i].v.taddr = va_arg(args, jref);
			while (*sig != ';') {
				sig++;
			}
			break;
		default:
			ABORT();
		}
	}
    va_end(args);

    virtualMachine( meth, in, retval,
                    Kaffe_ThreadInterface.currentJava());

    sig++;
    switch(*sig)
    {
        case 'F':
        {
            __asm__ __volatile__ ("         \n\
                flds %0                      \n\
            "   :                           \
                :   "m" (retval->v.tfloat));
        }
        break;

        case 'D':
        {
            __asm__ __volatile__ ("         \n\
                fldl %0                      \n\
            "   :                           \
                :   "m" (retval->v.tdouble));
        }
        break;
    }
}

static bool
methodCanBeTranslated(Method *meth)
{
    return true;
}

void
virtualMachine(methods* meth, slots* volatile arg, slots* retval, Hjava_lang_Thread* tid)
{
	Hjava_lang_Object* volatile mobj;
	vmException mjbuf;
	accessFlags methaccflags;
	char* str;

	/* If these can be kept in registers then things will go much
	 * better.
	 */
	register bytecode* code;
	register slots* lcl;
	register slots* sp;
	register uintp pc;
	register uintp volatile npc;

	/* Misc machine variables */
	jlong lcc;
	jlong tmpl;
	slots tmp[1];
	slots tmp2[1];
	slots mtable[1];

	/* Variables used directly in the machine */
	int32 idx;
	jint low;
	jint high;

	/* Call, field and creation information */
	callInfo cinfo;
	fieldInfo finfo;
	Hjava_lang_Class* crinfo;

    callMethodInfo cM;

    if(Kaffe_JavaVMArgs[0].JITstatus == 30 && methodCanBeTranslated(meth))
    {
        meth->accflags ^= ACC_TOINTERPRET;
    }

CDBG(	dprintf("Call: %s.%s%s.\n", meth->class->name->data, meth->name->data, meth->signature->data); )

	/* If this is native, then call the real function */
	methaccflags = meth->accflags;
	if (methaccflags & ACC_NATIVE) {
NDBG(		dprintf("Call to native %s.%s%s.\n", meth->class->name->data, meth->name->data, meth->signature->data); )
		if (methaccflags & ACC_STATIC) {
			callMethodA(meth, meth, 0, (jvalue*)arg, (jvalue*)retval);
		}
		else {
			callMethodA(meth, meth, ((jvalue*)arg)[0].l, &((jvalue*)arg)[1], (jvalue*)retval);
		}
		return;
	}

	/* Verify method if required */
	if ((methaccflags & ACC_VERIFIED) == 0) {
		verifyMethod(meth);
		tidyVerifyMethod();
	}

	/* Allocate stack space and locals. */
	lcl = alloca(sizeof(slots) * (meth->localsz + meth->stacksz));

	mobj = 0;
	pc = 0;
	npc = 0;

	/* If we have any exception handlers we must prepare to catch them.
	 * We also need to catch if we are synchronised (so we can release it).
	 */
	mjbuf.pc = 0;
	mjbuf.mobj = mobj;
	mjbuf.meth = meth;

    mjbuf.retpc = 0;
    __asm__ __volatile__ ("       \n\
        movl %%ebp, %0          \n\
    "   :   "=g" (mjbuf.retbp)
        :
        : "memory" );

	if (tid != NULL && unhand(tid)->PrivateInfo != 0) {
		mjbuf.prev = (vmException*)unhand(tid)->exceptPtr;
		unhand(tid)->exceptPtr = (struct Hkaffe_util_Ptr*)&mjbuf;
	}
	if (meth->exception_table != 0 || (methaccflags & ACC_SYNCHRONISED)) {
		if (setjmp(mjbuf.jbuf) != 0) {
			unhand(tid)->exceptPtr = (struct Hkaffe_util_Ptr*)&mjbuf;
			npc = mjbuf.pc;
			sp = &lcl[meth->localsz];
			sp->v.taddr = (void*)unhand(tid)->exceptObj;
			unhand(tid)->exceptObj = 0;
			goto restart;
		}
	}

	/* Calculate number of arguments */
	str = meth->signature->data;
	idx = sizeofSig(&str, false) + (methaccflags & ACC_STATIC ? 0 : 1);

	/* Copy in the arguments */
	sp = lcl;
	for (low = 0; low < idx; low++) {
		*(sp++) = *(arg++);
	}

	/* Sync. if required */
	if (methaccflags & ACC_SYNCHRONISED) { 
		if (methaccflags & ACC_STATIC) {
			mobj = &meth->class->head;
		}
		else {
			mobj = (Hjava_lang_Object*)lcl[0].v.taddr;
		}
		lockMutex(mobj);
	}       

	sp = &lcl[meth->localsz - 1];

	restart:
	code = (bytecode*)meth->c.bcode.code;

	/* Finally we get to actually execute the machine */
	for (;;) {
		assert(npc < meth->c.bcode.codelen);
		pc = npc;
		mjbuf.pc = pc;
		npc = pc + insnLen[code[pc]];

		switch (code[pc]) {
		default:
			fprintf(stderr, "Unknown bytecode %d\n", code[pc]);
			throwException(VerifyError);
			break;

case INVOKEVIRTUAL:
{
	/*
	 * ..., obj, ..args.., -> ...
	 */
	check_pcidx (0);

	idx = (uint16)((getpc(0) << 8) | getpc(1));

	slot_alloctmp(tmp);
	slot_alloctmp(mtable);

	get_method_info(idx);

	/* If method doesn't exists, call 'NoSuchMethodError' */
	if (method_method() == 0) {
		softcall_nosuchmethod(method_class(), method_name(), method_sig());
		low = method_returntype();
		pop(method_nargs() + 1);
		METHOD_RETURN_VALUE();
	}
	else {
		idx = method_nargs();

		CHECK_NULL(INVOKEVIRTUAL, stack(idx), 34);

		check_stack_ref(idx);

		/* Find dispatch table in object */
		load_offset_ref(mtable, stack(idx), method_dtable_offset);

		/* Check method table for cached entry */
		load_offset_ref(tmp, mtable,
			DTABLE_METHODOFFSET + method_idx() * DTABLE_METHODSIZE);

		/* Push arguments & object */
		build_call_frame(method_sig(), stack(idx), idx);
		idx++;

		slot_nowriteback(tmp);
		pop(idx);
		end_sub_block();

		/* Call it */
		low = method_returntype();
        softcall_initialise_class(method_class());

        cinfo.method = tmp[0].v.taddr;
        if(cinfo.method->accflags & ACC_TOINTERPRET)
        {
            virtualMachine(cinfo.method, sp+1, retval, tid);
        }
        else
        {
            cM.args = (jvalue*) (sp + 1);
            cM.ret = (jvalue*) retval;
            cM.function = cinfo.method->ncode;
            cM.rettype = cinfo.rettype;
            cM.retsize = cinfo.out;
            cM.argsize = cinfo.in;

            intrp_to_jit(cinfo.method, &cM);

            sysdepCallMethod(&cM);
        }

		/* Pop args */
		popargs();

		start_sub_block();
		METHOD_RETURN_VALUE();
	}
}
break;

case INVOKESPECIAL:
{
	/*
	 * ..., obj, ..args.., -> ...
	 */
	check_pcidx (0);

	idx = (uint16)((getpc(0) << 8) | getpc(1));

	slot_alloctmp(mtable);

	get_special_method_info(idx);

	/* If method doesn't exists, call 'NoSuchMethodError' */
	if (method_method() == 0) {
		softcall_nosuchmethod(method_class(), method_name(), method_sig());
		low = method_returntype();
		pop(method_nargs() + 1);
		METHOD_RETURN_VALUE();
	}
	else {
		idx = method_nargs();

		CHECK_NULL(INVOKESPECIAL, stack(idx), 34);

		check_stack_ref(idx);

		/* Push arguments & object */
		build_call_frame(method_sig(), stack(idx), idx);
		idx++;

		pop(idx);
		end_sub_block();

		/* Call it */
		low = method_returntype();

        softcall_initialise_class(method_class());

        if(cinfo.method->accflags & ACC_TOINTERPRET)
        {
            virtualMachine(cinfo.method, sp+1, retval, tid);
        }
        else
        {
            cM.args = (jvalue*) (sp + 1);
            cM.ret = (jvalue*) retval;
            cM.function = cinfo.method->ncode;
            cM.rettype = cinfo.rettype;
            cM.retsize = cinfo.out;
            cM.argsize = cinfo.in;

            intrp_to_jit(cinfo.method, &cM);

            sysdepCallMethod(&cM);
        }

		/* Pop args */
		popargs();

		start_sub_block();
		METHOD_RETURN_VALUE();
	}
}
break;

case INVOKESTATIC:
{
	/*
	 * ..., ..args.., -> ...
	 */
	check_pcidx (0);

	idx = (uint16)((getpc(0) << 8) | getpc(1));

	get_method_info(idx);

	/* If method doesn't exists, call 'NoSuchMethodError' */
	if (method_method() == 0) {
		softcall_nosuchmethod(method_class(), method_name(), method_sig());
		low = method_returntype();
		pop(method_nargs());
		METHOD_RETURN_VALUE();
	}
	else {
		idx = method_nargs();

		/* Push arguments */
		build_call_frame(method_sig(), 0, idx);

		pop(idx);
		end_sub_block();

		/* Call it */
		low = method_returntype();
        softcall_initialise_class(method_class());

        if(cinfo.method->accflags & ACC_TOINTERPRET)
        {
            virtualMachine(cinfo.method, sp+1, retval, tid);
        }
        else
        {
            cM.args = (jvalue*) (sp + 1);
            cM.ret = (jvalue*) retval;
            cM.function = cinfo.method->ncode;
            cM.rettype = cinfo.rettype;
            cM.retsize = cinfo.out;
            cM.argsize = cinfo.in;

            intrp_to_jit(cinfo.method, &cM);

            sysdepCallMethod(&cM);
        }

		/* Pop args */
		popargs();

		start_sub_block();
		METHOD_RETURN_VALUE();
	}
}
break;

case INVOKEINTERFACE:
{
	/*
	 * ..., obj, ..args.., -> ...
	 */
	check_pcidx (0);
	check_pc (2);

	idx = (uint16)((getpc(0) << 8) | getpc(1));

	slot_alloctmp(tmp);

	get_method_info(idx);

	/* If method doesn't exists, call 'NoSuchMethodError' */
	if (method_method() == 0) {
		softcall_nosuchmethod(method_class(), method_name(), method_sig());
		low = method_returntype();
		pop((uint8)getpc(2));
		METHOD_RETURN_VALUE();
	}
	else {
		idx = (uint8)getpc(2) - 1;

		CHECK_NULL(INVOKEINTERFACE, stack(idx), 34);

		check_stack_ref(idx);

		softcall_lookupmethod(tmp, method_method(), stack(idx));

		/* Push arguments & object */
		build_call_frame(method_sig(), stack(idx), idx);
		idx++;

		slot_nowriteback(tmp);
		pop(idx);
		end_sub_block();

		/* Call it */
		low = method_returntype();
        softcall_initialise_class(method_class());

        cinfo.method = tmp[0].v.taddr;
        if(cinfo.method->accflags & ACC_TOINTERPRET)
        {
            virtualMachine(cinfo.method, sp+1, retval, tid);
        }
        else
        {
            cM.args = (jvalue*) (sp + 1);
            cM.ret = (jvalue*) retval;
            cM.function = cinfo.method->ncode;
            cM.rettype = cinfo.rettype;
            cM.retsize = cinfo.out;
            cM.argsize = cinfo.in;

            intrp_to_jit(cinfo.method, &cM);

            sysdepCallMethod(&cM);
        }

		/* Pop args */
		popargs();

		start_sub_block();
		METHOD_RETURN_VALUE();
	}
}
break;

#include "kaffe.intrp.def"
		}
	}
	end:

	/* Unsync. if required */
	if (mobj != 0) {
		unlockMutex(mobj);
	}       
	if (tid != NULL && unhand(tid)->PrivateInfo != 0) {
		unhand(tid)->exceptPtr = (struct Hkaffe_util_Ptr*)mjbuf.prev;
	}

RDBG(	dprintf("Returning from method %s%s.\n", meth->name->data, meth->signature->data); )
}
