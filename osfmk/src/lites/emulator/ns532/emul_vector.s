/* 
 * Mach Operating System
 * Copyright (c) 1994 Ian Dall
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * IAN DALL ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
 * IAN DALL DISCLAIMS ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/*
 * HISTORY
 * $Log: emul_vector.s,v $
# Revision 1.2  1996/03/14  21:07:52  sclawson
# Ian Dall's signal fixes.
#
# Revision 1.1.1.1  1995/03/02  21:49:29  mike
# Initial Lites release from hut.fi
#
 */
/* 
 *	File:	emulator/ns532/emul_vector.s
 *	Author:	Ian Dall
 *	Date:	September 1994
 *
 *	Entry points from system call trampolines.
 */

/*
 * Emulator entry from BSD or SysV binaries.
 *
 *					ffffffff
 *		...
 *		args
 *		pc to return to
 * 	SP->	flags
 *					00000000
 * system call number is in r0
 */

/*
 * Generic call.
 */
	.globl _emul_trampoline
_emul_trampoline:
	save [r0,r1,r2,r3,r4,r5,r6,r7]
	sprd	fp,tos
	sprd	sb,tos
	lprd	sb,0                    /* the C-compiler may need this */
	sprd	sp,tos			/* pointer to struct (above) */
	bsr	_emul_syscall
	adjspb	-4
/*
 * Return
 */
/*	.globl	emul_exit */
emul_exit:
_emul_exit:
	lprd	sb,tos
	lprd	fp,tos
	restore [r0,r1,r2,r3,r4,r5,r6,r7]
	lprd	us, tos			/* load psr */
	ret	0

/* EOF */

