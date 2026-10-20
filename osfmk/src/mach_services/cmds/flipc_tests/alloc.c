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

#define INSTANCES 1

/*
 * Includes main, usage, and file system synchronization routines.
 */
#include <main.h>
#include <mach/mach_traps.h>
#include <mach/mach_port.h>

#define ENDPOINTS 10
#define EPGROUPS 11
#define BUFFERS 40
#define BUFFERS_PER_ENDPOINT 15

extern int disable_me_kick;

void usermain(char *filename, int instance)
{
    FLIPC_return_t fr;
    FLIPC_endpoint_t endpoints[ENDPOINTS+1];
    FLIPC_endpoint_t epgroups[EPGROUPS+1];
    FLIPC_domain_t domain;
    int i;
    FLIPC_address_t address;
    FLIPC_buffer_t buffer;
    int tmp;
    struct FLIPC_domain_info domain_info;

    domain_info.max_endpoints = ENDPOINTS;
    domain_info.max_epgroups = EPGROUPS;
    domain_info.max_buffers = BUFFERS;
    domain_info.max_buffers_per_endpoint = BUFFERS_PER_ENDPOINT;
    domain_info.yield_fn = &cthread_yield;
    domain_info.policy = SYNC_POLICY_FIFO;
    domain_info.msg_buffer_size = 0;
    domain_info.error_log_size = 0;
    bzero((void*)&domain_info.performance, sizeof(domain_info.performance));
    
    ftest1(FLIPC_domain_init(0, &domain_info, &domain),
	   fr, FLIPC_SUCCESS, 1);

    printf("At several points during this test, a failure may be \n");
    printf("indicated by the test hanging.  These points will be indicated\n");
    printf("by printing out an \"h\".  If this is not almost immediately\n");
    printf("followed by a \"p\", the test has failed.\n");

    /* Allocate one of each (of endpoint and endpoint group), just to play
       with interaction issues.  */
    ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Receive, 0, &endpoints[0]),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_allocate(domain, SEMAPHORE_NULL, &epgroups[0]),
	   fr, FLIPC_SUCCESS, 1);

    /* Allocate all the endpoints we can; make sure it ends up being the right
       number.  */
    for (i = 1; i < ENDPOINTS; i++)
	ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Send, 0, &endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Receive, 0,
				   &endpoints[ENDPOINTS]),
	   fr, FLIPC_ALLOCATE_ENDPOINT_SHORTAGE, 1);

    /* Allocate all the endpoint groups we can; make sure it ends up being
       the right number.  */
    for (i = 1; i < EPGROUPS; i++)
	ftest1(FLIPC_epgroup_allocate(domain, SEMAPHORE_NULL, &epgroups[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_allocate(domain, SEMAPHORE_NULL, &epgroups[EPGROUPS]),
	   fr, FLIPC_ALLOCATE_EPGROUP_SHORTAGE, 1);

    /* Free both down to zero, and try just a little bit more.  */
    for (i = 0; i < ENDPOINTS; i++)
	ftest1(FLIPC_endpoint_deallocate(endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[ENDPOINTS/2]),
	   fr, FLIPC_ENDPOINT_INACTIVE, 1);

    for (i = 0; i < EPGROUPS; i++)
	ftest1(FLIPC_epgroup_deallocate(epgroups[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_deallocate(epgroups[EPGROUPS/2]),
	   fr, FLIPC_EPGROUP_INACTIVE, 1);

    /* Go back up, down, and up again, separately.  */
    for (i = 0; i < ENDPOINTS; i++)
	ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Send, 0, &endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Receive, 0,
				   &endpoints[ENDPOINTS]),
	   fr, FLIPC_ALLOCATE_ENDPOINT_SHORTAGE, 1);

    for (i = ENDPOINTS-3; i < ENDPOINTS; i++)
	ftest1(FLIPC_endpoint_deallocate(endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[ENDPOINTS - 2]),
	   fr, FLIPC_ENDPOINT_INACTIVE, 1);

    for (i = ENDPOINTS-3; i < ENDPOINTS; i++)
	ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Send, 0, &endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Receive, 0,
				   &endpoints[ENDPOINTS]),
	   fr, FLIPC_ALLOCATE_ENDPOINT_SHORTAGE, 1);

    for (i = ENDPOINTS-1; i >= 0 ; i--)
	ftest1(FLIPC_endpoint_deallocate(endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[ENDPOINTS/2]),
	   fr, FLIPC_ENDPOINT_INACTIVE, 1);


    for (i = 0; i < EPGROUPS; i++)
	ftest1(FLIPC_epgroup_allocate(domain, SEMAPHORE_NULL, &epgroups[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_allocate(domain, SEMAPHORE_NULL, &epgroups[EPGROUPS]),
	   fr, FLIPC_ALLOCATE_EPGROUP_SHORTAGE, 1);

    for (i = EPGROUPS-3; i < EPGROUPS; i++)
	ftest1(FLIPC_epgroup_deallocate(epgroups[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_deallocate(epgroups[EPGROUPS - 2]),
	   fr, FLIPC_EPGROUP_INACTIVE, 1);

    for (i = EPGROUPS-3; i < EPGROUPS; i++)
	ftest1(FLIPC_epgroup_allocate(domain, SEMAPHORE_NULL, &epgroups[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_allocate(domain, SEMAPHORE_NULL, &epgroups[EPGROUPS]),
	   fr, FLIPC_ALLOCATE_EPGROUP_SHORTAGE, 1);

    for (i = EPGROUPS-1; i >= 0 ; i--)
	ftest1(FLIPC_epgroup_deallocate(epgroups[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_deallocate(epgroups[EPGROUPS/2]),
	   fr, FLIPC_EPGROUP_INACTIVE, 1);

    /* There shouldn't be anything allocated now.  */

    /* Force a buffer shortage and confirm it.  */
#if (BUFFERS/6)*6 == BUFFERS
    BUFFERS must not be an integral multiple of six. 
#endif
	for (i = 0; i < BUFFERS / 6; i++)
	    ftest1(FLIPC_endpoint_allocate(domain, 6, FLIPC_Send, 0, &endpoints[i]),
		   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 6, FLIPC_Receive, 0,
				   &endpoints[i]),
	   fr, FLIPC_ALLOCATE_BUFFER_SHORTAGE, 1);

    /* Free.  */
    for (i = 0; i < BUFFERS / 6; i++)
	ftest1(FLIPC_endpoint_deallocate(endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[BUFFERS/10]),
	   fr, FLIPC_ENDPOINT_INACTIVE, 1);

    /* Try to hit the number of buffers you have on the mark exactly.  */
#if (BUFFERS / 5) * 5 != BUFFERS
    BUFFERS must be an integral multiple of five.  
#endif
	for (i = 0; i < BUFFERS / 5; i++)
	    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &endpoints[i]),
		   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 1, FLIPC_Receive, 0,
				   &endpoints[i]),
	   fr, FLIPC_ALLOCATE_BUFFER_SHORTAGE, 1);

    /* Free.  */
    for (i = 0; i < BUFFERS / 5; i++)
	ftest1(FLIPC_endpoint_deallocate(endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[BUFFERS/10]),
	   fr, FLIPC_ENDPOINT_INACTIVE, 1);
    
    /* A little bit of playing with endpoints and endpoint groups.  */
    ftest1(FLIPC_epgroup_allocate(domain, SEMAPHORE_NULL, &epgroups[0]),
	   fr, FLIPC_SUCCESS, 1);
    for (i = 0; i < ENDPOINTS; i++) {
	ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Send, 0, &endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_join_epgroup(endpoints[i], epgroups[0]),
	       fr, FLIPC_SUCCESS, 1);
    }
    ftest1(FLIPC_endpoint_join_epgroup(endpoints[0], FLIPC_EPGROUP_NULL),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_epgroup_deallocate(epgroups[0]),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[0]),
	   fr, FLIPC_SUCCESS, 1);
    
    /* Allocate and free all endpoints, making sure that you get what
       you should.  */
    for (i = 0; i < ENDPOINTS; i++)
	ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Send, 0, &endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 3, FLIPC_Receive, 0,
				   &endpoints[ENDPOINTS]),
	   fr, FLIPC_ALLOCATE_ENDPOINT_SHORTAGE, 1);
    
    for (i = ENDPOINTS-1; i >= 0 ; i--)
	ftest1(FLIPC_endpoint_deallocate(endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[ENDPOINTS/2]),
	   fr, FLIPC_ENDPOINT_INACTIVE, 1);

    /* See if freeing with buffers on either side of the processed
       line works.  */
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &endpoints[0]),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Receive, 0, &endpoints[1]),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_address(endpoints[1], &address),
	   fr, FLIPC_SUCCESS, 1);
    
    /* Put an unacquired message on the receive endpoint.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(endpoints[0]), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, address),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(endpoints[0], buffer),
	   fr, FLIPC_SUCCESS, 1);
    while (!FLIPC_buffer_processed(buffer))
	;
    printf("h");
    fflush(stdout);
    while (FLIPC_endpoint_buffer_available(endpoints[1]) == 0)
        ;
    ftest1(FLIPC_endpoint_buffer_available(endpoints[1]), tmp, 1, 1);
    printf("p");
    fflush(stdout);

    /* Put an unprocessed message on the send endpoint.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(endpoints[0]), buffer,
	     FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, address),
	   fr, FLIPC_SUCCESS, 1);
    disable_me_kick = 1;
    ftest1(FLIPC_endpoint_buffer_release(endpoints[0], buffer),
	   fr, FLIPC_SUCCESS, 1);
    disable_me_kick = 0;

    /* Free them both and make sure we can get all buffers.  */
    ftest1(FLIPC_endpoint_deallocate(endpoints[0]), fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[1]), fr, FLIPC_SUCCESS, 1);

    /* Did we screw up number of buffers available?  */
    for (i = 0; i < BUFFERS / 5; i++)
	ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 1, FLIPC_Receive, 0,
				   &endpoints[i]),
	   fr, FLIPC_ALLOCATE_BUFFER_SHORTAGE, 1);

    /* Free.  */
    for (i = 0; i < BUFFERS / 5; i++)
	ftest1(FLIPC_endpoint_deallocate(endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[BUFFERS/10]),
	   fr, FLIPC_ENDPOINT_INACTIVE, 1);

    /* See if freeing with buffers on either side of the real start
       of the bufferlist array works.  */
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &endpoints[0]),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Receive, 0, &endpoints[1]),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_address(endpoints[1], &address),
	   fr, FLIPC_SUCCESS, 1);
    
    /* Move the buffers around so they straddle the beginning of the array.  */
    for (i = 0; i < BUFFERS_PER_ENDPOINT - 3; i++) {
	/* Each time through the loop moves the five buffers on each
	   endpoint one notch around the circular buffer.  Doing this
	   BUFFERS_PER_ENDPOINT - 3 times should leave the five buffers
	   nicely straddling the endpoint.  */
	ftestnot(FLIPC_endpoint_buffer_acquire(endpoints[0]), buffer,
		 FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_buffer_set_destination(buffer, address),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(endpoints[0], buffer),
	       fr, FLIPC_SUCCESS, 1);
	while (!FLIPC_buffer_processed(buffer))
	    ;
	printf("h");
	fflush(stdout);
	while (FLIPC_endpoint_buffer_available(endpoints[1]) == 0)
	    ;
	ftest1(FLIPC_endpoint_buffer_available(endpoints[1]), tmp, 1, 1);
	printf("p");
	fflush(stdout);
	ftestnot(FLIPC_endpoint_buffer_acquire(endpoints[1]), buffer,
		 FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_endpoint_buffer_release(endpoints[1], buffer),
	       fr, FLIPC_SUCCESS, 1);
    }

    /* Free the endpoints.   */
    ftest1(FLIPC_endpoint_deallocate(endpoints[0]), fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[1]), fr, FLIPC_SUCCESS, 1);
    
    /* Did we screw up number of buffers available?  */
    for (i = 0; i < BUFFERS / 5; i++)
	ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 1, FLIPC_Receive, 0,
				   &endpoints[i]),
	   fr, FLIPC_ALLOCATE_BUFFER_SHORTAGE, 1);

    /* Free.  */
    for (i = 0; i < BUFFERS / 5; i++)
	ftest1(FLIPC_endpoint_deallocate(endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_deallocate(endpoints[BUFFERS/10]),
	   fr, FLIPC_ENDPOINT_INACTIVE, 1);

    /* Close down.  */
    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);
}
