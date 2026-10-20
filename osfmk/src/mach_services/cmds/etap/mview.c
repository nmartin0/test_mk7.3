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

#include <stdio.h>
#include <mach.h>
#include <math.h>

#include <mach/etap.h>
#include <mach/etap_events.h>

main()
{
	struct monitor_buffer	*mb = {0};
	mbuff_entry_t		scan;
	int 			ret;
	int 			dum;
	int			entries;
	int			free;
	int			x;
	double	  		time;

	ret = etap_info(&dum, &dum, &dum, &dum, &entries, &dum);

	if (entries == 0) {
	   printf("etap: kernel configuration excludes monitor buffer\n");
	   exit(1);
	}
	ret = etap_map(ETAP_BUFFER_MONITORED, 0, &mb);

	if (ret != KERN_SUCCESS) {
	   printf("etap: error mapping monitor buffer\n");
	   exit(1);
	}

	scan = mb->entry;

	printf ("free          = %d\n",free = mb->free);
	printf ("timestamp     = %d\n", mb->timestamp);

	for (x=0; x < free; x++) {
#define tvalspec_to_nsec(tv) \
	((double) (tv).tv_sec * NSEC_PER_SEC + (tv).tv_nsec)

		time = tvalspec_to_nsec(scan->time);

		printf ("%4d> %4x %12.0f th=%x\n\t   Data=[%8x %8x %8x %8x]\n",
			x,
			scan->event,
			time,
			scan->instance,
			scan->data[0],
			scan->data[1],
			scan->data[2],
			scan->data[3]);

		scan++;
	}

	exit(0);
}
	
