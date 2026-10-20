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

#include <stdio.h>
#include <string.h>
#include <mach.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <mach/norma_special_ports.h>

#define streql(a, b)	(strcmp((a), (b)) == 0)

void netname_init();

char *program = NULL;
boolean_t Debug = FALSE;

int my_node;
int node_count;
mach_port_t	sname_servers[256];	/* sname service port for distant nodes */
mach_port_t	privileged_host_port;

mach_port_t notify;
mach_port_t service;

static boolean_t snames_demux();

static void
usage()
{
    fprintf(stderr, "usage: %s [-d] [-n nodes] command [args ...]\n", program);
    exit(1);
}

void
main(argc, argv)
    int argc;
    char *argv[];
{
    mach_port_t pset;
    int i;
    kern_return_t kr;

    program = rindex(argv[0], '/');
    if (program == NULL)
	program = argv[0];
    else
	program++;

    for (i = 1; i < argc; i++)
	if (streql(argv[i], "-d")) {
	    Debug = TRUE;
	} else if (streql(argv[i], "-n")) {
            if (i < argc - 1) {
		i++;
		node_count = atoi(argv[i]);
	    } else usage();
	} else if (streql(argv[i], "--")) {
	    i++;
	    break;
	} else if (argv[i][0] == '-')
	    usage();
	else
	    break;

    argv += i;
    argc -= i;

    if (argc != 0)
	usage();

    /* Allocate our notification port. */

    kr = mach_port_allocate(mach_task_self(),
			    MACH_PORT_RIGHT_RECEIVE, &notify);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: mach_port_allocate: %s\n",
		program, mach_error_string(kr));
	exit(1);
    }

    kr = service_checkin(service_port, name_server_port, &service);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: service_checkin: %s\n",
		program, mach_error_string(kr));
	exit(1);
    }

    /* 
     * If node_count > 1, tell set the node name server port via
     *  norma_set_nameserver_port(). Need to be privileged user
     */

    if (node_count > 1)
    	if (!norma_register(service)) {
	    fprintf(stderr, "could not register as norma name server\n");
	    node_count = 1;
	}

    /*
     *	Prepare the name service.
     *	The three do_netname_check_in calls will consume user-refs
     *	for their port args, so we have to generate the refs.
     *	Note that mach_task_self() is just a macro;
     *	it doesn't return a ref.
     */

    netname_init();

    kr = mach_port_mod_refs(mach_task_self(), mach_task_self(),
			    MACH_PORT_RIGHT_SEND, 3);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: mach_port_mod_refs: %s\n",
		program, mach_error_string(kr));
	exit(1);
    }

    kr = mach_port_mod_refs(mach_task_self(), service,
			    MACH_PORT_RIGHT_SEND, 3);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: mach_port_mod_refs: %s\n",
		program, mach_error_string(kr));
	exit(1);
    }

    kr = do_netname_check_in(service, "NameServer",
			     mach_task_self(), service);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: netname_check_in: %s\n",
		program, mach_error_string(kr));
	exit(1);
    }

    kr = do_netname_check_in(service, "NMMonitor",
			     mach_task_self(), service);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: netname_check_in: %s\n",
		program, mach_error_string(kr));
	exit(1);
    }

    kr = do_netname_check_in(service, "NMControl",
			     mach_task_self(), service);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: netname_check_in: %s\n",
		program, mach_error_string(kr));
	exit(1);
    }

    /* Prepare our port set. */

    kr = mach_port_allocate(mach_task_self(),
			    MACH_PORT_RIGHT_PORT_SET, &pset);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: mach_port_allocate: %s\n",
		program, mach_error_string(kr));
	exit(1);
    }

    kr = mach_port_move_member(mach_task_self(), service, pset);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: mach_port_move_member: %s\n",
		program, mach_error_string(kr));
	exit(1);
    }

    kr = mach_port_move_member(mach_task_self(), notify, pset);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: mach_port_move_member: %s\n",
		program, mach_error_string(kr));
	exit(1);
    }

    /* Enter service loop. */

    kr = mach_msg_server(snames_demux, 256, pset, MACH_MSG_OPTION_NONE);
    fprintf(stderr, "%s: mach_msg_server: %s\n",
	    program, mach_error_string(kr));
    exit(1);
}

static boolean_t
snames_demux(request, reply)
    mach_msg_header_t *request, *reply;
{
    if (request->msgh_local_port == service)
	return netname_server(request, reply);
    else if (request->msgh_local_port == notify)
	return notify_server(request, reply);
    else {
	fprintf(stderr, "%s: snames_demux: port = %x\n",
		program, request->msgh_local_port);
	exit(1);
    }
}

norma_register(service)
mach_port_t service;
{
    kern_return_t kr;

    if ((privileged_host_port = mach_host_priv_self()) == MACH_PORT_NULL) {
        fprintf(stderr, "Cant get host privileged port\n");
	return(0);
    } else if (Debug)
	fprintf(stderr, "privileged host port %x\n", privileged_host_port);

    kr = norma_node_self(privileged_host_port, &my_node);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "%s: norma_node_self: %s\n", program, mach_error_string(kr));
	return(0);
    } else if (Debug)
	fprintf(stderr, "my_node %x\n", my_node);

    kr = norma_set_nameserver_port(privileged_host_port, service);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "%s: norma_set_nameserver_port: %s\n",
		 program, mach_error_string(kr));
	return(0);
    }
    return(1);
}
