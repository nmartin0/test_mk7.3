/*
 * AI-ONLY NOTE: new file, test_mk7.3's, under no other copyright.
 *
 *	File:	server/i386/syscall.c
 *
 *	System calls from a process the server runs without the
 *	emulator (NO_EMULATOR, the STD+WS+osfmach3+noemulator
 *	configuration; docs/roadmap.md, Tier 1 item 2).
 *
 *	The design is Patience's, "Redirecting System Calls in Mach 3.0"
 *	(1993): a process with no emulation vector traps into the kernel,
 *	which raises EXC_SYSCALL (OSFMK 7.3, i386/locore.s,
 *	mach_call_range); each process's thread sends that exception to a
 *	port of its own, here the process's request port, with behaviour
 *	EXCEPTION_STATE (section 4), so the message names the process by
 *	the port it arrives on and carries the registers; the reply
 *	carries them back, with the call's results.  The call itself is
 *	the integrated kernel's syscall() (section 7): this is 4.4BSD's
 *	i386 syscall() -- arguments from the user stack, the switch table,
 *	the carry flag for an error -- with its "preamble" taking the
 *	registers from the message and its "postamble" putting them in
 *	the reply.  The table is noemul_sysent (kern/noemul_sysent.c),
 *	4.4BSD's numbers, which are NetBSD 1.0's.
 *
 *	What 4.4BSD's i386 syscall() does that this follows (and which
 *	NetBSD 1.0's programs expect): a call is "lcall $0x7,$0", seven
 *	bytes; the arguments start one word above the user stack pointer,
 *	past the return address of the C stub; calls 0 (syscall) and 198
 *	(__syscall) take the call number from the first argument, a word
 *	or a quad; edx goes in as the second return value; success clears
 *	the carry flag and returns eax and edx; an error sets it, with
 *	the errno in eax; ERESTART backs eip up over the lcall; and
 *	EJUSTRETURN leaves the registers as the call left them.
 */

#include "no_emulator.h"

#include <serv/server_defs.h>
#include <serv/syscall_subr.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/synch.h>
#include <mach/exception.h>
#include <mach/error.h>
#include <device/device_types.h>
#include <i386/eflags.h>

#if NO_EMULATOR

extern struct sysent	noemul_sysent[];
extern int		noemul_nsysent;

extern boolean_t exc_server(mach_msg_header_t *, mach_msg_header_t *);
extern mach_error_t nosys();

#define	LCALL_SIZE	7		/* lcall $0x7,$0x0 */
#define	NOEMUL_SYS_syscall	0	/* 4.4BSD's numbers, not the */
#define	NOEMUL_SYS___syscall	198	/* server's <sys/syscall.h> */
#define	NOEMUL_MAXARGS	8		/* most words a call takes */

/*
 * The thread's registers as a program starts: pc and stack pointer
 * set, the general registers zeroed, as the emulator starts one
 * (emulator/i386/e_trampoline.c, emul_exec_start), the flags as
 * set_emulator_state() leaves them.  The segment registers are kept.
 * Set on the thread, which is not running -- init's has not run, a
 * set-id program's is new, and an exec's is waiting for the reply
 * (serv/server_exec.c) -- and, for an exec, in the reply too, which
 * carries the registers the call left (EJUSTRETURN, below).
 */
void
noemul_set_program_state(
	struct proc	*p,
	vm_offset_t	pc,
	vm_offset_t	sp)
{
	proc_invocation_t pk = get_proc_invocation();
	struct i386_thread_state ts;
	mach_msg_type_number_t count = i386_THREAD_STATE_COUNT;
	kern_return_t kr;

	kr = thread_get_state(p->p_thread, i386_THREAD_STATE,
			      (thread_state_t) &ts, &count);
	if (kr != KERN_SUCCESS)
		panic("noemul_set_program_state: get", kr);
	ts.eip = pc;
	ts.uesp = sp;
	ts.eax = ts.ebx = ts.ecx = ts.edx = 0;
	ts.esi = ts.edi = ts.ebp = 0;
	ts.efl = 0;
	kr = thread_set_state(p->p_thread, i386_THREAD_STATE,
			      (thread_state_t) &ts, i386_THREAD_STATE_COUNT);
	if (kr != KERN_SUCCESS)
		panic("noemul_set_program_state: set", kr);
	if (pk->k_trap_state && pk->k_p == p)
		*(struct i386_thread_state *) pk->k_trap_state = ts;
}

/*
 * Send the thread's system call exceptions to the process's request
 * port, with its registers (Patience, sections 3 and 4).  A thread
 * exception port, not the task's: the task's is the one Lites maps
 * faults to signals on (kern/init_main.c), which a debugger may
 * take; and a thread's is not inherited by a forked task.  The send
 * right is made the way task_to_proc_enter() makes the one it hands
 * to task_set_bootstrap_port().
 */
void
noemul_set_syscall_port(struct proc *p)
{
	kern_return_t kr;

	kr = port_object_make_send(p->p_req_port);
	if (kr != KERN_SUCCESS)
		panic("noemul_set_syscall_port: make send", kr);
	kr = thread_set_exception_ports(p->p_thread, EXC_MASK_SYSCALL,
					p->p_req_port, EXCEPTION_STATE,
					i386_THREAD_STATE);
	if (kr != KERN_SUCCESS)
		panic("noemul_set_syscall_port", kr);
	(void) mach_port_deallocate(mach_task_self(), p->p_req_port);
}

/*
 * The errno a program sees for an error inside the server: the
 * emulator's conversion (emulator/error_codes.c,
 * e_mach_error_to_errno and e_kernel_error_to_lites_error), for a
 * BSD program.  The emulator ends a program given an error it cannot
 * convert; so does this, by SIGKILL, taken on the way out.
 */
static int
noemul_errno(
	struct proc	*p,
	int		syscode,
	mach_error_t	kr)
{
	switch (kr) {
	      case KERN_INVALID_ADDRESS:
	      case KERN_PROTECTION_FAILURE:
		kr = EFAULT;
		break;
	      case KERN_INVALID_ARGUMENT:
	      case KERN_INVALID_NAME:
	      case KERN_INVALID_TASK:
	      case KERN_INVALID_RIGHT:
	      case KERN_INVALID_VALUE:
	      case D_INVALID_RECNUM:
	      case D_INVALID_SIZE:
	      case MIG_TYPE_ERROR:
	      case MIG_REPLY_MISMATCH:
	      case MIG_REMOTE_ERROR:
	      case MIG_BAD_ID:
	      case MIG_BAD_ARGUMENTS:
	      case MIG_ARRAY_TOO_LARGE:
		kr = EINVAL;
		break;
	      case KERN_RESOURCE_SHORTAGE:
		kr = EAGAIN;
		break;
	      case D_IO_ERROR:
	      case D_NO_MEMORY:
	      case D_READ_ONLY:
		kr = EIO;
		break;
	      case D_WOULD_BLOCK:
		kr = EWOULDBLOCK;
		break;
	      case D_NO_SUCH_DEVICE:
		kr = ENXIO;
		break;
	      case D_ALREADY_OPEN:
		kr = EBUSY;
		break;
	      case D_DEVICE_DOWN:
		kr = ENETDOWN;
		break;
	      case D_INVALID_OPERATION:
		kr = ENOTTY;
		break;
#ifdef D_NO_SPACE
	      case D_NO_SPACE:
		kr = ENOSPC;
		break;
#endif
	}
	if ((kr & (system_emask|sub_emask)) == unix_err(0))
		return err_get_code(kr);
	printf("(lites) pid %d: system call %d: error x%x (%s) has no errno\n",
	       p->p_pid, syscode, kr, mach_error_string(kr));
	psignal(p, SIGKILL);
	return err_get_code(EINVAL);
}

/*
 * 4.4BSD's userret(): take the pending signals before the process
 * goes back to user mode.  Under the lock bsd_take_signal() holds
 * for CURSIG().
 */
static void
noemul_userret(struct proc *p)
{
	int sig;

	while (p->p_task != MACH_PORT_NULL && !(p->p_flag & P_WEXIT)) {
		mutex_lock(&p->p_lock);
		sig = CURSIG(p);
		mutex_unlock(&p->p_lock);
		if (sig == 0)
			break;
		postsig(sig);
	}
}

/*
 * The system call, from the exception message's registers.  Called
 * by exc_server() from bsd_syscall_server(), with the process from
 * the port in pk->k_p, locked.  Replies with the registers the
 * thread is to go on with, or not at all if it has gone.
 */
kern_return_t
catch_exception_raise_state(
	mach_port_t		port,
	exception_type_t	exception,
	exception_data_t	code,
	mach_msg_type_number_t	codeCnt,
	int			*flavor,
	thread_state_t		old_state,
	mach_msg_type_number_t	old_stateCnt,
	thread_state_t		new_state,
	mach_msg_type_number_t	*new_stateCnt)
{
	proc_invocation_t pk = get_proc_invocation();
	struct proc *p = pk->k_p;
	struct i386_thread_state *regs = (struct i386_thread_state *) new_state;
	register struct sysent *callp;
	vm_offset_t params, opc;
	mach_port_t task;
	int syscode, error, args[NOEMUL_MAXARGS], rval[2];

	if (p == 0)
		return KERN_FAILURE;		/* not from a process */
	pk->k_p = 0;		/* start_server_op() sets it again */
	if (exception != EXC_SYSCALL || *flavor != i386_THREAD_STATE
	    || old_stateCnt != i386_THREAD_STATE_COUNT) {
		mutex_unlock(&p->p_lock);
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 * Preamble.
	 */
	bcopy((char *) old_state, (char *) new_state,
	      old_stateCnt * sizeof (natural_t));
	*new_stateCnt = old_stateCnt;
	syscode = regs->eax;
	opc = regs->eip - LCALL_SIZE;
	params = regs->uesp + sizeof (int);
	task = p->p_task;

	pk->k_p = p;
	/* the reply's registers, for noemul_fork() and signals */
	pk->k_trap_state = (vm_offset_t) regs;
	error = start_server_op(p, syscode);	/* unlocks p */
	if (error == ERESTART) {
		/*
		 * Signals were pending before the call: 4.4BSD would
		 * have taken them on the way back to user mode, before
		 * the program made it.  Take them, then make the call.
		 */
		mutex_lock(&p->p_lock);
		error = start_server_op(p, 1000);	/* take_signal */
		if (error == 0) {
			/*
			 * A handler's frame saves the call's lcall as the
			 * place to go back to, so that the program makes the
			 * call again after the handler, as if the signal had
			 * come before it; and the call is not made now.
			 */
			regs->eip = opc;
			pk->k_sigframe = 0;
			unix_master();
			noemul_userret(p);
			unix_release();
			if (p->p_task == MACH_PORT_NULL) {
				server_thread_deregister(p);
				pk->k_p = 0;
				pk->k_trap_state = 0;
				return MIG_NO_REPLY;
			}
			if (pk->k_sigframe) {
				/* a handler is to run first */
				pk->k_trap_state = 0;
				(void) end_server_op(p, 0, (boolean_t *) 0);
				pk->k_p = 0;
				return KERN_SUCCESS;
			}
			regs->eip = opc + LCALL_SIZE;
		}
	}
	if (error) {
		pk->k_p = 0;
		pk->k_trap_state = 0;
		return MIG_NO_REPLY;		/* exiting */
	}

	if (syscode == NOEMUL_SYS_syscall) {
		syscode = fuword((void *) params);
		params += sizeof (int);
	} else if (syscode == NOEMUL_SYS___syscall) {
		/* the quad's low word, first on the i386 */
		syscode = fuword((void *) params);
		params += sizeof (quad_t);
	}
	if (syscode < 0 || syscode >= noemul_nsysent)
		callp = &noemul_sysent[NOEMUL_SYS_syscall]; /* nosys */
	else
		callp = &noemul_sysent[syscode];
	if (callp->sy_narg > NOEMUL_MAXARGS)
		panic("catch_exception_raise_state: sy_narg");
	if (callp->sy_call == nosys)	/* while calls are missing */
		printf("(lites) pid %d: system call %d is not in the server\n",
		       p->p_pid, syscode);

	rval[0] = 0;
	rval[1] = regs->edx;
	if (callp->sy_narg)
		error = copyin((void *) params, (void *) args,
			       callp->sy_narg * sizeof (int));
	else
		error = 0;

	unix_master();
	if (error == 0)
		error = (*callp->sy_call)(p, args, rval);

	/*
	 * Postamble.
	 */
	if (p->p_task != MACH_PORT_NULL) {
		switch (error) {
		      case 0:
			regs->efl &= ~EFL_CF;
			regs->eax = rval[0];
			regs->edx = rval[1];
			break;
		      case ERESTART:
			regs->eip = opc;
			break;
		      case EJUSTRETURN:
			/* the call set the reply's registers: execve, sigreturn */
			break;
		      default:
			regs->efl |= EFL_CF;
			regs->eax = noemul_errno(p, syscode, error);
			break;
		}
		noemul_userret(p);
	}
	unix_release();

	pk->k_trap_state = 0;
	if (p->p_task == MACH_PORT_NULL) {
		/* exited: no thread to reply to (ux_generic_server) */
		server_thread_deregister(p);
		pk->k_p = 0;
		return MIG_NO_REPLY;
	}
	(void) end_server_op(p, 0, (boolean_t *) 0);
	pk->k_p = 0;
	/* a set-id exec ran the program in a new task, its thread gone */
	return p->p_task == task ? KERN_SUCCESS : MIG_NO_REPLY;
}

/*
 * Signals (docs/roadmap.md, Tier 1 item 2, step 5).  A caught signal
 * is delivered as an integrated kernel delivers it on the way back to
 * user mode (Patience, section 7: the kernel's own code reused): the
 * registers of the call or fault, here the reply's, are saved in a
 * frame on the user stack and the thread is sent to the handler;
 * sigreturn() restores them.  Frame, context, the code that calls the
 * handler and sigreturn() are 4.4BSD-Lite2's i386 ones
 * (i386/i386/machdep.c, sendsig() and sigreturn(); locore.s, sigcode),
 * and the context is the one this tree's emulator gives a NetBSD
 * program (emulator/i386/e_machinedep.c, take_a_signal(); this tree's
 * <i386/signal.h>).
 */
struct noemul_sigframe {		/* 4.4BSD-Lite2, i386 */
	int	sf_signum;
	int	sf_code;
	struct sigcontext *sf_scp;
	sig_t	sf_handler;
	int	sf_eax;
	int	sf_edx;
	int	sf_ecx;
	struct sigcontext sf_sc;
};

/*
 * 4.4BSD-Lite2's i386 sigcode (locore.s), entered with the stack
 * pointer at the frame:
 *	movl	12(%esp),%eax		the handler
 *	call	%eax			(signum, code, scp)
 *	xorl	%eax,%eax
 *	movb	$103,%al		sigreturn()
 *	lcall	$7,$0
 *	hlt				never gets here
 * Lite2 keeps it in the process's pcb, read-only to the process; here
 * it is in the process's read-only shared page (include/sys/ushared.h,
 * us_sigcode), mapped at the top of the emulator's space
 * (serv/serv_fork.c, mapin_user()), set in process 0's and copied to
 * each child's with the page, as Lite2's init386() sets process 0's
 * pcb and fork copies it.
 */
static const unsigned char noemul_sigcode[] = {
	0x8b, 0x44, 0x24, 0x0c,
	0xff, 0xd0,
	0x31, 0xc0,
	0xb0, 0x67,
	0x9a, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00,
	0xf4
};

#define	NOEMUL_SIGCODE_ADDR \
	(EMULATOR_END - vm_page_size \
	 + (vm_offset_t) &((struct ushared_ro *) 0)->us_sigcode)

void
noemul_sigcode_init(struct proc *p)
{
	bzero((caddr_t) p->p_shared_ro->us_sigcode,
	      sizeof (p->p_shared_ro->us_sigcode));
	bcopy((caddr_t) noemul_sigcode, (caddr_t) p->p_shared_ro->us_sigcode,
	      sizeof (noemul_sigcode));
}

/*
 * The flags a context can set: those a program can set itself with
 * popf, as this tree's emulator restores a context (emulator/i386/
 * emul_misc_asm.s, emul_load_state) -- carry, parity, adjust, zero,
 * sign, trap, direction, overflow, alignment check, ID.  OSFMK's
 * thread_set_state() keeps IOPL, NT and RF clear and IF set
 * (i386/pcb.c) whatever is asked.
 */
#define	NOEMUL_EFL_USER	(EFL_CF|EFL_PF|EFL_AF|EFL_ZF|EFL_SF|EFL_TF|EFL_DF| \
			 EFL_OF|EFL_AC|EFL_ID)

/*
 * 4.4BSD-Lite2's i386 sendsig(), from postsig(): the frame for a
 * caught signal, on the signal stack if the signal asks for it and the
 * process has one and is not on it, else below the stack pointer.  The
 * registers are the reply's (pk->k_trap_state), set for a system call
 * by catch_exception_raise_state(), for a fault by thread_signal() and
 * for a process in user code by noemul_signal_notify().
 */
void
noemul_sendsig(
	sig_t		catcher,
	int		sig,
	int		mask,
	unsigned	code)
{
	proc_invocation_t pk = get_proc_invocation();
	struct proc *p = pk->k_p;
	struct sigacts *psp = p->p_sigacts;
	struct i386_thread_state *regs =
		(struct i386_thread_state *) pk->k_trap_state;
	struct noemul_sigframe frame, *fp;
	int oonstack;

	if (regs == 0)
		panic("noemul_sendsig: no registers");
	oonstack = psp->ps_sigstk.ss_flags & SA_ONSTACK;
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct noemul_sigframe *)
			((vm_offset_t) psp->ps_sigstk.ss_base
			 + psp->ps_sigstk.ss_size
			 - sizeof (struct noemul_sigframe));
		psp->ps_sigstk.ss_flags |= SA_ONSTACK;
	} else
		fp = (struct noemul_sigframe *)
			(regs->uesp - sizeof (struct noemul_sigframe));

	frame.sf_signum = sig;
	frame.sf_code = code;
	frame.sf_scp = &fp->sf_sc;
	frame.sf_handler = catcher;
	frame.sf_eax = regs->eax;
	frame.sf_edx = regs->edx;
	frame.sf_ecx = regs->ecx;
	frame.sf_sc.sc_onstack = oonstack;
	frame.sf_sc.sc_mask = mask;
	frame.sf_sc.sc_sp = regs->uesp;
	frame.sf_sc.sc_fp = regs->ebp;
	frame.sf_sc.sc_ap = 0;
	frame.sf_sc.sc_pc = regs->eip;
	frame.sf_sc.sc_ps = regs->efl;

	if (copyout((caddr_t) &frame, (caddr_t) fp, sizeof (frame))) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		SIGACTION(p, SIGILL) = SIG_DFL;
		sig = sigmask(SIGILL);
		p->p_sigignore &= ~sig;
		p->p_sigcatch &= ~sig;
		p->p_sigmask &= ~sig;
		psignal(p, SIGILL);
		return;
	}
	regs->uesp = (unsigned int) fp;
	regs->eip = NOEMUL_SIGCODE_ADDR;
	pk->k_sigframe = 1;
}

/*
 * 4.4BSD-Lite2's i386 sigreturn(), NetBSD 1.0's call 103, made by the
 * sigcode above with the stack pointer at the frame: the scratch
 * registers from the frame, the rest from its context.  EJUSTRETURN:
 * the reply carries the registers as this left them.
 */
struct noemul_sigreturn_args {
	struct sigcontext *sigcntxp;
};

int
noemul_sigreturn(
	struct proc	*p,
	struct noemul_sigreturn_args *uap,
	int		*retval)
{
	proc_invocation_t pk = get_proc_invocation();
	struct i386_thread_state *regs =
		(struct i386_thread_state *) pk->k_trap_state;
	struct noemul_sigframe frame;
	struct sigcontext sc;

	if (regs == 0)
		panic("noemul_sigreturn: no registers");
	if (copyin((caddr_t) regs->uesp, (caddr_t) &frame, sizeof (frame)))
		return (EINVAL);
	if (copyin((caddr_t) frame.sf_scp, (caddr_t) &sc, sizeof (sc)))
		return (EINVAL);
	regs->eax = frame.sf_eax;
	regs->edx = frame.sf_edx;
	regs->ecx = frame.sf_ecx;
	if (sc.sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SA_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SA_ONSTACK;
	p->p_sigmask = sc.sc_mask & ~sigcantmask;
	regs->ebp = sc.sc_fp;
	regs->uesp = sc.sc_sp;
	regs->eip = sc.sc_pc;
	regs->efl = (regs->efl & ~NOEMUL_EFL_USER)
		| (sc.sc_ps & NOEMUL_EFL_USER);
	return (EJUSTRETURN);
}

/*
 * A caught signal for a process with no call in the server: its
 * thread is in user code, or on its way in with a call or a fault,
 * or waiting for the reply to one, or in the kernel for a page.  An
 * integrated kernel takes the signal at the thread's next return to
 * user mode.  With the emulator, Lites' psignal() calls send_signal()
 * (serv/serv_syscalls.c), which sends signal_notify (serv/signal.defs)
 * to the process's signal port; the emulator's signal thread
 * suspends the thread, aborts it, sends it to its signal entry and
 * resumes it (emulator/i386/e_signal.c, do_signal_notify()), and the
 * entry asks the server for the signal with bsd_take_signal() on the
 * process's port, which the server serves as a call (start_server_op()
 * with code 1000).
 *
 * Without the emulator the server is the receiver: the process's
 * signal port is a port of its own that the server receives on
 * (serv/proc_to_task.c, task_to_proc_enter(); POT_SIGRECV), so the
 * same notification comes to the server, and is served as
 * bsd_take_signal() is, as a call of the process, doing
 * do_signal_notify()'s steps itself and taking the signals into the
 * registers there.  The abort is the
 * OSF Mach 3 Kernel Interfaces' thread_abort_safely(), in its recipe
 * for a POSIX signal: thread_suspend, thread_abort_safely,
 * thread_set_state, thread_resume.  It will not abort "exception
 * processing", the system call's and the fault's RPC to this server
 * (OSF Mach Kernel Interface Changes, 1993: "Make thread_abort_safely
 * not abort exception sequences"; OSFMK 7.3, kern/thread_act.c), nor,
 * in OSFMK 7.3, a page fault, which waits uninterruptibly
 * (vm/vm_fault.c); "The caller of this function must then allow the
 * thread to resume and attempt to abort it later."  A call takes the
 * signals on its way out, as 4.4BSD's userret() does; a reply crossing
 * the signal, and a page wait, are retried as OSF/1 AD retried, "every
 * one second or so until the operation is interrupted (or completes
 * normally)" (Zajcew et al., USENIX Winter 1993): the notification is
 * sent again from the timer, as realitexpire() posts SIGALRM from it.
 * Only the process's first thread: NetBSD 1.0's programs have one.
 */
#define	NOEMUL_SIGRETRY	1000	/* ms: "every one second or so" */

extern kern_return_t signal_notify(mach_port_t, mach_port_t);

/*
 * send_signal()'s notification (serv/serv_syscalls.c, sendsig() in
 * serv/sendsig.c), from psignal() at splhigh, or from the timer: one
 * outstanding at a time, as SAS_NOTIFY_DONE marks it there.  A send
 * that fails after the message is built -- the port's queue full --
 * gives the rights in it back (OSFMK 7.3, ipc/mach_msg.c,
 * ipc_kmsg_copyout_pseudo()), and they are let go here; one that fails
 * before takes nothing but the thread's right made for it.  As
 * sendsig() does, a failure is named and not tried again: the signal
 * stays pending, and the next call of the process, or the next signal,
 * takes it.  The queue holds 5 (MACH_PORT_QLIMIT_DEFAULT) and holds
 * a notification or two at most (the flag), so it is not full of them.
 */
void
noemul_signal_running(struct proc *p)
{
	struct sigacts *ps = p->p_sigacts;
	thread_t thread = p->p_thread;
	kern_return_t kr;

	if ((ps->ps_flags & SAS_NOTIFY_DONE) || (p->p_flag & P_WEXIT)
	    || !MACH_PORT_VALID(p->p_sigport) || !MACH_PORT_VALID(thread))
		return;
	if (port_object_copy_send(thread) != KERN_SUCCESS)
		return;
	/*
	 * The flag goes up before the send: the server thread that takes
	 * the notification clears it, and one that did so before it was
	 * set (the timer sends without the master lock) would leave it
	 * set with nothing outstanding.
	 */
	ps->ps_flags |= SAS_NOTIFY_DONE;
	kr = signal_notify(p->p_sigport, thread);	/* moves the right */
	if (kr == KERN_SUCCESS)
		return;
	ps->ps_flags &= ~SAS_NOTIFY_DONE;
	printf("signal_notify failed pid=%d: %s\n",	/* as sendsig() */
	       p->p_pid, mach_error_string(kr));
	if (kr == MACH_SEND_TIMED_OUT || kr == MACH_SEND_INTERRUPTED)
		(void) mach_port_deallocate(mach_task_self(), p->p_sigport);
	(void) mach_port_deallocate(mach_task_self(), thread);
}

static void
noemul_signal_retry(void *arg)
{
	struct proc *p = (struct proc *) arg;
	int s = splhigh();		/* as psignal() notifies */

	if (!(p->p_flag & P_WEXIT) && p->p_task != MACH_PORT_NULL
	    && p->p_stat != SSTOP && HAVE_SIGNALS(p))
		noemul_signal_running(p);
	splx(s);
}

/* exit1()'s, as for realitexpire() (kern/kern_exit.c) */
void
noemul_signal_untimeout(struct proc *p)
{
	untimeout(noemul_signal_retry, (void *) p);
}

/*
 * The notification, on the process's signal port.  No reply: it is
 * a simpleroutine.
 */
static void
noemul_signal_notify(
	mach_msg_header_t	*InHeadP,
	mach_msg_header_t	*OutHeadP)
{
	typedef struct {		/* serv/signal_user.c's Request */
		mach_msg_header_t Head;
		mach_msg_body_t msgh_body;
		mach_msg_port_descriptor_t thread;
	} Request;
	Request *in = (Request *) InHeadP;
	mig_reply_error_t *out = (mig_reply_error_t *) OutHeadP;
	proc_invocation_t pk = get_proc_invocation();
	mach_msg_format_0_trailer_t *trailer;
	struct i386_thread_state ts;
	mach_msg_type_number_t count;
	struct proc *p;
	thread_t thread = MACH_PORT_NULL;
	kern_return_t kr;
	boolean_t pending, again = FALSE;

	out->Head.msgh_remote_port = MACH_PORT_NULL;	/* no reply */
	out->RetCode = MIG_NO_REPLY;
	if (MACH_PORT_VALID(in->Head.msgh_remote_port)) {
		/* a reply port, which signal_notify does not send */
		(void) mach_port_deallocate(mach_task_self(),
					    in->Head.msgh_remote_port);
		in->Head.msgh_remote_port = MACH_PORT_NULL;
	}
	if ((in->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)
	    && in->Head.msgh_size == sizeof (Request)
	    && in->msgh_body.msgh_descriptor_count == 1
	    && in->thread.type == MACH_MSG_PORT_DESCRIPTOR
	    && in->thread.disposition == MACH_MSG_TYPE_PORT_SEND)
		thread = in->thread.name;
	else if (in->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)
		mach_msg_destroy(InHeadP);	/* no reply port to destroy */

	trailer = (mach_msg_format_0_trailer_t *)
		((vm_offset_t) InHeadP + round_msg(InHeadP->msgh_size));
	p = (struct proc *) port_object_receive_lookup(InHeadP->msgh_local_port,
						       trailer->msgh_seqno,
						       POT_SIGRECV);
	pk->k_p = p;
	if (start_server_op(p, 1000) != 0) {	/* take_signal; unlocks p */
		pk->k_p = 0;
		goto out;
	}
	p = pk->k_p;
	unix_master();
	/* this one is taken: a signal from now on notifies again */
	p->p_sigacts->ps_flags &= ~SAS_NOTIFY_DONE;
	if (thread != p->p_thread)	/* not the process's, or stale */
		goto done;
	mutex_lock(&p->p_lock);
	pending = HAVE_SIGNALS(p);
	mutex_unlock(&p->p_lock);
	if (!pending || p->p_stat == SSTOP)	/* SIGCONT notifies again */
		goto done;
	if (p->p_servers_count > 1) {	/* a call takes them, or a reply crossed */
		again = TRUE;
		goto done;
	}

	if (thread_suspend(thread) != KERN_SUCCESS) {
		again = TRUE;
		goto done;
	}
	kr = thread_abort_safely(thread);
	if (kr == KERN_SUCCESS) {
		count = i386_THREAD_STATE_COUNT;
		kr = thread_get_state(thread, i386_THREAD_STATE,
				      (thread_state_t) &ts, &count);
		if (kr == KERN_SUCCESS) {
			pk->k_reply_msg = (mach_msg_header_t *) 1; /* copyout() to the task */
			pk->k_trap_state = (vm_offset_t) &ts;
			noemul_userret(p);
			pk->k_trap_state = 0;
			pk->k_reply_msg = 0;
			if (p->p_task != MACH_PORT_NULL
			    && !(p->p_flag & P_WEXIT))
				(void) thread_set_state(thread,
							i386_THREAD_STATE,
							(thread_state_t) &ts,
							i386_THREAD_STATE_COUNT);
		} else
			again = TRUE;
	} else
		again = TRUE;
	(void) thread_resume(thread);
done:
	if (again && p->p_task != MACH_PORT_NULL && !(p->p_flag & P_WEXIT)) {
		untimeout(noemul_signal_retry, (void *) p);
		timeout(noemul_signal_retry, (void *) p, NOEMUL_SIGRETRY);
	}
	unix_release();
	if (p->p_task == MACH_PORT_NULL)
		server_thread_deregister(p);	/* exited, as a call does */
	else
		(void) end_server_op(p, 0, (boolean_t *) 0);
	pk->k_p = 0;
out:
	if (MACH_PORT_VALID(thread))
		(void) mach_port_deallocate(mach_task_self(), thread);
}

/*
 * The pending signals, into the registers of a fault being answered
 * (serv/serv_syscalls.c, thread_signal()), as 4.4BSD's userret()
 * takes them after trapsignal().
 */
void
noemul_take_signals(struct proc *p)
{
	noemul_userret(p);
}

/*
 * fork() and vfork(): Lites' s_fork() (serv/serv_fork.c) makes the
 * child's task, a copy of the parent's memory (task_create(), with
 * inheritance), and its thread, and starts the thread with the
 * registers it is given, eax the parent's pid and edx 1, as 4.4BSD's
 * i386 cpu_fork() leaves the child; NetBSD's C stubs make that 0.
 * With the emulator those registers came from it; here they are the
 * parent's at the call, with the carry flag cleared, so the child
 * returns from the same call, successfully.  The parent gets the
 * child's pid, and edx 0.  vfork() is fork(): OSF/1's vfork() shares
 * memory as fork() does (OSF/1 1.0 manual, fork(2); docs/osf1-calls.md,
 * fork-15), so the parent does not wait for the child, as Lites'
 * vfork() would.  docs/roadmap.md, Tier 1 item 2, step 4.
 */
int
noemul_fork(
	struct proc	*p,
	void		*uap,
	int		*retval)
{
	proc_invocation_t pk = get_proc_invocation();
	struct i386_thread_state child;
	int pid, error;

	if (pk->k_trap_state == 0)
		panic("noemul_fork: no registers");
	child = *(struct i386_thread_state *) pk->k_trap_state;
	child.efl &= ~EFL_CF;
	error = s_fork(p, (thread_state_t) &child, i386_THREAD_STATE_COUNT,
		       &pid, FALSE);
	if (error)
		return (error);
	retval[0] = pid;
	retval[1] = 0;
	return (0);
}

/*
 * The exception_raise_state message (exc.defs, 2402) on a process's
 * request port: its sequence number is taken in turn, as for every
 * message on the port (port_object_receive_lookup()), and the
 * process is handed to exc_server()'s call above.  Only a process
 * port gets one: the server's other exception port is
 * EXCEPTION_STATE_IDENTITY, 2403 (kern/init_main.c).
 */
boolean_t
bsd_syscall_server(
	mach_msg_header_t	*InHeadP,
	mach_msg_header_t	*OutHeadP)
{
	proc_invocation_t pk = get_proc_invocation();
	mach_msg_format_0_trailer_t *trailer;
	mig_reply_error_t *out = (mig_reply_error_t *) OutHeadP;
	struct proc *p;

	if (InHeadP->msgh_id == 102000) {	/* signal_notify, serv/signal.defs */
		noemul_signal_notify(InHeadP, OutHeadP);
		return TRUE;
	}
	if (InHeadP->msgh_id != 2402)
		return FALSE;

	trailer = (mach_msg_format_0_trailer_t *)
		((vm_offset_t)InHeadP + round_msg(InHeadP->msgh_size));
	p = proc_receive_lookup(InHeadP->msgh_local_port,
				trailer->msgh_seqno);
	pk->k_p = p;
	/* not 0: copyout() and uiomove() see a user address space */
	pk->k_reply_msg = OutHeadP;
	pk->k_current_size = 0;

	(void) exc_server(InHeadP, OutHeadP);

	if (pk->k_p) {
		/* the stub refused it before the call: p still locked */
		mutex_unlock(&pk->k_p->p_lock);
		pk->k_p = 0;
	}
	pk->k_reply_msg = 0;

	/*
	 * The turn is taken: a stub's type check failure must not make
	 * ux_server_loop() take another (its MIG_BAD_ARGUMENTS case).
	 */
	if (out->RetCode == MIG_BAD_ARGUMENTS) {
		mach_port_t reply_port = InHeadP->msgh_remote_port;

		out->RetCode = KERN_INVALID_ARGUMENT;
		InHeadP->msgh_remote_port = MACH_PORT_NULL;
		mach_msg_destroy(InHeadP);
		InHeadP->msgh_remote_port = reply_port;
	}
	return TRUE;
}

#endif /* NO_EMULATOR */
