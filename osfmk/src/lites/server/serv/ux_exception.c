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
 * HISTORY
 * 12-Oct-95  Ian Dall (dall@hfrd.dsto.gov.au)
 *	Make catch_exception_raise return unsuccessful status if the
 *	exception is handled by the task (caught signal).
 *
 * 20-Jan-94  Johannes Helander (jvh) at Helsinki University of Technology
 *	Default to signal zero on unknown exceptions and EXC_SOFTWARE codes.
 *
 * $Log: ux_exception.c,v $
 * Revision 1.2  1996/03/14  21:08:38  sclawson
 * Ian Dall's signal fixes.
 *
 * Revision 1.1.1.2  1995/03/23  01:17:00  law
 * lites-950323 from jvh.
 *
 * Revision 2.2  93/02/26  12:56:35  rwd
 * 	Include sys/systm.h for printf prototypes.
 * 	[92/12/09            rwd]
 * 
 * Revision 2.1  92/04/21  17:11:14  rwd
 * BSDSS
 * 
 *
 */

#include "no_emulator.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <mach/exception.h>
#include <sys/ux_exception.h>

#include <serv/import_mach.h>

/*
 *	Unix exception handler.
 */

/*
 * Returns exception port to map exceptions to signals.
 */
mach_port_t ux_handler_setup()
{
  	kern_return_t	r;
	mach_port_t	ux_local_port;

	/*
	 *	Allocate the exception port.
	 */
	r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
			       &ux_local_port);
	if (r != KERN_SUCCESS)
		panic("ux_handler_setup: can't allocate");

	r = mach_port_insert_right(mach_task_self(),
				   ux_local_port, ux_local_port,
				   MACH_MSG_TYPE_MAKE_SEND);
	if (r != KERN_SUCCESS)
		panic("ux_handler_setup: can't acquire send right");

	/*
	 * Add it to the server port set.
	 */
	ux_server_add_port(ux_local_port);

	/*
	 * Return the exception port.
	 */

	return (ux_local_port);
}

#if OSFMACH3
/*
 * AI-ONLY NOTE: new.  TRUE if page 0 of task is mapped with some
 * access -- a program's text, not mach_init()'s VM_PROT_NONE
 * reservation -- so that ux_exception()'s page-0 rule, SIGSEGV, does
 * not apply to a fault there.  Asked only for a protection failure in
 * page 0.  The vm_region() call is serv/device_misc.c's.
 */
static boolean_t ux_page0_mapped(task_t task)
{
	struct vm_region_basic_info r;
	mach_msg_type_number_t icount = VM_REGION_BASIC_INFO_COUNT;
	vm_offset_t addr = 0;
	vm_size_t size;
	mach_port_t object_name = MACH_PORT_NULL;

	if (vm_region(task, &addr, &size, VM_REGION_BASIC_INFO,
		      (vm_region_info_t) &r, &icount, &object_name)
	    != KERN_SUCCESS)
		return FALSE;
	if (MACH_PORT_VALID(object_name))
		(void) mach_port_deallocate(mach_task_self(), object_name);
	return addr == 0 && r.max_protection != VM_PROT_NONE;
}

static void ux_page0_fixup(task_t task, exception_type_t exception,
			   integer_t code, integer_t subcode, int *signal)
{
	if (exception == EXC_BAD_ACCESS && code == KERN_PROTECTION_FAILURE
	    && (vm_offset_t) subcode < vm_page_size && *signal == SIGSEGV
	    && ux_page0_mapped(task))
		*signal = SIGBUS;
}
#endif /* OSFMACH3 */

/* forward */
boolean_t thread_psignal(task_t, thread_t, int, int, thread_state_t,
			 mach_msg_type_number_t);
#if OSFMACH3
extern thread_state_flavor_t machine_exception_flavor;
#endif
void ux_exception(integer_t exception, integer_t code, integer_t subcode,
		  int *ux_signal, int *ux_code);

/*
 * AI-ONLY NOTE: under OSFMACH3, OSFMK 7.3's exc.defs signature: the
 * codes come as an array and its count, (exception, code[], codeCnt),
 * where CMU's passed (exception, code, subcode).  Read with CMU's,
 * code was the array's address and subcode its count, so
 * KERN_INVALID_ADDRESS never matched and every bad access was SIGBUS
 * (docs/deferred.md, S-3).  OSFMK's own servers take it so
 * (bootstrap/bootstrap.c, catch_exception_raise).
 */
#if OSFMACH3
kern_return_t catch_exception_raise(
	mach_port_t		exception_port,
	thread_t		thread,
	task_t			task,
	exception_type_t	exception,
	exception_data_t	codes,
	mach_msg_type_number_t	codeCnt)
{
	integer_t code = codeCnt > 0 ? codes[0] : 0;
	integer_t subcode = codeCnt > 1 ? codes[1] : 0;
#else /* OSFMACH3 */
kern_return_t catch_exception_raise(
	mach_port_t		exception_port,
	thread_t		thread,
	task_t			task,
	integer_t		exception,
	integer_t		code,
	integer_t		subcode)
{
#endif /* OSFMACH3 */
	int	signal = 0, ux_code = 0;
	int	ret = KERN_SUCCESS;

	/*
	 *	Catch bogus ports
	 */
	if (MACH_PORT_VALID(task) && MACH_PORT_VALID(thread)) {

	    /*
	     *	Convert exception to unix signal and code.
	     */
	    ux_exception(exception, code, subcode, &signal, &ux_code);
#if OSFMACH3
	    ux_page0_fixup(task, exception, code, subcode, &signal);
#endif

	    /*
	     *	Send signal.
	     */
	    if (signal != 0) {
		    if (thread_psignal(task, thread, signal, ux_code,
				       (thread_state_t) 0, 0)) {
		      /* The tasks signal thread has been notified and
		       * it will unblock the thread which got the exception
		       * so we don't want exc_server() reply.
		       */
		      ret = MIG_NO_REPLY;
		    }
	    }
	} else {
	    printf("catch_exception_raise: task %x thread %x\n",
		   task, thread);
	    ret = KERN_INVALID_ARGUMENT;
	}

	if (MACH_PORT_VALID(task))
		(void) mach_port_deallocate(mach_task_self(), task);

	if (MACH_PORT_VALID(thread))
		(void) mach_port_deallocate(mach_task_self(), thread);

	return(ret);
}

#if OSFMACH3
/*
 * AI-ONLY NOTE: the two state routines take OSFMK 7.3's exc.defs
 * arguments, which the stubs did not: a code array and count, the
 * flavour, and each state with its count.  EXCEPTION_STATE, without
 * the thread and task, names no process, so it stays a stub.
 */
#if !NO_EMULATOR
/*
 * AI-ONLY NOTE: the server without the emulator has its own, the
 * system call (i386/syscall.c), sent to a process's port.
 */
kern_return_t catch_exception_raise_state(
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
	panic("catch_exception_raise_state");
}
#endif /* !NO_EMULATOR */

/*
 * AI-ONLY NOTE: new; was a panic stub.  The exception port is
 * registered EXCEPTION_STATE_IDENTITY (kern/init_main.c), OSFMK's
 * form in which the reply carries the thread's new state.  OSFMK's
 * exception() resumes a faulting thread only by a reply: one aborted
 * out of the exception RPC, as the emulator's do_signal_notify()
 * does, goes on to the task's handler and then exception_no_server(),
 * which terminates the task -- where Mach 3 and Mach4 resumed it
 * (docs/deferred.md, S-7).  So a signal the process catches is sent
 * here, in the reply: thread_psignal() is given the state, and
 * machine_signal_redirect() points it at the emulator's signal entry
 * with the old state on the stack, as do_signal_notify() would have.
 * Any other outcome replies the state unchanged.
 */
kern_return_t catch_exception_raise_state_identity(
	mach_port_t		port,
	mach_port_t		thread,
	mach_port_t		task,
	exception_type_t	exception,
	exception_data_t	code,
	mach_msg_type_number_t	codeCnt,
	int			*flavor,
	thread_state_t		old_state,
	mach_msg_type_number_t	old_stateCnt,
	thread_state_t		new_state,
	mach_msg_type_number_t	*new_stateCnt)
{
	int	signal = 0, ux_code = 0;
	kern_return_t ret = KERN_SUCCESS;

	bcopy((char *) old_state, (char *) new_state,
	      old_stateCnt * sizeof (natural_t));
	*new_stateCnt = old_stateCnt;

	if (MACH_PORT_VALID(task) && MACH_PORT_VALID(thread)) {
	    ux_exception(exception,
			 codeCnt > 0 ? code[0] : 0,
			 codeCnt > 1 ? code[1] : 0,
			 &signal, &ux_code);
	    ux_page0_fixup(task, exception,
			   codeCnt > 0 ? code[0] : 0,
			   codeCnt > 1 ? code[1] : 0, &signal);
	    if (signal != 0)
		(void) thread_psignal(task, thread, signal, ux_code,
				      *flavor == machine_exception_flavor ?
				      new_state : (thread_state_t) 0,
				      *new_stateCnt);
	} else {
	    printf("catch_exception_raise_state_identity: task %x thread %x\n",
		   task, thread);
	    ret = KERN_INVALID_ARGUMENT;
	}

	if (MACH_PORT_VALID(task))
		(void) mach_port_deallocate(mach_task_self(), task);
	if (MACH_PORT_VALID(thread))
		(void) mach_port_deallocate(mach_task_self(), thread);

	return ret;
}
#endif /* OSFMACH3 */

boolean_t machine_exception(integer_t, integer_t, integer_t, int *, int *);

/*
 *	ux_exception translates a mach exception, code and subcode to
 *	a signal and u.u_code.  Calls machine_exception (machine dependent)
 *	to attempt translation first.
 */

void ux_exception(integer_t exception, integer_t code, integer_t subcode,
		  int *ux_signal, int *ux_code)
{
	/*
	 *	Try machine-dependent translation first.
	 */
	if (machine_exception(exception, code, subcode, ux_signal, 
	    ux_code))
		return;
	
	switch(exception) {

	    case EXC_BAD_ACCESS:
		if (code == KERN_INVALID_ADDRESS)
		    *ux_signal = SIGSEGV;
#if OSFMACH3
		/*
		 * AI-ONLY NOTE: a protection failure in the first page is
		 * SIGSEGV.  OSFMK's mach_init() reserves page 0
		 * VM_PROT_NONE "so that the program doesn't get it", so a
		 * NULL dereference is a protection failure here, where on
		 * NetBSD 1.0 -- whose binaries start at __LDPGSZ and leave
		 * page 0 unmapped -- it is an invalid address.  The rule
		 * below is every BSD trap()'s; the reserved page is not
		 * part of the process's BSD address space, so a reference
		 * to it is to an address the process does not have.
		 * subcode is the fault address (catch_exception_raise*'s
		 * codes[1]).  Where page 0 holds the program's text --
		 * 386BSD and Linux binaries load at 0, and mach_init()'s
		 * reservation then fails -- the callers undo this, with
		 * ux_page0_mapped().  docs/deferred.md, S-4.
		 */
		else if (code == KERN_PROTECTION_FAILURE &&
			 (vm_offset_t) subcode < vm_page_size)
		    *ux_signal = SIGSEGV;	/* see ux_page0_mapped() */
#endif /* OSFMACH3 */
		else
		    *ux_signal = SIGBUS;
		break;

	    case EXC_BAD_INSTRUCTION:
	        *ux_signal = SIGILL;
		break;

	    case EXC_ARITHMETIC:
	        *ux_signal = SIGFPE;
		break;

	    case EXC_EMULATION:
		*ux_signal = SIGEMT;
		break;

	    case EXC_SOFTWARE:
		switch (code) {
		    case EXC_UNIX_BAD_SYSCALL:
			*ux_signal = SIGSYS;
			break;
		    case EXC_UNIX_BAD_PIPE:
		    	*ux_signal = SIGPIPE;
			break;
		    case EXC_UNIX_ABORT:
			*ux_signal = SIGABRT;
			break;
		    default:
			printf("ux_exception: unknown EXC_SOFTWARE code x%x\n",
			       code);
			*ux_signal = 0;
			break;
		}
		break;

	    case EXC_BREAKPOINT:
		*ux_signal = SIGTRAP;
		break;
	    default:
		printf("ux_exception: unknown exception x%x code x%x\n",
		       exception, code);
		*ux_signal = 0;
		break;
	}
}
