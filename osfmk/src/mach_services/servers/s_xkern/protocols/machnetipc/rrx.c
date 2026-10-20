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
 * rrx.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1991  Arizona Board of Regents
 *
 *
 *  Revision: 1.25
 *  Date: 1993/08/05 20:55:47
 */
/* 
 * Mach Receive Right Transfer protocol (RRX)
 *
 * See Notes.portXfer.receive* for a high-level description of the
 * protocol. 
 */

#include <xkern/include/xkernel.h>
#include <s_xkern/protocols/machnetipc/rrx_i.h>
#include <s_xkern/include/prot/xfer.h>

int		tracerrxp;

/* 
 * Most static variables should be in protocol state if we every wanted to
 * multiply instantiate this protocol.
 */
static XObj	rrxProtlSelf;
static IPhost	myHost;
static BootId	myBid;

/* 
 * We may be able to eventually get by without a lockedMap in this
 * protocol if netIPC can override the locked state in which it might
 * find the port in the event of ORR reboot.
 */
static Map	lockedMap;
static Map	xferMap;


extern	int	insque();
extern	int	remque();

static xkern_return_t
	doCall( RrxReqOutgoing *, RrxRepMsg *, XObj );

static void
	freeSendersList( ListElement * );

static int 
	initSendersList( void *, void *, void * );

static void 
	lockForMovingReceiver( XObj, RrxReqMsg *, RrxRepMsg *, XObj );

static void 
	lockSenders( mnetport *, ListElement * );

static void 
	moveReceiveRight( XObj, mnetport *, IPhost *, BootId, MsgId, XObj );

static xkern_return_t
	notifyNrr( mnetport *, IPhost *, XObj, MsgId, ListElement *);

static void
	portUnlocked( mnetport *, void * );

static void 
	receiveNewReceiveRight( XObj, RrxReqMsg *, RrxRepMsg * );

static xkern_return_t
	rrxCallDemux( XObj, XObj, Msg *, Msg * );

static int
	rrxControl( XObj, int, char *, int );

static void 
	unlockForMovingReceiver( XObj, RrxReqMsg *, RrxRepMsg *, XObj );

static void 
	unlockSenders( mnetport *, IPhost *, BootId, ListElement * );


void
rrx_init(
	XObj	self)
{
    Part	p;

    xTrace0(rrxp, TR_GROSS_EVENTS, "RRX init");
    if (!xIsProtocol(xGetDown(self, XFER_BIDCTL_I)) ||
	!xIsProtocol(xGetDown(self, XFER_XPORT_I))) {
	xError("RRX is not properly configured");
	return;
    }
    if (xControl(xGetDown(self, XFER_XPORT_I), GETMYHOST,
	    (char *)&myHost, sizeof(IPhost))
	< (int)sizeof(IPhost)) {
	xError("RRX could not get local host address");
	return;
    }
    if ( xControl(xGetDown(self, XFER_BIDCTL_I), BIDCTL_GET_LOCAL_BID,
	    (char *)&myBid, sizeof(BootId))
	< (int)sizeof(BootId)) {
	xError("RRX could not get local Boot ID");
	return;
    }
    xferCreateMaps(&lockedMap, &xferMap);
    rrxProtlSelf = self;
    self->calldemux = rrxCallDemux;
    self->control = rrxControl;
    partInit(&p, 0);
    if ( xOpenEnable(self, self, xGetDown(self, XFER_XPORT_I), &p)
	== XK_FAILURE ) {
	xTrace0(rrxp, TR_ERRORS, "RRX could not openEnable transport protl");
    }
}


/* 
 * description in rrx.h
 */
xkern_return_t
rrxMoveReceiveRights(
	IPhost		dst,
	MsgId		msgId,
	mnetport	**ports)
{
    XObj	lls;
    BootId	dstBid;

    if ( ! xIsProtocol(rrxProtlSelf) ) {
	xTrace0(rrxp, TR_ERRORS, "RRX -- self object not initialized");
	return XK_FAILURE;
    }
    xTrace2(rrxp, TR_EVENTS,
	"rrx transferring ports for msg %d to %s", msgId, ipHostStr(&dst));
    if ( (lls = xferOpen(rrxProtlSelf, &dst, &dstBid)) == ERR_XOBJ ) {
	xTrace1(rrxp, TR_SOFT_ERRORS,
	    "rrxMoveReceiveRights could not open lls for host %s",
	    ipHostStr(&dst));
	return XK_FAILURE;
    }
    /* 
     * At this point we know that this channel is talking to the same
     * incarnation of NRR as was alive when the transfer was
     * requested.  If NRR reboots, sends on this channel will fail.
     */
    for ( ; *ports; ports++ ) {
	moveReceiveRight(rrxProtlSelf, *ports, &dst, dstBid, msgId, lls);
    }
    xClose(lls);
    return XK_SUCCESS;
}


/* 
 * Moves a receive right for 'np' to 'nrr', pending receipt of msg
 * 'msgId'.  'llsNrr' is a lower session which should be used to
 * communicate with the new holder of the receive right.
 */
static void
moveReceiveRight(
	XObj		self,
	mnetport	*np,
	IPhost		*nrr,
	BootId		nrrBid,
	MsgId		msgId,
	XObj		llsNrr)
{
    ListElement		*hostList;
    xkern_return_t	res;

    xTrace3(rrxp, TR_EVENTS,
	"RRX transferring port %d to host %s (bid %x)",
	np->net_port_number, ipHostStr(nrr), nrrBid);
    xTrace3(rrxp, TR_EVENTS,
	"RRX Getting a WRITER LOCK on port %x (readers = %d) (writer = %d)",
	np->net_port_number,
	np->rwlock.activeReaders,
	np->rwlock.activeWriter);
    if (writerLock(&np->rwlock) == XK_FAILURE ) {
	xTrace0(rrxp, TR_ERRORS, "RRX -- writer lock fails!!");
	xTrace1(rrxp, TR_EVENTS,
	    "RRX Releasing a WRITER LOCK on port %x", np->net_port_number);
	writerUnlock(&np->rwlock);
	return;
    }
    xTrace1(rrxp, TR_EVENTS,
	"RRX -- port state == %s", netPortStateStr(np->net_port_type));
    if (np->net_port_type != MN_VALID ) {
	xTrace1(rrxp, TR_EVENTS,
	    "RRX -- aborting due to port state %d", np->net_port_type);
	xTrace1(rrxp, TR_EVENTS,
	    "RRX Releasing a WRITER LOCK on port %x", np->net_port_number);
	writerUnlock(&np->rwlock);
	return;
    }
    np->net_port_type = MN_MOVING_RECEIVER;
    /* 
     * Can't use the ordinary senders map here because it might get
     * modified if a sender reboots (and the modifications would
     * destroy the integrity of a mapForEach operation.)  We copy it
     * to a list instead.
     */
    hostList = X_NEW(ListElement);
    hostList->next = hostList->prev = hostList;
    mapForEach(np->senders_map, initSendersList, hostList);
    lockSenders(np, hostList);

#if 0
    xTrace0(rrxp, TR_ALWAYS, "\n\n\ndelay before transfer");
    Delay( 10 * 1000 );
#endif /* 0 */

    xTrace0(rrxp, TR_EVENTS, "sending transfer notification to new receiver");
    res = notifyNrr(np, nrr, llsNrr, msgId, hostList);
    if (res == XK_FAILURE ) {
	xTrace0(rrxp, TR_EVENTS, "RRX -- send to new receiver fails");
	/* 
	 * Cause confirmation failures and subsequent deallocation
	 */
	nrrBid = 0;
    }
    np->net_port_type = MN_FORWARDING;

#if 0
    xTrace0(rrxp, TR_ALWAYS, "\n\n\ndelay before unlocking");
    Delay( 10 * 1000 );
#endif /* 0 */

    /* 
     * notify any local senders
     */
    np->receiver_host_addr = *nrr;
    if ( nrrBid ) {
	receiverMoved(np);
    } else {
	receiveRightDeallocated(np);
    }

    unlockSenders(np, nrr, nrrBid, hostList);
    freeSendersList(hostList);
    xTrace1(rrxp, TR_EVENTS,
	"RRX Releasing a WRITER LOCK on port %x", np->net_port_number);
    writerUnlock(&np->rwlock);
}


/* 
 * Send a lockRequest message to each valid sender in 'hostList'.  If the
 * lock can not be achieved, the sender is marked as invalid.  
 */
static void
lockSenders(
	mnetport	*np,
	ListElement	*hostList)
{
    RrxReqOutgoing	req;
    RrxRepMsg		rep;
    ListElement		*e;
    XObj		llsSender;
    BootId		senderBid;

    /* 
     * Prepare lock request message
     */
    req.type = RECEIVE_LOCK_REQ;
    req.port = np;
    /* 
     * Send lock request to each holder of send rights
     */
    for ( e = hostList->next; e != hostList; e = e->next ) {
	IPhost	*h;

	h = &e->sh.xh.h;
	xTrace1(rrxp, TR_EVENTS, "RRX -- locking sender %s", ipHostStr(h));
	llsSender = xferOpen(rrxProtlSelf, h, &senderBid);
	if ( llsSender == ERR_XOBJ ) {
	    xTrace1(rrxp, TR_SOFT_ERRORS,
		"RRX couldn't open channel to sender %s", ipHostStr(h));
	    e->valid = FALSE;
	    continue;
	}
	if ( mapResolve(np->senders_map, h, 0) == XK_FAILURE ) {
	    xTrace1(rrxp, TR_SOFT_ERRORS,
		"RRX lockSenders -- sender %s rebooted", ipHostStr(h));
	    e->valid = FALSE;
	    continue;
	}
	e->lls = llsSender;
	if (doCall(&req, &rep, llsSender) == XK_FAILURE ||
	    rep.replyCode != RRX_SUCCESS ) {
	    xTrace1(rrxp, TR_ERRORS,
		"RRX couldn't send lock request to sender %s", ipHostStr(h));
	    e->valid = FALSE;
	    continue;
	}
    }
}


/* 
 * Send an unlockRequest message to each valid sender in 'hostList'.  If the
 * unlock request fails, the sender is marked as invalid.  
 */
static void
unlockSenders(
	mnetport	*np,
	IPhost		*nrr,
	BootId		nrrBid,
	ListElement	*hostList)
{
    RrxReqOutgoing	req;
    RrxRepMsg		rep;
    ListElement		*e;

    /* 
     * Prepare unlock request message
     */
    req.type = RECEIVE_UNLOCK_REQ;
    req.port = np;
    req.u.unlock.nrr.h = *nrr;
    req.u.unlock.nrr.bid = nrrBid;
    /* 
     * Send request to each holder of send rights
     */
    for ( e = hostList->next; e != hostList; e = e->next ) {
	IPhost	*h;

	h = &e->sh.xh.h;
	if ( e->valid ) {

	    xTrace1(rrxp, TR_EVENTS,
		"RRX -- unlocking sender %s", ipHostStr(h));
	    xAssert(xIsSession(e->lls));
	    if (doCall(&req, &rep, e->lls) == XK_FAILURE ||
		rep.replyCode != RRX_SUCCESS ) {
		xTrace1(rrxp, TR_ERRORS,
		    "RRX couldn't send unlock request to sender %s",
		    ipHostStr(h));
		e->valid = FALSE;
	    }
	} else {
	    xTrace1(rrxp, TR_EVENTS,
		"RRX unlock -- sender %s invalid", ipHostStr(h));
	}
    }
}


/* 
 * Add an entry to the list for each host in the map
 */
static int
initSendersList(
	void	*key,
	void	*val,
	void	*arg)
{
    XObj		bidctl;
    ListElement		*firstElem, *e;
    BidctlBootMsg	bm;

    xTrace2(rrxp, TR_MORE_EVENTS,
	"rrx initSendersList adding sender %s, send count %d",
	ipHostStr((IPhost *)key), (int)val);
    firstElem = (ListElement *)arg;
    xAssert(firstElem);
    e = X_NEW(ListElement);
    e->sh.xh.h = *(IPhost *)key;
    e->sh.sendCount = (int)val;
    e->valid = TRUE;
    e->lls = 0;
    bm.h = *(IPhost *)key;
    bm.id = 0;
    bidctl = xGetDown(rrxProtlSelf, XFER_BIDCTL_I);
    xAssert(xIsProtocol(bidctl));
    if ((xControl(bidctl, BIDCTL_GET_PEER_BID, (char *)&bm, sizeof(bm))
	    < (int)sizeof(bm)) ||
	bm.id == 0 ) {
	xTrace1(rrxp, TR_ERRORS,
	    "RRX couldn't get BID for sender %s", ipHostStr(&bm.h));
	e->valid = FALSE;
    } else {
	e->sh.xh.bid = bm.id;
	xTrace1(rrxp, TR_DETAILED, "sender %s", xferHostStr(&e->sh.xh));
    }
    /* 
     * Add this host to the end of the list
     */
    insque(e, firstElem->prev);
    return MFE_CONTINUE;
}


static void
freeSendersList(
	ListElement	*l)
{
    ListElement	*e;

    e = l->next;
    while ( e != l ) {
	e = e->next;
	xFree((char *)e->prev);
    }
    xFree((char *)e);
}


/* 
 * Compose and send a 'send right transfer' message to the new receiver.
 */
static xkern_return_t
notifyNrr(
	mnetport	*np,
	IPhost		*nrr,
	XObj		lls,
	MsgId		msgId,
	ListElement	*hostList)
{
    RrxReqOutgoing	req;
    RrxRepMsg		rep;
    xkern_return_t	res;

    req.type = RECEIVE_RIGHT_TRANSFER;
    req.port = np;
    req.u.transfer.senders = hostList;
    req.u.transfer.msgId = msgId;
    req.u.transfer.orr.h = myHost;
    req.u.transfer.orr.bid = myBid;
    res = ( doCall(&req, &rep, lls) == XK_SUCCESS &&
    rep.replyCode == RRX_SUCCESS ) ? XK_SUCCESS : XK_FAILURE;
    return res;
}


/* 
 * Construct a message with the given request, call on lls, and
 * extract the reply.  Returns XK_SUCCESS if xCall succeeds and the
 * reply message had an RrxRepMsg attached to it.
 */
static xkern_return_t
doCall(
	RrxReqOutgoing	*req,
	RrxRepMsg	*rep,
	XObj		lls)
{
    Msg			msg, rmsg;
    xkern_return_t	res = XK_FAILURE;

    msgConstructEmpty(&msg);
    msgConstructEmpty(&rmsg);
    rrxStoreRequest(req, &msg);
    if ( xCall(lls, &msg, &rmsg) == XK_SUCCESS ) {
	if (msgPop(&rmsg, rrxLoadReply, rep, RRX_REPMSG_NETLEN, 0)) {
	    xTrace1(rrxp, TR_DETAILED,
		"rrx -- call returns with %s",
		rep->replyCode == RRX_SUCCESS ? "SUCCESS" : "FAILURE");
	    res = XK_SUCCESS;
	} else {
	    xTrace0(rrxp, TR_ERRORS, "RRX -- msgPop failed");
	}
    } else {
	xTrace0(rrxp, TR_SOFT_ERRORS, "RRX -- xCall failed");
    }
    msgDestroy(&msg);
    msgDestroy(&rmsg);
    return res;
}


/* 
 * callDemux -- both the NRR and senders will receive RRX requests.
 */
static xkern_return_t
rrxCallDemux(
	XObj	self,
	XObj	lls,
	Msg	*m,
	Msg	*rm)
{
    RrxReqMsg	req;
    RrxRepMsg	rep;

    xTrace0(rrxp, TR_EVENTS, "rrxCallDemux");
    if (rrxLoadRequest(&req, m) == XK_FAILURE) {
	xTrace0(rrxp, TR_ERRORS, "rrxCallDemux: msgPop fails");
	return XK_FAILURE;
    }
    rep.type = req.type;
    switch ( req.type ) {
	case RECEIVE_LOCK_REQ:
	    lockForMovingReceiver(self, &req, &rep, lls);
	    break;

	case RECEIVE_UNLOCK_REQ:
	    unlockForMovingReceiver(self, &req, &rep, lls);
	    break;

	case RECEIVE_RIGHT_TRANSFER:
	    receiveNewReceiveRight(self, &req, &rep);
	    break;

	default:
	    xTrace1(rrxp, TR_ERRORS,
		"rrxDemux -- unknown msg type %d", req.type);
	    rep.replyCode = RRX_FAILURE;
    }
    rrxReqDispose(&req);
    msgPush(rm, rrxStoreReply, &rep, RRX_REPMSG_NETLEN, 0);
    return XK_SUCCESS;
}


/* 
 * This is executed on the host holding a send right to the port.
 */
static void
lockForMovingReceiver(
	XObj		self,
	RrxReqMsg	*req,
	RrxRepMsg	*rep,
	XObj		lls)
{
    mnetport	*npd;	/* network port descriptor */

    rep->replyCode = RRX_FAILURE;
    {
	mportNetRep		netport;

	bzero((char *)&netport, sizeof(netport));
	netport.net_port_number = req->portNumber;
	if (findNetPort(&netport, req->archTag, FALSE, &npd) == XK_FAILURE) {
	    xTrace0(rrxp, TR_SOFT_ERRORS,
		"could not find port for lock request");
	    return;
	}
    }
    xTrace1(rrxp, TR_EVENTS, "lock request for port %d", npd->net_port_number);

#if 0
    /* 
     * XXX -- is this the right check to be making?
     */
    if (!(npd->net_port_rights & MACH_PORT_TYPE_SEND)) {
	xTrace0(rrxp, TR_ERRORS,
	    "request for locking port without send right");
	return;
    }
#endif /* 0 */

    switch ( npd->net_port_type ) {
	case MN_FORWARDING:
	case MN_VALID: {
	    xTrace3(rrxp, TR_EVENTS,
		"RRX Getting a WRITER LOCK on port %x (readers = %d) (writer = %d)",
		npd->net_port_number,
	    npd->rwlock.activeReaders, npd->rwlock.activeWriter);
	    writerLock(&npd->rwlock);
	    if ( npd->net_port_type != MN_INVALID ) {
		npd->old_net_port_type = npd->net_port_type;
		xTrace1(rrxp, TR_EVENTS,
		    "RRX Setting mode to MOVING_RECEIVER",
		    npd->net_port_number);
		npd->net_port_type = MN_MOVING_RECEIVER;
		xferLockedMapAdd(self, lockedMap, &npd->receiver_host_addr,
		npd, 0);
		break;
	    } else {
		xTrace1(rrxp, TR_EVENTS,
		    "RRX Releasing a WRITER LOCK on port %x",
		    npd->net_port_number);
		writerUnlock(&npd->rwlock);
		/* 
		 * Fall-through
		 */
	    }
	}
	case MN_MOVING_RECEIVER: {
	    xTrace1(rrxp, TR_EVENTS,
		"SRX: port %d already locked", npd->net_port_number);
	    /* 
	     * We don't consider this an error.  It is possible for the
	     * same host to be on the senders list under two different
	     * host ID's, so we'll only lock it once.
	     *
	     * This is also the path taken when the receiver host is on
	     * the senders list.
	     */
	    rep->replyCode = RRX_SUCCESS;
	    return;
	}
	case MN_INVALID: {
	    xTrace1(rrxp, TR_EVENTS,
		"SRX: port %d deallocated before it could be locked",
		npd->net_port_number);
	    return;
	}
	default: {
	    sprintf(errBuf,
		"netIpcRrx lock request while in %s mode",
		netPortStateStr(npd->net_port_type));
	    xError(errBuf);
	    return;
	}
    }
    rep->replyCode = RRX_SUCCESS;
}


static void
unlockForMovingReceiver(
	XObj		self,
	RrxReqMsg	*req,
	RrxRepMsg	*rep,
	XObj		lls)
{
    mnetport	*npd;	/* network port descriptor */

    xAssert(req->type == RECEIVE_UNLOCK_REQ);
    rep->replyCode = RRX_SUCCESS;
    {
	mportNetRep		netport;

	bzero((char *)&netport, sizeof(netport));
	netport.net_port_number = req->portNumber;
	if ( findNetPort(&netport, req->archTag, FALSE, &npd) == XK_FAILURE ) {
	    xTrace0(rrxp, TR_SOFT_ERRORS,
		"RRX -- could not find port for unlock request");
	    rep->replyCode = RRX_FAILURE;
	    return;
	}
    }
    if (npd->net_port_type != MN_MOVING_RECEIVER ) {
	/* 
	 * We don't consider this an error (see comment in
	 * lockForMovingReceiver.)  In the case where the receiver is
	 * on the senders list, it will be in FORWARDING_QUEUE state
	 * at this point.
	 */
	xTrace2(rrxp, TR_EVENTS,
	    "RRX unlock request for port %d in state %s",
	    npd->net_port_number, netPortStateStr(npd->net_port_type));
	return;
    }
    xferLockedMapRemove(lockedMap, &npd->receiver_host_addr, npd, portUnlocked);
    if (xferConfirmBid(self, &req->u.unlock.nrr) ) {
	npd->receiver_host_addr = req->u.unlock.nrr.h;
	receiverMoved(npd);
    } else {
	receiveRightDeallocated(npd);
    }
}


static void
portUnlocked(
	mnetport	*np,
	void		*arg)
{
    xTrace1(rrxp, TR_DETAILED,
	"RRX unlockCallBack, port %d", np->net_port_number);
    xTrace1(rrxp, TR_EVENTS,
	"RRX Releasing a WRITER LOCK on port %x", np->net_port_number);
    writerUnlock(&np->rwlock);
    np->net_port_type = np->old_net_port_type;
}


/* 
 * This message is received by the new receiver.  Add the local right,
 * verify the senders, and add state to watch ORR for reboots before
 * the actual (machIPC) transfer message arrives.
 */
static void
receiveNewReceiveRight(
	XObj		self,
	RrxReqMsg	*req,
	RrxRepMsg	*rep)
{
    mnetport	*npd;
    int		i;

    xAssert( req->type == RECEIVE_RIGHT_TRANSFER );
    {
	mportNetRep		netport;

	netport.net_port_number = req->portNumber;
	netport.net_port_rights = MACH_PORT_TYPE_RECEIVE;
	netport.receiver_host_addr = myHost;
	netport.make_send_count = 0;

	if ( findNetPort(&netport, req->archTag, TRUE, &npd) == XK_FAILURE ) {
	    xError("RRX rcvNewRcvRight -- findNetPort fails!");
	    rep->replyCode = RRX_FAILURE;
	    return;
	}
	updatePortId(npd, req->u.transfer.msgId, req->u.transfer.orr.h);
    }
    xTrace1(rrxp, TR_EVENTS,
	"RRX receive new receive right for port %d", npd->net_port_number);
    xTrace1(rrxp, TR_MORE_EVENTS,
	"RRX -- %d senders", req->u.transfer.numSenders);
    /* 
     * Add senders to local structure
     */
    for ( i=0; i < req->u.transfer.numSenders; i++ ) {
	XferHost	*xh;
	int		sendCount;

	xh = &req->u.transfer.senders[i].xh;
	sendCount = req->u.transfer.senders[i].sendCount;
	if ( xferConfirmBid(self, xh) ) {
	    xTrace1(rrxp, TR_MORE_EVENTS,
		"RRX -- sender %s BID confirmed", ipHostStr(&xh->h));
	    addNewSender(npd, xh->h, sendCount);
	    xTrace2(rrxp, TR_MORE_EVENTS,
		"RRX -- set make-send-count for %s to %d", 
		ipHostStr(&xh->h), sendCount);
	} else {
	    xTrace1(rrxp, TR_SOFT_ERRORS,
		"RRX -- sender %s rebooted", ipHostStr(&xh->h));
	}
    }
    /* 
     * Keep track of ORR reboots
     */
    if ( ! xferConfirmBid(self, &req->u.transfer.orr) ) {
	xTrace1(rrxp, TR_SOFT_ERRORS,
	    "RRX -- ORR %s rebooted", ipHostStr(&req->u.transfer.orr.h));
	removeReceiveRight(npd);
	rep->replyCode = RRX_FAILURE;
	return;
    }
    xferTransferMapAdd(self, xferMap, &req->u.transfer.orr.h,
	req->u.transfer.msgId, npd);
    rep->replyCode = RRX_SUCCESS;
}


static int
rrxControl(
	XObj	self,
	int	op,
	char	*buf,
	int	len)
{
    switch ( op ) {
	/* 
	 * Catch 'peer rebooted' messages from BIDCTL
	 */
	case BIDCTL_FIRST_CONTACT:
	    return 0;

	case BIDCTL_PEER_REBOOTED: {
	    BidctlBootMsg	*bm = (BidctlBootMsg *)buf;

	    xTrace1(rrxp, TR_EVENTS,
		"rrxControl -- peer %s rebooted", ipHostStr(&bm->h));
	    xferPeerRebooted(self, &bm->h, lockedMap, xferMap,
		removeReceiveRight, portUnlocked);
	    xTrace0(rrxp, TR_DETAILED, "End RRX reboot handler");
	    return 0;
	}
	default:
	    return xControl(xGetDown(self, XFER_XPORT_I), op, buf, len);
    }
}


/* 
 * Called to notify us that a message carrying complicated port rights
 * (i.e., rights that we processed) has arrived and the network rights
 * have been converted to mach port rights.  We can remove the
 * temporary rights that we held.
 */
void
rrxTransferComplete(
	IPhost	peer,
	MsgId	msgId)
{
    xTrace2(rrxp, TR_EVENTS,
	"RRX notes arrival of message %d from %s", msgId, ipHostStr(&peer));
    xferTransferMapRemove(xferMap, &peer, msgId);
}
