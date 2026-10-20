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


#include <mach.h>
#include <stdio.h>
#include <mach_init.h>
#include <mach/rpc.h>
#include <mach/sync.h>	/*** to support wa_printf() ***/
#include <mig_errors.h>
#include <servers/netname.h>

#include "rpc_defs.h"
#include "rpctestUser.h"
#include "rpctestServer.h"

extern kern_return_t process_command(mach_port_t, function_table_entry *, char *);
extern boolean_t is_from_remote(mach_port_t);
extern function_table_entry rpctest_client_functions[];
extern int rprintf(char *, ...);
extern mach_port_t local_rpc_port();
extern char *get_int_arg(char *, int *);
extern char *get_string_arg(char *, char **);
extern char *get_port_arg(char *, mach_port_t *);

extern char *taskname;
extern boolean_t quit_rpctest;

#define return_from_rpc(p,v) 			    \
if (is_from_remote(p)) 				    \
{ 				                    \
  __asm__ volatile ("movl %0, %%ecx" : : "g" (v) ); \
  mach_rpc_return_trap();            		    \
}		         			    \
else                           			    \
{                               		    \
  __asm__ volatile ("movl %0, %%ecx" : : "g" (v) ); \
  return; 					    \
}

/* define various levels of debugging */
/*** this should be moved to someplce reasonable -- rkc, 9/16/94 ***/
#define DEBUG_LEVEL_NONE		( 0 )
#define DEBUG_LEVEL_TRACE		( DEBUG_LEVEL_NONE + 1 )
#define DEBUG_LEVEL_DETAILED_INFO	( DEBUG_LEVEL_TRACE + 1 )
#define DEBUG_LEVEL_MORE_DETAILED_INFO	( DEBUG_LEVEL_DETAILED_INFO + 1 )

#define debug_printf(level) if (debug >= level) rprintf

extern int debug;

kern_return_t
call(mach_port_t object, char *args, mach_msg_type_number_t length)
{
    mach_port_t target;

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: call(%x \"%s\")\n", taskname, object, args);

    args = get_port_arg(args, &target);
    if (is_from_remote(object))
	new_activation();
	
    rprintf("%s: activation %s calling...\n",
	    taskname, rthread_get_name(rthread_self()));
    process_command(target, rpctest_client_functions, args);
    debug_printf(DEBUG_LEVEL_TRACE)
	("  %s: Call done.\n", taskname);
    return_from_rpc(object, KERN_SUCCESS);
}

kern_return_t
spin(mach_port_t object, char *args, mach_msg_type_number_t length)
{
    int count;

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: spin(%x \"%s\")\n", taskname, object, args);

    args = get_int_arg(args, &count);
    count = count > 0 ? count : -1;

    if (is_from_remote(object))
	new_activation();
	
    debug_printf(DEBUG_LEVEL_TRACE)
	 ("  %s: Spin count = %d\n", taskname,  count);

    rprintf("%s: activation %s spinning...\n",
	    taskname, rthread_get_name(rthread_self()));
    while (count)
    {
	if (count > 0)
	    count--;
    }
    rprintf("  %s: activation %s done spinning\n",
	    taskname, rthread_get_name(rthread_self()));

    process_command(local_rpc_port(), rpctest_client_functions, args);

    return_from_rpc(object, KERN_SUCCESS);
}

kern_return_t
kill(mach_port_t object, char *args, mach_msg_type_number_t length)
{
    mach_port_t target;
    kern_return_t kr;

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: kill(%x \"%s\")\n", taskname, object, args);

    args = get_port_arg(args, &target);
    if (is_from_remote(object))
	new_activation();

    kr = thread_terminate(target);
    process_command(local_rpc_port(), rpctest_client_functions, args);
	
    return_from_rpc(object, kr);
}

kern_return_t
term(mach_port_t object, char *args, mach_msg_type_number_t length)
{
    mach_port_t target;
    kern_return_t kr;

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: term(%x \"%s\")\n", taskname, object, args);

    args = get_port_arg(args, &target);
    if (is_from_remote(object))
	new_activation();

    kr =  task_terminate(target);
    process_command(local_rpc_port(), rpctest_client_functions, args);

    return_from_rpc(object, kr);
}

kern_return_t
nop(mach_port_t object, char *args, mach_msg_type_number_t length)
{
    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: nop(%x \"%s\")\n", taskname, object, args);

    process_command(local_rpc_port(), rpctest_client_functions, args);

    return_from_rpc(object, KERN_SUCCESS);
}

kern_return_t
quit(mach_port_t object, char *args, mach_msg_type_number_t length)
{
    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: quit(%x \"%s\")\n", taskname, object, args);

    quit_rpctest = TRUE;
    return_from_rpc(object, KERN_SUCCESS);
}

