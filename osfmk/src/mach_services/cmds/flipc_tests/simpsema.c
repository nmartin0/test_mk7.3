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
 * Do a simple semaphore test.
 */

#define INSTANCES 2

/*
 * Includes main, usage, and file system synchronization routines.
 */
#include <main.h>
#include <mach/sync.h>
#include <mach/mach_traps.h>
#include <mach/mach_port.h>

#define ENDPOINTS 10
#define EPGROUPS 2
#define BUFFERS 40
#define BUFFERS_PER_ENDPOINT 15

void simpsema_0();
void simpsema_1();

void usermain(char *filename, int instance)
{
    common_init(filename, instance,
		ENDPOINTS, EPGROUPS, BUFFERS, BUFFERS_PER_ENDPOINT);

    /* Do instance specific stuff.  */
    switch(instance) {
      case 0:
	simpsema_0();
	break;
      case 1:
	simpsema_1();
	break;
      default:
	fprintf(stderr, "Bad instance number in usermain; \"can't happen\"\n");
	abort();
	break;
    }
}

void simpsema_0()
{
    FLIPC_return_t fr;
    kern_return_t kr;
    FLIPC_endpoint_t ep1, ep2;
    FLIPC_epgroup_t epg;
    semaphore_port_t local_sema, response_sema;
    mach_port_t my_task;
    int tmp;
    unsigned int utmp;
    FLIPC_endpoint_t tmpep;
    FLIPC_endpoint_t send;
    int wakeup_req;
    FLIPC_buffer_t buffer;
    struct FLIPC_epgroup_info epgroup_info;

    /* Allocate a semaphore for association with the epgroup.  */
    my_task = mach_task_self();
    local_sema = SEMAPHORE_NULL;
    ftest1(semaphore_create(my_task, &local_sema, 0, 0), kr, KERN_SUCCESS, 0);
    mach_port_deallocate(my_task, my_task);

    /* Allocate two endpoints and an endpoint group.  */
    ftest1(FLIPC_endpoint_allocate(domain, 10, FLIPC_Receive, 0, &ep1),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 10, FLIPC_Receive, 0, &ep2),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_allocate(domain, local_sema, &epg),
	   fr, FLIPC_SUCCESS, 1);

    /* Put the endpoints on the endpoint group.  */
    ftest1(FLIPC_endpoint_join_epgroup(ep1, epg),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_join_epgroup(ep2, epg),
	   fr, FLIPC_SUCCESS, 1);

    /* Is the semaphore ok?  */
    ftest1(FLIPC_epgroup_semaphore(epg, &response_sema),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(local_sema, tmp, response_sema, 0);

    /* Set the endpoint wakeup counter.  */
    ftest1(FLIPC_epgroup_set_wakeup_count(epg, 2),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(FLIPC_epgroup_query(epg, &epgroup_info),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(epgroup_info.msgs_per_wakeup, tmp, 2, 0);
    
    /* Try to get a buffer from the epgroup.  This should fail, but
       should lead to a wakeup request on the epgroup.  */
    ftest1(FLIPC_epgroup_get_message(epg, 1, &tmpep, &wakeup_req),
	   buffer, FLIPC_BUFFER_NULL, 1);
    ftest1(wakeup_req, tmp, 1, 1);

    /* Allocate a send endpoint and use it to request messages from the
       other instance.  */
    ftest1(FLIPC_endpoint_allocate(domain, 2, FLIPC_Send, 0, &send),
	   fr, FLIPC_SUCCESS, 1);
    ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_address(ep1, &utmp), fr, FLIPC_SUCCESS, 1);
    bcopy((char*)&utmp, buffer, sizeof(utmp));
    ftest1(FLIPC_endpoint_address(ep2, &utmp), fr, FLIPC_SUCCESS, 1);
    bcopy((char*)&utmp, ((char *)buffer)+sizeof(utmp), sizeof(utmp));
    ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Sleep on the semaphore.  */
    semaphore_wait(local_sema);
    
    /* We had better now have two messages waiting for us, one on
       each receive endpoint.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(ep1),
	     buffer, FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_endpoint_buffer_acquire(ep1),
	   buffer, FLIPC_BUFFER_NULL, 1);
    ftestnot(FLIPC_endpoint_buffer_acquire(ep2),
	     buffer, FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_endpoint_buffer_acquire(ep2),
	   buffer, FLIPC_BUFFER_NULL, 1);

    /* Close down.  */
    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);
}

void simpsema_1()
{
    FLIPC_return_t fr;
    FLIPC_endpoint_t send;
    FLIPC_buffer_t rbuffer, sbuffer;

    /* Allocate a send endpoint.  */
    ftest1(FLIPC_endpoint_allocate(domain, 2, FLIPC_Send, 0, &send),
	   fr, FLIPC_SUCCESS, 1);

    /* Wait for input from the remote node.  */
    while ((rbuffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;

    /* We've been requested to send some data.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send), sbuffer, FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(sbuffer, *(int*)rbuffer),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send, sbuffer),
	   fr, FLIPC_SUCCESS, 1);

    ftestnot(FLIPC_endpoint_buffer_acquire(send), sbuffer, FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(sbuffer, *(((int*)rbuffer)+1)),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send, sbuffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Our job's done.  */
    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);
}



