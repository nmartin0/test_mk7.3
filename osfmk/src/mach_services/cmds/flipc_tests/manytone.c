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
 * Hammer node zero with messages from all the other nodes.
 */

/* We don't define INSTANCES; that's an argument for these routines.  */

/*
 * Includes main, usage, and file system synchronization routines.
 */
#include <main.h>
#include <mach/sync.h>
#include <mach/mach_traps.h>
#include <mach/mach_port.h>

#define TOTAL_MESSAGES 64
#define REPETITIONS 10

#define ENDPOINTS 10
#define BUFFERS TOTAL_MESSAGES + 30
#define BUFFERS_PER_ENDPOINT TOTAL_MESSAGES
#define EPGROUPS 0

FLIPC_address_t *remote_addressess;

void instance_0(void), other_instances(int);

void usermain(char *filename, int instance)
{
    common_init(filename, instance,
		ENDPOINTS, EPGROUPS, BUFFERS,
		BUFFERS_PER_ENDPOINT > 100 ? BUFFERS_PER_ENDPOINT : 100);

    /* We now split into two camps; instance 0, and everyone else.  */
    if (instance == 0)
	instance_0();
    else
	other_instances(instance);

    /* Shutdown code here.  */
}

char *checkphrase = "Instance %d sends greetings %d of %d.  STOMP!\n";

void instance_0(void)
{
    FLIPC_return_t fr;
    int i, loopvar, j, tmp;
    cthread_t thread1, thread2;
    kern_return_t kr;
    FLIPC_buffer_t buffer;
    FLIPC_endpoint_t endpoint_var, send_endpoint, data_endpoint;
    struct FLIPC_domain_info info;
    unsigned long *node_messages_received;
    /* A couple of constants, set by comment explaining them.  */
    int fromeveryone, top_incr_node;

    /* Now everyone has to find out about each others addresses.  */
    remote_addressess =
	(FLIPC_address_t *) malloc(total_instances
				   * sizeof(FLIPC_address_t));

    /* Create a send endpoint.  */
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &send_endpoint),
	   fr, FLIPC_SUCCESS, 1);
    
    ftest1(FLIPC_domain_query(domain, &info), fr, FLIPC_SUCCESS, 1);

    ftestb(total_instances * sizeof(FLIPC_address_t), tmp,
	   0, info.msg_buffer_size+1, 1);

    /* Start the chain by grabbing a buffer, putting your information
       in it, and sending it around.  */
    
    ftestnot(FLIPC_endpoint_buffer_acquire(send_endpoint),
	     buffer, FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_endpoint_address(control_receive,
				  ((FLIPC_address_t *)buffer) + 0),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;

    /* You've got the information.  Save it, then send it to everyone
       else.  */
    bcopy((char*)buffer, (char*)remote_addressess,
	  total_instances * sizeof(FLIPC_address_t));

    for (i = 0; i < total_instances; i++) {
	while ((buffer = FLIPC_endpoint_buffer_acquire(send_endpoint))
	       == FLIPC_BUFFER_NULL)
	    ;
	bcopy((char*)remote_addressess, (char*)buffer,
	      total_instances * sizeof(FLIPC_address_t));
	ftest1(FLIPC_buffer_set_destination(buffer, remote_addressess[i]),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
	       fr, FLIPC_SUCCESS, 1);
    }

    /* Allocate stuff for keeping track of the incoming messages.  
       For "n" total messages from "m" total instances (one of which
       is the receive instance), we expect n/(m-1) + 1 messages from the
       first n % (m-1) nodes, and n/(m-1) messages from the other nodes.
       We keep track of this with bits, making sure it will all fit.  */
    fromeveryone = TOTAL_MESSAGES / (total_instances - 1);
    top_incr_node = TOTAL_MESSAGES % (total_instances - 1);

    ftestb((TOTAL_MESSAGES-1)/(total_instances-1) + 1, tmp,
	   0, sizeof(unsigned long)*8 + 1, 1);

    ftestnot(malloc((total_instances-1)*sizeof(unsigned long)),
	     node_messages_received, (unsigned long *) 0, 1);

    /* Initialize the nodes messages received to zero.  */
    for (i = 0; i < total_instances-1; i++)
	node_messages_received[i] = 0;

    /* Allocate the receive endpoint.  */
    ftest1(FLIPC_endpoint_allocate(domain, 100, FLIPC_Receive, 0,
				   &data_endpoint),
	   fr, FLIPC_SUCCESS, 1);

    for (loopvar = 0; loopvar < REPETITIONS; loopvar++) {
	/* Tell everyone to send to us.  One could argue that the
	   following really should be a broadcast tree, or maybe a
	   butterfly, to get all of them started at approximately the same
	   time.  */
	for (i = 0; i < total_instances; i++) {
	    while ((buffer = FLIPC_endpoint_buffer_acquire(send_endpoint))
		   == FLIPC_BUFFER_NULL)
		;
	    ftest1(FLIPC_endpoint_address(control_receive,
					  (FLIPC_address_t *) buffer),
		   fr, FLIPC_SUCCESS, 1);
	    ftest1(FLIPC_buffer_set_destination(buffer, remote_addressess[i]),
		   fr, FLIPC_SUCCESS, 1);
	    ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);
	}

	/* They are now pounding us with messages.  Receive the messages,
	   and keep track of where they came from.  */
	for (i = 0; i < TOTAL_MESSAGES; i++) {
	    int send_instance, greeting, total_greeting;
	    while ((buffer = FLIPC_endpoint_buffer_acquire(data_endpoint))
		   == FLIPC_BUFFER_NULL)
		;
	    ftest1(sscanf((char*)buffer, checkphrase, &send_instance, &greeting,
			  &total_greeting), tmp, 3, 1);
	    ftestb(send_instance, tmp, 1, total_instances, 1);
	    ftest1(total_greeting, tmp,
		   (send_instance <= top_incr_node ?
		    fromeveryone + 1 : fromeveryone), 1);
	    ftestb(greeting, tmp, 0, total_greeting, 1);
	    ftest1(node_messages_received[send_instance] & (1 << greeting),
		   tmp, 0, 1);
	    node_messages_received[send_instance] |= (1 << greeting);
	}

	/* Did we get everything we expected and no more?  */
	for (i = 1; i < total_instances; i++) {
	    ftest1(node_messages_received[i], tmp,
		   (1 << (i <= top_incr_node ? fromeveryone+1 : fromeveryone)) -1,
		   1);
	    node_messages_received[i] = 0;
	}
	/* Note one iteration to the terminal.  */
	printf(".");
	fflush(stdout);
    }
}

void other_instances(int instance)
{
    FLIPC_return_t fr;
    int i, j, loopvar, tmp;
    kern_return_t kr;
    FLIPC_buffer_t buffer;
    FLIPC_endpoint_t endpoint_var, send_endpoint;
    struct FLIPC_domain_info info;
    int i_should_send;

    /* Now everyone has to find out about each others addresses.  */
    remote_addressess =
	(FLIPC_address_t *) malloc(total_instances
				   * sizeof(FLIPC_address_t));

    /* Create a send endpoint.  */
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &send_endpoint),
	   fr, FLIPC_SUCCESS, 1);
    
    ftest1(FLIPC_domain_query(domain, &info), fr, FLIPC_SUCCESS, 1);

    ftestb(total_instances * sizeof(FLIPC_address_t), tmp,
	   0, info.msg_buffer_size+1, 1);

    /* Everyone else follows instances 0's lead, which means basically
       receiving a message from the instance before us, adding our
       information to the message, and shipping it back out again.
       We then receive the completed information from instance 0 and
       copy it into our local buffer.  */
    /* Don't break the chain.  */
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    ftest1(FLIPC_endpoint_address(control_receive,
				  ((FLIPC_address_t *)buffer)+instance),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(send_endpoint))
	   == FLIPC_BUFFER_NULL)
	;
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);
    
    /* Collect the millions.  */
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    bcopy((char*)buffer, (char*)remote_addressess,
	  total_instances * sizeof(FLIPC_address_t));
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);
	
    /* Setup our control variable.  */
    i_should_send =
	((instance <= TOTAL_MESSAGES % (total_instances - 1))
	 ? TOTAL_MESSAGES / (total_instances - 1) + 1
	 : TOTAL_MESSAGES / (total_instances - 1));

    for (loopvar = 0; loopvar < REPETITIONS; loopvar++) {
	/* Get permission to beat on the receive instance.  */
	while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	       == FLIPC_BUFFER_NULL)
	    ;
	ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	       fr, FLIPC_SUCCESS, 1);

	/* Lay on, Macduff!  */
	for (i = 0; i < i_should_send; i++) {
	    while ((buffer = FLIPC_endpoint_buffer_acquire(send_endpoint))
		   == FLIPC_BUFFER_NULL)
		;
	    sprintf((char*)buffer, checkphrase, instance, i, i_should_send);
	    ftest1(FLIPC_buffer_set_destination(buffer, remote_addressess[0]),
		   fr, FLIPC_SUCCESS, 1);
	    ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);
	}

	/* Hold, enough!!  */
    }
}

	    
	    


	
	
	

