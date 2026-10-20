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
 * Test just the initialization, attach (should fail), query, and teardown
 * pathway.
 */

void init_0(char *filename);

#define INSTANCES 1

/*
 * Includes main, usage, and file system synchronization routines.
 */
#include <main.h>

/*
 * We just initialize the domain, query it, see if we agree with the queries,
 * detach, and exit.
 */
#define ENDPOINTS 5
#define EPGROUPS 2
#define BUFFERS 40
#define BUFFERS_PER_ENDPOINT 10
void usermain(char *filename, int instance)
{
    FLIPC_domain_t domain, domain1;
    struct FLIPC_domain_info info;
    FLIPC_return_t fr;
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

    /* Make sure that non-0 domain indexes don't work.  */

    ftest1(FLIPC_domain_init(1, &domain_info, &domain),
	   fr, FLIPC_DOMAIN_NOT_AVAILABLE, 1);

    ftest1(FLIPC_domain_init(0, &domain_info, &domain),
	   fr, FLIPC_SUCCESS, 1);

    bzero(&domain_info, sizeof(domain_info));
    domain_info.yield_fn = &cthread_yield;
    ftest1(FLIPC_domain_attach(0, &domain_info, &domain1),
	   fr, FLIPC_DOMAIN_ATTACHED, 1);

    domain_info.policy = SYNC_POLICY_FIFO;
    domain_info.max_endpoints = ENDPOINTS + 1;
    domain_info.max_epgroups = EPGROUPS + 1;
    domain_info.max_buffers = BUFFERS + 1;
    domain_info.max_buffers_per_endpoint = BUFFERS_PER_ENDPOINT + 1;

    ftest1(FLIPC_domain_init(0, &domain_info, &domain),
	   fr, FLIPC_DOMAIN_ATTACHED, 1);
    
    ftest1(FLIPC_domain_query(domain, &info), fr, FLIPC_SUCCESS, 0);

    ftest1(info.max_endpoints, tmp, ENDPOINTS, 0);
    ftest1(info.max_epgroups, tmp, EPGROUPS, 0);
    ftest1(info.max_buffers, tmp, BUFFERS, 0);
    ftest1(info.max_buffers_per_endpoint, tmp, BUFFERS_PER_ENDPOINT, 0);
    ftest1((int) info.yield_fn, tmp, (int) &cthread_yield, 0);
    ftest1(info.performance.messages_sent, tmp, 0, 0);
    ftest1(info.performance.messages_received, tmp, 0, 0);

    printf("user msg buffer size is %d\n", info.msg_buffer_size);
    
    /* Send a message to yourself and check if the performance data
       increments correctly.  */
    if (info.performance.performance_valid) {
	FLIPC_endpoint_t send;
	FLIPC_endpoint_t control_receive;
	FLIPC_address_t local_control;
	FLIPC_buffer_t buffer;

	ftest1(FLIPC_endpoint_allocate(domain, 1, FLIPC_Send, 0, &send),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_allocate(domain, 1, FLIPC_Receive, 0,
				       &control_receive),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_address(control_receive, &local_control),
	       fr, FLIPC_SUCCESS, 1);
	
	ftestnot(FLIPC_endpoint_buffer_acquire(send), buffer,
		 FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_buffer_set_destination(buffer, local_control),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send, buffer),
	       fr, FLIPC_SUCCESS, 1);
	while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	       == FLIPC_BUFFER_NULL)
	    ;
	ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_domain_query(domain, &info), fr, FLIPC_SUCCESS, 0);
	ftest1(info.performance.messages_sent, tmp, 1, 0);
	ftest1(info.performance.messages_received, tmp, 1, 0);
    }

    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);

    domain_info.max_endpoints = ENDPOINTS;
    domain_info.max_epgroups = EPGROUPS;
    domain_info.max_buffers = BUFFERS;
    domain_info.max_buffers_per_endpoint = BUFFERS_PER_ENDPOINT;
    bzero((void*)&domain_info.performance, sizeof(domain_info.performance));

    ftest1(FLIPC_domain_init(0, &domain_info, &domain),
	   fr, FLIPC_SUCCESS, 1);

    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);
}
