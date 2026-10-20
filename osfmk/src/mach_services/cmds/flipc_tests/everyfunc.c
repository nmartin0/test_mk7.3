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
/*
 * Test at least one call to every function/macro that flipc exports.
 */

#define INSTANCES 2

/*
 * Includes main, usage, and file system synchronization routines.
 */
#include <main.h>
#include <mach/sync.h>
#include <mach/mach_traps.h>
#include <mach/mach_port.h>

#define ENDPOINTS 12
#define EPGROUPS 2
#define BUFFERS 50
#define BUFFERS_PER_ENDPOINT 15

/*
 * Both sides of the test.
 */
void usermain(char *filename, int instance)
{
    FLIPC_return_t fr;
    FLIPC_domain_t domain;
    struct FLIPC_domain_info info;
    FLIPC_endpoint_t send, control_receive, data_receive;
    FLIPC_buffer_t buffer, buffer2;
    FLIPC_epgroup_t local_epgroup;
    int i;
    int tmp;
    FLIPC_endpoint_t tmpep, epgroup_ep;
    FLIPC_epgroup_t tmpeg;
    void (*tmpf)();
    FLIPC_endpoint_t endpoint_array[2];
    int endpoint_array_size;
    FLIPC_address_t
	local_control, local_data,
	remote_data, remote_control;
    semaphore_port_t local_sema, response_sema;
    kern_return_t kr;
    mach_port_t my_task;
    FLIPC_domain_errors_t error_ptr, tmperrp;
    struct FLIPC_domain_info domain_info;
    struct FLIPC_endpoint_info endpoint_info;
    struct FLIPC_epgroup_info epgroup_info;
    int wakeup_requested;
    int error_info_kept;

    domain_info.max_endpoints = ENDPOINTS;
    domain_info.max_epgroups = EPGROUPS;
    domain_info.max_buffers = BUFFERS;
    domain_info.max_buffers_per_endpoint = BUFFERS_PER_ENDPOINT;
    domain_info.yield_fn = &cthread_yield;
    domain_info.policy = SYNC_POLICY_FIFO;
    domain_info.msg_buffer_size = 0;
    domain_info.error_log_size = 0;
    bzero((void*)&domain_info.performance, sizeof(domain_info.performance));

    /* Either initialized the domain or attach to it, as you will.  */
    ftest2(FLIPC_domain_init(0, &domain_info, &domain),
	   fr, FLIPC_DOMAIN_INITIALIZED, FLIPC_SUCCESS, 1);
    if (fr == FLIPC_DOMAIN_INITIALIZED) {
	printf("Init failed; attaching. \n");
	bzero(&domain_info, sizeof(domain_info));
	domain_info.yield_fn = &cthread_yield;
	ftest1(FLIPC_domain_attach(0, &domain_info, &domain),
	       fr, FLIPC_SUCCESS, 1);
    }

    /* Abort since I actually need the information later.  */
    ftest1(FLIPC_domain_query(domain, &info), fr, FLIPC_SUCCESS, 1);

    /* Is this what I would expect?  */
    ftest1(info.max_endpoints, tmp, ENDPOINTS, 0);
    ftest1(info.max_epgroups, tmp, EPGROUPS, 0);
    ftest1(info.max_buffers, tmp, BUFFERS, 0);
    ftest1(info.max_buffers_per_endpoint, tmp, BUFFERS_PER_ENDPOINT, 0);
    ftest1(info.yield_fn, tmpf, &cthread_yield, 0);
    ftest1(info.performance.messages_sent, tmp, 0, 0);
    ftest1(info.performance.messages_received, tmp, 0, 0);

    /* Give me my endpoints.  */
    ftest1(FLIPC_endpoint_allocate(domain, 10, FLIPC_Send, 0, &send),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 2, FLIPC_Receive,
				   0, &control_receive),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Receive,
				   0, &data_receive),
	   fr, FLIPC_SUCCESS, 1);
    
    /* Lets see if the query functions work.  */
    ftest1(FLIPC_endpoint_query(send, &endpoint_info),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(endpoint_info.type, tmp, FLIPC_Send, 0);
    ftest1(endpoint_info.processed_buffers_dropped_p, tmp, 0, 0);
    ftest1(endpoint_info.number_buffers, tmp, 10, 0);

    ftest1(FLIPC_endpoint_query(control_receive, &endpoint_info),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(endpoint_info.type, tmp, FLIPC_Receive, 0);
    ftest1(endpoint_info.processed_buffers_dropped_p, tmp, 0, 0);
    ftest1(endpoint_info.number_buffers, tmp, 2, 0);

    ftest1(FLIPC_endpoint_query(data_receive, &endpoint_info),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(endpoint_info.type, tmp, FLIPC_Receive, 0);
    ftest1(endpoint_info.processed_buffers_dropped_p, tmp, 0, 0);
    ftest1(endpoint_info.number_buffers, tmp, 5, 0);

    /* Get local address.  */
    ftest1(FLIPC_endpoint_address(control_receive, &local_control),
	   fr, FLIPC_SUCCESS, 1);

    /* Exchange control addresses.  */
    remote_control =
	bootstrap_instance_integers(filename, instance, local_control);

    /* Guarding against a test error, rather than a flipc error--if one
       of the instance files was left around from a previous improper
       termination, the test could conceivably run single-sided *and
       pass*.  I believe that this is because the second side of the
       test symmetrically allocates the same endpoints as the first did
       (that died and left the file around).  Thus it sends and receives
       messages to/from itself.  I don't want this.  I'll conditionalize
       it on number of instances, since I could imagine wanting to run
       this test single-instance at some point, and currently it will
       work.  */
#if INSTANCES > 1
    ftestnot(remote_control, tmp, local_control, 1);
#endif

    /* Exchange data addresses.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_address(data_receive, &local_data),
	   fr, FLIPC_SUCCESS, 1);
    bcopy((char*) &local_data, (char*) buffer, sizeof(local_data));
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);

    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    bcopy((char*) buffer, (char*) &remote_data, sizeof(remote_data));

    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Create a semaphore to be associated with an upcoming epgroup.  */
    my_task = mach_task_self();
    local_sema = SEMAPHORE_NULL;
    ftest1(semaphore_create(my_task, &local_sema, 0, 0),
	   kr, KERN_SUCCESS, 0);
    mach_port_deallocate(my_task, my_task);
    
    /* Create an endpoint group and put both receive endpoints in it.  */
    ftest1(FLIPC_epgroup_allocate(domain, local_sema, &local_epgroup), fr,
	   FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_join_epgroup(data_receive, local_epgroup),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_join_epgroup(control_receive, local_epgroup),
	   fr, FLIPC_SUCCESS, 1);

    /* Confirm that they made it.  */
    ftest1(FLIPC_endpoint_query(data_receive, &endpoint_info),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(endpoint_info.epgroup, tmpeg, local_epgroup, 0);
    
    ftest1(FLIPC_endpoint_query(control_receive, &endpoint_info),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(endpoint_info.epgroup, tmpeg, local_epgroup, 0);
    
    endpoint_array_size = 2;
    ftest1(FLIPC_epgroup_endpoints(local_epgroup, &endpoint_array_size,
				   endpoint_array),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(endpoint_array_size, tmp, 2, 0);
    ftest2(endpoint_array[0], tmpep, data_receive, control_receive, 0);
    ftest2(endpoint_array[1], tmpep, data_receive, control_receive, 0);
    ftestnot(endpoint_array[0], tmpep, endpoint_array[1], 0);

    /* Confirm association of semaphore with epgroup.  */
    ftest1(FLIPC_epgroup_semaphore(local_epgroup, &response_sema),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(local_sema, tmp, response_sema, 0);

    /* Take the control endpoint off of it.  */
    ftest1(FLIPC_endpoint_join_epgroup(control_receive, FLIPC_EPGROUP_NULL),
	   fr, FLIPC_SUCCESS, 1);

    /* Set epgroup messages and check it.  */
    ftest1(FLIPC_epgroup_set_wakeup_count(local_epgroup, 2),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(FLIPC_epgroup_query(local_epgroup, &epgroup_info),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(epgroup_info.msgs_per_wakeup, tmp, 2, 0);

    /* Send a single message.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_data),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_buffer_destination(buffer), tmp, remote_data, 0);
    *(int *)buffer = 0xdeadbeef;
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Wait a bit, then tell the other side you've sent it.  */
    for (i = 0; i < 1000000; i++)
	;
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_buffer_destination(buffer), tmp, remote_control, 0);
    *(int *)buffer = 0xbeefdead;
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);
    
    /* Wait until we get notification.  */
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    ftest1(*(int*)buffer, tmp, 0xbeefdead, 0);
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* See if you can get it from the epgroup.  Test to make sure
       wakeup requesting behavior is correct as well.  */
    wakeup_requested = 0;
    ftest1(FLIPC_epgroup_message_available(local_epgroup), tmp, 1, 1);
    ftestnot(FLIPC_epgroup_get_message(local_epgroup, 1, &epgroup_ep,
				       &wakeup_requested),
	     buffer, FLIPC_BUFFER_NULL, 1);
    ftest1(wakeup_requested, tmp, 0, 0);
    ftest1(data_receive, tmpep, epgroup_ep, 0);

    /* It had better not be on the receive endpoint now.  */
    ftest1(FLIPC_endpoint_buffer_acquire(data_receive), buffer2,
	   FLIPC_BUFFER_NULL, 1);

    /* Release it back to the receive endpoint.  */
    ftest1(FLIPC_endpoint_buffer_release(data_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Synchronize; we can't afford to have either side race into
       this next section.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Test the unlocked versions of the interfaces.  */
    ftestnot(FLIPC_endpoint_buffer_acquire_unlocked(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_data),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_buffer_destination(buffer), tmp, remote_data, 0);
    *(int *)buffer = 0xdeadbeef;
    ftest1(FLIPC_endpoint_buffer_release_unlocked(send, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Wait a bit, then tell the other side you've sent it.  */
    for (i = 0; i < 1000000; i++)
	;
    ftestnot(FLIPC_endpoint_buffer_acquire_unlocked(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_buffer_destination(buffer), tmp, remote_control, 0);
    *(int *)buffer = 0xbeefdead;
    ftest1(FLIPC_endpoint_buffer_release_unlocked(send, buffer),
	   fr, FLIPC_SUCCESS, 1);
    
    /* Wait until we get notification.  */
    while ((buffer = FLIPC_endpoint_buffer_acquire_unlocked(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    ftest1(*(int*)buffer, tmp, 0xbeefdead, 0);
    ftest1(FLIPC_endpoint_buffer_release_unlocked(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* See if it's on our endpoint.  */
    ftestnot(FLIPC_endpoint_buffer_acquire_unlocked(data_receive), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_endpoint_buffer_release_unlocked(data_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Synchronize; we can't afford to have either side race into
       this next section.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Now we bombard the receive endpoint on the other side (which
       should have five buffers waiting to receive) with ten buffers.
       Afterwards we check the overrun and total number of messages
       received.  All done with the macros, just to test another section
       of the system.  */

    /* Bombs away!  */
    for (i = 0; i < 10; i++) {
	buffer = FLIPC_endpoint_buffer_acquire(send);
	ftestnot(buffer, buffer, FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_buffer_set_destination(buffer, remote_data),
	       fr, FLIPC_SUCCESS, 1);
	fr = FLIPC_endpoint_buffer_release(send, buffer);
	ftest1(fr, fr, FLIPC_SUCCESS, 1);
    }

    /* Wait a second ...*/
    for (i = 0; i < 1000000; i++)
	;

    /* Let the receive know what we've done.   */
    buffer = FLIPC_endpoint_buffer_acquire(send);
    ftestnot(buffer, buffer, FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    fr = FLIPC_endpoint_buffer_release(send, buffer);
    ftest1(fr, fr, FLIPC_SUCCESS, 1);

    /* Wait for *our* notification.  */
    do {
	buffer = FLIPC_endpoint_buffer_acquire(control_receive);
    } while (buffer == FLIPC_BUFFER_NULL);

    fr = FLIPC_endpoint_buffer_release(control_receive, buffer);
    ftest1(fr, fr, FLIPC_SUCCESS, 1);

    /* How many are waiting for us?  */
    i = 0;
    while (1) {
	buffer = FLIPC_endpoint_buffer_acquire(data_receive);
	if (buffer == FLIPC_BUFFER_NULL)
	    break;
	fr = FLIPC_endpoint_buffer_release(data_receive, buffer);
	ftest1(fr, fr, FLIPC_SUCCESS, 1);
	i++;
    }
    
    /* We should have 5 received and five overruns.  */
    ftest1(i, tmp, 5, 0);
    ftest1(FLIPC_endpoint_overrun_count(data_receive, 0), tmp, 5, 0);

    /* Synchronize; we can't afford to have either side race into
       this next section.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Send a single message and make sure that the buffer made it through
       the send endpoint.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);  
    ftest1(FLIPC_buffer_set_destination(buffer, remote_data),
	   fr, FLIPC_SUCCESS, 1);	   
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Wait for it.  */
    while (!FLIPC_buffer_processed(buffer))
	;

    {
	FLIPC_buffer_t buffer_temp[10];
	
	/* Make sure that there are 10 buffers, all able to be acquired, on the
	   send endpoint.  */
	for (i = 0; i < 10; i ++) {
	    ftest1(FLIPC_endpoint_buffer_available(send), tmp, 1, 0);
	    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
		     FLIPC_BUFFER_NULL, 1);
	    buffer_temp[i] = buffer;
	}
	
	/* No more, right?  */
	ftest1(FLIPC_endpoint_buffer_available(send), tmp, 0, 0);

	/* Put them back.  */
	for (i = 0; i < 10; i++) {
	    /* Cheating by using null destination.  */
	    ftest1(FLIPC_buffer_set_destination_null(buffer_temp[i]),
		   fr, FLIPC_SUCCESS, 1);
	    ftest1(FLIPC_endpoint_buffer_release(send, buffer_temp[i]),
		   fr, FLIPC_SUCCESS, 1);
	}
    }
    

    /* Test out drop processed messages.  */
    {
	FLIPC_endpoint_t send_dpb, recv_dpb, buffer_pool;
	FLIPC_buffer_t buffer_snd, buffer_recv;
	FLIPC_address_t local_dpb_addr, remote_dpb_addr;
	
	/* Need to make sure allocating a receive endpoint with
	   buffers on it and dpb set works.  */
	ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Receive, 1, &recv_dpb),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_deallocate(recv_dpb), fr, FLIPC_SUCCESS, 1);

	/* Make sure that allocating a send endpoint with both buffers
	   and dpb set fails.  */
	ftestnot(FLIPC_endpoint_allocate(domain, 3, FLIPC_Send, 1, &send_dpb),
		 fr, FLIPC_SUCCESS, 0);

	/* Allocate a send and receive endpoints with this bit set.
	   We need an extra send endpoint to provide a buffer source.  */
	ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &buffer_pool),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_allocate(domain, 0, FLIPC_Send, 1, &send_dpb),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_allocate(domain, 0, FLIPC_Receive, 1, &recv_dpb),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_address(recv_dpb, &local_dpb_addr),
	       fr, FLIPC_SUCCESS, 1);

	/* Pull a receive buffer and put it on the receive endpoint.  */
	ftestnot(FLIPC_endpoint_buffer_acquire(buffer_pool), buffer_recv,
		 FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_endpoint_buffer_release(recv_dpb, buffer_recv),
	       fr, FLIPC_SUCCESS, 1);

	/* Tell the other side that we are ready to party.  */
	ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer_snd,
		 FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_buffer_set_destination(buffer_snd, remote_control),
	       fr, FLIPC_SUCCESS, 1);
	bcopy((char*)&local_dpb_addr, buffer_snd, sizeof(local_dpb_addr));
	ftest1(FLIPC_endpoint_buffer_release(send, buffer_snd),
	       fr, FLIPC_SUCCESS, 1);

	/* Get our information.  */
	while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	       == FLIPC_BUFFER_NULL)
	    ;
	bcopy((char*)buffer, (char*)&remote_dpb_addr, sizeof(remote_dpb_addr));
	ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	       fr, FLIPC_SUCCESS, 1);

	/* Send a message using the send endpoint.  */
	ftestnot(FLIPC_endpoint_buffer_acquire(buffer_pool), buffer_snd,
		 FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_buffer_set_destination(buffer_snd, remote_dpb_addr),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send_dpb, buffer_snd),
	       fr, FLIPC_SUCCESS, 1);

	/* Confirm that it behaved properly.  */
	while (!FLIPC_buffer_processed(buffer_snd))
	    ;
	ftest1(FLIPC_endpoint_buffer_acquire(send_dpb),
	       buffer, FLIPC_BUFFER_NULL, 1);
	/* This next is cheating; the user wouldn't be able to do it.  */
	ftest1(FLIPC_BUFFER_STATE(buffer_snd), tmp, flipc_Ready, 0);
	/* As is this.  */
	ftest1(FLIPC_buffer_set_destination_null(buffer_snd),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(buffer_pool, buffer_snd),
	       fr, FLIPC_SUCCESS, 1);

	/* Confirm that the receive behaved properly.  */
	while (!FLIPC_buffer_processed(buffer_recv))
	    ;
	ftest1(FLIPC_endpoint_buffer_acquire(recv_dpb),
	       buffer, FLIPC_BUFFER_NULL, 1);
	/* This next is cheating; the user wouldn't be able to do it.  */
	ftest1(FLIPC_BUFFER_STATE(buffer_recv), tmp, flipc_Ready, 0);
	/* As is this.  */
	ftest1(FLIPC_buffer_set_destination_null(buffer_recv),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(buffer_pool, buffer_recv),
	       fr, FLIPC_SUCCESS, 1);

	/* Get rid of the endpoints.  */
	ftest1(FLIPC_endpoint_deallocate(send_dpb), fr, FLIPC_SUCCESS, 0);
	ftest1(FLIPC_endpoint_deallocate(buffer_pool), fr, FLIPC_SUCCESS, 0);
	ftest1(FLIPC_endpoint_deallocate(recv_dpb), fr, FLIPC_SUCCESS, 0);
    }

    /* Send a message to the null destination; if this doesn't work
       as spec'd you should get an error when you check the errors
       section, below.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination_null(buffer),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);
    /* Wait for it.  */
    while (!FLIPC_buffer_processed(buffer))
	;

    /* Synchronize; we can't afford to have either side race into
       this next section, in case the possible error caused by
       the above is on the other side.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Confirm that we are currently errorless.  */
    error_ptr = (FLIPC_domain_errors_t) malloc(info.error_log_size + 4);
    ftestnot(error_ptr, tmperrp, (FLIPC_domain_errors_t) 0, 1);
    *(int*)(((char*)error_ptr)+info.error_log_size) = 0xdeadbeef;

    ftest1(FLIPC_domain_error_query(domain, error_ptr),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(*(int*)(((char*)error_ptr)+info.error_log_size), tmp, 0xdeadbeef, 0);
    
    printf("Messages dropped because buffer was inactive: %d\n",
	   error_ptr->msgdrop_inactive);
    ftest1(error_ptr->msgdrop_disabled, tmp, 0, 0);
    ftest1(error_ptr->msgdrop_badtype, tmp, 0, 0);

    for (i = 0;
	 i < (info.error_log_size
	      - sizeof(struct FLIPC_domain_errors)
	      + sizeof(int)) / sizeof(int);
	 i++)
	ftest1(error_ptr->transport_error_info[i], tmp, 0, 0);

    /* Save info for later.  */
    error_info_kept = error_ptr->error_full_config_p;
    free(error_ptr);

    /* Synchronize, to make sure that instance 0 doesn't create errors
       before instance one has done the above.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Create some errors, only in one instance.  Don't do this section
       if the kernel isn't configured to noticed these errors.  */
    if (instance == 0 && error_info_kept) {
	/* Bad type.  */
	/* Really shouldn't do this, but it's the only way to create an
	   invalid type error since error checking was added to
	   FLIPC_endpoint_address.  Serious under-the-hood poking
	   going on here.  */
	ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
		 FLIPC_BUFFER_NULL, 1);
	/* Manually compute the address of the endpoint.  */
	/* Yuck.  */
	tmp = 0;
	tmp |= ((int) local_control) & 0xffff0000;
	tmp |= 
	    ((((char *) send) - 
	      (((char*)control_receive)
	       - ((((int)local_control)&0xffff) * sizeof(struct flipc_endpoint))))
	     / sizeof(struct flipc_endpoint));
	ftest1(FLIPC_buffer_set_destination(buffer, (FLIPC_address_t) tmp),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	       fr, FLIPC_SUCCESS, 1);
	
	/* Can't yet test errors noticed by the transport (such as
	   bad destination) since we don't have a good way to export
	   those back to the user.  */

	/* Partial cleanup, with the minor twist of putting the control
	   endpoint back on the endpoint group.  */
	ftest1(FLIPC_endpoint_join_epgroup(control_receive, local_epgroup),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_epgroup_deallocate(local_epgroup),
	       fr, FLIPC_SUCCESS, 0);
	
	/* Create an invalid endpoint error.  */
	ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
		 FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_buffer_set_destination(buffer, local_control),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	       fr, FLIPC_SUCCESS, 1);
	
	/* Make sure this last one makes it through.  */
	while (!FLIPC_buffer_processed(buffer))
	    ;
	
	/* Finish cleaning up.  */
	ftest1(FLIPC_endpoint_deallocate(send),
	       fr, FLIPC_SUCCESS, 0);
	
	/* Check the error state again.  */
	error_ptr = (FLIPC_domain_errors_t) malloc(info.error_log_size + 4);
	ftestnot(error_ptr, tmperrp, (FLIPC_domain_errors_t) 0, 1);
	*(int*)(((char*)error_ptr)+info.error_log_size) = 0xdeadbeef;
	
	ftest1(FLIPC_domain_error_query(domain, error_ptr),
	       fr, FLIPC_SUCCESS, 0);
	ftest1(*(int*)(((char*)error_ptr)+info.error_log_size), tmp, 0xdeadbeef, 0);
	
	printf("Messages dropped because buffer was inactive: %d\n",
	       error_ptr->msgdrop_inactive);
	ftest1(error_ptr->msgdrop_disabled, tmp, 1, 0);
	ftest1(error_ptr->msgdrop_badtype, tmp, 1, 0);
	
	for (i = 0;
	     i < (info.error_log_size
		  - sizeof(struct FLIPC_domain_errors)
		  + sizeof(int)) / sizeof(int);
	     i++)
	    ftest1(error_ptr->transport_error_info[i], tmp, 0, 0);
	
	free(error_ptr);
    } else {
	/* Just do cleanup here.  */
	ftest1(FLIPC_endpoint_join_epgroup(control_receive, local_epgroup),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_epgroup_deallocate(local_epgroup),
	       fr, FLIPC_SUCCESS, 0);
	ftest1(FLIPC_endpoint_deallocate(send),
	       fr, FLIPC_SUCCESS, 0);
    }

    /* Check to make sure that everything went away.  */
    ftest1(FLIPC_endpoint_query(send, &endpoint_info),
	   fr, FLIPC_ENDPOINT_INACTIVE, 0);
    ftest1(FLIPC_endpoint_query(control_receive, &endpoint_info),
	   fr, FLIPC_ENDPOINT_INACTIVE, 0);
    ftest1(FLIPC_endpoint_query(data_receive, &endpoint_info),
	   fr, FLIPC_ENDPOINT_INACTIVE, 0);

    /* Close the buffer.  */
    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);
}



