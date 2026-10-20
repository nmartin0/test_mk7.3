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
 * xfer_i.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.2
 * Date: 1993/02/01 22:35:06
 */
/* 
 * Mach Send Right Transfer protocol
 */

#ifndef xfer_i_h
#define xfer_i_h

/* 
 * Size for maps which will hold information on "in-transit" messages
 * from a specific host.
 */
#define XFER_MSG_MAP_SZ	5

/* 
 * Size for maps which are keyed on individual ports being transferred
 * in a single message 
 */
#define XFER_PD_MAP_SZ	1

/* 
 * Size for the map keyed by hosts locking ports
 */
#define XFER_LOCKED_MAP_SZ	13

/* 
 * Size for the transfer map keyed by peer hosts 
 */
#define XFER_TRANSFER_MAP_SZ	53

#endif	/* !xfer_i_h */
