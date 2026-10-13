/* 
 * Mach Operating System
 * Copyright (c) 1994 Johannes Helander
 * Copyright (c) 1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JOHANNES HELANDER AND THE UNIVERSITY OF UTAH AND CSL ALLOWS FREE
 * USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.  JOHANNES HELANDER
 * AND THE UNIVERSITY OF UTAH AND CSL DISCLAIMS ANY LIABILITY OF ANY
 * KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS
 * SOFTWARE.
 *
 * 	Utah $Hdr: e_machinedep.c 1.8 95/03/23$
 */

#include <e_defs.h>
#include <syscall_table.h>
#include <parisc/eregs.h>
#include <e_hpux.h>

char *atbin_names[] =
{
	ATSYS_NAMES("parisc_")
};

/*
 * For fork/vfork, the parent returns 0 in rval[1] (ret1), child returns 1.
 *
 * XXX not machine or HPBSD specific, should be elsewhere.  e_bsd.c?
 */
errno_t e_machine_fork(
	int			*argp,
	int			*rval,
	struct emul_regs	*regs)
{
	errno_t err;

	err = e_fork((pid_t *)rval);
	if (err)
		return err;
	if (rval[0])
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

	err = e_vfork((pid_t *)rval);
	if (err)
		return err;
	if (rval[0])
		rval[1] = 0;
	else
		rval[1] = 1;

	return ESUCCESS;
}

errno_t
e_fork_call(boolean_t isvfork, pid_t *pid, boolean_t *ischild)
{
	struct parisc_thread_state state;
	int rv[2];
	errno_t error;
	volatile int x;

	/*
	 * Set up registers for child.  It resumes on its own stack.
	 */

	bzero((caddr_t) &state, sizeof state);
	x = emul_save_state(&state);

	if (x != 0) {
		/*
		 * XXX need to pass in regs so we can set a flag telling
		 * emul_trampoline to load child sr0 value from sr4.
		 * Right now the trampoline code just uses sr4 for its
		 * return space which works fine as long as there is
		 * only one space per process.
		 */
		*ischild = TRUE;
		*pid = x;
		child_init();
		if (syscall_debug > 1)
			e_emulator_error("CHILD parent=%d", x);
		return 0;
	}

	/*
	 * Arrange for a non-zero return in the child
	 * for emul_save_state above.
	 */
	state.r28 = 1;
	
	/*
	 * Create the child.
	 */
	if (isvfork)
		error = bsd_vfork(our_bsd_server_port,
				  (int *)&state, PARISC_THREAD_STATE_COUNT, rv);
	else
		error = bsd_fork(our_bsd_server_port,
				 (int *)&state, PARISC_THREAD_STATE_COUNT, rv);
	
	if (error == 0)
		*pid = rv[0];

	*ischild = FALSE;
	return error;
}

/*
 * XXX make sure kframe info is right
 */
noreturn emul_exec_start(
	int			*kframe,
	enum binary_type	binary_type,
	int			argc,
	char			*argv[],
	char			*envp[],
	struct exec_load_info	*li)
{
	struct parisc_thread_state state;

	bzero((caddr_t) &state, sizeof state);
	(void) emul_save_state(&state);

	/*
	 * Load argument registers:
	 *	arg0 = argc
	 *	arg1 = argv
	 *	arg2 = envp
	 *	arg3 = PA-RISC version * 10 (XXX always 1.1 for now)
	 * Note that DP is set by crt0 included in all binaries.
	 */
	state.r26 = argc;
	state.r25 = (int) argv;
	state.r24 = (int) envp;
	state.r23 = 11;
	state.iioq_head = (unsigned) li->pc | 3;
	state.iioq_tail = state.iioq_head + 4;

	if (syscall_debug > 1)
		e_emulator_error("emul_exec_start: x%x(x%x, x%x, x%x, x%x)\n",
				 state.iioq_head,
				 state.r26, state.r25, state.r24, state.r23);
	emul_load_state(&state);
	/*NOTREACHED*/
}

/*
 * PA-specific entry point for HPBSD wait/wait3.  Input formats:
 *	wait(status_p, xxx, xxx, 0xFacade)
 *	wait3(status_p, options, rusage_p, 0xFacade3)
 * Both convert to wait4 format:
 *	wait4(pid, status_p, options, rusage_p)
 */
errno_t
e_machine_wait(
	int			*argp,
	int			*rval,
	struct emul_regs	*regs)
{
	errno_t err;

#define WAIT_ANY (-1)

	if (argp[3] == 0xFacade3)
		err = e_hpbsd_wait4(WAIT_ANY, argp[0], argp[1], argp[2], rval);
	else
		err = e_hpbsd_wait4(WAIT_ANY, argp[0], 0, 0, rval);

	return err;
}

/* 
 * Process signals after a system call.
 * Called emul_syscall if needed.
 *
 * XXX This signal code does not care about sigstacks and similar
 * XXX messy stuff. However, it works in most simple cases.
 */
void take_signals_and_do_sigreturn(struct emul_regs *regs)
{
	struct parisc_thread_state state;
	volatile int x;

	bzero((caddr_t) &state, sizeof state);
	x = emul_save_state(&state);

	if (x != 0) {
		emul_set_dp(emul_dp());
		return;
	}

	/* XXX should we set state.flags to SS_INSYSCALL here? */

	/* XXX make sure state reflects user's DP, not emulator's */
	state.r27 = regs->dp;

	while (take_a_signal(&state))
	    ;

	if (state.r2 == 0) {
		e_emulator_error ("would jump to zero!\n");
		return;
	}
	/*
	 * Now return where requested (or from this function if state unchanged)
	 */
	if (state.r28 == 0 &&
	    state.iioq_head > EMULATOR_BASE && state.iioq_head < EMULATOR_END)
		state.r28 = 1;		/* make sure we return above */
	emul_load_state(&state);
	/*NOTREACHED*/
}

/*
 * e_hpux_sigreturn(handler_rval)
 *
 * Get frame from users stack pointer (given "regs->argp") and hand it
 * over to e_machine_sigreturn() in the "traditional" way.
 */
void
e_hpux_sigreturn(int *argp, int *rval, struct emul_regs	*regs)
{
	hpux_sigframe_t *fp;
	struct a {
		struct sigcontext *sigcntxp;
	} a_hack;

	fp = ((hpux_sigframe_t *)(regs->argp + 8)) - 1;
	a_hack.sigcntxp = &fp->sf_sc;
	(void) e_machine_sigreturn((int *)&a_hack, rval, regs);
	/*NOTREACHED*/
}

/*
 * Take a signal.
 * Called both after a system call detected signal and asynchronous signals
 * delivered via the server (to the emulator signal thread).
 */
boolean_t take_a_signal(struct parisc_thread_state *state)
{
	sigset_t old_mask;
	int old_onstack, sig, code, new_sp;
	int error, edp;
	void (*handler)();
	struct sigcontext sc;

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
	 * Prepare for and call the appropriate signal handler.
	 */
	if (e_my_binary_type == BT_HPUX) {
		extern boolean_t __hpux_resethand[];
		extern void (*__hpux_sigret)();
		struct sigaction act;
		register hpux_sigframe_t *fp;	/* addr of new stack frame */
		struct sigcontext *scp;		/* context we are creating */
		u_int nsp;			/* new stack pointer */
		int oonstack = old_onstack;	/* previously on signal stack */

		/*
		 * If we are a HP-UX binary, check for SysV-style signal
		 * semantics (where handler is reset when caught).
		 */
		if (__hpux_resethand[sig]) {
			__hpux_resethand[sig] = FALSE;
			act.sa_handler = SIG_DFL;
			act.sa_mask = 0;
			act.sa_flags = 0;
			(void) e_sigaction(sig, &act, 0);
		}

		/*
		 * Effectively push the signal stack frame on our stack.
		 * This is a little tricky, as we are likely using the
		 * stack we are pushing the frame on.  To deal with this,
		 * we add 2048 to the saved $sp and promise not to call
		 * any functions once we start setting up the frame.
		 *
		 * Alternately, we could:
		 *   1) Let compiler allocate frame on the stack and blow
		 *      that in as new $sp.  However, this may make back-
		 *      traces and debugging more difficult.
		 *   2) Fill in a structure and copy it just before call.
		 *      This is nice, except that we waste time copying.
		 */
		nsp = roundup(state->r30 + 2048 + sizeof(*fp), 8);
		fp = ((hpux_sigframe_t *)nsp) - 1;

		if (syscall_debug > 1)
			e_emulator_error(
			 "take_a_signal: sig %d fp %x osp %x sp %x scp %x %x\n",
			 sig, fp, state->r30, nsp, &fp->sf_sc, handler);

		fp->sf_signum = signo_bsdtohpux(sig);
		if (sig == SIGILL || sig == SIGFPE || sig == SIGBUS)
			fp->sf_code = code;
		else
			fp->sf_code = 0;
		fp->sf_scp = scp = &fp->sf_sc;
		fp->sf_handler = handler;

		/*
		 * Build the signal context to be used by sigreturn.
		 */
		scp->sc_onstack = oonstack;
		scp->sc_mask = old_mask;
		scp->sc_sp = state->r30;		/* sp */
		scp->sc_fp = 0;				/* XXX */
		scp->sc_ap = (int)&fp->sf_state;
		scp->sc_ps = state->cr22;		/* ipsw */

		/*
		 * Save necessary hardware state.
		 */
		bcopy(state, &fp->sf_state.ss_state, sizeof(*state));
		fp->sf_state.ss_cr16 = fp->sf_state.ss_cr23 = 0;

		/*
		 * (XXX) Need to think about trap/syscall distinction.
		 */
		fp->sf_state.ss_syscall = 0;
		scp->sc_pcsqt = scp->sc_pcsqh = state->sr4;
		scp->sc_pcoqh = state->iioq_head;
		scp->sc_pcoqt = state->iioq_tail;

		/*
		 * Reset floating point coprocessor state.
		 */
		{
			struct hpux_fp {
				int ss_frstat;
				int ss_frexcp1;
				int ss_frexcp2;
				int ss_frexcp3;
				int ss_frexcp4;
				int ss_frexcp5;
				int ss_frexcp6;
				int ss_frexcp7;
			} *regs = (struct hpux_fp *)&fp->sf_state.ss_state.fr0;
			regs->ss_frstat &= ~0x40;
			regs->ss_frexcp1 = regs->ss_frexcp2 = regs->ss_frexcp3 =
			    regs->ss_frexcp4 = regs->ss_frexcp5 =
			    regs->ss_frexcp6 = regs->ss_frexcp7 = 0;
		}

		/*
		 * Set up args to signal handler (signo,code,scp).
		 */
		state->r26 = fp->sf_signum;
		state->r25 = fp->sf_code;
		state->r24 = (int)fp->sf_scp;

		/*
		 * If our handler has the magic `HPUX_SHAREDLIBS' bit set,
		 * then it is not really a handler at all!  Rather, it
		 * is a pointer to 2 words; the first being our handler and
		 * the second is our "External DP".  Ugh.
		 */
		if ((u_int)handler & HPUX_SHAREDLIBS) {
			void (**tmp)();

			tmp = (void (**)()) ((u_int)handler & ~HPUX_SHAREDLIBS);
			handler = fp->sf_handler = *tmp;
			state->r19 = *((int *)tmp+1);
			tmp = (void(**)())
			    ((u_int)__hpux_sigret & ~HPUX_SHAREDLIBS);
			fp->sf_fm.fm_erp = state->r2 = (int)*tmp;
			fp->sf_fm.fm_edp = (int)*((int *)tmp+1);
		} else {
			state->r2 = (u_int)__hpux_sigret;
		}
		state->iioq_head = (u_int)handler;
		state->iioq_tail = (u_int)handler + 4;

		/*
		 * The users stack has grown a *little*; update the stack ptr.
		 * First, stuff previous stack ptr into frame marker giving the
		 * stack a wholesome "Utah" look.
		 */
		fp->sf_fm.fm_psp = state->r30;
		state->r30 = nsp;

		if ((u_int)state->iioq_head < 4)  /* sanity... or reality? */
			emul_panic("take_a_signal: jump to zero!\n");

		/*
		 * Say yaw pwayers, wabbit...
		 */
		emul_load_state(state);
		/*NOTREACHED*/

		emul_panic("take_a_signal: emul_load_state returned!");
	}

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sc.sc_onstack = old_onstack;
	sc.sc_mask = old_mask;
		
	sc.sc_sp = state->r30;
	sc.sc_fp = 0;
	sc.sc_ap = (int) state;	/* XXX not quite right */
	sc.sc_ps = state->cr22;

	/*
	 * XXX need to think about the INTRAP/INSYSCALL distinction
	 *     and whether we need to use PC queues vs. sr0/r31.
	 */
	sc.sc_pcsqh = sc.sc_pcsqt = state->sr4;
	sc.sc_pcoqh = state->iioq_head;
	sc.sc_pcoqt = state->iioq_tail;
		
	/*
	 * Then just call it
	 * XXX make sure we call with the user's DP, not the emulator's
	 */
	emul_set_dp(state->r27);
	(*handler)(sig, code, &sc);
	emul_set_dp(emul_dp());
		
	state->r30 = sc.sc_sp;
	state->cr22 = sc.sc_ps;
	
	/*
	 * XXX need to think about the INTRAP/INSYSCALL distinction
	 *     and whether we need to use PC queues vs. sr0/r31.
	 */
	state->iioq_head = sc.sc_pcoqh;
	state->iioq_tail = sc.sc_pcoqt;

	if (syscall_debug > 2)
		e_emulator_error("return_SIG%d", sig);

	/* Re-enable signal */
	error = bsd_sigreturn(our_bsd_server_port,
			      old_onstack & 01,
			      old_mask);
	return TRUE;
}

/*
 * Return from a signal.
 * May be called from a signal handler (sc_ap != 0)
 * or from a longjmp (sc_ap == 0) not related to a signal.
 */
errno_t e_machine_sigreturn(
	int	  		*argp,
	int			*rvals,
	struct emul_regs	*regs)
{
	struct parisc_thread_state state;
	struct sigcontext *scp;			/* ptr to new sig context */
	hpux_sigstate_t *rf;			/* old saved hardware state */

	scp = (struct sigcontext *) argp[0];

	if (syscall_debug > 1)
		e_emulator_error("e_machine_sigreturn(%x, %x, %x)",
			scp, rvals, regs);

	if ((rf = (hpux_sigstate_t *)scp->sc_ap) == 0) {
		/* longjmp */
		/* Re-enable signal */
		bsd_sigreturn(our_bsd_server_port,
			scp->sc_onstack & 01, scp->sc_mask);

		/* Arrange to return to the caller of setjmp.  */
		regs->reto = (int *) scp->sc_pcoqh;

		/* Arrange to reset the stack pointer.  */
		regs->flags |= SB_NEW_SP_IN_RET1;
		regs->ret1 = scp->sc_sp;

		/* Return value from longjmp.  */
		return scp->sc_pcoqt;
	}

	bcopy(&rf->ss_state, &state, sizeof(state));
	state.cr22 = scp->sc_ps;
	state.r30 = scp->sc_sp;

	/*
	 * XXX need to think about the INTRAP/INSYSCALL distinction
	 *     and whether we need to use PC queues vs. sr0/r31.
	 */
	state.iioq_head = scp->sc_pcoqh;
	state.iioq_tail = scp->sc_pcoqt;

	if (syscall_debug > 2)
		e_emulator_error("e_machine_sigreturn pc %x/%x sp %x\n",
			state.iisq_head, state.iioq_head, state.r30);

	/*
	 * Re-enable signal before returning.
	 *
	 * Load saved state and resume execution at "state.iioq_head".
	 * If this takes us back into take_signals_and_do_sigreturn()
	 * (handler called sigreturn itself) set ret0 so we don't loop.
	 */
	bsd_sigreturn(our_bsd_server_port, scp->sc_onstack & 01, scp->sc_mask);

	if (state.r28 == 0 &&
	    state.iioq_head > EMULATOR_BASE && state.iioq_head < EMULATOR_END)
		state.r28 = 1;

	if ((u_int)state.iioq_head < 4)		/* sanity... or reality? */
		emul_panic("e_machine_sigreturn: jump to zero!\n");

	emul_load_state(&state);
	/*NOTREACHED*/

	emul_panic("e_machine_sigreturn: emul_load_state returned!");
	return 0;	/* XXX for gcc */
}
