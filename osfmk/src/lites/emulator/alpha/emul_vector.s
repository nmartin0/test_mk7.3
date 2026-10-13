/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1994 Jukka Virtanen
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND JUKKA VIRTANEN ALLOW FREE USE OF THIS SOFTWARE IN
 * ITS "AS IS" CONDITION.  CARNEGIE MELLON AND JUKKA VIRTANEN DISCLAIM
 * ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM
 * THE USE OF THIS SOFTWARE.
 */
/* 
 *	File:	 emulator/alpha/emul_vector.s
 *	Authors: Alessandro Forin (CMU 1992), Jukka Virtanen (HUT 1994)
 */

#include <sys/syscall.h>
#include <mach/alpha/asm.h>
#include <mach/alpha/alpha_instruction.h>

#define	DEBUG 	0

/*
 * Stack frame definitions [aka struct emul_exception_frame]
 *
 * In order to optimize signal handling, this
 * structure is laid down in accordance with
 * the sigcontext structure defined by ALPHA.
 * This saves extra copies in case we have to
 * take a signal on exit from a syscall.
 * [Note that this way we could define an even
 *  faster sigtramp, because the regstate got
 *  saved in user space to begin with..]
 */
#define SC_ONSTACK	0
#define SC_MASK		8
#define SC_PC		16
#define SC_PS		24
#define	SC_V0		32
#define	SC_T0		40
#define	SC_T1		48
#define	SC_T2		56
#define	SC_T3		64
#define	SC_T4		72
#define	SC_T5		80
#define	SC_T6		88
#define	SC_T7		96
#define	SC_S0		104
#define	SC_S1		112
#define	SC_S2		120
#define	SC_S3		128
#define	SC_S4		136
#define	SC_S5		144
#define	SC_S6		152
#define SC_A0		160
#define SC_A1		168
#define SC_A2		176
#define SC_A3		184
#define	SC_A4		192
#define	SC_A5		200
#define	SC_T8		208
#define	SC_T9		216
#define	SC_T10		224
#define	SC_T11		232
#define	SC_RA		240
#define	SC_T12		248
#define	SC_AT		256
#define	SC_GP		264
#define	SC_SP		272
#define	SC_ZERO		280

#define SC_USE_FPA	288
#define SC_FPA_REGS	296
			   /*(296+(32*8)) gives..*/
#define SC_SIZE	552

/* compiler leaves no room on stack for regargs == +(6*8) */
/* Stack must be allocated in 16 byte multiplies == rouding */
#define	SC_ALLOC	(((SC_SIZE+6*8)+0xff) &~(0xff))

/* make VECTOR macro happy */
#define	MSS_SIZE	SC_ALLOC

/*
 * Generic emulator entry, known outside to be 4 instr long
 */
#define	EE(n)	\
	stq a0,(SC_A0-SC_ALLOC)(sp); \
	lda a0,(n)(zero); \
	br zero,emul_common; \
	nop

/*
 * Generic call
 */
#define ONLY_S IM_S0|IM_S1|IM_S2|IM_S3|IM_S4|IM_S5|IM_S6
#define ALL_BUT_S (0xffffffff&(~ONLY_S))

	.set	noreorder
	.set	noat

VECTOR(emul_common,ALL_BUT_S)

#if	DEBUG
	call_pal	op_bpt
#endif
	stq	sp,(SC_SP-SC_ALLOC)(sp)
	lda	sp,-SC_ALLOC(sp)
	stq	at,SC_AT(sp)
	.set	reorder
emul_common_1:
	stq	a0,SC_V0(sp)		/* the syscall code */
	stq	v0,SC_PC(sp)		/* the return PC */
					/* a0 saved already */
	stq	a1,SC_A1(sp)
	stq	a2,SC_A2(sp)
	stq	a3,SC_A3(sp)
	stq	a4,SC_A4(sp)
	stq	a5,SC_A5(sp)
	stq	ra,SC_RA(sp)
	stq	gp,SC_GP(sp)
	stq	t0,SC_T0(sp)
	stq	t1,SC_T1(sp)
	stq	t2,SC_T2(sp)
	stq	t3,SC_T3(sp)
	stq	t4,SC_T4(sp)
	stq	t5,SC_T5(sp)
	stq	t6,SC_T6(sp)
	stq	t7,SC_T7(sp)
	stq	t8,SC_T8(sp)
	stq	t9,SC_T9(sp)
	stq	t10,SC_T10(sp)
	stq	t11,SC_T11(sp)
	stq	t12,SC_T12(sp)
	.set	noreorder

	br	t12,1f			/* get our gp */
1:	ldgp	gp,0(t12)

	/* emul_syscall (struct emul_regs *addr);
	   Stack pointer points to the saved reg area.
	   Give the address as parameter to emul_syscall.
	 */
	mov	sp,a0
	CALL(emul_syscall)
	
	/*
	 * Return here when syscall completes.
	 */
					/* restore user's registers */
	.set	reorder
	
	ldq	t0,SC_T0(sp)
	ldq	t1,SC_T1(sp)
	ldq	t2,SC_T2(sp)
	ldq	t3,SC_T3(sp)
	ldq	t4,SC_T4(sp)
	ldq	t5,SC_T5(sp)
	ldq	t6,SC_T6(sp)
	ldq	t7,SC_T7(sp)
	ldq	t8,SC_T8(sp)
	ldq	t9,SC_T9(sp)
	ldq	t10,SC_T10(sp)
	ldq	t11,SC_T11(sp)
	ldq	t12,SC_T12(sp)
	
	ldq	ra,SC_RA(sp)
	ldq	gp,SC_GP(sp)
	ldq	at,SC_AT(sp)
	ldq	v0,SC_V0(sp)
	ldq	a0,SC_A0(sp)
	ldq	a1,SC_A1(sp)
	ldq	a2,SC_A2(sp)
	ldq	a3,SC_A3(sp)
	ldq	a4,SC_A4(sp)
#if 0	
	ldq	a5,SC_A5(sp)
#endif
	ldq	a5,SC_PC(sp)		/* CLOBBERS A5 !! seems to be ok */
	.set	noreorder
	ldq	sp,SC_SP(sp)
	jmp	zero,(a5)

	END(emul_common)

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
EE(260);EE(261);EE(262);EE(263);EE(264);EE(265);EE(266);EE(267);EE(268);EE(269)

/*
 * Calls that do not fit in the table:
 */
LEAF(emul_task_by_pid,0)
	EE( -33)
	END(emul_task_by_pid)
LEAF(emul_pid_by_task,0)
	EE( -34)
	END(emul_pid_by_task)
LEAF(emul_init_process,0)
	EE(-41)
	END(emul_init_process)
LEAF(emul_htg_syscall,0)
	EE( -59)
	END(emul_htg_syscall)
