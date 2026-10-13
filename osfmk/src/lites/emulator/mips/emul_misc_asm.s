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
 * $Log: emul_misc_asm.s,v $
# Revision 1.1.1.1  1995/03/02  21:49:32  mike
# Initial Lites release from hut.fi
#
 */
/* 
 *	File:	emulator/i386/misc_asm.c
 *	Author:	Johannes Helander, Helsinki University of Technology, 1994.
 *	Date:	November 1994
 *
 *	save/load state & random stuff.
 */

#include <mach/mips/asm.h>

/* Offsets into struct mips_thread_state */
#define TS_AT	0x00
#define TS_V0	0x04
#define TS_V1	0x08
#define TS_A0	0x0c
#define TS_A1	0x10
#define TS_A2	0x14
#define TS_A3	0x18
#define TS_T0	0x1c
#define TS_T1	0x20
#define TS_T2	0x24
#define TS_T3	0x28
#define TS_T4	0x2c
#define TS_T5	0x30
#define TS_T6	0x34
#define TS_T7	0x38
#define TS_S0	0x3c
#define TS_S1	0x40
#define TS_S2	0x44
#define TS_S3	0x48
#define TS_S4	0x4c
#define TS_S5	0x50
#define TS_S6	0x54
#define TS_S7	0x58
#define TS_T8	0x5c
#define TS_T9	0x60
#define TS_K0	0x64
#define TS_K1	0x68
#define TS_GP	0x6c
#define TS_SP	0x70
#define TS_FP	0x74
#define TS_RA	0x78
#define TS_MDLO	0x7c
#define TS_MDHI	0x80
#define TS_PC	0x84

/* 
 * int emul_save_state(struct mips_thread_state *state)
 *
 * Saves current registers into struct pointed to by arg.
 * "setjmp".
 *
 * Caller saved regs at,v1,a0-3,t0-7,ra,mdlo,mdhi are not saved but zeroed.
 * v0 (return value) is not saved but is set to one.
 * Returns zero normally but may return anything when "longjmped" to.
 */

.set noreorder
.set nomacro
.set noat
.set volatile

LEAF(emul_save_state)
	/* caller pc is now in ra. save it as pc */
	sw	ra, TS_PC(a0)
	/* save callee saved registers */
	sw	s0, TS_S0(a0)
	sw	s1, TS_S1(a0)
	sw	s2, TS_S2(a0)
	sw	s3, TS_S3(a0)
	sw	s4, TS_S4(a0)
	sw	s5, TS_S5(a0)
	sw	s6, TS_S6(a0)
	sw	s7, TS_S7(a0)
	sw	t8, TS_T8(a0)
	sw	t9, TS_T9(a0)
	sw	gp, TS_GP(a0)
	sw	sp, TS_SP(a0)
	sw	fp, TS_FP(a0)

	/* save one as return value */
	lui	t0, 1
	sw	t0, TS_V0(a0)
	/* caller saved registers: save as zero */
	sw	zero, TS_AT(a0)
	sw	zero, TS_V1(a0)
	sw	zero, TS_A0(a0)
	sw	zero, TS_A1(a0)
	sw	zero, TS_A2(a0)
	sw	zero, TS_A3(a0)
	sw	zero, TS_T0(a0)
	sw	zero, TS_T1(a0)
	sw	zero, TS_T2(a0)
	sw	zero, TS_T3(a0)
	sw	zero, TS_T4(a0)
	sw	zero, TS_T5(a0)
	sw	zero, TS_T6(a0)
	sw	zero, TS_T7(a0)
	sw	zero, TS_RA(a0)
	sw	zero, TS_MDLO(a0)
	sw	zero, TS_MDHI(a0)
	/* k0,k1 are not used by user programs */
	sw	zero, TS_K0(a0)
	sw	zero, TS_K1(a0)

	/* return */
	j	ra
	move	v0, zero		/* return zero */
END(emul_save_state)

/* 
 * noreturn emul_load_state(struct mips_thread_state *state)
 *
 * Loads all cpu registers (except k0,k1) from struct pointed to by arg.
 * "longjmp".
 *
 * Never returns. The stack is unwound.
 */

LEAF(emul_load_state)
	/* First load most registers */
	lw	AT, TS_AT(a0)
	lw	v0, TS_V0(a0)
	lw	v1, TS_V1(a0)
	lw	a2, TS_A2(a0)
	lw	a3, TS_A3(a0)
	lw	t0, TS_T0(a0)
	lw	t1, TS_T1(a0)
	lw	t2, TS_T2(a0)
	lw	t3, TS_T3(a0)
	lw	t4, TS_T4(a0)
	lw	t5, TS_T5(a0)
	lw	t6, TS_T6(a0)
	lw	t7, TS_T7(a0)
	lw	s0, TS_S0(a0)
	lw	s1, TS_S1(a0)
	lw	s2, TS_S2(a0)
	lw	s3, TS_S3(a0)
	lw	s4, TS_S4(a0)
	lw	s5, TS_S5(a0)
	lw	s6, TS_S6(a0)
	lw	s7, TS_S7(a0)
	lw	t8, TS_T8(a0)
	lw	t9, TS_T9(a0)
	lw	gp, TS_GP(a0)
	lw	fp, TS_FP(a0)
	lw	ra, TS_RA(a0)

	/* mdlo, mdhi must be moved through another register. */
	lw	a1, TS_MDLO(a0)
	nop
	mtlo	a1
	lw	a1, TS_MDHI(a0)
	nop
	mthi	a1

	/* Fetch the stack pointer, add some room */
	lw	a1, TS_SP(a0)
	nop
	addiu	a1, -4
	/* Now let's use the new stack */
	move	sp, a1

	/* Fetch a0 and put it into the new stack */
	lw	a1, TS_A0(a0)
	nop
	sw	a1, 0(sp)

	/* Now restore a1 */
	lw	a1, TS_A1(a0)

	/* 
	 * NOW pc and a0 are on stack. Pop them and return.
	 * Coordinate with asynchronous signal activator!
	 */
	/* Fetch pc from stack */
	lw	a0, TS_PC(a0)
	/* BEGIN CRITICAL SECTION */
XLEAF(emul_load_space_critical_0)
	addiu	sp, 4
	/* a0 value is now OUTSIDE STACK */
XLEAF(emul_load_space_critical_1)
	/* Jump and load a0 in the delay slot */
	j	a0
XLEAF(emul_load_space_critical_2)
	lw	a0, -4(sp)
XLEAF(emul_load_space_critical_3)
	/* END CRITICAL SECTION */
END(emul_load_state)
