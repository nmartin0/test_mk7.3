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
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * cmk1.1
 */

#ifndef	_MACHID_INTERNAL_H_
#define	_MACHID_INTERNAL_H_

#include <mach/kern_return.h>
#include <mach/port.h>
#include <servers/machid_types.h>

#define DEFAULT_NODE 0

typedef enum mach_op {
    mo_Port,		/* get the object's port - restricted */
    mo_Write,		/* "write" the object - restricted */
    mo_Read,		/* "read" the object - restricted */
    mo_Info		/* get "info" about the object - not restricted */
} mach_op_t;

#define Debug(arglist)	if (debug) printf arglist

extern boolean_t debug;

extern mach_id_t name_lookup(mach_port_t, mach_type_t);
extern kern_return_t port_lookup(mach_id_t, mach_port_t, mach_op_t,
				 mach_port_t *);
extern mach_type_t type_lookup(mach_id_t);
extern void port_consume(mach_port_t);
extern void port_destroy(mach_port_t);
extern void assoc_create(mach_id_t, mach_type_t, mach_id_t);
extern mach_id_t assoc_lookup(mach_id_t, mach_type_t);
extern void auth_consume(mach_port_t);
extern void name_node_set(mach_id_t, int);
extern int  name_node_get(mach_id_t);






#endif	/* _MACHID_INTERNAL_H_ */
