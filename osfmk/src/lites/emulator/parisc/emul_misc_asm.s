/* 
 * Copyright (c) 1990, 1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: emul_misc_asm.s 1.4 94/12/16$
 */

#include "parisc/asm.h"

	.space	$TEXT$
	.subspa $CODE$

/*
 * int
 * emul_save_state(struct parisc_thread_state *state)
 * 
 * Basically _setjmp: save callee-save registers, don't muck with signals.
 *
 * XXX lot of magic offsets for parisc_thread_state locations,
 *     should use some genassym-style method to produce offsets.
 */
	.export	emul_save_state,entry
	.proc
	.callinfo
emul_save_state

	.entry
	/*
	 * Save rp (r2), sp (r30) and the callee-save registers,
	 * XXX zero the caller save registers?
	 */
	stw	r0,0(arg0)	/* flags */
	stw	r2,8(arg0)	/* rp */
	stw	r3,12(arg0)
	stw	r4,16(arg0)
	stw	r5,20(arg0)
	stw	r6,24(arg0)
	stw	r7,28(arg0)
	stw	r8,32(arg0)
	stw	r9,36(arg0)
	stw	r10,40(arg0)
	stw	r11,44(arg0)
	stw	r12,48(arg0)
	stw	r13,52(arg0)
	stw	r14,56(arg0)
	stw	r15,60(arg0)
	stw	r16,64(arg0)
	stw	r17,68(arg0)
	stw	r18,72(arg0)
	stw	r27,108(arg0)	/* emulator dp */
	stw	r30,120(arg0)	/* sp */
	stw	r2,132(arg0)	/* iioq_head (rp) */
	ldo	4(r2),t1
	stw	t1,140(arg0)	/* iioq_tail (rp+4) */
	stw	r0,164(arg0)	/* ipsw -- passed to sig handlers */

	/*
	 * XXX save sr4 as the "canonical" space for this task.
	 * sr4 cannot be set by thread_set_state, but this location
	 * may be used by our caller as a return space value.
	 */
	mfsp	sr4,t1
	stw	t1,172(arg0)

	mfsp	sr3,t1
	stw	t1,188(arg0)

	/*
	 * Save FP state.
	 * If the address pointed to by arg0 is aligned then this is easy
	 */
	ldo	352(arg0),arg0
	bb,>=,n	arg0,29,$emul_save_state_aligned

	/*
	 * Not aligned, this is harder.
	 * We know that sp is currently aligned to a double word,
	 * use this double word as a temporary area.
	 */
	fstds	fr12,0(sp)
	ldw	0(sp),t1
	stwm	t1,4(arg0)
	ldw	4(sp),t1
	stwm	t1,4(arg0)

	fstds	fr13,0(sp)
	ldw	0(sp),t1
	stwm	t1,4(arg0)
	ldw	4(sp),t1
	stwm	t1,4(arg0)

	fstds	fr14,0(sp)
	ldw	0(sp),t1
	stwm	t1,4(arg0)
	ldw	4(sp),t1
	stwm	t1,4(arg0)

	fstds	fr15,0(sp)
	ldw	0(sp),t1
	stwm	t1,4(arg0)
	ldw	4(sp),t1
	stwm	t1,4(arg0)

	fstds	fr16,0(sp)
	ldw	0(sp),t1
	stwm	t1,4(arg0)
	ldw	4(sp),t1
	stwm	t1,4(arg0)

	fstds	fr17,0(sp)
	ldw	0(sp),t1
	stwm	t1,4(arg0)
	ldw	4(sp),t1
	stwm	t1,4(arg0)

	fstds	fr18,0(sp)
	ldw	0(sp),t1
	stwm	t1,4(arg0)
	ldw	4(sp),t1
	stwm	t1,4(arg0)

	fstds	fr19,0(sp)
	ldw	0(sp),t1
	stwm	t1,4(arg0)
	ldw	4(sp),t1
	stwm	t1,4(arg0)

	fstds	fr20,0(sp)
	ldw	0(sp),t1
	stwm	t1,4(arg0)
	ldw	4(sp),t1
	stwm	t1,4(arg0)

	fstds	fr21,0(sp)
	ldw	0(sp),t1
	stwm	t1,4(arg0)
	ldw	4(sp),t1
	b	$emul_save_state_exit
	stwm	t1,4(arg0)

$emul_save_state_aligned
	fstds,ma fr12,8(arg0)
	fstds,ma fr13,8(arg0)
	fstds,ma fr14,8(arg0)
	fstds,ma fr15,8(arg0)
	fstds,ma fr16,8(arg0)
	fstds,ma fr17,8(arg0)
	fstds,ma fr18,8(arg0)
	fstds,ma fr19,8(arg0)
	fstds,ma fr20,8(arg0)
	fstds	 fr21,0(arg0)

$emul_save_state_exit
	bv	0(rp)
	copy	r0,ret0
	.exit
	.procend



/*
 * void 
 * emul_load_state(struct parisc_thread_state *state)
 * 
 * Basically _longjmp: restore registers, don't muck with signals.
 * Forces a return to state->iioq_head with whatever value is in state->ret0.
 */
	.export	emul_load_state,entry
	.proc
	.callinfo
emul_load_state

	.entry
	/*
	 * Use the magic break instruction which forces a thread_set_state
	 * on the current thread.  This is necessary since this routine is
	 * used to do sigreturn.
	 */
	break	0,9	/* BREAK_KERNEL, BREAK_THREAD_SETSTATE */

#if 1
	/*
	 * XXX I'm leaving this in case the "right" way fails
	 * (i.e. we return from the break).
	 * We can at least do a half-assed restore in that case.
	 */
	copy	arg0,r2		/* saved state pointer */
	ldw	12(r2),r3	/* restore most general registers */
	ldw	16(r2),r4
	ldw	20(r2),r5
	ldw	24(r2),r6
	ldw	28(r2),r7
	ldw	32(r2),r8
	ldw	36(r2),r9
	ldw	40(r2),r10
	ldw	44(r2),r11
	ldw	48(r2),r12
	ldw	52(r2),r13
	ldw	56(r2),r14
	ldw	60(r2),r15
	ldw	64(r2),r16
	ldw	68(r2),r17
	ldw	72(r2),r18
	ldw	76(r2),r19
	ldw	80(r2),r20
	ldw	84(r2),r21
	ldw	88(r2),r22
	ldw	92(r2),r23
	ldw	96(r2),r24
	ldw	100(r2),r25
	ldw	104(r2),r26
	ldw	108(r2),r27
	ldw	112(r2),r28
	ldw	116(r2),r29
	ldw	120(r2),r30
	ldw	124(r2),r31

	ldw	128(r2),r1	/* and sar */
	mtctl	r1,cr11

	ldw	176(r2),r1	/* and space registers */
	mtsp	r1,sr0
	ldw	180(r2),r1
	mtsp	r1,sr1
	ldw	184(r2),r1
	mtsp	r1,sr2
	ldw	188(r2),r1
	mtsp	r1,sr3

				/* XXX ignore FP for now */

	ldw	4(r2),r1	/* get r1 value from saved state */
	stw	r1,0(sp)	/* store on user stack for later */
	ldw	132(r2),r1	/* saved state iioqh is new pc */
	ldw	8(r2),r2	/* restore r2 (loses saved state ptr) */
	be	0(sr4,r1)	/* return and ... */
	ldw	0(sp),r1	/* ... reload final GR */
#endif
	.exit
	.procend

/*
 * Return the emulator DP value.
 */
	.export	emul_dp,entry
	.proc
	.callinfo
emul_dp
	.entry
	ldil	L%$global$,ret0
	bv	0(rp)
	ldo	R%$global$(ret0),ret0
	.exit
	.procend

/*
 * Set the current DP value, return the old value.
 */
	.export	emul_set_dp,entry
	.proc
	.callinfo
emul_set_dp
	.entry
	copy	dp,ret0
	bv	0(rp)
	copy	arg0,dp
	.exit
	.procend
