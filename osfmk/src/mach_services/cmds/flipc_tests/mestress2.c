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
 * 
 */
/*
 * cmk1.1
 */

#define INSTANCES 2

/*
 * Includes main, usage, and file system synchronization routines.
 */
#include <main.h>
#include <mach/sync.h>
#include <mach/mach_traps.h>
#include <mach/mach_port.h>
#include <cthreads.h>

#define ENDPOINTS 40
#define EPGROUPS 4
#define BUFFERS 120
#define BUFFERS_PER_ENDPOINT 15

extern int disable_me_kick;
extern void flipc_kick_message_engine(void *);

#ifdef MUTEX_CHECK
/*
 * A log of line numbers tracking the state of the printf mutex.
 */
struct {
    int line;
    int mutex_held;
} printf_mutex_log[4096];

int printf_mutex_log_index = 0;

#define MC()							\
do {								\
    printf_mutex_log[printf_mutex_log_index].line = __LINE__;	\
    printf_mutex_log[printf_mutex_log_index++].mutex_held	\
	= printf_mutex.held;					\
} while (0)
#else
#define MC()
#endif

/*
 * Stuff needed by several of the following routines.
 */
FLIPC_endpoint_t send_endpoints[5];
FLIPC_endpoint_t recv_endpoints[5];
FLIPC_epgroup_t shared_epgroup;
FLIPC_address_t local_addresses[5], remote_addresses[5];
semaphore_port_t shared_sema;

typedef void *any_t;

any_t thread_1(any_t arg);
any_t thread_2(any_t arg);

int global_instance_var;

char *checkphrase =
"mary had a little lamb at endpoint %d.  It went to endpoint %d\n";

void usermain(char *filename, int instance)
{
    FLIPC_return_t fr;
    int i;
    cthread_t thread1, thread2;
    kern_return_t kr;
    FLIPC_buffer_t buffer;
    mach_port_t my_task = mach_task_self();

    common_init(filename, instance,
		ENDPOINTS, EPGROUPS, BUFFERS, BUFFERS_PER_ENDPOINT);

    global_instance_var = instance;

    ftest1(semaphore_create(my_task, &shared_sema, 0, 0),
	   kr, KERN_SUCCESS, 1);
    ftest1(FLIPC_epgroup_allocate(domain, shared_sema, &shared_epgroup),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_set_wakeup_count(shared_epgroup, 50),
	   fr, FLIPC_SUCCESS, 1);

    mach_port_deallocate(my_task, my_task);

    for (i = 0; i < 5; i++) {
	ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0,
				       &send_endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_join_epgroup(send_endpoints[i], shared_epgroup),
	       fr, FLIPC_SUCCESS, 1);

	ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Receive, 0,
				       &recv_endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_join_epgroup(recv_endpoints[i], shared_epgroup),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_address(recv_endpoints[i], &local_addresses[i]),
	       fr, FLIPC_SUCCESS, 1);
    }

    /* Let the other side know about your receive endpoints, and find
       out about theirs.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send_endpoints[0]), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    bcopy((char*)local_addresses, (char*)buffer, sizeof(local_addresses));
    ftest1(FLIPC_endpoint_buffer_release(send_endpoints[0], buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    bcopy((char*)buffer, (char*)remote_addresses, sizeof(remote_addresses));
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Fork threads.  */
    thread1 = cthread_fork(thread_1, (any_t)100);
    thread2 = cthread_fork(thread_2, (any_t)100);

    cthread_join(thread1);
    cthread_join(thread2);

    /* Close down the domain.  */
    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);
}

/*
 * The goal for this section is to have a lot (arg) of individual waves.
 * During each wave each send side will send 25 messages (spread among the
 * receive endpoints) and each receive side will receive 25 messages.
 * Immediately at the beginning of a wave, the receive side will wait;
 * when it's done sending, the send side will wait.  Whichever of them wakes
 * up first will signal the semaphore again to wakeup the other.
 *
 * In general, the receive side will be waiting for the send side
 * to tell it to go ahead.
 */

int semaphore_signaled = 0;
int clear_to_receive = 0;
int clear_to_receive_ack = 0;
int clear_to_submit_wakeup =  1;

any_t thread_1(any_t arg)
{
    int i, j;
    FLIPC_return_t fr;
    FLIPC_buffer_t buffer;
    int tmp, loopvar;
    FLIPC_endpoint_t tmp_endpoint, control_send_endpoint;
    int wakeup_requested;

    ftest1(FLIPC_endpoint_allocate(domain, 2, FLIPC_Send, 0,
				   &control_send_endpoint),
	   fr, FLIPC_SUCCESS, 1);

    for (loopvar = 0; loopvar < (int)arg; loopvar++) {
	/* To reliably enable epgroup wakeup with a known count, we need to
	   have no endpoints on the epgroup.  Serious pain.  */
	/* Need to wait for the receiver to finish his last.  */
	while (!clear_to_submit_wakeup)
	    ;
	clear_to_submit_wakeup = 0;

	for (i = 0; i < 5; i++)
	    ftest1(FLIPC_endpoint_join_epgroup(send_endpoints[i],
					       FLIPC_EPGROUP_NULL),
		   fr, FLIPC_SUCCESS, 1);

	ftest1(FLIPC_epgroup_get_message(shared_epgroup, 1, &tmp_endpoint,
					 &wakeup_requested),
	       buffer, FLIPC_BUFFER_NULL, 1);
	ftest1(wakeup_requested, tmp, 1, 1);
	
	for (i = 0; i < 5; i++)
	    ftest1(FLIPC_endpoint_join_epgroup(send_endpoints[i], shared_epgroup),
		   fr, FLIPC_SUCCESS, 1);

	/* Synchronize everyone here.  First with local receiver.  */
	clear_to_receive = 1;
	while (!clear_to_receive_ack)
	    ;
	clear_to_receive_ack = 0;

	/* Then with remote send side.  */
	ftestnot(FLIPC_endpoint_buffer_acquire(control_send_endpoint), buffer,
		 FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(control_send_endpoint, buffer),
	       fr, FLIPC_SUCCESS, 1);
	while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	       == FLIPC_BUFFER_NULL)
	    ;
	ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	       fr, FLIPC_SUCCESS, 1);
	
	/* Wait a somewhat random period of time.  */
	for (i = random() & 0x7fff; i > 0; i--)
	    ;

	/* This is just a hint, as other processes on this node may
	   kick off the ME.  */
	disable_me_kick = 1;

	for (i = 0; i < 5; i++) {
	    for (j = 0; j < 5; j++) {
		ftestnot(FLIPC_endpoint_buffer_acquire(send_endpoints[i]),
			 buffer, FLIPC_BUFFER_NULL, 1);
		ftest1(FLIPC_buffer_set_destination(buffer,
						    remote_addresses[(i + j) % 5]),
		       fr, FLIPC_SUCCESS, 1);
		mutex_lock(&printf_mutex);
		sprintf((char*)buffer, checkphrase, i, (i+j)%5);
		mutex_unlock(&printf_mutex);
		ftest1(FLIPC_endpoint_buffer_release(send_endpoints[i], buffer),
		       fr, FLIPC_SUCCESS, 1);
	    }
	}
	disable_me_kick = 0;

	flipc_kick_message_engine(shared_epgroup);

	semaphore_wait(shared_sema);
	if (!semaphore_signaled) {
	    semaphore_signaled = 1;
	    semaphore_signal(shared_sema);
	} else
	    semaphore_signaled = 0;
	/* Let the user know we're alive.  */
	mutex_lock(&printf_mutex);
	printf(".");
	fflush(stdout);
	mutex_unlock(&printf_mutex);
    }
    return (any_t) 0;
}

any_t thread_2(any_t arg)
{
    int i, j, k;
    FLIPC_buffer_t buffer;
    FLIPC_endpoint_t tmp_endpoint, control_send_endpoint;
    int tmp, scanf_result, ep_idx;
    FLIPC_return_t fr;

    for (k = 0; k < (int)arg; k++) {
	while (!clear_to_receive)
	    ;
	clear_to_receive = 0;
	clear_to_receive_ack = 1;

	semaphore_wait(shared_sema);
	if (!semaphore_signaled) {
	    semaphore_signaled = 1;
	    semaphore_signal(shared_sema);
	} else
	    semaphore_signaled = 0;
	
	for (i = 0; i < 5; i++) {
	    int buffer_gotten[5];

	    for (j = 0; j < 5; j++)
		buffer_gotten[j] = 0;
	    
	    for (j = 0; j < 5; j++) {
		int from, to;
		ftestnot(FLIPC_endpoint_buffer_acquire(recv_endpoints[i]),
			 buffer, FLIPC_BUFFER_NULL, 1);
		mutex_lock(&printf_mutex);
		from = -1;
		to = -1;
		scanf_result = sscanf((char*) buffer, checkphrase, &from, &to);
		mutex_unlock(&printf_mutex);
		ftest1(scanf_result, tmp, 2, 1);
		ftest1(to, tmp, i, 1);
		buffer_gotten[from]++;
		bzero((char*)buffer, 100);
		ftest1(FLIPC_endpoint_buffer_release(recv_endpoints[i], buffer),
		       fr, FLIPC_SUCCESS, 1);
	    }

	    for (j = 0; j < 5; j++)
		ftest1(buffer_gotten[j], tmp, 1, 1);

	}
	clear_to_submit_wakeup = 1;
    }
    return (any_t) 0;
}


