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
 * ssr_internal.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.4
 * Date: 1993/02/02 00:21:06
 */
/*
 * global data for SSR; need data structure for assigned/free port nums
 */

#ifndef	ssr_internal_h
#define	ssr_internal_h

int	num_registered_services = 0;

/*
 * the protocol state
 */
struct ssr_state {
    mach_port_t	wk_port;
    mach_port_t	wk_port_send_right;
    XObj	session;
    mach_port_t	*service_map;
    Map		service_map_hash;
};

/*
 * The map might someday be keyed on the service_id and reply_port
 */
typedef struct {
    mach_port_type_t	reply_port;
    int			service_id;
    Sessn		sessn;
} SSR_ActiveId;

#define SS_MAP_SIZE	100

#endif	/* ssr_internal_h */
