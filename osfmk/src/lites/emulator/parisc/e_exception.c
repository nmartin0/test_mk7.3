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
 * 	Utah $Hdr: e_exception.c 1.5 95/03/02$
 */

#include <e_defs.h>
#include <mach/exception.h>

extern exc_server();
void port_consume(mach_port_t);

noreturn
e_thread_kill_self()
{
	(noreturn) thread_terminate(mach_thread_self());
}

void
e_fork_a_thread(void (*func)())
{
	kern_return_t kr;
	vm_address_t stack_start;
	thread_t thread;
	struct parisc_thread_state state;
	vm_size_t stack_size = vm_page_size * 16;

	/* Make stack. Keep it away from sbrk! */
	for (stack_start = EMULATOR_END;
	     stack_start < VM_MAX_ADDRESS;
	     stack_start += stack_size)
	{
		kr = vm_allocate(mach_task_self(),
				 &stack_start,
				 stack_size,
				 FALSE);
		if (kr == KERN_SUCCESS)
		    break;
	}
	if (kr != KERN_SUCCESS)
	    emul_panic("e_fork_a_thread allocate stack");

	/* Make initial frame */
	stack_start += 64;

	/* Set initial register values */
	emul_save_state(&state);
	state.r2 = (int) e_thread_kill_self;
	state.r30 = stack_start;
	state.iioq_head = (int) func;
	state.iioq_tail = state.iioq_head + 4;

	/* Fork off a thread */
	kr = thread_create(mach_task_self(), &thread);
	if (kr)
		emul_panic("e_fork_a_thread thread_create");
	

	/* Write the state into the registers */
	kr = thread_set_state(thread,
			      PARISC_THREAD_STATE,
			      (thread_state_t) &state,
			      PARISC_THREAD_STATE_COUNT);
	if (kr)
		emul_panic("e_fork_a_thread thread_set_state");

	/* And off it goes */
	kr = thread_resume(thread);
	if (kr)
		emul_panic("e_fork_a_thread thread_resume");
}


mach_port_t original_exception_port = MACH_PORT_NULL;
mach_port_t e_exception_port = MACH_PORT_NULL;

/* 
 * e_exception_setup is always called by the original thread.
 */
void e_exception_setup()
{
	kern_return_t kr;

#if 0
	/* Called by child_init after fork as well */
	if (original_exception_port != MACH_PORT_NULL)
		emul_panic("e_exception_setup called twice");
#endif

	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
				&e_exception_port);
	if (kr)
		emul_panic("e_exception_setup mach_port_allocate");

	/* A send right is needed for task_set_special_port */
	kr = mach_port_insert_right(mach_task_self(), e_exception_port,
				    e_exception_port, MACH_MSG_TYPE_MAKE_SEND);
	if (kr)
		emul_panic("e_exception_setup mach_port_insert_right");

	kr = thread_get_special_port(mach_thread_self(), THREAD_EXCEPTION_PORT,
				     &original_exception_port);
	if (kr)
		emul_panic("e_exception_setup thread_get_exception_port");

	if (!MACH_PORT_VALID(original_exception_port)) {
		kr = task_get_special_port(mach_task_self(),
					   TASK_EXCEPTION_PORT,
					   &original_exception_port);
		if (kr)
			emul_panic("e_exception_setup task_get_exception_port");
	}

	kr = thread_set_special_port(mach_thread_self(), THREAD_EXCEPTION_PORT,
				     e_exception_port);
	if (kr)
		emul_panic("e_exception_setup task_set_exception_port");

	/* The send right is no longer needed */
	kr = mach_port_mod_refs(mach_task_self(), e_exception_port,
				MACH_PORT_RIGHT_SEND, -1);
	if (kr)
		emul_panic("e_exception_setup mach_port_mod_refs");
}

void e_exception_thread()
{
	e_mach_msg_server(exc_server, 4096, e_exception_port);
	emul_panic("e_mach_msg_server returned");
}

kern_return_t catch_exception_raise(
	mach_port_t	port,
	mach_port_t	thread,
	mach_port_t	task,
	integer_t	exception,
	integer_t	code,
	integer_t	subcode)
{
	struct parisc_thread_state state;
	kern_return_t kr;

	/* sanity */
	if (task != mach_task_self())
		emul_panic("catch_exception_raise: not me!");

	if (!MACH_PORT_VALID(original_exception_port))
		emul_panic("catch_excpeption_raise: original_exception_port");

	kr = exception_raise(original_exception_port,
			     thread, task, exception, code, subcode);
	if (kr)
		e_emulator_error("exception_raise: x%x", kr);

	port_consume(thread);
	port_consume(task);

	return KERN_SUCCESS;
}

void
port_consume(mach_port_t port)
{
	/* XXX Optimize: delete many ports at once */
	mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, -1);
}
