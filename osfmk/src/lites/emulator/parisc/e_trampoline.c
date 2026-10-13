/* 
 * Copyright (c) 1990, 1991, 1992, 1994, The University of Utah and
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
 * 	Utah $Hdr: e_trampoline.c 1.5 95/03/03$
 */

#define KERNEL
#include <sys/errno.h>
#undef KERNEL

#include <e_defs.h>
#include <syscall_table.h>
#include <parisc/eregs.h>
#include <e_hpux.h>

int	emul_low_entry = -9;
int	emul_high_entry = 324;
boolean_t with_kframe = FALSE;

/*
 * Used for debugging system calls.
 */
static	char scalldbg_prf[] =
		"emul_syscall[%d] %s(x%x, x%x, x%x, x%x, x%x, x%x, x%x, x%x)";
static	char *scalldbg_arg = scalldbg_prf + 20;
extern	int e_nosys();

/*
 * If EBADF_COMPRESS is #define'd and EMULATOR_DEBUG < 4 compress EBADF
 * error returns while BADF_ISFUNC(function) remains true.
 * In other words, when a process loops calling close() on fds 0 - 127,
 * we see one call and a message saying how many were ignored.
 *
 * Caveat: While ignoring BADF_ISFUNC(function)'s, if we crash/hang in
 * said function, we will certainly not see the function call...
 */
#define	EBADF_COMPRESS

#ifdef	EBADF_COMPRESS
extern	int e_close(), e_shutdown();
static	char badf_scalldbg_prf[] = "err_return[%d] ignoring %d EBADFs...";
static	int badf_syscode = 0, badf_syscnt = 0;
#define	BADF_ERRNO	(EBADF & 0xff)
#define	BADF_CLEAR	{						\
	if (syscall_debug > 1 && badf_syscnt)				\
		e_emulator_error(badf_scalldbg_prf,badf_syscode,badf_syscnt); \
	badf_syscnt = badf_syscode = 0;					\
}
#define	BADF_ISFUNC(f)	((f) == e_close || (f) == e_shutdown)
#endif

extern emul_trampoline();

errno_t e_htg_syscall()
{
        return e_mach_error_to_errno(LITES_ENOSYS);
}


binary_type_t e_my_binary_type;

int current_nsysent = 0;
struct sysent *current_sysent = 0;

void emul_setup(task_t task, enum binary_type binary_type)
{
	int i;
	kern_return_t	rc;

	for (i = emul_low_entry; i <= emul_high_entry; i++)
		(void) task_set_emulation(task,
					  (vm_address_t) emul_trampoline, i);

	/* XXX do we want/need these?  for mach_init? */
	(void) task_set_emulation(task, (vm_address_t) emul_trampoline, -33);
	(void) task_set_emulation(task, (vm_address_t) emul_trampoline, -34);
	(void) task_set_emulation(task, (vm_address_t) emul_trampoline, -41);
	(void) task_set_emulation(task, (vm_address_t) emul_trampoline, -52);

	switch (binary_type) {
	case BT_HPBSD:
		current_nsysent = e_hpbsd_nsysent;
		current_sysent = e_hpbsd_sysent;
		break;
	case BT_ELF:
		current_nsysent = e_bsd_nsysent;
		current_sysent = e_bsd_sysent;
		break;
	case BT_HPUX:
		current_nsysent = e_hpux_nsysent;
		current_sysent = e_hpux_sysent;
		__e_hpux_exrest(&__hpux_exsave, sizeof(__hpux_exsave));
		break;
	default:
		emul_panic("emul_setup: unsupported binary type %d",
			   binary_type);
	}
}

/*
 * System calls enter here.
 */
int
emul_syscall(
	     int		a0,
	     int		a1,
	     int		a2,
	     int		a3,
	     struct emul_regs	*regs)
{
	int a4, a5, a6, a7;
	int syscode;
	int error = 0;
	struct sysent *callp;
	int rval[2];
	boolean_t interrupt = FALSE;
	int args[8], *nextarg;
	int extra_args;

	nextarg = regs->argp - 4;	/* additional args are BEFORE this */
	syscode = regs->scnum;

#if defined(MAP_UAREA) && 0
	if (shared_enabled) {
		if (shared_base_ro->us_cursig) {
			error = ERESTART;
			goto signal;
		}
	}
#endif	MAP_UAREA

	extra_args = 0;
	if (syscode == 0) {
		/*
		 * Indirect system call.
		 */
		syscode = a0;
		a0 = a1;
		a1 = a2;
		a2 = a3;
		a3 = *--nextarg;
	} else if (syscode == 198) {
		/*
		 * Code is a quad argument.
		 * Ugh!  The PA is a big endian machine, *except* when
		 * it comes to passing quads in registers; the low
		 * order bits are in the higher register number!
		 */
		syscode = a0;
		if (syscode != SYS_lseek)
		a0 = a2;
		a1 = a3;
		a2 = *--nextarg;
		a3 = *--nextarg;
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
			callp = &sysent_table;
		else if (syscode == -34)
			callp = &sysent_pid_by_task;
		else if (syscode == -41)
			callp = &sysent_init_process;
		else if (syscode == -59)
			callp = &sysent_htg_ux_syscall;
		else
			callp = &null_sysent;
	}

	if (callp->nargs < 0 || callp->nargs >= 5) {
		a4 = *--nextarg;
		a5 = *--nextarg;
		a6 = *--nextarg;
		a7 = *--nextarg;
		/*
		 * Forward order for the sake of E_GENERIC and E_CHANGE_REGS
		 */
		if (callp->nargs < 0) {
			args[0] = a0;
			args[1] = a1;
			args[2] = a2;
			args[3] = a3;
			args[4] = a4;
			args[5] = a5;
			args[6] = a6;
			args[7] = a7;
		}
	} else {
		a4 = 0;
		a5 = 0;
		a6 = 0;
		a7 = 0;
	}

	if (syscall_debug > 1) {
		char sav1, sav2, *cp = scalldbg_arg;
		int nosys = 0;

		if (callp->routine == e_nosys) {
			nosys = 1;
			callp->nargs = 7;
		}
#ifdef	EBADF_COMPRESS
		if (badf_syscode) {
			if (BADF_ISFUNC(callp->routine))
				badf_syscnt++;
			else
				BADF_CLEAR;
		}
		if (nosys || syscall_debug > 3 ||
		    (syscall_debug > 2 && !badf_syscode))
#else
		if (nosys || syscall_debug > 2)
#endif
		{
			if (callp->nargs > 0)
				cp += (callp->nargs * 5) - 2;
			sav1 = *cp; sav2 = *(cp + 1);
			*cp = ')'; *(cp + 1) = '\0';
			e_emulator_error(scalldbg_prf, syscode, callp->name,
				a0, a1, a2, a3, a4, a5, a6, a7);
			*cp = sav1; *(cp + 1) = sav2;
		}
	}

	/*
	 * Set up the initial return values.
	 */
	rval[0] = rval[1] = 0;

	/*
	 * Call the routine, passing arguments according to the table entry.
	 */
	switch (callp->nargs) {
	case 0:
		error = (*callp->routine)(rval);
		break;
	case 1:
		error = (*callp->routine)(a0, rval);
		break;
	case 2:
		error = (*callp->routine)(a0, a1, rval);
		break;
	case 3:
		error = (*callp->routine)(a0, a1, a2, rval);
		break;
	case 4:
		error = (*callp->routine)(a0, a1, a2, a3, rval);
		break;
	case 5:
		error = (*callp->routine)(a0, a1, a2, a3, a4, rval);
		break;
	case 6:
		error = (*callp->routine)(a0, a1, a2, a3, a4, a5, rval);
		break;
	case 7:
		error = (*callp->routine)(a0, a1, a2, a3, a4, a5, a6, rval);
		break;
	case 8:
		error = (*callp->routine)(a0, a1, a2, a3, a4, a5, a6, a7, rval);
		break;

	case E_GENERIC:		/* generic */
		error = (*callp->routine)(syscode, args, rval);
		break;

	case E_CHANGE_REGS:	/* pass registers to modify */
		error = (*callp->routine)(args, rval, regs);
		break;
	}

#ifdef  EBADF_COMPRESS
	/*
	 * In the rare event where, after a string of EBADF returns we
	 * get something else, reset badf and (if necessary) display
	 * current system call arguments after-the-fact.
	 */
	if (badf_syscode && error != BADF_ERRNO) {
		BADF_CLEAR;
		if (syscall_debug > 2)
			e_emulator_error(scalldbg_prf, syscode,
				callp->name, a0, a1, a2, a3, a4, a5, a6, a7);
	}
#endif

	/*
	 * Set up return values.
	 */

#ifdef	MAP_UAREA
signal:
#endif	MAP_UAREA

	switch (error) {
	case EJUSTRETURN:
		/* Do not alter registers */
		if (syscall_debug > 1)
			e_emulator_error("JUSTRETURN[%d] %s",
					 syscode, callp->name);
		break;

	case ERESTART:
		/*
		 * Restart call.
		 * Stash arguments where they can be reloaded and reset
		 * the return offset to redo the call.
		 */
		regs->flags |= SB_RESTART;
		nextarg = regs->argp;
		if (regs->scnum == 0 && syscode != 0) {
			*--nextarg = syscode;
			*--nextarg = a0;
			*--nextarg = a1;
			*--nextarg = a2;
		} if (regs->scnum == 198) {
			*--nextarg = syscode;
			*--nextarg = 0;
			*--nextarg = a0;
			*--nextarg = a1;
		} else {
			*--nextarg = a0;
			*--nextarg = a1;
			*--nextarg = a2;
			*--nextarg = a3;
		}
		regs->reto -= 3;
		if (syscall_debug > 1)
			e_emulator_error("RESTART[%d] %s",
					 syscode, callp->name);
		break;

	case 0:
		/* Success */
		regs->scnum = 0;
		regs->ret1 = rval[1];
		if (syscall_debug > 2)
			e_emulator_error("return[%d] %s = (x%x x%x)",
					 syscode, callp->name,
					 rval[0], rval[1]);
		break;

	default:
		/* error */
		regs->scnum = 1;
		rval[0] = error;
#ifdef  EBADF_COMPRESS
		if (syscall_debug > 1 && !badf_syscode) {
			e_emulator_error("err_return[%d] %s -> %d",
				syscode, callp->name, error);
			if (syscall_debug <= 3 && error == BADF_ERRNO &&
			    BADF_ISFUNC(callp->routine))
				badf_syscode = syscode;
		}
#else
		if (syscall_debug > 1)
			e_emulator_error("err_return[%d] %s -> %d",
					 syscode, callp->name, error);
#endif
		break;
	}

	/*
	 * Handle interrupt request.
	 */
	e_checksignals(&interrupt);

	if (error == ERESTART || error == EINTR || interrupt) {
		take_signals_and_do_sigreturn(regs);
		if (syscall_debug > 1)
			e_emulator_error("took sig: rv=%d, dp=%x, rp=%x, ret=%x.%x",
					 rval[0], regs->dp, regs->rp,
					 regs->rets, regs->reto);
	}

	return rval[0];
}
