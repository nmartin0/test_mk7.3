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
 *  File     :  convert.c
 *
 *  Function :  contains convertion routines for ETAP utilities
 *
 *              - text_to_short()
 *              - file_name_conv()
 */

#include <stdio.h>
#include <mach.h>
#include <sys/types.h>

#include "config.h"
#include <mach/etap_events.h>
#include <mach/etap.h>
#include "convert.h"

/*
 *  ROUTINE:    text_to_short
 *
 *  FUNCTION:   Converts an array of lock text names to 
 *              their true u_short representation.
 *
 *              This allows the user to associate text names with
 *              system locks, while the kernel can still use a simple  
 *              number representation.
 *
 */

int
text_to_short (
	int      search,
	int      *index,
	char     object_name_list[MAX_OBJECTS][EVENT_NAME_LENGTH],
	u_short  object_list[])
{
	char   temp_name_list[MAX_OBJECTS][EVENT_NAME_LENGTH];

	event_table_t  	event_table;
	event_table_t  	event_scan;
	subs_table_t  	subs_table;
	subs_table_t	subs_scan;

	boolean_t     found;
	int           i,te,z, ret;
	int           event_entries, subs_entries, dum;
 
	/*
	 *  Get table configuration
	 */

	ret = etap_info (&event_entries, 
			 &subs_entries,
			 &dum, &dum, &dum, &dum);

	if (ret) {
		fprintf(stderr,"cannot obtain ETAP configuration\n");
		exit(1);
	}

	/*
	 *  Map event table in VAS
	 */

	ret = etap_map (ETAP_TABLE_EVENT, NULL, &event_table);

	if (ret) {
		fprintf(stderr,"cannot map event table into VAS\n");
		exit(1);
	}
   
	/* 
	 *  Event search 
	 */

	if (search == LOCK_NAMES) {

		for (i=0; i < *index; i++) {
			event_scan = event_table;
			found      = FALSE;
			te         = event_entries;

			while (!found && te--) {
				found = !strcmp(event_scan->name,
						&object_name_list[i][0]);
				if (found)
					object_list[i] = event_scan->event;
				else
					event_scan++;
			}

			if (!found) {
				fprintf(stderr,"invalid event: %s\n",
					&object_name_list[i][0]);
				exit(1);
			}
		}
		return(0);
	}

	/*
	 *  Subsystem search
	 */

	if (search == SUBSYS_NAMES) {

		/*
		 *  Map subsystem table in VAS
		 */

		ret = etap_map (ETAP_TABLE_SUBSYSTEM, NULL, &subs_table);

		if (ret) {
			fprintf(stderr,"cannot map subsystem table into VAS\n");
			exit(1);
		}

		/* ==================================================== */
		/* all subsystem names will be converted to their       */
		/* associated event names.  This is done to provide     */
		/* the user with the event text name in the analysis    */
		/* output. Thus, relevent data is provided.             */   
		/* The event names will be returned in the              */
		/* object_name_list array. To avoid losing the selected */
		/* subsystem names, a temp_name_list is used to store   */
		/* a copy of the original name list.                    */
		/* ==================================================== */
  
		i=0;
		while (i < *index) {
			strcpy(&temp_name_list[i][0],&object_name_list[i][0]);
			i++;
		}

		/*
		 *  Parse subsystem name object list
		 */

		z = 0;
		for (i=0; i < *index; i++) {
			subs_scan  = subs_table;
			event_scan = event_table;
			found      = FALSE;
			te         = subs_entries;

			while (!found && te--) {
				found = !strcmp(subs_scan->name,
						&temp_name_list[i][0]);

				/* ======================================== */
				/* get all event names within the subsystem */
				/* and store them in the object_name_list.  */
				/* Also get the event IDs and store them in */
				/* the object list.                         */
				/* ======================================== */

				if (found) {
				   te = event_entries;
				      while (te--) {
					 if ((event_scan->event& 0xff00)
					    == subs_scan->subs) {
					     object_list[z] = event_scan->event;
					     strcpy(&object_name_list[z][0],
						    event_scan->name);
					     z++;
				         }
					 event_scan++;
				      }
			        }
				else
				   subs_scan++;
			}

			if (!found) {
				fprintf(stderr,"invalid subsystem name: %s\n",
					&temp_name_list[i][0]);
				exit(1);
			}
		}
		*index = z;
		return (0);
	}

	/*
	 * All lock type names are found and loaded
	 */

	if (search == ALL_NAMES) {
	     z = 0;
	     for (te=0; te < event_entries; te++) {
		    if (event_table->event != ETAP_NO_TRACE) {
			    object_list[z] = event_table->event;
			    strcpy(&object_name_list[z][0],event_table->name);
			    z++;
		    }
		    event_table++;
	     }
	     *index = z;
	     return (0);
        }

	return (1);
}


/*
 *  ROUTINE:    file_name_conv
 *
 *  FUNCTION:   Inserts a cpu number in the provided filename.
 *              This is used by monitor buffer utilities.
 */

void
file_name_conv (
	char *file,
	char *new,
	int  cpu)

{
	boolean_t done = FALSE;
	char insert[6];
	int  ins_size;
	int  x = 0;

	sprintf(insert,"_%d.",cpu);
	ins_size = strlen(insert);

	while (*file != '\0') {
		if (*file != '.') {
			*new = *file;
			file++;
			new++;
		}
		else if (!done) {
			file++;
			while (x < ins_size) {
				*new = insert[x];
				new++;
				x++;
			}
			done = TRUE;
		}
	}
	*new = '\0';

	if (!done) {
		sprintf(insert,"_%d",cpu);
		strcat(new,insert);
	}
}
