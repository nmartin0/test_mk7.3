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
#include <mach_error.h>
#include <servers/netname.h>
#include <servers/machid.h>

#define streql(a, b)	(strcmp((a), (b)) == 0)

static mach_port_t server;
static mach_port_t auth;

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
		if (digit)	n += *p++ - '0';
		else if (lcase)	n += 10 + (*p++ - 'a');
		else		n += 10 + (*p++ - 'A');
	}
	return(n);
}

static void
usage()
{
	fprintf(stderr, "usage: vmread [-host machine] taskid addr words\n");
	exit(1);
}

main(argc, argv)
    int argc;
    char *argv[];
{
    char *hostname = "";
    vm_offset_t addr, first, last, memory;
    vm_size_t size, memsize;
    mtask_t task;
    int i;
    kern_return_t kr;

    for (i = 1; i < argc; i++)
	if (streql(argv[i], "-host") && (i < argc-1))
	    hostname = argv[++i];
	else if (streql(argv[i], "--")) {
	    i++;
	    break;
	} else if (argv[i][0] == '-')
	    usage();
	else
	    break;

    argc -= i;
    argv += i;

    if (argc != 3)
	usage();

    kr = netname_look_up(name_server_port, hostname, "MachID", &server);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "vmread: netname_lookup_up(MachID): %s\n",
	     mach_error_string(kr));
	exit(1);
    }

    auth = mach_host_priv_self();
    if (auth == MACH_PORT_NULL)
	auth = mach_task_self();

    task = atoi(argv[0]);
    addr = atoh(argv[1]);
    size = atoh(argv[2]);

    first = trunc_page(addr);
    last = round_page(addr + size);

    kr = machid_vm_read(server, auth, task, first, last - first,
			&memory, &memsize);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "vmread: machid_vm_read: %s\n", mach_error_string(kr));
	exit(1);
    }

    memory += addr - first;

    for (i = 0; i < size/4; i++) {
	if (i % 4 == 0)
	    printf("%08x: ", addr + 4 * i);
	
	printf("%08x", ((unsigned int *) memory)[i]);
	
	if (i % 4 == 3)
	    printf("\n");
	else
	    printf(" ");
    }
    if (i % 4 != 0)
	printf("\n");

    exit(0);
}
