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
 * input_proc.c,v
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * input_proc.c,v
 * Revision 1.13.1.6.1.2  1994/09/09  20:07:18  menze
 * OSF modifications
 *
 * Revision 1.13.1.5  1994/07/21  23:41:20  menze
 * Messages take Paths rather than Allocators
 *
 * Revision 1.13.1.4  1994/04/05  22:11:06  menze
 * Debugging support:
 *    pool consistency checks
 *    name field for pools
 *
 * Revision 1.13.1.3  1994/04/01  16:57:04  menze
 * New free queue with existing buffers was being marked as empty
 *
 * Revision 1.13.1.2  1994/03/24  00:48:14  menze
 * Added calls to allocators.  No testing yet ...
 *
 * Revision 1.13.1.1  1994/03/11  20:02:25  menze
 * Initial cut at providing for multiple input pools
 *
 * Revision 1.13  1994/02/05  00:11:53  menze
 *   [ 1994/01/28          menze ]
 *   xk_assert.h renamed to assert.h
 *
 */

#include <mach.h>
#include <cthreads.h>
#include <mach/message.h>

#include <xkern/include/xkernel.h>
#include <xkern/include/assert.h>
#include <xkern/include/platform.h>
#include <xkern/include/list.h>
#include <xkern/include/event_i.h>
#include <xkern/include/prot/eth.h>
#include <xkern/include/romopt.h>
#include <s_xkern/include/eth_support.h>

typedef struct {
    Path	path;
    int		allocSize;
    int 	activeThreads;   
    int		activeHighWater;  /* keeps track of max concurrent threads used */
    int		numThreads;
    EthInputBuffer	**inBuffs;
} PrivPoolInfo;


/* 
 * Default thread policy for input pools
 */
static int		defaultPriority = XK_INPUT_THREAD_PRIORITY;
static XkThreadPolicy_s defaultPolicy = {
    xkPolicyFixedFifo,
    &defaultPriority,
    sizeof(defaultPriority)
};

/* 
 * Number of msec to delay after a failed allocation attempt
 */
#define INTER_ALLOC_DELAY	(5 * 1000)

#define XK_THREAD_STATS	XK_DEBUG

#if XK_DEBUG
int	traceinputpool;
#endif /* XK_DEBUG */

static void	EthInputLoop( Event, void * );


static xkern_return_t
protectedQueueInit( pq, path )
    ProtectedQueue	*pq;
    Path		path;
{
    pq->lock = mutex_alloc();
    pq->notEmpty = condition_alloc();
    if ( ! (pq->list = (list_t)pathAlloc(path, sizeof(struct list_head))) ) {
	return XK_FAILURE;
    }
    list_init(pq->list);
    pq->empty = 1;
    return XK_SUCCESS;
}


static boolean_t
firstDataPtr( char *ptr, long len, void *arg )
{
    *(char **)arg = ptr;
    return 0;
}


/*
 * xkEthBufferPoolInit()
 *
 *    Initial input handling structures
 *
 */
EthInputPool *
EthInPoolInit(
	      u_int numBuffers, 
	      u_int numThreads, 
	      u_int dataSize, 
	      Path path,
	      char *name,
	      XkThreadPolicy policy)
{
    int 		i;
    PrivPoolInfo	*pi;
    EthInputPool	*p;
    xkern_return_t	xkr;
    Event		*ev;
    
    if ( (msgReserveContig(path, numBuffers, dataSize, 0) != XK_SUCCESS) ||
	 (p = pathAlloc(path, sizeof(EthInputPool))) == 0 ||
	 (protectedQueueInit(&p->inQueue, path) != XK_SUCCESS) ||
	 (protectedQueueInit(&p->freeQueue, path) != XK_SUCCESS) ||
	 (p->name = pathAlloc(path, strlen(name) + 1)) == 0 ||
	 (pi = pathAlloc(path, sizeof(PrivPoolInfo))) == 0 ||
	 (pi->inBuffs = pathAlloc(path, sizeof(EthInputBuffer *) * numBuffers))
		== 0 ) {
	xTrace0(inputpool, TR_ERRORS, "input pool allocation error");
	return 0;
    }
    if ( policy ) {
	if ( ! (p->policy.arg = pathAlloc(path, policy->argLen)) ) {
	    xTrace0(inputpool, TR_ERRORS, "input pool allocation error");
	    return 0;
	}
	memcpy(p->policy.arg, policy->arg, policy->argLen);
	p->policy.argLen = policy->argLen;
	p->policy.func = policy->func;
    } else {
	memcpy(&p->policy, &defaultPolicy, sizeof(XkThreadPolicy_s));
    }
    for ( i=0; i < numBuffers; i++ ) {
	pi->inBuffs[i] = pathAlloc(path, sizeof(EthInputBuffer));
	if ( pi->inBuffs[i] == 0 ) {
	    xTrace0(inputpool, TR_ERRORS, "input buffer allocation error");
	    return 0;
	}
	bzero((char *)pi->inBuffs[i], sizeof(EthInputBuffer));
    }
    strcpy(p->name, name);
    p->internal = pi;
    pi->activeThreads = 0;
    pi->activeHighWater = 0;
    pi->allocSize = dataSize;
    pi->numThreads = numThreads;
    pi->path = path;
    if ( ! (ev = pathAlloc(path, numThreads * sizeof(Event))) ) {
	return 0;
    }
    for ( i=0; i < numThreads; i++ ) {
	if ( ! (ev[i] = evAlloc(path)) ) {
	    while ( --i > 0 ) {
		evDetach(ev[i]);
		pathFree(ev);
		return 0;
	    }
	}
    }
    /*
     * create a pool of buffers for incoming data
     */
    xTrace1(inputpool, TR_GROSS_EVENTS,
	    "initializing %d buffers for incoming packets", numBuffers);
    for ( i=0; i < numBuffers; i++ ) {
	EthInputBuffer	*b;

	b = pi->inBuffs[i];
#if XK_DEBUG
	b->id = i;
#endif /* XK_DEBUG */
	xkr = msgConstructContig(&b->upmsg, path, dataSize, 0);
	if ( xkr == XK_FAILURE ) {
	    /* 
	     * This shouldn't happen, since the preceding
	     * msgReserve succeeded. 
	     */
	    xError("EthInPoolInit -- msgConstructAllocate fails!");
	    return 0;
	}
	msgForEach(&b->upmsg, firstDataPtr, &b->data);
	xAssert(b->data);
	b->q.next = 0;
	enlist( p->freeQueue.list, &b->q);
    }
    if ( numBuffers ) {
	p->freeQueue.empty = 0;
    }
    xTrace1(inputpool, TR_GROSS_EVENTS,
	    "initializing %d threads for incoming packets", numThreads);
    for ( i=0; i < numThreads; i++ ) {
	evDetach(evSchedule(ev[i], EthInputLoop, p, 0));
    }
    pathFree(ev);
    return p;
}



/*
 * Entry point for a pool thread
 *
 * A pool thread is activated by the driver signalling on the input queue's
 * condition variable; it processes packets from the queue until the
 * input queue is empty.
 *
 */
static void
EthInputLoop( evSelf, arg )
    Event	evSelf;
    void	*arg;
{
    EthInputPool	*myPool = arg;
    EthInputBuffer	*buffer;
    Msg         	msg;
    int			threadId;
    static int		numThreads;
    PrivPoolInfo	*info = (PrivPoolInfo  *)myPool->internal;
    xkern_return_t	xkr;
    
    xAssert(myPool->policy.func);
    xkr = myPool->policy.func(myPool->policy.arg, myPool->policy.argLen);
    if ( xkr != XK_SUCCESS ) {
	xTrace1(inputpool, TR_ERRORS,
		"input thread policy function returns %d", xkr);
    }
    threadId = ++numThreads;
    /* 
     * As an event we were started under the master lock.
     */
#ifdef XK_THREAD_TRACE
    evMarkBlocked(0);
#endif
    xk_master_unlock();
    while( TRUE ) {
	/* 
	 * Pull a buffer off the input queue
	 */
	pqRemove( myPool->inQueue, buffer );
	xkPoolCheck(myPool);
#ifdef XK_THREAD_STATS
	if ( ++info->activeThreads > info->activeHighWater ) {
	    info->activeHighWater = info->activeThreads;
	}
#endif
	xTrace1(inputpool, TR_EVENTS,
		"%d shepherd threads active", info->activeThreads);
	/* 
	 * Shepherd the message up
	 */
	msg = &buffer->upmsg;
	xTrace1(inputpool, TR_EVENTS,
		"input_process: thread %d acquires master lock", threadId);
	xk_master_lock();
#ifdef XK_THREAD_TRACE
	evMarkRunning();
#endif
	xTrace3(inputpool, TR_EVENTS,
		"input_process(%s): thread %d demux with buffer %d",
		myPool->name, threadId, buffer->id);
	xDemux(buffer->driverProtl, msg);
	/*
	 * refresh the buffer -- this must run within the lock
	 */
	while ( msgConstructContig(msg, info->path, info->allocSize, 0) == XK_FAILURE ) {
	    xTrace0(inputpool, TR_SOFT_ERRORS,
		    "constructAllocate fails in input thread loop ... sleeping");
	    /* 
	     * XXX ... Actually, maybe this thread should continue on and let
	     * someone else try to replenish the buffer pool (perhaps
	     * schedule someone?)
	     */
	    Delay( INTER_ALLOC_DELAY );
	}
	msgForEach(msg, firstDataPtr, &buffer->data);
	xAssert(buffer->data);
	
#ifdef XK_THREAD_TRACE
	evMarkBlocked(0);
#endif
	xk_master_unlock();
	/* 
	 * Return the input buffer to the free queue
	 */
	pqAppend( myPool->freeQueue, buffer );
	xkPoolCheck(myPool);
#ifdef XK_THREAD_STATS
	info->activeThreads--;
#endif
    }
}

void
EthInPoolStats( EthInputPool *p )
{
    PrivPoolInfo	*i = (PrivPoolInfo *)p->internal;

    xTrace0(inputpool, TR_ALWAYS, "xkernel input thread statistics:");
    xTrace3(inputpool, TR_ALWAYS,
	    "\tthreads: %d\tactive: %d\thigh-water: %d",
	    i->numThreads, i->activeThreads, i->activeHighWater);
}


static void
pqCheck( ProtectedQueue *pq )
{
    int			i;
    list_entry_t	elt;

    mutex_lock(pq->lock);
    for ( i=0, elt = pq->list->head; elt; elt = elt->next, i++ ) {
	EthInputBuffer	*ib = (EthInputBuffer *)elt;

	xAssert(ib);
	xTrace3(inputpool, TR_DETAILED,
		"Input buffer %d (%lx), data == %lx",
		ib->id, (u_long)ib, (u_long)ib->data);
	xAssert(ib->data);
    }
    xTrace1(inputpool, TR_DETAILED, "%d elements", i);
    mutex_unlock(pq->lock);
}


#if XK_DEBUG

void
xkPoolCheck( EthInputPool *pool )
{
    xTrace1(inputpool, TR_DETAILED, "consistency check on pool %s", pool->name);
    xTrace0(inputpool, TR_DETAILED, "  inputQueue:");
    pqCheck(&pool->inQueue);
    xTrace0(inputpool, TR_DETAILED, "  freeQueue:");
    pqCheck(&pool->freeQueue);
}

#endif
