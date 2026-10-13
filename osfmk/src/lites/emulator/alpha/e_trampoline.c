/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1995 Johannes Helander
 * Copyright (c) 1995 Jukka Virtanen
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND JOHANNES HELANDER AND JUKKA VIRTANEN ALLOW FREE USE
 * OF THIS SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND JOHANNES
 * HELANDER AND JUKKA VIRTANEN DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY
 * DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/* 
 * HISTORY
 * $Log: e_trampoline.c,v $
 * Revision 1.1.1.2  1995/03/23  01:15:46  law
 * lites-950323 from jvh.
 *
 *
 */
/* 
 *	File:	 emulator/alpha/e_trampoline.c
 *	Authors: Johannes Helander, Jukka Virtanen
 *	Date:	 Nov 1994
 *	Origin:  Partially based on CMU's old emulator.
 *
 *	Handler for emulated BSD and OSF system calls.
 */

#define KERNEL
#include <sys/errno.h>
#undef KERNEL

#include <e_defs.h>
#include <syscall_table.h>
#include <mach/alpha/alpha_instruction.h>

binary_type_t e_my_binary_type = BT_BAD;

char *atbin_names[] =
{
	ATSYS_NAMES("alpha_")
};

/*
 * User's registers are all stored on the user's stack
 * in an exception frame that is just a sigcontext structure.
 * But we know what follows on the user's stack.
 *
 * The emulator's stack only contains a pointer to the
 * exception frame, and argument save space as required.
 */
struct emul_regs {
        natural_t    onstack;        /* see sigcontext */
        natural_t    mask;
        natural_t    pc;             /* program counter */
        natural_t    ps;             /* program status word */
        natural_t    v0;             /* return value 0 */
        natural_t    t0;             /* caller saved 0 */
        natural_t    t1;             /* caller saved 1 */
        natural_t    t2;             /* caller saved 2 */
        natural_t    t3;             /* caller saved 3 */
        natural_t    t4;             /* caller saved 4 */
        natural_t    t5;             /* caller saved 5 */
        natural_t    t6;             /* caller saved 6 */
        natural_t    t7;             /* caller saved 7 */
        natural_t    s0;             /* callee saved 0 */
        natural_t    s1;             /* callee saved 1 */
        natural_t    s2;             /* callee saved 2 */
        natural_t    s3;             /* callee saved 3 */
        natural_t    s4;             /* callee saved 4 */
        natural_t    s5;             /* callee saved 5 */
        natural_t    s6;             /* callee saved 6 */
        natural_t    a0;             /* arg register 0 */
        natural_t    a1;             /* arg register 1 */
        natural_t    a2;             /* arg register 2 */
        natural_t    a3;             /* arg register 3 */
        natural_t    a4;             /* arg register 4 */
        natural_t    a5;             /* arg register 5 */
        natural_t    t8;             /* caller saved 8 */
        natural_t    t9;             /* caller saved 9 */
        natural_t    t10;            /* caller saved 10 */
        natural_t    t11;            /* caller saved 11 */
        natural_t    ra;             /* return address */
        natural_t    t12;            /* caller saved 12 */
        natural_t    at;             /* assembler temporary */
        natural_t    gp;             /* global pointer */
        natural_t    sp;             /* stack pointer */
        natural_t    zero;           /* wired zero */

        natural_t    use_fpa;        /* did we use the fpa */
        natural_t    fpa_regs[32];   /* gp fpa regs */

        /* unsigned stack_args[0..5] */
};

/* Each emulation vector element is 4 instructions */
extern alpha_instruction emul_vector_base[][4],
			 emul_task_by_pid[][4],
			 emul_pid_by_task[][4],
			 emul_map_fd[][4],
			 emul_init_process[][4],
			 emul_htg_syscall[][4];

/* The vector really is -10..269, see emul_vector.s */
int	emul_low_entry = -9;
int	emul_high_entry = 260;

extern emul_trampoline();

void take_signals_and_do_sigreturn(struct emul_regs *regs);

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
			(vm_address_t) emul_htg_syscall,
			-59);
#if 0
	/* ALPHA specific */
        rc = task_set_emulation(task,
                        emul_fork,
                        2);             /* fork */
        rc = task_set_emulation(task,
                        emul_fork,
                        66);            /* vfork */
        rc = task_set_emulation(task,
                        emul_indirect,
                        0);             /* COULD be a fork */

#endif

	switch (binary_type) {
	      case BT_ALPHAOSF1:
		current_nsysent = e_osf1_nsysent;
		current_sysent  = e_osf1_sysent;
		break;
	      default:
		current_nsysent = e_bsd_nsysent;
		current_sysent  = e_bsd_sysent;
		break;
	}
}

/*
 * System calls enter here.
 */
void emul_syscall(struct emul_regs *regs)
{
	int syscode;
	int error;
	struct sysent *callp;
	natural_t rval[2];
	boolean_t interrupt;
	natural_t *args;

restart_system_call:

	interrupt = FALSE;
	error = 0;

	args = (natural_t *)(regs+1);	/* args on stack top */
	args++;	/* point to first argument - skip return address */

	/* System call number in v0 */
	syscode = regs->v0;

#if defined(MAP_UAREA) && 0
	if (shared_enabled) {
		if (shared_base_ro->us_cursig) {
			error = ERESTART;
			goto signal;
		}
	}
#endif	MAP_UAREA

	/*
	 *	Store on stack args that assembly code did not
	 *	[This is unfortunate, but needed for indirects]
	 */
	args[0] = regs->a0;
	args[1] = regs->a1;
	args[2] = regs->a2;
	args[3] = regs->a3;
	args[4] = regs->a4;
	args[5] = regs->a5;

 	/*
 	 * Check for indirect system call.
 	 */
 	switch (syscode) {
	      case 0 /* SYS_syscall */:
		  syscode = *args++;
		  break;
#if 0
	      case 198 /* SYS___syscall */:
		/* code is a quad argument with the high order bits 0.
		 * Endian dependent XXX
		 *
		 * jtv@hut.fi: What is this???
		 */
		syscode = *args;
		args += 2;
		break;
#endif
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
#if 0	/* jtv@hut.fi; XXXX */
	    else if (syscode == -6)
		callp = &e_bsd_sysent[SYS_table];
#endif
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
	    e_emulator_error("emul_syscall[%d] %s(x%lx, x%lx, x%lx, x%lx, x%lx, x%lx, x%lx, x%lx)",
			     syscode, callp->name, args[0], args[1], args[2],
			     args[3], args[4], args[5], args[6], args[7]);
	/*
	 * Set up the initial return values.
	 */
	rval[0] = 0;
	rval[1] = regs->a4;

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
		regs->v0 = rval[0];
		regs->a4 = rval[1]; /* $a4 is needed by fork */
		regs->a3 = 0;
		if (syscall_debug > 2)
		    e_emulator_error("return[%d] %s = (x%lx x%lx)",
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
		regs->v0 = error;
		regs->a3 = 1;
		if (syscall_debug > ((syscode == 6 /* close */) ? 2 : 1))
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
}

/* The stack points at argc. */
boolean_t with_kframe = TRUE;

noreturn emul_exec_start(
	integer_t		*kframe,
	enum binary_type	binary_type,
	int			argc,
	char			*argv[],
	char			*envp[],
	struct exec_load_info	*li)
{
	struct alpha_thread_state state;

	bzero((void *) &state, sizeof(state));

	state.pc  = li->pc;		/* pc */
	state.r30 = (natural_t) kframe; /* sp */

	if (syscall_debug > 1)
	    e_emulator_error("emul_exec_start: starting at x%lx k=x%lx (x%lx x%lx x%lx x%lx x%lx)",
			     li->pc, kframe, kframe[0], kframe[1],
			     kframe[2], kframe[3], kframe[4]);

	emul_load_state(&state);
	/*NOTREACHED*/
}

#if defined(COMPAT_43) && 0
/* 
 * Compatibility with historical brain damage.
 * Fortunately new code doesn't use sigvec so it is not necessary to
 * be compatible with that simultaneously.
 */
int alpha_e_sigvec(
	int			*argp,
	natural_t		*rval,
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
#endif /* COMPAT_43 */

errno_t e_htg_syscall()
{
	return e_mach_error_to_errno(LITES_ENOSYS);
}

errno_t e_machine_fork(
	int			*argp,
	natural_t		*rval,
	struct emul_regs	*regs)
{
	errno_t err;

	err = e_fork((pid_t *) rval);
	if (err)
	    return err;

	/* if a4 != 0 then fork returns 0 (child) */
	if (*rval)	/* parent */
	    rval[1] = 0;
	else
	    rval[1] = 1;
	if (syscall_debug > 2)
	  e_emulator_error("e_machine_fork(e=%d pid=%ld a4=x%lx)",
			   err, rval[0], rval[1]);
	return ESUCCESS;
}

errno_t e_machine_vfork(
	int			*argp,
	natural_t		*rval,
	struct emul_regs	*regs)
{
	errno_t err;

	err = e_vfork((pid_t *) rval);
	if (err)
	    return err;
	if (*rval)
	    rval[1] = 0;	/* parent edx = 0 */
	else
	    rval[1] = 1;	/* child  edx = 1 */
	return ESUCCESS;
}


errno_t
e_fork_call(boolean_t isvfork, pid_t *pid, boolean_t *ischild)
{
	struct alpha_thread_state	state;
	errno_t error;
	volatile natural_t x;

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
		if (syscall_debug > 0)
		    e_emulator_error("CHILD parent=%d", x);
		return 0;
	}

	state.r0 = 1;	/* v0 */
	
	/* FP regs!!!! */

	/*
	 * Create the child.
	 */
	error = (isvfork?bsd_vfork:bsd_fork)
	  (our_bsd_server_port,
	   (thread_state_t)&state,
	   ALPHA_THREAD_STATE_COUNT,
	   pid);

	*ischild = FALSE;
	return error;
}

void do_signal_notify() {}
void e_enable_asynch_signals() {}
void e_signal_child_setup() {}

e_machine_wait()
{
	emul_panic("e_machine_wait");
}

/* Signals */

/* Take a signal */
boolean_t take_a_signal(
	struct alpha_thread_state *state,
	struct emul_regs *regs)
{
	sigset_t	old_mask;
	int 		old_onstack;
	int		sig;
	integer_t	code;
	vm_offset_t	new_sp;
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
				(integer_t *) &handler,
				&new_sp);

	/*
	 * If there really were no signals to take, return.
	 */
	if (error || sig == 0)
	    return FALSE;

	if (syscall_debug > 1)
	    e_emulator_error("SIG%d x%lx", sig, handler);

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

			sc.sc_onstack = old_onstack;
			sc.sc_mask    = old_mask;
			sc.sc_pc      = state->pc;
			
			bcopy(state, &sc.sc_regs[0], sizeof(*state));
			/* sc.sc_regs[31] might be zero but we assume pc */
			sc.sc_ownedfp  = 0; /* XXX */

			/* Then just call it. */
			(*handler)(sig, code, &sc);

			bcopy(&sc.sc_regs[0], state, sizeof(*state));
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
	struct alpha_thread_state state;
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
	if ((state.r0 == 0)	/* v0 */
	    && (state.pc >= EMULATOR_BASE)
	    && (state.pc < EMULATOR_END))
	{
		state.r0 = 1;	/* for setjmp test above (avoid loop) */
	}
	emul_load_state(&state);
	/*NOTREACHED*/
}

errno_t e_bsd_sigreturn(struct sigcontext *sc)
{
	struct alpha_thread_state state;

	emul_save_state(&state);

	bcopy(&sc->sc_regs[0], &state, sizeof(state));
	state.pc = sc->sc_pc;
	if ((state.r0 == 0)
	    && (state.pc >= EMULATOR_BASE)
	    && (state.pc < EMULATOR_END))
	{
		state.r0 = 1;	/* for setjmp test above (avoid loop) */
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
