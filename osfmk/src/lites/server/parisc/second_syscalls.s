/* 
 * Copyright (c) 1992,1993,1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 */
/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * system calls needed by second server to access the
 * primary server via unix system calls.
 * We just rename them unix_foo instead of foo not to
 * mix up with this server entries.
 */

#include <sys/syscall.h>
#include <machine/asm.h>

#ifndef SYS_sigvec
#define SYS_sigvec 108
#endif
	.space	$TEXT$
	.subspa	$CODE$

	.IMPORT	second_cerror,code

/*
 * XXX Need to merge this with the ukernel.
 *
 * HACK HACK HACK: For reasons unknown every read from the console
 * under the 2nd server claims to return an error, though the chars
 * we want are available...  Hack Hack Hack, ignore them.
 *
 */
#define SYSCALL(name)\
second_##name##:					!\
		.proc					!\
		.callinfo				!\
		.entry					!\
		ldil	L%0xC0000000,%r1		!\
		ble	4(%sr7,%r1)			!\
		ldi	SYS_##name,%r22			!\
		or,tr	%r0,%r22,%r0			!\
		b,n	second_cerror			!\
		bv,n	0(%r2)				!\
		.exit					!\
		.procend				!\
		.export	second_##name,entry

#define SYSTRAP(second_name,name)\
second_##second_name##:					!\
		.proc					!\
		.callinfo				!\
		.entry					!\
		ldil	L%0xC0000000,%r1		!\
		ble	4(%sr7,%r1)			!\
		ldi	SYS_##name,%r22			!\
		bv,n	0(%r2)				!\
		.exit					!\
		.procend				!\
		.export	second_##second_name,entry

SYSCALL(open)
SYSCALL(fstat)
SYSCALL(access)
SYSCALL(lseek)
SYSCALL(write)
SYSCALL(read)
SYSCALL(close)
SYSCALL(ioctl)
SYSCALL(sigvec)
SYSCALL(sigreturn)
SYSCALL(getpid)

SYSTRAP(_exit,exit)

/*
 * second cerror stuff
 */
	.export	second_cerror,entry
	.proc
	.callinfo
second_cerror
	.entry
	addil	L%second_errno-$global$,%dp
	stw	%ret0,R%second_errno-$global$(%r1)
	bv	0(%r2)
	ldi     -1,%ret0
	.exit
	.procend

	.space  $PRIVATE$
	.subspa $DATA$

	.export	second_errno,data
second_errno
	.WORD	0
