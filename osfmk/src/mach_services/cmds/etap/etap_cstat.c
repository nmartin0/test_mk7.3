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
 *  File     :  etap_cstat.c
 *
 *  Author   :  Joe CaraDonna
 *
 *  Function :  Reports cumulative trace data statistics
 *
 */

#include <stdio.h>
#include <mach.h>
#include <math.h>

#include <mach/etap.h>
#include <mach/etap_events.h>

#include "config.h"
#include "skiplist.h"
#include "convert.h"

typedef struct lock_head {
	u_short   event;         	     /* event number                */
	char      name[EVENT_NAME_LENGTH];   /* text name of event          */
	u_long	  num_inst;   		     /* number of instances in list */
	u_short	  kind;       		     /* complex or simple lock      */
	SkipList  list;       	    	     /* list of event instances     */
} *lock_head_t;

struct stats {
	double 	h_mean;
	double	h_sd;
	u_long  h_accesses;
	double  w_mean;
	double  w_sd;
	u_long  w_accesses;
};

struct cbuff_entry	aggregate;

boolean_t	w	= FALSE;
boolean_t	a	= FALSE;
boolean_t	t	= FALSE;
boolean_t	c	= FALSE;
boolean_t	h	= FALSE;

SkipList subsystem [NUM_SUBSYS];

int     	record_size;
u_long  	num_recs;
u_long		time_slice;

FILE 		*fd;

/*
 *  Forwards
 */

void 		collect			(cbuff_entry_t, char*);
int 		compute_results		(lock_head_t, int*);
int 		terse 			(cbuff_entry_t, char*);
void 		interval_info 		(int, cbuff_entry_t);
cbuff_entry_t	get_instance_node	(void);
lock_head_t 	get_head_node		(void);
int 		lock_head_cmp 		(lock_head_t, lock_head_t);
int 		lock_instance_cmp 	(cbuff_entry_t, cbuff_entry_t);
int 		usage			(int);

main(int argc, char *argv[])
{
	char          filename          [FILE_NAME_SIZE];
	char          object_name_list  [MAX_OBJECTS][EVENT_NAME_LENGTH];
	char          *cp;
	u_short       object_list	[MAX_OBJECTS];
	int           index = 0;
	int           x,ret;
	int           chunk_max_recs;
	boolean_t     lock_search   = FALSE;
	boolean_t     subsys_search = FALSE;
	boolean_t     type_found    = FALSE;
	boolean_t     found         = FALSE;
	cbuff_entry_t chunk;
	cbuff_entry_t record;

	strcpy(filename, CBUFF_DATA_FILE);

	/* =================== */
	/* parse argument list */
	/* =================== */

	for (x=1; x < argc; x++) {

		cp = argv[x];
		if (*cp != '-')
			usage(0);

		switch (*(++cp)) {
		case 'w' : w  = TRUE; break;
		case 'a' : a  = TRUE; break;
		case 't' : t  = TRUE; break;
		case 'c' : c  = TRUE; break;
		case 'h' : h  = TRUE; break;
		case 'e' : 
			if (subsys_search || (++x == argc)) usage(0);
			lock_search = TRUE;
			strcpy(&object_name_list[index++][0], argv[x]);
			break;
		case 's' :
			if (lock_search || (++x == argc)) usage (0);
			subsys_search = TRUE;
			strcpy(&object_name_list[index++][0],argv[x]);
			break;
		case 'i' : strcpy(filename,argv[++x]); break;
		case '?' : usage (1);
		default  : usage (0);
		}
	}
  
	/* =============== */
	/* default options */
	/* =============== */

	if (!(a|t))
		a = c = TRUE;

	if (!(h|w))
		h = w = TRUE;


	/* ========== */
	/* open files */
	/* ========== */

	if ((fd = fopen(filename,"r")) == NULL)  {
		fprintf(stderr,"error opening file: %s\n",filename);
		exit(1);
	}

	/* ========================== */
	/* Read header info from file */
	/* ========================== */

	fread(&num_recs,   sizeof(u_long), 1, fd);
	fread(&time_slice, sizeof(u_long), 1, fd);

	/* ========== */
	/* initialize */
	/* ========== */

	/* map text names to their numerical representation */
	if (lock_search)
		text_to_short (LOCK_NAMES,
			       &index,
			       object_name_list,
			       object_list);
	else if (subsys_search)
		text_to_short (SUBSYS_NAMES,
			       &index,
			       object_name_list,
			       object_list);
	else {
		text_to_short (ALL_NAMES,
			       &index,
			       object_name_list,
			       object_list);
		lock_search = TRUE;
	}

	/* initialize subsystem skip lists */
	for (x=0; x < NUM_SUBSYS; x++)
		subsystem[x] = NewSL(lock_head_cmp, NULL, NO_DUPLICATES);

	/* setup */
	record_size = sizeof(struct cbuff_entry);
	bzero ((char *) &aggregate, sizeof(struct cbuff_entry));

	/* initialize file read parameters */
	chunk_max_recs = (BUFSIZ)/record_size;
	chunk = (cbuff_entry_t) malloc(BUFSIZ);
  
	/* ======================= */
	/* Collect data statistics */
	/* ======================= */

	printf("collecting data...\n");

	while (!feof(fd)) {
		ret = fread(chunk, record_size, chunk_max_recs, fd);
		record = chunk;

		/* ========================================== */
		/* scan chunk of records for selected objects */
		/* ========================================== */

		while (ret--) {
			found = FALSE;
			x = 0;

			while (x < index && !found) {
				if (record->event == object_list[x])
					found = TRUE;
				else
					x++;
			}

			if (found) {
				/* ============ */
				/* Convert Time */
				/* ============ */

					                   /* XXX */

				collect(record, &object_name_list[x][0]);
			} 
			if (ret)
				record++;
		}
	}

	fclose(fd);

	/* ============================ */
	/*  Process the collected data  */
	/* ============================ */

	for (x=0; x < NUM_SUBSYS; x++)
		DoForSL(subsystem[x], compute_results, NULL);
}

void
collect (
	cbuff_entry_t	record,
	char 		txt_name[])
{
	int                 ss;
	int                 ret;
	struct lock_head    h_key;
	lock_head_t         head;
	struct cbuff_entry  i_key;
	cbuff_entry_t	    inst;

	/* ============================================= */
	/* If the record doesn't contain any data, don't */
	/* bother collecting it.                         */
	/* ============================================= */

	if (!(record->hold.triggered | record->wait.triggered))
		return;

	/* =================================================== */
	/* examine appropriate subsystem list for event header */
	/* =================================================== */

	ss = (record->event & 0xff00) >> 8;
	h_key.event = record->event;

	if  ((head = SearchSL (subsystem[ss],&h_key)) == NULL) {
		/* lock TYPE isn't in the list yet: add and init */
		head = get_head_node();
		head->event = record->event;
		head->kind = record->kind;
		strcpy (head->name,txt_name);
		if ((ret = InsertSL (subsystem[ss],head)) != TRUE) {
			fprintf(stderr,"Subs header insertion error: %d\n",ret);
			return;
		}
	}

	/* ================================== */
	/* Insert record into the appropriate */
	/* event instance list. Each record   */
	/* is unique, so it is assumed not to */
	/* be in the list yet. 		      */
	/* (determined by instance value)     */
	/* ================================== */

	/* allocate  */
	inst = get_instance_node();
	/* load data */
	bcopy((char *) record, (char *)inst, (unsigned int) record_size);
	/* insert    */
	if ((ret = InsertSL(head->list, inst)) != TRUE) {
		/*
		 *  If this warning occurs, the kernel event table must
		 *  be modified.  The detected dynamic event must be
		 *  assigned a unique dynamic value instead of its
		 *  current STATIC assignment.
		 */
		printf("Warning: dynamic event detected [%x] [%x]\n",
		       inst->event, inst->instance);
	}
	else
		head->num_inst++;
}

compute_results (
	lock_head_t	head,
	int    		*dumb)
{
	struct stats	calc = {0.0,0.0,0,0.0,0.0,0};
	int     	x;

	printf("%-*s        %lu instance(s)\n",
	       EVENT_NAME_LENGTH, head->name, head->num_inst);

	printf("*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*\n");
    
	DoForSL(head->list, terse, head->name);
	
	if (a) {
		if (h) {

			/* ========================================== */
			/* Calculate aggregate hold mean if necessary */
			/* ========================================== */
    
			if (aggregate.hold.triggered)
				calc.h_mean = aggregate.hold.time.value /
					aggregate.hold.triggered;

			/* ============================================ */
			/* Calculate aggregate hold standard deviation  */
			/* if necessary 				*/
			/* ============================================ */

			if (aggregate.hold.triggered > 1)
				calc.h_sd = sqrt((double)
				  (aggregate.hold.time_sq.value -
			          ((aggregate.hold.time.value *
				  aggregate.hold.time.value)/
				  aggregate.hold.triggered)) /
				  (aggregate.hold.triggered-1));
		}

		if (w) {
			/* ========================================== */
			/* Calculate aggregate wait mean if necessary */
			/* ========================================== */

			if (aggregate.wait.triggered)
				calc.w_mean = aggregate.wait.time.value /
					aggregate.wait.triggered;

			/* =========================================== */
			/* Calculate aggregate wait standard deviation */
			/* if necessary 	 		       */
			/* =========================================== */

			if (aggregate.wait.triggered > 1)
				calc.w_sd = sqrt ((double)
				  (aggregate.wait.time_sq.value -
				  ((aggregate.wait.time.value *
				  aggregate.wait.time.value)/
				  aggregate.wait.triggered)) /
				  (aggregate.wait.triggered-1));
		}

		/* ========================= */
		/* Display Aggregate Results */
		/* ========================= */

		printf("AGGREGATE    mean       stdv    min time   max time    accesses   kind\n");
		printf("-------------------------------------------------------------------------\n");

		if (h) {
			printf("hold    |  %2.2f  | %2.2f  | %2.2f | %4.2f | %9lu | ",
			       calc.h_mean, calc.h_sd,
			       aggregate.hold.min_time.value,
			       aggregate.hold.max_time.value,
			       aggregate.hold.triggered);

			if (head->kind & SPIN_LOCK) printf ("spin\n");
			else  printf ("complex\n");
		}

		if (w) {
			printf("wait    |  %2.2f  | %2.2f  | %2.2f | %4.2f | %9lu |\n",
			       calc.w_mean, calc.w_sd,
			       aggregate.wait.min_time.value,
			       aggregate.wait.max_time.value,
			       aggregate.wait.triggered);
		}
	}

	printf ("-------------------------------------------------------------------------\n");
    
	if (c && a)
		interval_info(1, &aggregate);

	/* 
	 *  Reset aggregate data
	 */

	bzero((char *) &aggregate, sizeof(struct cbuff_entry));

	return (TRUE);
}

int
terse (
	cbuff_entry_t	inst,
	char 		*name)
{
	struct stats	calc = {0.0,0.0,0,0.0,0.0,0};
	int 		x;

	if (t) {

		if (h) {

			/* ====================================== */
			/* Calculate terse hold mean if necessary */
			/* ====================================== */

			if (inst->hold.triggered)
				calc.h_mean = inst->hold.time.value /
					inst->hold.triggered;

			/* ======================================= */
			/* Calculate terse hold standard deviation */
			/* if necessary 			   */
			/* ======================================= */

			if (inst->hold.triggered > 1)
				calc.h_sd = sqrt ((double) 
					(inst->hold.time_sq.value -
					((inst->hold.time.value *
					inst->hold.time.value) /
					inst->hold.triggered)) /
					(inst->hold.triggered-1));
		}

		if (w) {

			/* ====================================== */
			/* Calculate terse wait mean if necessary */
			/* ====================================== */

			if (inst->wait.triggered)
				calc.w_mean = inst->wait.time.value /
					inst->wait.triggered;

			/* ======================================= */
			/* Calculate terse wait standard deviation */
			/* if necessary 			   */
			/* ======================================= */

			if (inst->wait.triggered > 1)
				calc.w_sd = sqrt ((double)
					(inst->wait.time_sq.value -
					((inst->wait.time.value *
					inst->wait.time.value) /
					inst->wait.triggered)) /
					(inst->wait.triggered-1));
		}

		/* ===================== */
		/* Display Terse Results */	
		/* ===================== */

		printf ("%-10x   mean       stdv    min time   max time    accesses    op\n",inst->instance);
		printf ("-------------------------------------------------------------------------\n");

		if (h) {
			printf ("hold    |  %2.2f  | %2.2f  | %2.2f | %4.2f | %9lu | ",
				calc.h_mean, calc.h_sd,
				inst->hold.min_time.value,
				inst->hold.max_time.value,
				inst->hold.triggered);

			if      (inst->kind & SPIN_LOCK)    printf ("spin\n");
			else if (inst->kind & READ_LOCK)    printf ("read\n");
			else                                printf ("write\n");

		}

		if (w) {
			printf ("wait    |  %2.2f  | %2.2f  | %2.2f | %4.2f | %9lu |\n",
				calc.w_mean, calc.w_sd,
				inst->wait.min_time.value,
				inst->wait.max_time.value,
				inst->wait.triggered);
		}
	
		printf("-------------------------------------------------------------------------\n");
	}

	if (c && t)
		interval_info (0,inst);

	/* ================================ */
	/* tally aggregate data if selected */
	/* ================================ */

	if (a) {

		aggregate.hold.time.value    += inst->hold.time.value;
		aggregate.wait.time.value    += inst->wait.time.value;

		aggregate.hold.triggered += inst->hold.triggered;
		aggregate.wait.triggered += inst->wait.triggered;

		aggregate.hold.time_sq.value += inst->hold.time_sq.value;
		aggregate.wait.time_sq.value += inst->wait.time_sq.value;

		/* ========================= */
		/* collect minimum time data */
		/* ========================= */

		if (aggregate.wait.min_time.value == 0)
			aggregate.wait.min_time.value =
				inst->wait.min_time.value;
		else 
			if (aggregate.wait.min_time.value >
			    inst->wait.min_time.value)
				aggregate.wait.min_time.value =
					inst->wait.min_time.value;

		if (aggregate.hold.min_time.value == 0)
			aggregate.hold.min_time.value = 
				inst->hold.min_time.value;
		else 
			if (aggregate.hold.min_time.value >
			    inst->hold.min_time.value)
				aggregate.hold.min_time.value =
					inst->hold.min_time.value;

		/* ========================= */
		/* collect maximum time data */
		/* ========================= */

		if (aggregate.hold.max_time.value <
		    inst->hold.max_time.value)
			aggregate.hold.max_time.value =
				inst->hold.max_time.value;

		if (aggregate.wait.max_time.value <
		    inst->wait.max_time.value)
			aggregate.wait.max_time.value =
				inst->wait.max_time.value;

		/* ================================================ */
		/* update aggregate interval array data if selected */
		/* ================================================ */

		if (c)
			for (x=0; x < ETAP_CBUFF_IBUCKETS; x++) {
				aggregate.hold_interval[x] += 
					inst->hold_interval[x];
				aggregate.wait_interval[x] +=
					inst->wait_interval[x];
			}

	}

	return (TRUE);
}

void
interval_info (
	int 		mode,
	cbuff_entry_t	record)
{
	int x,y = 0;

	if (mode == 0)      printf ("INTERVAL  ");
	else                printf ("A-INTERVAL");

	for (x=1; x <= (ETAP_CBUFF_IBUCKETS); x++) {
		if (x == ETAP_CBUFF_IBUCKETS)
			printf("  %3d-oo",y);
		else
			printf("  %3d-%3d",y,x*time_slice-1);
		y = x*time_slice;
	}

	printf("\n----------");
	for (x=0; x < ETAP_CBUFF_IBUCKETS; x++)
		printf("---------");
    
	if (h) {
		printf("\nhold      ");
		for (x=0; x < ETAP_CBUFF_IBUCKETS; x++)
			printf(" |%7lu",record->hold_interval[x]);
	}

	if (w) {
		printf("\nwait      ");
		for (x=0; x < ETAP_CBUFF_IBUCKETS; x++)
			printf(" |%7lu",record->wait_interval[x]);
	}

	printf ("\n----------");
	for (x=0; x< ETAP_CBUFF_IBUCKETS; x++)
		printf("---------");
	printf ("\n");
}
   
cbuff_entry_t
get_instance_node (void)
{
    cbuff_entry_t	new;

    /* ================================= */
    /* create a new node for linked list */
    /* ================================= */

    if ((new = (cbuff_entry_t) malloc (record_size)) == NULL) {
        fprintf (stderr,"memory allocation error creating instance node\n");
        exit (1);
    }

    return (new);
}

lock_head_t
get_head_node (void)
{
    lock_head_t new;

    /* ================================= */
    /* create a new node for linked list */
    /* ================================= */

    if ((new = (lock_head_t) malloc (sizeof(struct lock_head))) == NULL) {
        fprintf (stderr,"memory allocation error creating head node\n");
        exit (1);
    }

    new->name[0]  = '\0';
    new->event    = 0;
    new->num_inst = 0;

    new->list = NewSL (lock_instance_cmp,NULL,NO_DUPLICATES);

    return (new);
}

int
lock_head_cmp (
	lock_head_t a,
	lock_head_t b)
{
    return (a->event - b->event);
}

int
lock_instance_cmp (
	cbuff_entry_t	a,
	cbuff_entry_t	b)
{
    return (a->instance - b->instance);
}

int
usage (int help)
{
    printf ("usage: etap_cstat [-t] [-a] [-c] [-e event] [-s subsystem] [-h] [-w] [-i input file]\n");

    if (help) {
        printf ("\n-t: terse statistical summary of each event\n");
        printf ("-a: aggregate statistical summary for each event\n");
        printf ("-c: cumulative interval information (used with switches a or t)\n");
        printf ("-e: event filter\n");
        printf ("-s: subsystem filter\n");
        printf ("-h: report hold times in microseconds\n");
        printf ("-w: report wait times in microseconds\n\n");
        printf ("-i: use a specified cumulative data file\n");
    }
    exit (1);
}

