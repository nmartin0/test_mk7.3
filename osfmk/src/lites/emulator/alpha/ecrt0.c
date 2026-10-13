/* 
 * Mach Operating System
 * Copyright (c) 1995 Johannes Helander
 * Copyright (c) 1995 Jukka Virtanen
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JOHANNES HELANDER AND JUKKA VIRTANEN ALLOW FREE USE OF THIS SOFTWARE
 * IN ITS "AS IS" CONDITION.  JOHANNES HELANDER AND JUKKA VIRTANEN DISCLAIM
 * ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM
 * THE USE OF THIS SOFTWARE.
 */
/* 
 * HISTORY
 * $Log: ecrt0.c,v $
 * Revision 1.1.1.1  1995/03/02  21:49:32  mike
 * Initial Lites release from hut.fi
 *
 *
 */
/* 
 * 	Emulator entry point.
 */

#include <e_defs.h>

/* just in case someone defined it to empty */
#undef register

int	(*mach_init_routine)();

extern	unsigned char	_eprol;
void __start()
{
	/* 
	 * The server set_emulator_state() sets pc, sp, gp, $16,$17,$18
	 *
	 * sp and $16 points to the arguments.
	 * $17 points to the BSS dirty page (shared with data)
	 *	that needs to be cleared.
	 * $18 is the clearing count for the BSS fragment.
	 */
	register char *zero_start     asm("$17");
	register integer_t zero_count asm("$18");
	register integer_t real_sp    asm("$16");
	register integer_t sp;

	/* Store the stack pointer in a variable */
	sp = real_sp;

	/* Clear beginning of BSS (on the page shared with DATA) */
	for ( ; zero_count > 0; zero_count--)
	    *zero_start++ = 0;

	/* This might be redundant */
	asm volatile ("br	$27,1f
		       1:
		       ldgp	$29, 0($27)"
		      ::: "27", "29");
	
	if (mach_init_routine)
		(void) mach_init_routine();

asm(".globl _eprol");
asm("_eprol:");

	exit(emulator_main(sp));
}
