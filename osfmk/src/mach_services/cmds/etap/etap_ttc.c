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
 *  File: 	etap_ttc.c
 *
 *  Author: 	Joe CaraDonna
 *
 *  Function:	Sample ETAP thread trace control program.
 *	      	User-level probes are also demonstrated here.
 */
 
#include <stdio.h>
#include <mach.h>
#include <mach_error.h>
#include <rthreads.h>
#include <mach/etap.h>  

#define	RECORD_THREAD_ID	0xdeed

unsigned long	max_children = 25;
void 		child (unsigned int);


main (int argc, char *argv[]) 
{
	int	id = 0;
	int 	count = 0;
	
	if (argc == 2)
 		max_children = atol(argv[1]);
 
	/*
	 *  Spawn specified number of child threads
	 */
	while (id < max_children) {
		rthread_spawn((rthread_fn_t) child, (any_t) id);
		id++;
	}

	/*
	 *  Wait for children to complete
	 */
	rthread_wait();

	/*
	 *  Terminate task execution
	 */
	exit(0);
}

void
child (unsigned int id) 
{ 
	mach_port_t	thread_port;
	kern_return_t	ret;
	etap_data_t	probe_data;
	char		buffer[10] = {"h","e","l","l","o","\0"};

	/*
	 *  Get the thread's kernel port
	 */
	thread_port = rthread_kernel_port(rthread_self());

	/*
	 *  Enable ETAP thread trace status
	 */
	ret = etap_trace_thread(thread_port, TRUE);

	if (ret != KERN_SUCCESS)
		printf ("etap_trace_thread: failed\n");

	/*
	 *  The thread would typically do some work here.
	 *
	 *  For kicks, lets insert a user-level probe here to record
	 *  the user-level thread ID.
	 *
	 *  NOTE:
	 *
	 *  Only the first array entry in probe_data is being
	 *  used here since we only care about a single integer
	 *  value, the thread id.
	 */

	probe_data[0] = id;
	etap_probe(RECORD_THREAD_ID, ETAP_DATA_SIZE, &probe_data); 

	/*
	 *  Terminate thread execution
	 */
	rthread_exit(0);
}




