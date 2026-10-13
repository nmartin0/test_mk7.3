/*
 * Copyright (c) 1991,1992,1994 The University of Utah and
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
 * 	Utah $Hdr: ecrt0.s 1.5 94/12/16$
 */

/*
 * User startup routine.
 * Nearly identical to libmach crt0.s, except:
 *
 *	has no arguments
 *	call emulator_main instead of main with old and new SP values
 */


#include "parisc/asm.h"

	.space	$TEXT$

	.subspa $UNWIND_START$,QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=56
	.subspa	$CODE$

/*
 * $START$(argc, argv, envp)
 * 	int argc;
 * 	char **argv;
 * 	char **envp;
 *
 * Initial execution of a task. We assume that our parent set us up with
 * a valid stack to run on.
 */
	.proc
	.callinfo SAVE_SP,FRAME=128
	.export	$START$,entry
	.entry
$START$ 
	/* 
	 * I'm not all that happy about needing to do this here, but
	 * I'm not going to fight it.
	 *
	 * Zero any BSS space that actually ended up on the last page
	 * of the initialized data.
	 *
	 * We can't pass values in registers (well, we could pass in %dp
	 * and %r2, but that's real gross), so the server places the
	 * magic values at sp-8 (start address) and sp-4 (size) on our stack.
	 * we deallocate the extra space once we've snarf'd the data.  */
	ldw -8(sp),%r21
	ldw -4(sp),%r22
	ldo -64(sp),sp
	comib,=,n 0,%r22,L$load_dp
L$store_zero:
	addibf,= -1,%r22,L$store_zero
	stbs,ma %r0,1(%r21)
	
L$load_dp:
	/*
	 * initialize the global data pointer dp
	 */
	ldil    L%$global$,dp
	ldo     R%$global$(dp),dp

	/*
	 * Double word align the stack and allocate a stack frame to start out
	 * with, save this value for passing to emulator_main.
	 */
	ldo     128(sp),sp
	depi    0,31,3,sp
	copy	sp,r10
	copy	sp,r11

	/*
	 * See if mach_init is defined. If so, call it to get MIG initialized
	 * for the task.
	 */
        .import mach_init_routine,data
	.import $$dyncall,millicode
        ldil    L%mach_init_routine,%r22
        ldw     R%mach_init_routine(%r22),%r22

        comb,=,n r0,t1,$no_minit

	.call
	bl $$dyncall,%r31
	copy %r31,%r2

$no_minit
	/*
	 * Okay, get ready to call main (finally). First we have to reload
	 * the argument registers.
	 */

	copy	r10,arg0
	copy	r11,arg1

	.import emulator_main,code
	ldil	L%emulator_main,%r22
	ldo	R%emulator_main(%r22),%r22

	.call
	bl $$dyncall,%r31
	copy %r31,%r2

	/*
	 * call exit() directly with the return status from main.
	 */
	.import exit,code
	ldil	L%exit,%r22
	ldo	R%exit(%r22),%r22

	copy	ret0,arg0

	.call
	bl $$dyncall,%r31
	copy %r31,%r2

	/*
	 * never returns
	 */
	break	0,0

	.exit
	.procend

	.subspa $UNWIND_START$
	.export $UNWIND_START,data
$UNWIND_START
	.subspa	$UNWIND_END$,QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=72
	.export	$UNWIND_END,data
$UNWIND_END

	.subspa	$RECOVER_START$,QUAD=0,ALIGN=4,ACCESS=0x2c,SORT=73
	.export $RECOVER_START,data
$RECOVER_START
	.subspa	$RECOVER$MILLICODE$,QUAD=0,ALIGN=4,ACCESS=0x2c,SORT=78
	.subspa	$RECOVER$
	.subspa	$RECOVER_END$,QUAD=0,ALIGN=4,ACCESS=0x2c,SORT=88
	.export	$RECOVER_END,data
$RECOVER_END

	.space	$PRIVATE$
        .SUBSPA $DATA$

mach_init_routine	.comm 8

	.subspa	$GLOBAL$
	.export	$global$,data
$global$

	.end
