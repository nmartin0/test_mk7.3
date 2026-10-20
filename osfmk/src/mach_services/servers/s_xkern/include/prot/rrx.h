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
 * rrx.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.7
 * Date: 1993/02/01 22:34:29
 */
/* 
 * Mach Receive Right Transfer protocol (RRX)
 *
 * Interface description
 */

#ifndef rrx_h
#define rrx_h

void	rrx_init( XObj );

/* 
 * Moves the receive rights in 'np' (a null-terminated array of
 * mnetport pointers) to host 'h' pending delivery of message 'id'.
 */
xkern_return_t
	rrxMoveReceiveRights( IPhost h, MsgId id, mnetport **np );

/* 
 * Used to notify RRX that a message with ports transferred by RRX has
 * arrived.  
 */
void	rrxTransferComplete( IPhost , MsgId );

#endif	/* !rrx_h */

