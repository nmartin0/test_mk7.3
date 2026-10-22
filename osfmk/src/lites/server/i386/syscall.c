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
#include <mach/exception.h>
#include <mach/error.h>
#include <device/device_types.h>
#include <i386/eflags.h>

#if NO_EMULATOR

extern struct sysent	noemul_sysent[];
extern int		noemul_nsysent;

extern boolean_t exc_server(mach_msg_header_t *, mach_msg_header_t *);

#define	LCALL_SIZE	7		/* lcall $0x7,$0x0 */
#define	NOEMUL_SYS_syscall	0	/* 4.4BSD's numbers, not the */
#define	NOEMUL_SYS___syscall	198	/* server's <sys/syscall.h> */
#define	NOEMUL_MAXARGS	8		/* most words a call takes */

/*
 * The thread's registers as a program starts: pc and stack pointer
 * set, the general registers zeroed, as the emulator starts one
 * (emulator/i386/e_trampoline.c, emul_exec_start), the flags as
 * set_emulator_state() leaves them.  The segment registers are kept.
 */
void
noemul_set_program_state(
	struct proc	*p,
	vm_offset_t	pc,
	vm_offset_t	sp)
{
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
	mach_msg_type_number_t count;

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
			unix_master();
			noemul_userret(p);
			unix_release();
			if (p->p_task == MACH_PORT_NULL) {
				server_thread_deregister(p);
				pk->k_p = 0;
				return MIG_NO_REPLY;
			}
		}
	}
	if (error) {
		pk->k_p = 0;
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
			/* registers set by the call: execve */
			count = i386_THREAD_STATE_COUNT;
			if (p->p_task == task
			    && thread_get_state(p->p_thread,
						i386_THREAD_STATE,
						new_state, &count)
			       == KERN_SUCCESS)
				*new_stateCnt = count;
			break;
		      default:
			regs->efl |= EFL_CF;
			regs->eax = noemul_errno(p, syscode, error);
			break;
		}
		noemul_userret(p);
	}
	unix_release();

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
