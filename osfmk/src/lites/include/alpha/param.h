/* 
 * Mach Operating System
 * Copyright (c) 1995 Jukka Virtanen
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JUKKA VIRTANEN ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION. JUKKA VIRTANEN DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)param.h	8.1 (Berkeley) 6/11/93
 */
/*
 * Machine dependent constants for Alpha AXP.
 */

#ifndef _ALPHA_PARAM_H_
#define _ALPHA_PARAM_H_

#define	MACHINE	"alpha"

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is U_LONG and must be cast to
 * any desired pointer type.
 */
#define	ALIGNBYTES	7
#define	ALIGN(p)	(((u_long)(p) + ALIGNBYTES) &~ ALIGNBYTES)

#define	NBPG		8192		/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		13		/* LOG2(NBPG) */
#define	NPTEPG		(NBPG/(sizeof (struct pte)))

#define NBSEG		0x400000	/* bytes/segment */
#define	SEGOFSET	(NBSEG-1)	/* byte offset into segment */
#define	SEGSHIFT	22		/* LOG2(NBSEG) */

#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define	CLSIZE		1
#define	CLSIZELOG2	0

/* NOTE: SSIZE, SINCR and UPAGES must be multiples of CLSIZE */
#define	SSIZE		1		/* initial stack size/NBPG */
#define	SINCR		1		/* increment of stack/NBPG */

#define	UPAGES		1		/* pages of u-area */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		128		/* size of an mbuf */
#define	MCLBYTES	1024
#define	MCLSHIFT	10
#define	MCLOFSET	(MCLBYTES - 1)
#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	512		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	256		/* map size, max cluster allocation */
#endif
#endif

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages
 */ 
#ifndef NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(512*1024/CLBYTES)
#endif
/*
 * Some macros for units conversion
 */
/* Core clicks (NBPG bytes) to segments and vice versa */
#define	ctos(x)	(x)
#define	stoc(x)	(x)

/* pages ("clicks") (NBPG bytes) to disk blocks */
#define	ctod(x)	(((vm_offset_t)(x))<<(PGSHIFT-DEV_BSHIFT))
#define	dtoc(x)	(((vm_offset_t)(x))>>(PGSHIFT-DEV_BSHIFT))
#define	dtob(x)	(((vm_offset_t)(x))<<DEV_BSHIFT)

/* pages to bytes */
#define	ctob(x)	(((vm_offset_t)(x))<<PGSHIFT)

/* bytes to pages */
#define	btoc(x)	((((vm_offset_t)(x))+PGOFSET)>>PGSHIFT)

#define	btodb(bytes)	 		/* calculates (bytes / DEV_BSIZE) */ \
	(((vm_offset_t)(bytes)) >> DEV_BSHIFT)
#define	dbtob(db)			/* calculates (db * DEV_BSIZE) */ \
	(((vm_offset_t)(db)) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

/* LITES stuff */

/* stack grows to lower addresses */
#define	STACK_GROWS_UP	0

/*
 * Base address for system call emulator.
 */
#define	EMULATOR_BASE	(0x11c000000UL)
#define	EMULATOR_END	(0x120000000UL)
				/* 256 Kbytes */

/*
 * Base address for user mapped files
 */
#define MAP_FILE_BASE	((vm_address_t) 0x118000000)
				/* 64k*64files = 4 Meg */

/*
 * Alpha emulator sits above stack.
 */
#define	EMULATOR_ABOVE_STACK	1

#define TRAMPOLINE_MAX_SIZE	0x100

#define PAGE_SIZE ALPHA_PGBYTES

#endif /* _ALPHA_PARAM_H_ */
