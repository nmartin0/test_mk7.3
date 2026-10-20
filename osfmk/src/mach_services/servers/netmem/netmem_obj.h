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
 *      File:   netmem_obj.h
 *      Author: David L. Black
 *
 *      Internal definitions for netmem_msg server.
 */

#include <mach.h>

/*
 *	Possible object states
 */
#define	NETMEM_OBJ_NONE		1	/* No memory exists for the object */
#define NETMEM_OBJ_READ 	2	/* Server has read only copy, kernel
				   	   may have read only copy */
#define NETMEM_OBJ_SERVER_WRITE	3	/* Server has writable copy, no
					   kernel copy */
#define NETMEM_OBJ_KERNEL_WRITE 4	/* Kernel may have writable copy,
					   server has no-access copy */
#define NETMEM_OBJ_CLEANING	5	/* Pending request to kernel to
					   clean page and set read-only */
#define NETMEM_OBJ_FLUSHING	6	/* Pending request to kernel to
					   clean and flush page */
/*
 *	Structure for a pending message operation
 */
#define PENDING_READ	1
#define PENDING_WRITE	2

struct	pending_op {
	struct pending_op	*next;
	int			op;		/* read or write */
	vm_address_t		offset;
	int			value;
	mach_port_t		reply_port;
};

typedef struct pending_op	*pending_op_t, pending_op_data_t;

#define PENDING_OP_NULL	((pending_op_t) 0)

/*
 *	Object data structure
 */
struct netmem_obj {
	mach_port_t		object_port;	/* represents object */
	mach_port_t		control_port;   /* kernel control port */
	int			send_rights;    /* control send right count */
	int			state;		/* who can access how */
	vm_address_t		data;		/* pointer to contents */

	/*
	 *	Next two fields used only in cleaning and flushing states.
	 */
	pending_op_t		pending_op_queue;	/* pending msg ops */
	vm_prot_t		pending_kernel_access;  /* pending kernel op */
	int			pending_kernel_type;    /* which kernel op */
#define PENDING_TYPE_LOCK	1
#define PENDING_TYPE_DATA	2
#if	MOR
	mach_port_t		representative;	/* memory object represent. */
	mach_port_t		representative1;/* memory object represent. */
#endif
};

typedef struct netmem_obj	*netmem_obj_t, netmem_obj_data_t;

#define NETMEM_OBJ_NULL		((netmem_obj_t) 0)
