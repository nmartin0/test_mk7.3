/* 
 * Mach Operating System
 * Copyright (c) 1994 Johannes Helander
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JOHANNES HELANDER ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  JOHANNES HELANDER DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/*
 * HISTORY
 * $Log: e_linux_trampoline.c,v $
 * Revision 1.1.1.2  1995/03/23  01:15:40  law
 * lites-950323 from jvh.
 *
 */
/* 
 *	File:	emulator/i386/e_linux_trampoline.c
 *	Author:	Johannes Helander, Helsinki University of Technology, 1994.
 *	Date:	May 1994
 *
 *	Linux binary emulation.
 */

#define KERNEL
#include <sys/errno.h>
#undef KERNEL
#include <e_defs.h>
#include <syscall_table.h>
#include <i386/eflags.h>

#define	DEBUG 0

extern int syscall_debug;

extern emul_linux_trampoline();
extern void e_exception_thread(void);
void e_linux_setup(task_t, enum binary_type);

extern void (*emul_child_init_func)();

void linux_child_init()
{
	e_linux_setup(mach_task_self(), e_my_binary_type);
}

extern int _linux_libc_trampoline_code();
extern vm_size_t _linux_libc_trampoline_size;
extern int emul_linux_trampoline_2();

void e_linux_setup(task_t task, enum binary_type binary_type)
{
	vm_offset_t addr;
	vm_offset_t *vector;
	kern_return_t kr;

	/* Setup trampoline for patched libc */
	addr = 0xa0100000; /* See linux-libc/sysdeps/linux/i386/sysdep.h */
	vm_allocate(mach_task_self(), &addr, 0x1000, FALSE);
	/* vm_allocate fails after fork. Doesn't matter at all */
	vector = (vm_offset_t *) addr;
	*vector = (vm_offset_t) emul_linux_trampoline_2;

	emul_child_init_func = linux_child_init;

	current_nsysent = e_linux_nsysent;
	current_sysent = e_linux_sysent;

	e_exception_setup();
	e_fork_a_thread(e_exception_thread);
}

int (*func)() = (int (*)()) -1;	/* XXX make local below but now for debugging*/

noreturn emul_exec_linux_start(
	int			*kframe,
	enum binary_type	binary_type,
	int			argc,
	char			*argv[],
	char			*envp[],
	struct exec_load_info	*li)
{
	struct i386_thread_state state;
	int x;

	if (x = emul_save_state(&state))
	    exit(x);

	state.eip = li->pc;
	state.ebx = 0;
	state.ebp = 0;
	state.esi = 0;
	state.edi = 0;

	/* 
	 * Linux has a slightly different kframe.
	 * The start function should be called with sp
	 * pointing to argc. above that argp and envp.
	 *
	 * The BSD/SYSV frame has sp pointing to argc with argv0 arv1
	 * ... argvN 0 envv0 ...
	 *
	 * What I can't see is why neither could have been a normal
	 * function call frame!
	 *
	 * Pushing the extra arguments is safe even if we are running
	 * on the same stack as a few extra words were allocated by
	 * the server when it left room for argv etc.
	 */

	/* Replace argc with envp */
	*kframe = (int) envp;

	/* Push pointer to argv start. */
	kframe--;		/* 4 bytes. it's an int pointer */
	*kframe = (int) argv;

	/* And argc */
	kframe--;
	*kframe = argc;

	/* Unwind stack */
	state.uesp = (int) kframe;

	if (syscall_debug > 1)
	    e_emulator_error("emul_exec_linux_start: starting at x%x k=x%x (x%x x%x x%x x%x)",
			     li->pc, kframe, kframe[0], kframe[1],
			     kframe[2], kframe[3]);
	(noreturn) emul_load_state(&state);
	/*NOTREACHED*/
}

struct emul_linux_slow_regs {
	int	fp;
	int	edi;
	int	esi;
	int	ebx;

	int	edx;
	int	ecx;
	int	eax;
	int	pc;
};

struct emul_linux_regs {
	int	fp;
	int	edi;
	int	esi;
	int	ebx;

	int	edx;
	int	ecx;
	int	pc;
	int	eax;
};

extern void emul_linux_syscall(struct emul_linux_regs *regs);

/* From int 0x80 through fixup thread */
void emul_linux_slow_syscall(struct emul_linux_slow_regs *sr)
{
	struct emul_linux_regs r;

	r.fp = sr->fp;
	r.edi = sr->edi;
	r.esi = sr->esi;
	r.ebx = sr->ebx;
	r.edx = sr->edx;
	r.ecx = sr->ecx;
	r.pc = sr->pc;
	r.eax = sr->eax;

	emul_linux_syscall(&r);

	sr->fp = r.fp;
	sr->edi = r.edi;
	sr->esi = r.esi;
	sr->ebx = r.ebx;
	sr->edx = r.edx;
	sr->ecx = r.ecx;
	sr->pc = r.pc;
	sr->eax = r.eax;
}

/*
 * System calls enter here.
 */
void emul_linux_syscall(struct emul_linux_regs *regs)
{
	int syscode;
	int error;
	struct sysent *callp;
	int rval[2];
	boolean_t interrupt;
	int *args;

restart_system_call:
	interrupt = FALSE;
	error = 0;

	args = (int *)(regs+1);	/* args on stack top */
	args++;	/* point to first argument - skip return address */

	syscode = regs->eax;

	/*
	 * Find system call table entry for the system call.
	 */
	if (syscode >= current_nsysent)
	    callp = &null_sysent;
	else if (syscode >= 0)
	    callp = &current_sysent[syscode];
	else {
	    /*
	     * Negative system call numbers are CMU extensions.
	     */
	    if (syscode == -33)
		callp = &sysent_task_by_pid;
	    else if (syscode == -6)
		callp = &e_bsd_sysent[SYS_table];
	    else if (syscode == -34)
		callp = &sysent_pid_by_task;
	    else if (syscode == -41)
		callp = &sysent_init_process;
	    else if (syscode == -59)
		callp = &sysent_htg_ux_syscall;
	    else
		callp = &null_sysent;
	}

	if (syscall_debug > 2)
	    e_emulator_error("emul_syscall[%d] %s(x%x, x%x, x%x, x%x, x%x)",
			     syscode, callp->name, regs->ebx, regs->ecx,
			     regs->edx, regs->esi, regs->edi);
	/*
	 * Set up the initial return values.
	 */
	rval[0] = 0;
	rval[1] = regs->edx;

	/*
	 * Call the routine, passing arguments according to the table
	 * entry.
	 */
	switch (callp->nargs) {
	    case 0:
		error = (*callp->routine)(rval);
		break;
	    case 1:
		error = (*callp->routine)(regs->ebx, rval);
		break;
	    case 2:
		error = (*callp->routine)(
				regs->ebx, regs->ecx,
				rval);
		break;
	    case 3:
		error = (*callp->routine)(
				regs->ebx, regs->ecx, regs->edx,
				rval);
		break;
	    case 4:
		error = (*callp->routine)(
				regs->ebx, regs->ecx, regs->edx,
				regs->esi,
				rval);
		break;
	    case 5:
		error = (*callp->routine)(
				regs->ebx, regs->ecx, regs->edx,
				regs->esi, regs->edi,
				rval);
		break;

	    case -1:	/* generic */
		error = (*callp->routine)(
				syscode,
				args,
				rval);
		break;

	    case -2:	/* pass registers to modify */
		error = (*callp->routine)(
				args,
				rval,
				regs);
		break;
	}

	/*
	 * Set up return values.
	 */

#ifdef	MAP_UAREA
signal:
#endif	MAP_UAREA

	switch (error) {
	    case EJUSTRETURN:
		if (syscall_debug > 1)
		    e_emulator_error("JUSTRETURN[%d] %s", syscode, callp->name);
		/* Do not alter registers */
		break;

	    case 0:
		/* Success */
		regs->eax = rval[0];
/*		regs->edx = rval[1]; */
		if (syscall_debug > 2)
		    e_emulator_error("return[%d] %s = x%x",
				     syscode, callp->name, rval[0]);
		break;

	    case ERESTART:
		if (syscall_debug > 1)
		    e_emulator_error("RESTART[%d] %s", syscode, callp->name);
		/* restart call */
		/* 
		 * Since there are two routes into the server it is
		 * not possible to just fix the program counter.
		 * Instead a simple goto will do. (But do signals first).
		 */
		/* regs->pc -= 2; */
		take_signals_and_do_sigreturn();
		goto restart_system_call;
		break;

	    default:
		/* error */
		regs->eax = -error; /* negated errno */
		if (syscall_debug > 1)
		    e_emulator_error("err_return[%d] %s(x%x, x%x, x%x, x%x) -> %d",
				     syscode, callp->name,
				     regs->ebx, regs->ecx, regs->edx, regs->esi,
				     error);
		break;
	}

	/*
	 * Handle interrupt request
	 */
	e_checksignals(&interrupt);

	if (error == ERESTART || error == EINTR || interrupt)
	    take_signals_and_do_sigreturn();
}
