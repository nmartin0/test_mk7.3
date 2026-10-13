/* 
 * Mach Operating System
 * Copyright (c) 1994 Johannes Helander
 * Copyright (c) 1994 Ian Dall
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JOHANNES HELANDER AND IAN DALL ALLOW FREE USE OF THIS SOFTWARE IN
 * ITS "AS IS" CONDITION.  JOHANNES HELANDER AND IAN DALL DISCLAIM ANY
 * LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 * USE OF THIS SOFTWARE.
 */
/*
 * HISTORY
 * $Log: e_machinedep.c,v $
 * Revision 1.2  1996/03/14  21:07:51  sclawson
 * Ian Dall's signal fixes.
 *
 * Revision 1.1.1.2  1995/03/23  01:15:37  law
 * lites-950323 from jvh.
 *
 * 02-Sep-95  Ian Dall (dall@hfrd.dsto.gov.au)
 *	Make sigcontext conform to the netbsd definition.
 *
 */
/* 
 *	File:	emulator/ns532/e_machinedep.c
 *	Author:	Ian Dall
 *
 * 	Based heavilly on the i386 version.
 *
 *	Machine dependent stuff for the ns532.
 */

#include <e_defs.h>
#include <syscall_table.h>
#include <ns532/psl.h>

binary_type_t e_my_binary_type;

char *atbin_names[] =
{
	ATSYS_NAMES("ns532_")
};

errno_t
e_fork_call(boolean_t isvfork, pid_t *pid, boolean_t *ischild)
{
	struct ns532_thread_state	state;
	
	int rv[2];
	errno_t error;

	volatile int x;

	/*
	 * Set up registers for child.  It resumes on its own stack.
	 */

	x = emul_save_state(&state);
	if (x != 0) {
		*ischild = TRUE;
		*pid = x;
		child_init();
		if (syscall_debug > 1)
		    e_emulator_error("CHILD parent=%d", x);
		return 0;
	}

	state.r0 = 1;
	
	/* FP regs!!!! */

	/*
	 * Create the child.
	 */
	error = (isvfork?bsd_vfork:bsd_fork)
	    (our_bsd_server_port,
	     (int *)&state,
	     NS532_THREAD_STATE_COUNT,
	     rv);

	if (error == 0)
	    *pid = rv[0];

	*ischild = FALSE;
	return error;
}

struct	cmu_43ux_sigcontext {
	int	sc_onstack;		/* sigstack state to restore */
	int	sc_mask;		/* signal mask to restore */
        int     sc_r0;          /* 8 */
        int     sc_r1;          /* 12 */
        int     sc_r2;          /* 16 */
        int     sc_r3;          /* 20 */
        int     sc_r4;
        int     sc_r5;
        int     sc_r6;
        int     sc_r7;
        int     sc_sb;          /* 40 */
        int     sc_fp;
        int     sc_sp;          /* 48 */
        int     sc_psr;
        int     sc_pc;          /* 56 */
    };

struct mach532_sigframe {
	int	sf_signum;
	int	sf_code;
	struct cmu_43ux_sigcontext *sf_scp;
	int	sf_handler;
	struct cmu_43ux_sigcontext sf_sc;
    };

struct sigframe {
	int	sf_signum;
	int	sf_code;
	struct	sigcontext *sf_scp;
	sig_t	sf_handler;
	struct	sigcontext sf_sc;
} ;


/* Take a signal */
boolean_t take_a_signal(struct ns532_thread_state *state)
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
	      case BT_CMU_43UX:
		{
			struct cmu_43ux_sigcontext sc;

			/*
			 * Build the signal context to be used by sigreturn.
			 */
			sc.sc_onstack = old_onstack;
			sc.sc_mask = old_mask;

			sc.sc_r0 = state->r0;
			sc.sc_r1 = state->r1;
			sc.sc_r2 = state->r2;
			sc.sc_r3 = state->r3;
			sc.sc_r4 = state->r4;
			sc.sc_r5 = state->r5;
			sc.sc_r6 = state->r6;
			sc.sc_r7 = state->r7;
			sc.sc_sb = state->sb;
			sc.sc_fp = state->fp;
			sc.sc_sp = state->sp;
			sc.sc_psr = state->psr;
			sc.sc_pc = state->pc;

			/* Then just call it */
			(*handler)(sig, code, &sc);

			state->r0 = sc.sc_r0;
			state->r1 = sc.sc_r1;
			state->r2 = sc.sc_r2;
			state->r3 = sc.sc_r3;
			state->r4 = sc.sc_r4;
			state->r5 = sc.sc_r5;
			state->r6 = sc.sc_r6;
			state->r7 = sc.sc_r7;
			state->sb = sc.sc_sb;
			state->fp = sc.sc_fp;
			state->sp = sc.sc_sp;
			state->psr = sc.sc_psr;
			state->pc = sc.sc_pc;

			break;
		}
	      case BT_LINUX:
		emul_panic("take_a_signal: linux executables not supported");
		break;
	      default:
		{
			struct sigcontext sc;

			sc.sc_onstack = old_onstack;
			sc.sc_mask = old_mask;

			sc.sc_pc = state->pc;
			sc.sc_ps = state->psr;
			sc.sc_sp = state->sp;
			sc.sc_fp = state->fp;
			sc.sc_reg[0] = state->r0;
			sc.sc_reg[1] = state->r1;
			sc.sc_reg[2] = state->r2;
			sc.sc_reg[3] = state->r3;
			sc.sc_reg[4] = state->r4;
			sc.sc_reg[5] = state->r5;
			sc.sc_reg[6] = state->r6;
			sc.sc_reg[7] = state->r7;

			/* Then just call it */
			(*handler)(sig, code, &sc);

			state->pc = sc.sc_pc;
			state->psr = sc.sc_ps;
			state->sp = sc.sc_sp;
			state->fp = sc.sc_fp;
			state->r0 = sc.sc_reg[0];
			state->r1 = sc.sc_reg[1];
			state->r2 = sc.sc_reg[2];
			state->r3 = sc.sc_reg[3];
			state->r4 = sc.sc_reg[4];
			state->r5 = sc.sc_reg[5];
			state->r6 = sc.sc_reg[6];
			state->r7 = sc.sc_reg[7];
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
void take_signals_and_do_sigreturn()
{
	struct ns532_thread_state state;
	volatile int x;

	x = emul_save_state(&state);
	/* 
	 * Check for longjmp or sigreturn out of handler.
	 */
	if (x != 0)
	    return;

	while (take_a_signal(&state))
	    ;

	/* Now return where requested or from this function if unchanged */
	if ((state.r0 == 0)
	    && (state.pc >= EMULATOR_BASE)
	    && (state.pc < EMULATOR_END))
	{
		state.r0 = 1;	/* for setjmp test above (avoid loop) */
	}
	emul_load_state(&state);
	/*NOTREACHED*/
}

/* This is needed for gdb (and longjmp in general) to work */
errno_t e_cmu_43ux_sigreturn(struct cmu_43ux_sigcontext *sc)
{
	struct ns532_thread_state state;

	emul_save_state(&state);

			state.r0 = sc->sc_r0;
			state.r1 = sc->sc_r1;
			state.r2 = sc->sc_r2;
			state.r3 = sc->sc_r3;
			state.r4 = sc->sc_r4;
			state.r5 = sc->sc_r5;
			state.r6 = sc->sc_r6;
			state.r7 = sc->sc_r7;
			state.sb = sc->sc_sb;
			state.fp = sc->sc_fp;
			state.sp = sc->sc_sp;
			state.psr = sc->sc_psr;
			state.pc = sc->sc_pc;

	if ((state.r0 == 0)
	    && (state.pc > EMULATOR_BASE)
	    && (state.pc < EMULATOR_END))
	{
		state.r0 = 1;	/* for setjmp test above (avoid loop) */
	}

	if (syscall_debug > 2)
	    e_emulator_error("cmu_43ux_SIGRETURN");

	/* Re-enable signal */
	bsd_sigreturn(our_bsd_server_port,
		      sc->sc_onstack & 01,
		      sc->sc_mask);
	emul_load_state(&state);
	/*NOTREACHED*/
	emul_panic("e_cmu_43ux_sigreturn: _longjmp returned!");
}

errno_t e_bsd_sigreturn(struct sigcontext *sc)
{
	struct ns532_thread_state state;

	emul_save_state(&state);

	state.pc = sc->sc_pc;
	state.psr = sc->sc_ps;
	state.sp = sc->sc_sp;
	state.fp = sc->sc_fp;
	state.r0 = 1;

	if (syscall_debug > 2)
	    e_emulator_error("bsd_SIGRETURN");

	/* Re-enable signal */
	bsd_sigreturn(our_bsd_server_port,
		      sc->sc_onstack & 01,
		      sc->sc_mask);
	emul_load_state(&state);
	/*NOTREACHED*/
	emul_panic("e_bsd_sigreturn: _longjmp returned!");
}

errno_t e_linux_sigreturn(/* struct linux_sigcontext */ void *sc)
{
  emul_panic("e_linux_sigreturn: linux not supported");
}
