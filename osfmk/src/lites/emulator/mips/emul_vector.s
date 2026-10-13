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
 * $Log: emul_vector.s,v $
# Revision 1.1.1.1  1995/03/02  21:49:32  mike
# Initial Lites release from hut.fi
#
 */
/* 
 *	File:	emulator/mips/emul_vector.s
 *	Author:	Johannes Helander, Helsinki University of Technology, 1994.
 *	Date:	November 1994
 *
 *	Entry points from system call trampolines.
 *
 *	This code uses ideas by Alessandro Forin.
 */

#include <mach/mips/asm.h>

/* 
 * When coming from the kernel trampoline v0 contains the trap PC.
 * Since v0 also was supposed to have the syscall number we need to
 * regenerate it in the trampoline code.  Each system call has its own
 * trampoline so the trampoline knows the number and loads an immediate
 * after first saving a register.
 *
 * The whole system makes saving all registers on the stack possible.
 * Currently I'm saving less.
 *
 * Before saving any registers the stack should be enlarged so as to
 * avoid losing values in an asynch signal.
 */
#define	EE(n)		sw a0,SAVE_ARG0(sp); j emul_common; li a0,(n)

/* 
 *	a3
 *	a2
 *	a1
 * osp->a0 <-vfp
 *	ret_v1
 *	ret_a3
 *	ra
 *	gp
 *	at (saved pc)
 *	a3, ...		save area for called function
 *	a2
 *	a1
 * sp->	a0
 */
/* 
 * Debug info for a stack trace that will skip the system call stub.
 * The stub did not include any debug info nor did it use the stack.
 */

/* Save area for args. relative to virtual fp (original sp) */
#define SAVE_ARG3	12
#define SAVE_ARG2	8
#define SAVE_ARG1	4
#define SAVE_ARG0	0

/* Saved stuff (relative to new sp) */
#define SAVE_SIZE	36
#define SAVE_LOCAL_V1	32		/* local variable: for ret v1 */
#define SAVE_LOCAL_A3	28		/* local variable: for ret a3 */
#define SAVE_LOCAL_PC	24
#define SAVE_RA		20
#define SAVE_GP		16
/*
#define SAVE_A3		12
#define SAVE_A2		8
#define SAVE_A1		4
#define SAVE_A0		0
*/

	.set noreorder
	.globl emul_common
	.ent emul_common
emul_common:
	.frame	sp, SAVE_SIZE, $0
	.mask	M_RA | M_GP, (SAVE_RA-SAVE_SIZE)

	sw	a1, SAVE_ARG1(sp)
	sw	a2, SAVE_ARG2(sp)
	sw	a3, SAVE_ARG3(sp)

	move	a2, sp		/* orig sp: points to args give as arg2 */
	addu	sp, -SAVE_SIZE

	sw	v0, SAVE_LOCAL_PC(sp)
	sw	v1, SAVE_LOCAL_V1(sp)	/* for brk failure case */
	sw	ra, SAVE_RA(sp)
	sw	gp, SAVE_GP(sp)

	la	gp, _gp

	/* sp points to struct. Give it as arg1. (arg0 is syscode) */
	move	a1, sp
	/* int emul_syscall(int syscode, struct emul_regs *a0, int *args) */
	jal	emul_syscall
	nop

.set noat				/* v0 already contains retval */
	lw	a3, SAVE_LOCAL_A3(sp)	/* 1 err, 0 ok */
	lw	v1, SAVE_LOCAL_V1(sp)	/* return value msw */
	lw	AT, SAVE_LOCAL_PC(sp)	/* stub pc */
	lw	gp, SAVE_GP(sp)
	lw	ra, SAVE_RA(sp)
	addu	sp, SAVE_SIZE
	j	AT
	lw	a0, SAVE_ARG0(sp)	/* restore a0 for brk */
.set at
END(emul_trampoline)



/*
 * Declare the trap vectors
 */
EE(-10);EE(-9); EE(-8); EE(-7); EE(-6); EE(-5); EE(-4); EE(-3); EE(-2); EE(-1)
	EXPORT(emul_vector_base)
EE(  0);EE(  1);EE(  2);EE(  3);EE(  4);EE(  5);EE(  6);EE(  7);EE(  8);EE(  9)
EE( 10);EE( 11);EE( 12);EE( 13);EE( 14);EE( 15);EE( 16);EE( 17);EE( 18);EE( 19)
EE( 20);EE( 21);EE( 22);EE( 23);EE( 24);EE( 25);EE( 26);EE( 27);EE( 28);EE( 29)
EE( 30);EE( 31);EE( 32);EE( 33);EE( 34);EE( 35);EE( 36);EE( 37);EE( 38);EE( 39)
EE( 40);EE( 41);EE( 42);EE( 43);EE( 44);EE( 45);EE( 46);EE( 47);EE( 48);EE( 49)
EE( 50);EE( 51);EE( 52);EE( 53);EE( 54);EE( 55);EE( 56);EE( 57);EE( 58);EE( 59)
EE( 60);EE( 61);EE( 62);EE( 63);EE( 64);EE( 65);EE( 66);EE( 67);EE( 68);EE( 69)
EE( 70);EE( 71);EE( 72);EE( 73);EE( 74);EE( 75);EE( 76);EE( 77);EE( 78);EE( 79)
EE( 80);EE( 81);EE( 82);EE( 83);EE( 84);EE( 85);EE( 86);EE( 87);EE( 88);EE( 89)
EE( 90);EE( 91);EE( 92);EE( 93);EE( 94);EE( 95);EE( 96);EE( 97);EE( 98);EE( 99)
EE(100);EE(101);EE(102);EE(103);EE(104);EE(105);EE(106);EE(107);EE(108);EE(109)
EE(110);EE(111);EE(112);EE(113);EE(114);EE(115);EE(116);EE(117);EE(118);EE(119)
EE(120);EE(121);EE(122);EE(123);EE(124);EE(125);EE(126);EE(127);EE(128);EE(129)
EE(130);EE(131);EE(132);EE(133);EE(134);EE(135);EE(136);EE(137);EE(138);EE(139)
EE(140);EE(141);EE(142);EE(143);EE(144);EE(145);EE(146);EE(147);EE(148);EE(149)
EE(150);EE(151);EE(152);EE(153);EE(154);EE(155);EE(156);EE(157);EE(158);EE(159)
EE(160);EE(161);EE(162);EE(163);EE(164);EE(165);EE(166);EE(167);EE(168);EE(169)
EE(170);EE(171);EE(172);EE(173);EE(174);EE(175);EE(176);EE(177);EE(178);EE(179)
EE(180);EE(181);EE(182);EE(183);EE(184);EE(185);EE(186);EE(187);EE(188);EE(189)
EE(190);EE(191);EE(192);EE(193);EE(194);EE(195);EE(196);EE(197);EE(198);EE(199)
EE(200);EE(201);EE(202);EE(203);EE(204);EE(205);EE(206);EE(207);EE(208);EE(209)
EE(210);EE(211);EE(212);EE(213);EE(214);EE(215);EE(216);EE(217);EE(218);EE(219)
EE(220);EE(221);EE(222);EE(223);EE(224);EE(225);EE(226);EE(227);EE(228);EE(229)
EE(230);EE(231);EE(232);EE(233);EE(234);EE(235);EE(236);EE(237);EE(238);EE(239)
EE(240);EE(241);EE(242);EE(243);EE(244);EE(245);EE(246);EE(247);EE(248);EE(249)
EE(250);EE(251);EE(252);EE(253);EE(254);EE(255);EE(256);EE(257);EE(258);EE(259)

/*
 * Calls that do not fit in the table:
 */
LEAF(emul_task_by_pid)
	EE( -33)
	END(emul_task_by_pid)
LEAF(emul_pid_by_task)
	EE( -34)
	END(emul_pid_by_task)
LEAF(emul_init_process)
	EE(-41)
	END(emul_init_process)
LEAF(emul_map_fd)
	EE( -43)
	END(emul_map_fd)
LEAF(emul_htg_syscall)
	EE( -59)
	END(emul_htg_syscall)
LEAF(emul_set_ras_address)
	EE( -53)
	END(emul_set_ras_address)


