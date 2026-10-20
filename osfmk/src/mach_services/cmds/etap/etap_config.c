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
 * File: etap_config.c
 *
 * Function: etap_config displays the current systems ETAP configuration
 */
 
#include <stdio.h>
#include <mach.h>
#include <mach_error.h>
#include <mach/etap.h>
#include <mach/etap_events.h>
#include <mach/mach_types.h>

main(int argc, char *argv[]) 
{
	int	et_entries = 0;
	int	st_entries = 0;
	int 	cb_width   = 0;
	int	mb_size    = 0;
	int     mb_entries = 0;
	int     mb_cpus    = 0;
 
	kern_return_t	r;
 
	r = etap_info(&et_entries,
		      &st_entries,
		      &cb_width,
		      &mb_size,
		      &mb_entries,
		      &mb_cpus);

	if (r) {
		printf("etap_info: unsuccessful %d\n",r);
		exit(1);
	}

	if (!et_entries || !st_entries) {
		printf ("ETAP is not configured in the server.\n");
		exit(1);
	}

	printf ("\nEvent Trace Analysis Package Configuration\n");
	printf ("==========================================\n");

	printf("\nTable Info\n");

	printf("\tEvent table entries     : %lu\n",et_entries);
	printf("\tSubsystem table entries : %lu\n\n",st_entries);

	if (cb_width) {
		printf("Cumulative Info\n");

		printf("\tbuffer size     : %lu\n",
		       sizeof(struct cumulative_buffer));
		printf("\tentry  size     : %lu\n",
		       sizeof(struct cbuff_entry));
		printf("\tmax entries     : %lu\n",
		       sizeof(struct cumulative_buffer) / 
		       sizeof(struct cbuff_entry));
		printf("\tinterval width  : %lu\n",cb_width);
	} else
		printf("Cumulative\n\tNOT CONFIGURED\n");
    
	if (mb_cpus) {
		printf("\nMonitored Info\n");
		printf("\tbuffer size : %lu\n",mb_size);
		printf("\tentry size  : %lu\n",sizeof(struct mbuff_entry));
		printf("\tmax entries : %lu\n",mb_entries);
		printf("\tbuffers     : %lu\n",mb_cpus);
	} else
		printf ("\nMonitored\n\tNOT CONFIGURED\n");

	exit(0);
}
