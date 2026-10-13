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
 * $Log: param.h,v $
 * Revision 1.2  1995/03/10  17:18:27  mike
 * Changes to make it compile/link in the new build environment
 *
 * Revision 1.1.1.1  1995/03/02  21:49:38  mike
 * Initial Lites release from hut.fi
 *
 */

/*
 * Machine dependent constants for PA-RISC (1.1).
 */

#define MACHINE "parisc"

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_int and must be cast to
 * any desired pointer type.
 */
#define	ALIGNBYTES	3
#define	ALIGN(p)	(((u_int)(p) + ALIGNBYTES) &~ ALIGNBYTES)

#define	NBPG		4096		/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		12		/* LOG2(NBPG) */
#define	NPTEPG		(NBPG/(sizeof (struct pte)))

#define NBSEG		0x40000000	/* bytes/segment (quadrant) */
#define	SEGOFSET	(NBSEG-1)	/* byte offset into segment */
#define	SEGSHIFT	30		/* LOG2(NBSEG) */

#define	UTEXTSEG	0x00000000	/* user shared text segment */
#define	UDATASEG	0x40000000	/* user private data segment */
#define	KSTAKSEG	0x80000000	/* kernel stack segment (3rd quad) */
#define	USHRMSEG	0xc0000000	/* user shared memory segment */

#define	KERNSPC		0		/* kernel space id */
#define	KERNBASE	0x00000000	/* start of kernel virtual */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define	STACK_GROWS_UP	1		/* stack grows to higher addresses */

#define	CLSIZE		1
#define	CLSIZELOG2	0

/* NOTE: SSIZE, SINCR and UPAGES must be multiples of CLSIZE */
#define	SSIZE		(8192/NBPG)	/* initial stack size/NBPG */
#define	SINCR		(8192/NBPG)	/* increment of stack/NBPG */

#define	KSTAKSIZE	(8192/NBPG)	/* pages in kernel stack */
#define	USIZE		(4096/NBPG)	/* pages for user struct */
#define	UPAGES		(KSTAKSIZE+USIZE) /* pages of u-area */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		128		/* size of an mbuf */
#define	MCLBYTES	2048		/* large enough for ether MTU */
#define	MCLSHIFT	11
#define	MCLOFSET	(MCLBYTES - 1)
#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	((1024 * 1024) / MCLBYTES)	/* cl map size: 1MB */
#else
#define	NMBCLUSTERS	((1024 * 512) / MCLBYTES)	/* cl map size: 0.5MB */
#endif
#endif

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages
 */
#ifndef NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(1024*1024/CLBYTES)
#endif

/* pages ("clicks") (4096 bytes) to disk blocks */
#define	ctod(x)	((x)<<(PGSHIFT-DEV_BSHIFT))
#define	dtoc(x)	((x)>>(PGSHIFT-DEV_BSHIFT))
#define	dtob(x)	((x)<<DEV_BSHIFT)

/* pages to bytes */
#define	ctob(x)	((x)<<PGSHIFT)

/* bytes to pages */
#define	btoc(x)	(((unsigned)(x)+(NBPG-1))>>PGSHIFT)

#define LABELSECTOR	(1024/DEV_BSIZE)
#define LABELOFFSET	0

#define	btodb(bytes)	 		/* calculates (bytes / DEV_BSIZE) */ \
	((unsigned)(bytes) >> DEV_BSHIFT)
#define	dbtob(db)			/* calculates (db * DEV_BSIZE) */ \
	((unsigned)(db) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

/*
 * Macros to decode processor status word.
 */
#define	PC_PRIV_MASK	3
#define	PC_PRIV_KERN	0
#define	PC_PRIV_USER	3
#define	USERMODE(pc)	(((pc) & PC_PRIV_MASK) != PC_PRIV_KERN)

#define	DELAY(n)	{ register int N = (n); while (--N > 0); }

#ifdef HPUXCOMPAT
/*
 * Constants/macros for HPUX multiple mapping of user address space.
 * Pages in the first 256Mb are mapped in at every 256Mb segment.
 */
#define HPMMMASK	0xF0000000
#define ISHPMMADDR(v)	0		/* XXX ...jef */
#define HPMMBASEADDR(v)	((unsigned)(v) & ~HPMMMASK)
#endif

#define	NPIDS		16	/* maximum number of PIDs per process */
#define	NIOPIDS		8	/* maximum number of IO space PIDs */

/*
 * LITES stuff
 */

/*
 * Base address for U*X system call emulator.
 */
#define	EMULATOR_BASE	0x90100000
#define	EMULATOR_END	0x90200000	/* 1 Mbyte */

/*
 * Define base for user mapped files
 */
#define MAP_FILE_BASE	0x90300000

/*
 * PA-RISC stack sits below emulator.
 */
#define	EMULATOR_ABOVE_STACK	1

#define TRAMPOLINE_MAX_SIZE	0x100

#define PAGE_SIZE NBPG
