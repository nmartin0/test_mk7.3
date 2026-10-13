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
 * $Log: e_exception.c,v $
 * Revision 1.1.1.1  1995/03/02  21:49:29  mike
 * Initial Lites release from hut.fi
 *
 */
/* 
 *	File:	emulator/ns532/e_exception.c
 *	Authors:
 *	Johannes Helander, Helsinki University of Technology, 1994.
 *	Ian Dall, Adelaide, 1994.
 *
 *	Exception handler.
 */

#include <e_defs.h>
#include <ns532/psl.h>
#include <mach/exception.h>
#include <ns532/trap.h>

/* XXX all the trap.h files are a mess */
#define T_GENERAL_PROTECTION    13

extern exc_server();

e_thread_kill_self()
{
	thread_terminate(mach_thread_self());
}

e_fork_a_thread(void (*func)())
{
	kern_return_t kr;
	vm_address_t stack_start;
	thread_t thread;
	struct ns532_thread_state state;
	vm_offset_t stack_end;
	vm_size_t stack_size = vm_page_size * 16;

	/* Make stack. Keep it away from sbrk! */
	for (stack_start = 0xa8000000;
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

	stack_end = stack_start + stack_size;

	/* Make initial frame */
	stack_end -= 4;
	*(int *)stack_end = (int) e_thread_kill_self;

	/* Fork off a thread */
	kr = thread_create(mach_task_self(), &thread);
	if (kr)
	    emul_panic("e_fork_a_thread thread_create");
	
	/* Fill other registers with reasonable defaults */
	emul_save_state(&state);

	state.sb = 0;
	state.fp = 0;
	state.psr = PSR_U;

	/* And PC and SP with real values */
	state.pc = (int) func;
	state.sp = stack_end;

	/* Write the state into the registers */
	kr = thread_set_state(thread,
			      NS532_THREAD_STATE,
			      (thread_state_t) &state,
			      NS532_THREAD_STATE_COUNT);
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
 * This now works for Linux syscalls. Generalize later.
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
		kr = task_get_special_port(mach_task_self(), TASK_EXCEPTION_PORT,
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
  struct ns532_thread_state state;
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

port_consume(mach_port_t port)
{
	/* XXX Optimize: delete many ports at once */
	mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, -1);
}

