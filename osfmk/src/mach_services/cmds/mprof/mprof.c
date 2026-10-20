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
 */

/*
 * cmk1.1
 */

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <stdio.h>
#include <mach.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <mach_error.h>
#include <servers/netname.h>
#include <servers/machid.h>
#include <a.out.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <setjmp.h>
#include <mach/thread_switch.h>
#include "mprof.h"

#define kr_check(exit_code, print_args)					\
	if (kr != KERN_SUCCESS) {					\
       	  fprintf(stderr, "%s%s\n", print_args, mach_error_string(kr));	\
	  kill_parent();	       					\
	  exit(exit_code);						\
	} 

static mach_port_t server;
static mach_port_t auth;
jmp_buf saved_state;

#define streql(a, b)	(strcmp((a), (b)) == 0)

#define task_by_pid(pid)	syscall(-33u, (pid))

char *sample_file_name = "mprof.out";
FILE *sample_file = 0;

char *symbol_file_name = "/mach_kernel";
FILE *symbol_file;
boolean_t symbol_file_option = FALSE;

#define MPROF_NULL	0x00
#define MPROF_ON	0x01
#define MPROF_OFF	0x02
#define MPROF_PRINT	0x04
#define MPROF_SELF	0x08

int	action = MPROF_NULL;

struct target {
	mach_id_t	mid;
	mach_port_t	port;
	mach_type_t	type;
} *targets;

char **mid_names;

int ntargets;

task_port_t		get_kernel_task();

struct timeval timeval, start_time, stop_time;
struct timezone tz;

#define SEEK_SET 0

/*
 * PCs are signed because of qsort
 */

struct symbol *symbols;
int	nsymbols;			/* number of symbol names */

unsigned int	*samples;
int		nsamples;

int debug;
int foreground;
int parent;

float min_percent = 0;

void
leave()
{
	exit(0);
}

main(argc, argv)
char *argv[];
{
	int i;

#if linux
	{
		extern int (*_mach_init_routine)(void);
		/* inform server that we are mach-aware */
		ioperm(0,0,1);
		/* make sure that libmach gets initialised */
		if (_mach_init_routine) {
			(*_mach_init_routine)();
		}
	}
#endif /* linux */

	for (i = 1; i < argc; i++) {
	      	if (streql(argv[i], "?")) {
		  	help_msg(0);
			exit(0);
	      	} else if (streql(argv[i], "-help")) {
		  	help_msg(1);
			exit(0);
	      	} else if  (streql(argv[i], "on") ||
		    streql(argv[i], "-on"))
		  	set_action(MPROF_ON);
		else if (streql(argv[i], "off") ||
			 streql(argv[i], "-off"))
		  	set_action(MPROF_OFF);
		else if (streql(argv[i], "print") ||
			 streql(argv[i], "-print"))
		  	set_action(MPROF_PRINT);
		else if (streql(argv[i], "-F"))
			foreground++;
		else if (streql(argv[i], "-v") || 
			 streql(argv[i], "-d"))
		  	debug++;
		else if (streql(argv[i], "-f")) {
			check_action(MPROF_PRINT|MPROF_ON|MPROF_SELF);
			if (++i >= argc || *argv[i] == '-')
				usage();
			sample_file_name = argv[i];
		} else if (streql(argv[i], "-")) {
			check_action(MPROF_PRINT);
			if ((i+1) < argc && *argv[i+1] != '-')
				usage();
			sample_file = stdin;
		} else if (streql(argv[i], "-n")) {
			check_action(MPROF_PRINT|MPROF_SELF);
			if (++i >= argc || *argv[i] == '-')
				usage();
			symbol_file_option = TRUE;
			symbol_file_name = argv[i];
		} else if (streql(argv[i], "-trunc")) {
			check_action(MPROF_PRINT);
			if (++i >= argc || *argv[i] == '-')
				usage();
			min_percent = atoi(argv[i]);
		} else if (!ntargets && atod(argv[i], 0)) {
	    		mid_names = argv+i;
			ntargets = 1;
			while (i+1 < argc && atod(argv[i+1], 0)) {
				ntargets++;
				i++;
			}
	        } else 
			break;
	}

	check_action(MPROF_PRINT|MPROF_ON|MPROF_OFF|MPROF_SELF);

	argv += i;
	argc -= i;

	switch(action) {
	case MPROF_ON:
		parent = getpid();
		signal(SIGHUP, leave);
		if (!foreground) {
			int child;

			if ((child = fork()) < 0) {
				perror("can't fork");
				exit(1);
			}
			else if (child)
				pause();
			else {
#ifdef	linux
				{
					extern int (*_mach_init_routine)(void);
					ioperm(0,0,1);
					if (_mach_init_routine) {
						(*_mach_init_routine)();
					}
				}
#endif
			}
		}
		/* fall through */
	case MPROF_OFF:
		if (argc)
			usage();
		get_mids();
		profile(0);
		break;
	case MPROF_PRINT:
		if (argc)
			usage();
		else {
			get_symbols();
			read_samples();
			print_samples();
			break;
		}
		/* fall through */
	case MPROF_SELF: /* Must be a command */
		if (!argc)
			usage();
		parent = getpid();
		profile_self(argv);
	}
}

help_msg(help)
{
    fprintf(stderr, "usage:\n\n");

    fprintf(stderr, "mprof -on [-f <output>] [MIDS]\n");
    if (help) {
     fprintf(stderr, "	o Starts profiling on a set of tasks and/or threads.\n");
     fprintf(stderr, "	  <MIDS> are mach ids as given by the ms command.\n");
     fprintf(stderr, "	o If <MIDS> are omitted, the microkernel task is the\n");
     fprintf(stderr, "	  profiled target, otherwise tasks and threads\n");
     fprintf(stderr, "	  associated to <MIDS> are the profiled targets.\n");
     fprintf(stderr, "	o <ouput> designates where to write the profiling\n");
     fprintf(stderr, "	  data. (default is mprof.out)\n\n");
    }	

    fprintf(stderr, "mprof -off [MIDS]\n");
    if (help) {
     fprintf(stderr, "	o Stops profiling on a set of tasks and/or threads.\n");
     fprintf(stderr, "	o If <MIDS> are omitted, the microkernel task is the\n");
     fprintf(stderr, "	  target, otherwise tasks and threads associated\n");
     fprintf(stderr, "	  to <MIDS> are the targets.\n\n");
;
    }	

    fprintf(stderr, "mprof [MIDS] [-f <output>] command\n");
    if (help) {
     fprintf(stderr, "	o Profiles a set of tasks and/or threads for the\n");
     fprintf(stderr, "	  duration of <command>.\n");
     fprintf(stderr, "	o If <MIDS> are omitted, the command task is the\n");
     fprintf(stderr, "	  profiled target, otherwise tasks and threads\n");
     fprintf(stderr, "	  associated to <MIDS> are the profiled targets\n");
     fprintf(stderr, "	o <ouput> designates where to write the profiling\n");
     fprintf(stderr, "	  data. (default is mprof.out)\n\n");
    }	

    fprintf(stderr, "mprof -print [- | -f <input>] [-n <symbols>] [-trunc <value>]\n");
    if (help) {
     fprintf(stderr, "	o Formats and prints the profiling data.\n");
     fprintf(stderr, "	o <input> designates where to read the profiling\n");
     fprintf(stderr, "	  data. (default is mprof.out) The alternate\n"); 
     fprintf(stderr, "	  \"-\" option indicates that standard input should\n");
     fprintf(stderr, "	  be used to read the profiling data.\n");
     fprintf(stderr, "	o <symbols> designates the file where to read the\n");
     fprintf(stderr, "	  symbols. (default is /mach_kernel)\n");
     fprintf(stderr, "	o <value> is used to truncate formated output for\n");
     fprintf(stderr, "	  time percentages smaller than <value> %%\n\n");
    }	
    fprintf(stderr, "mprof -help\n");
    if (help) {
     fprintf(stderr, "	o prints this\n");
    }	

}

usage() {
    help_msg(0);
    kill_parent();
    exit(1);
}

set_action(a)
{
	if (action != MPROF_NULL) {
		usage();
	} else {
		action = a;
	}
}

check_action(a)
{
	if (action == MPROF_NULL)
		action = MPROF_SELF;
	if (!(action & a))
		usage();
}

quit(code, s, args)
char *s;
{
	fprintf(stderr, s, args);
	usage();
}

/*
 * allocate space for targets
 */

target_alloc(size) {

 	targets = (struct target *) malloc(size * sizeof (struct target));
	if (targets == NULL)
		quit(1, "Malloc()\n");
}

/*
 * Get kernel task port 
 */

task_port_t
get_kernel_task()
{
	mach_port_t host;
	processor_set_name_port_t *proc_sets;
    	unsigned int proc_setsCnt;
   	task_port_t *tasks_list;
    	unsigned int tasksCnt;
    	processor_set_name_port_t *proc_set_names;
    	kern_return_t kr;
    	thread_port_t *threads;
    	unsigned int threadsCnt;


	/* get host privileged port */

	if ((host = mach_host_priv_self()) == MACH_PORT_NULL)
		quit(1, "Can't get host privileged port\n");

	/* get processor sets */

	kr = host_processor_sets(host, &proc_set_names, &proc_setsCnt);
	kr_check(1, "host_processor_sets(): ");

	/* convert processor set 0 name to send right privileged port */

	kr = host_processor_set_priv(host, proc_set_names[0], &proc_sets);
	kr_check(1, "host_processor_set_priv(): ");

	/* get tasks */

	kr = processor_set_tasks(proc_sets, &tasks_list, &tasksCnt);
	kr_check(1, "processor_set_tasks(): ");

	/* assume it is task 0 */

	return(tasks_list[0]);
}

int name_server_alive = 0;

void
waiting_name_server()
{
	if (!name_server_alive) {
		fprintf(stderr, "Can't contact name server\n");
		alarm(5);
	} else {
		struct sigaction sa;

		sa.sa_handler = SIG_DFL;
		sa.sa_flags = sa.sa_mask = 0;
		sigaction(SIGALRM, &sa, 0);
	}
}

get_mids()
{
	int i;
	mach_type_t type;
	mach_port_t server;
	mach_port_t auth;
    	kern_return_t kr;
	struct sigaction sa;

	target_alloc(ntargets ? ntargets : 1);
	if (ntargets == 0) {
		ntargets = 1;
		targets[0].port = get_kernel_task();
		targets[0].type = MACH_TYPE_TASK;
		return ntargets;
	}

	sa.sa_handler = waiting_name_server;
	sa.sa_flags = sa.sa_mask = 0;
	sigaction(SIGALRM, &sa, 0);

	alarm(5);
 	kr = netname_look_up(name_server_port, "", "MachID", &server);
	alarm(0);

	kr_check(1, "get_mids: netname_lookup_up(MachID) ");

	name_server_alive = 1;
	waiting_name_server();	/* resets handler */

  	auth = mach_host_priv_self();

	for (i=0; i < ntargets; i++) {
	        atod(mid_names[i], &targets[i].mid);
		kr = machid_mach_type(server, auth, targets[i].mid, &type);
		kr_check(1, "get_mids: machid_mach_type(): ");
		targets[i].type = type;
		switch(type) {
		case MACH_TYPE_TASK:
		case MACH_TYPE_THREAD:
			kr = machid_mach_port(server, auth, targets[i].mid, &targets[i].port);
			kr_check(1, "get_mids: machid_mach_port(): ");
			break;
		default:
			quit(1, "Bad mach ID %d\n", targets[i].mid);
		}
		if (debug)
			fprintf(stderr, "%mid %d: %s port %x\n",
				targets[i].mid,
				(targets[i].type == MACH_TYPE_TASK) ?
				"task" : "thread",
				targets[i].port);
	}
	return ntargets;
}

mach_port_t
prof_port_allocate() {
	mach_port_t port;
    	kern_return_t kr;

	kr = mach_port_allocate(mach_task_self(),
				 MACH_PORT_RIGHT_RECEIVE, &port);
	kr_check(1, "mach_port_allocate(): ");
	return(port);
}

profile(silent)
{
	register struct target *tg;
    	kern_return_t kr;
	mach_port_t	prof_port;
	register i;

	if (action == MPROF_ON) {

		sample_open("w");

		prof_port = prof_port_allocate();
		if (debug)
			fprintf(stderr, "prof_port %x\n", prof_port);
	}

	for(i=0, tg = targets; i < ntargets; i++, tg++) {
	  	if (debug) 
			fprintf(stderr, "target %d: port %x\n", i, tg->port);
		if (tg->type == MACH_TYPE_THREAD) {
			if (action == MPROF_ON)
				kr = thread_sample(tg->port, prof_port);
			else
				kr = thread_sample(tg->port, MACH_PORT_NULL);
			if (kr != KERN_SUCCESS && !silent)
				fprintf(stderr, "thread_sample %s\n", mach_error_string(kr));
		} else {
			if (action == MPROF_ON)
				kr = task_sample(tg->port, prof_port);
			else
				kr = task_sample(tg->port, MACH_PORT_NULL);
			if (kr != KERN_SUCCESS && !silent)
				fprintf(stderr, "task_sample %s\n", mach_error_string(kr));
		}
		if (kr != KERN_SUCCESS && !silent) {
			if (action == MPROF_OFF)
				fprintf(stderr, "Could not stop profiling on MID %d\n", tg->mid);
			else {
				fprintf(stderr, "Could not activate profiling on MID %d, ", tg->mid);
				action = MPROF_OFF;
				ntargets = i;
				profile(1);
				quit(1, "Mprof aborted\n");
			}
		}
		gettimeofday(&timeval, &tz);
		if (debug)
			fprintf(stderr, "timeofday %s (%d usec)\n", 
			       ctime(&timeval.tv_sec), timeval.tv_usec);
	}

	if (action == MPROF_ON) {
		kill_parent();
		catch_samples(prof_port);
	}
}

sample_open(mode)
char *mode;
{
	if (debug)
		fprintf(stderr, "sample_open\n");
	if (sample_file)
		return;
	user_id();
	sample_file = fopen(sample_file_name, mode);
	if (sample_file == NULL) {
		perror(sample_file_name);
		quit(1, "Could not open sample file\n");
	}
	priv_id();
}

sample_close()
{
	fclose(sample_file);
	sample_file = 0;
}

pc_cmp(p1, p2)
	struct symbol *p1, *p2;
{

	return(p1->pc - p2->pc);
}

get_symbols () {
	struct	exec exec;	/* exec header for an a.out file */
	unsigned strsz;		/* symbol table size */
	char	*strtab;	/* symbol table */
	unsigned nsyms;
	struct nlist *nlist;
	register i;

	user_id();
	if (debug)
		fprintf(stderr, "get_symbols\n");
	/* Open symbol file (Executable file */

	symbol_file = fopen(symbol_file_name,"r");
	if (symbol_file == NULL) {
		perror(symbol_file_name);
		quit(1, "Could not open symbol file\n");
	}

	if (!machine_get_symbols(symbol_file, symbol_file_name)) {
		quit(1, "get_symbols(%s): unknown file format\n", 
		     symbol_file_name);
	}

	qsort(symbols, nsymbols, sizeof(struct symbol), pc_cmp);
	priv_id();
}

sample_cmp(s1, s2)
unsigned *s1, *s2;
{

	return(*s1 - *s2);
}

read_samples()
{
	unsigned int *sample = 0;
	struct stat stat;
	register i;
	struct symbol *sym;

	if (debug)
		fprintf(stderr, "read_samples\n");

	sample_open("r");

	if (sample_file != stdin) {
		if (fstat(fileno(sample_file), &stat) != 0) {
			perror("fstat");
			quit(1, "Could not stat sample table");
		}
	
		if (fread(&start_time, sizeof (start_time),
			  1, sample_file) != 1) {
			perror("fread");
			quit(1, "Can not read start_time\n");
		}

		if (stat.st_size >= 2 * sizeof(struct timeval)) 
			nsamples = (stat.st_size -
				    2 * sizeof(struct timeval)) /
				      sizeof (*sample);
		else
			nsamples = 0;

		if (debug)
			fprintf(stderr, "%d samples\n", nsamples);

		if (nsamples) {
			samples = (unsigned int *) calloc(nsamples,
							  sizeof (*sample));

			if (samples == 0) {
				quit(1, stderr,
				     "prof: No room for %d sample pc's\n",
				     nsamples);
			}

			if (fread(samples, sizeof (*sample),
				  nsamples, sample_file) != nsamples) {
				perror("fread");
				quit(1, "Can not read samples\n");
			}
		}

		if (fread(&stop_time, sizeof (stop_time),
			  1, sample_file) != 1) {
			perror("fread");
			quit(1, "Can not read stop_time\n");
		}
	} else {
		int size = 0;
		unsigned int *prev_samples;
		int samples_per_page = vm_page_size/sizeof(*sample);

		if (fread(&start_time, sizeof (start_time),
			  1, sample_file) != 1) {
			perror("fread");
			quit(1, "Can not read start_time\n");
		}
		
		while (1) {
			int size_read;

			prev_samples = samples;
			samples = (unsigned int *)calloc(size+vm_page_size, 1);
			if (samples == 0) {
				quit(1,stderr,
				     "prof: No room for sample pc's\n");
			}

			if (prev_samples) {
				bcopy((char *)prev_samples,
				      (char *)samples,
				      size);
				free(prev_samples);
			}

			size_read = fread(((char *)samples)+size, 1,
					 vm_page_size, sample_file);
			if (size_read < 0) {
				perror("fread");
				fprintf(stderr, "Can not read samples\n");
			} else if (debug)
				fprintf(stderr, "read %d bytes of samples\n",
					size_read);

			size += size_read;
			if (size_read != vm_page_size) {
				nsamples = (size - sizeof(stop_time))/
				  		sizeof(*sample);
				bcopy((char *)(samples + nsamples),
				      (char *)&stop_time,
				      sizeof(stop_time));
				if (debug)
					fprintf(stderr, "%d samples\n",
						nsamples);
				break;
			}
		}
	}

	if (nsamples)
		qsort(samples, nsamples, sizeof(*sample), sample_cmp);

	sample = samples;
	sym = symbols;

	if (debug)
		fprintf(stderr, "assigning samples\n");

	for (i=0; i < nsamples; i++) {
		while(sym < &symbols[nsymbols] && sym->pc <= *sample)
			sym++;
		if (debug)
			fprintf(stderr, "sample %x: %s\n",
				*sample, (sym-1)->name);
		(sym-1)->count++;
		sample++;
	}
}

count_cmp(n1, n2)
struct symbol *n1, *n2;
{
	return(n1->count - n2->count);
}

print_samples()
{
	register i = nsymbols;
	struct symbol *sym = &symbols[i-1];
	float f, elapsed, cpu_time;
	int hz;
	float tot_perc;

	if (debug)
		fprintf(stderr, "print_samples\n");

	hz = hertz();

	elapsed = stop_time.tv_usec - start_time.tv_usec;
	elapsed /= 1000000;
	elapsed += (stop_time.tv_sec - start_time.tv_sec);

	cpu_time = 1;
	cpu_time /= hz;
	cpu_time *= nsamples;

	qsort(symbols, nsymbols, sizeof(struct symbol), count_cmp);

	printf("\nSymbols file:	%s\n", symbol_file_name);
	printf("\nTotal elapsed time (seconds): %10.3f", elapsed);
	printf("\nNumber of samples:            %10d", nsamples);
	printf("\nSampling rate (HZ):           %10d", hz);
	printf("\nEstimated cpu time (seconds): %10.3f", cpu_time);
	printf("\nCPU utilisation (%%):          %10.2f", (cpu_time/elapsed)*100);
	printf("\n\n     %%    seconds\n");
	tot_perc = 0;
	while(i-- && sym->count) {
		f = sym->count;
		f /= nsamples;
		if (f*100 < min_percent)
			break;
		tot_perc += f;
		printf("%6.2f %10.3f  %s\n", f*100, cpu_time * f, sym->name);
		sym--;
	}
	if (tot_perc < 1 && nsamples)
		printf("%6.2f %10.3f  (others)\n", (1-tot_perc)*100, cpu_time * (1-tot_perc));

	printf("------ ----------\n%6.2f %10.3f  total\n", (float)100, (float)cpu_time);
}




catch_samples(prof_port)
{
    	kern_return_t kr;
	extern boolean_t prof_server(), demux();
	mach_port_t	notify_port, port_set, previous_port;

	if (debug)
		fprintf(stderr, "catch_samples\n");

	if (setjmp(saved_state))
		return;
	kr = mach_port_allocate(mach_task_self(),
				 MACH_PORT_RIGHT_RECEIVE, &notify_port);
	kr_check(1, "mach_port_allocate(): ");

	kr = mach_port_request_notification(mach_task_self(), prof_port,
					    MACH_NOTIFY_NO_SENDERS, 0,
					    prof_port,
					    MACH_MSG_TYPE_MAKE_SEND_ONCE,
					    &previous_port); 
	kr_check(1, "mach_port_request_notification(): ");

	user_id();
	if (fwrite(&timeval, sizeof(timeval), 1, sample_file) != 1) {
		perror(sample_file_name);
		quit(1, "Could not write sample file\n");
	}
	priv_id();

	kr = mach_msg_server(demux, 2048, prof_port, MACH_MSG_OPTION_NONE);
	if (kr == MACH_RCV_INTERRUPTED) {
		/* Can be interrupted by child deatch */
		kr = mach_msg_server(demux, 2048, prof_port,
				     MACH_MSG_OPTION_NONE);
	}
	kr_check(1, "catch_samples:  mach_msg_server(): ");
	no_more_samples();
}

boolean_t
demux(in, out)
mach_msg_header_t *in, *out;
{
	if (prof_server(in, out)) {
		return(TRUE);
	} else {
	  	if (debug)
			fprintf(stderr, "demux: prof_server() failed\n");
		if (!notify_server(in, out))
			return(FALSE);
		no_more_samples();
		longjmp(saved_state, 1);
	}
}


receive_samples(reply_port, samples, count)
mach_port_t reply_port;
unsigned *samples;
unsigned count;
{
	kern_return_t kr;

	user_id();
	if (debug)
		fprintf(stderr, "received %d samples @ %x\n", count, samples);
	if (fwrite(samples, sizeof (unsigned), count, sample_file) != count) {
		perror(sample_file_name);
		quit(1, "Could not write sample file\n");
	}
	priv_id();
}

receive_notices(reply_port, samples, count, opts)
mach_port_t reply_port;
unsigned *samples;
unsigned count;
int opts;
{
	quit(1, "receive_notices called!\n");
}

no_more_samples()
{
	if (debug)
		fprintf(stderr, "no more samples\n");
	gettimeofday(&timeval, &tz);
	if (debug)
		fprintf(stderr, "timeofday %s (%d usec)\n", 
		       ctime(&timeval.tv_sec), timeval.tv_usec);
	user_id();
	if (fwrite(&timeval, sizeof(timeval), 1, sample_file) != 1) {
		perror(sample_file_name);
		quit(1, "Could not write sample file\n");
	}
	sample_close();
	priv_id();
}

kill_parent()
{
	if (parent && getpid() != parent)
		kill(parent, SIGHUP);
	parent = 0;
}

int child;
mach_port_t child_port;

void
child_death()
{
	int status;
	wait(&status);
	if (ntargets != 1 || targets[0].port != child_port) {
		action = MPROF_OFF;
		profile(0);
	}
	if (debug)
		fprintf(stderr, "command done status %x\n", status);
}

profile_self(args)
char *args[];
{	
	kern_return_t kr;
	struct sigaction sa;
	
	if (debug) 
		fprintf(stderr, "command %s\n", args[0]);

	if (!symbol_file_option) {
		symbol_file_name = args[0];
	}

	sa.sa_handler = child_death;
	sa.sa_flags = sa.sa_mask = 0;
	sigaction(SIGCHLD, &sa, 0);

	if (!(child = fork())) {
#ifdef	linux
		{
			extern int (*_mach_init_routine)(void);
			ioperm(0,0,1);
			if (_mach_init_routine) {
				(*_mach_init_routine)();
			}
		}
#endif
		kr = task_suspend(mach_task_self());
		if (setuid(getuid()) < 0) {
			perror("profile_self()");
			quit(1, "could not setuid to %d\n", getuid());
		}
		execvp(args[0], &args[0]);
	      	perror("execvp");
		fprintf(stderr, "could not execute %s\n", args[0]);
		exit(1);
	}

	child_port = task_by_pid(child); 

	if (debug)
	  	fprintf(stderr, "child_port %x\n", child_port);

	if (!ntargets) {
		target_alloc(1);
		targets[0].port = child_port;
		targets[0].type = MACH_TYPE_TASK;
		ntargets = 1;
	} else
		get_mids();

	action = MPROF_ON;

	while(task_resume(child_port) != KERN_SUCCESS)
		thread_switch(MACH_PORT_NULL, SWITCH_OPTION_NONE, 0);

	profile(0);

	if (symbol_file_option) {
		get_symbols();
		read_samples();
		print_samples();
	}
}

int saved_id;

user_id()
{
	saved_id = geteuid();
	
	if(setreuid(getuid(), getuid()) < 0) {
		perror("user_id()");
		quit(1, "could not setreuid to %d\n", getuid());
	}
}

priv_id()
{
	if(setuid(saved_id) < 0) {
		perror("priv_id()");
		quit(1, "could not setuid to %d\n", saved_id);
	}
}

do_mach_notify_port_deleted(server_port, name)
{
	if (debug)
		fprintf(stderr,
			"mach_notify_port_deleted(), name = %x\n",
			name);
	return(KERN_SUCCESS);
}

do_mach_notify_port_destroyed(server_port, rcv_right)
{
	if (debug)
		fprintf(stderr,
			"mach_notify_port_destroyed(), rcv_right = %x\n",
			rcv_right);
	return(KERN_SUCCESS);
}

do_mach_notify_no_senders(server_port, count)
{
	if (debug)
		fprintf(stderr,
			"mach_notify_no_senders(), count = %x\n",
			count);
	return(KERN_SUCCESS);
}

do_mach_notify_send_once(server_port)
{
	if (debug)
		fprintf(stderr, "mach_notify_send_once()\n");
	return(KERN_SUCCESS);
}

do_mach_notify_dead_name(server_port, name)
{
	if (debug)
		fprintf(stderr,
			"mach_notify_dead_name(), name = %x\n", name);
	return(KERN_SUCCESS);
}

atod(s, i)
char *s;
int *i;
{
	char c;
	int res;

	res = 0;
	while(c = *s++) {
		if (c >= '0' && c <= '9')
			res = (res)*10 + c - '0';
		else
			return(0);
	}
	if (i)
		*i = res;
	return(1);			
}
