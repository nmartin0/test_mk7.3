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
 * process.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * $Revision: 1.1.11.3 $
 * $Date: 1995/05/16 18:41:18 $
 */

#ifndef process_h
#define process_h

#include <xkern/include/domain.h>
#include <mach/message.h>
#include <xkern/include/xtype.h>
#include <xkern/include/xk_thread.h>

/*
 * define priorities; higher numbers have less priority
 */

#if MACH_RT
#define XK_THREAD_PRIORITY_MIN		96  /* BASPRI_USER */
#define XK_THREAD_PRIORITY_MAX		126 /* LOWPRI_USER */
#else
#define XK_THREAD_PRIORITY_MIN		12  /* BASPRI_USER */
#define XK_THREAD_PRIORITY_MAX		32 /* LOWPRI_USER */
#endif

#define XK_INPUT_THREAD_PRIORITY_MIN	XK_THREAD_PRIORITY_MIN
#define XK_INPUT_THREAD_PRIORITY_MAX	XK_THREAD_PRIORITY_MAX
#define	XK_INPUT_THREAD_PRIORITY	(XK_THREAD_PRIORITY_MIN + (XK_THREAD_PRIORITY_MAX - XK_THREAD_PRIORITY_MIN)/2)
#define	XK_CLOCK_THREAD_PRIORITY	XK_THREAD_PRIORITY_MIN
#define	XK_EVENT_THREAD_PRIORITY	XK_THREAD_PRIORITY_MIN
#define	XK_DRIVER_THREAD_PRIORITY	XK_THREAD_PRIORITY_MIN


#ifndef NULL
#define NULL 0
#endif


extern void	Delay( int );

typedef struct sxkSemaphore {
	struct mutex		lock;
	struct condition	cond;
	int			count;
	int			sleepers;
} xkSemaphore;

#define semWait(S) { if (--(S)->count < 0) realP(S); }
#define semSignal(S) { if (++(S)->count <= 0) realV(S); }

extern void semInit( xkSemaphore *, unsigned int );
extern void realP( xkSemaphore * );
extern void realV( xkSemaphore * );

/*
 * The -DXKLOCKDEBUG switch provides for counting the locking depth, and
 * complaining if it isn't 1.  It has a bug, in that the increment & decrement
 * instructions aren't atomic in RISC architectures.  Presumably, the occasions
 * where this fails are rare.
 */
#ifdef XKLOCKDEBUG
extern int xklockdepthreq;
extern int xklockdepth;
extern int tracexklock;
#endif

/*
 * the master concurrency locks
 */
#ifndef XKLOCKDEBUG

#define MASTER_LOCK	mutex_lock(sledgehammer_concurrency_control)
#define MASTER_UNLOCK	mutex_unlock(sledgehammer_concurrency_control)

#else /* XKLOCKDEBUG */

#define	MASTER_LOCK							      \
{xklockdepthreq++;							      \
 xTrace1(xklock,TR_EVENTS,"requesting xklock, depthreq %d",xklockdepthreq);   \
 mutex_lock(sledgehammer_concurrency_control);				      \
 xklockdepth++;								      \
 if (xklockdepth!=1)							      \
 { xTrace1(xklock,TR_ERRORS,"got xklock, wrong depth %d",xklockdepth); };     \
 xTrace2(xklock,TR_EVENTS,"got xklock, depth %d, depthreq %d",		      \
	 xklockdepth,xklockdepthreq); }
#define	MASTER_UNLOCK							      \
{if (xklockdepth!=1)							      \
 {xTrace1(xklock,TR_ERRORS,"giving up xklock, wrong depth %d",xklockdepth); };\
 xTrace2(xklock,TR_EVENTS,"giving up xklock, depth %d, depthreq %d",	      \
	 xklockdepth,xklockdepthreq);					      \
 xklockdepth--;								      \
 mutex_unlock(sledgehammer_concurrency_control);			      \
 xTrace1(xklock,TR_EVENTS,"gave up xklock, depthreq %d",xklockdepthreq);      \
 xklockdepthreq--; }

#endif /* XKLOCKDEBUG */

typedef void (*gen_func) (void *, ...);

xkern_return_t	CreateProcess( gen_func, Event ev, int numArgs, ... );
void	Yield( void );
xkern_return_t	threadInit( void );
void	anchorsInit( void );
void	xk_master_lock( void );
void	xk_master_unlock( void );
void	xkIncreaseCthreadLimit( int );
void	VAll( xkSemaphore * );

#define xk_thread_checkin		xk_master_lock
#define xk_thread_checkout( arg )	xk_master_unlock()

xkern_return_t	procAllocState( Event, Allocator );
void		procFreeState( Event );

XkThreadPolicyFunc	xkPolicyFixedRR;
XkThreadPolicyFunc	xkPolicyFixedFifo;
#endif
