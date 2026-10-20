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
 * rrx_hdr.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.11
 * Date: 1993/02/01 22:34:55
 */
/* 
 * Routines to load and store RRX messages.  This file also contains
 * descriptions of the network RRX message representations.
 */

#include <xkern/include/xkernel.h>
#include <s_xkern/protocols/machnetipc/rrx_i.h>


static long
	loadXferMsg( void *, char *, long, void * );

static long
	loadReq( void *, char *, long, void * );

static long
	loadUnlockMsg( void *, char *, long, void * );

static void
	rrxReqDisplay( RrxReqMsg * );

static void
	storeRequest( void *, char *, long, void * );


/* 
 * RRX reply message network format (4 bytes):
 *
 *	|--------|--------|--------|--------|
 *	|  type	 |  code  |     unused	    |
 *	|--------|--------|--------|--------|
 */

long
rrxLoadReply(
	void	*hdr,
	char 	*src,
	long 	len,
	void 	*arg)
{
    xAssert( len == RRX_REPMSG_NETLEN );
    ((RrxRepMsg *)hdr)->type = (RrxMsgType)src[0];
    ((RrxRepMsg *)hdr)->replyCode = (RrxReplyCode)src[1];
    return len;
}


void
rrxStoreReply(
	void	*hdr,
	char 	*dst,
	long 	len,
	void 	*arg)
{
    xAssert( len == RRX_REPMSG_NETLEN );
    dst[0] = (char)((RrxRepMsg *)hdr)->type;
    dst[1] = (char)((RrxRepMsg *)hdr)->replyCode;
    dst[2] = dst[3] = 0;
}



/* 
 * RRX request message network format (message-type-independent part)
 *
 *	|--------|--------|--------|--------|
 *	|  type	 | unused |   # of senders  |	# of senders only used for
 *	|--------|--------|--------|--------|	TRANSFER messages
 * 	|	  Architecture tag ...	    |
 *	|--------|--------|--------|--------|	
 * 	|	    network port ...	    |
 *	|--------|--------|--------|--------|
 *
 * The remainder of the header depends on the type of the message
 *
 */

/* 
 * Network length of the type-independent part of a request message 
 */
#define RRX_REQMSG_NETLEN	(4 + MN_ARCH_TAG_NETLEN + PORT_NUMBER_NETLEN)


/*
 * Remainder of request message for a TRANSFER request
 *
 *	|--------|--------|--------|--------|
 *	|  		msgId		    |
 *	|--------|--------|--------|--------|
 * 	|	      ORR IPhost	    |	
 *	|--------|--------|--------|--------|	
 * 	|	      ORR BootID	    |
 *	|--------|--------|--------|--------|
 * 	|	  sender IPhost #1	    |	
 *	|--------|--------|--------|--------|	
 * 	|	    sender BID #1	    |
 *	|--------|--------|--------|--------|
 * 	|     sender make-send-count #1	    |
 *	|--------|--------|--------|--------|
 * 	|	  sender IPhost #2	    |
 *	|--------|--------|--------|--------|
 * 	|	    sender BID #2	    |
 *	|--------|--------|--------|--------|
 * 	|     sender make-send-count #2	    |
 *	|--------|--------|--------|--------|
 * 	|		 ...	    	    |
 */

#define RRX_SENDHOST_NETLEN	(XFERHOST_NETLEN + SENDCOUNT_NETLEN)

static void
rrxSendHostLoad(
	RrxSendHost	*sh,
	char		*src)
{
    xferHostLoad(&sh->xh, src);
    bcopy(src + XFERHOST_NETLEN, (char *)&sh->sendCount, SENDCOUNT_NETLEN);
}

static void
rrxSendHostStore(
	RrxSendHost	*sh,
	char		*dst)
{
    xferHostStore(&sh->xh, dst);
    bcopy((char *)&sh->sendCount, dst + XFERHOST_NETLEN, SENDCOUNT_NETLEN);

}


/* 
 * Network length of the transfer-specific part of a request message
 */
#define RRX_XFERMSG_NETLEN(numSenders)	\
	( MSGID_NETLEN + ((numSenders) + 1) * RRX_SENDHOST_NETLEN )

static long
loadXferMsg(
	void	*hdr,
	char 	*src,
	long 	len,
	void 	*arg)
{
#define HDR	((RrxReqMsg *)hdr)

    RrxSendHost	*hosts;
    int		i;

    bcopy(src, (char *)&HDR->u.transfer.msgId, MSGID_NETLEN);
    src += MSGID_NETLEN;
    xferHostLoad(&HDR->u.transfer.orr, src);
    src += XFERHOST_NETLEN;
    xTrace1(rrxp, TR_MORE_EVENTS,
	"loading %d senders", HDR->u.transfer.numSenders);
    if ( HDR->u.transfer.numSenders ) {
	hosts = (RrxSendHost *)
	    xMalloc( HDR->u.transfer.numSenders * sizeof(RrxSendHost) );
	for (i = 0;
	     i < HDR->u.transfer.numSenders;
	     i++, src += RRX_SENDHOST_NETLEN ) {
	    rrxSendHostLoad(&hosts[i], src);
	}
    } else {
	hosts = 0;
    }
    HDR->u.transfer.senders = hosts;
    return len;

#undef HDR
}


xkern_return_t
rrxLoadRequest(
	RrxReqMsg	*hdr,
	Msg		*msg)
{
    long	len = RRX_REQMSG_NETLEN;

    xIfTrace(rrxp, TR_FULL_TRACE) {
	/* 
	 * Ensure the entire message body is contiguous to ease debugging. 
	 */
	len = msgLen(msg);
    }
    /* 
     * Load the message-type-independent part first
     */
    if ( ! msgPop(msg, loadReq, hdr, len, 0) ) {
	return XK_FAILURE;
    }
    /* 
     * Now the message-type-specific part
     */
    switch ( hdr->type ) {
	case RECEIVE_RIGHT_TRANSFER:
	    if (!msgPop(msg, loadXferMsg, hdr,
		    RRX_XFERMSG_NETLEN(hdr->u.transfer.numSenders), 0) ) {
		return XK_FAILURE;
	    }
	    break;

	case RECEIVE_UNLOCK_REQ:
	    if ( ! msgPop(msg, loadUnlockMsg, hdr, XFERHOST_NETLEN, 0) ) {
		return XK_FAILURE;
	    }
	    break;

	default:
	    break;
    }
    xIfTrace(rrxp, TR_DETAILED) {
	rrxReqDisplay(hdr);
    }
    return XK_SUCCESS;
}


/* 
 * Load the message-type-independent part of the request
 */
static long
loadReq(
	void	*hdr,
	char 	*src,
	long 	len,
	void 	*arg)
{
#define HDR	((RrxReqMsg *)hdr)

    xAssert( len >= RRX_REQMSG_NETLEN );
    xIfTrace(rrxp, TR_FULL_TRACE) {
	int	i;

	xTrace0(rrxp, TR_ALWAYS, "incoming message dump:");
	for ( i=0; i < len; i++ ) {
	    printf("%3x%c", (u_char)src[i], ((i+1) % 16) ? ' ' : '\n');
	}
	printf("\n");
    }
    HDR->type = (RrxMsgType)src[0];
    if ( HDR->type == RECEIVE_RIGHT_TRANSFER ) {
	bcopy(&src[2], (char *)&HDR->u.transfer.numSenders, 2);
	HDR->u.transfer.numSenders = ntohs(HDR->u.transfer.numSenders);
    }
    src += 4;
    bcopy(src, (char *)&HDR->archTag, MN_ARCH_TAG_NETLEN);
    src += MN_ARCH_TAG_NETLEN;
    portNumberLoad(&HDR->portNumber, src);
    return RRX_REQMSG_NETLEN;

#undef HDR
}


/*
 * Remainder of request message for UNLOCK messages
 *
 *	|--------|--------|--------|--------|
 *	|  	       nrrHost		    |
 *	|--------|--------|--------|--------|
 *	|	       nrrBid		    |
 *	|--------|--------|--------|--------|
 */

static long
loadUnlockMsg(
	void	*hdr,
	char 	*src,
	long 	len,
	void 	*arg)
{
#define HDR	((RrxReqMsg *)hdr)

    xAssert( len >= XFERHOST_NETLEN );
    xferHostLoad(&HDR->u.unlock.nrr, src);
    return XFERHOST_NETLEN;

#undef HDR
}


void
rrxStoreRequest(
	RrxReqOutgoing	*hdr,
	Msg		*msg)
{
    int		len = RRX_REQMSG_NETLEN;
    int		numSenders = 0;
    ListElement	*e;

    switch ( hdr->type ) {
	case RECEIVE_RIGHT_TRANSFER:
	    for ( e = hdr->u.transfer.senders->next;
		  e != hdr->u.transfer.senders;
		  e = e->next ) {
		if ( e->valid ) {
		    numSenders++;
		}
	    }
	    len += RRX_XFERMSG_NETLEN(numSenders);
	    break;

	case RECEIVE_UNLOCK_REQ:
	    len += XFERHOST_NETLEN;
	    break;

	default:
	    break;
    }
    msgPush(msg, storeRequest, hdr, len, 0);
}


static void
storeRequest(
	void	*hdr,
	char 	*dst,
	long 	len,
	void 	*arg)
{
#define HDR	((RrxReqOutgoing *)hdr)
    char		*origDst = dst;
    mn_arch_tag_t	archTag;

    dst[0] = (u_char)HDR->type;
    dst[1] = 0;
    dst += 4;
    archTag = MN_ARCH_MARKER;
    bcopy((char *)&archTag, dst, MN_ARCH_TAG_NETLEN);
    dst += MN_ARCH_TAG_NETLEN;
    portNumberStore(&HDR->port->net_port_number, dst);
    dst += PORT_NUMBER_NETLEN;
    switch ( HDR->type ) {
	case RECEIVE_RIGHT_TRANSFER: {
	    ListElement	*e;
	    u_short	numSenders = 0;

	    bcopy((char *)&HDR->u.transfer.msgId, dst, MSGID_NETLEN);
	    dst += MSGID_NETLEN;
	    xferHostStore(&HDR->u.transfer.orr, dst);
	    dst += XFERHOST_NETLEN;
	    for ( e = HDR->u.transfer.senders->next;
		  e != HDR->u.transfer.senders;
		  e = e->next ) {
		if ( e->valid ) {
		    rrxSendHostStore(&e->sh, dst);
		    dst += RRX_SENDHOST_NETLEN;
		    numSenders++;
		}
	    }
	    numSenders = htons(numSenders);
	    bcopy((char *)&numSenders, origDst + 2, 2);
	    break;
	}
	case RECEIVE_UNLOCK_REQ:
	    xferHostStore(&HDR->u.unlock.nrr, dst);
	    /*
	     * fallthrough
	     */
	default:
	    origDst[2] = origDst[3] = 0;
	    break;
    }
    xIfTrace(rrxp, TR_FULL_TRACE) {
	int	i;

	xTrace0(rrxp, TR_ALWAYS, "outgoing message dump:");
	for ( i=0; i < len; i++ ) {
	    printf("%3x%c", (u_char)origDst[i], ((i+1) % 16) ? ' ' : '\n');
	}
	printf("\n");
    }
#undef HDR
}


/* 
 * description in rrx_i.h
 */
void
rrxReqDispose(
	RrxReqMsg	*h)
{
    if ( h->type == RECEIVE_RIGHT_TRANSFER && h->u.transfer.senders ) {
	xFree((char *)h->u.transfer.senders);
    }
}


static void
rrxReqDisplay(
	RrxReqMsg	*h)
{
    xTrace3(rrxp, TR_ALWAYS,
	"RRX %s request, port %d, arch tag %x",
	rrxMsgTypeStr(h->type), h->portNumber, (u_int)h->archTag);
    switch( h->type ) {
	case RECEIVE_LOCK_REQ:
	break;

	case RECEIVE_RIGHT_TRANSFER: {
	    int		i;

	    xTrace2(rrxp, TR_ALWAYS,
		"Message ID: %d,  Old receiver: %s",
		h->u.transfer.msgId, ipHostStr(&h->u.transfer.orr.h));
	    xTrace0(rrxp, TR_ALWAYS, "Senders:");
	    for ( i=0; i < h->u.transfer.numSenders; i++ ) {
		xTrace2(rrxp, TR_ALWAYS, "    %s, send-count: %d",
		    xferHostStr(&h->u.transfer.senders[i].xh),
		    h->u.transfer.senders[i].sendCount);
	    }
	    break;
	}
	case RECEIVE_UNLOCK_REQ:
	    xTrace1(rrxp, TR_ALWAYS,
		"New receiver: %s", xferHostStr(&h->u.unlock.nrr));
	    break;
    }
}

