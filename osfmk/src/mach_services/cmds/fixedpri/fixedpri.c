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
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include <mach.h>
#include <mach/policy.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <sys/resource.h>

extern int optind;
extern char *optarg;

char *progname;
int verbose = FALSE;
pid_t pid = 0;
int niceval = 0;	/* Adjustment to base user priority */

typedef mach_port_t	thread_t;
typedef mach_port_t	task_t;
typedef mach_port_t	host_priv_t;
typedef mach_port_t	processor_set_name_t;
typedef mach_port_t	processor_set_t;

extern int parse_options(int argc, char **argv);
extern void usage(char *);
extern void set_fixedpri(thread_t);
extern task_t get_task(pid_t);

#define	VERBOSE(S,A)	if (verbose) {					\
				fprintf(stderr, "%s: ", progname);	\
				fprintf(stderr, S, A);			\
			}

#define	FAIL(S,E)	{						\
				fprintf(stderr, "%s FAILED: ", progname);\
				fprintf(stderr, S, E);			\
				exit(1);				\
			}

#define	MACH_FAIL(S,E)	{						\
				fprintf(stderr, "%s FAILED: %s: %s\n",	\
					progname, S, 			\
					mach_error_string(E));		\
				exit(1);				\
			}

#define	UNIX_FAIL(S,E)	{						\
				fprintf(stderr, "%s FAILED: %s: %s\n",	\
					progname, S, 			\
					strerror(E));			\
				exit(1);				\
			}

int
main(int argc, char **argv, char **envp)
{
	char *filename;
	task_t task = mach_task_self();

	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];

	if (parse_options(argc, argv)) {
		usage(progname);
		return(1);
	}

	/* Only root can run change the priority of other processes or run
	 * processes with better priority than normal user tasks.  We
	 * now attempt to set the UNIX priority using our real-id, and
	 * only if the OSF/1 server allows it, do we permit the changing
	 * of the Mach priority.  This is also done to keep the UNIX
	 * priority (displayed by ps) in synch with the Mach priority:
	 */
	seteuid(getuid());
        if (setpriority(PRIO_PROCESS, pid, niceval) != 0) {
		perror("fixedpri");
		return(2);
	}
	seteuid(0);

	if (pid) task = get_task(pid);

	set_fixedpri(task);

	if (pid) return(0);

	if (setuid(getuid()) < 0)	/* run as real uid */
		UNIX_FAIL("setuid", errno);

	argv += optind;
	filename = argv[0];

	VERBOSE("Executing %s ...\n", argv[0]);
	if (execvp(filename, argv) < 0)
		UNIX_FAIL("execvp", errno);

	return(1);
}

int
parse_options(int argc, char **argv)
{
	int c, errorflag = 0;

	while ((c = getopt(argc, argv, "vp:n:")) != EOF) {
		switch (c) {
		case 'v': verbose++; break;
		case 'p': pid = atoi(optarg); break;
		case 'n': niceval = atoi(optarg); break;
		default: errorflag++; break;
		}
	}

	return(errorflag);
}

void
usage(char *s)
{
	fprintf(stderr, "Usage: %s [-v] [ -n nice ] [-p pid | command]\n", s);
}

void
set_fixedpri(task_t task)
{
	kern_return_t rval;
	host_priority_info_data_t host_pri_info;
	host_priv_t host_priv;
        processor_set_name_t pset_default;
        processor_set_t pset_default_priv;
	struct policy_fifo_base base;
	struct policy_fifo_limit limit;
	unsigned count;

	if ((rval = processor_set_default(mach_host_self(), &pset_default))
			!= KERN_SUCCESS)
		MACH_FAIL("processor_set_default", rval);

	if ((host_priv = mach_host_priv_self()) == MACH_PORT_NULL)
		FAIL("Could not acquire privileged host port\n",0);

	if ((rval = host_processor_set_priv(host_priv, pset_default,
			&pset_default_priv)) != KERN_SUCCESS)
		MACH_FAIL("host_processor_set_priv", rval);

	count = HOST_PRIORITY_INFO_COUNT;
	if ((rval = host_info(mach_host_self(), HOST_PRIORITY_INFO,
		(host_info_t)&host_pri_info, &count)) != KERN_SUCCESS)
		FAIL("host_priority_info", rval);

	base.base_priority = host_pri_info.user_priority + niceval;
	limit.max_priority = host_pri_info.user_priority + niceval;
	if ((rval = task_set_policy(task,
				pset_default_priv,
				POLICY_FIFO,
				(policy_base_t)&base,
				POLICY_FIFO_BASE_COUNT,
				(policy_limit_t)&limit,
				POLICY_FIFO_LIMIT_COUNT,
				TRUE)) != KERN_SUCCESS)
		MACH_FAIL("task_set_policy", rval);

	return;
}

task_t
get_task(pid_t pid)
{
	kern_return_t rval;
	task_t task;

	if ((rval = task_by_unix_pid(mach_task_self(), pid, &task))
			!= KERN_SUCCESS)
		MACH_FAIL("task_by_unix_pid", rval);

	return(task);
}

kern_return_t
task_by_unix_pid(mach_port_t self, pid_t pid, mach_port_t *task)
{
        if ((*task = syscall(-33, pid)) == (mach_port_t)-1)
                *task = MACH_PORT_NULL;
	return(KERN_SUCCESS);
}
