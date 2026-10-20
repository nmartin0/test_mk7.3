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
 *  File     :  etap_mstat.c
 *
 *  Author   :  Joe CaraDonna
 *
 *  Function :  Reports statistical data on selected monitored lock event data.
 *
 *  Switches :  -t terse                 -e lock event
 *              -a aggregate             -s subsystem 
 *              -v validity report       -h hold times
 *              -u under time            -w wait times
 *              -o over time             -i input file
 *              -r per-record data       -d display vervose activity
 *              -c show distribution     -b change file buffer size
 *		-k kernel binary name
 *
 */

#include <stdio.h>
#include <mach.h>
#include <math.h>

#include "config.h"
#include <mach/etap.h>
#include <mach/etap_events.h>

#include "skiplist.h"
#include "convert.h"
#include "sym_ops.h"

typedef struct record_entry {
	double		     timestamp;/* converted microsecond stamp      */
	struct mbuff_entry   rec;  /* where record data is actually stored */
	struct record_entry *next; /* pointer to next record in list       */
} record_entry_t;

typedef struct type_header {
	char       	name [EVENT_NAME_LENGTH];/* text name of lock type */
	unsigned long	event;		/* lock subsys and type number     */
	unsigned long	num_inst;	/* number of instances in list     */
	SkipList	list;		/* list of instances               */
} type_header_t;

typedef struct inst_header {
	unsigned long	 instance;  /* address of the lock instance */
	unsigned long	 kind;	    /* complex/spinlock             */
	record_entry_t	*hold_list; /* pointer to beginning of hold list    */
	record_entry_t	*wait_list; /* pointer to beginning of wait list    */
	record_entry_t	*h_tail;    /* pointer to tail of hold list(append) */
	record_entry_t	*w_tail;    /* pointer to tail of wait list(append) */
	double           w_time;    /* vv TERSE data collected per instance */
	double           w_time_sq;
	unsigned long	 w_num;
	double           w_min;
	double           w_max;
	unsigned long	 w_interval [MSTAT_IBUCKETS];

	double           h_time;
	double           h_time_sq;
	unsigned long	 h_num;
	double           h_min;
	double           h_max;
	unsigned long	 h_interval [MSTAT_IBUCKETS];
} inst_header_t;

/* ======================== */
/* aggregate data structure */
/* collected per event      */
/* ======================== */

typedef struct agg {		  
	double		w_time;
	double		w_time_sq;
	unsigned long	w_num;
	double		w_min;
	double		w_max;
	unsigned long	w_interval [MSTAT_IBUCKETS];

	double		h_time; 
	double		h_time_sq;
	unsigned long	h_num;
	double		h_min;
	double		h_max;
	unsigned long	h_interval [MSTAT_IBUCKETS];
}agg_t;

agg_t aggregate;

SkipList subsystem [NUM_SUBSYS];  /* each subsystem has its own lock type list*/

boolean_t     w      = FALSE;         /* switch definitions  */
boolean_t     h      = FALSE;
boolean_t     a      = FALSE;
boolean_t     t      = FALSE;
boolean_t     r      = FALSE;         /* all records switch */
boolean_t     c      = FALSE;
boolean_t     d      = FALSE;
boolean_t     b      = FALSE;

FILE *fd [PROCESSORS];

struct st_entry *symbol_table; /* mk symbol table entries are stored in table */
int             table_size;    /* number of entries in symbol table           */
int             time_slice = MSTAT_IBUCKETS;
int             file_buffer_size = MSTAT_BUFSIZE;


void collect (
	mbuff_entry_t record,
	double timestamp,
	char txt_name[]);

int compute_results (
	type_header_t *t_head,
	int *dumb);

int terse (
	inst_header_t *i_head,
	int *dumb);

void interval_info (
	int mode,
	inst_header_t *i_head);

void valid_report (
	char filename[]);

inst_header_t* get_new_inst_header(void);

type_header_t* get_new_type_header(void);

record_entry_t* get_new_record_entry(void);

int lock_type_cmp (
	type_header_t *a,
	type_header_t *b);

int lock_instance_cmp (
	inst_header_t *a,
	inst_header_t *b);

void usage (
	int help);


int
main(int argc, char *argv[])
{
	char		filename[FILE_NAME_SIZE];    /* input filename     */
	char      	newname[CONV_FILE_NAME_SIZE];/* converted filename */
	char		*kernelname = "/mach_kernel";
	char      	*cp;                         
	char		object_name_list [MAX_OBJECTS][EVENT_NAME_LENGTH];
	unsigned short	object_list [MAX_OBJECTS];
	unsigned long	under_time = 0xffffffff;
	unsigned long	over_time  = 0;
	int       	index = 0;
	int		ret;   
	int		x;  
	int		file_num;         /* will contain file header info */
	boolean_t	lock_search   = FALSE; 
	boolean_t	subsys_search = FALSE; 
	boolean_t	v             = FALSE;	/* validity report display */
	boolean_t	found         = FALSE;
	struct mbuff_entry record;		/* read record             */
	double		ts;

	strcpy(filename, MBUFF_DATA_FILE);

	/* =================== */
	/* parse argument list */
	/* =================== */

	for (x=1; x < argc; x++) {

		cp = argv[x];
		if (*cp != '-')
			usage(0);

		switch (*(++cp)) {
			case 'w' : w  = TRUE; break;
			case 'h' : h  = TRUE; break;
			case 'a' : a  = TRUE; break;
			case 't' : t  = TRUE; break;
			case 'v' : v  = TRUE; break;
			case 'r' : r  = TRUE; break;
			case 'd' : d  = TRUE; break;
			case 'c' :
				c  = TRUE;
				if (x+1 <argc &&argv[x+1][0] >='0' &&
				    argv[x+1][0] <='9')
					time_slice = atoi(argv[++x]);
				break;
			case 'b' :
				b  = TRUE;
				if (x+1 < argc)
					file_buffer_size = atoi(argv[++x]);
				else
					usage(0);
				break;
			case 'k' :
				if (++x < argc)
					kernelname = argv[x];
				else
					usage(0);
				break;
			case 'u' : under_time = atol (argv[++x]); break;
			case 'o' : over_time  = atol (argv[++x]); break;
			case 'e' :
				if (subsys_search || (++x == argc)) usage (0);
				lock_search = TRUE;
				strcpy(&object_name_list[index++][0], argv[x]);
				break;
			case 's' :
				if (lock_search || (++x == argc)) usage (0);
				subsys_search = TRUE;
				strcpy(&object_name_list[index++][0], argv[x]);
				break;
			case 'i' : strcpy(filename, argv[++x]); break;
			case '?' : usage (1);
			default  : usage (0);
		}
	}
   
	/* =============== */
	/* default options */
	/* =============== */

	if (!(a|t|v|r))
		a = c = TRUE;

	if (!(h|w))
		h = w = TRUE;

	if (d) {
		printf("configuration:\n");
		printf("\tinput base file: %s\n",filename);
		printf("\tfile buffer size = %u\n",file_buffer_size);
		printf("\tdistribution interval width = %u\n",time_slice);
		printf("\tnumber of objects = %d\n\n",index);
	}

	/* ===================================== */ 
	/* generate validity report is requested */
	/* ===================================== */

	if (v) valid_report(filename);

	/* ================================== */
	/* open data files and assign buffers */
	/* ================================== */

	/* open first file and read header */
	file_name_conv(filename,newname,0);

	if (d)
		printf("opening %s\n", newname); 

	if ((fd[0] = fopen(newname,"r")) == NULL)  {
		fprintf(stderr,"error opening file: %s\n",newname);
		exit(1);
	}

	/* set file buffer */
	if (setvbuf(fd[0], NULL, _IOFBF, file_buffer_size)) {
		fprintf(stderr,"file buffer assignment error\n");
		exit(1);
	}

	fread(&file_num, sizeof(int), 1, fd[0]);

	/* open other processor files if any exist */
	for (x=1; x < file_num; x++) {

		/* append cpu id to file name */
		file_name_conv(filename, newname, x);

		if (d)
			printf("opening %s\n",newname);

		/* open file */
		if ((fd[x] = fopen(newname,"r")) == NULL)  {
			fprintf(stderr,"error opening file: %s\n",newname);
			exit(1);
		}

		/* assign file buffer */
		if (setvbuf(fd[x], NULL, _IOFBF, file_buffer_size)) {
			fprintf(stderr,"file buffer assignment error\n");
			exit(1);
		}

		/* make sure all headers have the same value */
		fread(&ret, sizeof(int), 1, fd[x]);
		if (ret != file_num) {
			fprintf(stderr,"data file mismatch: %s\n",newname);
			exit(1);
		}
	}

	/* ========== */
	/* initialize */
	/* ========== */

	if (d)
		printf ("verifing object list\n");

	/* map text names to their unsigned long representation */
	if (lock_search)
		text_to_short(LOCK_NAMES,&index,object_name_list,object_list);
	else if (subsys_search)
		text_to_short(SUBSYS_NAMES,&index,object_name_list,object_list);
	else {
		text_to_short(ALL_NAMES,&index,object_name_list,object_list);
		lock_search = TRUE;
	}

	bzero((char *) &aggregate, sizeof(struct agg));

	aggregate.h_min = aggregate.w_min = HUGE_VAL;

	/* initialize subsystem skip lists */
	for (x=0; x < NUM_SUBSYS; x++)
		subsystem[x] = NewSL(lock_type_cmp,NULL,NO_DUPLICATES);


	/* initialize symbol table entries if individual */
	/* occurrances are selected 			 */

	if (r) {
		symbol_table = symbol_table_load(kernelname, &table_size);
		if (symbol_table == NULL) {
			fprintf(stderr, "failed to load symbol table\n");
			exit(1);
		}
	}

  	/* ===================================== */
	/* collect data from each processor file */
	/* ===================================== */

	printf("\ncollecting data...\n");

	while (file_num--) {

		if (d)
			printf("\tscanning processor file %d...\n",file_num);

		while (1) {

			found = FALSE;
			x = 0;

			fread(&record,
			      sizeof(struct mbuff_entry),
			      1,
			      fd[file_num]);

			if (feof(fd[file_num]))
				break;

			while (x < index && !found) {
				if (record.event == object_list[x])
					found = TRUE;
				else
					x++;
			}

			/* ================================================== */
			/*  If found and the time is in the specified range,  */
			/*  collect the data                                  */
			/* ================================================== */

			if (found) {
				/* ============ */
				/* Convert Time */
				/* ============ */
#define tvalspec_to_usec(tv) \
	((double) (tv).tv_sec * USEC_PER_SEC + \
	 (double) (tv).tv_nsec / NSEC_PER_USEC)

				ts = tvalspec_to_usec(record.time);

 				/* =========== */
				/* Check range */
				/* =========== */

				if (ts >= over_time && ts <= under_time)
					collect(&record, ts,
						&object_name_list[x][0]);
			}
		}
		fclose(fd[file_num]);
	}

	/* ====================== */
	/* Process collected data */
	/* ====================== */
       
	for (x=0; x < NUM_SUBSYS; x++) 
		DoForSL(subsystem[x],compute_results,NULL);
	return 0;
}

void
collect (
	mbuff_entry_t record,
	double timestamp,
	char txt_name[])
{
	int      	ss;
	int      	ret;
	unsigned short 	bucket;
	type_header_t   t_key;
	type_header_t   *t_head;
	inst_header_t   i_key;
	inst_header_t   *i_head;
	record_entry_t  *new_record;

	/* ============================================= */
	/* Only collect the record if the hold/wait flag */
	/* is equal to the hold/wait option(s) selected. */
	/* ============================================= */

	if ((w == FALSE) && record->flags & ETAP_CONTENTION)
		return;
	if ((h == FALSE) && record->flags & ETAP_DURATION)
		return;

	/* ======================================================== */
	/* examine appropriate subsystem list for type header entry */
	/* ======================================================== */

	ss = (record->event & 0xff00) >> 8;
	t_key.event = record->event;

	if ((t_head = SearchSL(subsystem[ss], &t_key)) == NULL) {
		/* lock header doesn't exist: add and init */
		t_head = get_new_type_header();
		strncpy((char *) &t_head->name,
			(const char *) txt_name,
			EVENT_NAME_LENGTH-1);
		t_head->event = record->event;
		if ((ret = InsertSL(subsystem[ss],t_head)) != TRUE) {
			fprintf(stderr,
			"Warning: Type header insertion error [%d]\n",ret);
			return;
		}
	}

	/* ========================================== */
	/* search type list for instance header entry */
	/* ========================================== */

	i_key.instance = record->instance;

	if  ((i_head = SearchSL(t_head->list, &i_key)) == NULL) {
		/* lock instance doesn't exist: add and init */
		i_head = get_new_inst_header();
		i_head->instance = record->instance;
		i_head->kind = record->flags;
		if ((ret = InsertSL(t_head->list, i_head)) != TRUE) { 
			fprintf(stderr,
			"Warning: Instance header insertion error [%d]\n",ret);
			return;
		}
		t_head->num_inst++;
	}

	/* ================================================== */
	/* Append new record entry to proper list (hold/wait) */
	/* ONLY if the -r option is being used. (time saver)  */
	/* ================================================== */

	if (r || (c && a)) {
		/* allocate list node */
		new_record = get_new_record_entry();
		/* load new node with record data */
		new_record->rec = *record;
		new_record->timestamp = timestamp;
		/* append */
		if (record->flags & ETAP_CONTENTION) {
			if (i_head->wait_list == NULL)
				i_head->wait_list = new_record;
			else
				i_head->w_tail->next = new_record;
			i_head->w_tail = new_record;
		}
		else if (record->flags & ETAP_DURATION) {
			if (i_head->hold_list == NULL)
				i_head->hold_list = new_record;
			else
				i_head->h_tail->next = new_record;
			i_head->h_tail = new_record;
		}
		else {
			fprintf(stderr,
			"illegal op: neither HOLD or WAIT are specified\n");
			exit(1);
		}
	}

	if (t || a) {

		/* =================== */
		/* calculate the terse */
		/* =================== */

		/* Is it a lock wait entry ? */
		if (record->flags & ETAP_CONTENTION) {
			i_head->w_time += timestamp;
			i_head->w_num++;
			if (i_head->w_min > timestamp)
				i_head->w_min = timestamp;

			if (i_head->w_max < timestamp)
				i_head->w_max = timestamp;

			i_head->w_time_sq +=
				(unsigned long)(timestamp * timestamp);

			/* ================================================== */
			/* Increment appropriate wait agg distribution bucket */
			/* ================================================== */

			if (c) {
				bucket = timestamp / time_slice;
				if (bucket >= MSTAT_IBUCKETS)
				       i_head->w_interval[MSTAT_IBUCKETS-1]++;
				else
					i_head->w_interval[bucket]++;
			}
		}

		/* OR is it a lock hold entry ? */
		else if (record->flags & ETAP_DURATION) {
			i_head->h_time += timestamp;
			i_head->h_num++;
			if (i_head->h_min > timestamp)
				i_head->h_min = timestamp;

			if (i_head->h_max < timestamp)
				i_head->h_max = timestamp;

			i_head->h_time_sq +=
				(unsigned long)(timestamp * timestamp);

			/* ================================================== */
			/* Increment appropriate hold agg distribution bucket */
			/* ================================================== */

			if (c) {
				bucket = timestamp / time_slice;
				if (bucket >= MSTAT_IBUCKETS)
					i_head->h_interval[MSTAT_IBUCKETS-1]++;
				else
					i_head->h_interval[bucket]++;
			}
		}
	}
}

int
compute_results (
	type_header_t 	*t_head,
	int 		*dumb)
{
	int h_mean  = 0;
	int w_mean  = 0;
	int h_sd    = 0;
	int w_sd    = 0;

	printf("\n%-*s      %lu type instance(s)\n",
	       EVENT_NAME_LENGTH,t_head->name,t_head->num_inst);

	printf("*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*\n");
	DoForSL(t_head->list,terse,NULL);

	if (a) {

		/* ====================================== */
		/* Calculate aggregate means if necessary */
		/* ====================================== */

		if (aggregate.h_num)
			h_mean = aggregate.h_time / aggregate.h_num;
		if (aggregate.w_num) 
			w_mean = aggregate.w_time / aggregate.w_num;
		if (aggregate.h_min == HUGE_VAL)
			aggregate.h_min = 0;	/* There were no hold values. */
		if (aggregate.w_min == HUGE_VAL)
			aggregate.w_min = 0;	/* There were no wait values. */

		/* ==================================================== */
		/* Calculate aggregate standard deviations if necessary */
		/* ==================================================== */

		if (aggregate.h_num > 1)
			h_sd = sqrt ((double) (aggregate.h_time_sq -
				    ((aggregate.h_time * aggregate.h_time) /
				     aggregate.h_num)) /
				     (aggregate.h_num-1));

		if (aggregate.w_num > 1)
			w_sd = sqrt ((double) (aggregate.w_time_sq -
				    ((aggregate.w_time*aggregate.w_time) /
				     aggregate.w_num)) /
				     (aggregate.w_num-1));

		printf("AGGREGATE    mean       stdv    min time   max time    accesses\n");
		printf("-------------------------------------------------------------------------\n");

		if (h)
		    printf("hold    |  %7d  | %7d  | %6.0f | %9.0f | %9lu\n",
			   h_mean, h_sd, aggregate.h_min, aggregate.h_max,
			   aggregate.h_num);

		if (w)
		    printf("wait    |  %7d  | %7d  | %6.0f | %9.0f | %9lu\n",
			   w_mean, w_sd, aggregate.w_min, aggregate.w_max,
			   aggregate.w_num);

		printf("-------------------------------------------------------------------------\n");

	}

	if (a && c)
		interval_info(1,NULL);

	bzero((char *) &aggregate, sizeof(struct agg));

	aggregate.h_min = aggregate.w_min = HUGE_VAL;

	return (TRUE);
}


int
terse (
	inst_header_t *i_head,
	int *dumb)

{
	int    h_mean      = 0;   
	int    w_mean      = 0;
	int    h_sd        = 0;
	int    w_sd        = 0;
	int    bucket;
	unsigned long	 l_offset;
	unsigned long	 ul_offset;
	record_entry_t	*scan;
	struct st_entry	*locker;
	struct st_entry	*unlocker;


	/* ==================================== */
	/* If all records are to be printed out */
	/* do it now, prior to terse data       */
	/* ==================================== */

	if (r) {
		/* ================== */
		/* print hold records */
		/* ================== */

		scan = i_head->hold_list;
		if (scan != NULL) {
			printf("HOLD         time (us)     op      offset        function name\n");
			printf("-------------------------------------------------------------------------\n");
			while (scan != NULL) {

				/* ======================== */
				/* Find symbol table entrys */
				/*			    */	
			        /* data[0] = start_pc	    */
				/* data[1] = end_pc	    */
				/* ======================== */

				locker   = funky_find(symbol_table,
						      table_size,
						      scan->rec.data[0],
						      &l_offset);

				unlocker = funky_find(symbol_table,
						      table_size,
						      scan->rec.data[1],
						      &ul_offset);

				printf("locker    | %9d  | ",
				       (int) scan->timestamp);

				if (scan->rec.flags & SPIN_LOCK)
					printf("spin  ");
				else if (scan->rec.flags & READ_LOCK)
					printf("read  ");
				else if (scan->rec.flags & WRITE_LOCK)
					printf("write ");
				else
					printf("undef ");

				printf(" | %7ld  |  %s\n",l_offset,locker->f_name);
				printf ("unlocker  |            |        | %7ld  |  %s\n",ul_offset,unlocker->f_name);

				scan = scan->next;
				printf(".........................................................................\n");
			}
			printf("-------------------------------------------------------------------------\n");
		}

		/* ================== */
		/* print wait records */
		/* ================== */

		scan = i_head->wait_list;
		if (scan != NULL) {
			printf("WAIT         time (us)     op      offset        function name\n");
			printf("-------------------------------------------------------------------------\n");

			while (scan != NULL) {

				/* ======================== */
				/* Find symbol table entry  */
				/*			    */	
			        /* data[0] = start_pc	    */
				/* ======================== */

				locker = funky_find(symbol_table,
						    table_size,
						    scan->rec.data[0],
						    &l_offset);

				printf("locker    | %9d  | ",
				       (int) scan->timestamp);

				if (scan->rec.flags & SPIN_LOCK)
					printf ("spin  ");
				else if (scan->rec.flags & READ_LOCK)
					printf ("read  ");
				else printf ("write ");

				printf(" | %7ld  |  %s\n",l_offset,
				       locker->f_name);
               
				scan = scan->next;
				printf (".........................................................................\n");
			}
			printf ("-------------------------------------------------------------------------\n");
		}
	}

	/* ============================ */
	/* Print terse data if selected */
	/* ============================ */

	if (t) {

		if (i_head->h_num)
			h_mean = i_head->h_time / i_head->h_num;

		if (i_head->h_num > 1)
			h_sd = sqrt ((double) (i_head->h_time_sq -
				    ((i_head->h_time * i_head->h_time) /
				     i_head->h_num)) /
				     (i_head->h_num-1));

		if (i_head->w_num)
			w_mean = i_head->w_time / i_head->w_num;

		if (i_head->w_num > 1)
			w_sd = sqrt ((double) (i_head->w_time_sq -
				    ((i_head->w_time * i_head->w_time) /
				     i_head->w_num)) /
				     (i_head->w_num-1));


		printf("TERSE        mean       stdv    min time   max time    accesses   kind\n");
		printf("-------------------------------------------------------------------------\n");

		if (h) {
		    printf("hold    |  %7d  | %7d  | %6.0f | %9.0f | %9lu | ",
			   h_mean, h_sd, i_head->h_min, i_head->h_max,
			   i_head->h_num);

		    if (i_head->kind & SPIN_LOCK)
			    printf("spin\n");
		    else if (i_head->kind & COMPLEX_LOCK)
			    printf("complex\n");
		    else
			    printf("undef\n");
		}

		if (w)
		   printf("wait    |  %7d  | %7d  | %6.0f | %9.0f | %9lu |\n",
			  w_mean, w_sd, i_head->w_min, i_head->w_max,
			  i_head->w_num);

		printf ("-------------------------------------------------------------------------\n");
    }

	if (t && c)
		interval_info(0,i_head);

	/* ================================== */
	/* Update aggregate data if specified */
	/* ================================== */

	if (a) {

		aggregate.w_time += i_head->w_time;
		aggregate.h_time += i_head->h_time;

		aggregate.w_time_sq += i_head->w_time_sq;
		aggregate.h_time_sq += i_head->h_time_sq;
		aggregate.w_num     += i_head->w_num;
		aggregate.h_num     += i_head->h_num;


		if (aggregate.w_min > i_head->w_min)
			aggregate.w_min = i_head->w_min;

		if (aggregate.h_min > i_head->h_min)
			aggregate.h_min = i_head->h_min;
    
		if (aggregate.w_max < i_head->w_max)
			aggregate.w_max = i_head->w_max;

		if (aggregate.h_max < i_head->h_max)
			aggregate.h_max = i_head->h_max;

	}

	/* ==================================================== */
	/* Increment appropriate aggregate distribution buckets */
	/* ==================================================== */

	if (c && a) {

		scan = i_head->hold_list;
		while (scan != NULL) {
			bucket = scan->timestamp / time_slice;
			if (bucket >= MSTAT_IBUCKETS)
				aggregate.h_interval[MSTAT_IBUCKETS-1]++;
			else
				aggregate.h_interval[bucket]++;
			scan = scan->next;
		}

		scan = i_head->wait_list;
		while (scan != NULL) {
			bucket = scan->timestamp / time_slice;
			if (bucket >= MSTAT_IBUCKETS)
				aggregate.w_interval[MSTAT_IBUCKETS-1]++;
			else
				aggregate.w_interval[bucket]++;
			scan = scan->next;
		}
	}

	return (TRUE);
}

void
interval_info (
	int mode,
	inst_header_t *i_head)

{
	int x,y = 0;

	/* ========================================================= */
	/* Mode 0 = terse mode (an instance header is expected)      */
	/* Mode 1 = aggregate mode (uses global aggregate structure) */
	/* ========================================================= */

	if (mode == 0) {
		if (i_head == NULL)
			return;
		else
			printf("T-INTERVAL");
	}
	else
		printf("A-INTERVAL");

	for (x=1; x <= (MSTAT_IBUCKETS); x++) {
		if (x == MSTAT_IBUCKETS)
			printf("  %3d-oo",y);
		else
			printf("  %3d-%3d",y,x*time_slice-1);
		y = x*time_slice;
	}

	printf("\n----------");
	for (x=0; x < MSTAT_IBUCKETS; x++)
		printf("---------");
    
	if (h) {
		printf("\nhold      ");
		for (x=0; x < MSTAT_IBUCKETS; x++) {
			if (mode == 0)
				printf(" |%7lu",i_head->h_interval[x]);
			else
				printf(" |%7lu",aggregate.h_interval[x]);
		}
	}

	if (w) {
		printf("\nwait      ");
		for (x=0; x < MSTAT_IBUCKETS; x++) {
			if (mode == 0)
				printf(" |%7lu",i_head->w_interval[x]);
			else
				printf(" |%7lu",aggregate.w_interval[x]);
		}
	}

	printf("\n----------");
	for (x=0; x< MSTAT_IBUCKETS; x++)
		printf("---------");
	printf("\n");
}

void
valid_report (
	char filename[])

{
	FILE *lost;
	int  x = 0;
	float percent; 
  
	struct lost_stat {
		int written;
		int lost;
	};

	struct lost_stat ls;

	strcat (filename,".lost");
	if ((lost = fopen(filename,"r")) == NULL)  {
		fprintf (stderr,"error opening file: %s\n",filename);
		exit (1);
	}

	printf ("-------------------------------------------------------------------------\n");
	printf ("   cpu       written        lost          valid\n");
	printf ("-------------------------------------------------------------------------\n");
 
	while (!feof(lost)) {
		fread (&ls,sizeof(struct lost_stat),1,lost);
		if (!feof(lost)) {
			if (ls.written == 0)
				printf("   %2d   |     -na-    |     -na-    |      -na-\n",x++); 
			else {
				percent = ((float)ls.written /
					   (ls.written+ls.lost))*100;
				printf("   %2d   | %10d  | %10d  |  %7.1f%%\n",
				       x++,ls.written, ls.lost,percent);
			}
		}
	}
    
	printf ("-------------------------------------------------------------------------\n");
  
	fclose (lost);

	if (!(a|t))
		exit (0);
}

inst_header_t* 
get_new_inst_header(void)
{
	inst_header_t *new;
    
	/* ================================= */
	/* create a new node for linked list */
	/* ================================= */

	if ((new = (inst_header_t *) malloc (sizeof(inst_header_t))) == NULL) {
		fprintf(stderr,
		"memory allocation error creating instance header node\n");
		exit(1);
	}

	bzero((char *) new, sizeof(inst_header_t));

	new->w_min = new->h_min = HUGE_VAL;

	return(new);
}

type_header_t* 
get_new_type_header(void)
{
	type_header_t *new;
    
	/* ================================= */
	/* create a new node for linked list */
	/* ================================= */

	if ((new = (type_header_t *) malloc (sizeof(type_header_t))) == NULL) {
		fprintf(stderr,
		"memory allocation error creating type header node\n");
		exit (1);
	}

	bzero((char *) new, sizeof(type_header_t));

	new->list = NewSL(lock_instance_cmp, NULL, NO_DUPLICATES);

	return(new);
}

record_entry_t*  
get_new_record_entry(void)
{
	record_entry_t *new;

	if ((new = (record_entry_t *) malloc(sizeof(record_entry_t))) == NULL) {
		fprintf(stderr,
		"memory allocation error creating record entry node\n");
		exit(1);
	}

	bzero((char *) new, sizeof(record_entry_t));

	return(new);
}

int
lock_type_cmp (
	type_header_t *a,
	type_header_t *b)
{
	return (a->event - b->event);
}

int
lock_instance_cmp (
	inst_header_t *a,
	inst_header_t *b)
{
	return (a->instance - b->instance);
}

void
usage (
	int help)
{
  printf("etap_mstat is a statistical tool designed to analyze monitored lock\n");
  printf("trace data created by the etap_md daemon.\n\n");

  printf("usage: etap_mstat [-t] [-a] [-e lock event] [-s subsystem] [-h] [-w]\n"); 
  printf("               [-u time] [-o time] [-i input file] [-c] [-v]\n");
  printf("               [-d] [-b #]\n\n");

  if (help) {
     printf("\t-r: provides per-record data, including function names & offsets\n");
     printf("\t-t: provides terse statistical summary of each lock\n");
     printf("\t-a: provides aggregate statistical summary for each lock type\n\n");
     printf("\t-e: lock event filter\n");
     printf("\t-s: subsystem filter\n\n");
     printf("\t-c: provides lock distribution information\n");
     printf("\t-h: reports lock hold times in microseconds\n");
     printf("\t-w: reports lock wait times in microseconds\n\n");
     printf("\t-u: selects records with measured times under time microseconds\n");
     printf("\t-o: selects records with measured times over  time microseconds\n\n");
     printf("\t-i: provides an alternate input file containing lock data\n");
     printf("\t-v: validity report\n");
     printf("\t-d: display verbose activity information\n");
     printf("\t-b: change default file buffer size\n\n");
     printf("\t-k: supply the name of the kernel image (default /mach_kernel)\n");
  }

  exit(0);
}
