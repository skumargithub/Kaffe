/*
 * i386/trampolines.c
 * i386 trampolines codes for various occasions.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#if defined(TRAMPOLINE_FUNCTION)
/*
 * If we have an explit function defined then use that.
 */
TRAMPOLINE_FUNCTION()

#else
/*
 * Otherwise we'll try to construct one.
 */
#if !defined(START_ASM_FUNC)
#define	START_ASM_FUNC() ".text\n\t.align 4\n\t.globl "
#endif
#if !defined(END_ASM_FUNC)
#define	END_ASM_FUNC() ""
#endif
#if defined(HAVE_UNDERSCORED_C_NAMES)
#define	C_FUNC_NAME(FUNC) "_" #FUNC
#else
#define	C_FUNC_NAME(FUNC) #FUNC
#endif

#if defined(NO_SHARED_VMLIBRARY)

asm(
	START_ASM_FUNC() C_FUNC_NAME(i386_do_fixup_trampoline) "\n"
C_FUNC_NAME(i386_do_fixup_trampoline) ":			\n
    pushl   %ecx        /* address to go to */
    pushl   %ebx        /* flag indicating if JIT or intrp */
	call	" C_FUNC_NAME(soft_fixup_trampoline) "
	popl	%ebx
	popl	%ecx

    cmpl    $1,%ebx
    je      1f

    pushl   %eax
    pushl   %edx
    call    *%ecx
    popl    %eax
    popl    %edx

    popl    %ecx
    ret

1:
	popl	%eax
	jmp	    *%ecx"
	END_ASM_FUNC()
);

#else

asm(
	START_ASM_FUNC() C_FUNC_NAME(i386_do_fixup_trampoline) "\n"
C_FUNC_NAME(i386_do_fixup_trampoline) ":			\n
	call	i386_dft1					\n
i386_dft1:							\n
	popl	%ebx						\n
	addl	$_GLOBAL_OFFSET_TABLE_+[.-i386_dft1],%ebx	\n
    pushl   %eax                                        \n
	call	" C_FUNC_NAME(soft_fixup_trampoline) "@PLT	\n
	popl	%eax						\n
	popl	%ecx						\n
	jmp	*%eax"
	END_ASM_FUNC()
);

#endif

#endif
