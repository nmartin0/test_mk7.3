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
 *  File     :  etap_md.c
 *
 *  Author   :  Joe CaraDonna
 *
 *  Function :  A multi-threaded daemon that writes data collected,
 *              during ETAP event monitoring, to disk.
 *
 */

#include <stdio.h>
#include <sys/signal.h>
#include <sys/time.h>

#include <mach.h>
#include <rthreads.h>

#include "config.h"
#include <mach/etap.h>
#include <mach/etap_events.h>
#include <mach/sync_policy.h>
#include <mach/etap.h>

#define	ONE_SECOND	1000

int procs    = 0;               /* number of processors to monitor          */ 
int CPUs     = 0;
int buf_size = 0;               /* size of monitor buffer in bytes          */
int buf_recs = 0;               /* number of records in monitor buffer      */
int run      = 1;               /* termination var, changed by shutdown()   */

boolean_t  t = FALSE;		/* trace daemon activity                    */


FILE *data_log[PROCESSORS];     /* holds records written in monitor buffers */
FILE *lost_log;                 /* holds lost record data                   */ 

struct lost_stat {              /* stores record info for validity %        */
    int total_written;
    int total_lost;
};

struct lost_stat *lost_stat_data;

char	utilname[40];

void monitor(int cpu);		/* thread routine (1 per processor)         */
void shut_down(int);		/* clean up routine (invoked on a KILL)     */
void usage(void);		/* parameter usage */


int
main (int argc,	char *argv[])
{
	char              filename[FILE_NAME_SIZE];
	char              newname [CONV_FILE_NAME_SIZE];
	char		  *cp;
	mach_msg_header_t fake_msg;
	int               dum,x,ret;
	semaphore_port_t  wait_sema;
	
	strncpy(filename, MBUFF_DATA_FILE, sizeof(filename)-1);
    
	/* =================== */
	/* parse argument list */
	/* =================== */

	for (x=1; x < argc; x++) {
		cp = argv[x];
		if (*cp != '-')
			usage();

		switch (*(++cp)) {
			case 'o' : strcpy(filename, argv[++x]); break;
			case 't' : t = TRUE; break;
			case '?' : usage();
			default  : usage();
		}
	}

	ret = etap_info(&dum,&dum,&dum,&buf_size,&buf_recs,&procs);

	/* ===================================================== */
	/* Make sure monitor tracing is configured in the kernel */
	/* ===================================================== */

	if (ret || !buf_size || !buf_recs || !procs) {
	      fprintf (stderr,
	      "\nETAP monitor tracing is not configured properly\n");
	      exit(1);
        }

	CPUs = procs;

	/* ========================================= */
	/* Allocate memory for data validity records */
	/* ========================================= */

	lost_stat_data = (struct lost_stat*)
		malloc(sizeof(struct lost_stat) * CPUs);

	semaphore_create(mach_task_self(), &wait_sema, SYNC_POLICY_FIFO, 0);

	/* ============= */
	/* setup signals */
	/* ============= */

	signal (SIGINT,  shut_down);
	signal (SIGHUP,  shut_down);
	signal (SIGQUIT, shut_down);
	signal (SIGTERM, shut_down);

	/* ====================================================== */
	/* open files                                             */
 	/*                                                        */
	/* Note: each processor thread will write to its own file */
	/* ====================================================== */

	for (x=0; x<CPUs; x++) {
		file_name_conv(filename, newname, x);

		if ((data_log[x] = fopen(newname, "w")) == NULL) {
			fprintf(stderr,"\nerror opening file: %s\n", newname);
			exit(1);
		}
		/* write simple header to each open file */
		fwrite(&CPUs, sizeof(int), 1, data_log[x]);
		fflush(data_log[x]);
	}

	strcat(filename, ".lost");
	if ((lost_log = fopen(filename,"w")) == NULL) {
		fprintf(stderr,"\nerror opening lost data log file: %s\n",
			filename);
		exit(1);
	}

	/* ====================== */
	/* Launch monitor threads */
	/* ====================== */

	for (x= (CPUs-1); x >= 0; x--)
		rthread_spawn((rthread_fn_t) monitor, (any_t) x);

	/* ========================= */
	/* Wait for shut down signal */
	/* ========================= */

	semaphore_wait(wait_sema);
	/* NEVER REACHED */
}

void
monitor (int cpu)
{
	monitor_buffer_t	addr;
	struct lost_stat      	*status;
	mach_msg_header_t     	fake_msg;
	mach_port_t           	sleep_port;
	u_long                	l_free, c_free;
	u_long               	l_time, c_time;
	int                   	iteration  = 0;
	u_long                	lost       = 0;
	int                   	asleep     = 0;
	int                   	record_size;
	int                   	num;
	int                   	ret;
	int                   	x;
	etap_data_t		probe_data;

	/* =========================== */
	/*  Initialize Monitor Thread  */ 
	/* =========================== */

	status = lost_stat_data+cpu;

	ret = mach_port_allocate(mach_task_self(),
				 MACH_PORT_RIGHT_RECEIVE,
				 &sleep_port);

	if (ret != KERN_SUCCESS) {
		fprintf(stderr,"error creating monitor thread sleep port\n");
		rthread_exit(1);
	}

	ret = etap_map(ETAP_BUFFER_MONITORED, cpu, &addr);
  
	if (ret) {
		fprintf(stderr,"cannot map monitor buffer %d into task VAS\n",
			cpu);
		fprintf(stderr,"cpu[%d] monitor aborting\n",cpu);
		rthread_exit(1);
	}

	record_size = sizeof(struct mbuff_entry);

	/*
	 * Set indexes
	 */

	l_time = c_time = addr->timestamp;
	l_free = c_free = addr->free;

	/*
	 * Enable tracing if requested
	 */
	if (t)	{
		rthread_trace(rthread_self(), TRUE);

		probe_data[0] = cpu;
		probe_data[1] = 0xffff;
		probe_data[2] = 0xffff;
		probe_data[3] = 0xffff;
		
		etap_probe(0xffff, ETAP_DATA_SIZE, &probe_data);
	}

	/* ============================================= */
	/* Collect data from circular monitor buffer     */
	/* - sleep if no updates have been made          */
	/* ============================================= */

	while (run) {

		/* ===================================== */
		/* No changes occurred within the buffer */
		/* ===================================== */

		if ((c_free == l_free) && (c_time == l_time))  {
			asleep = (asleep % 5) + 1;

			ret = mach_msg(&fake_msg,
				 MACH_RCV_MSG | MACH_RCV_TIMEOUT,
				 0,1,
				 sleep_port,
				 asleep * ONE_SECOND,
				 MACH_PORT_NULL);
		}

		/* ================================== */
		/* Normal: changes occurred in buffer */
		/* ================================== */
		else if ((c_free > l_free) && (c_time == l_time)) {
			asleep = 0; 
			num = c_free - l_free;

			ret = fwrite(&addr->entry[l_free],
				     record_size * num,
				     1,
				     data_log[cpu]);

			if (ret != 1) 
				fprintf(stderr, "write error: state (1)\n");
			status->total_written += num;
		}

		/* ========================================== */ 
		/* Managable Loop: changes occurred in buffer */
		/* ========================================== */
		else if ((c_free < l_free) && (c_time == l_time+1)) {
			asleep = 0;

			/* write from l_free to end of buffer */
			ret= fwrite(&addr->entry[l_free],
				    record_size,
				    buf_recs-l_free,
				    data_log[cpu]);

			/* write from beginning of buffer to c_free */
			ret += fwrite(&addr->entry[0],
				      record_size,
				      c_free,
				      data_log[cpu]);

			if (ret != (buf_recs-l_free)+c_free) 
				fprintf(stderr, "write error: state (2)\n");

			status->total_written += ret;
		}

		/* =============================================== */
		/* Managable Tied-Loop: changes occurred in buffer */
		/* =============================================== */
		else if ((c_free == l_free) && (c_time == l_time+1))  {
			asleep = 0;
			ret = fwrite(&addr->entry[0],
				     record_size * buf_recs,
				     1,
				     data_log[cpu]);

			if (ret != 1) 
				fprintf(stderr, "write error: state (3)\n");
		}

		/* =========================== */
		/* OVERWRITE-Looped: Data lost */
		/* =========================== */
		else if (c_time > l_time) {
			asleep = 0;

			/* calculate number of records lost */
			if (c_free > l_free)
				lost = buf_recs * (c_time-l_time-1) +
					c_free - l_free;
			else
				lost = buf_recs * (c_time-l_time-1) +
					(buf_recs-l_free) + c_free;
            
			ret = fwrite(&addr->entry[0],
				     record_size * buf_recs,
				     1,
				     data_log[cpu]);

			if (ret != 1) 
				fprintf(stderr, "write error: state (4)\n");

			status->total_lost     += lost;
			status->total_written  += buf_recs;
		}

		/* ========================================================== */
		/* Note:						      */
		/*    c_time should never be less than l_time at this point.  */
		/*    If it is, then it's attributed to a circular buffer     */
		/*    loop where the timestamp hasn't been updated yet.       */
		/*    This occurrence can therefore be handled as a normal    */
		/*    event here. Thus it is assumed the timestamp mismatch   */
		/*    will correct itself by the next iteration.              */
		/* ========================================================== */
		else {
			asleep = 0;

			/* write from l_free to end of buffer */
			ret = fwrite(&addr->entry[l_free],
				     record_size,
				     buf_recs - l_free,
				     data_log[cpu]);

			/* write from beginning of buffer to c_free */
			ret += fwrite(&addr->entry[0],
				      record_size,
				      c_free,
				      data_log[cpu]);

			if (ret != (buf_recs-l_free)+c_free) 
				fprintf(stderr, "write error: state (5)\n");

			status->total_written += ret;
		}

		l_time = c_time;
		l_free = c_free;

		c_free = addr->free;
		/* kill some time, wait for timestamp to update */
		if (c_free == 0)
			for (x=0; x < TIMESTAMP_UPDATE_DELAY; x++);
		c_time = addr->timestamp;

		iteration = (iteration+1) % 2;
	}

	/* ============================ */
	/* thread prepares to terminate */
	/* ============================ */

	fflush(data_log[cpu]);

	if (fclose(data_log[cpu]))
		fprintf(stderr, "error closing data log file [%d]\n",cpu);

	rthread_exit(0);
}

void
shut_down (int signo)
{
	run = 0;

	etap_probe(0xdead,0,ETAP_DATA_NULL);

	rthread_wait();

	fwrite(lost_stat_data, sizeof(struct lost_stat), CPUs,lost_log);
	if (fclose (lost_log))
		fprintf(stderr,"error closing lost log file\n");

	etap_probe(0xdeee,0,ETAP_DATA_NULL);

	exit(0);
}

void
usage(void)
{
	fprintf(stderr,"usage: %s [-o filename] [-t]\n", utilname);
	fprintf(stderr,"       -t enable daemon thread event tracing\n");
	exit(1);
}

