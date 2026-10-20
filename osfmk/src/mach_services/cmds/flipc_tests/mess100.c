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

#define INSTANCES 2

/*
 * Includes main, usage, and file system synchronization routines.
 */
#include <main.h>

#define ENDPOINTS 5
#define EPGROUPS 2
#define BUFFERS 40
#define BUFFERS_PER_ENDPOINT 10
/*
 * Both sides of the 100 message ping-pong test.
 */
void usermain(char *filename, int instance)
{
    FLIPC_return_t fr;
    FLIPC_domain_t domain;
    struct FLIPC_domain_info info;
    FLIPC_endpoint_t send_endpoint, receive_endpoint;
    FLIPC_buffer_t buffer;
    int i;
    FLIPC_address_t dest;
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

    /* Allocate both a send and a receive endpoint with a couple of buffers.  */
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &send_endpoint),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Receive, 0,
				   &receive_endpoint),
	   fr, FLIPC_SUCCESS, 1);

    /* Exchange information on addresses with the other guy.  */
    ftest1(FLIPC_endpoint_address(receive_endpoint, &dest),
	   fr, FLIPC_SUCCESS, 1);
    write_instance_integer(filename, instance, dest);
    dest = read_instance_integer(filename, 1 - instance);
    /* Safe to delete the *other* guys file, since we have the information
       and there are only two of us.  */
    delete_instance_file(filename, 1 - instance);

    if (instance == 1) {
	/* If we are instance 1, the first thing we do is receive.  */
	while ((buffer = FLIPC_endpoint_buffer_acquire(receive_endpoint))
	       == FLIPC_BUFFER_NULL)
	    ;
    } else {
	/* If we are instance 0, we need to setup the buffer.  */
	ftestnot(FLIPC_endpoint_buffer_acquire(send_endpoint),
		 buffer, FLIPC_BUFFER_NULL, 1);
	/* Set it up to be sent.  */
	for (i = 0; i < info.msg_buffer_size; i++)
	    ((unsigned char *) buffer)[i] = (unsigned char) i;
    }

    /* Long loop, eventually ending the buffer up on the other side.  */
    for (i = 0; i < 100; i++) {
	/* Send your message.  */
	ftest1(FLIPC_buffer_set_destination(buffer, (FLIPC_address_t) dest),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
	       fr, FLIPC_SUCCESS, 1);

	/* Move a buffer over to the receive endpoint, so we don't
	   starve it.  */
	ftestnot(FLIPC_endpoint_buffer_acquire(send_endpoint),
		 buffer, FLIPC_BUFFER_NULL, 1);
	ftest1(FLIPC_endpoint_buffer_release(receive_endpoint, buffer),
	       fr, FLIPC_SUCCESS, 1);

	/* Wait for the receive message.  */
	while ((buffer = FLIPC_endpoint_buffer_acquire(receive_endpoint))
	       == FLIPC_BUFFER_NULL)
	    ;

	/* Send *that* message back, up at the top of the loop.  */
    }

    if (instance == 0) {
	/* Send it across for the last time.  */
	ftest1(FLIPC_buffer_set_destination(buffer, (FLIPC_address_t) dest),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
	       fr, FLIPC_SUCCESS, 1);
    } else {
	/* We've gotten the buffer; make sure it's intact.  */
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

    /* Deallocate the endpoints.  This is only safe in this implementation;
       eventually we may need to confirm that the message has been sent.  */
    ftest1(FLIPC_endpoint_deallocate(send_endpoint),
	   fr, FLIPC_SUCCESS, 0);
    ftest1(FLIPC_endpoint_deallocate(receive_endpoint),
	   fr, FLIPC_SUCCESS, 0);

    /* Close the buffer.  */
    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);

    /* Our job is done now; exit quietly.  */
}
