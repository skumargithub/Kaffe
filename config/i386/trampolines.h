#ifndef trampolines_h
#define trampolines_h

/**/
/* Method dispatch.  */
/**/

#define HAVE_TRAMPOLINE

typedef struct _methodTrampoline {
	unsigned char call	__attribute__((packed));
	int fixup		__attribute__((packed));
	struct _methods* meth	__attribute__((packed));
} methodTrampoline;

extern void i386_do_fixup_trampoline(void);

#define FILL_IN_TRAMPOLINE(t,m)						\
	do {								\
		(t)->call = 0xe8;					\
		(t)->fixup = (int)i386_do_fixup_trampoline - (int)(t) - 5; \
		(t)->meth = (m);					\
	} while (0)

#define FIXUP_TRAMPOLINE_DECL	Method** _pmeth
#define FIXUP_TRAMPOLINE_INIT	(meth = *_pmeth)

#endif
