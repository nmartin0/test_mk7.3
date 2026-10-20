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
#define TIMING
#include <time_trace.h>

#include <stdlib.h>
#include <stdio.h>
#include <mach.h>
#include <servers/netname.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <mach/norma_special_ports.h>
#include <sys/time.h>
#include <device/device.h>
#ifdef	PARAGON
#include "/afs/ri/project/kernel/build/dipc2_shared/src/mach_kernel/i860/PARAGON/rpm.h"
#else
#include <mach/time_value.h>
#endif	/* PARAGON */

#ifdef	PARAGON
void paragon_timer_init(void);
volatile unsigned int	*paragon_clock;
#endif	/* PARAGON */

/* Real timer log declarations.  */
struct timer_log_entry timer_log[TIMER_LOG_SIZE];

int timer_log_index;

#ifdef PARAGON
/*
 *	Paragon timing package.
 */

void
paragon_timer_init(void)
{
	struct rpm	*rpm;
	int i;

	rpm = (struct rpm *) RPM_BASE_VADDR;
	paragon_clock = (volatile unsigned int *) &rpm->rpm_time;

	/* Set up the log.  */
	timer_log_index = 0;
	for (i = 0; i < TIMER_LOG_SIZE; i++) {
	    timer_log[i].t.sample[0] = timer_log[i].t.sample[1] = 0;
	    timer_log[i].annotation = (char *) 0;
	}
}

void
paragon_timer_convert(timer_sample_t source, double *dest)
{
    *dest = source->sample[0] / 10000000.0 + source->sample[1] * 429.49672960;
}

void timer_dump_log_naked(FILE *stream)
{
    int i;
    /* Start just above the log index and circle round, skipping anything
       without an annotation.  */
    i = timer_log_index;
    do {
	if (timer_log[i].annotation)
	    fprintf(stream, "0x%08x 0x%08x %04d %s\n",
		    timer_log[i].t.sample[1], timer_log[i].t.sample[0],
		    i, timer_log[i].annotation);
	i++;
	if (i == TIMER_LOG_SIZE) i = 0;
    } while (i != timer_log_index);
}

extern struct timer_log_entry *timer_read_log_naked(FILE *stream)
{
    int local_log_size = TIMER_LOG_SIZE;
    struct timer_log_entry *local_log =
	(struct timer_log_entry *)
	    malloc(local_log_size * sizeof(struct timer_log_entry));
    int local_log_index = 0;
    char input_line[160];
    char *ptr;

    while (fgets(input_line, 160, stream) != NULL) {
	local_log[local_log_index].t.sample[1] = strtol(input_line, &ptr, 16);
	ptr++;
	local_log[local_log_index].t.sample[0] = strtol(ptr, &ptr, 16);
	ptr++;
	strtol(ptr, &ptr, 10);
	ptr++;
	local_log[local_log_index].annotation = (char *) malloc(strlen(ptr)+1);
	strcpy(local_log[local_log_index].annotation, ptr);
	local_log[local_log_index].annotation[strlen(ptr)-1] = '\0';
	local_log_index++;
	if (local_log_index == local_log_size) {
	    local_log_size *= 2;
	    local_log = (struct timer_log_entry *)
		realloc(local_log,
			local_log_size * sizeof(struct timer_log_entry));
	}
    }
    local_log[local_log_index].annotation = (char *) 0;
    return local_log;
}

int compare_logents(struct timer_log_entry *e1, struct timer_log_entry *e2)
{
    if (e1->t.sample[1] > e2->t.sample[1])
	return 1;
    else if (e1->t.sample[1] < e2->t.sample[1])
	return -1;
    else if (e1->t.sample[0] > e2->t.sample[0])
	return 1;
    else if (e1->t.sample[0] < e2->t.sample[0])
	return -1;
    else return 0;
}
 
extern void timer_dump_log_clothed(FILE *stream, struct timer_log_entry *log)
{
    int log_size, i;

    for (log_size = 0; log[log_size].annotation != (char *) 0; log_size++)
	;

    /* I promised to sort it.  Silly me.  */
    qsort(log, log_size, sizeof(struct timer_log_entry), &compare_logents);

    fprintf(stream, "Delta from previous\tAnnotation\n");
    fprintf(stream, "-------------------\t----------\n");
    fprintf(stream, "                   \t%s\n",
	    log[0].annotation);
    for (i = 1; i < log_size; i++) {
	timer_sample_data diff;
	double tmp;

	TIMER_SUBTRACT(&log[i].t, &log[i-1].t, &diff);
	TIMER_CONVERT(&diff, &tmp);
	fprintf(stream, "%10.10lf\t\t%s\n", tmp, log[i].annotation);
    }
}

#if	0
#define	TIME_DEVICE	"rpm0"
#define	TIME_MAPPABLE
struct rpm		*rpm;
#if	0
volatile double		*paragon_clock;
#endif
boolean_t
paragon_timer_init(void)
{
	device_t	time_device;
	mach_port_t	bootstrap_port, device_server_port;
	mach_port_t	host_priv_port;
	mach_port_t	time_pager;
	vm_offset_t	time_page;
	rpm_counter_t	rpm_time1, rpm_time2;
	int		ticks;
	kern_return_t	kr;
	double		d;
	int		i1, i2;

	kr = bootstrap_privileged_ports(mach_task_self(),
					&host_priv_port,
					&device_server_port);
	if (kr != KERN_SUCCESS) {
		printf("bootstrap_privileged_ports:  fails 0x%x (%d)\n",
		       kr, kr);
		return FALSE;
	}

	kr = device_open(device_server_port, D_READ|D_WRITE, "rpm0",
			 &time_device);
	if (kr != KERN_SUCCESS) {
		printf("device_open fails %d\n", kr);
		return FALSE;
	}

	kr = device_map(time_device, VM_PROT_READ, 0, vm_page_size,
			&time_pager, FALSE);
	if (kr != KERN_SUCCESS) {
		printf("device_map fails %d\n", kr);
		return FALSE;
	}

	time_page = 0;
	kr = vm_map(mach_task_self(), &time_page, vm_page_size, 0, TRUE,
		    time_pager, 0, FALSE, VM_PROT_READ, VM_PROT_READ,
		    VM_INHERIT_DEFAULT);
	if (kr != KERN_SUCCESS) {
		printf("vm_map fails %d\n", kr);
		return FALSE;
	}
	
#if	0
	paragon_clock = (volatile double *) (((int)time_page) +
					  (DP_EPOCH_LO - DP_ADDR_PH));
	printf("paragon_clock = 0x%x\n", paragon_clock);
#endif

	rpm = (struct rpm *) time_page;
	printf("rpm at address 0x%x\n", rpm);
	printf("rpm_time at address 0x%x\n", &rpm->rpm_time);

	printf("control %d, time %f, cpu0 %d, cpu1 %d, ltu %d\n",
	       rpm->rpm_control, rpm->rpm_time, rpm->rpm_cpu0, rpm->rpm_cpu1,
	       rpm->rpm_ltu);
	printf("exp %d cpu2 %d dbus %d dram %d n %d s %d e %d w %d\n",
	       rpm->rpm_exp, rpm->rpm_cpu2, rpm->rpm_dbus, rpm->rpm_dram,
	       rpm->rpm_north, rpm->rpm_south, rpm->rpm_east, rpm->rpm_west);
	return TRUE;
}
#endif	/* 0 */

#else  /* !PARAGON */
mapped_time_value_t *time_map_location = (mapped_time_value_t *)0;

void
time_map(void)
{
    mach_port_t
	my_task = mach_task_self(),
	privileged_host_port, device_server_port,
	device_port, memobj_port;
    int node;
    extern mach_port_t bootstrap_port;	/* from libmach */
    extern mach_port_t mach_host_priv_self(); /* from libmach */
    kern_return_t kr;
    security_token_t	security_token;
    int i;
    
    /*
     * Open the device.  This is a real pain, and for now requires
     * root priveleges.
     */

    privileged_host_port = mach_host_priv_self();
    if (privileged_host_port == MACH_PORT_NULL) {
	fprintf(stderr, "Couldn't get the host priveleged port\n");
	exit(-1);		
    }

    /* Don't use the device server port directly; we want to make sure
       we have the right device for the node we're on.  */

    kr = norma_port_location_hint(my_task, my_task, &node);
    if (kr != KERN_SUCCESS) {
	mach_error("norma_node_self fails", kr);
	exit(-1);
    }

    kr = norma_get_device_port(privileged_host_port, node,
			       &device_server_port);
    if (kr != KERN_SUCCESS) {
	mach_error("norma_get_device_port fails",kr);
	mach_port_deallocate(my_task, privileged_host_port);
	exit(-1);
    }
    
    kr = device_open(device_server_port,
		     (mach_port_t)0,
		     D_READ|D_WRITE,
		     security_token,
		     "time",
		     &device_port);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "time_map: device_open: %s\n",
		mach_error_string(kr));
	exit(-1);
    }
    
    /* Clean up from this section.  */
    mach_port_deallocate(my_task, device_server_port);
    mach_port_deallocate(my_task, privileged_host_port);
    
    /* Now that we have the device open, map it.  */
    
    /* Get the device mapping port.  */
    kr = device_map(device_port,
		    VM_PROT_READ|VM_PROT_WRITE,
		    0,
		    sizeof(time_value_t),
		    &memobj_port,
		    0);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "time_map: device_map: %s\n",
		mach_error_string(kr));
	exit(-1);
    }
    
    /* Map in the communications buffer.  */
    kr = vm_map(my_task,
		(vm_address_t *) &time_map_location,
		sizeof(time_value_t),
		0,		/* Mask. */
		1,		/* Map anywhere? */
		memobj_port,
		0,		/* Offset in object.  */
		0,		/* Copy?  */
		VM_PROT_READ,	/* Cur protection.  */
		VM_PROT_READ,	/* Max protection.  */
		VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "time_map: vm_map: %s\n",
		mach_error_string(kr));
	exit(-1);
    }
    
    /* Set up the log.  */
    timer_log_index = 0;
    for (i = 0; i < TIMER_LOG_SIZE; i++) {
	timer_log[i].t = 0.0;
	timer_log[i].annotation = (char *) 0;
    }

    /* Cleanup.  */
    mach_port_deallocate(my_task, memobj_port);
    mach_port_deallocate(my_task, device_port);
}

void map_time_sample(double *result)
{
    int start_secs[2];
    
    do {
	start_secs[0] = time_map_location->seconds;
	start_secs[1] = time_map_location->microseconds;
    } while (start_secs[0] != time_map_location->check_seconds);
    *result = start_secs[0] + 1.0e-6 * (double) start_secs[1];
}

void
time_unmap(void)
{
    mach_port_t my_task = mach_task_self();
    kern_return_t kr;
    
    kr = vm_deallocate(my_task,
		       (vm_address_t) time_map_location,
		       (vm_size_t) sizeof(time_value_t));		     
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "time_unmap: vm_deallocate: %s\n",
		mach_error_string(kr));
	exit(-1);
    }
}

void timer_dump_log_naked(FILE *stream)
{
    int i;
    /* Start just above the log index and circle round, skipping anything
       without an annotation.  */
    i = timer_log_index;
    do {
	fprintf(stream, "%032lf %04d %s\n",
		timer_log[i].t, i, timer_log[i].annotation);
	i++;
	if (i == TIMER_LOG_SIZE) i = 0;
    } while (i != timer_log_index);
}

extern struct timer_log_entry *timer_read_log_naked(FILE *stream)
{
    int local_log_size = TIMER_LOG_SIZE;
    struct timer_log_entry *local_log =
	(struct timer_log_entry *)
	    malloc(local_log_size * sizeof(struct timer_log_entry));
    int local_log_index = 0;
    char input_line[160];
    char *ptr;

    while (fgets(input_line, 160, stream) != NULL) {
	local_log[local_log_index].t = strtod(input_line, &ptr);
	ptr++;
	strtol(ptr, &ptr, 10);
	ptr++;
	local_log[local_log_index].annotation = (char *) malloc(strlen(ptr)+1);
	strcpy(local_log[local_log_index].annotation, ptr);
	local_log[local_log_index].annotation[strlen(ptr)-1] = '\0';
	local_log_index++;
	if (local_log_index == local_log_size) {
	    local_log_size *= 2;
	    local_log = (struct timer_log_entry *)
		realloc(local_log,
			local_log_size * sizeof(struct timer_log_entry));
	}
    }
    local_log[local_log_index].annotation = (char *) 0;
    return local_log;
}

int compare_logents(const void *e1, const void *e2)
{
    const struct timer_log_entry
	*e1t = e1,
	*e2t = e2;
    return (int) (e1t->t - e2t->t);
}
 
extern void timer_dump_log_clothed(FILE *stream, struct timer_log_entry *log)
{
    int log_size, i;

    for (log_size = 0; log[log_size].annotation != (char *) 0; log_size++)
	;

    /* I promised to sort it.  Silly me.  */
    qsort(log, log_size, sizeof(struct timer_log_entry), &compare_logents);

    fprintf(stream, "Delta from previous\tAnnotation\n");
    fprintf(stream, "-------------------\t----------\n");
    fprintf(stream, "                   \t%s\n",
	    log[0].annotation);
    for (i = 1; i < log_size; i++) {
	timer_sample_data diff;
	double tmp;

	TIMER_SUBTRACT(&log[i].t, &log[i-1].t, &diff);
	TIMER_CONVERT(&diff, &tmp);
	fprintf(stream, "%10.10lf\t\t%s\n", tmp, log[i].annotation);
    }
}

#endif	/* PARAGON */

