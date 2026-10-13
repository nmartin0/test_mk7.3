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
 * $Log: vmparam.h,v $
 * Revision 1.1.1.1  1995/03/02  21:49:38  mike
 * Initial Lites release from hut.fi
 *
 */

#ifndef _PARISC_BSD_VMPARAM_H_
#define _PARISC_BSD_VMPARAM_H_

/*
 * Machine dependent constants for HP800
 */
/*
 * USRTEXT is the start of the user text/data space, while USRSTACK
 * is the top (end) of the user stack.  LOWPAGES and HIGHPAGES are
 * the number of pages from the beginning of the P0 region to the
 * beginning of the text and from the beginning of the P1 region to the
 * beginning of the stack respectively.
 */
#define	USRTEXT		UTEXTSEG
#define	USRSTACK	0x68FF3000		/* Start of user stack */
#define	BTOPUSRSTACK	btop(USRSTACK)		/* btop(USRSTACK) */
#define	P1PAGES		0
#define	LOWPAGES	0
#define	HIGHPAGES	UPAGES

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(16*1024*1024)		/* max text size */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(16*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(64*1024*1024)		/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(512*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		MAXDSIZ			/* max stack size */
#endif

/*
 * Default sizes of swap allocation chunks (see dmap.h).
 * The actual values may be changed in vminit() based on MAXDSIZ.
 * With MAXDSIZ of 64Mb and NDMAP of 62, dmmax will be 4096.
 * DMMIN should be at least ctod(1) so that vtod() works.
 * vminit() ensures this.
 */
#define	DMMIN	64			/* smallest swap allocation */
#define	DMMAX	4096			/* largest potential swap allocation */
#define	DMTEXT	4096			/* swap allocation for text */

/*
 * Sizes of the system and user portions of the system page table.
 */
/* SYSPTSIZE IS SILLY; IT SHOULD BE COMPUTED AT BOOT TIME */
#define	SYSPTSIZE	((1024*1024*64)/NBPG)	/* 64mb */
#define	USRPTSIZE	(8 * NPTEPG)		/* 32mb */

/*
 * PTEs for system V style shared memory.
 * This is basically slop for kmempt which we actually allocate (malloc) from.
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	((1024*1024*10)/NBPG)	/* 10mb */
#endif

/*
 * The size of the clock loop.
 */
#define	LOOPPAGES	(maxfree - firstfree)

/*
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time.  You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
#define	MAXSLP 		20

/*
 * A swapped in process is given a small amount of core without being bothered
 * by the page replacement algorithm.  Basically this says that if you are
 * swapped in you deserve some resources.  We protect the last SAFERSS
 * pages against paging and will just swap you out rather than paging you.
 * Note that each process has at least UPAGES+CLSIZE pages which are not
 * paged anyways (this is currently 8+2=10 pages or 5k bytes), so this
 * number just means a swapped in process is given around 25k bytes.
 * Just for fun: current memory prices are 4600$ a megabyte on VAX (4/22/81),
 * so we loan each swapped in process memory worth 100$, or just admit
 * that we don't consider it worthwhile and swap it out to disk which costs
 * $30/mb or about $0.75.
 */
#define	SAFERSS		(16384/NBPG)	/* nominal ``small'' resident set size
					   protected against replacement */

/*
 * DISKRPM is used to estimate the number of paging i/o operations
 * which one can expect from a single disk controller.
 */
#define	DISKRPM		60

/*
 * Klustering constants.  Klustering is the gathering
 * of pages together for pagein/pageout, while clustering
 * is the treatment of hardware page size as though it were
 * larger than it really is.
 *
 * KLMAX gives maximum cluster size in CLSIZE page (cluster-page)
 * units.  Note that ctod(KLMAX*CLSIZE) must be <= DMMIN in dmap.h.
 * ctob(KLMAX) should also be less than MAXPHYS (in vm_swp.c)
 * unless you like "big push" panics.
 */

#define	KLMAX	((16384/NBPG)/CLSIZE)
#define	KLSEQL	(( 8192/NBPG)/CLSIZE)	/* in klust if vadvise(VA_SEQL) */
#define	KLIN	((16384/NBPG)/CLSIZE)	/* default data/stack in klust */
#define	KLTXT	((16384/NBPG)/CLSIZE)	/* default text in klust */
#define	KLOUT	((16384/NBPG)/CLSIZE)

/*
 * KLSDIST is the advance or retard of the fifo reclaim for sequential
 * processes data space.
 */
#define	KLSDIST	3		/* klusters advance/retard for seq. fifo */

/*
 * Paging thresholds (see vm_sched.c).
 * Strategy of 1/19/85:
 *	lotsfree is 512k bytes, but at most 1/4 of memory
 *	desfree is 200k bytes, but at most 1/8 of memory
 *	minfree is 64k bytes, but at most 1/2 of desfree
 */
#define	VMLOTSFREE	(512 * 1024)
#define	VMLOTSFREEFRACT	4
#define	VMDESFREE	(200 * 1024)
#define	VMDESFREEFRACT	8
#define	VMMINFREE	(64 * 1024)
#define	VMMINFREEFRACT	2

/*
 * Paged text files that are less than PGTHRESH bytes (text + idata)
 * may be swapped in instead of paged in.
 */
#define PGTHRESH		(100 * 1024)

/*
 * There are two clock hands, initially separated by HANDSPREAD bytes
 * (but at most all of user memory).  The amount of time to reclaim
 * a page once the pageout process examines it increases with this
 * distance and decreases as the scan rate rises.
 */
#define	HANDSPREAD	(2 * 1024 * 1024)

/*
 * The number of times per second to recompute the desired paging rate
 * and poke the pagedaemon.
 */
#define	RATETOSCHEDPAGING	4

/* virtual sizes (bytes) for various kernel submaps */
#define VM_MBUF_SIZE		(NMBCLUSTERS*MCLBYTES)
#define VM_KMEM_SIZE		(NKMEMCLUSTERS*CLBYTES)
#define VM_PHYS_SIZE		(USRIOSIZE*CLBYTES)

/*
 * Determine how large a u area can become.
 */
#define	MAXNPROC	660		/* for MAXUSERS == 64 */
#define	UPTSIZE		(sizeof(struct user) * MAXNPROC / NBPG)

/*
 * Virtual address of kernel gateway page (for system calls).
 */
#define	GWPGADDR	0xC0000000

#endif	/* _PARISC_BSD_VMPARAM_H_ */
