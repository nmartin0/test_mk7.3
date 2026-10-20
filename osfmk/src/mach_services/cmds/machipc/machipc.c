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
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * cmk1.1
 */

#include <mach.h>
#include <mach/msg_type.h>
#include <mach_error.h>
#include <mach/message.h>
#include <servers/netname.h>
#include <stdio.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/resource.h>

#define streql(a, b)	(strcmp((a), (b)) == 0)

static void client_ool(), client_simple();
static void server_ool(), server_simple();
static void null_trap();

struct msg_ool {
	mach_msg_header_t h;
	mach_msg_body_t			body;
	mach_msg_ool_descriptor_t	ool;
	mach_msg_format_0_trailer_t	trailer;
};

static unsigned int atoh(ap)
char *ap;
{
        register char *p;
        register unsigned int n;
        register int digit,lcase;

        p = ap;
        n = 0;
        while(*p == ' ' || *p == '	')
                p++;
        while ((digit = (*p >= '0' && *p <= '9')) ||
                (lcase = (*p >= 'a' && *p <= 'f')) ||
                (*p >= 'A' && *p <= 'F')) {
                n *= 16;
                if (digit)      n += *p++ - '0';
                else if (lcase) n += 10 + (*p++ - 'a');
                else            n += 10 + (*p++ - 'A');
        }
        return(n);
}

usage()
{
	fprintf(stderr, "usage: machipc -s [-ool] [-n service-name]\n");
	fprintf(stderr, "\t-n: specify name registered with netname service\n");
	fprintf(stderr, "\t-ool: be prepared for OOL data\n");
	fprintf(stderr, "usage: machipc -c [-t target] [-i iterations] [-ool size] [-n service-name]\n");
	fprintf(stderr, "\t-n: specify name registered with netname service\n");
	fprintf(stderr, "\t-i: number of RPCs measured\n");
	fprintf(stderr, "\t-ool: size (bytes, in hex) of OOL data in request\n");
	fprintf(stderr, "usage: machipc -null [-i iterations]\n");
	fprintf(stderr, "\t-i: number of null traps measured\n");
	exit(1);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	enum { Nothing, Client, Server, NullTrap } mode = Nothing;
	boolean_t AmClient = FALSE, AmServer = FALSE;
	boolean_t ServerOOL = FALSE;
	int iterations = 1000;
	vm_size_t oolsize = -1;
	char *servicename = "MachIPCPerformance";

	mach_port_t port;
	kern_return_t kr;
	int i;
	char *target_node = NULL;

	for (i = 1; i < argc; i++)
		if ((mode == Nothing) && streql(argv[i], "-c"))
			mode = Client;
		else if ((mode == Nothing) && streql(argv[i], "-s"))
			mode = Server;
		else if ((mode == Nothing) && streql(argv[i], "-null"))
			mode = NullTrap;
		else if (((mode == Client) || (mode == NullTrap)) &&
			 streql(argv[i], "-i") && (i < argc-1))
			iterations = atoi(argv[++i]);
		else if ((mode == Client) &&
			 streql(argv[i], "-ool") && (i < argc-1))
			oolsize = atoh(argv[++i]);
		else if ((mode == Client) &&
			 streql(argv[i], "-t") && (i < argc-1))
			target_node = argv[++i];
		else if ((mode == Server) && streql(argv[i], "-ool"))
			ServerOOL = TRUE;
		else if (((mode == Client) || (mode == Server)) &&
			 streql(argv[i], "-n") && (i < argc-1))
			servicename = argv[++i];
		else
			usage();

	switch (mode) {
	    case Nothing:
		usage();

	    case Client:
		kr = netname_look_up(name_server_port, 
				     (target_node) ? target_node: "*",
				     servicename, 
				     &port);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "machipc: netname_look_up: %s\n",
			     mach_error_string(kr));
			exit(1);
		}

		if (oolsize == -1)
			client_simple(port, iterations);
		else
			client_ool(port, iterations, oolsize);
		break;

	    case Server:
		kr = mach_port_allocate(mach_task_self(),
					MACH_PORT_RIGHT_RECEIVE, &port);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "machipc: mach_port_allocate: %s\n",
			     mach_error_string(kr));
			exit(1);
		}

		kr = netname_check_in(name_server_port, servicename,
				      mach_task_self(), port);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "machipc: netname_check_in: %s\n",
			     mach_error_string(kr));
			exit(1);
		}

		if (ServerOOL)
			server_ool(port);
		else
			server_simple(port);
		break;

	    case NullTrap:
		null_trap(iterations);
		break;
	}

	exit(0);
}

double
periter(before, after, iterations)
	struct timeval *before, *after;
	int iterations;
{
	/* usecs/RPC */

	return (((after->tv_sec - before->tv_sec) * 1000000 +
		 (after->tv_usec - before->tv_usec)) /
		(double) iterations);
}

double
throughput(before, after, iterations, size)
	struct timeval *before, *after;
	int iterations;
	vm_size_t size;
{
	/* bytes/sec */

	return (((double)iterations * size) /
		((after->tv_sec - before->tv_sec) +
		 (after->tv_usec - before->tv_usec) / 1000000.0));
}

static void
client_simple(port, iterations)
	mach_port_t port;
	int iterations;
{
	struct {
		mach_msg_header_t	h;
		mach_msg_format_0_trailer_t	t;
	} msg;

	struct rusage rbefore, rafter;
	struct timeval tbefore, tafter;
	mach_port_t reply;
	int i;
	kern_return_t kr;

	reply = mach_reply_port();

	printf("%d RPCs, no OOL data:\n", iterations);

	msg.h.msgh_id = 0;
	msg.h.msgh_local_port = reply;
	msg.h.msgh_remote_port = port;
	msg.h.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
				         MACH_MSG_TYPE_MAKE_SEND_ONCE);
	kr = mach_msg_overwrite(&msg.h, MACH_SEND_MSG|MACH_RCV_MSG,
			        sizeof (mach_msg_header_t), 
				sizeof msg, 
				reply, 
				MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
				0, 
				(mach_msg_size_t) 0);

	if (kr != MACH_MSG_SUCCESS) {
		fprintf(stderr, "machipc: client_simple(): mach_msg: %s\n",
		     mach_error_string(kr));
		exit(1);
	}

	(void) gettimeofday(&tbefore, (struct timezone *) NULL);
	(void) getrusage(RUSAGE_SELF, &rbefore);

	for (i = 0; i < iterations; i++) {
		msg.h.msgh_remote_port = port;
		msg.h.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
                                               MACH_MSG_TYPE_MAKE_SEND_ONCE);

		(void)mach_msg_overwrite_trap(&msg.h,
					      MACH_SEND_MSG|MACH_RCV_MSG,
					      sizeof (mach_msg_header_t), 
					      sizeof msg, 
					      reply,
					      MACH_MSG_TIMEOUT_NONE,
					      MACH_PORT_NULL,
					      0,
					      (mach_msg_size_t) 0);
	}

	(void) getrusage(RUSAGE_SELF, &rafter);
	(void) gettimeofday(&tafter, (struct timezone *) NULL);

	printf("Elapsed usecs/RPC: %7.3f\n",
	       periter(&tbefore, &tafter, iterations));
	printf("User    usecs/RPC: %7.3f\n",
	       periter(&rbefore.ru_utime, &rafter.ru_utime, iterations));
	printf("System  usecs/RPC: %7.3f\n",
	       periter(&rbefore.ru_stime, &rafter.ru_stime, iterations));
}

static void
server_simple(port)
	mach_port_t port;
{
	struct {
		mach_msg_header_t	h;
		mach_msg_format_0_trailer_t	t;
	} msg;
	mach_port_t pset;
	kern_return_t kr;

	kr = mach_port_allocate(mach_task_self(),
				MACH_PORT_RIGHT_PORT_SET, &pset);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "machipc: mach_port_allocate: %s\n",
		     mach_error_string(kr));
		exit(1);
	}

	kr = mach_port_move_member(mach_task_self(), port, pset);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "machipc: mach_port_move_member: %s\n",
		     mach_error_string(kr));
		exit(1);
	}

	kr = mach_msg_overwrite(&msg.h, MACH_RCV_MSG, 0, sizeof msg, pset,
			        MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
			        (mach_msg_header_t*) NULL, (mach_msg_size_t) 0);

	if (kr != MACH_MSG_SUCCESS) {
		fprintf(stderr, "machipc: server_simple(): mach_msg: %s\n",
		     mach_error_string(kr));
		exit(1);
	}

	for (;;) {
		msg.h.msgh_bits =
			MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
		msg.h.msgh_local_port = MACH_PORT_NULL;

		(void) mach_msg_overwrite_trap(&msg.h,
					       MACH_SEND_MSG|MACH_RCV_MSG,
					       sizeof (msg.h), sizeof msg, pset,
					       MACH_MSG_TIMEOUT_NONE,
					       MACH_PORT_NULL,
					       (mach_msg_header_t*) NULL,
					       (mach_msg_size_t) 0);
	}
}

static void
client_ool(port, iterations, oolsize)
	mach_port_t port;
	int iterations;
	vm_size_t oolsize;
{
	struct msg_ool msg;
	struct rusage rbefore, rafter;
	struct timeval tbefore, tafter;
	mach_port_t reply;
	vm_offset_t data;
	double tput;
	kern_return_t kr;
	int i;

	reply = mach_reply_port();

	if (oolsize > 0) {
		kr = vm_allocate(mach_task_self(), &data,
				 oolsize, TRUE);
		if (kr != KERN_SUCCESS) {
		    fprintf(stderr, "client: vm_allocate: %s\n",
			 mach_error_string(kr));
		    exit(1);
		}

		for (i = 0; i < oolsize/sizeof(int); i++)
		    ((int *) data)[i] = i;
	} else
		data = 0;

	printf("%d RPCs, 0x%x bytes OOL data:\n",
	       iterations, oolsize);

	(void) gettimeofday(&tbefore, (struct timezone *) NULL);
	(void) getrusage(RUSAGE_SELF, &rbefore);

	for (i = 0; i < iterations; i++) {
		msg.h.msgh_id = 0;
		msg.h.msgh_remote_port = port;
		msg.h.msgh_local_port = reply;
		msg.h.msgh_bits =
			(MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
					MACH_MSG_TYPE_MAKE_SEND_ONCE) |
			 MACH_MSGH_BITS_COMPLEX);
		msg.h.msgh_size = sizeof msg - sizeof(msg.trailer);

		/* While typed IPC can set the remaining values just once,
		 * because the reply message never overwrites the data, untyped
		 * IPC must reset the values each time because the trailer in
		 * the reply message overwrites them. */
		msg.body.msgh_descriptor_count = 1;
		msg.ool.address = (void *) data;
		msg.ool.type = MACH_MSG_OOL_DESCRIPTOR;
		msg.ool.size = oolsize;
		msg.ool.deallocate = FALSE;
		msg.ool.copy = MACH_MSG_VIRTUAL_COPY;

		kr = mach_msg_overwrite(&msg.h, MACH_SEND_MSG|MACH_RCV_MSG,
				        sizeof(msg) - sizeof(msg.trailer),
					sizeof(msg.h) + sizeof(msg.trailer),
					reply,
				        MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
				        (mach_msg_header_t *) NULL,
				        (mach_msg_size_t) 0);

		if (kr != MACH_MSG_SUCCESS) {
			fprintf(stderr, "machipc: client_ool(): mach_msg: %s\n",
			     mach_error_string(kr));
			exit(1);
		}
	}

	(void) getrusage(RUSAGE_SELF, &rafter);
	(void) gettimeofday(&tafter, (struct timezone *) NULL);

	printf("Elapsed usecs/RPC: %7.3f\n",
	       periter(&tbefore, &tafter, iterations));
	printf("User    usecs/RPC: %7.3f\n",
	       periter(&rbefore.ru_utime, &rafter.ru_utime, iterations));
	printf("System  usecs/RPC: %7.3f\n",
	       periter(&rbefore.ru_stime, &rafter.ru_stime, iterations));

	tput = throughput(&tbefore, &tafter, iterations, oolsize);

	printf("\n");
	printf("Throughput bytes/sec: %d (%dK)\n",
	       (int) tput, (int) (tput / 1024.0));
}

static void
server_ool(port)
	mach_port_t port;
{
	struct msg_ool msg;
	mach_port_t pset;
	kern_return_t kr;

	kr = mach_port_allocate(mach_task_self(),
				MACH_PORT_RIGHT_PORT_SET, &pset);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "machipc: mach_port_allocate: %s\n",
		     mach_error_string(kr));
		exit(1);
	}

	kr = mach_port_move_member(mach_task_self(), port, pset);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "machipc: mach_port_move_member: %s\n",
		     mach_error_string(kr));
		exit(1);
	}

	kr = mach_msg_overwrite(&msg.h, MACH_RCV_MSG, 0, sizeof msg, pset,
			        MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
			        (mach_msg_header_t*) NULL, (mach_msg_size_t) 0);

	if (kr != MACH_MSG_SUCCESS) {
		fprintf(stderr, "machipc: mach_msg: %s\n",
		     mach_error_string(kr));
		exit(1);
	}

	for (;;) {
		if (msg.h.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
			vm_offset_t	data;
			vm_size_t	oolsize;

			data = (vm_offset_t) msg.ool.address;
			oolsize = msg.ool.size;

			if (oolsize > 0) {
				kr = vm_deallocate(mach_task_self(),
						   data, oolsize);
				if (kr != KERN_SUCCESS) {
					fprintf(stderr, "machipc: server_ool(): vm_deallocate: %s\n",
					     mach_error_string(kr));
					exit(1);
				}
			}
		}

		msg.h.msgh_bits =
			MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
		msg.h.msgh_local_port = MACH_PORT_NULL;

		kr = mach_msg_overwrite(&msg.h, MACH_SEND_MSG|MACH_RCV_MSG,
				        sizeof msg.h, sizeof msg, pset,
				        MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
				        (mach_msg_header_t *) NULL,
					(mach_msg_size_t) 0);

		if (kr != MACH_MSG_SUCCESS) {
			fprintf(stderr, "machipc: server_ool(): mach_msg: %s\n",
			     mach_error_string(kr));
			exit(1);
		}
	}
}

static void
null_trap(iterations)
	int iterations;
{
	struct rusage rbefore, rafter;
	struct timeval tbefore, tafter;
	int i;

	printf("%d iterations:\n", iterations);

	(void) getrusage(RUSAGE_SELF, &rbefore);
	(void) gettimeofday(&tbefore, (struct timezone *) NULL);

	for (i = 0; i < iterations; i++)
		(void) mach_msg_overwrite_trap((mach_msg_header_t *) 0,
					       MACH_MSG_OPTION_NONE,
					       0, 0, MACH_PORT_NULL,
					       MACH_MSG_TIMEOUT_NONE,
					       MACH_PORT_NULL,
					       (mach_msg_header_t *) NULL,
					       (mach_msg_size_t) 0);

	(void) getrusage(RUSAGE_SELF, &rafter);
	(void) gettimeofday(&tafter, (struct timezone *) NULL);

	printf("Elapsed usecs/trap: %7.3f\n",
	       periter(&tbefore, &tafter, iterations));
	printf("User    usecs/trap: %7.3f\n",
	       periter(&rbefore.ru_utime, &rafter.ru_utime, iterations));
	printf("System  usecs/trap: %7.3f\n",
	       periter(&rbefore.ru_stime, &rafter.ru_stime, iterations));
}
