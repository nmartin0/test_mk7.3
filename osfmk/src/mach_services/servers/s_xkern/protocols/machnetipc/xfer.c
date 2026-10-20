
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
 * xfer.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.13
 * Date: 1993/02/01 22:35:12
 */
/* 
 * Mach port transfer protocols
 */

/* 
 * see xfer.h for the interface description
 */
#include <xkern/include/xkernel.h>
#include <xkern/include/prot/bidctl.h>
#include <s_xkern/include/prot/xfer.h>

/* ZZZ : it should be "global", not internal */
#include <s_xkern/include/prot/machripc.h>
#include <s_xkern/include/prot/machripc_xfer.h>


typedef struct {
    XferRemFunc	f;
    IPhost	*h;
} CallBackInfo;


static int
	abortLock( void *, void *, void * );

static int
	abortPort( void *, void *, void * );

static int
	abortTransfer( void *, void *, void * );

static int
	findEntry( void *, void *, void * );

static int
	mapIsEmpty( Map );

static xkern_return_t
	regBidInterest( XObj, IPhost * );

static void
	releaseMapOnReboot(
	    XObj, Map, MapForEachFun, IPhost *, void *, char *);

static xkern_return_t
	unregBidInterest( XObj, IPhost * );


/* 
 * XFER maps -- internal structure:
 *
 * transferMap -- { IPhost -> { msgId -> { (npd *) -> reference count} } }.
 * Contains network ports for rights being transferred to this host.
 *
 * lockedMap -- { IPhost -> { (npd *) -> (void *) } }.
 * Contains ports which are locked by remote hosts.  The final bound
 * object (the void *) is supplied in the bind and can be returned in
 * a callback when unbound.
 */

/* 
 * Size for the internal maps in the 'transfer' map chain, keyed by
 * msgId.  These maps are closed when the message arrives, so the size
 * need only reflect ports whose messages have yet to arrive.
 */
#define XFER_MSG_MAP_SZ		5

/* 
 * Size for the interior maps in the 'transfer' map chain, 
 * keyed on descriptors for individual ports being transferred
 * in a single message 
 */
#define XFER_XFER_PD_MAP_SZ	1

/* 
 * Size for the interior maps in the 'locked' map chain, 
 * keyed on descriptors for ports locked by a single host.
 */
#define XFER_LOCKED_PD_MAP_SZ	5

/* 
 * Size for the map keyed by hosts currently locking ports.
 */
#define XFER_LOCKED_MAP_SZ	13

/* 
 * Size for the transfer map keyed by hosts currently transferring
 * port rights. 
 */
#define XFER_TRANSFER_MAP_SZ	13

int	tracexferp;

/* 
 * description in xfer.h
 */
XObj
xferOpen(
	XObj	self,
	IPhost	*dst,
	BootId	*bidPtr)
{
    Part		p;
    XObj		lls, bidctl, xport;
    BidctlBootMsg	bm;
    BootId		bidSave;

    bidctl = xGetDown(self, XFER_BIDCTL_I);
    xAssert(xIsProtocol(bidctl));
    xport = xGetDown(self, XFER_XPORT_I);
    xAssert(xIsProtocol(xport));
    partInit(&p, 1);
    partPush(p, dst, sizeof(IPhost));
    bm.h = *dst;
    bm.id = 0;
    /* 
     * Compare peer BID's before and after opening the lls to avoid
     * transferring rights immediately after the peer reboots.
     */
    /* 
     * This call won't block in real usage ...
     */
    if ( xControl(bidctl, BIDCTL_GET_PEER_BID_BLOCKING, (char *)&bm,
	sizeof(bm)) < (int)sizeof(bm) ) {
	xTrace1(xferp, TR_ERRORS,
	    "xferOpen couldn't get BID of right recipient %s",
	    ipHostStr(&bm.h));
	return ERR_XOBJ;
    }
    bidSave = bm.id;
    bm.id = 0;
    if ( (lls = xOpen(self, self, xport, &p, xMyPath(self))) == ERR_XOBJ ) {
	xTrace1(xferp, TR_SOFT_ERRORS,
	    "xferOpen could not open lls for host %s", ipHostStr(dst));
	return ERR_XOBJ;
    }
    if ( xControl(bidctl, BIDCTL_GET_PEER_BID,
	(char *)&bm, sizeof(bm)) < (int)sizeof(bm) ) {
	xTrace1(xferp, TR_ERRORS,
	    "xferOpen couldn't get BID of right recipient %s",
	    ipHostStr(&bm.h));
	xClose(lls);
	return ERR_XOBJ;
    }
    if ( bm.id != bidSave ) {
	xTrace1(xferp, TR_ERRORS,
	    "xferOpen -- right recipient %s rebooted before transfer",
	    ipHostStr(&bm.h));
	xClose(lls);
	return ERR_XOBJ;
    }
    /* 
     * At this point we know that this channel is talking to the same
     * incarnation of the peer as was alive when the transfer was
     * requested.  If the peer reboots, sends on this channel will fail.
     */
    if ( bidPtr ) {
	*bidPtr = bidSave;
    }
    return lls;
}


/* 
 * description in xfer.h
 */
int
xferConfirmBid(
	XObj		self,
	XferHost	*xh)
{
    BidctlBootMsg	bmsg;
    XObj		bidctl;

    bidctl = xGetDown(self, XFER_BIDCTL_I);
    if ( ! xIsProtocol(bidctl) ) {
	xTrace0(xferp, TR_ERRORS, "xferConfirmBid -- protl is misconfigured");
	return 0;
    }
    bmsg.h = xh->h;
    bmsg.id = xh->bid;
    if ( xControl(bidctl, BIDCTL_GET_PEER_BID_BLOCKING,
	(char *)&bmsg, sizeof(bmsg)) < (int)sizeof(bmsg) ) {
	xTrace1(xferp, TR_ERRORS,
	    "xferConfirm could not get BID of peer %s", ipHostStr(&xh->h));
	return 0;
    }
    xTrace3(xferp, TR_MORE_EVENTS,
	"xferConfirm BID of host %s -- \n\tmsg == %x, cur == %x",
	ipHostStr(&xh->h), xh->bid, bmsg.id);
    return xh->bid == bmsg.id;
}


/* 
 * description in xfer.h -- see map structure description above
 */
void
xferLockedMapAdd(
	XObj		self,
	Map		lockedMap,
	IPhost		*peer,
	mnetport	*port,
	void		*arg)
{
    Map		pdMap;
    Bind	bind;

    if ( mapResolve(lockedMap, peer, &pdMap) == XK_FAILURE ) {
	xTrace1(xferp, TR_MORE_EVENTS,
	    "creating lock submap for peer %s",
	    ipHostStr(peer));
	pdMap = mapCreate(XFER_LOCKED_PD_MAP_SZ, sizeof(mnetport *));
	bind = mapBind(lockedMap, peer, pdMap);
	xAssert(bind != ERR_BIND);
	regBidInterest(self, peer);
    }
    bind = mapBind(pdMap, &port, arg);
    xAssert(bind != ERR_BIND);
}


/* 
 * description in xfer.h -- see map structure description above
 */
void
xferLockedMapRemove(
	Map		lockedMap,
	IPhost		*peer,
	mnetport	*port,
	LockRemFunc	callBack)
{
    Map			pdMap;
    xkern_return_t	res;
    void		*arg;

    if ( mapResolve(lockedMap, peer, &pdMap) == XK_FAILURE ) {
	xTrace0(xferp, TR_ERRORS, "xfer unlock -- no port map!!");
    } else {
	if ( callBack ) {
	    if ( mapResolve(pdMap, &port, &arg) == XK_SUCCESS ) {
		callBack(port, arg);
	    }
	}
	if ( mapUnbind(pdMap, &port) == XK_FAILURE ) {
	    xTrace0(xferp, TR_ERRORS,
		"xferLockedMapRemove -- could not unbind");
	}
	if ( mapIsEmpty(pdMap) ) {
	    xTrace1(xferp, TR_EVENTS,
		"Closing locked map for host %s", ipHostStr(peer));
	    res = mapUnbind(lockedMap, peer);
	    xAssert( res == XK_SUCCESS );
	    mapClose(pdMap);
	} else {
	    xTrace0(xferp, TR_EVENTS, "locked map is not empty");
	}
    }
}


/* 
 * description in xfer.h -- see map structure description above
 */
void
xferTransferMapAdd(
	XObj		self,
	Map		xferMap,
	IPhost		*peer,
	MsgId		msgId,
	mnetport	*port)
{
    Bind	bind;
    Map		pdMap, msgMap;
    void	*rcnt;

    if ( mapResolve(xferMap, peer, &msgMap) == XK_FAILURE ) {
	xTrace1(xferp, TR_MORE_EVENTS,
	    "creating transfer submap for peer %s",
	    ipHostStr(peer));
	msgMap = mapCreate(XFER_MSG_MAP_SZ, sizeof(MsgId));
	bind = mapBind(xferMap, peer, msgMap);
	xAssert(bind != ERR_BIND);
	regBidInterest(self, peer);
    }
    if ( mapResolve(msgMap, &msgId, &pdMap) == XK_FAILURE ) {
	xTrace1(xferp, TR_MORE_EVENTS,
	    "creating message submap for msg %d", msgId);
	pdMap = mapCreate(XFER_XFER_PD_MAP_SZ, sizeof(mnetport *));
	bind = mapBind(msgMap, &msgId, pdMap);
	xAssert(bind != ERR_BIND);
    }
    if ( mapResolve(pdMap, &port, &rcnt) == XK_SUCCESS ) {
	mapUnbind(pdMap, &port);
	mapBind(pdMap, &port, (void *)(((int)rcnt) + 1));
	xTrace1(xferp, TR_EVENTS,
	    "increasing port transfer refcount to %d",
	    ((int)rcnt) + 1);
    } else {
	xTrace0(xferp, TR_EVENTS,
	    "adding new entry for this port to msg map");
	mapBind(pdMap, &port, (void *)1);
    }
}


/* 
 * description in xfer.h -- see map structure description above
 */
void
xferTransferMapRemove(
	Map	xferMap,
	IPhost	*peer,
	MsgId	msgId)
{
    Map			msgMap, pdMap;
    xkern_return_t	res;

    if ( mapResolve(xferMap, peer, &msgMap) == XK_FAILURE ) {
	xTrace1(xferp, TR_ERRORS,
	    "transfer complete notification -- no host map for msg %s",
	    ipHostStr(peer));
	return;
    }
    if ( mapResolve(msgMap, &msgId, &pdMap) == XK_FAILURE ) {
	xTrace1(xferp, TR_ERRORS,
	    "transfer complete notification -- no port map for msg %d", msgId);
	return;
    }
    xTrace1(xferp, TR_EVENTS,
	"xferTransferMapRemove closing map for message %d", msgId);
    res = mapUnbind(msgMap, &msgId);
    xAssert( res == XK_SUCCESS );
    mapClose(pdMap);
    if ( mapIsEmpty(msgMap) ) {
	xTrace1(xferp, TR_EVENTS,
	    "Closing empty xfer map for host %s", ipHostStr(peer));
	res = mapUnbind(xferMap, peer);
	xAssert( res == XK_SUCCESS );
	mapClose(msgMap);
    } else {
	xTrace0(xferp, TR_EVENTS, "xfer map is not empty");
    }
}


/* 
 * This is a MapForEachFun applied to the last map in the
 * 'transferMap' map chain:
 *	key == mnetport *
 *	val == reference count
 *	arg == abort function pointer
 *
 * The port (the key) is, in fact, not going to be
 * transferred.   We need to tell the port manager to remove the port
 * right by calling the abort function.
 */
static int
abortPort(
	void	*key,
	void	*val,
	void	*arg)
{
    int		i;
    XferRemFunc	cb = (XferRemFunc)arg;

    xAssert(cb);
    xTrace2(xferp, TR_EVENTS,
	"port number %d, refs == %d",
	(*(mnetport **)key)->net_port_number, (int)val);
    for ( i=0; i < (int)val; i++ ) {
	cb(*(mnetport **)key);
    }
    return 1;
}


/* 
 * This is a mapForEachFun applied to the middle map in the
 * 'transferMap' map chain:
 *	key == msgId *
 *	val == interior map
 *
 * The rights in the interior map are to be deallocated and the
 * interior map closed.
 */
static int
abortTransfer(
	void	*key,
	void	*val,
	void	*arg)
{
    xIfTrace(xferp, TR_EVENTS) {
	xTrace0(xferp, TR_EVENTS, "aborting transfer of ports:");
	mapForEach((Map)val, abortPort, arg);
    }
    mapClose((Map)val);
    return MFE_CONTINUE;
}


/* 
 * This is a mapForEachFun applied to the interior map in the
 * 'lockedMap' map chain:
 *	key == mnetport *
 *
 * We unlock this port.
 */
static int
abortLock(
	void	*key,
	void	*val,
	void	*arg)
{
    mnetport	*np = *(mnetport **)key;
    LockRemFunc	cb = (LockRemFunc)arg;

    xAssert( np );
    xAssert( cb );
    xTrace1(xferp, TR_EVENTS,
	"XFER aborting lock on port %d", np->net_port_number);
    cb(np, val);
    return MFE_CONTINUE;
}


/* 
 * 'topMap' is either a transferMap or a lockedMap.  'host' is a peer
 * that rebooted.  If the top-level map has locked or transferred
 * ports, appropriate 'release' actions will be taken.  
 */
static void
releaseMapOnReboot(
	XObj		self,
	Map		topMap,
	MapForEachFun	f,
	IPhost		*peer,
	void		*arg,
	char		*txt)
{
    Map			subMap;
    xkern_return_t	res;

    if ( mapResolve(topMap, peer, &subMap) == XK_SUCCESS ) {
	xTrace1(xferp, TR_EVENTS,
	    "XFER reboot handler -- %s", txt);
	mapForEach(subMap, f, arg);
	mapClose(subMap);
	res = mapUnbind(topMap, peer);
	xAssert( res == XK_SUCCESS );
	/* 
	 * XXX -- Schedule this
	 */
	unregBidInterest(self, peer);
    }
}


/* 
 * description in xfer.h
 */
void
xferPeerRebooted(
	XObj		self,
	IPhost		*peer,
	Map		lockedMap,
	Map		transferMap,
	XferRemFunc	xrf,
	LockRemFunc	lrf)
{
    /* 
     * Unlock ports which were locked by the rebooted host. 
     */
    releaseMapOnReboot(
	self, lockedMap, abortLock, peer, (void *)lrf, "aborting locks");
    /* 
     * Release rights which were being transferred to 
     * this host from the rebooted host
     */
    releaseMapOnReboot(self,
	transferMap, abortTransfer, peer, (void *)xrf, "aborting transfers");
}


/* 
 * Unregister 'protl's interest in 'peer's reboot by openDisabling the
 * BootId protocol
 */
static xkern_return_t
unregBidInterest(
	XObj	protl,
	IPhost	*peer)
{
    XObj	llp;
    Part	p;

    xAssert(xIsProtocol(protl));
    xTrace1(xferp, TR_MAJOR_EVENTS,
	"xfer unregistering interest in peer %s with bidctl", ipHostStr(peer));
    llp = xGetDown(protl, XFER_BIDCTL_I);
    xAssert(xIsProtocol(llp));
    partInit(&p, 1);
    partPush(p, (void *)peer, sizeof(IPhost));
    return xOpenDisable(protl, protl, llp, &p);
}


/* 
 * Register 'protl's interest in 'peer's reboot by openEnabling the
 * BootId protocol
 */
static xkern_return_t
regBidInterest(
	XObj	protl,
	IPhost	*peer)
{
    XObj	llp;
    Part	p;

    xAssert(xIsProtocol(protl));
    xTrace1(xferp, TR_MAJOR_EVENTS,
	"xfer registering interest in peer %s with bidctl", ipHostStr(peer));
    llp = xGetDown(protl, XFER_BIDCTL_I);
    xAssert(xIsProtocol(llp));
    partInit(&p, 1);
    partPush(p, (void *)peer, sizeof(IPhost));
    return xOpenEnable(protl, protl, llp, &p);
}


/* 
 * description in xfer.h
 */
void
xferCreateMaps(
	Map	*lockedMap,
	Map	*xferMap)
{
    *lockedMap = mapCreate(XFER_LOCKED_MAP_SZ, sizeof(IPhost));
    *xferMap = mapCreate(XFER_TRANSFER_MAP_SZ, sizeof(IPhost));
}


char *
xferHostStr(
	XferHost	*rh)
{
    static char		buf[80];

    sprintf(buf, "%s  (BID == %x)", ipHostStr(&rh->h), rh->bid);
    return buf;
}


static int
findEntry(
	void	*key,
	void	*val,
	void	*arg)
{
    xTrace0(xferp, TR_FULL_TRACE, "xfer findEntry");
    *(void **)arg = key;
    return 0;
}


static int
mapIsEmpty(
	Map	m)
{
    void	*key;

    key = 0;
    mapForEach(m, findEntry, &key);
    return key == 0;
}

