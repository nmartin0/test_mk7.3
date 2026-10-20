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
 *      File:   netmem_defs.h
 *      Author: David L. Black
 *
 *      External definitions for netmem_pager.
 */

#include <mach/error.h>

/*
 *	Name of the pager for the nameservice.
 */

#define	NETMEM_NAME		"netmem_pager"
#define NETMEM_NAME1            "netmem_pager1"

/*
 *	Failure codes.  Use user-reserved errors.
 */

#define NETMEM_BAD_OBJECT	(err_local|err_sub(13)|1)
#define NETMEM_BAD_OFFSET	(err_local|err_sub(13)|2)
#define NETMEM_FAILURE		(err_local|err_sub(13)|3)
