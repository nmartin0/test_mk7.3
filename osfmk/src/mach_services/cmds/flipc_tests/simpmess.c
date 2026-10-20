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
 * Test a simple message transfer; from instance 0 to 1.
 */

void simpmess_0(char *filename);
void simpmess_1(char *filename);

#define INSTANCES 2

/*
 * Includes main, usage, and file system synchronization routines.
 */
#include <main.h>

void usermain(char *filename, int instance)
{
    switch(instance) {
      case 0:
	simpmess_0(filename);
	return;
      case 1:
	simpmess_1(filename);
	return;
      default:
	fprintf(stderr, "usermain called with out of bounds instance!\n");
	abort();
    }
}

#define ENDPOINTS 5
#define EPGROUPS 2
#define BUFFERS 40
#define BUFFERS_PER_ENDPOINT 10
/*
 * The send side of the simple message test.
 */
void simpmess_0(char *filename)
{
    FLIPC_return_t fr;
    FLIPC_domain_t domain;
    struct FLIPC_domain_info info;
    FLIPC_endpoint_t send_endpoint;
    FLIPC_buffer_t buffer;
    int i;
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

    /* Allocate a send endpoint with a couple of buffers.  */
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &send_endpoint),
	   fr, FLIPC_SUCCESS, 1);

    /* Grab a buffer.  */
    ftest1((buffer = FLIPC_endpoint_buffer_acquire(send_endpoint))
	   != FLIPC_BUFFER_NULL, tmp, 1, 1);

    /* Set it up to be sent.  */
    for (i = 0; i < info.msg_buffer_size; i++)
	((unsigned char *) buffer)[i] = (unsigned char) i;

    ftest1(FLIPC_buffer_set_destination(buffer,
					(FLIPC_address_t)
					read_instance_integer(filename,
							      1)),
	   fr, FLIPC_SUCCESS, 1);

    /* Send it.  */
    ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Deallocate the endpoint.  This is only safe in this implementation;
       eventually we may need to confirm that the message has been sent.  */
    ftest1(FLIPC_endpoint_deallocate(send_endpoint),
	   fr, FLIPC_SUCCESS, 0);

    /* Close the buffer.  */
    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);

    /* Our job is done now; exit quietly.  */
}

/*
 * The receive side of the simple message test.
 */
void simpmess_1(char *filename)
{
    FLIPC_return_t fr;
    FLIPC_domain_t domain;
    struct FLIPC_domain_info info;
    FLIPC_endpoint_t receive_endpoint;
    FLIPC_buffer_t buffer;
    FLIPC_address_t addr;
    int i;
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

    /* Allocate a receive endpoint, with a couple of buffers with which to
       receive.  */
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Receive,
				   0, &receive_endpoint),
	   fr, FLIPC_SUCCESS, 1);

    /* Get the address.  */
    ftest1(FLIPC_endpoint_address(receive_endpoint, &addr),
	   fr, FLIPC_SUCCESS, 1);
    
    /* Write it out.  */
    write_instance_integer(filename, 1, (int) addr);

    /* Wait to receive something on the buffer.  */
    while ((buffer = FLIPC_endpoint_buffer_acquire(receive_endpoint))
	   == FLIPC_BUFFER_NULL)
	;

    /* It's safe to delete the file now.  */
    delete_instance_file(filename, 1);

    /* Check it out.  */
    {
	int mismatch = 0;
	for (i = 0; i < info.msg_buffer_size; i++)
	    if (((unsigned char *) buffer)[i] != i) {
		fprintf(stderr, "Buffer mismatch at position %d: value: %d\n",
			i, (int) ((unsigned char *)buffer)[i]);
		mismatch = 1;
	    }

	if (!mismatch)
	    printf("Message send succeeded!!!\n");
	else
	    test_failure = 1;
    }

    /* Deallocate the receive endpoint.  */
    ftest1(FLIPC_endpoint_deallocate(receive_endpoint),
	   fr, FLIPC_SUCCESS, 0);

    /* Close down the buffer.  */
    ftest1(FLIPC_domain_detach(domain),
	   fr, FLIPC_SUCCESS, 1);

    /* We're done; exit as appropriate.  */
}
