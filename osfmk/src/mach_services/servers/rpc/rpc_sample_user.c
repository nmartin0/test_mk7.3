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
 * IDENTIFICATION:
 * stub generated Tue Apr 12 17:12:56 1994
 * with a MiG generated Thu Jan 20 14:05:30 EST 1994 by condict@wilma
 * OPTIONS: 
 */
#include "rpc_sample_user.h"
/* LINTLIBRARY */


#ifndef	mig_internal
#define	mig_internal	static
#endif	/* mig_internal */

#ifndef	mig_external
#define mig_external
#endif	/* mig_external */

#ifndef	TypeCheck
#define	TypeCheck 1
#endif	/* TypeCheck */

#ifndef	min
#define	min(a,b)  ( ((a) < (b))? (a): (b) )
#endif	/* min */

#ifndef	UseStaticTemplates
#define	UseStaticTemplates	1
#endif	/* UseStaticTemplates */

#define msgh_request_port	msgh_remote_port
#define msgh_reply_port		msgh_local_port


long out;
#define PRINT_EBP						\
		__asm__("movl %%ebp, %0" : "=r" (out) :);	\
		printf("test_null: ebp is 0x%x\n", out);


/* Routine test_null */
mig_external kern_return_t test_null
(
	mach_port_t object
)
{
	/* Post-Process the IN and OUT arguments: */

#if 0
        rtn = RPC_SIMPLE(object, 600, 1, (object));
#else
	return  mach_rpc_trap(object, 600, 0, 0);
#endif
}

/* Routine test_simple */
mig_external kern_return_t test_simple
(
	mach_port_t object,
	int arg_one,
	int arg_two
)
{

#if 0
	rtn = RPC_SIMPLE(object, 601, 3, (object, arg_one, arg_two));
#else
	return mach_rpc_trap(object, 601, 0, 0);
#endif

}

