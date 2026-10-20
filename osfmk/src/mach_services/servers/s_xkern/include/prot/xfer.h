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
 * xfer.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.10
 * Date: 1993/02/01 22:35:17
 */
/* 
 * Mach Port Transfer protocols
 */
/* 
 * This module (XFER) provides some support routines by the Mach port
 * transfer protocols.  These routines are used by both SRX, the send
 * right transfer protocol, and RRX, the receive right transfer
 * protocol.  
 *
 * Maps -- Each of the protocols should keep two maps, one for ports
 * which are locked while a transfer is occurring to a third-party
 * host (a 'lockedMap') and one for ports which are being transferred
 * to the local host (a 'transferMap'.)  These maps are searched when
 * a remote host reboots to determine whether ports should be unlocked
 * or transfers aborted.
 *
 * The transfer protocols should be oblivious to the structure (keys,
 * mappings, etc.) of the maps.
 */

#ifndef xfer_h
#define xfer_h

#include <xkern/include/prot/bidctl.h>
#include <s_xkern/include/prot/machripc.h>

/* 
 * Protocols will occasionally pass their 'self' objects into these
 * routines.  The protocols must have lower protocols providing
 * Transport service and BootId Control service, and these lower
 * protocols must be in these positions in the down vector.
 */
#define	XFER_XPORT_I	0
#define	XFER_BIDCTL_I	1

typedef struct {
    IPhost	h;
    BootId	bid;
} XferHost;

#define XFERHOST_NETLEN	(sizeof(IPhost) + sizeof(BootId))

#define xferHostLoad(_xh, _src) 				\
    {								\
	bcopy((_src), (char *)&(_xh)->h, sizeof(IPhost));	\
	bcopy((_src) + sizeof(IPhost), (char *)&(_xh)->bid, sizeof(BootId)); \
    }

#define xferHostStore(_xh, _dst) 				\
    {								\
	bcopy((char *)&(_xh)->h, (_dst), sizeof(IPhost));	\
	bcopy((char *)&(_xh)->bid, (_dst) + sizeof(IPhost), sizeof(BootId)); \
    }

typedef int	SendCount;

#define SENDCOUNT_NETLEN	4

/* 
 * Open a transport session to the indicated peer, setting the
 * BootId pointer parameter to the peer's current BootId.  The open will
 * fail if:
 *
 * 	1) the transport open fails
 *	2) the host reboots while opening the transport channel.  
 *
 * The 'self' XObj is assumed to have bidctl and xport lower protocols
 * indexed using XFER_BIDCTL_I and XFER_XPORT_I
 */
XObj	xferOpen( XObj, IPhost *, BootId * );

/* 
 * Returns true if the current BootId of the peer matches the given BootId. 
 */
int	xferConfirmBid( XObj, XferHost * );

/* 
 * Initializes a protocol's maps (see above description)
 */
void	xferCreateMaps( Map *lockedMap, Map *xferMap );

/* 
 * Add the port 'p' to the 'locked map' (locked by peer 'h')
 * and register 'protl's interest with the BootId protocol.  The port
 * is bound to 'arg' which can be returned via an XferLockCallBack
 * function in xferLockedMapRemove.
 */
void	xferLockedMapAdd( XObj protl, Map, IPhost *h, mnetport *p, void *arg );

typedef 	void	(* XferRemFunc)( mnetport * );

typedef 	void	(* LockRemFunc)( mnetport *, void * );

/* 
 * Remove a port which was previously added with xferLockedMapAdd.
 * The callback function (if non-null) will be called with the port
 * and the argument to which it was bound.
 */
void	xferLockedMapRemove( Map, IPhost *, mnetport *, LockRemFunc );

/* 
 * The transfer protocols themselves, rather than this (XFER) module, receive
 * and process reboot notifications.  This function is used to tell
 * the XFER module to go through the maps, unlocking ports in 'lockedMap'
 * calling 'rf' for ports in the transfer map.
 */
void	xferPeerRebooted(
	    XObj, IPhost *, Map lockedMap, Map xferMap,
	    XferRemFunc xferFn, LockRemFunc lockedFn );

/* 
 * Add the port 'p' to the 'transfer map', awaiting receipt of msgId
 * 'id'.  Register 'protl's interest with the BootId protocol.
 */
void	xferTransferMapAdd( XObj protl, Map, IPhost *h, MsgId, mnetport *p );

/* 
 * Used to notify XFER that message 'id' has arrived from host 'h'.
 */
void	xferTransferMapRemove( Map, IPhost *h, MsgId id );

char *	xferHostStr( XferHost * );

#endif	/* !xfer_h */

