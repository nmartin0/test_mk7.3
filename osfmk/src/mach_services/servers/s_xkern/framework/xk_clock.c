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
 * xk_clock.c
 *
 * x-kernel v3.2
 *
 */

#include	<stdio.h>
#include	<mach.h>
#include	<mach/ndr.h>
#include	<mach/clock.h>
#include	<mach/mach_traps.h>
#include	<mach_error.h>
#include	<servers/netname.h>

#include	<xkern/include/xkernel.h>
#include	<xkern/include/xk_debug.h>
#include	<s_xkern/include/xk_clock.h>

int		traceclockprof;

#ifdef	XK_PROFILE

#define	TRUE	1
#define	FALSE	0

#ifndef	VM_ALLOCATE_ANYWHERE
#define	VM_ALLOCATE_ANYWHERE	1
#endif	/* !VM_ALLOCATE_ANYWHERE */

#define	CLOCK_BUFFER_MAX	32768
#define	CLOCK_BUFFER_LEN	(CLOCK_BUFFER_MAX*sizeof(ct_entry_t))

static mach_port_t	clock_port = MACH_PORT_NULL;
static int		clock_resolution = 0;
static ct_entry_t	*clock_buffer = NULL;
static int		clock_buffer_idx = 0;
static tvalspec_t	clock_overhead = { 0, 0 };
static mach_port_t	clock_profile_port = MACH_PORT_NULL;
static void		clock_trace_nop(char *caller_id);

void (*xk_clock_trace)(char *caller_id);

static void
clock_buffer_reply(
	clock_prof_reply_msg_t	*rmsg)
{
    ct_entry_t		*cte_buffer;
    int			cte_index;
    char		*ids_buffer;
    char		*ids_buf_ptr;
    int			ids_buf_len;
    kern_return_t	kr;

    xTrace0(clockprof, TR_EVENTS, "clockprof reply: started");

    if (clock_buffer == NULL) {
	xTrace0(clockprof, TR_ERRORS, "clockprof reply: no clock_buffer");
	return;
    }

    kr = vm_allocate(mach_task_self(), (vm_address_t *)&cte_buffer,
	CLOCK_BUFFER_LEN, VM_ALLOCATE_ANYWHERE);
    if (kr == KERN_SUCCESS) {
	xTrace2(clockprof, TR_EVENTS,
	    "clockprof reply: work_buffer=0x%x len=%d",
	    cte_buffer, CLOCK_BUFFER_LEN);
    } else {
	xTrace2(clockprof, TR_ERRORS,
	    "clockprof reply: vm_allocate %x %s",
	    kr, mach_error_string(kr));
	return;
    }

    /* ZZZ : Don't worry about racing condition :-) */
    bcopy((char *)clock_buffer, (char *)cte_buffer, CLOCK_BUFFER_LEN);

    for (cte_index=0, ids_buf_len = 0;
	 cte_index < CLOCK_BUFFER_MAX && cte_buffer[cte_index].name != NULL;
	 cte_index++) {
	ids_buf_len += strlen(cte_buffer[cte_index].name) + 1;
    }
    if (ids_buf_len == 0) {
	xTrace0(clockprof, TR_ERRORS, "clockprof reply: no clock data");
	vm_deallocate(mach_task_self(),
	    (vm_address_t)cte_buffer, CLOCK_BUFFER_LEN);
	return;
    }
    kr = vm_allocate(mach_task_self(),
	    (vm_address_t *)&ids_buffer, ids_buf_len, VM_ALLOCATE_ANYWHERE);
    if (kr == KERN_SUCCESS) {
	xTrace2(clockprof, TR_EVENTS,
	    "clockprof reply: ids_buffer=0x%x len=%d",
	    ids_buffer, ids_buf_len);
    } else {
	xTrace2(clockprof, TR_ERRORS,
	    "clockprof reply: vm_allocate for ids %x %s",
	    kr, mach_error_string(kr));
	vm_deallocate(mach_task_self(),
	    (vm_address_t)cte_buffer, CLOCK_BUFFER_LEN);
	return;
    }
    for (cte_index=0, ids_buf_ptr = ids_buffer;
	cte_index < CLOCK_BUFFER_MAX && cte_buffer[cte_index].name != NULL;
	cte_index++, ids_buf_ptr += strlen(ids_buf_ptr)+1) {
	strcpy(ids_buf_ptr, cte_buffer[cte_index].name);
	cte_buffer[cte_index].name = (char *)(ids_buf_ptr - ids_buffer);
    }

    if (rmsg->Head.msgh_remote_port == MACH_PORT_NULL ||
	(MACH_MSGH_BITS_REMOTE(rmsg->Head.msgh_bits)
	    != MACH_MSG_TYPE_PORT_SEND &&
	 MACH_MSGH_BITS_REMOTE(rmsg->Head.msgh_bits)
	    != MACH_MSG_TYPE_PORT_SEND_ONCE)) {
	xTrace1(clockprof, TR_EVENTS,
	    "clockprof reply: invalid message %x",
	    rmsg->Head.msgh_bits);
	vm_deallocate(mach_task_self(),
	    (vm_address_t)cte_buffer, CLOCK_BUFFER_LEN);
	vm_deallocate(mach_task_self(),
	    (vm_address_t)ids_buffer, ids_buf_len);
	return;
    }
    rmsg->Head.msgh_bits |= MACH_MSGH_BITS_COMPLEX;
    rmsg->Head.msgh_id += 100;
    rmsg->Body.msgh_descriptor_count = 2;
    rmsg->Ool_tbl.address = cte_buffer;
    rmsg->Ool_tbl.size = CLOCK_BUFFER_LEN;
    rmsg->Ool_tbl.deallocate = FALSE;
    rmsg->Ool_tbl.copy = MACH_MSG_VIRTUAL_COPY;
    rmsg->Ool_tbl.type = MACH_MSG_OOL_DESCRIPTOR;
    rmsg->Ool_ids.address = ids_buffer;
    rmsg->Ool_ids.size = ids_buf_len;
    rmsg->Ool_ids.deallocate = TRUE;
    rmsg->Ool_ids.copy = MACH_MSG_VIRTUAL_COPY;
    rmsg->Ool_ids.type = MACH_MSG_OOL_DESCRIPTOR;
    rmsg->NDR = NDR_record;
    rmsg->Resolution = clock_resolution;
    kr = mach_msg(
	    &rmsg->Head,
	    MACH_SEND_MSG,
	    sizeof(clock_prof_reply_msg_t),
	    0,
	    MACH_PORT_NULL,
	    MACH_MSG_TIMEOUT_NONE,
	    MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
	xTrace2(clockprof, TR_ERRORS,
	    "clockprof reply: mach_msg(MACH_SEND_MSG) %x %s",
	    kr, mach_error_string(kr));
    }
    return;
}

static void
clock_buffer_print( void )
{
    ct_entry_t *	cte_buffer;
    int			cte_index;
    int			next_index;
    int			previous;
    kern_return_t	kr;

    if (clock_buffer == NULL) {
	xTrace0(clockprof, TR_ERRORS,
	    "clockprof print: no clock_buffer");
	return;
    }

    kr = vm_allocate(mach_task_self(), (vm_address_t *)&cte_buffer,
	CLOCK_BUFFER_LEN, VM_ALLOCATE_ANYWHERE);
    if (kr == KERN_SUCCESS) {
	xTrace2(clockprof, TR_EVENTS,
	    "clockprof print: work_buffer=0x%x len=%d",
	    cte_buffer, CLOCK_BUFFER_LEN);
    } else {
	xTrace2(clockprof, TR_ERRORS,
	    "clockprof print: vm_allocate %x %s",
	    kr, mach_error_string(kr));
	return;
    }
    next_index = clock_buffer_idx;
    /* ZZZ : Don't worry about racing condition :-) */
    bcopy((char *)clock_buffer, (char *)cte_buffer, CLOCK_BUFFER_LEN);

    if (next_index < 0 || CLOCK_BUFFER_MAX < next_index) {
	xTrace1(clockprof, TR_ERRORS,
	    "clockprof print: clock_buffer_idx broken %x",
	    cte_index);
	vm_deallocate(mach_task_self(),
	    (vm_address_t)cte_buffer, CLOCK_BUFFER_LEN);
	return;
    } else if (cte_buffer[0].time.tv_sec == 0 &&
	       cte_buffer[0].time.tv_nsec == 0) {
	xTrace0(clockprof, TR_ERRORS, "clockprof print: no clock data");
	vm_deallocate(mach_task_self(),
	    (vm_address_t)cte_buffer, CLOCK_BUFFER_LEN);
	return;
    }

    while (TRUE) {
	if (next_index == CLOCK_BUFFER_MAX || next_index == 0) {
	    next_index = 0;
	    cte_index = CLOCK_BUFFER_MAX - 1;
	} else {
	    cte_index = next_index - 1;
	}
	if (CMP_TVALSPEC(&cte_buffer[cte_index].time,
	    &cte_buffer[next_index].time) > 0) {
	    if (cte_buffer[next_index].time.tv_sec == 0 &&
		cte_buffer[next_index].time.tv_nsec == 0) {
		cte_index = 0;
	    } else {
		cte_index = next_index;
	    }
	    break;
	} else {
	    next_index++;
	}
    }

    printf("clockprof print: trace overhead is %d.%9d sec\n",
	clock_overhead.tv_sec, clock_overhead.tv_nsec);
    do {	
	printf("%9d.%9d %s\n",
	    cte_buffer[cte_index].time.tv_sec,
	    cte_buffer[cte_index].time.tv_nsec,
	    cte_buffer[cte_index].name);
	if (++cte_index == CLOCK_BUFFER_MAX) {
	    cte_index = 0;
	}
    } while (cte_index != next_index);
    vm_deallocate(mach_task_self(),
	(vm_address_t)cte_buffer, CLOCK_BUFFER_LEN);
    xTrace0(clockprof, TR_EVENTS, "clockprof print: done");
    return;
}

static void
clock_profile_server(
    Event	ev,
    void	*vp)
{
    kern_return_t		kr;
    clock_prof_reply_msg_t *	rmsg;
    int 	rmsg_len = sizeof(clock_prof_reply_msg_t)+MAX_TRAILER_SIZE;
    int				trial_count = 0;

    xTrace0(clockprof, TR_FULL_TRACE, "clockprof profile server: started");
    xk_master_unlock();
    rmsg = (clock_prof_reply_msg_t *)xMalloc(rmsg_len);
    if (rmsg == NULL) {
	xTrace0(clockprof, TR_ERRORS,
	    "clockprof profile server: xMalloc for rmsg failed");
	xk_master_lock();
	return;
    } else {
	xTrace1(clockprof, TR_EVENTS,
	    "clockprof profile server: rmsg %x", rmsg);
    }

    kr = mach_port_allocate(
	mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &clock_profile_port);
    if (kr != KERN_SUCCESS) {
	xTrace2(clockprof, TR_ERRORS,
	    "clockprof profile server: mach_port_allocate %x %s\n",
	    kr, mach_error_string(kr));
	xk_master_lock();
	return;
    } else {
	xTrace1(clockprof, TR_EVENTS,
	    "clockprof profile server: service port %x",
	    clock_profile_port);
    }

    xk_master_lock();	/* the master lock is released in Delay() */
    Delay(1000);	/* wait 1 second */
    xk_master_unlock();
    xTrace0(clockprof, TR_EVENTS,
	"clockprof profile server: let's check me in");

    while ((kr =
	netname_check_in(
	    name_server_port,
	    NMS_PROFILE_SERVER,
	    mach_task_self(),
	    clock_profile_port)) != KERN_SUCCESS) {
	if (trial_count == 0 || trial_count++ > 10) {
	    xTrace2(clockprof, TR_ERRORS,
		"clockprof profile server: netname_check_in %x %s\n",
		kr, mach_error_string(kr));
	    trial_count = 1;
	}
	xk_master_lock();   /* the master lock is released in Delay() */
	Delay(1000);	    /* wait 1 second */
	xk_master_unlock();
    }
    xTrace1(clockprof, TR_EVENTS,
	"clockprof profile server: %s checked in", NMS_PROFILE_SERVER);

    while(1) {
	bzero((char *)rmsg, rmsg_len);
	kr = mach_msg(
		&rmsg->Head,
		MACH_RCV_MSG,
		0,
		rmsg_len,
		clock_profile_port,
		MACH_MSG_TIMEOUT_NONE,
		MACH_PORT_NULL);
	if (kr == MACH_MSG_SUCCESS) {
	    xTrace0(clockprof, TR_EVENTS,
		"clockprof profile server: message received");
	    if (rmsg->Head.msgh_id == CLOCKPROF_PRINT) {
		clock_buffer_print();
	    } else if (rmsg->Head.msgh_id == CLOCKPROF_REPLY) {
		clock_buffer_reply(rmsg);
	    }
	} else {
	    xTrace2(clockprof, TR_ERRORS,
		"clockprof profile server: mach_msg(RCV_MSG) %x %s",
		kr, mach_error_string(kr));
	}
    }
    xk_master_lock();
}

static void
clock_thread_init( void )
{
    evDetach(evSchedule(clock_profile_server, (void *)0, 0));
    /*
     * cthread_t	child;
     *
     * child = cthread_fork((cthread_fn_t)clock_profile_server, 0);
     * cthread_set_name(child,"clock profile server");
     * cthread_detach(child);
     * xTrace0(clockprof, TR_EVENTS,
     * 	"clockprof init: profile server starting");
     */
}

static ct_entry_t *
clock_buffer_init( void )
{
    kern_return_t	kr;
    vm_address_t *	buffer;

    xTrace0(clockprof, TR_EVENTS, "clockprof init: thread init started");
    kr = vm_allocate(mach_task_self(), (vm_address_t *)&buffer,
	CLOCK_BUFFER_LEN, VM_ALLOCATE_ANYWHERE);
    if (kr == KERN_SUCCESS) {
	xTrace2(clockprof, TR_EVENTS,
	    "clockprof init: clock_buffer=0x%x len=%d",
	    buffer, CLOCK_BUFFER_LEN);
    } else {
	xTrace2(clockprof, TR_ERRORS,
	    "clockprof init: vm_allocate %x %s",
	    kr, mach_error_string(kr));
	return (ct_entry_t *)NULL;
    }
    bzero((char *)buffer, CLOCK_BUFFER_LEN);
    return (ct_entry_t *)buffer;
}

static void
clock_trace_name_buffer(
	char	*caller_id)
{
    register int	buffer_idx;

    buffer_idx = clock_buffer_idx++;

    if (buffer_idx < CLOCK_BUFFER_MAX) {
	clock_get_time(clock_port, &clock_buffer[buffer_idx].time);
	clock_buffer[buffer_idx].name = caller_id;
	return;
    } else {
	clock_buffer_idx = 0;
	clock_trace_name_buffer(caller_id);
	return;
    }
}

static void
clock_trace_name(
	char	*caller_id)
{
    kern_return_t	kr;
    tvalspec_t      current_time;

    kr = clock_get_time(clock_port, &current_time);
    if (kr == KERN_SUCCESS) {
	xTrace3(clockprof, TR_ALWAYS,
	    "clockprof trace: %d.%9d %s",
	    current_time.tv_sec, current_time.tv_nsec, caller_id);
    } else {
	xTrace3(clockprof, TR_ERRORS,
	    "clockprof trace: clock_get_time %x %s %s",
	    kr, mach_error_string(kr), caller_id);
    }
}

static void
clock_trace_nop(
	char	*caller_id)
{
    return;
}

static void
get_clock_trace_over_head( void )
{
    tvalspec_t	start_time;

    xk_clock_trace("clockprof overhd: 0");
    xk_clock_trace("clockprof overhd: 1");
    xk_clock_trace("clockprof overhd: 2");
    xk_clock_trace("clockprof overhd: 3");
    xk_clock_trace("clockprof overhd: 4");
    xk_clock_trace("clockprof overhd: 5");
    xk_clock_trace("clockprof overhd: 6");
    xk_clock_trace("clockprof overhd: 7");
    xk_clock_trace("clockprof overhd: 8");
    xk_clock_trace("clockprof overhd: 9");
    xk_clock_trace("clockprof overhd: A");
    xk_clock_trace("clockprof overhd: B");
    xk_clock_trace("clockprof overhd: C");
    xk_clock_trace("clockprof overhd: D");
    xk_clock_trace("clockprof overhd: E");
    xk_clock_trace("clockprof overhd: F");
    xk_clock_trace("clockprof overhd: G");

    start_time = clock_buffer[0].time;
    clock_overhead = clock_buffer[16].time;
    SUB_TVALSPEC(&clock_overhead, &start_time);
    if (clock_overhead.tv_sec == 0) {
	xTrace1(clockprof, TR_EVENTS,
	    "clockprof overhd: 16*overhead %9d nsec",
	    clock_overhead.tv_nsec);
	clock_overhead.tv_nsec /= 16;
    } else {
	xTrace2(clockprof, TR_ERRORS,
	    "clockprof overhd: 16*overhead %d.%9d sec. I can't believe it !!",
	    clock_overhead.tv_sec, clock_overhead.tv_nsec);
    }
    return;
}

xkern_return_t
xk_clock_init( void )
{
    kern_return_t	kr;
    mach_msg_type_number_t	attr_count = 1;

    kr = host_get_clock_service(mach_host_self(),
	REALTIME_CLOCK, &clock_port);
    if (kr == KERN_SUCCESS) {
	xTrace1(clockprof, TR_ALWAYS,
	    "clockprof init: clock service port obtained %x",
	    clock_port);
    } else {
	xTrace2(clockprof, TR_ERRORS,
	    "clockprof init: host_get_clock_service %x %s",
	    kr, mach_error_string(kr));
	xk_clock_trace = clock_trace_nop;
	return XK_SUCCESS;
    }

    kr = clock_get_attributes(clock_port, CLOCK_GET_TIME_RES,
	&clock_resolution, &attr_count);
    if (kr == KERN_SUCCESS) {
	xTrace1(clockprof, TR_ALWAYS,
	    "clockprof init: clock resolution is %d", clock_resolution);
    } else {
	xTrace2(clockprof, TR_ERRORS,
	    "clockprof init: clock_get_attributes %x %s",
	    kr, mach_error_string(kr));
	return XK_FAILURE;
    }

    clock_buffer = clock_buffer_init();
    if (clock_buffer != NULL) {
	xk_clock_trace = clock_trace_name_buffer;
    } else {
	xk_clock_trace = clock_trace_name;
    }

    get_clock_trace_over_head();

    clock_thread_init();

    return XK_SUCCESS;
}

#if	0
#ifdef	i386
static void
clock_trace_address( void )
{
    register vm_address_t	caller_address;

    asm volatile("movl 4(%ebp), %ebx");   /* %ebx is caller_address */
    xk_clock_trace((char *)caller_address);
}
#else	/* !i386 */
static void
clock_trace_address( void )
{
    clock_trace_name("(no caller address)");
}
#endif	/* !i386 */
#endif	/* 0 */
#else	/* !XK_PROFILE */
xkern_return_t xk_clock_init( void ) { return XK_SUCCESS; }
#endif	/* !XK_PROFILE */

