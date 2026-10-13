/* 
 * Mach Operating System
 * Copyright (c) 1994 Johannes Helander
 * Copyright (c) 1994 Ian Dall
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JOHANNES HELANDER AND IAN DALL ALLOW FREE USE OF THIS SOFTWARE IN
 * ITS "AS IS" CONDITION.  JOHANNES HELANDER AND IAN DALL DISCLAIM ANY
 * LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 * USE OF THIS SOFTWARE.
 */
/*
 * HISTORY
 * $Log: ecrt0.c,v $
 * Revision 1.1.1.1  1995/03/02  21:49:29  mike
 * Initial Lites release from hut.fi
 *
 */
/* 
 *	File: 	emulator/ns532/ecrt0.o
 *	Authors:
 *	Johannes Helander, Helsinki University of Technology, 1994.
 *	Ian Dall, Adelaide, 1994.
 *
 * 	Emulator entry point.
 */

#define cthread_sp() \
	({  int _sp__; \
	    __asm__("sprd sp, %0" \
	      : "=g" (_sp__) ); \
	    _sp__; })

int	(*mach_init_routine)();

/* just in case someone defined it to empty */
#undef register

int exit();

extern	unsigned char	_eprol;
_start()
{
	/*
	 * The server set_emulator_state() sets pc, sp, psr, r0, r1
	 * sp points to the arguments.
	 * r0 points to the BSS dirty page (shared with data)
	 *	that needs to be cleared.
	 * r1 is the clearing count for the BSS fragment.
	 */
	register char *zero_start asm("r0");
	register int zero_count asm("r1");
	register int sp;

	/* Store the stack pointer in a variable */
	sp = cthread_sp();

	/* Clear beginning of BSS (on the page shared with DATA) */
	for ( ; zero_count > 0; zero_count--)
	    *zero_start++ = 0;

	if (mach_init_routine)
		(void) mach_init_routine();

asm(".globl __eprol");
asm("__eprol:");

	exit(emulator_main(sp, sp));
}
    

