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
 * machripc_xfer.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1991  Arizona Board of Regents
 *
 *
 * Revision: 1.7
 * Date: 1993/08/05 16:45:21
 */

#ifndef machripc_xfer_h
#define machripc_xfer_h

typedef int	NetPortNumber;

#define PORT_NUMBER_NETLEN	4

#define portNumberLoad(_p, _src)	\
	    bcopy((char *)(_src), (char *)(_p), PORT_NUMBER_NETLEN)

#define portNumberStore(_p, _dst)	\
	    bcopy((char *)(_p), (char *)(_dst), PORT_NUMBER_NETLEN)

char *	portNumStr( void );

/* 
 * Set '*npd' to the netport descriptor for the given network mach port
 * representation.  The architecture tag is given to allow decoding of
 * the network representation.  If there is no appropriate local port
 * and 'create' is true, a new local port should be created.  If no
 * local port exists and 'create' is false, XK_FAILURE is returned.
 */
extern xkern_return_t
	findNetPort(
	    mportNetRep *, mn_arch_tag_t, boolean_t create, mnetport **npd );

/*
 * Set the msgId and host name of the original right owner host
 * in the npd port structure. This is needed to identify the actual
 * right move message which comes after the move rights protocol
 * is complete.
 */
extern void
	updatePortId(mnetport *npd, msg_id_t msgId, IPhost orig_host);

/* 
 * A new send right is being sent to the indicated host and we, as the
 * holder of the receive right are to add the host to the senders list.
 * If the host is not already on the senders list, we will initialize
 * the make_send_count to the indicated value.  
 */
extern void
	addNewSender( mnetport *, IPhost, int make_send_cnt );

/* 
 * A send right which was previously transferred via 'addSendRight' is
 * being revoked.
 */
extern void
	removeSendRight( mnetport * );

/* 
 * A receive right which was previously transferred via 'addReceiveRight' is
 * being revoked.
 */
extern void
	removeReceiveRight( mnetport * );

/* 
 * Undoes the effect of a previous 'addNewSender'.
 */
extern void
	removeSender( mnetport *, IPhost );

/* 
 * RRX has put the new receiver in the receiver_host_addr field of the
 * port.  This notification causes NetIPC to register BootId interest
 * with the new receiver, etc.
 *
 * If the local host was the old holder of the receive right for this
 * port, this call indicates the time when messages can be forwarded
 * to the new receiver (port information has been transferred to the
 * new receiver but all senders are still blocked.)
 */
extern void
	receiverMoved( mnetport * );

/* 
 * Used by RRX to inform netIPC that the receive right has gone away
 * (in a way that netIPC wouldn't necessarily detect.)  
 */
extern void
	receiveRightDeallocated( mnetport * );

#if 0

/* 
 * Loads 'dst' from the (unaligned) message buffer pointed to by 'src'
 */
extern void
	mnetportLoad( char *src, mportNetRep *dst );

/* 
 * Stores 'src' into the (unaligned) message buffer pointed to by 'dst'
 */
extern void
	mnetportStore( mportNetRep *src, char *dst );

#endif

#define mnetportLoad( _src, _dst )				\
	    bcopy((_src), (char *)(_dst), MNETPORT_NETLEN)

#define mnetportStore( _src, _dst )				\
	    bcopy((char *)(_src), (_dst), MNETPORT_NETLEN)

#endif	/* !machripc_xfer_h */
