/*
 * Copyright 1991-1998 by Open Software Foundation, Inc. 
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */
/*
 * cmk1.1
 */
/*
 * event.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 */

#include <xkern/include/domain.h>
#include <mach/message.h>
#include <xkern/include/xk_debug.h>
#include <xkern/include/upi.h>
#include <xkern/include/platform.h>
#include <xkern/include/assert.h>
#include <xkern/include/xk_path.h>
#include <xkern/include/event.h>
#include <xkern/include/event_i.h>

static mach_port_t	evClock_port;
static THREAD_T		evClock_thread;

void evClock(int); /* Forward */

/*
 * evInit -- initialize the timer queues and start the clock thread.
 *
 */
xkern_return_t
evInit(
	int interval)
{
    int i;

    xTrace0(event,TR_GROSS_EVENTS,"evInit enter");

#ifdef XK_THREAD_TRACE
    localEventMap = mapCreate(BIG_N, sizeof(Event), xkSystemPath);
    threadEventMap = mapCreate(BIG_N, sizeof(THREAD_T), xkSystemPath);
    if ( ! localEventMap || ! threadEventMap ) {
	xTrace0(event, TR_ERRORS, "allocation error");
	return XK_FAILURE;
    }
#endif

    for ( i=0; i < BIG_N + 1; i++ ) {
        evhead[i].evnext = (queue_chain_t *)&evhead[i];
        evhead[i].evprev = (queue_chain_t *)&evhead[i];
        evhead[i].func = THIS_IS_THE_HEADER;
    }
    tickmod = 0;
 
    /* initialize timers and semaphores */
    event_mutex = mutex_alloc();
    (void) mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
		              &evClock_port);
    (void) mach_port_insert_right(mach_task_self(),
			          evClock_port, evClock_port,
				  MACH_MSG_TYPE_MAKE_SEND);
  
/*
 * Since evClock does not use any xkernel routines, this should be 
 * allowed to remain a cthread, in spite of the sledgehammer concurrency
 * control otherwise enforced in the xkernel.
 */
    xTrace0(event,TR_EVENTS,"evInit starting evClock thread");

    evClock_thread = cthread_fork((cthread_fn_t)evClock, 
				  /*(any_t)*/ (void *)interval);
    cthread_detach(evClock_thread);

    xTrace0(event,TR_EVENTS,"evInit exit");
    return XK_SUCCESS;
}


/*
 * evClock --- update the event queues periodically
 *
 * This routine is not under the master lock.
 * It decrements the tick count for each event bucket; events
 * whose time has expired are scheduled for execution via the
 * master lock.  The events are stored in the bucket in order
 * of increasing time to expiration.  Each event's "deltat" field
 * indicates the time by which it exceeds the previous event's
 * timeout value.
 *
 * the "kernel_thread" routine for the Mach inkernel version did
 * not seem to pass the interval argument to this thread, so
 * it uses the global "event_granualarity" directly.
 *
 */
int tracetickshort = 0;
int xk_heart;

void evClock(
	int	interval)
{
    Event e;
    boolean_t  ret;
    mach_msg_header_t m;
    int	priority = XK_CLOCK_THREAD_PRIORITY;

    cthread_set_name(cthread_self(), "evClock");
    xkPolicyFixedFifo(&priority, sizeof(priority));

    xTrace0(event,TR_FULL_TRACE,"evClock enter");
    while(TRUE) {
	cthread_yield();
	if (mach_msg(&m, MACH_RCV_MSG|MACH_RCV_TIMEOUT,
		     0, sizeof m, evClock_port,
		     interval, MACH_PORT_NULL) == MACH_RCV_TIMED_OUT)
	{
	    if (tracetickshort) {
		xIfTrace(event,TR_EVENTS) {
		    printf("{tick}");
		}
	    } else {
		xTrace0(event,TR_EVENTS,"evClock tick");
	    }
	    EVENT_LOCK();
	    ((Event)evhead[tickmod].evnext)->deltat--;
	    tickmod = (tickmod+1)%BIG_N; /* optimize if BIG_N is power of 2*/
	    for (e = (Event)evhead[tickmod].evnext; e != &evhead[tickmod] 
	         && e->deltat == 0; e = (Event)e->evnext) {
	        remque((queue_entry_t) e);
	        e->state = E_SCHEDULED;
#ifdef XK_THREAD_TRACE
	        xGetTime(&e->startTime);
#endif
		/*
		 * cthread_detach(cthread_fork(stub, e));
		 *
		 * Disallowed, use sledgehammer instead.
		 */
	        xTrace0(event, TR_EVENTS, "evClock starting event");
	        CreateProcess((gen_func) xk_event_stub, e, 1, e);
	    }
	    /* 
	     * Look for delayed events
	     */
	    for ( e = (Event)evhead[DELAY_BUCKET].evnext;
		  e != &evhead[DELAY_BUCKET] && e->deltat == 0;
		  e = (Event)e->evnext ) {
		remque(e);
		semSignal((xkSemaphore *)e->extra);
	    }
	    if ( e != &evhead[DELAY_BUCKET] ) {
		e->deltat--;
	    }
	    EVENT_UNLOCK();
	}
        else {
	    printf("evClock: received a message!!!\n");
	    /*
	     * If we get a message. In this version we shouldn't! [mats]
	     */
	}
    }
}

