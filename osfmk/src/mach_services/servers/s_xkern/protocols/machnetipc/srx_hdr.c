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
 * srx_hdr.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.11
 * Date: 1993/02/01 22:34:44
 */
/* 
 * Mach Send Right Transfer protocol (SRX)
 */

#include <xkern/include/xkernel.h>
#include <s_xkern/protocols/machnetipc/srx_i.h>

/* 
 * Functions to load and store SRX messages
 */
static long	loadLockRep( void *, char *, long, void * );

static long	loadLockReq( void *, char *, long, void * );

static long	loadRep( void *, char *, long, void * );

static long	loadReq( void *, char *, long, void * );

static long	loadXferReq( void *, char *, long, void * );

static void	srxReqDisplay( SrxReqMsg * ) ;

static void	storeReply( void *, char *, long, void * );

static void	storeRequest( void *, char *, long, void * );

/* 
 * SRX reply message network format (4 bytes):
 *
 *	|--------|--------|--------|--------|
 *	|  type	 |  code  |     unused	    |
 *	|--------|--------|--------|--------|
 *
 * SRX lock reply messages have an additional field:
 *
 *	|--------|--------|--------|--------|
 *	|	    send count		    |
 *	|--------|--------|--------|--------|
 */
#define SRX_REPMSG_NETLEN 	4
#define SRX_LOCKREPLY_NETLEN	SENDCOUNT_NETLEN

static long
loadRep(
	void	*hdr,
	char 	*src,
	long 	len,
	void 	*arg)
{
    xAssert( len == SRX_REPMSG_NETLEN );
    ((SrxRepMsg *)hdr)->type = (SrxMsgType)src[0];
    ((SrxRepMsg *)hdr)->replyCode = (SrxReplyCode)src[1];
    return len;
}


static long
loadLockRep(
	void	*hdr,
	char 	*src,
	long 	len,
	void 	*arg)
{
    xAssert( len == SRX_LOCKREPLY_NETLEN );
    bcopy(src, (char *)&((SrxRepMsg *)hdr)->u.lock.sendCount, SENDCOUNT_NETLEN);
    return len;
}


xkern_return_t
srxLoadReply(
	SrxRepMsg	*rep,
	Msg		*msg)
{
    /* 
     * Load the message-type-independent part first
     */
    if ( msgPop(msg, loadRep, rep, SRX_REPMSG_NETLEN, 0) ) {
	return XK_FAILURE;
    }
    if ( rep->type == SEND_LOCK_REQ ) {
	if (!msgPop(msg, loadLockRep, rep, SRX_LOCKREPLY_NETLEN, 0) ) {
	    return XK_FAILURE;
	}
    }
    return XK_SUCCESS;
}


static void
storeReply(
	void	*hdr,
	char 	*dst,
	long 	len,
	void 	*arg)
{
#define HDR	((SrxRepMsg *)hdr)

    xAssert( len >= SRX_REPMSG_NETLEN );
    dst[0] = (char)HDR->type;
    dst[1] = (char)HDR->replyCode;
    dst[2] = dst[3] = 0;
    if (HDR->type == SEND_LOCK_REQ ) {
	xAssert( len == SRX_REPMSG_NETLEN + SRX_LOCKREPLY_NETLEN );
	dst += 4;
	bcopy((char *)&HDR->u.lock.sendCount, dst, SENDCOUNT_NETLEN);
    }
#undef HDR
}


void
srxStoreReply(
	SrxRepMsg	*rep,
	Msg		*msg)
{
    long	len = SRX_REPMSG_NETLEN;

    if ( rep->type == SEND_LOCK_REQ ) {
	len += SRX_REPMSG_NETLEN;
    }
    msgPush(msg, storeReply, (void *)rep, len, 0);
}


/* 
 * SRX request message network format (message-type-independent part)
 *
 *	|--------|--------|--------|--------|
 *	|  type	 |	  unused   	    |	
 *	|--------|--------|--------|--------|	
 * 	|	  Architecture tag ...	    |
 *	|--------|--------|--------|--------|	
 * 	|	    network port ...	    |
 *	|--------|--------|--------|--------|
 *	|  	       osrHost		    |
 *	|--------|--------|--------|--------|
 *	|	       osrBid		    |
 *	|--------|--------|--------|--------|
 *
 * The remainder of the header depends on the type of the message
 *
 */

/* 
 * Network length of the type-independent part of a request message 
 */
#define SRX_REQMSG_NETLEN	\
	    (4 + MN_ARCH_TAG_NETLEN + PORT_NUMBER_NETLEN + XFERHOST_NETLEN)

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
#define HDR	((SrxReqMsg *)hdr)

    xAssert( len >= SRX_REQMSG_NETLEN );
    xIfTrace(srxp, TR_FULL_TRACE) {
	int	i;

	xTrace0(srxp, TR_ALWAYS, "incoming message dump:");
	for ( i=0; i < len; i++ ) {
	    printf("%3x%c", (u_char)src[i], ((i+1) % 16) ? ' ' : '\n');
	}
	printf("\n");
    }
    HDR->type = (SrxMsgType)src[0];
    src += 4;
    bcopy(src, (char *)&HDR->archTag, MN_ARCH_TAG_NETLEN);
    src += MN_ARCH_TAG_NETLEN;
    portNumberLoad(&HDR->portNumber, src);
    src += PORT_NUMBER_NETLEN;
    xferHostLoad(&HDR->osr, src);
    return SRX_REQMSG_NETLEN;

#undef HDR
}


/*
 * Remainder of request message for LOCK messages
 *
 *	|--------|--------|--------|--------|
 *	|  	       nsrHost		    |
 *	|--------|--------|--------|--------|
 *	|	       nsrBid		    |
 *	|--------|--------|--------|--------|
 */

#define	SRX_LOCKMSG_NETLEN	XFERHOST_NETLEN

static long
loadLockReq(
	void	*hdr,
	char 	*src,
	long 	len,
	void 	*arg)
{
#define HDR	((SrxReqMsg *)hdr)

    xAssert( len >= SRX_LOCKMSG_NETLEN );
    xferHostLoad(&HDR->u.lock.nsr, src);
    return SRX_LOCKMSG_NETLEN;

#undef HDR
}


/*
 * Remainder of request message for SEND_RIGHT_TRANSFER messages
 *
 *	|--------|--------|--------|--------|
 *	|  		msgId		    |
 *	|--------|--------|--------|--------|
 *	|  		rrHost		    |
 *	|--------|--------|--------|--------|
 *	|		rrBid		    |
 *	|--------|--------|--------|--------|
 *	|	      sendCount		    |
 *	|--------|--------|--------|--------|
 */

#define SRX_XFERMSG_NETLEN  (MSGID_NETLEN + XFERHOST_NETLEN + SENDCOUNT_NETLEN)


static long
loadXferReq(
	void	*hdr,
	char 	*src,
	long 	len,
	void 	*arg)
{
#define HDR	((SrxReqMsg *)hdr)

    xAssert(len >= SRX_XFERMSG_NETLEN);

    bcopy(src, (char *)&HDR->u.xfer.msgId, MSGID_NETLEN);
    src += MSGID_NETLEN;
    xferHostLoad(&HDR->u.xfer.rr, src);
    src += XFERHOST_NETLEN;
    bcopy(src, (char *)&HDR->u.xfer.sendCount, SENDCOUNT_NETLEN);
    return SRX_XFERMSG_NETLEN;

#undef HDR
}


xkern_return_t
srxLoadRequest(
	SrxReqMsg	*hdr,
	Msg		*msg)
{
    long	len = SRX_REQMSG_NETLEN;

    xIfTrace(srxp, TR_FULL_TRACE) {
	/* 
	 * Ensure the entire message body is contiguous to ease
	 * debugging. 
	 */
	len = msgLen(msg);
    }
    /* 
     * Load the message-type-independent part first
     */
    if (!msgPop(msg, loadReq, hdr, len, 0)) {
	return XK_FAILURE;
    }
    /* 
     * Now the message-type-specific part
     */
    switch ( hdr->type ) {
	case SEND_LOCK_REQ:
	    if ( ! msgPop(msg, loadLockReq, hdr, SRX_LOCKMSG_NETLEN, 0) ) {
		return XK_FAILURE;
	    }
	    break;

	case SEND_RIGHT_TRANSFER:
	    if ( ! msgPop(msg, loadXferReq, hdr, SRX_XFERMSG_NETLEN, 0) ) {
		return XK_FAILURE;
	    }
	    break;

	default:
	    break;
    }
    xIfTrace(srxp, TR_DETAILED) {
	srxReqDisplay(hdr);
    }
    return XK_SUCCESS;
}


void
srxStoreRequest(
	SrxReqOutgoing	*hdr,
	Msg		*msg)
{
    int		len = SRX_REQMSG_NETLEN;

    switch ( hdr->type ) {
	case SEND_LOCK_REQ:
	    len += SRX_LOCKMSG_NETLEN;
	    break;

	case SEND_RIGHT_TRANSFER:
	    len += SRX_XFERMSG_NETLEN;
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
#define HDR	((SrxReqOutgoing *)hdr)

    char		*origDst = dst;
    mn_arch_tag_t	archTag;

    dst[0] = (u_char)HDR->type;
    dst[1] = dst[2] = dst[3] = 0;
    dst += 4;
    archTag = MN_ARCH_MARKER;
    bcopy((char *)&archTag, dst, MN_ARCH_TAG_NETLEN);
    dst += MN_ARCH_TAG_NETLEN;
    portNumberStore(&HDR->port->net_port_number, dst);
    dst += PORT_NUMBER_NETLEN;
    xferHostStore(&srxMyXferHost, dst);
    dst += XFERHOST_NETLEN;
    switch ( HDR->type ) {
	case SEND_LOCK_REQ:
	    xAssert(len == SRX_REQMSG_NETLEN + SRX_LOCKMSG_NETLEN);
	    xferHostStore(&HDR->u.lock.nsr, dst);
	    break;

	case SEND_RIGHT_TRANSFER:
	    xAssert(len == SRX_REQMSG_NETLEN + SRX_XFERMSG_NETLEN);
	    bcopy((char *)&HDR->u.xfer.msgId, dst, MSGID_NETLEN);
	    dst += MSGID_NETLEN;
	    xferHostStore(&HDR->u.xfer.rr, dst);
	    dst += XFERHOST_NETLEN;
	    bcopy((char *)&HDR->u.xfer.sendCount, dst, SENDCOUNT_NETLEN);
	    break;

	default:
	    break;
    }
    xIfTrace(srxp, TR_FULL_TRACE) {
	int	i;

	xTrace0(srxp, TR_ALWAYS, "outgoing message dump:");
	for ( i=0; i < len; i++ ) {
	    printf("%3x%c", (u_char)origDst[i], ((i+1) % 16) ? ' ' : '\n');
	}
	printf("\n");
    }
#undef HDR
}


static void
srxReqDisplay(
	SrxReqMsg	*h)
{
    xTrace3(srxp, TR_ALWAYS,
	"SRX %s request, port %d, arch tag %x",
	srxMsgTypeStr(h->type), h->portNumber, (u_int)h->archTag);
    switch( h->type ) {
	case SEND_RIGHT_TRANSFER:
	    xTrace2(srxp, TR_ALWAYS,
		"Message ID: %d, receiver: %s",
		h->u.xfer.msgId, xferHostStr(&h->u.xfer.rr));
	    xTrace1(srxp, TR_ALWAYS, "makeSendCount: %d", h->u.xfer.sendCount);
	    break;

	case SEND_LOCK_REQ:
	    xTrace1(srxp, TR_ALWAYS,
		"New sender: %s", xferHostStr(&h->u.lock.nsr));
	    break;

	case UNLOCK_WITH_TRANSFER:
	case UNLOCK_NO_TRANSFER:
	default:
	    break;
    }
}

