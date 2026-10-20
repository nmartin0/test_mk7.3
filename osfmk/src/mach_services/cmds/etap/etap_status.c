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
 *  File : etap_status.c
 *
 *  Author: Joe CaraDonna
 */

#include <stdio.h>
#include <mach.h>
#include <sys/types.h>

#include "config.h"
#include <mach/etap.h>
#include <mach/etap_events.h>
#include <mach/mach_types.h>


/* protos */

int usage (char util_name[]);

main(int argc, char *argv[])
{
	char            *cp;
	int             x;
	int             ret;

	u_short         show_event_table = FALSE;
	u_short         show_subs_table  = FALSE;

	u_long          et_entries;
	u_long          st_entries;
	u_long          dum;    

	event_table_t   etp;
	subs_table_t    stp;

	/*  
	 *  Parse arguments
	 */

	for (x=1; x < argc; x++) {
		cp = argv[x];
		if (*cp != '-')
			usage(argv[0]);

		switch (*(++cp)) {
		case 'e' :  show_event_table = TRUE; break;
		case 's' :  show_subs_table  = TRUE; break;
		default  :  usage(argv[0]);
		}
	}

	/*
	 *  Default: show event table
	 */

	if (!(show_event_table | show_subs_table))
		show_event_table = TRUE;

	/*
	 *  Get table configuration
	 */

	ret = etap_info (&et_entries, 
			 &st_entries,
			 &dum, &dum, &dum, &dum);

	if (ret || !et_entries || !st_entries) {
		fprintf(stderr,"ETAP is not configured in the kernel\n");
		exit(1);
	}
 
	/*
	 *  Show type table if requested
	 */

	ret = etap_map (ETAP_TABLE_EVENT, NULL, &etp);
    
	if (ret != 0) {
		fprintf(stderr,"unable to map event table into VAS [%d]\n",ret);
		exit(1);
	}

	if (show_event_table == TRUE) {

		if (et_entries <= 0) {
			fprintf(stderr,
			"Event table is not configured in the kernel\n");
			exit(1);
		}

		printf("Event Table [%lu]\n",et_entries);
		printf("==================================================================\n");
		printf("  Name                  ID      C_HOLD   C_WAIT   M_HOLD   M_WAIT\n");
		printf("==================================================================\n");

		while (et_entries--) {

			if (etp->dynamic)
				printf("D ");
			else
				printf("  ");

			printf("%-*s %4x     ",
			       EVENT_NAME_LENGTH,
			       etp->name,
			       etp->event);

			if (etp->status & (ETAP_CUMULATIVE & ETAP_DURATION))
				printf("  ON    ");
			else
				printf("  --    ");

			if (etp->status & (ETAP_CUMULATIVE & ETAP_CONTENTION))
				printf("   ON    ");
			else
				printf("   --    ");

			if (etp->status & (ETAP_MONITORED & ETAP_DURATION))
				printf("   ON    ");
			else
				printf("   --    ");

			if (etp->status & (ETAP_MONITORED & ETAP_CONTENTION))
				printf("   ON\n");
			else
				printf("   --\n");

			etp++;
		}
	}

	/*
	 *  Show subsystem table if requested
	 */

	if (show_subs_table == TRUE) {

		if (st_entries <= 0) {
			fprintf (stderr,
			"Subsystem table is not configured in the kernel\n");
			exit(1);
		}

		ret = etap_map(ETAP_TABLE_SUBSYSTEM, NULL, &stp);
    
		if (ret != 0) {
			fprintf(stderr,
			"unable to map subsystem table into VAS [%d]\n",ret);
			exit(1);
		}

		printf("Subsystem Table [%lu]\n", st_entries);
		printf("============================\n");
		printf("Name                  ID   \n");
		printf("============================\n");

		while (st_entries--) {
			printf("%-*s %4x\n",
			       EVENT_NAME_LENGTH,
			       stp->name,
			       stp->subs);
			stp++;
		}
	}
}

int
usage(char util_name[])
{
	fprintf(stderr,"%s [-e] [-s]\n\n",util_name);
	fprintf(stderr,"   -e : show event table\n");
	fprintf(stderr,"   -s : show subsystem table\n");

	exit(0);
}



