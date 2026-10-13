/* 
 * Mach Operating System
 * Copyright (c) 1994 Johannes Helander
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JOHANNES HELANDER ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  JOHANNES HELANDER DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/*
 * HISTORY
 * $Log: ecrt0.c,v $
 * Revision 1.1.1.1  1995/03/02  21:49:32  mike
 * Initial Lites release from hut.fi
 *
 */
/* 
 *	File:	emulator/mips/ecrt0.c
 *	Author:	Johannes Helander, Helsinki University of Technology, 1994.
 *
 * 	Emulator entry point.
 */

#include <e_defs.h>

/* just in case someone defined it to empty */
#undef register

int	(*mach_init_routine)();

extern int _gp;

extern	unsigned char	_eprol;
void __start()
{
	/* 
	 * The server set_emulator_state() sets pc, sp, gp, r4, r5, r6
	 * sp and r6 points to the arguments.
	 * r4 points to the BSS dirty page (shared with data)
	 *	that needs to be cleared.
	 * r5 is the clearing count for the BSS fragment.
	 */
	register char *zero_start asm("$4");
	register int zero_count asm("$5");
	register int real_sp asm("$6");
	register int sp;

	/* Store the stack pointer in a variable */
	sp = real_sp;

	/* Clear beginning of BSS (on the page shared with DATA) */
	for ( ; zero_count > 0; zero_count--)
	    *zero_start++ = 0;

	asm volatile ("la $28, _gp");
	
	if (mach_init_routine)
		(void) mach_init_routine();

asm(".globl _eprol");
asm("_eprol:");

	exit(emulator_main(sp, sp));
}
