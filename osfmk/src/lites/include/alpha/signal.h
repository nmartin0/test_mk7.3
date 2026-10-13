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
 * Machine-dependent signal definitions
 */
#ifndef _ALPHA__SIGNAL_H_
#define _ALPHA_SIGNAL_H_

struct  sigcontext {
  	integer_t sc_onstack;
        integer_t sc_mask;
        integer_t sc_pc;
        integer_t sc_ps;
        integer_t sc_regs[32];
        integer_t sc_ownedfp;
        integer_t sc_fpregs[32];
        natural_t sc_fpcr;
        natural_t sc_fp_control;

        integer_t sc_reserved1;
        integer_t sc_reserved2;
        size_t    sc_ssize;
        caddr_t   sc_sbase;
        natural_t sc_traparg_a0;
        natural_t sc_traparg_a1;
        natural_t sc_traparg_a2;
        natural_t sc_fp_trap_pc;
        natural_t sc_fp_trigger_sum;
        natural_t sc_fp_trigger_inst;
};

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

#endif /* _ALPHA_SIGNAL_H_ */
