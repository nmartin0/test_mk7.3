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
 * process.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 */

#include <xkern/include/xkernel.h>
#include <xkern/include/assert.h>
#include <xkern/include/platform.h>
#include <xkern/include/event_i.h>

/* forwards */

static void
	enter_master_sledgehammer_monitor( void ** );

#define MAXARGS		6

#define MAX_XK_THREADS 32
/*
 * try to keep running threads less than this
 */
#define XK_MAX_RUNNING_THREADS 24

static int		current_xk_threads = 0;
int			max_xk_threads_inuse = 0;
static mach_port_t	default_processor_set;

mutex_t		sledgehammer_concurrency_control;
#define XTHREAD_SELF()	cthread_self()
#define XTHREAD_TYPE	cthread_t

static xkern_return_t	taskDefaultPolicy( void );

#ifdef XKLOCKDEBUG
int		xklockdepthreq = 0;
int		xklockdepth = 0;
int		tracexklock = 0;
#endif /* XKLOCKDEBUG */

typedef struct {
    int			*args;	/* pointer to argument block */
} argBlock;


/*
 * xk_master_lock, xk_master_unlock
 *
 * user callable master concurrency control
 *
 */
void
xk_master_lock()
{
    MASTER_LOCK;
}

void
xk_master_unlock()
{
    MASTER_UNLOCK;
}


/*
 * threadInit
 */
xkern_return_t
threadInit()
{
    kern_return_t	kr;

    xTrace0(processcreation, TR_FULL_TRACE, "Enter event function threadInit");

    sledgehammer_concurrency_control = mutex_alloc();
    return XK_SUCCESS;
}


/*
 *  xkernel_activate_thread
 *
 *     startup a thread, given the arguments
 *
 */
static void
xkernel_activate_thread(
    void	*args)
{

    XTHREAD_TYPE	child;

    xTrace0(processcreation, TR_FULL_TRACE, "xkernel_activate_thread enter");
    /*
     * here we can start the thread with a pointer to arguments directly
     */
#if 0
    if (current_xk_threads+4 >= xk_thread_limit) {
	xk_thread_limit += 2;
	cthread_set_kernel_limit(xk_thread_limit);
	xTrace1(processcreation, TR_ERRORS,
	    "Cthread kernel limit now %d", xk_thread_limit);
    }
#endif
    child = cthread_fork(
		(cthread_fn_t)enter_master_sledgehammer_monitor, (void *)args);
    cthread_set_name(child,"xkernel thread");

    cthread_detach(child);
    xTrace1(processcreation, TR_MAJOR_EVENTS, "Created thread_id: %d", child);
    xTrace1(processswitch, TR_GROSS_EVENTS, "Created thread_id: %d", child);
}


/*
 *
 * CreateProcess(function_ptr, Event, argcount, arg1, arg2, ...)
 *
 */
xkern_return_t
CreateProcess( gen_func r, Event ev, int numArgs, ...)
{
    va_list	ap;
    int 	i;
    void 	**argp;

    xTrace0(processcreation, TR_EVENTS, "Enter CreateProcess");

    va_start(ap, numArgs);

    if ( numArgs > MAXARGS ) {
	xTrace1(processcreation, TR_ERRORS,
		"x-kernel CreateProcess: too many arguments (%d)", numArgs);
	return XK_FAILURE;
    }
    argp = ev->extra;
    *argp++ = (void *)r;
    *argp++ = (void *)numArgs;
    for ( i=0; i < numArgs; i++ ) {
	*argp++ = va_arg(ap, void *);
    }
    xTrace2(processcreation, TR_MAJOR_EVENTS,
	    "Mach CreateProcess: func_addr %lx nargs %d", (long) r, numArgs);
    xkernel_activate_thread(ev->extra);
  
    return XK_SUCCESS;
}


xkern_return_t
procAllocState( Event ev, Allocator a )
{
    return (ev->extra = allocGet(a, (MAXARGS + 2) * sizeof(void *))) ?
      		XK_SUCCESS : XK_FAILURE;
}


void
procFreeState( Event ev )
{
    allocFree(ev->extra);
}


/*
 *
 * Yield
 *
 */
void
Yield()
{
    xTrace0(processswitch, TR_FULL_TRACE, "Enter event function Yield");
    MASTER_UNLOCK;
    cthread_yield();
    MASTER_LOCK;
}


/*
 *
 * enter_master_sledgehammer_monitor
 *
 * Concurrency control is enforced in the xkernel by means of a monitor,
 *  in which all execution takes place. Here it is!
 * 
 * A new thread begins here.
 * Inside the kernel:
 *	Dequeue a newThreadInfo block and do what it says
 * Outside the kernel:
 *	We are passed the argBlock.
 *
 * All the process block stuff looks totally defunct
 *
 */
static void
enter_master_sledgehammer_monitor(
    void **	args)
{
    gen_func	user_fn;
    int		nargs;
    int		threadcount;



    xTrace0(processcreation, TR_MAJOR_EVENTS,
	"enter_master_monitor: entering\n");

    /* 
     * Both input-pool and other event threads pass through here; in
     * both cases, the policy of these threads will be explicitly set.
     * Set the policy intially for a regular event thread -- it will
     * be overridden later if this is really an input-pool thread.
     */
    {
      int	priority = XK_EVENT_THREAD_PRIORITY;
      xkPolicyFixedFifo(&priority, sizeof(priority));
    }

    MASTER_LOCK;
    xTrace0(processcreation, TR_MAJOR_EVENTS, "enter_master_monitor: locked\n");

    user_fn = (gen_func)*(args++);
    nargs = (int)*(args++);
    threadcount = current_xk_threads++;
    if (threadcount > max_xk_threads_inuse) {
	max_xk_threads_inuse = threadcount;
	xTrace1(processcreation, TR_EVENTS,
	    "processcreate: new thread high water mark %d", threadcount);
    }

    if (nargs > MAXARGS) 
	Kabort("master_sledgehammer_monitor: cannot handle more than 6 args.");
    xTrace1(processcreation, TR_EVENTS,
	"enter_master_monitor: starting thread %d", XTHREAD_SELF());

    switch(nargs) {
	case 0: (*((void (*)(void))user_fn))();
            break;
	case 1: (*user_fn)(args[0]);
            break;
	case 2: (*user_fn)(args[0],args[1]);
            break;
	case 3: (*user_fn)(args[0],args[1],args[2]);
            break;
	case 4: (*user_fn)(args[0],args[1],args[2],args[3]);
            break;
	case 5: (*user_fn)(args[0],args[1],args[2],args[3],args[4]);
            break;
	case 6: (*user_fn)(args[0],args[1],args[2],args[3],args[4],args[5]);
            break;
    }

    current_xk_threads--;
    MASTER_UNLOCK;
    xTrace1(processcreation, TR_MAJOR_EVENTS,
	"master_monitor: thread %d unlocks", XTHREAD_SELF() );
}


/*
 * Delay
 */
void
Delay(
    int		n)	/* n in ms */
{
    evDelay(n);
}


/*
 * semInit
 */
void
semInit(
    xkSemaphore		*s,
    unsigned int	n)
{
    xTrace0(processswitch, TR_FULL_TRACE, "Enter event function semInit");
    mutex_init(&s->lock);
    condition_init(&s->cond);
    s->sleepers = 0;
    s->count = n;
}


/*
 * realP
 */
void
realP(
    xkSemaphore	*s)
{
    xTrace2(processswitch, TR_MAJOR_EVENTS,
	"P on %#x by 0x%x", s, XTHREAD_SELF());
    if (s->count < 0) {
	xTrace2(processswitch, TR_GROSS_EVENTS,
	    "Blocking P on %#x by 0x%x", s, XTHREAD_SELF());
#ifdef XK_THREAD_TRACE
	evMarkBlocked(s);
#endif
	MASTER_UNLOCK;		/* let the realV signaller get a chance */
	mutex_lock(&s->lock);	/* lock the semaphore while modifying*/
	s->sleepers--;		/* put ourself into the pool */
	while (s->count <= s->sleepers) {
				/* check for someone leaving the pool */
	    /*
	     * wait for wake-up signal AND lock acquistion, else release lock
	     */
	    condition_wait(&s->cond,&s->lock);
	}
	s->sleepers++;		/* remove ourself */
	mutex_unlock(&s->lock);	/* let another into condition wait */
	/*
	 * changed suggested by Travostino, OSF
	 */
	MASTER_LOCK;		/* get back into the xkernel swing of things */
#ifdef XK_THREAD_TRACE
	evMarkRunning();
#endif
	xTrace2(processswitch, TR_MAJOR_EVENTS,
	    "Waking-up from P on %#x by 0x%x", s, XTHREAD_SELF());
    }
}


/*
 * realV
 */
void
realV(
    xkSemaphore	*s)
{
    xTrace2(processswitch, TR_MAJOR_EVENTS,
	"V on %#x by 0x%x", s, XTHREAD_SELF());
    /*
     * get the lock so we can check the semaphore state
     * and then signal the waiters
     */
    mutex_lock(&s->lock);
    if (s->count <= 0) {
	xTrace2(processswitch, TR_GROSS_EVENTS,
	    "Unblocking V on %#x by 0x%x", s, XTHREAD_SELF());
	/*
	 * let a waiter go; he tries for the lock now
	 */
	condition_signal(&s->cond);
    }
    mutex_unlock(&s->lock);  /* let sleepers have a chance */
}


/*
 * VAll
 *
 *  This operation is not supported; it cannot be easily implemented under
 *  the Mach cthreads package.  However, a simpler form that uses
 *  an alternative user call in place of semWait could be implemented.
 *  If that call is "waitP", then this routine will do "wake all threads
 *  in waitP".
 *
 */
void
VAll(
    xkSemaphore	*s)
{
    xTrace2(processswitch, TR_MAJOR_EVENTS,
	"VAll on %#x by 0x%x", s, XTHREAD_SELF());
    s->count = 0;
    MASTER_UNLOCK;
    mutex_lock(&s->lock);
    mutex_unlock(&s->lock);
    MASTER_LOCK;
    condition_broadcast(&s->cond);
}



#define err_check(str, call) 						\
    do {								\
        kern_return_t kr;						\
        kr = call;							\
	if ( kr != KERN_SUCCESS ) {					\
		sprintf(errBuf, "xkPolicy call %s returns %s", 		\
			str, mach_error_string(kr));			\
		xError(errBuf);						\
		return XK_FAILURE; 					\
	}								\
    } while (0)


#if XK_DEBUG

static xkern_return_t
threadSchedInfo( char *str )
{
    struct thread_basic_info	info;
    mach_msg_type_number_t	outCount;

    outCount = THREAD_BASIC_INFO_COUNT;
    err_check("thread_info", 
	      thread_info(mach_thread_self(), THREAD_BASIC_INFO,
			  (thread_info_t)&info, &outCount));
    switch (info.policy) {
      case POLICY_RR:
	{
	    struct policy_rr_info	rr_info;

	    outCount = POLICY_RR_INFO_COUNT;
	    err_check("thread_info (rr)",
		      thread_info(mach_thread_self(), THREAD_SCHED_RR_INFO,
				  (thread_info_t)&rr_info, &outCount));
	    xTrace4(processcreation, TR_MAJOR_EVENTS,
		    "thread policy (%s): round-robin, quantum %d, max %d, base %d",
		    str,
		    rr_info.quantum,
		    rr_info.max_priority,
		    rr_info.base_priority);
	}
	break;

      case POLICY_FIFO:
	{
	    struct policy_fifo_info	fifo_info;

	    outCount = POLICY_FIFO_INFO_COUNT;
	    err_check("thread_info (fifo)",
		      thread_info(mach_thread_self(), THREAD_SCHED_FIFO_INFO,
				  (thread_info_t)&fifo_info, &outCount));
	    xTrace3(processcreation, TR_MAJOR_EVENTS,
		    "thread policy (%s): fifo, max %d, base %d",
		    str,
		    fifo_info.max_priority,
		    fifo_info.base_priority);
	}
	break;

      default:
	xTrace2(processcreation, TR_ALWAYS, "thread policy (%s): %d", str, 
		info.policy);
    }
    return XK_SUCCESS;
}

#endif /* XK_DEBUG */


static xkern_return_t
getDefProcSet( void )
{
    mach_port_t default_processor_set_name;
    mach_port_t privileged_host_port;
    mach_port_t device_server_port;
    mach_port_t bootstrap_port;
    mach_port_t ledger_wired;
    mach_port_t ledger_paged;
    mach_port_t security_port;


    /*
     * Get privileged host port:
     */
    err_check("task_get_special_port",
	      task_get_special_port(
				    mach_task_self(),
				    TASK_BOOTSTRAP_PORT,
				    &bootstrap_port));

    err_check("bootstrap_ports",
	      bootstrap_ports(
			      bootstrap_port,
			      &privileged_host_port,
			      &device_server_port,
			      &ledger_wired,
			      &ledger_paged,
			      &security_port));

    xTrace0(processcreation, TR_MAJOR_EVENTS, "Got the privileged host port");
    
    /* Get default_processor_set: */
    err_check("processor_set_default",
	      processor_set_default(mach_host_self(),
				    &default_processor_set_name));
    err_check("host_processor_set_priv",
	      host_processor_set_priv(privileged_host_port,
				      default_processor_set_name,
				      &default_processor_set));
    return XK_SUCCESS;
}


xkern_return_t
xkPolicyFixedRR(
    void	*arg,
    u_int	argLen)
{
    policy_rr_base_data_t rr_base;
    policy_rr_limit_data_t rr_limit;
    int	priority;

    if ( argLen != sizeof(int) ) {
	return XK_FAILURE;
    }
    if ( ! default_processor_set && getDefProcSet() != XK_SUCCESS ) {
	return XK_FAILURE;
    }
    priority = *(int *)arg;
    if ( priority < XK_THREAD_PRIORITY_MIN ||
	 priority > XK_THREAD_PRIORITY_MAX ) {
	return XK_FAILURE;
    }
    xTrace1(processcreation, TR_MAJOR_EVENTS, "setting round-robin priority %d",
	    priority);
    cthread_wire();
    /* 
     * Policy-setting code from Michael Condict's rtpolicy program.
     */
#if XK_DEBUG    
    threadSchedInfo("before");
#endif

    /* Set policy: */
    rr_base.quantum = 0;
    rr_base.base_priority = priority;
    rr_limit.max_priority = priority;
    err_check("thread_set_policy",
	      thread_set_policy(mach_thread_self(),
				default_processor_set,
				POLICY_RR,
				(policy_base_t)&rr_base,
				POLICY_RR_BASE_COUNT,
				(policy_limit_t)&rr_limit,
				POLICY_RR_LIMIT_COUNT));
#if XK_DEBUG    
    threadSchedInfo("after");
#endif
    return XK_SUCCESS;
}


xkern_return_t
xkPolicyFixedFifo(
    void	*arg,
    u_int	argLen)
{
    policy_fifo_base_data_t	fifo_base;
    policy_fifo_limit_data_t	fifo_limit;
    int				priority;

    if ( argLen != sizeof(int) ) {
	return XK_FAILURE;
    }
    if ( ! default_processor_set && getDefProcSet() != XK_SUCCESS ) {
	return XK_FAILURE;
    }
    priority = *(int *)arg;
    if ( priority < XK_THREAD_PRIORITY_MIN ||
	 priority > XK_THREAD_PRIORITY_MAX ) {
	return XK_FAILURE;
    }
    xTrace1(processcreation, TR_MAJOR_EVENTS, "setting fifo priority %d",
	    priority);
    cthread_wire();
#if XK_DEBUG    
    threadSchedInfo("before");
#endif

    /* Set policy: */
    fifo_base.base_priority = priority;
    fifo_limit.max_priority = priority;
    err_check("thread_set_policy",
	      thread_set_policy(mach_thread_self(),
				default_processor_set,
				POLICY_FIFO,
				(policy_base_t)&fifo_base,
				POLICY_FIFO_BASE_COUNT,
				(policy_limit_t)&fifo_limit,
				POLICY_FIFO_LIMIT_COUNT));
#if XK_DEBUG    
    threadSchedInfo("after");
#endif
    return XK_SUCCESS;
}



void
xkIncreaseCthreadLimit(
    int		n)
{
#if 0
    if (cthread_kernel_limit()) {
	xk_thread_limit += n;
	cthread_set_kernel_limit( xk_thread_limit );
	xTrace1(processcreation, TR_MAJOR_EVENTS,
	    "increased xkernel cthread limit to %d",
	cthread_kernel_limit());
    } else {
	xTrace0(processcreation, TR_MAJOR_EVENTS,
	  "increaseCthreadLimit -- xkernel is not limiting number of cthreads");
    }
#endif
}

