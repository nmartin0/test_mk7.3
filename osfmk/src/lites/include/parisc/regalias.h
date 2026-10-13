/* 
 * Copyright (c) 1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the Computer
 * Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 */
/*
 * HISTORY
 * $Log: regalias.h,v $
 * Revision 1.1.1.1  1995/03/02  21:49:38  mike
 * Initial Lites release from hut.fi
 *
 */

#ifndef	ASSEMBLER
/*
 * useful synonyms for registers
 */
#define rp	r2
#define arg3	r23
#define arg2	r24
#define arg1	r25
#define arg0	r26
#define	dp	r27
#define	ret0	r28
#define	ret1	r29
#define	sp	r30

#define fret	fr4
#define farg0	fr4
#define farg1	fr5
#define farg2	fr6
#define farg3	fr7

#define rctr	cr0
#define pidr1	cr8
#define pidr2	cr9
#define ccr	cr10
#define sar	cr11
#define pidr3	cr12
#define pidr4	cr13
#define iva	cr14
#define eiem	cr15
#define itmr    cr16
#define pcsq	cr17
#define pcoq	cr18
#define iir	cr19
#define isr	cr20
#define ior	cr21
#define ipsw    cr22
#define eirr	cr23
#define ppda    cr24
#define hta     cr25
#endif	ASSEMBLER
