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
 * Do a somewhat more complex semaphore test.
 */

#define INSTANCES 2

/*
 * Includes main, usage, and file system synchronization routines.
 */
#include <main.h>
#include <mach/sync.h>
#include <mach/mach_traps.h>
#include <mach/mach_port.h>

#define ENDPOINTS 20
#define EPGROUPS 2
#define BUFFERS 100
#define BUFFERS_PER_ENDPOINT 22

void multisema_0();
void multisema_1();

void usermain(char *filename, int instance)
{
    common_init(filename, instance, 
		ENDPOINTS, EPGROUPS, BUFFERS, BUFFERS_PER_ENDPOINT);

    /* Do instance specific stuff.  */
    switch(instance) {
      case 0:
	multisema_0();
	break;
      case 1:
	multisema_1();
	break;
      default:
	fprintf(stderr, "Bad instance number in usermain; \"can't happen\"\n");
	abort();
	break;
    }
}

void multisema_0()
{
    FLIPC_return_t fr;
    kern_return_t kr;
    FLIPC_endpoint_t receive_endpoints[10];
    FLIPC_address_t receive_addresses[10];
    int receive_msg_count[10];
    FLIPC_epgroup_t epg;
    FLIPC_endpoint_t epgroup_ep;
    semaphore_port_t local_sema, response_sema;
    mach_port_t my_task;
    int tmp;
    unsigned int utmp;
    FLIPC_endpoint_t tmpep;
    FLIPC_endpoint_t send;
    int wakeup_req;
    FLIPC_buffer_t buffer;
    int i, j;

    /* Init the receive messages count (substantially before use).  */
    for (i = 0; i < 10; i++)
	receive_msg_count[i] = 0;

    /* Allocate a semaphore for association with the epgroup.  */
    my_task = mach_task_self();
    local_sema = SEMAPHORE_NULL;
    ftest1(semaphore_create(my_task, &local_sema, 0, 0), kr, KERN_SUCCESS, 0);
    mach_port_deallocate(my_task, my_task);

    /* Allocate an endpoint group.  */
    ftest1(FLIPC_epgroup_allocate(domain, local_sema, &epg),
	   fr, FLIPC_SUCCESS, 1);

    /* Confirm association.  */
    ftest1(FLIPC_epgroup_semaphore(epg, &response_sema),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(local_sema, tmp, response_sema, 0);

    /* Allocate 10 endpoints.  */
    for (i = 0; i < 10; i++) {
	ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Receive, 0,
				       & receive_endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_join_epgroup(receive_endpoints[i], epg),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_address(receive_endpoints[i], &utmp),
	       fr, FLIPC_SUCCESS, 1);
	receive_addresses[i] = utmp;
    }

    /* Set the wait count.  */
    ftest1(FLIPC_epgroup_set_wakeup_count(epg, 20), fr, FLIPC_SUCCESS, 0);

    /* Mark the epg to be woken up.  */
    ftest1(FLIPC_epgroup_get_message(epg, 1, &tmpep, &wakeup_req),
	   buffer, FLIPC_BUFFER_NULL, 1);
    ftest1(wakeup_req, tmp, 1, 1);

    /* Allocate a send endpoint and use it to tell the remote node about the
       10 endpoints.  */
    ftest1(FLIPC_endpoint_allocate(domain, 2, FLIPC_Send, 0, &send),
	   fr, FLIPC_SUCCESS, 1);
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    bcopy((char*) receive_addresses, (char*) buffer,
	  sizeof(receive_addresses));
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);
    
    /* Sleep on the semaphore.  */
    semaphore_wait(local_sema);

    for (i = 0; i < 20; i++) {
	ftestnot(FLIPC_epgroup_get_message(epg, 0, &epgroup_ep, 0),
		 buffer, FLIPC_BUFFER_NULL, 1);
	for (j = 0; j < 10; j++)
	    if (epgroup_ep == receive_endpoints[j]) {
		receive_msg_count[j]++;
		break;
	    }
	ftestnot(j, tmp, 10, 1);
	ftest1(FLIPC_endpoint_buffer_release(epgroup_ep, buffer),
	       fr, FLIPC_SUCCESS, 1);
    }

    for (i = 0; i < 10; i++)
	ftest1(receive_msg_count[i], tmp, 2, 1);

    /* Close down.  */
    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);
}

void multisema_1()
{
    FLIPC_return_t fr;
    FLIPC_endpoint_t send;
    FLIPC_buffer_t rbuffer, sbuffer;
    FLIPC_address_t remote_addresses[10];
    int i;

    /* Allocate a send endpoint.  */
    ftest1(FLIPC_endpoint_allocate(domain, 20, FLIPC_Send, 0, &send),
	   fr, FLIPC_SUCCESS, 1);

    /* Wait for input from the remote node.  */
    while ((rbuffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;

    /* Get the addresses.  */
    bcopy((char*) rbuffer, (char*) remote_addresses, sizeof(remote_addresses));

    /* Sleep a bit.  */
    for (i = 0; i < 3000000; i++)
	;

    /* Spray.  */
    for (i = 0; i < 10; i++) {
	ftestnot(FLIPC_endpoint_buffer_acquire(send),
		 sbuffer, FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_buffer_set_destination(sbuffer, remote_addresses[i]),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send, sbuffer),
	       fr, FLIPC_SUCCESS, 1);

	ftestnot(FLIPC_endpoint_buffer_acquire(send),
		 sbuffer, FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_buffer_set_destination(sbuffer, remote_addresses[i]),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send, sbuffer),
	       fr, FLIPC_SUCCESS, 1);
    }

    /* Close down.  */
    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);
}

