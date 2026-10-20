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

/*
 * Stuff needed by several of the following routines.
 */
FLIPC_endpoint_t send_endpoints[5];
FLIPC_endpoint_t recv_endpoints[5];
FLIPC_epgroup_t send_epgroup, recv_epgroup;
FLIPC_address_t local_addresses[5], remote_addresses[5];
semaphore_port_t send_sema, recv_sema;

typedef void *any_t;

any_t thread_1(any_t arg);
any_t thread_2(any_t arg);

int global_instance_var;

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

    ftest1(semaphore_create(my_task, &send_sema, 0, 0),
	   kr, KERN_SUCCESS, 1);
    ftest1(FLIPC_epgroup_allocate(domain, send_sema, &send_epgroup),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(semaphore_create(my_task, &recv_sema, 0, 0),
	   kr, KERN_SUCCESS, 1);
    ftest1(FLIPC_epgroup_allocate(domain, recv_sema, &recv_epgroup),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_set_wakeup_count(send_epgroup, 25),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_set_wakeup_count(recv_epgroup, 1),
	   fr, FLIPC_SUCCESS, 1);

    mach_port_deallocate(my_task, my_task);

    for (i = 0; i < 5; i++) {
	ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0,
				       &send_endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_join_epgroup(send_endpoints[i], send_epgroup),
	       fr, FLIPC_SUCCESS, 1);

	ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Receive, 0,
				       &recv_endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_join_epgroup(recv_endpoints[i], recv_epgroup),
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

char *checkphrase = "mary had a little lamb.  Iter %d, sendp: %d, destp: %d, sinst: %d, dinst: %d\n";
char *gatephrase = "Ok to proceed following iteration %d\n";

any_t thread_1(any_t arg)
{
    int i, j;
    FLIPC_return_t fr;
    FLIPC_buffer_t buffer;
    int tmp, loopvar, lv, scanf_result;
    FLIPC_endpoint_t tmp_endpoint;
    int wakeup_requested;

    for (loopvar = 0; loopvar < (int)arg; loopvar++) {
	/* This is just a hint, as other processes on this node may
	   kick off the ME.  */
	disable_me_kick = 1;

	/* To reliably enable epgroup wakeup with a known count, we need to
	   have no endpoints on the epgroup.  Serious pain.  */
	for (i = 0; i < 5; i++)
	    ftest1(FLIPC_endpoint_join_epgroup(send_endpoints[i],
					       FLIPC_EPGROUP_NULL),
		   fr, FLIPC_SUCCESS, 1);

	ftest1(FLIPC_epgroup_get_message(send_epgroup, 1, &tmp_endpoint,
					 &wakeup_requested),
	       buffer, FLIPC_BUFFER_NULL, 1);
	ftest1(wakeup_requested, tmp, 1, 1);
	
	for (i = 0; i < 5; i++)
	    ftest1(FLIPC_endpoint_join_epgroup(send_endpoints[i], send_epgroup),
		   fr, FLIPC_SUCCESS, 1);

	for (i = 0; i < 5; i++)
	    for (j = 0; j < 5; j++) {
		ftestnot(FLIPC_endpoint_buffer_acquire(send_endpoints[i]),
			 buffer, FLIPC_BUFFER_NULL, 1);
		ftest1(FLIPC_buffer_set_destination(buffer,
						    remote_addresses[(i+j)%5]),
		       fr, FLIPC_SUCCESS, 1);
		mutex_lock(&printf_mutex);
		sprintf((char*)buffer, checkphrase, loopvar,
			i, (i+j)%5,
			global_instance_var, 1 - global_instance_var);
		mutex_unlock(&printf_mutex);
		ftest1(FLIPC_endpoint_buffer_release(send_endpoints[i], buffer),
		       fr, FLIPC_SUCCESS, 1);
	    }
	disable_me_kick = 0;

	flipc_kick_message_engine(send_epgroup);

	semaphore_wait(send_sema);

	/* Synchronize here with remote receive side.  */
	while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	       == FLIPC_BUFFER_NULL)
	    ;
	mutex_lock(&printf_mutex);
	scanf_result = sscanf((char*)buffer, gatephrase, &lv);
	mutex_unlock(&printf_mutex);
	ftest1(scanf_result, tmp, 1, 1);
	ftest1(lv, tmp, loopvar, 1);
	ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	       fr, FLIPC_SUCCESS, 1);

	/* Let the user know we're alive.  */
	mutex_lock(&printf_mutex);
	printf(".");
	fflush(stdout);
	mutex_unlock(&printf_mutex);

	for (i = 0; i < (100 - loopvar)*101 + 11 * global_instance_var;
	     i++)
	    ;
    }
    return (any_t) 0;
}

any_t thread_2(any_t arg)
{
    int i, j, k;
    FLIPC_buffer_t buffer;
    FLIPC_endpoint_t tmp_endpoint, sync_send_endpoint;
    FLIPC_return_t fr;
    int receive_counts[5], recv_send_ep[5];
    int citer, csendp, cdestp, csinst, cdinst;
    int tmp, scanf_result;

    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0,
				   &sync_send_endpoint),
	   fr, FLIPC_SUCCESS, 1);

    for (k = 0; k < (int)arg; k++) {
	for (i = 0; i < 5; i++) {
	    receive_counts[i] = 0;
	    recv_send_ep[i] = 0;
	    ftest1(FLIPC_endpoint_overrun_count(recv_endpoints[i], 0),
		   tmp, 0, 1);
	}
	for (i = 0; i < 25; i++) {
	    while ((buffer
		    = FLIPC_epgroup_get_message(recv_epgroup, 1, &tmp_endpoint, 0))
		   == FLIPC_BUFFER_NULL)
		semaphore_wait(recv_sema);
	    mutex_lock(&printf_mutex);
	    scanf_result = sscanf((char*) buffer, checkphrase,
			  &citer, &csendp, &cdestp, &csinst, &cdinst);
	    mutex_unlock(&printf_mutex);
	    ftest1(scanf_result, tmp, 5, 1);
	    ftest1(citer, tmp, k, 1);
	    ftest1(cdinst, tmp, global_instance_var, 1);
	    ftest1(csinst, tmp, 1-global_instance_var, 1);
	    for (j = 0; j < 5; j++)
		if (recv_endpoints[j] == tmp_endpoint)
		    break;
	    ftest1(cdestp, tmp, j, 1);
	    ftest1(recv_send_ep[j] & (1 << csendp), tmp, 0, 1);
	    recv_send_ep[j] |= (1 << csendp);
	    bzero((char*)buffer, 100);
	    receive_counts[j]++;
	    ftest1(FLIPC_endpoint_buffer_release(tmp_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);
	    for (j = 0; j < 5; j++)
		ftest1(FLIPC_endpoint_overrun_count(recv_endpoints[j], 0),
		       tmp, 0, 1);
	}
	for (j = 0; j < 5; j++) {
	    ftest1(receive_counts[j], tmp, 5, 1);
	    ftest1(recv_send_ep[j], tmp, 0x1f, 1);
	    receive_counts[j] = 0;
	    recv_send_ep[j] = 0;
	}
	/* Synchronize here with remote send side.  */
	ftestnot(FLIPC_endpoint_buffer_acquire(sync_send_endpoint), buffer,
		 FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	       fr, FLIPC_SUCCESS, 1);
	mutex_lock(&printf_mutex);
	sprintf((char*)buffer, gatephrase, k);
	mutex_unlock(&printf_mutex);
	ftest1(FLIPC_endpoint_buffer_release(sync_send_endpoint, buffer),
	       fr, FLIPC_SUCCESS, 1);
    }
    return (any_t) 0;
}


