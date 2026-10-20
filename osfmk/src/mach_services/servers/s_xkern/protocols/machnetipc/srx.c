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
 * srx.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1991  Arizona Board of Regents
 *
 *
 * Revision: 1.22
 * Date: 1993/08/05 20:57:53
 */
/* 
 * Mach Send Right Transfer protocol
 */

#include <xkern/include/xkernel.h>
#include <s_xkern/protocols/machnetipc/srx_i.h>
#include <s_xkern/include/prot/xfer.h>


void	srx_init( XObj );

static xkern_return_t
	doCall( SrxReqOutgoing *, SrxRepMsg *, XObj );

static xkern_return_t
	getPeerBid( IPhost *, BootId * );

static void
	lockAborted( mnetport *, void * );

static void
	lockForAddingSender( XObj, SrxReqMsg *, SrxRepMsg *);

static srx_return_t
	moveSendRight( XObj, mnetport *, IPhost *, MsgId, XObj);

static void
	portUnlocked( mnetport *, void * );

static void
	receiveNewSendRight( XObj, SrxReqMsg *, SrxRepMsg *);

static xkern_return_t
	srxCallDemux( XObj, XObj, Msg *, Msg * );

static int
	srxControl( XObj, int, char *, int );

static void
	unlockForAddingSender( XObj, SrxReqMsg *, SrxRepMsg *);

#if	XK_DEBUG
static char *
	srxRetTypeStr( srx_return_t );
#endif /* XK_DEBUG */

int		tracesrxp;
XferHost	srxMyXferHost;

static XObj	srxProtlSelf;
static Map	transferMap;
static Map	lockedMap;

/* 
 * Locks a receive right for adding a sender and composes a reply
 * message 
 */
static void
lockForAddingSender(
	XObj		self,
	SrxReqMsg	*req,
	SrxRepMsg	*rep)
{

#define	ERROR_RETURN {				\
	    rep->replyCode = SRX_FAILURE;	\
	    writerUnlock(&npd->rwlock);		\
	    return;				\
	}

    mnetport		*npd;	/* network port descriptor */
    xkern_return_t	xkr;

    {
	mportNetRep	netport;

	bzero((char *)&netport, sizeof(netport));
	netport.net_port_number = req->portNumber;
	if ( findNetPort(&netport, req->archTag, FALSE, &npd) == XK_FAILURE ) {
	    xTrace0(srxp, TR_SOFT_ERRORS,
		"SRX: could not find port for lock request");
	    rep->replyCode = SRX_FAILURE;
	    return;
	}
    }
    xTrace1(srxp, TR_EVENTS, "lock request for port %d", npd->net_port_number);

#if 0
    /* 
     * XXX -- is this the right check to be making?
     */
    if ( ! (npd->net_port_local_rights & MACH_PORT_TYPE_RECEIVE) ) {
	xTrace0(srxp, TR_ERRORS,
	    "request for locking port without receive right");
	rep->replyCode = SRX_FAILURE;
	return;
    }
#endif /* 0 */

    /* 
     * We get a writer lock so only one third-party transfer can take
     * place at a time.  There is no fundamental reason why this
     * couldn't be a reader lock ... this just makes some of the local
     * bookkeeping easier.
     */
    xTrace1(srxp, TR_EVENTS,
	"SRX Getting a WRITER LOCK on port %x", npd->net_port_number);
    if ( writerLock(&npd->rwlock) == XK_FAILURE ) {
	xTrace0(srxp, TR_ERRORS, "SRX -- writer lock fails!!");
	rep->replyCode = SRX_FAILURE;
	return;
    }
    switch ( npd->net_port_type ) {
	case MN_FORWARDING:
	    /* 
	     * Port moved
	     */
	    ERROR_RETURN;

	case MN_VALID:
	    break;

	default:
	    xTrace1(srxp, TR_SOFT_ERRORS,
		"netIpcSrx lock request while in %s mode",
		netPortStateStr(npd->net_port_type));
	    ERROR_RETURN;
    }
    if ( ! xferConfirmBid(self, &req->u.lock.nsr) ) {
	xTrace0(srxp, TR_EVENTS, "SRX -- NSR rebooted");
	ERROR_RETURN;
    }
    npd->old_net_port_type = npd->net_port_type;
    npd->net_port_type = MN_ADDING_SENDER;
    xTrace3(srxp, TR_MORE_EVENTS,
	"port %d locked by host %s for adding sender %s",
	npd->net_port_number, ipHostStr(&req->osr.h),
	ipHostStr(&req->u.lock.nsr.h));
    /* 
     * addNewSender will increment the send count
     */
    addNewSender(npd, req->u.lock.nsr.h, INIT_MAKE_SEND_COUNT);
    xkr = mapResolve(
	npd->senders_map, &req->u.lock.nsr.h, &rep->u.lock.sendCount);
    if ( xkr == XK_FAILURE ) {
	xTrace2(srxp, TR_ERRORS,
	    "Couldn't find make-send-count for host %s, port %d",
	    ipHostStr(&req->u.lock.nsr.h), npd->net_port_number);
	removeSender(npd, req->u.lock.nsr.h);
	ERROR_RETURN;
    }
    /* 
     * Bind the NSR to this locked port so we know which right to
     * destroy if the transfer doesn't complete
     */
    {
	IPhost	*nsr = X_NEW(IPhost);

	*nsr = req->u.lock.nsr.h;
	xferLockedMapAdd(self, lockedMap, &req->osr.h, npd, (void *)nsr);
    }

    rep->replyCode = SRX_SUCCESS;
#undef ERROR_RETURN
}


static void
lockAborted(
	mnetport	*np,
	void		*arg)
{
    IPhost	*newSender = (IPhost *)arg;

    /* 
     * Undo the effects of the 'addNewSender' that we did at the start
     * of this transfer.  
     */
    xTrace2(srxp, TR_DETAILED,
	"SRX lockAbortCallBack, port %d, NSR %s",
	np->net_port_number, ipHostStr((IPhost *)arg));
    removeSender(np, *newSender);
    portUnlocked(np, arg);
}


static void
portUnlocked(
	mnetport	*np,
	void		*arg)
{
    xTrace2(srxp, TR_DETAILED,
	"SRX unlockCallBack, port %d, NSR %s",
	np->net_port_number, ipHostStr((IPhost *)arg));
    xFree((char *)arg);
    xTrace1(srxp, TR_EVENTS,
	"SRX Releasing a WRITER LOCK on port %x", np->net_port_number);
    writerUnlock(&np->rwlock);
    np->net_port_type = np->old_net_port_type;
}


static void
unlockForAddingSender(
	XObj		self,
	SrxReqMsg	*req,
	SrxRepMsg	*rep)
{
    mnetport		*npd;

    xTrace0(srxp, TR_MORE_EVENTS, "request to unlock port");
    {
	mportNetRep	netport;

	bzero((char *)&netport, sizeof(netport));
	netport.net_port_number = req->portNumber;
	if ( findNetPort(&netport, req->archTag, FALSE, &npd) == XK_FAILURE ) {
	    xTrace0(srxp, TR_ERRORS, "request for unlocking nonexistent port");
	    rep->replyCode = SRX_FAILURE;
	    return;
	}
    }
    if ( npd->net_port_type != MN_ADDING_SENDER ) {
	xTrace0(srxp, TR_ERRORS,
	    "request for unlocking port that is not locked");
	rep->replyCode = SRX_FAILURE;
	return;
    }
    if ( req->type == UNLOCK_WITH_TRANSFER ) {
	xTrace0(srxp, TR_MORE_EVENTS, "SRX -- transfer is indicated");
	xferLockedMapRemove(lockedMap, &req->osr.h, npd, portUnlocked);
    } else {
	xTrace0(srxp, TR_MORE_EVENTS, "SRX -- no transfer");
	xferLockedMapRemove(lockedMap, &req->osr.h, npd, lockAborted);
    }
}


/* 
 * receiveNewSendRight -- someone is giving us a new send right to
 * a remote port.  This transfer is conditional on final receipt of
 * the actual netIpc message transferring the right.
 */
static void
receiveNewSendRight(
	XObj		self,
	SrxReqMsg	*req,
	SrxRepMsg	*rep)
{
    mnetport	*npd;

    xTrace1(srxp, TR_EVENTS,
	"srx receive new send right from host %s", ipHostStr(&req->osr.h));
    if ( ! xferConfirmBid(self, &req->u.xfer.rr) ) {
	xTrace1(srxp, TR_EVENTS, "SRX -- rr %s has rebooted",
	    ipHostStr(&req->u.xfer.rr.h));
	rep->replyCode = SRX_SUCCESS;
	return;
    }
    if ( ! xferConfirmBid(self, &req->osr) ) {
	xTrace1(srxp, TR_EVENTS, "SRX -- osr %s has rebooted",
	    ipHostStr(&req->osr.h));
	rep->replyCode = SRX_FAILURE;
	return;
    }
    {
	mportNetRep		netport;

	netport.net_port_number = req->portNumber;
	netport.net_port_rights = MACH_PORT_TYPE_SEND;
	netport.receiver_host_addr = req->u.xfer.rr.h;
	netport.make_send_count = req->u.xfer.sendCount;
	if (findNetPort(&netport, req->archTag, TRUE, &npd)
	    == XK_FAILURE ) {
	    xError("receiveNewSendRight -- findNetPort(create) fails!");
	    rep->replyCode = SRX_FAILURE;
	    return;
	}
	updatePortId(npd, req->u.xfer.msgId, req->u.xfer.rr.h);
    }
    xferTransferMapAdd(self, transferMap, &req->osr.h, req->u.xfer.msgId, npd);
}


/* 
 * Called to notify us that a message carrying complicated port rights
 * (i.e., rights that we processed) has arrived and the network rights
 * have been converted to mach port rights.  We can remove the
 * temporary rights that we held.
 */
void
srxTransferComplete(
	IPhost	peer,
	MsgId	msgId)
{
    xTrace2(srxp, TR_EVENTS,
	"SRX notes arrival of message %d from %s", msgId, ipHostStr(&peer));
    xferTransferMapRemove(transferMap, &peer, msgId);
}


xkern_return_t
srxMoveSendRights(
	IPhost		dst,
	MsgId		msgId,
	mnetport	**ports)
{
    XObj		lls;
    srx_return_t	res = SRX_TRANSFERRED;

    if ( ! xIsProtocol(srxProtlSelf) ) {
	xTrace0(srxp, TR_ERRORS, "SRX -- self object not initialized");
	return XK_FAILURE;
    }
    xTrace2(srxp, TR_EVENTS, "srx transferring ports for msg %d to %s",
	msgId, ipHostStr(&dst));
    lls = xferOpen(srxProtlSelf, &dst, 0);
    if ( lls == ERR_XOBJ ) {
	xTrace1(srxp, TR_SOFT_ERRORS,
	    "srxMoveSendRights could not open lls for host %s",
	    ipHostStr(&dst));
	return XK_FAILURE;
    }
    for ( ; *ports && res != SRX_DESTINATION_FAILURE; ports++ ) {
	res = moveSendRight(srxProtlSelf, *ports, &dst, msgId, lls);
	xTrace1(srxp, TR_ERRORS,
	    "moveSendRight returns error code %s", srxRetTypeStr(res));
    }
    xClose(lls);
    return XK_SUCCESS;
}


static xkern_return_t
getPeerBid(
	IPhost	*host,
	BootId	*bidPtr)
{
    BidctlBootMsg	bm;

    bm.h = *host;
    bm.id = 0;
    if ( xControl(xGetDown(srxProtlSelf, XFER_BIDCTL_I),
	BIDCTL_GET_PEER_BID_BLOCKING,
	(char *)&bm, sizeof(bm)) < (int)sizeof(bm) ) {
	xTrace1(srxp, TR_ERRORS,
	    "moveSendRight couldn't get remote BID for peer %s",
	    ipHostStr(&bm.h));
	return XK_FAILURE;
    }
    *bidPtr = bm.id;
    return XK_SUCCESS;
}


/* 
 * Moves a sendright for 'np' to 'nsr', pending receipt of msg
 * 'msgId'.  'llsNsr' is a lower session which should be used to
 * communicate with the new holder of the send right.
 */
static srx_return_t
moveSendRight(
	XObj		self,
	mnetport	*np,
	IPhost		*nsr,
	MsgId		msgId,
	XObj		llsNsr)
{
    Part		p;
    SrxReqOutgoing	req;
    SrxRepMsg		rep;
    XObj		llpBidctl;
    XObj		llsRr;	/* lower session for talking to RR */
    srx_return_t	rVal;
    BootId		rrBid;

    req.port = np;
    req.u.lock.nsr.h = *nsr;
    llpBidctl = xGetDown(self, XFER_BIDCTL_I);
    xAssert(xIsProtocol(llpBidctl));
    if ( getPeerBid(nsr, &req.u.lock.nsr.bid) == XK_FAILURE ) {
	return SRX_DESTINATION_FAILURE;
    }
    /* 
     * Send lock request to holder of receive right
     */
    req.type = SEND_LOCK_REQ;
    do {
	IPhost	savedRcvr;

	savedRcvr = np->receiver_host_addr;
	partInit(&p, 1);
	partPush(p, &np->receiver_host_addr, sizeof(IPhost));
	llsRr = xOpen(self, self, xGetDown(self, XFER_XPORT_I), &p,
		      xMyPath(self));
	if ( llsRr == ERR_XOBJ ) {
	    xTrace0(srxp, TR_ERRORS, "moveSendRight couldn't open lls");
	    return SRX_LOCKING_FAILURE;
	}
	if ( getPeerBid(&np->receiver_host_addr, &rrBid) == XK_FAILURE ) {
	    return SRX_LOCKING_FAILURE;
	}
	xTrace0(srxp, TR_EVENTS,
	    "SRX sending lock request to holder of receive right");
	if ( doCall(&req, &rep, llsRr) == XK_FAILURE ) {
	    xClose(llsRr);
	    return SRX_LOCKING_FAILURE;
	}
	if ( rep.replyCode == SRX_SUCCESS ) {
	    break;
	}
	xTrace1(srxp, TR_SOFT_ERRORS,
	    "moveSendRight -- error reply %d from RR", rep.replyCode);
	if ( IP_EQUAL(np->receiver_host_addr, savedRcvr) ) {
	    xTrace0(srxp, TR_ERRORS, "moveSendRight -- lost track of rcvr!");
	    xClose(llsRr);
	    return SRX_LOCKING_FAILURE;
	} else {
	    xTrace0(srxp, TR_SOFT_ERRORS,
		"moveSendRight -- rcvr moved, retrying");
	}
    } while (1);

#if 0
    xTrace0(srxp, TR_ALWAYS, "\n\n\ndelay before transfer");
    Delay( 10 * 1000 );
#endif /* 0 */

    /* 
     * Transfer the right to the new holder
     */
    req.type = SEND_RIGHT_TRANSFER;
    req.u.xfer.msgId = msgId;
    req.u.xfer.rr.h = np->receiver_host_addr;
    req.u.xfer.rr.bid = rrBid;
    req.u.xfer.sendCount = rep.u.lock.sendCount;
    xTrace1(srxp, TR_EVENTS,
	"SRX transferring right to new holder, sendCount == %d",
	req.u.xfer.sendCount);
    if ( doCall(&req, &rep, llsNsr) == XK_FAILURE ||
	 rep.replyCode == SRX_FAILURE ) {
	req.type = UNLOCK_NO_TRANSFER;
	rVal = SRX_DESTINATION_FAILURE;
    } else {
	req.type = UNLOCK_WITH_TRANSFER;
	rVal = SRX_TRANSFERRED;
    }
    /* 
     * Unlock the receive right
     */
#if 0
    xTrace0(srxp, TR_ALWAYS, "\n\n\ndelay before unlock");
    Delay( 20 * 1000 );
#endif /* 0 */
    xTrace0(srxp, TR_EVENTS,
	"SRX sending unlock message to holder of receive right");
    if ( doCall(&req, &rep, llsRr) == XK_FAILURE ||
	rep.replyCode == SRX_FAILURE ) {
	xTrace0(srxp, TR_ERRORS, "moveSendRight -- unlock failed");
	rVal = (rVal == SRX_TRANSFERRED) ?
	    SRX_LOCKING_FAILURE : SRX_DESTINATION_FAILURE;
    }
    xClose(llsRr);
    xTrace2(srxp, TR_EVENTS,
	"SRX transfer of port %d completes with result %s",
	np->net_port_number, srxRetTypeStr(rVal));
    return rVal;
}


/* 
 * Construct a message with the given request, call on lls, and
 * extract the reply.  Returns XK_SUCCESS if xCall succeeds and the
 * reply message had an SrxRepMsg attached to it.
 */
static xkern_return_t
doCall(
	SrxReqOutgoing	*req,
	SrxRepMsg	*rep,
	XObj		lls)
{
    Msg			msg, rmsg;
    xkern_return_t	res;

    msgConstructEmpty(&msg);
    msgConstructEmpty(&rmsg);
    srxStoreRequest(req, &msg);
    if ( xCall(lls, &msg, &rmsg) == XK_SUCCESS ) {
	res = srxLoadReply(rep, &rmsg);
	if ( res == XK_SUCCESS ) {
	    xTrace0(srxp, TR_DETAILED, "srx -- successful return from call");
	} else {
	    xTrace0(srxp, TR_ERRORS, "moveSendRight -- msgPop failed");
	}
    } else {
	xTrace0(srxp, TR_SOFT_ERRORS, "moveSendRight -- xCall failed");
	res = XK_FAILURE;
    }
    msgDestroy(&msg);
    msgDestroy(&rmsg);
    return res;
}


static int
srxControl(
	XObj	self,
	int	op,
	char	*buf,
	int	len)
{
    switch ( op ) {
	case BIDCTL_FIRST_CONTACT:
	    return 0;

	case BIDCTL_PEER_REBOOTED: {
	    BidctlBootMsg	*bm = (BidctlBootMsg *)buf;

	    xTrace1(srxp, TR_EVENTS, "srxControl -- peer %s rebooted",
		ipHostStr(&bm->h));
	    xferPeerRebooted(self, &bm->h, lockedMap, transferMap,
		removeSendRight, lockAborted);
	    xTrace0(srxp, TR_DETAILED, "End SRX reboot handler");
	    return 0;
	}

	default:
	    return xControl(xGetDown(self, XFER_XPORT_I), op, buf, len);
    }
}


/* 
 * callDemux -- both the NSR and RR will receive SRX requests.
 */
static xkern_return_t
srxCallDemux(
	XObj	self,
	XObj	lls,
	Msg	*m,
	Msg	*rm)
{
    SrxReqMsg	req;
    SrxRepMsg	rep;

    xTrace0(srxp, TR_EVENTS, "srxCallDemux");
    if ( srxLoadRequest(&req, m) == XK_FAILURE ) {
	xTrace0(srxp, TR_ERRORS, "srxCallDemux: loadRequest fails");
	return XK_FAILURE;
    }
    rep.type = req.type;
    switch ( req.type ) {
	case SEND_LOCK_REQ:
	    lockForAddingSender(self, &req, &rep);
	    break;

	case UNLOCK_NO_TRANSFER:
	case UNLOCK_WITH_TRANSFER:
	    unlockForAddingSender(self, &req, &rep);
	    break;

	case SEND_RIGHT_TRANSFER:
	    receiveNewSendRight(self, &req, &rep);
	    break;

	default:
	    xTrace1(srxp, TR_ERRORS,
		"srxDemux -- unknown msg type %d", req.type);
	    rep.replyCode = SRX_FAILURE;
    }
    srxStoreReply(&rep, rm);
    return XK_SUCCESS;
}


void
srx_init(
	XObj	self)
{
    Part	p;

    xTrace0(srxp, TR_GROSS_EVENTS, "SRX init");
    if ( ! xIsProtocol(xGetDown(self, XFER_BIDCTL_I)) ||
	 ! xIsProtocol(xGetDown(self, XFER_XPORT_I )) ) {
	xError("SRX is not properly configured");
	return;
    }
    if ( xControl(xGetDown(self, XFER_XPORT_I), GETMYHOST,
	    (char *)&srxMyXferHost.h, sizeof(IPhost))
	 < (int)sizeof(IPhost) ) {
	xError("srx couldn't get local host");
	return;
    }
    if ( xControl(xGetDown(self, XFER_BIDCTL_I), BIDCTL_GET_LOCAL_BID,
	    (char *)&srxMyXferHost.bid, sizeof(BootId))
	 < (int)sizeof(BootId) ) {
	xError("srx couldn't get local BID");
	return;
    }
    srxProtlSelf = self;
    self->control = srxControl;
    self->calldemux = srxCallDemux;
    xferCreateMaps(&lockedMap, &transferMap);
    partInit(&p, 0);
    if ( xOpenEnable(self, self, xGetDown(self, XFER_XPORT_I), &p)
	 == XK_FAILURE ) {
	xError("srx couldn't openenable llp");
	return;
    }
}


#if	XK_DEBUG

static char *
srxRetTypeStr(
	srx_return_t	val)
{
    switch( val ) {
	case SRX_TRANSFERRED: 		return "SRX_TRANSFERRED";
	case SRX_LOCKING_FAILURE:	return "SRX_LOCKING_FAILURE";
	case SRX_DESTINATION_FAILURE:	return "SRX_DESTINATION_FAILURE";
    }
    return "UNKNOWN";
}

#endif	/* XK_DEBUG */

