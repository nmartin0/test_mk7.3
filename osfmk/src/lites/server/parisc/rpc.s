/* 
 * Copyright (c) 1992,1993,1994, The University of Utah and
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
 * 	Utah $Hdr: rpc.s 1.6 95/08/21$
 */
	.space	$PRIVATE$
	.subspa	$DATA$
$migr_rpc_panic
        .string "migr_rpc_exit returned!\x00"

	.SPACE $TEXT$
	.SUBSPA $CODE$
	.align 4
	.EXPORT migr_rpc_entry,ENTRY
migr_rpc_entry
	.PROC
	.CALLINFO FRAME=192,CALLS
	.ENTRY
	/*
	 * If informed that more activations are needed, handle that first.
	 */
	comib,=,n 0,%arg3,$haveacts
	stw	%arg2,-44(%sp)
	stw	%arg1,-40(%sp)
	.IMPORT	ux_more_acts,CODE
	.CALL
	bl	ux_more_acts,%rp
	stw	%arg0,-36(%sp)
	ldw	-44(%sp),%arg2
	ldw	-40(%sp),%arg1
	ldw	-36(%sp),%arg0
$haveacts
#ifdef USENRPC
	/*
	 * If arg1 is not 0, then it is a new MOM-style RPC.
	 * If arg1 is 0, it may be an old mach_msg compatible RPC but
	 * we must check _param0 value as well to differentiate from
	 * MOM stubs (which don't use arg1 and hence could be anything).
	 */
	comib,<> 0,%arg1,$nrpc
	ldil	L%5065031,%ret0
	ldo	R%5065031(%ret0),%ret0
	comb,<>,n %ret0,%r5,$orpc
	/*
	 * Otherwise jump straight to the new-style demux routine
	 * using the stack frame allocated by the kernel.
	 */
$nrpc
	.IMPORT	dispatcher,CODE
	.CALL
	/*
	 * dispatcher(port, id, seqno, moreacts)
	 *	may return so we perform the magic trap afterward
	 */
	bl	dispatcher,%r2
	ldo	migr_rpc_exit-$nrpc2(%r2),%r2
$nrpc2
#endif
$orpc
	.IMPORT	ux_act_entry,CODE
	.CALL
	/*
	 * ux_act_entry(request_ptr)
	 *
	 * Stack frame:
	 *	-192:	iargs	64 bytes
	 *	-128:	oargs	64 bytes
	 *	- 64:	frame	64 bytes
	 */
	bl	ux_act_entry,%rp
	ldo	192(%sp),%sp
	copy	%r0,%arg0
	copy	%r0,%arg1
	b	migr_rpc_exit
	copy	%r0,%ret0
	.EXIT
	.PROCEND

	.align 4
	.EXPORT migr_rpc_exit,ENTRY
migr_rpc_exit
	.PROC
	.CALLINFO
	.ENTRY
	ldil	L%0xC0000000,%r1
	ble	0x10(%sr7,%r1)
	nop
	/*
	 * YOW!  We returned for some reason, blow chunks
	 */
	ldil	L%$migr_rpc_panic,%arg0
	.IMPORT	panic,code
	.CALL
	bl	panic,%rp
	ldo	R%$migr_rpc_panic(%arg0),%arg0
	.EXIT
	.PROCEND
