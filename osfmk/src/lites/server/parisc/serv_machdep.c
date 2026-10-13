/* 
 * Copyright (c) 1991,1992,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
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
 *	Utah $Hdr: serv_machdep.c 1.16 94/12/14$
 */
/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */

/*
 *	Machine-dependent routines evicted from rest of BSD
 *	files.
 */

#include <serv/server_defs.h>

#if 0
#include <machine/psl.h>
#include <machine/reg.h>
#endif
#include <machine/regalias.h>
#include <machine/vmparam.h>

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/exec.h>
#include <sys/systm.h>		/* boothowto */
#include <sys/parallel.h>
#include <sys/syscall.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/exec_file.h>

get_base_pointer()
{
	register int dp;

	asm("copy	%%r27,%0" : "=r" (dp));
	return dp;
}

void set_arg_addr(arg_size,
	     arg_start_p,
	     arg_size_p)
	u_int		arg_size;
	vm_offset_t	*arg_start_p;
	vm_size_t	*arg_size_p;
{
	/*
	 * Round arg size to quadwords.
	 * Note that hpux and BSD normally add a word's padding
	 * after the argument strings.
	 */
	arg_size = (arg_size + NBPW + (2*NBPW) - 1) & ~((2*NBPW) - 1);

	/*
	 * Put argument list at bottom of stack.
	 */
	*arg_start_p = USRSTACK;
	*arg_size_p = arg_size;
}

void set_entry_address(ep, entry, entry_count)
	struct exec	*ep;
	int		*entry;		/* pointer to OUT array */
	unsigned int	*entry_count;	/* out */
{
	entry[0] = ep->ahdr.entry;
	*entry_count = 3;
}

/*
 * Set up the process' registers to start the emulator - duplicate
 * what execve() in the emulator would do.
 */
void set_emulator_state(
	struct proc	*p,
        struct exec_load_info *li_data)
{
	struct parisc_thread_state	ts;
	unsigned int			count;

	bzero((char *)&ts, sizeof(ts));
	count = PARISC_THREAD_STATE_COUNT;
	(void) thread_get_state(p->p_thread,
				PARISC_THREAD_STATE,
				(thread_state_t)&ts, &count);

	/* Pass the zero count and zero_start on the stack.  */
	vm_write(p->p_task, li_data->sp-4, &li_data->zero_count, 4);
	vm_write(p->p_task, li_data->sp-8, &li_data->zero_start, 4);
	ts.iioq_head = li_data->pc;
	ts.iioq_tail = ts.iioq_head + 4;
	ts.r31 = li_data->pc | 3;
	ts.sr0 = ts.sr4;
	ts.sp = (int)li_data->sp + 64;

	count = thread_set_state(p->p_thread,
				 PARISC_THREAD_STATE,
				 (thread_state_t)&ts,
				 PARISC_THREAD_STATE_COUNT);
	if (count)
		printf("thread_set_state failed (%x)\n", count);
}

/*
 * Do weird things to the first process' address space before loading
 * the emulator.
 */
void
emul_init_process(p)
	struct proc	*p;
{
	/*
	 * Clear out the entire address space.
	 */
	(void) vm_deallocate(p->p_task,
			VM_MIN_ADDRESS,
			(vm_size_t)(VM_MAX_ADDRESS - VM_MIN_ADDRESS));
}

sigreturn()
{
	panic("sigreturn - MiG interface");
}

/*
 * Clone the parent's registers into the child thread for fork.
 */
boolean_t
thread_dup(child_thread, new_state, new_state_count, parent_pid, rc)
	thread_t	child_thread;
	thread_state_t	new_state;
	unsigned int	new_state_count;
	int		parent_pid, rc;
{
	struct parisc_thread_state *regs =
		(struct parisc_thread_state *)new_state;

	if (new_state_count != PARISC_THREAD_STATE_COUNT)
	    return (FALSE);

	regs->ret0 = parent_pid;
	regs->ret1 = rc;

	(void) thread_set_state(child_thread, PARISC_THREAD_STATE,
				new_state, new_state_count);
	return (TRUE);
}
