/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
 * CARNEGIE MELLON AND JOHANNES HELANDER AND IAN DALL ALLOW FREE USE
 * OF THIS SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND
 * JOHANNES HELANDER AND IAN DALL DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/* 
 * HISTORY
 * 02-Jan-95  Ian Dall (dall@hfrd.dsto.gov.au)
 *	Renamed ns532_e_wait to e_machine_wait.
 *
 * $Log: e_trampoline.c,v $
 * Revision 1.1.1.2  1995/03/23  01:15:36  law
 * lites-950323 from jvh.
 *
 *
 */
/* 
 *	File:	emulator/ns532/e_trampoline.c
 *	Authors:
 *	Johannes Helander, Helsinki University of Technology, 1994.
 *	Ian Dall, Adelaide, 1994.
 *	Origin:	Derived from emulator/i386/e_trampoline.c
 *
 *	Handler for emulated BSD system calls.
 */

#define KERNEL
#include <sys/errno.h>
#undef KERNEL

#include <e_defs.h>
#include <syscall_table.h>
#include <ns532/psl.h>

int	emul_low_entry = -9;
int	emul_high_entry = 210;   /* As big as the largest sysent table.
				  * It doesn`t matter if this is too big.
				  * The calls are checked against the table
				  * size
				  */

extern emul_trampoline();

int current_nsysent = 0;
struct sysent *current_sysent = 0;

void emul_setup(task_t task, enum binary_type binary_type)
{
	int i;
	kern_return_t	rc;

	if (syscall_debug > 1)
	  e_emulator_error("emul_setup: type %d\n", binary_type);
	for (i = emul_low_entry;
	     i <= emul_high_entry;
	     i++) {
		rc = task_set_emulation(task,
					(vm_address_t) emul_trampoline,
					i);
	}
	rc = task_set_emulation(task,
			(vm_address_t) emul_trampoline,
			-33);
	rc = task_set_emulation(task,
			(vm_address_t) emul_trampoline,
			-34);
	rc = task_set_emulation(task,
			(vm_address_t) emul_trampoline,
			-41);
	rc = task_set_emulation(task,
			(vm_address_t) emul_trampoline,
			-52);

	switch (binary_type) {
	      case BT_CMU_43UX:
		current_nsysent = e_cmu_43ux_nsysent;
		current_sysent = e_cmu_43ux_sysent;
		break;
	      case BT_386BSD:
	      case BT_NETBSD:
	      case BT_FREEBSD:
	      default:
		current_nsysent = e_bsd_nsysent;
		current_sysent = e_bsd_sysent;
		break;
	}

}

struct emul_regs {
  int sb;
  int fp;
  int r7;
  int r6;
  int r5;
  int r4;
  int r3;
  
  int r2;
  int r1;
  int r0;
  int psr;
  int pc;
};

errno_t e_htg_syscall()
{
	return e_mach_error_to_errno(LITES_ENOSYS);
}

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

#define	PSR_ALLCC	(PSR_N|PSR_Z|PSR_F|PSR_L|PSR_C)
#define WAIT_ANY (-1)

	if ((regs->psr & PSR_ALLCC) == PSR_ALLCC) {
	    err = e_bnr_wait4(WAIT_ANY, &istat, regs->r1 /* options */,
			      (struct rusage *) regs->r2 /* rusage_p */, &wpid);
	} else {
	    err = e_bnr_wait4(WAIT_ANY, &istat, 0, 0, &wpid);
	}
	rval[0] = wpid;
	rval[1] = istat;	/* sic */
	return err;
}

#if defined(COMPAT_43) && 0
/* 
 * Compatibility with historical brain damage.
 * Fortunately new code doesn't use sigvec so it is not necessary to
 * be compatible with that simultaneously.
 */
int ns532_e_sigvec(
	int			*argp,
	int			*rval,
	struct emul_regs	*regs)
{
	return e_bnr_sigvec((int) argp[0],
			    (struct sigvec *) argp[1],	/* nsv */
			    (struct sigvec *) argp[2],	/* osv */
			    (vm_address_t) regs->r1 & ~0x80000000); /* tramp */
}

int OLD_ns532_e_sigvec(
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
			   regs->r1 & ~0x80000000); /* trampoline */
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
	    rval[1] = 0;	/* parent r0 = 0 */
	else
	    rval[1] = 1;	/* child r0 = 1 */
	return ESUCCESS;
}

errno_t ns532_e_htg_syscall()
{
	return e_mach_error_to_errno(LITES_ENOSYS);
}

/*
 * System calls enter here.
 */
void
emul_syscall(struct emul_regs *regs)
{
	int syscode;
	int error = 0;
	struct sysent *callp;
	int rval[2];
	boolean_t interrupt = FALSE;
	int *args;

	args = (int *)(regs+1);	/* args on stack top */
	args++;	/* point to first argument - skip return address */

	syscode = regs->r0;

#if defined(MAP_UAREA) && 0
	if (shared_enabled) {
		if (shared_base_ro->us_cursig) {
			error = ERESTART;
			goto signal;
		}
	}
#endif	MAP_UAREA
#if defined(COMPAT_43)
	/* Compat with ns532 UX binaries. Conflict with uname */
	if ((syscode == 164) && (e_my_binary_type == BT_CMU_43UX)) {
		syscode = SYS_getdirentries; /* 156 */
	}
#endif /* MAP_UAREA && COMPAT_43 */

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
		callp = &e_cmu_43ux_sysent[SYS_table];
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
	    e_emulator_error("emul_syscall[%d] %s(x%x, x%x, x%x, x%x, x%x, x%x, x%x, x%x)",
			     syscode, callp->name, args[0], args[1], args[2],
			     args[3], args[4], args[5], args[6], args[7]);
	/*
	 * Set up the initial return values.
	 */
	rval[0] = 0;
	rval[1] = regs->r1;

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

#ifdef	MAP_UAREA
signal:
#endif	MAP_UAREA

	switch (error) {
	    case EJUSTRETURN:
		if (syscall_debug > 1)
		    e_emulator_error("JUSTRETURN[%d] %s", syscode,callp->name);
		/* Do not alter registers */
		break;

	    case 0:
		/* Success */
		regs->psr &= ~PSR_C;
		regs->r0 = rval[0];
		regs->r1 = rval[1];
		if (syscall_debug > 2)
		    e_emulator_error("return[%d] %s = (x%x x%x)",
				     syscode, callp->name, rval[0], rval[1]);
		break;

	    case ERESTART:
		if (syscall_debug > 1)
		    e_emulator_error("RESTART[%d] %s", syscode, callp->name);
		/* restart call */
		regs->pc -= 1;	/* XXX goto again */
		break;

	    default:
		/* error */
		regs->psr |= PSR_C;
		regs->r0 = error;
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
	    take_signals_and_do_sigreturn();
}

/* Additional junk below argc is on the stack */
boolean_t with_kframe = TRUE;

errno_t emul_exec_start(
	int			*kframe,
	enum binary_type	binary_type,
	int			argc,
	char			*argv[],
	char			*envp[],
	struct exec_load_info	*li)
{
	struct ns532_thread_state state;
	int x;

	if (syscall_debug > 1)
	  e_emulator_error("emul_exec_start: entry = 0x%x\n", li->pc);
	if (x = emul_save_state(&state))
	    exit(x);

	state.pc = li->pc;
	state.sp = (unsigned int) kframe;

	state.psr = 0;
	state.sb = 0;

	/* Zero other registers ? */

	if (syscall_debug > 1)
	    e_emulator_error("emul_exec_start: starting at x%x k=x%x (x%x x%x x%x x%x)",
			     li->pc, kframe, kframe[0], kframe[1],
			     kframe[2], kframe[3]);
	emul_load_state(&state);
	/*NOTREACHED*/
}
