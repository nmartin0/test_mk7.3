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

#include <mach/etap.h>
#include <mach/etap_events.h>

main()
{
	cumulative_buffer_t	cb = {0};
	cbuff_entry_t		scan;
	int 			ret;
	int 			dum;
	int			width;
	int			entries;
	int			x;

	ret = etap_info(&dum, &dum, &width, &dum, &dum, &dum);

	if (width == 0) {
	   printf("etap: kernel configuration excludes cumulative buffer\n");
	   exit(1);
	}

	ret = etap_map(ETAP_BUFFER_CUMULATIVE, NULL, &cb);

	if (ret != KERN_SUCCESS) {
	   printf("etap: error mapping cumulative buffer\n");
	   exit(1);
	}

	scan = cb->entry;

	printf ("total entries = %d\n", entries = cb->next);
	printf ("number of dynamic locks = %d\n", cb->static_start);

	for (x=0; x < entries; x++) {
		if (scan->hold.triggered || scan->wait.triggered)	
			printf ("%d] %x: i=%x trig=%lu t=%Lf tsq=%Lf tmin=%Lf tmax=%Lf kind=%d\n",
				x,
				scan->event,
				scan->instance,
				scan->hold.triggered,
				scan->hold.time,
				scan->hold.time_sq,
				scan->hold.min_time,
				scan->hold.max_time,
				scan->kind);
		scan++;
	}

	exit(0);
}
	
