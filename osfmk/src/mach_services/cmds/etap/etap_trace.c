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
 *  File : etap_trace
 *
 *  Author : Joe CaraDonna
 *
 *  Function : user interface to the event trace analysis package
 *
 */

#include <stdio.h>
#include <mach.h>
#include <sys/types.h>

#include <mach/etap.h>
#include <mach/etap_events.h>
#include "config.h"
#include "convert.h"

/* protos */

void usage (int help);

main(int argc, char *argv[])
{
        char         object_name_list  [MAX_OBJECTS][EVENT_NAME_LENGTH];
        u_short      object_list       [MAX_OBJECTS];
        boolean_t    event_objects  =  FALSE;
        boolean_t    subs_objects   =  FALSE;
        boolean_t    all_events     =  FALSE;
        boolean_t    i              =  FALSE;
	boolean_t    enable	    =  TRUE;
        u_short      mode           =  ETAP_MONITORED;
	u_short	     type	    =  0;	
        int          index          =  0;
        int          x;
        int          rc;
        char         *cp;
    
        /* =================== */
        /* Parse argument list */
        /* =================== */
    
        for (x=1; x < argc; x++)   { 
                cp = argv[x];
                if (*cp != '-')
                        usage(0);

                switch (*(++cp)) {
                case 'a' :  if (subs_objects || event_objects)
			 	usage (0);
                            all_events = TRUE;
                            break;
                case 'd' :  enable = FALSE; 			break;
		case 'c' :  mode   = ETAP_CUMULATIVE; 		break;
                case 'm' :  mode   = ETAP_MONITORED;  		break;
                case 'R' :  mode   = ETAP_RESET;      		break;
                case 'h' :  type   = type | ETAP_DURATION;	break;
                case 'w' :  type   = type | ETAP_CONTENTION;	break;
                case 'i' :  
                            i = TRUE;
                            index = atoi(argv[++x]);
                            break;
                case 'e' :
                            if (subs_objects || (++x == argc) || all_events)
				    usage (0);
                            event_objects = TRUE;
                            strcpy (&object_name_list[index++][0],argv[x]);
                            break;
                case 's' :
                            if (event_objects || (++x == argc) || all_events)
                                usage (0);
                            subs_objects = TRUE;
                            strcpy (&object_name_list[index++][0],argv[x]);
                            break;
                case '?' : usage(1);
                default  : usage(0);
                }
        }

        /* ========================= */
        /*  Check argument validity  */
        /* ========================= */

	if (mode != ETAP_RESET)
		if (i| !mode | (!subs_objects && !event_objects && !all_events))
                        usage(0);

	if (!type)
		type = ETAP_DURATION;

        /* =============== */
        /* Collect objects */
        /* =============== */

        if (mode != ETAP_RESET) {
		if (event_objects)
			text_to_short(LOCK_NAMES,
				      &index,
				      object_name_list,
				      object_list);

		else if (subs_objects)
			text_to_short(SUBSYS_NAMES,
				      &index,
				      object_name_list,
				      object_list);

		else if (all_events)
			text_to_short(ALL_NAMES,
				      &index,
				      object_name_list,
				      object_list);

		else {
		      fprintf(stderr,"unknown object list, check parameters\n");
		      exit(1);
	      }
	}

        /* ============= */
        /* Make the call */
        /* ============= */

        rc = etap_trace_event (mode, type, enable, index, object_list);
  
        if (rc != KERN_SUCCESS)
                printf ("request unsuccessful, check parameters\n");

	exit(0);
}

void
usage(int help)
{
        printf ("usage: etap_trace [mode] [type] [-d] objects\n");
 
        if (help) {
		printf ("\n  mode\n");
		printf ("         -c cumulative event trace\n");
		printf ("         -m monitored event trace *\n");
		printf ("	  -R reset ETAP\n");
		printf ("\n  type\n");
		printf ("         -h trace hold time (duration) *\n");
		printf ("         -w trace wait time (contention)\n");
                printf ("         -i# interval width (only when -R is selected)\n");		printf ("\n  -d   disable trace\n");

                printf ("\n  objects\n");
		printf ("         -e a selected event will follow\n");
                printf ("         -s a selected subsystem will follow\n");
                printf ("         -a operation pertains to all events\n\n");
		printf ("Note: defaults are marked with an asterisk (*)\n");
        }

	exit(1);
}
