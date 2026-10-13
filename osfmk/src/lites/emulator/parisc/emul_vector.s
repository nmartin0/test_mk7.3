/* 
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 The University of Utah and
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
 * 	Utah $Hdr: emul_vector.s 1.5 95/03/03$
 */

/*
 * Emulator entry from Utah HPBSD or HP-UX binaries.
 *
 *	sr0/r31		return PC
 *	r22		system call number
 *	arg0-arg3	first 4 args
 *	sp - 0x34	arg4
 *	sp - 0x38	arg5
 *	...
 *
 * Note that arguments go *backwards* (toward lower addresses) in the stack.
 */

#include <machine/asm.h>

	.space	$TEXT$
	.subspa	$CODE$

	.import	emul_syscall,code

/*
 * We come here after reflecting off of the kernel...
 * We permit the kernel to trash some of our caller-saved registers;
 * the exceptions are arg0-arg3 (arguments to the system call) and
 * r22 (the system call number).
 */
	.export emul_trampoline,entry
emul_trampoline
	/*
	 * Allocate a frame for an emul_regs structure (and the standard
	 * frame marker) and tuck away a few essential registers.
	 */
	ldo	-32(sp),r1
	stwm	r1,128(sp)	/* arg ptr */
	stw	r0,-124(sp)	/* flags */
	stw	r2,-120(sp)	/* stub caller return offset */
	stw	r22,-116(sp)	/* syscall number */
	stw	r27,-112(sp)	/* caller dp */
	mfsp	sr0,r1
	stw	r31,-108(sp)	/* return offset for stub */
	stw	r1,-104(sp)	/* return space for stub */

	/*
	 * Load up the emulator DP.
	 * XXX an absolute relocation.
	 */
	ldil	L%$global$,r1
	ldo	R%$global$(r1),r27

	/*
	 * Call C code for system call emulation.
	 * arg0-3 are the first 4 syscall args, arg4 is a pointer to emul_regs.
	 */
	ldo	-128(sp),r1
	.call	argw0=gr,argw1=gr,argw2=gr,argw3=gr,rtnval=gr
	bl	emul_syscall,r2
	stw	r1,-52(sp)

	/*
	 * Clean up.
	 * Recover r22 (error indicator) and ret1 from the stack.
	 * We interpret flags to determine what action to take.
	 */
	ldw	-124(sp),r1
	ldw	-116(sp),r22
	comb,=	r0,r1,ec_escape		/* Hopefully this is usually taken */
	ldw	-100(sp),ret1

	/*
	 * SB_RESTART: reload syscall num (done above) and args in
	 * registers for restarted system calls.
	 */
	bb,>=,n	r1,30,ec_after_restart	/* sb_flags & SB_RESTART */
	ldw	-128(sp),arg0
	ldw	-16(arg0),arg3
	ldw	-12(arg0),arg2
	ldw	-8(arg0),arg1
	ldw	-4(arg0),arg0
ec_after_restart
	/*
	 * Tests for other flags...
	 */

ec_escape
	/*
	 * Branch back to the return point in the user's system call stub.
	 * Restore all remaining necessary caller-saved registers.
	 */
#if 0
/* XXX sr0 will be wrong in the child of a fork */
	ldw	-104(sp),r31
#else
	mfsp	sr4,r31
#endif
	ldw	-112(sp),r27
	mtsp	r31,sr0
	ldw	-108(sp),r31
	ldw	-120(sp),r2
	ldo	-128(sp),sp
	extru,=	r1,29,1,r0		/* sb_flags & SB_NEW_SP_IN_RET1 */
	copy	ret1,sp			/* restore SP for longjmp */
	be,n	0(sr0,r31)

/*
 * GCC invokes LD with "-u main", keep it happy.
 */
	.export main,entry
main
	bv,n	0(r2)
