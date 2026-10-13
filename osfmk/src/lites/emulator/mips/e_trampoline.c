/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1994 Johannes Helander
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND JOHANNES HELANDER ALLOW FREE USE OF THIS
 * SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND JOHANNES
 * HELANDER DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/* 
 * HISTORY
 * $Log: e_trampoline.c,v $
 * Revision 1.1.1.2  1995/03/23  01:15:47  law
 * lites-950323 from jvh.
 *
 *
 */
/* 
 *	File:	emulator/mips/e_trampoline.c
 *	Author:	Johannes Helander, Helsinki University of Technology, 1994.
 *	Date:	May 1994
 *	Origin:	Derived from emulator/i386/e_trampoline.c
 *
 *	Handler for emulated BSD and Ultrix system calls.
 */

#define KERNEL
#include <sys/errno.h>
#undef KERNEL

#include <e_defs.h>
#include <syscall_table.h>
#include <mach/mips/mips_instruction.h>

binary_type_t e_my_binary_type = BT_BAD;

char *atbin_names[] =
{
	ATSYS_NAMES("mips_")
};

struct emul_regs {
	natural_t a0, a1, a2, a3; /* save area for emul_common */	
	natural_t gp;
	natural_t ra;
	natural_t pc;
	natural_t ret_a3;
	natural_t ret_v1;		/* return value only */
};

extern mips_instruction emul_vector_base[][3],
			emul_task_by_pid[][3],
			emul_pid_by_task[][3],
			emul_map_fd[][3],
			emul_init_process[][3],
			emul_htg_syscall[][3];

int	emul_low_entry = -9;
int	emul_high_entry = 257;

extern emul_trampoline();
extern void take_signals_and_do_sigreturn(struct emul_regs *);

int current_nsysent = 0;
struct sysent *current_sysent = 0;

void emul_setup(task_t task, enum binary_type binary_type)
{
	int i;
	kern_return_t	rc;

	for (i = emul_low_entry;
	     i <= emul_high_entry;
	     i++) {
		rc = task_set_emulation(task,
					(vm_address_t) emul_vector_base[i],
					i);
		if (rc != KERN_SUCCESS)
		    e_emulator_error("emul_setup: task_set_emulation: %s",
				     mach_error_string(rc));
	}
	rc = task_set_emulation(task,
			(vm_address_t) emul_task_by_pid,
			-33);
	rc = task_set_emulation(task,
			(vm_address_t) emul_pid_by_task,
			-34);
	rc = task_set_emulation(task,
			(vm_address_t) emul_init_process,
			-41);
	rc = task_set_emulation(task,
			(vm_offset_t) emul_map_fd,
			-43);
	rc = task_set_emulation(task,
			(vm_address_t) emul_htg_syscall,
			-52);

	switch (binary_type) {
	      case BT_MIPSEL:
	      default:
		current_nsysent = e_ultrix_nsysent;
		current_sysent = e_ultrix_sysent;
		break;
	}
}

errno_t e_htg_syscall()
{
	return e_mach_error_to_errno(LITES_ENOSYS);
}

/*
 * System calls enter here.
 */
natural_t emul_syscall(int syscode, struct emul_regs *regs, int *args)
{
	int error;
	struct sysent *callp;
	int rval[2];
	boolean_t interrupt;
	int ret_v0 = 0;

restart_system_call:

	interrupt = FALSE;
	error = 0;

#if defined(MAP_UAREA) && 0
	if (shared_enabled) {
		if (shared_base_ro->us_cursig) {
			error = ERESTART;
			goto signal;
		}
	}
#endif	MAP_UAREA

 	/*
 	 * Check for indirect system call.
 	 */
 	switch (syscode) {
	      case 0 /* SYS_syscall */:
		  syscode = *args++;
		  break;
		case 198 /* SYS___syscall */:
		    /* code is a quad argument with the high order bits 0.
		     * Endian dependent XXX
		     */
		    syscode = *args;
		  args += 2;
		  break;
	  }

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
/*
	    else if (syscode < -ncmusysent)
		callp = &null_sysent;
	    else
		callp = &cmusysent[-syscode];
*/
	}

	if (syscall_debug > 2)
	    e_emulator_error("emul_syscall[%d] %s(x%x, x%x, x%x, x%x, x%x, x%x, x%x, x%x)",
			     syscode, callp->name, args[0], args[1], args[2],
			     args[3], args[4], args[5], args[6], args[7]);
	/*
	 * Set up the initial return values.
	 */
	rval[0] = 0;
	rval[1] = regs->ret_v1;

	/*
	 * Call the routine, passing arguments according to the table
	 * entry.
	 */
	switch (callp->nargs) {
	    case 0:
		error = (*callp->routine)(rval);
		break;
	    case 1:
		error = (*callp->routine)(args[0], rval);
		break;
	    case 2:
		error = (*callp->routine)(
				args[0], args[1],
				rval);
		break;
	    case 3:
		error = (*callp->routine)(
				args[0], args[1], args[2],
				rval);
		break;
	    case 4:
		error = (*callp->routine)(
				args[0], args[1], args[2], args[3],
				rval);
		break;
	    case 5:
		error = (*callp->routine)(
				args[0], args[1], args[2], args[3], args[4],
				rval);
		break;
	    case 6:
		error = (*callp->routine)(
				args[0], args[1], args[2],
				args[3], args[4], args[5],
				rval);
		break;
	    case 7:
		error = (*callp->routine)(
				args[0], args[1], args[2],
				args[3], args[4], args[5], args[6],
				rval);
		break;
	    case 8:
		error = (*callp->routine)(
				args[0], args[1], args[2],
				args[3], args[4], args[5], args[6],
				args[7],
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

	switch (error) {
	    case EJUSTRETURN:
		if (syscall_debug > 1)
		    e_emulator_error("JUSTRETURN[%d] %s", syscode,callp->name);
		/* Do not alter registers */
		break;

	    case 0:
		/* Success */
		ret_v0 = rval[0];
		regs->ret_v1 = rval[1];
		regs->ret_a3 = 0;

		if (syscall_debug > 2)
		    e_emulator_error("return[%d] %s = (x%x x%x)",
				     syscode, callp->name, rval[0], rval[1]);
		break;

	    case ERESTART:
		if (syscall_debug > 1)
		    e_emulator_error("RESTART[%d] %s", syscode, callp->name);
		/* restart call */
		take_signals_and_do_sigreturn(regs);
		goto restart_system_call;
		break;

	    default:
		/* error */
		ret_v0 = error;
	        regs->ret_a3 = 1;
		if (syscall_debug > 1)
		    e_emulator_error("err_return[%d] %s -> %d",
				     syscode, callp->name, error);
		break;
	}

	/*
	 * Handle interrupt request
	 */
	e_checksignals(&interrupt);

	if (error == ERESTART || error == EINTR || interrupt)
	    take_signals_and_do_sigreturn(regs);
	return ret_v0;
}

/* The stack points at argc. */
boolean_t with_kframe = TRUE;

noreturn emul_exec_start(
	int			*kframe,
	enum binary_type	binary_type,
	int			argc,
	char			*argv[],
	char			*envp[],
	struct exec_load_info	*li)
{
	struct mips_thread_state state;
	int x;

	bzero((void *) &state, sizeof(state));

	state.pc = li->pc;
	state.r28 = li->gp;	/* gp */
	state.r29 = (unsigned int) kframe;
	state.r30 = 0;		/* fp */
	state.r31 = 0;		/* ra */

	if (syscall_debug > 1)
	    e_emulator_error("emul_exec_start: starting at x%x k=x%x (x%x x%x x%x x%x)",
			     li->pc, kframe, kframe[0], kframe[1],
			     kframe[2], kframe[3]);
	emul_load_state(&state);
	/*NOTREACHED*/
}


#if defined(COMPAT_43) && 0
/* 
 * Compatibility with historical brain damage.
 * Fortunately new code doesn't use sigvec so it is not necessary to
 * be compatible with that simultaneously.
 */
int OLD_mips_e_sigvec(
	int			*argp,
	int			*rval,
	struct emul_regs	*regs)
{
	sigvec_t nsv, spare;
	sigvec_t *osv = (sigvec_t *) argp[2];
	/* if osv is null assume caller was not interested in it */
	if (!osv)
	    osv = &spare;
	if (argp[1])
	    nsv = *(sigvec_t *) argp[1];

	return bsd_osigvec(our_bsd_server_port,
			   argp[0],			/* sig */
			   !! (boolean_t) argp[1],	/* nsv is valid? */
			   nsv,
			   osv,
			   argp[3] & ~0x80000000); /* trampoline */
}

int mips_e_sigvec(
	int			*argp,
	int			*rval,
	struct emul_regs	*regs)
{
	return e_bnr_sigvec((int) argp[0],
			    (struct sigvec *) argp[1],	/* nsv */
			    (struct sigvec *) argp[2],	/* osv */
			    (vm_address_t) argp[3] & ~0x80000000); /* tramp */
}
#endif /* COMPAT_43 */

errno_t e_machine_fork(
	int			*argp,
	int			*rval,
	struct emul_regs	*regs)
{
	errno_t err;

	err = e_fork((pid_t *) rval);
	if (err)
	    return err;
	if (*rval)
	    rval[1] = 0;
	else
	    rval[1] = 1;
	return ESUCCESS;
}

errno_t e_machine_vfork(
	int			*argp,
	int			*rval,
	struct emul_regs	*regs)
{
	errno_t err;

	err = e_vfork((pid_t *) rval);
	if (err)
	    return err;
	if (*rval)
	    rval[1] = 0;	/* parent edx = 0 */
	else
	    rval[1] = 1;	/* child edx = 1 */
	return ESUCCESS;
}


errno_t
e_fork_call(boolean_t isvfork, pid_t *pid, boolean_t *ischild)
{
	struct mips_thread_state	state;
	
	int rv[2];
	errno_t error;

	volatile int x;

	/*
	 * Set up registers for child.  It resumes on its own stack.
	 */

	/* Prevent caching of caller saved registers. They are destroyed */
	asm volatile("");
	x = emul_save_state(&state);
	asm volatile("");
	if (x != 0) {
		*ischild = TRUE;
		*pid = x;
		child_init();
		if (syscall_debug > 1)
		    e_emulator_error("CHILD parent=%d", x);
		return 0;
	}

	state.r2 = 1;	/* v0 */
	
	/* FP regs!!!! */

	/*
	 * Create the child.
	 */
	error = (isvfork?bsd_vfork:bsd_fork)
	    (our_bsd_server_port,
	     (int *)&state,
	     MIPS_THREAD_STATE_COUNT,
	     rv);

	if (error == 0)
	    *pid = rv[0];

	*ischild = FALSE;
	return error;
}

void do_signal_notify() {}

void e_enable_asynch_signals() {}

void e_signal_child_setup() {}

/*
 * Wait has a weird parameter passing mechanism.
 */
errno_t
e_machine_wait(
	int			*argp,
	int			*rval,
	struct emul_regs	*regs)
{
	errno_t err;
	short wpid;
	int istat;

#define WAIT_ANY (-1)

	err = e_bnr_wait4(WAIT_ANY, &istat, argp[1],
			  (struct rusage *) argp[2], &wpid);
	if (!err && argp[0])
	    *((int *) argp[0]) = istat;	/* huh */
	rval[0] = wpid;
	return err;
}

/* SIGNALS */

/* Take a signal */
boolean_t take_a_signal(
	struct mips_thread_state *state,
	struct emul_regs *regs)	/* user gp is here */
{
	int	old_mask, old_onstack, sig, code, new_sp;
	int error;
	void (*handler)();
	/*
	 * Get the signal to take from the server.  It also
	 * switches the signal mask and the stack, so we must -- XXX Not stack
	 * be off the old user stack before calling it.
	 */
	error = bsd_take_signal(our_bsd_server_port,
				&old_mask,
				&old_onstack,
				&sig,
				&code,
				(int *) &handler,
				&new_sp);

	/*
	 * If there really were no signals to take, return.
	 */
	if (error || sig == 0)
	    return FALSE;

	if (syscall_debug > 1)
	    e_emulator_error("SIG%d x%x", sig, handler);

	/*
	 * Put the signal context and signal frame on the signal stack.
	 */
	if (new_sp != 0) {
		e_emulator_error("take_signals: stack switch needed");
	    /*
	     * Build signal frame and context on user's stack.
	     */
	}
	/*
	 * Build the signal context to be used by sigreturn.
	 */

	switch(e_my_binary_type) {
	      default:
		{
			struct sigcontext sc;
			natural_t user_gp = regs->gp;

			sc.sc_onstack = old_onstack;
			sc.sc_mask = old_mask;
			sc.sc_pc = state->pc;
			
			sc.sc_regs[0] = 0; /* zero */
			bcopy(state, &sc.sc_regs[1], sizeof(*state));
			sc.sc_fpused = 0; /* XXX */

			/* Then just call it. After fixing gp! */
			asm volatile ("move $28, %0" : : "r" (user_gp));
			(*handler)(sig, code, &sc);
			asm volatile ("la $28, _gp");

			bcopy(&sc.sc_regs[1], state, sizeof(*state));
			state->pc = sc.sc_pc;
		}
	}

	if (syscall_debug > 2)
	    e_emulator_error("return_SIG%d", sig);

	/* Re-enable signal */
	error = bsd_sigreturn(our_bsd_server_port,
			      old_onstack & 01,
			      old_mask);
	return TRUE;
}

/* 
 * Take some signals. Called after system calls if needed.
 *
 * XXX This signal code does not care about sigstacks and similar
 * XXX messy stuff. However, it works in most simple cases.
 */
void take_signals_and_do_sigreturn(struct emul_regs *regs)
{
	struct mips_thread_state state;
	volatile int x;

	x = emul_save_state(&state);

	/* 
	 * Check for longjmp or sigreturn out of handler.
	 */
	if (x != 0)
	    return;

	while (take_a_signal(&state, regs))
	    ;

	/* Now return where requested or from this function if unchanged */
	if ((state.r2 == 0)	/* v0 */
	    && (state.pc >= EMULATOR_BASE)
	    && (state.pc < EMULATOR_END))
	{
		state.r2 = 1;	/* for setjmp test above (avoid loop) */
	}
	emul_load_state(&state);
	/*NOTREACHED*/
}

errno_t e_bsd_sigreturn(struct sigcontext *sc)
{
	struct mips_thread_state state;

	emul_save_state(&state);

	bcopy(&sc->sc_regs[1], &state, sizeof(state));
	state.pc = sc->sc_pc;
	if ((state.r2 == 0)
	    && (state.pc > EMULATOR_BASE)
	    && (state.pc < EMULATOR_END))
	{
		state.r2 = 1;	/* for setjmp test above (avoid loop) */
	}

	if (syscall_debug > 2)
	    e_emulator_error("bsd_SIGRETURN");

	/* Re-enable signal */
	bsd_sigreturn(our_bsd_server_port,
		      sc->sc_onstack & 01,
		      sc->sc_mask);
	emul_load_state(&state);
	/*NOTREACHED*/
	(noreturn) emul_panic("e_bsd_sigreturn: _longjmp returned!");
	return 0;	/* silence bogus warning */
}
