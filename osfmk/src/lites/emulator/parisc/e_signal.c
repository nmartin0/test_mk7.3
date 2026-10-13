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
 * 	Utah $Hdr: e_signal.c 1.6 95/03/03$
 */

#include <e_defs.h>
#include <emul_mach_interface.h>

extern signal_server();
noreturn e_i_got_a_signal(struct parisc_thread_state *state);
void e_enable_asynch_signals();

mach_port_t signal_port = MACH_PORT_NULL;
mach_port_t signal_suspend_reply_port = MACH_PORT_NULL;

void e_signal_child_setup()
{
	if (signal_port != MACH_PORT_NULL) {
		signal_port = MACH_PORT_NULL;
		signal_suspend_reply_port = MACH_PORT_NULL;
		e_enable_asynch_signals();
	}
}

boolean_t e_signal_setup()
{
	kern_return_t kr;

	if (MACH_PORT_VALID(signal_port))
		return FALSE;

	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
				&signal_port);
	if (kr)
		emul_panic("e_signal_setup mach_port_allocate");

	/* No use to have a long queueu */
	kr = mach_port_set_qlimit(mach_task_self(), signal_port, 1);
	if (kr)
		emul_panic("e_signal_setup mach_port_set_qlimit");

	/* preallocate one reply port for later use */
	signal_suspend_reply_port = mig_get_reply_port();

	/* Check in signal port with server */
	kr = bsd_signal_port_register(process_self(), signal_port);
	if (kr)
		emul_panic("e_signal_setup bsd_signal_port_register");

	return TRUE;
}

void e_signal_thread()
{
	e_mach_msg_server(signal_server, 4096, signal_port);
	emul_panic("e_signal_thread: e_mach_msg_server returned");
}

/* Called when first signal handler is installed */
void e_enable_asynch_signals()
{
	if (e_signal_setup())
		e_fork_a_thread(e_signal_thread);
}

/* 
 * RPC handler for signal notifications from server.
 * This code is run by only one thread so there is no problem with the
 * code pushing stuff on top of the stack. To make it parallel the
 * thread_set_state must be moved ahead of the pushing but as there
 * still would be a race between getting and setting of states it
 * really must be run by one thread only in any case.
 */
kern_return_t do_signal_notify(
	mach_port_t	port,
	mach_port_t	thread)
{
	struct parisc_thread_state state;
	mach_msg_type_number_t count = PARISC_THREAD_STATE_COUNT;
	kern_return_t kr;
	vm_offset_t sp;
	int fsize;

	/* 
	 * Make sure the target thread is not holding the allocator
	 * lock. If the signal actually was an exception within
	 * e_mig_support we're dead. But that shouldn't happen.
	 */
	e_mig_lock();
	/* No we've got the lock so the other guy can't get it */
	kr = emul_thread_suspend(thread, signal_suspend_reply_port);
	/* Now it is safe to use the normal allocator again */
	e_mig_unlock();
	if (kr)
		emul_panic("catch_signal emul_thread_suspend");

	kr = thread_abort(thread);
	if (kr)
		emul_panic("catch_signal thread_abort");

	kr = thread_get_state(thread, PARISC_THREAD_STATE,
			      (thread_state_t) &state, &count);
	if (kr)
		emul_panic("catch_signal thread_get_state");

	/*
	 * Push current thread state on stack.
	 * Stack frame size is 64 bytes for basic frame marker plus
	 * sizeof saved state rounded to 64 bytes.
	 */
	sp = state.r30;
	fsize = ((sizeof state + 63) & ~63) + 64;
	*(struct parisc_thread_state *)sp = state;

	/*
	 * Modify thread state so that when it resumes, it appears to have
	 * made a call:
	 *
	 *	e_i_got_a_signal(struct parisc_thread_state *state)
	 */
	state.r26 = sp;
	state.r27 = emul_dp();
	state.r30 = sp + fsize;
	state.iioq_head = (int) e_i_got_a_signal;
	state.iioq_tail = state.iioq_head + 4;

	kr = thread_set_state(thread,
			      PARISC_THREAD_STATE,
			      (thread_state_t) &state,
			      PARISC_THREAD_STATE_COUNT);
	if (kr)
	    emul_panic("catch_signal thread_set_state");
	kr = thread_resume(thread);
	if (kr)
	    emul_panic("catch_signal thread_resume");

	port_consume(thread);
	return KERN_SUCCESS;
}

/* 
 * This function is never called but a thread intercepted by an
 * asynchronous signal by catch_signal above ends up here. It's
 * original state is on the stack.
 *
 * Scheduler activations would be great for doing the signal stuff.
 */
noreturn e_i_got_a_signal(struct parisc_thread_state *state)
{
	if (syscall_debug > 1)
		e_emulator_error("IGOTASIGNAL");

	while (take_a_signal(state))
		;
	emul_load_state(state);
	/*NOTREACHED*/
}
