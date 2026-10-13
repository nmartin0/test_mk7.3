/* 
 * Mach Operating System
 * Copyright (c) 1994 Ian Dall
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * IAN DALL ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
 * IAN DALL DISCLAIMS ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/*
 * HISTORY
 * $Log: emul_misc_asm.s,v $
# Revision 1.1.1.1  1995/03/02  21:49:29  mike
# Initial Lites release from hut.fi
#
 */
/* 
 *	File:	emulator/ns532/misc_asm.c
 *	Author:	Ian Dall
 *	Date:	September 1994
 *
 *	save/load state & random stuff.
 */
#include <ns532/asm.h>

/* 
 * int emul_save_state(struct ns532_thread_state *state)
 *
 * Saves current registers into struct pointed to by arg.
 * "setjmp".
 *
 * Caller saved registers r0, r1, and r2 are not saved but zeroed.
 * Returns zero normally but may return anything when "longjmped" to.
 */
ENTRY(emul_save_state)
	movd	S_ARG0, r0
	movqd	0, 0(r0)	/* state->r0 */
	movqd	0, 4(r0)	/* state->r1 */
	movqd	0, 8(r0)	/* state->r2 */
	movd	r3, 12(r0)	/* state->r3 */
	movd	r4, 16(r0)	/* state->r4 */
        movd	r5, 20(r0)	/* state->r5 */
     	movd	r6, 24(r0)	/* state->r6 */
        movd    r7, 28(r0)	/* state->r7 */
	sprd	sb, 32(r0)	/* state->sb */
        sprd	fp, 36(r0)	/* state->fp */
        addr	4(sp), 40(r0)	/* state->sp */
     	movd	0(sp), 44(r0)	/* state->pc */
        sprd	mod, 48(r0)	/* state->mod */
        sprd	us, 52(r0)	/* state->psr */
        movqd	0, r0		/* Return 0 */
	ret	0

/* 
 * noreturn emul_load_state(struct ns532_thread_state *state)
 *
 * Loads all (but seg) registers from struct pointed to by arg.
 * "longjmp".
 *
 * Never returns. The stack is unwound.
 */
/* Assume single threaded ? */
ENTRY(emul_load_state)
	/* First load a few registers */
        movd	S_ARG0, r0
	movd	8(r0), r2	/* state->r2 */
	movd	12(r0), r3	/* state->r3 */
	movd	16(r0), r4	/* state->r4 */
        movd	20(r0), r5	/* state->r5 */
     	movd	24(r0), r6	/* state->r6 */
        movd    28(r0), r7	/* state->r7 */
	lprd	sb, 32(r0)	/* state->sb */
        lprd	fp, 36(r0)	/* state->fp */
        lprd	mod, 48(r0)	/* state->mod */
        lprd	us, 52(r0)	/* state->psr */
	movd	40(r0), r1	/* state->sp */
	addqd	-8, r1		/* Make space on new stack */
     	movd	44(r0), 4(r1)	/* state->pc  to new stack */
     	movd	4(r0), 0(r1)	/* state->r1 to new stack */
	movd	0(r0), r0	/* state->r0 */
	lprd	sp, r1		/* use new stack */
	movd	tos, r1		/* pop r1 */
	ret	0		/* pop pc */
