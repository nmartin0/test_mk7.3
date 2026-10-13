/* 
 * Copyright (c) 1992, 1994, The University of Utah and
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
 * 	Utah $Hdr: vm_machdep.c 1.4 94/12/16$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/assert.h>
#include <sys/som.h>
#include <machine/cpu.h>

#include <vm/vm.h>

/*
 * Have to make sure the U-area we dump looks just like the Utah BSD kind.
 */
struct	bsd_pcb {
	char	pad0[0x28c-0x000];
	struct	bsd_exechdr pcb_exec;	/* BSD exec header */
	char	pad1[0x2e4-0x2c0];
	u_int	pcb_pgsz;		/* machine page size (bytes) */
	char	pad2[0x2f8-0x2e8];
};

struct	bsd_user {
	struct	bsd_pcb bsdu_pcb;
	char	pad1[0x3e8-0x2f8];
	size_t	bsdu_tsize;		/* text size (clicks) */
	size_t	bsdu_dsize;		/* data size (clicks) */
	size_t	bsdu_ssize;		/* stack size (clicks) */
};

/*
 *	fake_bsd_u:
 *
 *	Fake an BSD u-area structure for the specified thread.
 *	Only "interesting" fields are filled in.
 */
void
fake_bsd_u(bup, p, thread)
	register struct bsd_user *bup;
	struct proc		 *p;
	register thread_t	 thread;
{
	bzero((caddr_t) bup, sizeof(struct bsd_user));
	
	bup->bsdu_pcb.pcb_pgsz = NBPG;

#if notyet
	bup->bsdu_pcb.pcb_exec.a_tmem = (int)p->p_utask.uu_text_start;
	bup->bsdu_pcb.pcb_exec.a_dmem = (int)p->p_utask.uu_data_start;

	bup->bsdu_tsize = p->p_utask.uu_tsize;
	bup->bsdu_dsize = p->p_utask.uu_dsize;
	bup->bsdu_ssize = p->p_utask.uu_ssize;
#endif
}

/*
 * Dump the machine specific header information at the start of a core dump.
 */
mach_error_t
cpu_coredump(p, vp, cred)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
{
#if notyet
	vm_offset_t	 		stack_addr;
	vm_size_t	 		stack_size;
	int		 		error, offset;
	struct parisc_thread_state 	ts;
	unsigned int 			count;
	struct bsd_user			fake_uarea;

	/*
	 * Get the registers for the thread.
	 */
	count = PARISC_THREAD_STATE_COUNT;
	(void) thread_get_state(p->p_thread, PARISC_THREAD_STATE,
				(thread_state_t)&ts,
				&count);

	/*
	 * Fake the stack segment size from the current
	 * stack pointer value.
	 */
	stack_addr = USRSTACK;
	stack_size = round_page(ts.r30 - stack_addr + 4);
	p->p_utask.uu_ssize = btoc(stack_size);

	/*
	 * Dump a BSD style U-area (base of kernel stack).
	 */
	fake_bsd_u(&fake_uarea, p, p->p_thread);
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&fake_uarea, sizeof fake_uarea,
			(off_t)0, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT,
			cred, (int *)0, p);
	if (error)
		return (error);

	/*
	 * Dump saved state info (top of kernel stack). 
	 */
	offset = ctob(1),		/* XXX BSD offsets */
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&ts, sizeof ts, (off_t)offset,
			UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *)0, p);
	if (error)
		return (error);

	/*
	 * Write the data segment.
	 */
	offset += ctob(2);		/* XXX BSD offsets */
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)u.u_data_start,
			(int)ctob(u.u_dsize), (off_t)offset,
			UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *)0, p);
	if (error)
		return (error);

	/*
	 * Write the stack.
	 */
	offset += ctob(u.u_dsize);
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)stack_addr,
			(int)stack_size, (off_t)offset,
			UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *)0, p);
	return (error);
#else
	return EIO;
#endif
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of CLSIZE.
 */
void
pagemove(from, to, size)
	caddr_t from, to;
	int size;
{
#ifdef LITES
	kern_return_t kr;
	boolean_t dobcopy = FALSE;
	extern int donotfragmentbuf;

	/*
	 * Sometime we should physically copy:
	 *
	 * 1. Addresses not page aligned or size not page multiple.
	 *    Won't happen right now but will if we support FSes with
	 *    block sizes < 4K (e.g. ext2fs).
	 *
	 * 2. If we are avoiding fragmentation of the buffer cache VA space
	 *    (see block_io.c).
	 */
	if ((((int)from | (int)to | size) & PGOFSET)
	    || donotfragmentbuf)
		dobcopy = TRUE;

	/*
	 * XXX consider cost of vm_write vs. bcopying even for aligned data.
	 *
	 * Fastest way may depend on whether the destination is allocated
	 * or not; i.e., whether we take a fault or not.  If the destination
	 * isn't allocated it will probably almost always be better to
	 * vm_write than to take a fault on the destination.
	 *
	 * Pagemove is only used by the vfs_cluster code and only to
	 * contruct or break down clustered read/write buffers (the cases
	 * of shuffling excess memory around never happen in Lites).
	 *
	 * The problem is that for either read or write, a virtual copy
	 * of the data is left in the ``cluster buffer'' (which is actually
	 * the first component that was temporarily expanded to handle
	 * the other components) and a write to any of the other components
	 * will cause a fault and force a useless copy of the old data to
	 * the cluster buffer.  We could vm_deallocate followed by
	 * vm_allocate to avoid the copy, but that might be more expensive.
	 *
	 * NOTE: on an hp730 and quantum 425 disk we get about 1MB/sec
	 * through the filesystem regardless of whether we physially or
	 * virtually copy and regardless of whether we deallocate/reallocate.
	 * If clustering is disabled we get around 800KB-1MB/sec.
	 *
	 * XXX something else to think about.  Having buffers of 64K (or
	 * any multiple of 2) is going to have an affect on direct mapped
	 * virtually indexed cache machines.  Whether it is overall a good
	 * or bad effect, I haven't decided.  For example, if you have a
	 * 64K cache, every bsize chunk in the buffer cache is going to
	 * conflict in the cache.  This is bad for you if you are doing
	 * lots of FS accesses but may be good overall since less of the
	 * cache is eaten up by buffer cache activity.  Maybe try skewing
	 * buffers by PAGE_SIZE to see what happens.
	 */

	if (dobcopy) {
		bcopy(from, to, size);
		return;
	}
	kr = vm_write(mach_task_self(), (vm_offset_t) to,
		      (vm_offset_t) from, size);
	if (kr != KERN_SUCCESS)
		panic("pagemove: vm_write failed");
#if 0
	/*
	 * Current implementation assumes that full MAXBSIZE of all buffers
	 * is always allocated.  Hence, if we deallocate the old, we must
	 * reallocate in its place.
	 */
	kr = vm_deallocate(mach_task_self(), (vm_offset_t)from, size);
	if (kr != KERN_SUCCESS)
		panic("pagemove: vm_deallocate failed");
	kr = vm_allocate(mach_task_self(), (vm_address_t *)&from, size, FALSE);
	if (kr != KERN_SUCCESS)
		panic("pagemove: page reallocation failed");
#endif
#else
	register vm_offset_t pa;

	while (size > 0) {
		pa = pmap_extract(kernel_pmap, (vm_offset_t)from);
		pmap_remove(kernel_pmap,
			    (vm_offset_t)from, (vm_offset_t)from + PAGE_SIZE);
		pmap_enter(kernel_pmap,
			   (vm_offset_t)to, pa, VM_PROT_READ|VM_PROT_WRITE, 1);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
#endif
}
