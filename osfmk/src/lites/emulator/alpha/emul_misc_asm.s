/* 
 * Mach Operating System
 * Copyright (c) 1995 Jukka Virtanen
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JUKKA VIRTANEN ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION. JUKKA VIRTANEN DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/* 
 * HISTORY
 * $Log: emul_misc_asm.s,v $
# Revision 1.1.1.1  1995/03/02  21:49:32  mike
# Initial Lites release from hut.fi
#
 *
 */
/* 
 *	File:	emulator/alpha/misc_asm.c
 *	Author:	Jukka Virtanen <jtv@hut.fi>
 *	Date:	November 1994
 *
 *	save/load state & random stuff.
 */

#include <mach/alpha/asm.h>

#define DEBUG 1

#define	TS_OFF(n)	((n)*8)
	
/* Offsets into struct alpha_thread_state */
	
#define	TS_V0	TS_OFF(0)
#define TS_T0	TS_OFF(1)
#define TS_T1	TS_OFF(2)
#define TS_T2	TS_OFF(3)
#define TS_T3	TS_OFF(4)
#define TS_T4	TS_OFF(5)
#define TS_T5	TS_OFF(6)
#define TS_T6	TS_OFF(7)
#define TS_T7	TS_OFF(8)
#define TS_S0	TS_OFF(9)
#define TS_S1	TS_OFF(10)
#define TS_S2	TS_OFF(11)
#define TS_S3	TS_OFF(12)
#define TS_S4	TS_OFF(13)
#define TS_S5	TS_OFF(14)
#define TS_S6	TS_OFF(15)
#define TS_A0	TS_OFF(16)
#define TS_A1	TS_OFF(17)
#define TS_A2	TS_OFF(18)
#define TS_A3	TS_OFF(19)
#define TS_A4	TS_OFF(20)
#define TS_A5	TS_OFF(21)
#define TS_T8	TS_OFF(22)
#define TS_T9	TS_OFF(23)
#define TS_T10	TS_OFF(24)
#define TS_T11	TS_OFF(25)
#define TS_RA	TS_OFF(26)
#define TS_T12	TS_OFF(27)
#define TS_AT	TS_OFF(28)
#define	TS_GP	TS_OFF(29)
#define TS_SP	TS_OFF(30)
#define TS_PC	TS_OFF(31)

/* 
 * int emul_save_state(struct alpha_thread_state *state)
 *
 * Saves current registers into struct pointed to by arg.
 * Like "setjmp".
 *
 * v0 (return value) is not saved but is set to one.
 * Returns zero normally but may return anything when "longjmped" to.
 */
	
	.set	noat
	.set	noreorder

LEAF(emul_save_state, 1)
	
	.set	noreorder
	
#if DEBUG > 1
	call_pal 0x80
#endif	
	/* caller pc is now in ra. save it as pc */
	stq	ra, TS_PC(a0)
	
	/* save callee saved registers s0..s6 */
	stq	s0, TS_S0(a0)
	stq	s1, TS_S1(a0)
	stq	s2, TS_S2(a0)
	stq	s3, TS_S3(a0)
	stq	s4, TS_S4(a0)
	stq	s5, TS_S5(a0)
	stq	s6, TS_S6(a0)
	
	/* Save temporary registers t0..t12 */
	stq	t0, TS_T0(a0)
	stq	t1, TS_T1(a0)
	stq	t2, TS_T2(a0)
	stq	t3, TS_T3(a0)
	stq	t4, TS_T4(a0)
	stq	t5, TS_T5(a0)
	stq	t6, TS_T6(a0)
	stq	t7, TS_T7(a0)
	stq	t8, TS_T8(a0)
	stq	t9, TS_T9(a0)
	stq	t10, TS_T10(a0)
	stq	t11, TS_T11(a0)
	stq	t12, TS_T12(a0)
	
	/* Argument registers a0..a5 */
	stq	a0, TS_A0(a0)
	stq	a1, TS_A1(a0)
	stq	a2, TS_A2(a0)
	stq	a3, TS_A3(a0)
	stq	a4, TS_A4(a0)
	stq	a5, TS_A5(a0)
	
	/* Other regs */
	stq	ra, TS_RA(a0)
	stq	at, TS_AT(a0)
	stq	gp, TS_GP(a0)
	stq	sp, TS_SP(a0)
	
	/* save one as return value of 'longjump' */
	or	zero, 1, v0
	stq	v0, TS_V0(a0)

	/* But return 0 to the caller of this 'setjump' */
	or	zero,zero,v0
	RET
	END(emul_save_state)

/* 
 * noreturn emul_load_state(struct alpha_thread_state *state)
 *
 * Loads all cpu registers from struct pointed to by arg.
 * Like "longjmp".
 *
 * Never returns to caller. The stack is unwound.
 */
	.set	noat
	.set	noreorder
LEAF(emul_load_state, 1)
		
	.set	noreorder
	
#if DEBUG
	call_pal 0x80
#endif	
	
	/* load callee saved registers s0..s6 */
	ldq	s0, TS_S0(a0)
	ldq	s1, TS_S1(a0)
	ldq	s2, TS_S2(a0)
	ldq	s3, TS_S3(a0)
	ldq	s4, TS_S4(a0)
	ldq	s5, TS_S5(a0)
	ldq	s6, TS_S6(a0)
	
	/* Temporary registers t0..t12 */
	ldq	t0, TS_T0(a0)
	ldq	t1, TS_T1(a0)
	ldq	t2, TS_T2(a0)
	ldq	t3, TS_T3(a0)
	ldq	t4, TS_T4(a0)
	ldq	t5, TS_T5(a0)
	ldq	t6, TS_T6(a0)
	ldq	t7, TS_T7(a0)
	ldq	t8, TS_T8(a0)
	ldq	t9, TS_T9(a0)
	ldq	t10, TS_T10(a0)
	ldq	t11, TS_T11(a0)
	ldq	t12, TS_T12(a0)
	
	/* Argument registers a1..a5 */
	ldq	a1, TS_A1(a0)
	ldq	a2, TS_A2(a0)
	ldq	a3, TS_A3(a0)
	ldq	a4, TS_A4(a0)
	ldq	a5, TS_A5(a0)
	
	/* Other regs */
	ldq	v0, TS_V0(a0)
	ldq	ra, TS_RA(a0)
	ldq	at, TS_AT(a0)
	ldq	gp, TS_GP(a0)
	ldq	sp, TS_SP(a0)
	
	/* caller pc is loaded to a4 (return address) */
	ldq	a4, TS_PC(a0)
	
	/* Load the argument register a0. */
	ldq	a0, TS_A0(a0)

	/* ??How to return with all registers restored?? */
	
	jmp	zero, (a4)	/* CLOBBERS A4!.  Absolutely not ok */
	END(emul_load_state)
