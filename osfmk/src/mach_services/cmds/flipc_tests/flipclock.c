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
#include <mach/mach_traps.h>
#include <mach/mach_port.h>
#include <cthreads.h>

extern int disable_me_kick;

/*
 * Plan:
 *
 * Three sections of the test; in each section, one type of each of
 * the flipc data structures (send endpoint, receive endpoint, endpoint
 * group) is ganged up on by multiple threads.
 *
 * First round, one send endpoint is created, which ten send threads
 * share.  Ten receive endpoints are created by one receive thread,
 * which sends their addresses in ten separate messages to the other
 * side.  Each of the send threads grabs one address and sends ten
 * messages to that address (all using the same receive endpoint).
 * They then all spin on a go-ahead message from the remote receive
 * thread + a signal variable (incremented each round).  The thread
 * that actually gets the go-ahead message increments the signal
 * variable.
 * This all happens arg times (we'll start out at 100).
 *
 * Next round, one receive endpoint is created (monster # buffers),
 * which ten receive threads share.  The single send thread sends 100
 * messages to that receive endpoint, which the receive threads fight
 * over.  The receive thread that gets the last message sends a
 * go-ahead across to the send thread to start again.
 * This also happens 100 times.
 *
 * Next round, two receive endpoints are created and put into the same
 * epgroup.  The ten threads fight over access to the epgroup,
 * attempting to acquire from it.  The remote send thread alternates
 * which endpoint it sends messages to.  Beyond that, it's like the above.
 */

#define REPETITIONS 10
#define NUM_THREADS 10

int global_instance_var;

#define ENDPOINTS 40
#define EPGROUPS 4
#define BUFFERS 260
#define BUFFERS_PER_ENDPOINT (NUM_THREADS*10)

/*
 * Used as a control endpoint for the following;
 * we don't use the original control_receive because this needs to
 * be sized for the number of threads in the test.
 */
FLIPC_endpoint_t thread_control_receive;
FLIPC_address_t thread_remote_control;

typedef void *any_t;

/* Thread functions.  */
any_t sendgang_send(any_t send_endpoint);
any_t sendgang_recv(any_t arg);

any_t recvgang_recv(any_t thread_num);
any_t recvgang_send(any_t arg);

any_t groupgang_recv(any_t thread_num);
any_t groupgang_send(any_t arg);

int global_loopvar = 0;

/*
 * For internal synchronization of threads.
 */
volatile int threads_ready = 0;
struct mutex threads_ready_mutex;

/*
 * receive endpoint shared by receive thread gang.
 */
FLIPC_endpoint_t receive_shared_endpoint, send_shared_endpoint;
FLIPC_address_t remote_shared;
int receive_thread_counts[NUM_THREADS];
FLIPC_buffer_t message_counts[NUM_THREADS*10];

FLIPC_epgroup_t receive_shared_epgroup;
FLIPC_endpoint_t receive_shared_endpoint1, receive_shared_endpoint2;
FLIPC_address_t remote_receive_shared1, remote_receive_shared2;

void usermain(char *filename, int instance)
{
    FLIPC_return_t fr;
    int i;
    cthread_t thread1, thread2;
    kern_return_t kr;
    FLIPC_buffer_t buffer;
    FLIPC_endpoint_t endpoint_var, send_endpoint;
    cthread_t threads[NUM_THREADS];
    cthread_t opp_thread;

    common_init(filename, instance,
		ENDPOINTS, EPGROUPS, BUFFERS, BUFFERS_PER_ENDPOINT);

    global_instance_var = instance;

    /* Create a send endpoint.  */
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Send, 0, &send_endpoint),
	   fr, FLIPC_SUCCESS, 1);

    /* Create the thread control_receive endpoint, and xchange addresses
       with the remote.  */
    ftest1(FLIPC_endpoint_allocate(domain, NUM_THREADS, FLIPC_Receive, 0,
				   &thread_control_receive),
	   fr, FLIPC_SUCCESS, 1);
    ftestnot(FLIPC_endpoint_buffer_acquire(send_endpoint),
	     buffer, FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_endpoint_address(thread_control_receive,
				  (FLIPC_address_t *) buffer),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    bcopy((char*)buffer, (char*)&thread_remote_control,
	  sizeof(thread_remote_control));
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Init the internal synchronization protection mutex.  */
    mutex_init(&threads_ready_mutex);

    /* First round; beat on a send endpoint.  */

    printf("Starting beating on send endpoint.\n");

    /* Spawn the threads.  */
    for (i = 0; i < NUM_THREADS; i++)
	threads[i] = cthread_fork(sendgang_send, (any_t) send_endpoint);
    opp_thread = cthread_fork(sendgang_recv, (any_t) 0);

    /* And wait for them.  */
    cthread_join(opp_thread);
    for (i = 0; i < NUM_THREADS; i++)
	cthread_join(threads[i]);

    /* Second round; beat on a receive endpoint.  */

    /* Create it, tell the other side about it, and listen to what that
       side has to say.  */
    ftest1(FLIPC_endpoint_allocate(domain, 10*NUM_THREADS, FLIPC_Receive, 0,
				   &receive_shared_endpoint),
	   fr, FLIPC_SUCCESS, 1);

    ftestnot(FLIPC_endpoint_buffer_acquire(send_endpoint),
	     buffer, FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_address(receive_shared_endpoint,
				  (FLIPC_address_t *) buffer),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    remote_shared = *(FLIPC_address_t *) buffer;
    ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    printf("Starting beating on receive endpoint.\n");

    /* Clear the receiving and message counts for the threads.  */
    for (i = 0; i < NUM_THREADS; i++)
	receive_thread_counts[i] = 0;

    for (i = 0; i < NUM_THREADS*10; i++)
	message_counts[i] = FLIPC_BUFFER_NULL;

    /* Make the shared send endpoint visible to the threads.  */
    send_shared_endpoint = send_endpoint;

    /* Setup the global loopvar.  */
    global_loopvar = 0;

    /* Create the threads.  */
    for (i = 0; i < NUM_THREADS; i++)
	threads[i] = cthread_fork(recvgang_recv, (any_t) i);
    opp_thread = cthread_fork(recvgang_send, (any_t) 0);

    cthread_join(opp_thread);
    for (i = 0; i < NUM_THREADS; i++)
	cthread_join(threads[i]);

    ftest1(FLIPC_endpoint_deallocate(receive_shared_endpoint),
	   fr, FLIPC_SUCCESS, 1);

    /* Third round; beat on an epgroup.  */

    printf("Starting beating on receive endpoint group.\n");

    /* Allocate everything and set it up.  */
    ftest1(FLIPC_epgroup_allocate(domain, SEMAPHORE_NULL,
				  &receive_shared_epgroup),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 5 * NUM_THREADS, FLIPC_Receive, 0,
				   &receive_shared_endpoint1),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_allocate(domain, 5 * NUM_THREADS, FLIPC_Receive, 0,
				   &receive_shared_endpoint2),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_join_epgroup(receive_shared_endpoint1,
				       receive_shared_epgroup),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_join_epgroup(receive_shared_endpoint2,
				       receive_shared_epgroup),
	   fr, FLIPC_SUCCESS, 1);
    
    /* Tell the other side about it.  */
    ftestnot(FLIPC_endpoint_buffer_acquire(send_shared_endpoint),
	     buffer, FLIPC_BUFFER_NULL, 1);
    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_address(receive_shared_endpoint1,
				  (FLIPC_address_t *) buffer),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_address(receive_shared_endpoint2,
				  ((FLIPC_address_t *) buffer)+1),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_buffer_release(send_shared_endpoint,
					 buffer),
	   fr, FLIPC_SUCCESS, 1);
    while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    remote_receive_shared1 = ((FLIPC_address_t *)buffer)[0];
    remote_receive_shared2 = ((FLIPC_address_t *)buffer)[1];

    global_loopvar = 0;

    /* Spawn the threads.  */
    for (i = 0; i < NUM_THREADS; i++)
	threads[i] = cthread_fork(groupgang_recv, (any_t) i);
    opp_thread = cthread_fork(groupgang_send, (any_t) 0);

    cthread_join(opp_thread);
    for (i = 0; i < NUM_THREADS; i++)
	cthread_join(threads[i]);

    /* Clean up from this test.  */
    ftest1(FLIPC_epgroup_deallocate(receive_shared_epgroup),
	   fr, FLIPC_SUCCESS, 1);

    /* Clean up from global stuff.  */
    ftest1(FLIPC_endpoint_deallocate(send_shared_endpoint),
	   fr, FLIPC_SUCCESS, 1);

    /* End.  */
    ftest1(FLIPC_domain_detach(domain), fr, FLIPC_SUCCESS, 1);
}

/* Global to all instances of sendgang_send.  */
volatile int rounds_sent = 0;

char *checkphrase = "Mary's lamb # %d went to endpoint 0x%x\n";

any_t sendgang_send(any_t send_endpoint_arg)
{
    FLIPC_endpoint_t send_endpoint =
	(FLIPC_endpoint_t) send_endpoint_arg;
    FLIPC_address_t my_receive_endpoint;
    FLIPC_buffer_t buffer;
    int i;
    FLIPC_return_t fr;
    
    /* Who am I sending to?  */
    while ((buffer = FLIPC_endpoint_buffer_acquire(thread_control_receive))
	   == FLIPC_BUFFER_NULL)
	;
    bcopy((char*)buffer, (char*)&my_receive_endpoint,
	  sizeof(my_receive_endpoint));
    ftest1(FLIPC_endpoint_buffer_release(thread_control_receive, buffer),
	   fr, FLIPC_SUCCESS, 1);

    /* Synchronize with all the other send threads (so that each thread
       gets one of the above buffers).  */
    mutex_lock(&threads_ready_mutex);
    threads_ready++;
    mutex_unlock(&threads_ready_mutex);
    while (threads_ready != NUM_THREADS)
	;

    while (rounds_sent < REPETITIONS) {
	int this_round_count = rounds_sent; /* Can't increment till the
					       receive side gets our data.  */

	for (i = 0; i < 10; i++) {
	    while ((buffer = FLIPC_endpoint_buffer_acquire(send_endpoint))
		   == FLIPC_BUFFER_NULL)
		;
	    mutex_lock(&printf_mutex);
	    sprintf(buffer, checkphrase, rounds_sent * 10,
		    my_receive_endpoint);
	    mutex_unlock(&printf_mutex);
	    ftest1(FLIPC_buffer_set_destination(buffer, my_receive_endpoint),
		   fr, FLIPC_SUCCESS, 1);
	    ftest1(FLIPC_endpoint_buffer_release(send_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);
	}

	buffer = FLIPC_BUFFER_NULL;
	while (rounds_sent == this_round_count) {
	    buffer = FLIPC_endpoint_buffer_acquire(thread_control_receive);
	    if (buffer) {
		/* Return the buffer and let everybody know that the message
		   has shown up.  */
		ftest1(FLIPC_endpoint_buffer_release(thread_control_receive, buffer),
		       fr, FLIPC_SUCCESS, 1);
		rounds_sent++;
	    }
	}
    }
    return (any_t) 0;
}

any_t sendgang_recv(any_t arg)
{
    FLIPC_endpoint_t receive_endpoints[10];
    FLIPC_address_t receive_addresses[10];
    int receive_endpoint_count[10];
    FLIPC_endpoint_t send_control;
    FLIPC_epgroup_t receive_epgroup;
    FLIPC_return_t fr;
    int i, loopvar, tmp, scanf_result;
    FLIPC_buffer_t buffer;

    ftest1(FLIPC_endpoint_allocate(domain, 2, FLIPC_Send, 0, &send_control),
	   fr, FLIPC_SUCCESS, 1);

    ftest1(FLIPC_epgroup_allocate(domain, SEMAPHORE_NULL, &receive_epgroup),
	   fr, FLIPC_SUCCESS, 1);

    for (i = 0; i < 10; i++) {
	ftest1(FLIPC_endpoint_allocate(domain, 10, FLIPC_Receive, 0,
				       &receive_endpoints[i]),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_join_epgroup(receive_endpoints[i], receive_epgroup),
	       fr, FLIPC_SUCCESS, 1);
    }

    /* Tell "the other side" about the receive endpoints.  */
    for (i = 0; i < 10; i++) {
	while ((buffer = FLIPC_endpoint_buffer_acquire(send_control))
	       == FLIPC_BUFFER_NULL)
	    ;
	ftest1(FLIPC_endpoint_address(receive_endpoints[i],
				      &receive_addresses[i]),
	       fr, FLIPC_SUCCESS, 1);
	bcopy((char*)&receive_addresses[i], (char*)buffer,
	      sizeof(receive_addresses[i]));
	ftest1(FLIPC_buffer_set_destination(buffer, thread_remote_control),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send_control, buffer),
	       fr, FLIPC_SUCCESS, 1);
    }

    /* Go into reading stuff from endpoints and replying to it.  */
    for (loopvar = 0; loopvar < REPETITIONS; loopvar++)  {
	FLIPC_endpoint_t tmp_endpoint;
	int j;
	int message_index, endpoint_address;
	int endpoint_index;

	/* Initialize the count.  */
	for (j = 0; j < 10; j++)
	    receive_endpoint_count[j] = 0;

	for (j = 0; j < 100; j++) {
	    while ((buffer = FLIPC_epgroup_get_message(receive_epgroup, 0,
						       &tmp_endpoint,
						       0))
		   == FLIPC_BUFFER_NULL)
		;
	    for (endpoint_index = 0;
		 (endpoint_index < 10
		  && tmp_endpoint != receive_endpoints[endpoint_index]);
		 endpoint_index++)
		;
	    ftestnot(endpoint_index, tmp, 10, 1);

	    /* Check the message.  */
	    mutex_lock(&printf_mutex);
	    message_index = -1;
	    endpoint_address = -1;
	    scanf_result = sscanf((char*) buffer, checkphrase,
				  &message_index, &endpoint_address);
	    mutex_unlock(&printf_mutex);
	    ftest1(scanf_result, tmp, 2, 1);
	    ftest1(message_index, tmp, loopvar * 10, 1);
	    ftest1(endpoint_address, tmp, receive_addresses[endpoint_index], 1);
	    bzero((char*)buffer, 100);

	    /* Count it.  */
	    receive_endpoint_count[endpoint_index]++;

	    /* Give it back.  */
	    ftest1(FLIPC_endpoint_buffer_release(tmp_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);
	}

	/* Count's ok?  */
	for (j = 0; j < 10; j++)
	    ftest1(receive_endpoint_count[j], tmp, 10, 1);

	/* Tell the send side you're ready for more.  */
	while ((buffer = FLIPC_endpoint_buffer_acquire(send_control))
	       == FLIPC_BUFFER_NULL)
	    ;
	ftest1(FLIPC_buffer_set_destination(buffer, thread_remote_control),
	       fr, FLIPC_SUCCESS, 1);
	ftest1(FLIPC_endpoint_buffer_release(send_control, buffer),
	       fr, FLIPC_SUCCESS, 1);
	mutex_lock(&printf_mutex);
	printf("r");
	fflush(stdout);
	mutex_unlock(&printf_mutex);
    }
    /* Deallocate what I've allocated.  */
    ftest1(FLIPC_endpoint_deallocate(send_control),
	   fr, FLIPC_SUCCESS, 1);

    ftest1(FLIPC_epgroup_deallocate(receive_epgroup),
	   fr, FLIPC_SUCCESS, 1);
    return (any_t) 0;
}

any_t recvgang_recv(any_t thread_num_arg)
{
    int thread_num = (int) thread_num_arg;
    int loopvar;
    int message_index, endpoint_address;
    FLIPC_address_t expected_endpoint_address;
    int total;
    int tmp, i, scanf_result;
    FLIPC_return_t fr;
    FLIPC_buffer_t buffer, buffertmp;

    ftest1(FLIPC_endpoint_address(receive_shared_endpoint,
				  &expected_endpoint_address),
	   fr, FLIPC_SUCCESS, 1);

    for (loopvar = 0; loopvar < REPETITIONS; loopvar++) {
	while (1) {			/* We get out by breaking.  */
	    /* Attempt to grab a message.  */
	    while ((buffer = FLIPC_endpoint_buffer_acquire(receive_shared_endpoint))
		   == FLIPC_BUFFER_NULL
		   && global_loopvar == loopvar)
		;

	    if (global_loopvar > loopvar) {
		/* We shouldn't have a buffer here, as someone else has signaled that
		   we have received all the messages.  */
		ftest1(buffer, buffertmp, FLIPC_BUFFER_NULL, 1);
		break;
	    }

	    /* We've got a message.  Check it.  */
	    mutex_lock(&printf_mutex);
	    message_index = -1;
	    endpoint_address = -1;
	    scanf_result = sscanf((char*) buffer, checkphrase,
				  &message_index, &endpoint_address);
	    mutex_unlock(&printf_mutex);
	    ftest1(scanf_result, tmp, 2, 1);
	    ftest1(endpoint_address, tmp, expected_endpoint_address, 1);

	    /* There's a race here that might cause us not to notice two
	       people writing the same spot, but a) it doesn't cost us to
	       check, b) the race won't happen very often, and c) if two
	       threads write the same spot, we should see the problem in
	       other ways (not all the messages showing up or extra messages
	       coming in after the end).  */
	    ftest1(message_counts[message_index], buffertmp, FLIPC_BUFFER_NULL, 1);
	    message_counts[message_index] = buffer;

	    /* Give it back.  */
	    ftest1(FLIPC_endpoint_buffer_release(receive_shared_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);

	    /* Increment your variable.  */
	    receive_thread_counts[thread_num]++;

	    /* Check the counts.  */
	    total = 0;
	    for (i = 0; i < NUM_THREADS; i++)
		total += receive_thread_counts[i];

	    if (total == NUM_THREADS * 10) 
		/* This is the loop exit condition.  However, more
		   than one thread may reach this point because of
		   racing in above setting/checking code.  So it needs
		   to be written to be correct if executed by multiple
		   threads.  */
		global_loopvar = loopvar+1;
	}

	/* We need to synchronize here; we can't afford to allow more
	   messages through until there's no chance that this round of thread
	   will grab them.  For synchronization, everyone will set their
	   thread counters to -1, and thread zero will spin until it sees
	   that everything is -1.  */
	receive_thread_counts[thread_num] = -1;

	if (thread_num == 0) {
	    /* Only thread zero does the final checks and zeroing.
	       The others wander forward and start checking for buffers, which
	       won't arrive until thread 0 is done.  */
	    /* For the above reason, the number of buffers on the
	       receive shared endpoint should be stable here, and
	       hence it's a good place to put our debug assertion.  */

	    /* Is everyone out of the above loop?  */
	    for (i = 0; i < NUM_THREADS; i++)
		if (receive_thread_counts[i] != -1)
		    i--;

	    for (i = 0; i < NUM_THREADS*10; i++) {
		ftestnot(message_counts[i], buffertmp, FLIPC_BUFFER_NULL, 1);
		message_counts[i] = FLIPC_BUFFER_NULL;
	    }
	    for (i = 0; i < NUM_THREADS; i++)
		receive_thread_counts[i] = 0;

	    /* Tell the remote send process to go ahead.  */
	    while ((buffer
		    = FLIPC_endpoint_buffer_acquire(send_shared_endpoint))
		   == FLIPC_BUFFER_NULL)
		;
	    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
		   fr, FLIPC_SUCCESS, 1);
	    ftest1(FLIPC_endpoint_buffer_release(send_shared_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);
	}
    }
    return (any_t) 0;
}

any_t recvgang_send(any_t arg)
{
    int i, j;
    FLIPC_buffer_t buffer;
    FLIPC_return_t fr;

    mutex_lock(&printf_mutex);
    printf("S");
    fflush(stdout);
    mutex_unlock(&printf_mutex);
    for (i = 0; i < REPETITIONS; i++) {
	mutex_lock(&printf_mutex);
	printf("s");
	fflush(stdout);
	mutex_unlock(&printf_mutex);
	for (j = 0; j < NUM_THREADS*10; j++) {
	    while ((buffer
		    = FLIPC_endpoint_buffer_acquire(send_shared_endpoint))
		   == FLIPC_BUFFER_NULL)
		;
	    mutex_lock(&printf_mutex);
	    sprintf(buffer, checkphrase, j, remote_shared);
	    mutex_unlock(&printf_mutex);
	    ftest1(FLIPC_buffer_set_destination(buffer, remote_shared),
		   fr, FLIPC_SUCCESS, 1);
	    ftest1(FLIPC_endpoint_buffer_release(send_shared_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);
	}

	/* Wait for a go-ahead from the remote side.  */
	while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	       == FLIPC_BUFFER_NULL)
	    ;
	ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	       fr, FLIPC_SUCCESS, 1);
	mutex_lock(&printf_mutex);
	printf("*");
	fflush(stdout);
	mutex_unlock(&printf_mutex);
    }
    return (any_t) 0;
}

any_t groupgang_recv(any_t thread_num_arg)
{
    int thread_num = (int) thread_num_arg;
    int loopvar;
    int message_index, endpoint_address;
    FLIPC_address_t exp_addr1, exp_addr2;
    int total, tmp, i, scanf_result;
    FLIPC_endpoint_t tmp_endpoint;
    FLIPC_return_t fr;
    FLIPC_buffer_t buffer, buffertmp;

    ftest1(FLIPC_endpoint_address(receive_shared_endpoint1,
				  &exp_addr1),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_address(receive_shared_endpoint2,
				  &exp_addr2),
	   fr, FLIPC_SUCCESS, 1);

    for (loopvar = 0; loopvar < REPETITIONS; loopvar++) {
	while (1) {			/* We get out by breaking.  */
	    /* Attempt to grab a message.  */
	    while ((buffer = FLIPC_epgroup_get_message(receive_shared_epgroup, 0,
						       &tmp_endpoint, 0))
		   == FLIPC_BUFFER_NULL
		   && global_loopvar == loopvar)
		;

	    if (global_loopvar > loopvar)
		/* We shouldn't have a buffer here, as someone else
		   has signaled that we have received all the
		   messages.  Simply break.  */
		break;

	    /* We've got a message.  Check it.  */
	    mutex_lock(&printf_mutex);
	    message_index = -1;
	    endpoint_address = -1;
	    scanf_result = sscanf((char*) buffer, checkphrase,
				  &message_index, &endpoint_address);
	    mutex_unlock(&printf_mutex);
	    ftest1(scanf_result, tmp, 2, 1);
	    if (tmp_endpoint == receive_shared_endpoint1)
		ftest1(endpoint_address, tmp, exp_addr1, 1);
	    else
		ftest1(endpoint_address, tmp, exp_addr2, 1);
	    /* Possible race here; see comment in recvgang code.  */
	    ftest1(message_counts[message_index], buffertmp,
		   FLIPC_BUFFER_NULL, 1);
	    message_counts[message_index] = buffer;

	    /* Give it back.  */
	    ftest1(FLIPC_endpoint_buffer_release(tmp_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);

	    /* Increment your variable.  */
	    receive_thread_counts[thread_num]++;

	    /* Check the counts.  */
	    total = 0;
	    for (i = 0; i < NUM_THREADS; i++)
		total += receive_thread_counts[i];

	    if (total == NUM_THREADS * 10) 
		/* This is the loop exit condition.  However, more
		   than one thread may reach this point because of
		   racing in above setting/checking code.  So it needs
		   to be written to be correct if executed by multiple
		   threads.  */
		global_loopvar = loopvar+1;
	}

	/* We need to synchronize here; we can't afford to allow more
	   messages through until there's no chance that this round of thread
	   will grab them.  For synchronization, everyone will set their
	   thread counters to -1, and thread zero will spin until it sees
	   that everything is -1.  */
	receive_thread_counts[thread_num] = -1;

	if (thread_num == 0) {
	    /* Only thread zero does the final checks and zeroing.
	       The others wander forward and start checking for buffers, which
	       won't arrive until thread 0 is done.  */
	    /* Is everyone out of the above loop?  */
	    for (i = 0; i < NUM_THREADS; i++)
		if (receive_thread_counts[i] != -1)
		    i--;

	    for (i = 0; i < NUM_THREADS*10; i++) {
		ftestnot(message_counts[i], buffertmp, FLIPC_BUFFER_NULL, 1);
		message_counts[i] = FLIPC_BUFFER_NULL;
	    }
	    for (i = 0; i < NUM_THREADS; i++)
		receive_thread_counts[i] = 0;

	    /* Tell the remote send process to go ahead.  */
	    while ((buffer = FLIPC_endpoint_buffer_acquire(send_shared_endpoint))
		   == FLIPC_BUFFER_NULL)
		;
	    ftest1(FLIPC_buffer_set_destination(buffer, remote_control),
		   fr, FLIPC_SUCCESS, 1);
	    ftest1(FLIPC_endpoint_buffer_release(send_shared_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);
	}
    }
    return (any_t) 0;
}

any_t groupgang_send(any_t arg)
{
    int i, j;
    FLIPC_buffer_t buffer;
    FLIPC_return_t fr;

    for (i = 0; i < REPETITIONS; i++) {
	for (j = 0; j < NUM_THREADS*10; j++) {
	    while ((buffer
		    = FLIPC_endpoint_buffer_acquire(send_shared_endpoint))
		   == FLIPC_BUFFER_NULL)
		;
	    mutex_lock(&printf_mutex);
	    sprintf(buffer, checkphrase, j, (j % 2
					     ? remote_receive_shared1
					     : remote_receive_shared2));
	    mutex_unlock(&printf_mutex);
	    ftest1(FLIPC_buffer_set_destination(buffer,
						(j % 2
						 ? remote_receive_shared1
						 : remote_receive_shared2)),
		   fr, FLIPC_SUCCESS, 1);
	    ftest1(FLIPC_endpoint_buffer_release(send_shared_endpoint, buffer),
		   fr, FLIPC_SUCCESS, 1);
	}

	/* Wait for a go-ahead from the remote side.  */
	while ((buffer = FLIPC_endpoint_buffer_acquire(control_receive))
	       == FLIPC_BUFFER_NULL)
	    ;
	ftest1(FLIPC_endpoint_buffer_release(control_receive, buffer),
	       fr, FLIPC_SUCCESS, 1);
	mutex_lock(&printf_mutex);
	printf("s");
	fflush(stdout);
	mutex_unlock(&printf_mutex);
    }
    return (any_t) 0;
}











