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
 * init.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * init.c,v
 * Revision 1.36.1.6  1994/09/02  21:44:00  menze
 * build_pgraph is run from within an x-kernel event
 *
 * Revision 1.36.1.5  1994/09/01  04:21:49  menze
 * Subsystem initialization functions can fail
 * Thread and event init before alloc and path init
 *
 * Revision 1.36.1.4  1994/07/21  23:39:56  menze
 * Added calls to allocBootStrap and pathInit
 *
 * Revision 1.36.1.3  1994/05/06  00:51:50  hkaram
 * Added definition of rom array for inkernel portion
 *
 * Revision 1.36.1.2  1994/04/01  16:55:53  menze
 * Added call to allocInit
 *
 * Revision 1.36.1.1  1994/03/30  22:40:13  menze
 * no longer calls inputThreadInit
 *
 * Revision 1.36  1994/02/05  00:30:04  menze
 *   [ 1994/01/14          menze ]
 *   Moved rom initialization stuff to pi/initrom.c
 *
 */

#include <xkern/include/domain.h>
#include <xkern/include/upi.h>
#include <xkern/include/xk_debug.h>
#include <xkern/include/platform.h>
#include <xkern/include/netmask.h>
#include <xkern/include/prottbl.h>
#include <xkern/include/xk_path.h>
#include <xkern/include/event.h>

/*
 * the event scheduling interval granularity
 */
int  event_granularity = 100; 

int traceinit = 0;

extern build_pgraph();

int globalArgc;
char **globalArgv;


#define try(funcCall, str) \
{ \
   if ( funcCall == XK_FAILURE ) { \
       sprintf(errBuf, "Fatal x-kernel initialization error in %s module", \
	       str); \
       xError(errBuf); \
       xError("x-kernel initialization halted"); \
       return XK_FAILURE; \
   } \
}


static void
build_pgraph_stub( Event ev, void *arg )
{
    build_pgraph();
}


xkern_return_t
xkInit(
    int		argc,
    char	*argv[])
{
    setbuf(stdout, 0);
    globalArgc = argc;
    globalArgv = argv;

    try( allocBootStrap(), "allocator bootstrap" );
    try( pathBootStrap(), "path bootstrap" );
    try( xTraceInit(), "trace" );
    xTrace0(init, TR_EVENTS, "Calling initRom");
    try( initRom(), "rom" );
    try( map_init(), "map" );
    try( threadInit(), "thread" );
    try( evInit(event_granularity), "event" );
    try( allocInit(), "allocator" );
    try( pathInit(), "path" );
    try( msgInit(), "msg" );
    try( netMaskInit(), "netmask" );
    try( prottbl_init(), "protocol table" );
    try( upiInit(), "UPI" );
    try( xk_clock_init(), "xk clock" );	/* clock_profile utility */

    xTrace1(init, TR_EVENTS,
	    "Creating thread to build protocol graph (func %x)",
	    build_pgraph );
    build_pgraph_dev();
    {
	Event ev;

	if ( ev = evAlloc(xkSystemPath) ) {
	    evDetach(evSchedule(ev, build_pgraph_stub, 0, 0 ));
	} else {
	    xError("x-kernel couldn't allocate graph initialization event");
	}
    }
    xTrace0(init, TR_EVENTS, "init thread exits");
    cthread_exit((void *)0);
    return XK_SUCCESS;
}

