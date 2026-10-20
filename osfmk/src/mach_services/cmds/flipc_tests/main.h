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
 * Upon entry to this file (at compile time) the macro INSTANCES
 * is expected to be defined.  If it is not, the argument -I will be
 * required to specify the number of instances.  If it is, -I will not
 * be allowed as an argument.  In either case the global variable
 * total_instances will be initialized with the number of instances
 * expected.
 */
#ifdef INSTANCES
int total_instances = INSTANCES;
#else
int total_instances = -1;
#endif

/* Global variable to indicate which instance we are in, for
   debugging purposes.  */
int this_instance = -1;

/*
 * On each node, the function usermain will be
 * called.  It is called with the filename and instant number
 * as arguments.
 */
void usermain(char *filename, int instance);

/*
 * (Not yet implemented).  In addition, the array otherargs may be
 * initialized.  It consists of a list of
 * (arg character, arg type, address of var to set, argname) for args you
 * may want to get from the user for this test in addition to -i -f and
 * -v.
 */
struct {
    char argletter;
    enum { ARG_NONE, ARG_INT, ARG_STRING } argtype;
    void *variable;
    char *argname;
} otherargs;

/*
 * Todo:
 * Ways to print out arbitrary stuff at an arbitrary verbosity level,
 * on the same fd we use here.
 */

#include <stdio.h>
#include <flipc.h>
#include <stdlib.h>
#include <string.h>
#include <cthreads.h>
#include <fcntl.h>
#include <sys/mode.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <cthreads.h>
#include <mach/mach_traps.h>
#include <mach/sync_policy.h>

/*
 * Main debug macro with control and result variables.
 */

int test_verbose = 0;
int test_failure = 0;

/*
 * Printf locking.
 */
struct mutex printf_mutex;

void crash();

/*
 * Check for a single expected result.  Result is the variable to put
 * the result in.
 */
#define ftest1(expr, result, exp_result, abrt_on_failure)	\
do {								\
    result = (expr);						\
								\
    if (result != exp_result) {					\
	mutex_lock(&printf_mutex);				\
	printf("**%s:%d:I%d: %s != (%d <== %s)\n",		\
	       __FILE__, __LINE__, this_instance,		\
               #exp_result, (int)result, #expr);		\
	mutex_unlock(&printf_mutex);				\
	test_failure++;						\
    } else if (test_verbose) {					\
	mutex_lock(&printf_mutex);				\
	printf("%s:%d:I%d: %s <== %s\n",			\
	       __FILE__, __LINE__, this_instance,		\
               #exp_result, #expr);				\
	mutex_unlock(&printf_mutex);				\
    }								\
								\
    if (result != exp_result					\
	&& abrt_on_failure)					\
	crash();						\
} while (0)

/*
 * Check for one of two possible results.  Put the actual value in
 * the variable result.
 */
#define ftest2(expr, result, exp_result1, exp_result2, abrt_on_failure)\
do {								       \
    result = (expr);						       \
								       \
    if (result != exp_result1					       \
	&& result != exp_result2) {				       \
	mutex_lock(&printf_mutex);				       \
	printf("**%s:%d:I%d: {%s,%s} != (%d <== %s)\n",		       \
	       __FILE__, __LINE__, this_instance,		       \
               #exp_result1, #exp_result2,			       \
	       (int)result, #expr);				       \
	mutex_unlock(&printf_mutex);				       \
	test_failure++;						       \
	if (abrt_on_failure)					       \
	    crash();						       \
    } else if (test_verbose) {					       \
	mutex_lock(&printf_mutex);				       \
	printf("%s:%d:I%d: %s <== %s\n",			       \
	       __FILE__, __LINE__, this_instance,		       \
	       (result == exp_result1				       \
		? #exp_result1					       \
		: #exp_result2), #expr);			       \
	mutex_unlock(&printf_mutex);				       \
    }								       \
} while (0)

/*
 * Test for a result between two sets of bounds (>= lower, & < upper).
 */
#define ftestb(expr, result, bottom, top, abrt_on_failure)	\
do {								\
    (result) = (expr);						\
								\
    if ((result) < (bottom)) {					\
	mutex_lock(&printf_mutex);				\
	printf("**%s:%d:I%d: %s > (%d <== %s)\n",		\
	       __FILE__, __LINE__, this_instance,		\
	       #bottom, (int)result, #expr);			\
	mutex_unlock(&printf_mutex);				\
	test_failure++;						\
	if (abrt_on_failure)					\
	    crash();						\
    } else if ((result) >= (top)) {				\
	mutex_lock(&printf_mutex);				\
	printf("**%s:%d:I%d: %s <= (%d <== %s)\n",		\
	       __FILE__, __LINE__, this_instance,		\
	       #top, (int)result, #expr);			\
	mutex_unlock(&printf_mutex);				\
	test_failure++;						\
	if (abrt_on_failure)					\
	    crash();						\
    } else if (test_verbose) {					\
	mutex_lock(&printf_mutex);				\
	printf("%s:%d:I%d: [%s,%s) -} (%d <== %s)\n",		\
	       __FILE__, __LINE__, this_instance,		\
	       #bottom, #top, result, #expr);			\
	mutex_unlock(&printf_mutex);				\
    }								\
} while (0)

/*
 * Check for a result *other* than unexp_result.  Put the actual value in
 * the variable result.
 */
#define ftestnot(expr, result, unexp_result, abrt_on_failure)	       \
do {								       \
    result = (expr);						       \
								       \
    if (result == unexp_result) {				       \
	mutex_lock(&printf_mutex);				       \
	printf("**%s:%d:I%d: %s <== %s)\n",			       \
	       __FILE__, __LINE__, this_instance,		       \
               #unexp_result, #expr);				       \
	mutex_unlock(&printf_mutex);				       \
	test_failure++;						       \
	if (abrt_on_failure)					       \
	    crash();						       \
    } else if (test_verbose) {					       \
	mutex_lock(&printf_mutex);				       \
	printf("%s:%d:I%d: %s != (%d <== %s)\n",		       \
	       __FILE__, __LINE__, this_instance,		       \
               #unexp_result, (int)result, #expr);		       \
	mutex_unlock(&printf_mutex);				       \
    }								       \
} while (0)

/*
 * Fills buffer (at least up to size) with a unique, human readable
 * string identifying file, line number from which it came, along
 * with an arg to disambiguate indexes in arrays.
 *
 * If checkvar is set, it fills checkvar (a character array of size SIZE)
 * with enough information to reconstitute the string used above, if
 * given the instance that produced it.
 */

#define ffillbuf(buffer, size, instance, arg, checkvar)		       \
do {								       \
    int bufsize, bytes_written;					       \
    char *__buffer = (buffer);					       \
								       \
    mutex_lock(&printf_mutex);					       \
    sprintf((char*)__buffer, "%d(l):%d(i):%d(a):%s(f)",		       \
	    (argc), __LINE__, (instance), __FILE__);		       \
    mutex_unlock(&printf_mutex);				       \
    /* Talk about inelegant.  */				       \
    bufsize = strlen((char*)__buffer);				       \
    /* "+4" because we need 1 extra byte for the null at	       \
       the end, and we may need some others for the extra	       \
       stuff below.  */						       \
    if (bufsize+4 >= (size)) {					       \
	mutex_lock(&printf_mutex);				       \
	fprintf(stderr, "ffillbuff overran buffer!\n");		       \
	mutex_unlock(&printf_mutex);				       \
	crash();						       \
    }								       \
								       \
    /* Put it into the checkvar variable.  */			       \
    if (checkvar) {						       \
	mutex_lock(&printf_mutex);				       \
	sprintf((char*)__buffer,"%d(l):%%d(i):%d(a):%s(f)%%n",	       \
		(arg), __LINE__, __FILE__);			       \
	mutex_unlock(&printf_mutex);				       \
    }								       \
								       \
    /* Copy it multiple times in the buffer.  */		       \
    bytes_written = bufsize+1;					       \
    for (bytes_written=bufsize+1;				       \
	 bytes_written+bufsize+1 < (size);			       \
	 bytes_written+=bufsize+1)				       \
	bcopy(__buffer, __buffer + bytes_written, bufsize+1);	       \
    bcopy(__buffer, __buffer+bytes_written,			       \
	  (size)-bytes_written);				       \
} while (0)

#define fcheckbuf(buffer, size, checkvar, orig_instance, abrt_on_failure)\
do {									 \
    int read_instance;							 \
    int result;								 \
    int chars_read = 0;							 \
    int local_test_failure = 0;						 \
    int later_buffer_corruption = 0;					 \
    int bytes_checked, compare_to;					 \
    int bufsize;							 \
									 \
    mutex_lock(&printf_mutex);						 \
    (void) sscanf((char*)buffer, checkvar, &read_instance,		 \
		  &chars_read);						 \
    mutex_unlock(&printf_mutex);					 \
    bufsize = strlen((char*)buffer);					 \
    if (chars_read != strlen((char*)buffer))				 \
	local_test_failure++;						 \
    if (read_instance != (orig_instance))				 \
	local_test_failure++;						 \
									 \
    /* Check for corruption in the rest of the buffer.  */		 \
    for (bytes_checked = bufsize+1, compare_to = 0;			 \
	 bytes_checked < (size);					 \
	 bytes_checked++, compare_to++)					 \
	if (((char*)buffer)[bytes_checked] !=				 \
	    ((char*)buffer)[compare_to]) {				 \
	    later_buffer_corruption = bytes_checked;			 \
	    break;							 \
	}								 \
									 \
    /* We know what we have; what to do with it?  */			 \
    if (local_test_failure || later_buffer_corruption) {		 \
	test_failure++;							 \
	if (local_test_failure) {					 \
	    mutex_lock(&printf_mutex);					 \
	    fprintf(stderr, "**%s:%d: \"%s\" != \"",			 \
		    __FILE__, __LINE__, (char*)buffer);			 \
	    fprintf(stderr, checkvar, orig_instance);			 \
	    fprintf(stderr, "\"");					 \
	    mutex_unlock(&printf_mutex);				 \
	} else {	/* later buffer corruption.  */			 \
	    mutex_lock(&printf_mutex);					 \
	    fprintf(stderr,						 \
		    "**%s:%d: Buffer corruption starting at char %d\n",	 \
		    __FILE__, __LINE__, later_buffer_corruption);	 \
	    mutex_unlock(&printf_mutex);				 \
	}								 \
	if (abrt_on_failure)						 \
	    crash();							 \
    } else if (test_verbose) {						 \
	mutex_lock(&printf_mutex);					 \
	fprintf(stderr, "%s:%d: %s == \"",				 \
		__FILE__, __LINE__, #buffer);				 \
	fprintf(stderr, checkvar, orig_instance);			 \
	fprintf(stderr, "\"");						 \
	mutex_unlock(&printf_mutex);					 \
    }									 \
} while (0)

/* If INSTANCES is 1, you are allowed to default the -I parameter.
   Not otherwise.  */
void usage(char *name)
{
#ifdef INSTANCES
#if INSTANCES > 1
    fprintf(stderr, "%s [-v] [-h] -i <0:%d> [-f <filename-prefix>]\n",
	    name, INSTANCES-1);
#else
    fprintf(stderr, "%s [-v] [-h] [-i 0] [-f <filename-prefix>]\n",
	    name);
#endif	/* INSTANCES > 1 */
#else	/* INSTANCES */
    fprintf(stderr, "%s [-vh] -I <instances> -i <inst> [-f <filename-prefix>]\n",
	    name);
#endif

    exit(1);
}

void crash()
{
    exit(5);
}

char *progname = (char *) 0;

void main(int argc, char **argv)
{
#if INSTANCES > 1
    int instance = -1;
#else
    int instance = 0;
#endif
    char *filename = (char *) 0;

    progname = argv[0];

    while (++argv, --argc) {
	if (**argv != '-')
	    usage(progname);
	switch(argv[0][1]) {
	  case 'i':
	    instance = atoi(argv[1]);
	    argv++; argc--;
	    break;
	  case 'f':
	    filename = argv[1];
	    argv++; argc--;
	    break;
	  case 'h':
	    task_suspend(mach_task_self());
	    break;
	  case 'v':
	    test_verbose = 1;
	    break;
	  case 'I':
#ifndef INSTANCES
	    total_instances = atoi(argv[1]);
	    argv++; argc--;
	    break;
#endif
	    /* Falls through if INSTANCES isn't defined.  */
	  default:
	    usage(progname);
	    /* NOTREACHED.  */
	}
    }

    /* Default filename.  */
    if (!filename) {
	filename = (char *) malloc(strlen(progname) + 6);
	strcpy(filename, progname);
	strcat(filename, ".test");
    }

    if (instance >= total_instances
	|| instance < 0)
	usage(progname);

    /* Initialize global instance variable.  */
    this_instance = instance;

    /* Initialize locks.  */
    mutex_init(&printf_mutex);

    /* Initialize random number generator.  */
    srandom(instance);

    usermain(filename, instance);

    if (test_failure) {
	printf("Test FAILED.\n");
	exit(1);
    } else {
	printf("Test PASSED.\n");
	exit(0);
    }
}

/*
 * Find out what single integer is in the file <fileprefix>.instance.
 * If the filename hasn't yet been created, wait until it is.
 * Return the integer.
 */
int read_instance_integer(char *fileprefix, int instance)
{
    char *filename = malloc(strlen(fileprefix) + 5);
    int fd;
    int result;
    int len;
    int i;

    if (!filename) {
	fprintf(stderr, "Couldn't allocate memory.\n");
	exit(1);
    }

    /* What is the file?  */
    sprintf(filename, "%s.%d", fileprefix, instance);

    /* Open it.  */
    while ((fd = open(filename, O_RDONLY)) < 0
	   && errno == ENOENT) {
	/* Put in a little random delay to get around a filesystem bug.  */
	for (i = 0; i < (((double) random())/RAND_MAX)*1000000; i++)
	    ;
    }

    if (fd < 0) {
	perror("read_instance_integer: error on open");
	exit(1);
    }

    /* Get the value.  */
    if ((len = read(fd, &result, sizeof(result))) != sizeof(result)) {
	if (len >= 0)
	    fprintf(stderr, "Read did not get all data; only %d bytes were read\n",
		    len);
	else
	    perror("read_instance_integer: error on read");
	exit(1);
    }

    /* Close the file.  */
    close(fd);

    /* Free the memory.  */
    free(filename);

    /* Return the value.  */
    return result;
}

/*
 * Write the integer to the file <fileprefix>.<instance>.  It is an
 * error if this file exists.
 */
void write_instance_integer(char *fileprefix, int instance, int arg)
{
    char *filename = malloc(strlen(fileprefix) + 9);
    char *final_filename = malloc(strlen(fileprefix) + 5);
    int fd;
    int len;

    if (!filename) {
	fprintf(stderr, "Couldn't allocate memory.\n");
	exit(1);
    }

    /* What is the file?  The appearence has to be atomic, so we
       open a tmp file and rename it after closing.  */
    sprintf(filename, "%s.%d.tmp", fileprefix, instance);
    sprintf(final_filename, "%s.%d", fileprefix, instance);

    /* Open it.  */
    if ((fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0600)) < 0) {
	perror("write_instance_integer: error on open");
	exit(1);
    }

    /* Write out the integer.  */
    if ((len = write(fd, &arg, sizeof(arg))) != sizeof(arg)) {
	if (len >= 0)
	    fprintf(stderr, "Couldn't write full argument; only wrote %d bytes\n",
		    len);
	else
	    perror("write_instance_integer: error on write");
	exit(1);
    }

    /* Close the file.  */
    close(fd);

    /* Rename the file.  */
    rename(filename, final_filename);

    /* Free the memory.  */
    free(filename);
    free(final_filename);

    /* We done.  */
}

void delete_instance_file(char *fileprefix, int instance)
{
    char *filename = malloc(strlen(fileprefix) + 5);

    if (!filename) {
	fprintf(stderr, "Couldn't allocate memory.\n");
	exit(1);
    }

    /* What is the file?  */
    sprintf(filename, "%s.%d", fileprefix, instance);

    /* Kill it.  */
    unlink(filename);

    /* Free the memory.  */
    free(filename);

    return;
}

/*
 * This function daisy chains the bootstrap integers among all
 * the various instances of the test.  Specifically, each instance
 * calls this function with an integer, and the function
 * returns the integer that instace i-1 (mod total_instances)
 * was called with.
 */
int bootstrap_instance_integers(char *fileprefix, int instance, int arg)
{
    int result;
    int read_instance = instance == 0 ? total_instances - 1 : instance - 1;

    write_instance_integer(fileprefix, instance, arg);

    /* Read it, and since I'm the only one reading it, delete it.  */
    result = read_instance_integer(fileprefix, read_instance);
    delete_instance_file(fileprefix, read_instance);

    return result;
}

/*
 * Do common initialization if that's what is wanted.
 * This stuff will be masked by the function local variables if
 * we want to do it ourselves (as it is done in the older tests).
 */

FLIPC_domain_t domain;
FLIPC_endpoint_t control_receive;
FLIPC_address_t remote_control;

void common_init(char *fileprefix, int instance,
		 int endpoints, int epgroups, int buffers, int bufperep)
{
    FLIPC_return_t fr;
    FLIPC_address_t local_control;
    struct FLIPC_domain_info domain_info;

    domain_info.max_endpoints = endpoints;
    domain_info.max_epgroups = epgroups;
    domain_info.max_buffers = buffers;
    domain_info.max_buffers_per_endpoint = bufperep;
    domain_info.yield_fn = &cthread_yield;
    domain_info.policy = SYNC_POLICY_FIFO;
    domain_info.msg_buffer_size = 0;
    domain_info.error_log_size = 0;
    bzero((void*)&domain_info.performance, sizeof(domain_info.performance));

    /* Either initialized the domain or attach to it, as you will.  */
    ftest2(FLIPC_domain_init(0, &domain_info, &domain),
	   fr, FLIPC_DOMAIN_INITIALIZED, FLIPC_SUCCESS, 1);
    if (fr == FLIPC_DOMAIN_INITIALIZED) {
	printf("Init failed; attaching. \n");
	bzero(&domain_info, sizeof(domain_info));
	domain_info.yield_fn = &cthread_yield;
	ftest1(FLIPC_domain_attach(0, &domain_info, &domain),
	       fr, FLIPC_SUCCESS, 1);
    }

    /* Allocate the control endpoint and bootstrap the information 'round the
       ring.  */
    ftest1(FLIPC_endpoint_allocate(domain, 5, FLIPC_Receive,
				   0, &control_receive),
	   fr, FLIPC_SUCCESS, 1);
    ftest1(FLIPC_endpoint_address(control_receive, &local_control),
	   fr, FLIPC_SUCCESS, 1);
    remote_control =
	bootstrap_instance_integers(fileprefix, instance, local_control);
}

