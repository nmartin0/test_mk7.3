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
 * 02-Jan-95  Ian Dall (dall@hfrd.dsto.gov.au)
 *	Use sp to reference argument in ffs(). More efficient.
 *
 * 02-Jan-95  Ian Dall (dall@hfrd.dsto.gov.au)
 *	ntohl() and ntohs() used fp without initializing it. Use sp to
 *	reference arguments.
 *
 * 15-Dec-94  Johannes Helander (jvh) at Helsinki University of Technology
 *	Added byte order swapping functions.
 *
 * $Log: misc_asm.s,v $
# Revision 1.1.1.1  1995/03/02  21:49:40  mike
# Initial Lites release from hut.fi
#
 */
/* 
 *	File:	server/ns532/server/misc_asm.c
 *	Author:	Ian Dall, 1994.
 *
 *	Provide ffs function. This is a gcc built in when compiling with
 *	-O.
 */

.text
	.align 2
.globl _ffs
_ffs:
	movqd 0,r0
	ffsd 4(sp),r0
	bfs L1
	addqd 1,r0
L1:
	ret 0

.globl _htonl
.globl _ntohl
_htonl:
_ntohl:
	movd	4(sp),r0
	rotw	8,r0
	rotd	16,r0
	rotw	8,r0
	ret	0

.globl _htons
.globl _ntohs
_htons:
_ntohs:
	movd	4(sp),r0
	rotw	8,r0
	ret	0
