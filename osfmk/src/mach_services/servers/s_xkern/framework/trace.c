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
 * trace.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * trace.c,v
 * Revision 1.4.1.2  1994/09/01  04:20:51  menze
 * Subsystem initialization functions can fail
 */

#include <xkern/include/platform.h>
#include <xkern/include/xk_debug.h>
#include <xkern/include/compose.h>
			 
#ifdef XK_TRACE_LOCKING
static mutex_t lock;
#endif

void
xTraceLock()
{
#ifdef XK_TRACE_LOCKING
    mutex_lock( lock );
#endif
}


void
xTraceUnlock()
{
#ifdef XK_TRACE_LOCKING
    mutex_unlock( lock );
#endif
}


xkern_return_t
xTraceInit()
{

#ifdef XK_TRACE_LOCKING
    if ( ! (lock = mutex_alloc()) ) {
	return XK_FAILURE;
    }
#endif

    initTraceLevels();
    return XK_SUCCESS;
}
