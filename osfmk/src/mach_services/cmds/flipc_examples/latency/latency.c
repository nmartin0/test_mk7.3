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
 * latency.c
 */

#include <flipc.h>
#include <mach.h>
#include <mach/time_value.h>
#include <mach/mach_norma.h>
#include <mach/sync_policy.h>
#include <mach/norma_special_ports.h>
#include <servers/netname.h>
#include <device/device.h>

#include <stdio.h>
#include <math.h>

/* 
 * This program is a flipc program designed to measure the latency of
 * the underlying flipc subsytem.  It is also intended to serve as an
 * introduction to the flipc system.  As such, reports of bugs in
 * either the program or its comments are welcome.
 *
 * This program runs in two instances, a server and a client.  The
 * server's function is simply to bounce the messages the client sends
 * back at it.  The client will calculate the amount of time the
 * messages take round trip, and report statistics to the user.
 *
 * The client will run for a user-specified number of rounds.  The
 * first round will measure the latency of a single message (ie. no
 * averaging) and later rounds will increase the number of messages
 * used to calculate the per-message latency (by a user-specified
 * factor).  The user also specified how many times each round will be
 * run--the result output for each round will be the average from all
 * runs.
 *
 * The server must be invoked first.  As part of its initial processing,
 * it will print out an endpoint address, which must then be passed as
 * an argument to the client.  This allows communications bootstrap
 * between the two processes.
 */

/*
 * Arguments this program accepts:
 * 	-s		Program should function as a the server side
 *			of the connction; any additional arguments
 *			(other than -b) will be considered an error.
 *	-c		[Default] Program will function as the client
 *			side of the connection.
 *	-m <messages>	Number of messages for first round.
 *	-f <factor>	Factor to increase the number of messages by
 * 			for each new round.  This factor is multiplicative
 *			unless -a is passed, in which case it is additive.
 *	-r <rounds>	The number of rounds to run.
 *	-b <buffers>	How many buffers will be allocated on the
 * 			various endpoints.
 *	-n <repeats>	How many times to run each round for
 * 			averaging.
 *	-x <ep_addr>	Address of server endpoint--required.
 *	-d 		Attempt to measure "dispatch latency" ie. the
 *			amount of time spent on the client in sending
 *			out a single message.  This is done through
 *			putting extra messages into the pipe so that
 *			the client should always have a message ready
 *			to send or receive when it attempts a send or
 *			receive.  BUFFERS - 1 extra messages
 *			are put in at the beginning, and pulled out at
 *			the end, to "prime the pump".  Note that this
 *			flag isn't very useful without the -b flag as
 *			well, since the default BUFFERS is 2.
 *			Also note that this argument
 *	-a		Indicate that the factor specified for
 *			increasing the number of messages each round should
 *			be *additive*, not multiplicative.
 */

/*
 * Program outline.
 *
 * Upon startup, the program will:
 *	*) Parse its arguments and perform error checking on them (eg.
 * 	   specifying -s with any other arguments is an error).
 *	*) Initialize the flipc domain locally, or attach to a
 * 	   previously initialized flipc domain.  If attaching, it will
 * 	   check to make sure that the parameters of the domain are
 * 	   large enough to satisfy the user requests.
 *	*) Create a send and a receive endpoint.
 *	*) The server will print out the address of its receive endpoint.
 *	   This must have been given as an argument to the client program. 
 *	   Based on this information, the client will send the server
 *	   a message containing its receive endpoint address.
 *	*) The client will send a single message to the server telling
 * 	   it how many messages total to expect for this program run.
 * 	   This allows the server to know when to exit.
 *     [*) From this point forward the server is simply mirroring back
 * 	   to the client any message it receives.]
 *	*) Map the clock device to get (hopefully) accurate
 * 	   measurements of the round-trip message time.
 *	*) The client will now bounce a single message off of the
 * 	   server to confirm that the server is now actively polling
 * 	   for messages.
 *	*) The client now iterates, through the number of rounds that
 * 	   it has been asked for, and the number of repeats of each
 * 	   round that it has been asked for.  For each of these
 * 	   repeats, it (in a subroutine):
 *		*) Gets the current time.
 *		*) For each message it should send this round:
 *			*) Spin to acquire a buffer from the send
 * 	   		   endpoint. 
 *			*) Set the buffer destination.
 *			*) Release the buffer to the send endpoint.
 *			*) Acquire a buffer from the receive endpoint.
 * 	   		*) Release that buffer back to the receive
 * 	   		   endpoint. 
 *		*) Get the current time.
 *		*) Subtract the first time from the second, and divide
 * 	   	   by 2*<number of messages in this round>.  This is
 * 	   	   the one-way latency for messages from this round.
 *	*) For each round, the number of messages, number of
 * 	   repetitions of the round, mean and standard deviation of
 * 	   the one-way latency for that round are printed.
 *	*) After all messages have been sent, both server and client
 * 	   deallocate all flipc structures they have used and detach
 * 	   from their local flipc domains.
 *	*) The server then deregisters itself from the netname service
 * 	   and exits.
 *	*) The client then unmaps the clock device and exits.
 */

/*
 * Fast timer.  Intermediate representation is a timer_sample_data;
 * timer_convert converts this quantity to a double containing seconds
 * and fractional seconds to the resolution of the fast clock.  See
 * time_trace.h for more details.
 */
#define TIMING
#include <time_trace.h>

/*
 * We may use this program for very detailed timings of the flipc routines.
 * If so, we allow the definition of TIMER_SAMPLE_LOG through.  Otherwise,
 * we nix it.
 */
#ifndef FLIPC_TIMING_TRACE
#undef TIMER_SAMPLE_LOG
#define TIMER_SAMPLE_LOG(note) do { ; } while (0)
#endif

/*
 * Forward declarations.
 */
void usage(void);		/* Usage message.  */
void parse_args(int argc, char **argv);	/* Argument parsing.  */
FLIPC_domain_t setup_flipc(void); /* Setup the flipc domain.  */

/* Exchange flipc receive addresses.  */
FLIPC_address_t initial_bootstrap_server(FLIPC_endpoint_t send_endpoint,
					 FLIPC_endpoint_t receive_endpoint,
					 FLIPC_address_t local_address);
FLIPC_address_t initial_bootstrap_client(FLIPC_endpoint_t send_endpoint,
					 FLIPC_endpoint_t receive_endpoint,
					 FLIPC_address_t local_address);

/* Exchange initial setup information.  */
int server_handshake(FLIPC_endpoint_t receive_endpoint);
void client_handshake(FLIPC_endpoint_t send_endpoint,
		      FLIPC_address_t remote_address,
		      int start_messages, int number_rounds,
		      int round_factor, int round_repeats,
		      boolean_t additive_factor_p);

void local_thread_yield(void);	/* thread_yield fn for flipc.  */

/* Bounce number_messages through the endpoints and return.  */
void server_loop(int number_messages,
		 FLIPC_endpoint_t send_endpoint,
		 FLIPC_endpoint_t receive_endpoint,
		 FLIPC_address_t remote_address);

/* Send number_messages through the endpoints, and return the number
   of seconds it takes.  */
double client_round_time(int number_messages,
			 int prime_pump,
			 FLIPC_endpoint_t send_endpoint,
			 FLIPC_endpoint_t receive_endpoint,
			 FLIPC_address_t remote_address);

/*
 * Estimate the latency of a one way message, using the various
 * options passed to the routines.
 */
void client_measure_latency(FLIPC_endpoint_t send_endpoint,
			    FLIPC_endpoint_t receive_endpoint,
			    FLIPC_address_t remote_address,
			    int number_rounds, int round_factor,
			    int round_repeats, int start_messages,
			    int prime_pump, boolean_t additive_factor_p);


/*
 * Argument defaults.
 */
#define ROUND_FACTOR 4
#define NUMBER_ROUNDS 5
#define ROUND_REPEATS 3
#define NUMBER_BUFFERS 10
#define START_MESSAGES 2

/*
 * Argument variables; these are initialized to 0 and then set from
 * the argument list.  They are then error checked against each other
 * (eg. server_p vs. anything else being set).  After this point, any
 * still zero arguments are initialized from the compile time values
 * above. 
 */
boolean_t server_p = FALSE;
boolean_t client_p = FALSE;
boolean_t measure_dispatch_p = FALSE;
boolean_t additive_factor_p = FALSE;
int round_factor = 0;		/* Factor to increase number of
				   messages each round.  */
int number_rounds = 0;		/* Number of rounds to run.  */
int round_repeats = 0;		/* Number of times to repeat each round.  */
int number_buffers = 0;		/* Buffers per endpoint.  */
int start_messages = 0;		/* Number of messages for first round.  */
FLIPC_address_t server_remote_address = (FLIPC_address_t) 0;

/*
 * Global variable visible throughout the file for the name of the program.
 */
char *progname = (char *) 0;

/*
 * Argument parsing routine.  Also does error checking on arguments and
 * defaulting of arguments.
 */
void
parse_args(int argc, char **argv)
{
    /* Save program name.  */
    progname = argv[0];
    
    /* Parse arguments.  */
    while (++argv, --argc) {
	if (**argv != '-')
	    usage();
	switch((*argv)[1]) {
	  case 'a':
	    additive_factor_p = TRUE;
	    break;
	  case 's':
	    server_p = TRUE;
	    break;
	  case 'c':
	    client_p = TRUE;
	    break;
	  case 'f':
	    if (argc < 2)
		usage();
	    round_factor = atoi(argv[1]);
	    ++argv; --argc;
	    break;
	  case 'd':
	    measure_dispatch_p = TRUE;
	    break;
	  case 'm':
	    if (argc < 2)
		usage();
	    start_messages = atoi(argv[1]);
	    ++argv; --argc;
	    break;
	  case 'r':
	    if (argc < 2)
		usage();
	    number_rounds = atoi(argv[1]);
	    ++argv; --argc;
	    break;
	  case 'n':
	    if (argc < 2)
		usage();
	    round_repeats = atoi(argv[1]);
	    ++argv; --argc;
	    break;
	  case 'b':
	    if (argc < 2)
		usage();
	    number_buffers = atoi(argv[1]);
	    ++argv; --argc;
	    break;
	  case 'x':
	    if (argc < 2)
		usage();
	    server_remote_address =
		strtol(argv[1], (char **) 0, 16);
	    ++argv; --argc;
	    break;
	}
    }
    
    /* Error check arguments.  */
    if (server_p && client_p)
	usage();
    
    if (server_p &&
	(measure_dispatch_p
	 || (start_messages != 0)
	 || (round_repeats != 0)
	 || (number_rounds != 0)
	 || (round_factor != 0)
	 || (server_remote_address != 0)))
	usage();

    if (client_p && server_remote_address == 0)
	usage();

    /* Default values.  */
    if (!server_p && !client_p)
	client_p = TRUE;
    
    if (client_p) {
	if (round_repeats == 0)
	    round_repeats = ROUND_REPEATS;
	if (round_factor == 0)
	    round_factor = ROUND_FACTOR;
	if (number_rounds == 0)
	    number_rounds = NUMBER_ROUNDS;
	if (start_messages == 0)
	    start_messages = START_MESSAGES;
    }
    /* For both.  */
    if (number_buffers == 0)
	number_buffers = NUMBER_BUFFERS;

    /*
     * This next check is because our algorithm for bouncing messages
     * back from the server requires at least two buffers.  See the
     * comments above the server_loop function for details.
     */
    if (number_buffers == 1) {
	fprintf(stderr,
		"There must be at least 2 buffers on each endpoint.\n");
	exit(-1);
    }

    /* If we are measuring the dispatch latency, we need to start with
       at least enough messages to make "priming the pump" work.  */
    if (measure_dispatch_p
	&& (number_buffers - 1) >= start_messages) {
	fprintf(stderr, "Can't start out with more messages in transit then sending.\n");
	exit(-1);
    }
}


/*
 * Usage.
 */
void
usage()
{
    fprintf(stderr, "Usage: %s [-b <bufs>] (-s|-c -x <addr> [-d] [-a] [-f <fact>] [-r <rounds>] [-n <repeats>])\n", progname);
    exit(-1);
}

/*
 * Yield function for flipc; it doesn't need to do anything with
 * threads since this is a single threaded program.  It does, however,
 * need to be able to yield to other tasks if that should be required.
 * This is because, for this implementation of flipc, the allocations
 * lock is implemented by spinning on a simple lock and calling the
 * thread yield function.  If there is another flipc process on this
 * node, that thread yield function must be able to give it control.
 */
void
local_thread_yield(void)
{
    sleep(0);
}

/*
 * Helper routines for initialization to print out informative error
 * messages for the various errors that may happen during init.
 */
void
init_error(FLIPC_return_t fr)
{
}

/*
 * The code.
 */
void
main(int argc, char **argv)
{
    FLIPC_return_t fr;
    FLIPC_domain_t domain;
    FLIPC_endpoint_t send_endpoint, receive_endpoint;
    FLIPC_address_t local_receive_address, remote_receive_address;
    FLIPC_buffer_t buffer;
    int total_server_messages;
    int rounds, round_messages;
    
    /* Parse arguments and error check.  */
    parse_args(argc, argv);
    
    /* Setup the flipc domain.  */
    domain = setup_flipc();
    
    /*
     * Allocate some endpoints.  These calls may fail if either we
     * attached to some previously initialized domain in setup_flipc(),
     * or someone else is using resources that we asked for during
     * initialization.
     */
    
    fr = FLIPC_endpoint_allocate(domain, number_buffers, FLIPC_Receive,
				 0, &receive_endpoint);
    if (fr != FLIPC_SUCCESS) {
	/* 
	 * All of the possible errors are either "can't happen"s
	 * (because initialization should have taken care of it)
	 * or indicate a lack of space.  So we'll do a lack of space
	 * error message.
	 */

	fprintf(stderr,
		"Couldn't allocate endpoint or buffers on endpoint allocate call(%d).\n", fr);
	exit(-1);
    }
    fr = FLIPC_endpoint_allocate(domain, number_buffers, FLIPC_Send,
				 0, &send_endpoint);
    if (fr != FLIPC_SUCCESS) {
	fprintf(stderr,
		"Couldn't allocate endpoint or buffers on endpoint allocate call.\n");
	exit(-1);
    }
    
    /* Get the receive endpoint address.  */
    fr = FLIPC_endpoint_address(receive_endpoint, &local_receive_address);
    if (fr != FLIPC_SUCCESS) {
	fprintf(stderr,
		"FLIPC_endpoint_address indicated endpoint not valid.\n");
	fprintf(stderr,
		"This shouldn't happen (since the endpoint was just successfully allocated).\n");
	exit(-1);
    }
    
    /* We now split into two personalities for the server and the client. */
    if (server_p) {
	/*
	 * Do the initial bootstrap.  This involves the client and the
	 * server exchanging receive addresses.  The client will send
	 * its address to the server through regular mach ipc, and the
	 * server will return its address to the client through FLIPC.
	 */
	remote_receive_address =
	    initial_bootstrap_server(send_endpoint, receive_endpoint,
				     local_receive_address);
	
	/*
	 * The client now tells the server how many messages to process.
	 */
	total_server_messages = server_handshake(receive_endpoint);
	
	/*
	 * The server now has its marching orders; it can go off and
	 * do its own thing (bouncing messages back to the client).
	 */

	server_loop(total_server_messages, send_endpoint,
		    receive_endpoint, remote_receive_address);
    } else {			/* Client side.  */
	/* 
	 * Do the initial bootstrap.  This involves the client and the
	 * server exchanging receive addresses.  The client will send
	 * its address to the server through regular mach ipc, and the
	 * server will return its address to the client through FLIPC.
	 */
	remote_receive_address =
	    initial_bootstrap_client(send_endpoint, receive_endpoint,
				     local_receive_address);
	
	/*
	 * The client now tells the server how many messages to process.
	 */
	client_handshake(send_endpoint, remote_receive_address,
			 start_messages, number_rounds, round_factor,
			 round_repeats, additive_factor_p);
	
	/*
	 * The client now does its thing, bouncing messages off the
	 * server and measuring the time it takes according to the
	 * users requests.
	 */
	client_measure_latency(send_endpoint, receive_endpoint,
			       remote_receive_address, number_rounds,
			       round_factor, round_repeats, start_messages,
			       (measure_dispatch_p
				? number_buffers - 1
				: 1),
			       additive_factor_p);
    }
    
    /* Flipc cleanup is identical for server and client.  */
    
    /*
     * There is no reason for the following deallocates to fail, and
     * not much we can do if they do fail, so we will ignore the error
     * codes.  Note that it is important that we do this deallocate,
     * because if we don't the resources that we have allocated remain
     * allocated after we exit until the last task on this node using
     * flipc has exitted.
     */
    (void) FLIPC_endpoint_deallocate(send_endpoint);
    (void) FLIPC_endpoint_deallocate(receive_endpoint);
    
    /*
     * The above is true for the following call also.  In addition,
     * the equivalent of a FLIPC_domain_detach is executed whenever a
     * process exits (all this means is that the last process to exit
     * on a node will cause the flipc domain on that node to be torn
     * down.
     */
    (void) FLIPC_domain_detach(domain);

#ifdef FLIPC_TIMING_TRACE
    /*
     * We dump the trace log to stdout for both client and server.
     */
    timer_dump_log_naked(stderr);
#endif    
    
    exit(0);
}

/*
 * We assume we enter this function with space for at least one
 * extra buffer on the send endpoint.
 * This is so that we can release our buffer to the send endpoint
 * without worrying about whether or not it will be rejected.
 *
 * We assume that we are passed valid endpoints and addresses,
 * allowing us to be confident the the various calls we are making
 * won't return errors (so we don't have to check for them).
 *
 * We also assume that there is more than one buffer on each endpoint.
 * If this is not the case, we could receive another message on the
 * receive endpoint (which would be dropped) between the time we
 * released the original buffer to the send endpoint and released
 * the replacement buffer to the receive endpoint.
 */
void
server_loop(int number_messages,
	    FLIPC_endpoint_t send_endpoint,
	    FLIPC_endpoint_t receive_endpoint,
	    FLIPC_address_t remote_address)
{
    int i;
    FLIPC_buffer_t buffer;
    
#ifdef FLIPC_TIMING_TRACE
    TIMER_INIT();
#endif

    /* Bounce number_messages and return.  */
    for (i = 0; i < number_messages; i++) {
	
	/* Get a message from the receive endpoint and put it onto
	   the send endpoint.  We spin on buffer_available to avoid
	   taking the lock each time around the loop.  */
	while (!(FLIPC_endpoint_buffer_available(receive_endpoint)
		 && ((buffer = FLIPC_endpoint_buffer_acquire_unlocked(receive_endpoint))
		     != FLIPC_BUFFER_NULL)))
	    ;
	FLIPC_buffer_set_destination(buffer, remote_address);
	FLIPC_endpoint_buffer_release_unlocked(send_endpoint, buffer);
	
	/*
	 * Pull a buffer from the send endpoint and put it on the
	 * receive endpoint.  This is to keep the number of buffers
	 * on each endpoint balanced, so that we will continue to have
	 * buffers available on the receive endpoint with which to
	 * receive messages.
	 */

	/*
	 * A spin is required here in the general case because the
	 * message engine may be backed up in delivering messages from
	 * us, and hence may not have processed all previous messages
	 * we have sent from this endpoint.  In this specific algorithm
	 * we're ok because there are at least two buffers on the send
	 * endpoint, and we can't get a new message above until one of
	 * the buffers' messages has been sent and the buffer freed up.
	 * Thus, here, we actually don't need to spin.  But for good
	 * coding practice.
	 */

	while ((buffer = FLIPC_endpoint_buffer_acquire_unlocked(send_endpoint))
	       == FLIPC_BUFFER_NULL)
	    ;
	FLIPC_endpoint_buffer_release_unlocked(receive_endpoint, buffer);
    }
}

FLIPC_domain_t
setup_flipc(void)
{
    struct FLIPC_domain_info domain_info;
    FLIPC_return_t fr;
    FLIPC_domain_t domain;
    
    /* Attach to a flipc domain.  */
    
    /*
     * We try to initialize the local domain with enough space to
     * handle both the server and the client.  If this fails because
     * the domain is already initialized, we attach to it.
     */
    
    /* Setup for both client and server.  */
    domain_info.max_endpoints = 4;
    domain_info.max_epgroups = 0;
    domain_info.max_buffers = number_buffers * 4;
    /*
     * We need an extra bit of room on the endpoints to allow
     * release to the send endpoint to occur before acquiring a buffer
     * from the send endpoint.  See the server_loop function for
     * an example.
     */
    domain_info.max_buffers_per_endpoint = number_buffers + 1;
    domain_info.yield_fn = &local_thread_yield;
    domain_info.policy = SYNC_POLICY_FIFO;
    domain_info.msg_buffer_size = 0;
    bzero(&domain_info.performance, sizeof(domain_info.performance));
    domain_info.error_log_size = 0;
    
    /*
     * Try initialization.  We use domain index zero as that is currently
     * the only valid domain index.
     */
    fr = FLIPC_domain_init(0, &domain_info, &domain);
    
    /* If it's already initialized, attach to it instead.  */
    if (fr == FLIPC_DOMAIN_INITIALIZED) {
	bzero(&domain_info, sizeof(domain_info));
	domain_info.yield_fn = &local_thread_yield;
	fr = FLIPC_domain_attach(0, &domain_info, &domain);
    }
    
    /* Do general error returns to the user here.  */
    if (fr != FLIPC_SUCCESS)
	switch (fr) {
	  case FLIPC_DOMAIN_NOT_AVAILABLE:
	    fprintf(stderr,
		    "Flipc domain not available to this application\n");
	    fprintf(stderr,
		    "Possibly you aren't running as UID zero?\n");
	    exit(-1);
	  case FLIPC_DOMAIN_OP_IN_PROGRESS:
	    fprintf(stderr, "Flipc domain op in progress.\n");
	    fprintf(stderr, "This \"can't happen\" in a single-threaded program.\n");
	    exit(-1);
	  case FLIPC_DOMAIN_ATTACHED:
	    fprintf(stderr, "Flipc domain already attached.\n");
	    fprintf(stderr, "This \"can't happen\" in this program, since no\n");
	    fprintf(stderr, "ops are called before this point.\n");
	    exit(-1);
	  case FLIPC_DOMAIN_NO_THREAD_YIELD:
	    fprintf(stderr, "Initialization called with a bad thread yield function.\n");
	    fprintf(stderr, "This shouldn't have happened since we passed in a \n");
	    fprintf(stderr, "perfectly good thread yield function.\n");
	    exit(-1);
	  case FLIPC_DOMAIN_RESOURCE_SHORTAGE:
	    fprintf(stderr, "Not enough space in the kernel communications buffer.\n");
	    exit(-1);
	  default:
	    fprintf(stderr, "Unexpected error %d on initialization.\n", fr);
	    exit(-1);
	}
    
    /*
     * We check the message buffer size here.  We don't need a lot of
     * room for this application, but we do need some.
     */
    fr = FLIPC_domain_query(domain, &domain_info);
    if (fr != FLIPC_SUCCESS) {
	fprintf(stderr,
		"FLIPC_domain_query failed; shouldn't happen.\n");
	exit(-1);
    }
    if (domain_info.msg_buffer_size < sizeof(FLIPC_address_t)) {
	fprintf(stderr, "The message buffer size is less than %d bytes.\n",
		sizeof(FLIPC_address_t));
	fprintf(stderr, "The application cannot run.\n");
	exit(-1);
    }
    
    return domain;
}

/*
 * Do everything required on the server side to do the initial 
 * exchange of flipc addresses.
 */
FLIPC_address_t
initial_bootstrap_server(FLIPC_endpoint_t send_endpoint,
			 FLIPC_endpoint_t receive_endpoint,
			 FLIPC_address_t local_address)
{
    FLIPC_buffer_t buffer;
    FLIPC_address_t remote_address;
    
    /*
     * We need to tell the client our endpoint address.  We do this
     * by telling the user, and letting the user invoke the client
     * with this information as an argument.
     */
    printf("Server receive endpoint address: 0x%x\n",
	   local_address);

    /* The client will now tell us his address.  */

    /*
     * We spin, attempting to acquire a buffer from the
     * receive_endpoint.  The first buffer available for acquisition
     * on the receive endpoint will be the buffer sent by the client
     * with our information in it.
     */
    while ((buffer = FLIPC_endpoint_buffer_acquire(receive_endpoint))
	   == FLIPC_BUFFER_NULL)
	;
    
    /* We copy the information out of the buffer.  */
    remote_address = *(FLIPC_address_t *)buffer;
    
    /*
     * And we release the buffer back to the receive endpoint,
     * returning the receive resource to the system.  Again, there is
     * space because we got this buffer off of this endpoint above,
     * and no-one else on this node could have taken the space.
     */
    (void) FLIPC_endpoint_buffer_release(receive_endpoint, buffer);

    return remote_address;
}

/*
 * Do everything required on the client side to do the initial
 * exchange of flipc addresses.
 */
FLIPC_address_t
initial_bootstrap_client(FLIPC_endpoint_t send_endpoint,
			 FLIPC_endpoint_t receive_endpoint,
			 FLIPC_address_t local_address)
{  
    FLIPC_buffer_t buffer;
    FLIPC_return_t fr;
    
    /*
     * We know the servers remote address from the passed arguments.
     * We simply need to tell it what our's is.
     */

    /* Spin in case we need to wait for previous sends to complete.  */
    while ((buffer = FLIPC_endpoint_buffer_acquire(send_endpoint))
	   == FLIPC_BUFFER_NULL)
	;
    
    /*
     * Given that the server remote address is passed in on the
     * command line, we really should check to make sure that it
     * is valid.  Most possible errors will not be caught by this
     * function, however.
     */
    fr = FLIPC_buffer_set_destination(buffer, server_remote_address);
    if (fr != FLIPC_SUCCESS) {
	fprintf(stderr, "Bad server address argument.\n");
	exit(-1);
    }
    
    /*
     * The buffer variable points directly into the message space;
     * we can assign into that space as we wish, up to the message
     * buffer size.  We made sure in error checking that we had
     * enough space for the messages we would be sending.
     */
    *(FLIPC_address_t *)buffer = local_address;
    
    /*
     * Release the buffer back to the send endpoint.  A possible error
     * return here might be the endpoint being full, but since we
     * acquired the buffer from the endpoint above, and there are no
     * other threads in this applications to use up the space that
     * used to be taken up by the endpoint, releasing it should be
     * safe. This sends the message.
     */
    (void) FLIPC_endpoint_buffer_release(send_endpoint, buffer);

    /* The return value from this function is the server address.  */
    return server_remote_address;
}

/*
 * The server receives a message telling it how many messages to process.
 */
int
server_handshake(FLIPC_endpoint_t receive_endpoint)
{
    FLIPC_buffer_t buffer;
    int total_server_messages;
    
    while ((buffer = FLIPC_endpoint_buffer_acquire(receive_endpoint))
	   == FLIPC_BUFFER_NULL)
	;
    total_server_messages = *(int *)buffer;
    (void) FLIPC_endpoint_buffer_release(receive_endpoint, buffer);
    
    return total_server_messages;
}

/*
 * The client figures out how many messages it's going to send and tells
 * the server that number.
 */
void
client_handshake(FLIPC_endpoint_t send_endpoint,
		 FLIPC_address_t remote_address,
		 int start_messages, int number_rounds,
		 int round_factor, int round_repeats,
		 boolean_t additive_factor_p)
{
    int total_messages = 0;
    int factor_total = start_messages;	
    int i;
    FLIPC_buffer_t buffer;
    
    /*
     * Total messages the server has to process is
     * sum(i=0, i < number_rounds, round_repeats * round_factor^i) + 1.
     * The extra 1 is the initial message that the client pings off the
     * server to sync up.
     */
    for (i = 0; i < number_rounds; i++) {
	total_messages += factor_total;
	if (additive_factor_p)
	    factor_total += round_factor;
	else 
	    factor_total *= round_factor;
    }
    total_messages *= round_repeats;
    total_messages += 2;
    
    /*
     * See above for arguments for ignoring error codes; note that
     * the client side send_endpoint has not yet been used.
     */
    
    buffer = FLIPC_endpoint_buffer_acquire(send_endpoint);
    (void) FLIPC_buffer_set_destination(buffer, remote_address);
    *(int *)buffer = total_messages;
    (void) FLIPC_endpoint_buffer_release(send_endpoint, buffer);
}

/*
 * For a range of parameters, measure the latency of a single
 * flipc message.
 */
void
client_measure_latency(FLIPC_endpoint_t send_endpoint,
		       FLIPC_endpoint_t receive_endpoint,
		       FLIPC_address_t remote_address,
		       int number_rounds, int round_factor,
		       int round_repeats, int start_messages,
		       int prime_pump, boolean_t additive_factor_p)
{
    int rounds, round_messages;
    
    /* Map the clock to get accurate timing values.  */
    TIMER_INIT();
    
    /*
     * Bounce a single message off of the server to sync up.
     * Doing this through the client_round_time subroutine also pages in
     * the clock.
     */

    (void) client_round_time(2, 1, send_endpoint, receive_endpoint,
			     remote_address);
    
    /*
     * The meat of the program.  We now iterate overall all the values
     * that the user has asked above, calling a routine for each round
     * and reporting results from that round.
     */

    /* Print out header.  */
    printf("Round\t\tMessages\tRepetitions\tMean (s)\tStd dev (s)\n");
    printf("-----\t\t--------\t-----------\t--------\t-----------\n");
    for (rounds = 0, round_messages = start_messages;
	 rounds < number_rounds;
	 rounds++, (additive_factor_p
		    ? (round_messages += round_factor)
		    : (round_messages *= round_factor)) ) {
	double sumxisq=0.0, sumxi=0.0, mean, variance, stddev;
	int repeats;
	char output_buffer[160];
	int dec_ptr, sign;
	char *decstring, *place_holder;
	
	for (repeats = 0; repeats < round_repeats; repeats++) {
	    double xi = client_round_time(round_messages,
					  prime_pump,
					  send_endpoint,
					  receive_endpoint,
					  remote_address);
	    /*
	     * Divide by 2 to get one way time, and by
	     * round_messages to get single message time.
	     */
	    xi /= (double) round_messages * 2;
	    sumxi += xi;
	    sumxisq += xi*xi;
	}
	
	mean = sumxi / (double)round_repeats;
	variance = ((sumxisq
		     - 2 * sumxi * mean
		     + mean * mean * ((double)round_repeats))
		    / (double)round_repeats);
	stddev = sqrt(variance);
	
	/*
	 * We are in the unfortunate position of having to convert
	 * the floating point numbers by hand to work around a bug in the
	 * PGI compiler.
	 */

	/* We print what we can without asking sprintf to deal with floats.  */
	sprintf(output_buffer, "%d\t\t%d\t\t%d\t\t",
		rounds, round_messages, round_repeats);
	place_holder = output_buffer + strlen(output_buffer);

	/* Convert the mean and add it to the string.  */
	decstring = fcvt(mean, 6, &dec_ptr, &sign);
	if (sign)
	    strcat(output_buffer, "-");
	if (dec_ptr < 0) {
	    strcat(output_buffer, "0.");
	    while (dec_ptr++)
		strcat(output_buffer, "0");
	}
	strcat(output_buffer, decstring);
	
	/* If it's smaller than the field width, add another tab.  */
	strcat(output_buffer, (output_buffer + strlen(output_buffer)
			       < place_holder+8) ? "\t\t" : "\t");

	/* Convert the stddev and add it to the string.  */
	decstring = fcvt(stddev, 6, &dec_ptr, &sign);
	if (sign)
	    strcat(output_buffer, "-");
	if (dec_ptr < 0) {
	    strcat(output_buffer, "0.");
	    while (dec_ptr++)
		strcat(output_buffer, "0");
	}
	strcat(output_buffer, decstring);

	/* Print it.  */
	printf("%s\n", output_buffer);
    }
    
    /* Clock cleanup.  */
    TIMER_DESTROY();
}

/*
 * Returns the number of seconds taken to bounce the number of
 * messages requested off of the server.
 */
double
client_round_time(int number_messages,
		  int prime_pump,
		  FLIPC_endpoint_t send_endpoint,
		  FLIPC_endpoint_t receive_endpoint,
		  FLIPC_address_t remote_address)
{
    int start_secs, start_usecs, end_secs, end_usecs;
    int i;
    FLIPC_buffer_t receive_buffer, send_buffer;
    timer_sample_data	stop_time_data, start_time_data, diff;
    double		delta;

    /* number_messages must be > 2; the loop below has a minimum of
       two messages.  */
    if (number_messages < 2) {
	fprintf(stderr, "client_round_time called with number_messages < 2\n");
	exit(-1);
    }

    /* Can't prime with more messages than we have.  */
    if (prime_pump > number_messages) {
	fprintf(stderr, "client_round_time called with prime_pump > number_messages\n");
	exit(-1);
    }

    /* Read starting time.  */
    TIMER_SAMPLE(&start_time_data);
    
    /*
     * Note that this loop doesn't need to worry about keeping the
     * number of buffers on the two endpoints stable, as it always
     * puts a buffer back on the same endpoint it removed it from.
     */
    
    /*
     * Send messages to prime the pump, and then grab a send buffer
     * to get to the right point in the loop.
     */
    for (i = 0; i < prime_pump; i++) {
	while ((send_buffer
		= FLIPC_endpoint_buffer_acquire_unlocked(send_endpoint))
	       == FLIPC_BUFFER_NULL) {
	    ;
	}
	FLIPC_buffer_set_destination(send_buffer, remote_address);
	FLIPC_endpoint_buffer_release_unlocked(send_endpoint, send_buffer);
    }

    while ((send_buffer = FLIPC_endpoint_buffer_acquire_unlocked(send_endpoint))
	   == FLIPC_BUFFER_NULL) {
	;
    }
    FLIPC_buffer_set_destination(send_buffer, remote_address);
    
    /* We go in with a valid buffer in send_buffer, but not in
       receive_buffer.  */
    for (i = 0; i < number_messages - prime_pump - 1; i++) {
	/*
	 * Receive the reply from the server.  
	 * No information in this message; just the fact that we got it
	 * is all we want.
	 */
	while (!(FLIPC_endpoint_buffer_available(receive_endpoint)
		 && ((receive_buffer
		      = FLIPC_endpoint_buffer_acquire_unlocked(receive_endpoint))
		     != FLIPC_BUFFER_NULL)))
	    ;

	/*
	 * Send the message.
	 */
	FLIPC_endpoint_buffer_release_unlocked(send_endpoint, send_buffer);

	/*
	 * From here to the end of the loop is potentially free time;
	 * the message is in transit, so we may as well fill it in with as
	 * much as we can.
	 */
	/* Get the send buffer and set its destination.  */
	while ((send_buffer = FLIPC_endpoint_buffer_acquire_unlocked(send_endpoint))
	       == FLIPC_BUFFER_NULL) {
	    ;
	}
	FLIPC_buffer_set_destination(send_buffer, remote_address);
	
	/* Return the receive buffer back to the receive endpoint.  */
	FLIPC_endpoint_buffer_release_unlocked(receive_endpoint,
					       receive_buffer);
    }

    /* Finish off the loop; receive the messages in transit, and
       send and receive the last one we're holding.  */
    FLIPC_endpoint_buffer_release_unlocked(send_endpoint, send_buffer);

    for (i = 0; i < prime_pump; i++) {
	while (!(FLIPC_endpoint_buffer_available(receive_endpoint)
		 && ((receive_buffer
		      = FLIPC_endpoint_buffer_acquire_unlocked(receive_endpoint))
		     != FLIPC_BUFFER_NULL)))
	    ;
	FLIPC_endpoint_buffer_release_unlocked(receive_endpoint,
					       receive_buffer);
    }

    /* And receive the extra one sent just above.  */
    while (!(FLIPC_endpoint_buffer_available(receive_endpoint)
	     && ((receive_buffer
		  = FLIPC_endpoint_buffer_acquire_unlocked(receive_endpoint))
		 != FLIPC_BUFFER_NULL)))
	;
    FLIPC_endpoint_buffer_release_unlocked(receive_endpoint, receive_buffer);
    
    /* Read ending time.  */
    TIMER_SAMPLE(&stop_time_data);
    TIMER_SUBTRACT(&stop_time_data, &start_time_data, &diff);
    TIMER_CONVERT(&diff, &delta);
    return (delta);
}
