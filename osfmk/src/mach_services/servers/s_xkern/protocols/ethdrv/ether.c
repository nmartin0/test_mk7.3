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
 */
/*
 * cmk1.1
 */
/*     
 * ether.c,v
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * ether.c,v
 * Revision 1.54.2.4.1.2  1994/09/07  04:18:12  menze
 * OSF modifications
 *
 * Revision 1.54.2.4  1994/09/02  21:47:07  menze
 * mapBind and mapVarBind return xkern_return_t
 *
 * Revision 1.54.2.3  1994/09/01  04:15:54  menze
 * Meta-data allocations now use Allocators and Paths
 * VCI_ETH_NET_TYPE stored rather than recalculated
 *
 * Revision 1.54.2.2  1994/07/21  23:35:54  menze
 * Modified to use Msg-free allocator interface
 * vci option has moved into the Path module
 *
 * Revision 1.54.2.1  1994/07/21  23:33:26  menze
 *   [ 1994/04/05          menze ]
 *   Only need to copy ETH_HDR_LEN bytes of the header, not all 64
 *
 *   [ 1994/03/31          menze ]
 *   Builds with allocator and message tool mods
 *
 *   [ 1994/03/11          menze ]
 *   Initial cut at providing for multiple input pools
 *
 * Revision 1.54  1994/01/07  18:49:47  ho
 * Fixed filter bug introduced a couple of versions ago; made output
 * buffers part of protocol state instead of globals.
 *
 * Revision 1.53  1994/01/07  01:53:49  menze
 * RomOpt changes were allowing access to typeMap before
 * initialization.
 *
 */

#include <sys/file.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <mach.h> 
#include <cthreads.h> 
#include <device/device.h>
#include <device/net_status.h> 
#include <device/device_request.h>
#include <mach_error.h>
#include <mach/mach_host.h>
#include <mach/mach_traps.h>
#include <mach/message.h>

#include <xkern/include/xkernel.h>
#include <xkern/include/domain.h>
#include <xkern/include/assert.h>
#include <xkern/include/xk_debug.h>
#include <xkern/include/upi.h>
#include <xkern/include/list.h>
#include <xkern/include/prot/eth.h>
#include <xkern/protocols/eth/eth_i.h>
#include <s_xkern/include/eth_support.h>


#define ETHDRVMAPSIZE 16
#define ETHER_OUT_BUFFERS 32

struct net_status ether_net_status;

/*
 * Set default filter priority. >100 means noone else gets the packet.
 */
#define FILTER_PRIORITY_DEFAULT	20

#define XK_INTERFACE_PORT_QLIMIT	MACH_PORT_QLIMIT_MAX

/*
 * Defining TEST_TYPE_FILTER will enable code which filters packets on
 * type before creating processes.  The types in the filter can only
 * be configured once at boot time.
 */
#if XK_DEBUG
#  define TEST_TYPE_FILTER
#endif

/*
 * the device string is now part of the protocol name, e.g. ethdrv/SE0
 */
#ifndef	DEVICE_STR
#define DEVICE_STR	"SE"
#endif	/* !DEVICE_STR */

#define MAX_DEVICE_STRING	5

#define ETHER_INPUT_MAX (ROUND4(MAX_ETH_DATA_SZ) + ROUND4(NET_HDW_HDR_MAX) + 4)
#define BUFFER_ALLOC_SIZE	(ETHER_INPUT_MAX + ROUND4(sizeof(struct mach_hdrs)))

#define	DEF_POOL_BUFFERS	32
#define DEF_POOL_THREADS	16
#define BUFFERS_PER_VCI		16
#define THREADS_PER_VCI		8


/*
 *  The state structure for the ethernet driver;
 *    each interface needs a separate state block
 */
typedef struct estate {
  char		*ebuffers[ETHER_OUT_BUFFERS];
  int		ebuffindex;
  mach_port_t	interface_port;
  mach_port_t	filter_port;
#ifdef TEST_TYPE_FILTER
  Map           typeMap;
#endif
  int	        ethFilterPriority;
  ETHhost	bcast;
  ETHhost	myetheraddr;
  boolean_t	initialized;
  char          devname[MAX_DEVICE_STRING];
} PState;

/* global data of ethernet protocol */

#if XK_DEBUG
int	traceethdrvp; 	/* ethdrv == ethernet driver */
#endif /* XK_DEBUG */

ETHhost	stdbcast = BCAST_ETH_AD;

/* 
 * Right now, one set of pools is shared by all instances of this driver
 */
static	Map		poolMap;
#define POOL_MAP_SIZE	32

typedef struct {
    int		buffers;
    int 	threads;
    Allocator	alloc;
} InputInfo;


xkern_return_t
	ethdrv_init( XObj self );

static xmsg_handle_t
	eth_push( XObj self, Msg msg );

static int 
	eth_control( XObj self, int op, char *buf, int len );

#if	XK_DEBUG
static void 
	dumpIncomingPacket( struct mach_hdrs *, int );
#endif /* XK_DEBUG */


static xkern_return_t
xferBuff( EthInputPool *fromPool, EthInputPool *toPool, EthInputBuffer **buf )
{
    EthInputBuffer *newBuf;
    int		bufLen;

    /* 
     * We should probably eventually try to do an allocXfer here ...
     */
    xTrace0(ethdrvp, TR_EVENTS, "ethdrv xferBuff");
    if ( pqEmpty(toPool->freeQueue) ) {
	return XK_FAILURE;
    }
    pqRemove(toPool->freeQueue, newBuf);
    bufLen = ((struct mach_hdrs *)(*buf)->data)->msg_hdr.msgh_size;
    xTrace1(ethdrvp, TR_MORE_EVENTS, "transferring %d bytes to new header",
	    bufLen);
    bcopy((*buf)->data, newBuf->data, bufLen);
    pqAppend(fromPool->freeQueue, *buf);
    *buf = newBuf;
    xkPoolCheck(fromPool);
    xkPoolCheck(toPool);
    return XK_SUCCESS;
}



/*
 *  ether2demux
 *
 *	The permanent reading thread
 *	This is not master_locked.
 */
static int
ether2demux(
	XObj	self)
{
    kern_return_t 	kr;
    EthInputBuffer      *buf;
    struct mach_hdrs    *mh;
    EthInputPool		*lastPool, *newPool, *defaultPool;
    VciType		lastVci;
    xkern_return_t	xkr;
#if XK_DEBUG
    int			packetCount = 0;
#endif /* XK_DEBUG */
    int			priority = XK_DRIVER_THREAD_PRIORITY;

    cthread_set_name(cthread_self(), "ether_thread");
    /* 
     * If the xkernel is limiting the number of cthreads, increase
     * this limit by 1 (for the kernel thread we will wire to this
     * input cthread.)
     */
    xkIncreaseCthreadLimit(1);
    cthread_wire();
    xkPolicyFixedFifo(&priority, sizeof(priority));
    lastVci = 0;
    xkr = mapResolve(poolMap, &lastVci, &defaultPool);
    if ( xkr != XK_SUCCESS || ! defaultPool ) {
	xTraceP0(self, TR_ERRORS, "Could not get default pool");
	return 0;
    }
    lastPool = defaultPool;
    
    while (1) {
	int 		received_size;
	VciType		vci;
	ETHtype	        type;
	
	pqRemove( lastPool->freeQueue, buf );
	xkPoolCheck(lastPool);
	mh = (struct mach_hdrs *)buf->data;  /* hope this is aligned */
	kr = mach_msg(
	    (mach_msg_header_t *)mh,
	    MACH_RCV_MSG,
	    0,
	    ROUND4(sizeof(struct mach_hdrs)
		+ ROUND4(ether_net_status.max_packet_size))
		+ MAX_TRAILER_SIZE,			/* receive_limit */
	    ((PState *)self->state)->filter_port,	/* receive_name */
	    MACH_MSG_TIMEOUT_NONE,
	    MACH_PORT_NULL);
	CLOCK_TRACE("ether2demux: message received");
	if (kr != MACH_MSG_SUCCESS) {
	    xTrace2(ethdrvp, TR_ALWAYS,
		    "ether.ether2demux.mach_msg_receive() failed: %d (0x%x)\n",kr, kr);
	    pqAppend( lastPool->freeQueue, buf );
	    continue;
	}
#if	XK_DEBUG
	dumpIncomingPacket(mh, ++packetCount);
#endif /* XK_DEBUG */
#ifdef TEST_TYPE_FILTER
	/*
	 * Make sure the type is one that we accept
	 */
	{
	    ETHtype	type = ntohs(((ETHhdr *)mh->header)->type);

	    if ( mapResolve(((PState *)self->state)->typeMap, &type, 0)
			== XK_SUCCESS ) {
		xTrace1(ethdrvp, TR_MORE_EVENTS,
			"eth type %x blocked by type filter", type);
		pqAppend( lastPool->freeQueue, buf );
		continue;
	    } else {
		xTrace1(ethdrvp, TR_MORE_EVENTS,
		    "ethdrv ether2demux eth type %x passes the type filter",
		    type);
	    }
	}
#endif /* TEST_TYPE_FILTER */
	vci = (((ETHhdr *)mh->header)->type == vciEthNetType) ? 
	  	ntohs(*(VciType *)(mh + 1)) : 0;
	xIfTrace(vci, TR_EVENTS) {
	    printf("in: %d ", vci);
	    if ( vci == 0 ) {
		printf("(%x) ", (u_short)ntohs(((ETHhdr *)mh->header)->type));
	    } 
	}
	xTraceP1(self, TR_EVENTS, "Incoming packet VCI == %d", vci);
	if ( vci != lastVci ) {
	    xTraceP2(self, TR_EVENTS, "Changing buffer pools from %d to %d",
		     lastVci, vci);
	    if ( vci == 0 || mapResolve(poolMap, &vci, &newPool) == XK_FAILURE ) {
		xTraceP0(self, TR_EVENTS, "Using default pool");
		newPool = defaultPool;
	    } else {
		xTraceP1(self, TR_EVENTS, "Using vci-specific pool %x", newPool);
	    }
	    if ( newPool != lastPool ) {
		if ( xferBuff(lastPool, newPool, &buf) == XK_FAILURE ) {
		    xTraceP0(self, TR_SOFT_ERRORS,
			     "could not transfer buffer to new pool, dropping");
		    pqAppend( lastPool->freeQueue, buf );
		    continue;
		}
		lastPool = newPool;
	    }
	    lastVci = vci;
	}
	/* 
	 * The pool has to be able to supply a new input buffer or
	 * we'll drop
	 */
	if ( pqEmpty(lastPool->freeQueue) ) {
	    xTraceP1(self, TR_SOFT_ERRORS, "No buffers in freeQueue (%s)... dropping",
		     lastPool->name);
	    pqAppend( lastPool->freeQueue, buf );
	    continue;
	}
	bcopy((char *)&mh->header, (char *)&buf->hdr, ETH_HDR_LEN); /* set the eth hdr */
	/* 
	 * The received_size is MIN(44, ROUND4(real_length_sent + 1))  :-(
	 * At least it's always wrong.
	 */
        received_size = mh->msg_hdr.msgh_size - sizeof(struct mach_hdrs);
	buf->driverProtl = self;
	/*
	 * pop off the machipc headers; guarantee no underflow
	 */
	msgPopDiscard(&buf->upmsg, sizeof(struct mach_hdrs));
	msgTruncate(&buf->upmsg, received_size);
	msgSetAttr(&buf->upmsg, 0, &buf->hdr, sizeof(ETHhdr));
	
	/* 
	 * Add the buffer to the incoming packet queue.  It will be
	 * returned to the free queue by the shepherd thread.
	 */
	xTrace2(ethdrvp, TR_EVENTS, "enqueuing packet %x count %d", buf, packetCount);
	pqAppend( lastPool->inQueue, buf );
    }
}


#if	XK_DEBUG

static void
dumpIncomingPacket(
	struct mach_hdrs	*mh,
	int			packetCount)
{
    xTrace4(ethdrvp,TR_FUNCTIONAL_TRACE,
	    "ether in pkt(%d): msg bits=%d size=%d id=%d",
	    packetCount,
	    mh->msg_hdr.msgh_bits,
	    mh->msg_hdr.msgh_size,
	    mh->msg_hdr.msgh_id);
    xTrace3(ethdrvp,TR_FUNCTIONAL_TRACE,
	    "ether in pkt(%d): pkt length=%d type=%d",
	    packetCount,
	    mh->packet_header.length,
	    mh->packet_header.type);
    xTrace4(ethdrvp,TR_FUNCTIONAL_TRACE,
	    "ether in pkt(%d): dest 0x%04x 0x%04x 0x%04x",
	    packetCount,
	    ((ETHhdr *)mh->header)->dst.high,
	    ((ETHhdr *)mh->header)->dst.mid,
	    ((ETHhdr *)mh->header)->dst.low);
    xTrace4(ethdrvp,TR_FUNCTIONAL_TRACE,
	    "ether in pkt(%d): src 0x%04x 0x%04x 0x%04x",
	    packetCount,
	    ((ETHhdr *)mh->header)->src.high,
	    ((ETHhdr *)mh->header)->src.mid,
	    ((ETHhdr *)mh->header)->src.low);
    xTrace2(ethdrvp,TR_FUNCTIONAL_TRACE,
	    "ether in pkt(%d): type 0x%04x",
	    packetCount,
	    ((ETHhdr *)mh->header)->type);
    xTrace4(ethdrvp,TR_FULL_TRACE,
	    "ether in pkt(%d): hdr 0x%02x 0x%02x 0x%02x",
	    packetCount,
	    (u_char)mh->header[0], (u_char)mh->header[1], (u_char)mh->header[2]);
    xTrace4(ethdrvp,TR_FULL_TRACE,
	    "ether in pkt(%d): hdr 0x%02x 0x%02x 0x%02x",
	    packetCount,
	    (u_char)mh->header[3], (u_char)mh->header[4], (u_char)mh->header[5]);
    xTrace4(ethdrvp,TR_FULL_TRACE,
	    "ether in pkt(%d): hdr 0x%02x 0x%02x 0x%02x",
	    packetCount,
	    (u_char)mh->header[6], (u_char)mh->header[7], (u_char)mh->header[8]);
    xTrace4(ethdrvp,TR_FULL_TRACE,
	    "ether in pkt(%d): hdr 0x%02x 0x%02x 0x%02x",
	    packetCount,
	    (u_char)mh->header[9], (u_char)mh->header[10], (u_char)mh->header[11]);
    xTrace3(ethdrvp,TR_FULL_TRACE,
	    "ether in pkt(%d): hdr 0x%02x 0x%02x",
	    packetCount,
	    (u_char)mh->header[12], (u_char)mh->header[13]);
}

#endif /* XK_DEBUG */


#ifdef TEST_TYPE_FILTER

/*
 *  readHex
 *
 *	Read a number from 's' which may be either hex
 *	(prefixed with an x as in x34b2) or decimal.
 *	Non-zero on success, zero on failure
 */
static int
readHex(
	char	*s,
	int	*n)
{
    if ( *s == 'x' ) {
	return ( sscanf(s + 1, "%x", n) == 1 );
    } else {
	return ( sscanf(s, "%d", n) == 1 );
    }
}

#endif /* TEST_TYPE_FILTER */

#define ROMARG1 2

/* 
 * instName dev devName
 */
static xkern_return_t
readDeviceRom( self, str, nFields, line, arg )
    XObj	self;
    char	**str;
    int		nFields, line;
    void	*arg;

{
  PState	*ps = (PState *)self->state;
  int		slen;
  
  if (!str[ROMARG1]) {
    xTrace0(ethdrvp, TR_SOFT_ERRORS, "Device name missing from rom file entry; will continue with default selection");
    return XK_SUCCESS;
  }
  slen = strlen(str[ROMARG1]);
  if (slen > MAX_DEVICE_STRING) {
    xTrace0(ethdrvp, TR_ERRORS, "Device name too long");
    return XK_FAILURE;
  }
  strncpy(ps->devname, str[ROMARG1], MAX_DEVICE_STRING);
  xTrace1(ethdrvp, TR_FUNCTIONAL_TRACE, "ethdrv: ether device %s\n", ps->devname);
  xIfTrace(ethdrvp, TR_ERRORS) {
    if (slen <= 0) printf("ethdrv: null device string specified");
  }
  return (slen > 0 ? XK_SUCCESS:XK_FAILURE);
}

static xkern_return_t
readPriorityRom( self, str, nFields, line, arg )
    XObj	self;
    char	**str;
    int		nFields, line;
    void	*arg;
{
  PState	*ps = (PState *)self->state;

  return sscanf(str[ROMARG1], "%d", &ps->ethFilterPriority) >= 1?
    XK_SUCCESS : XK_FAILURE;
}

static xkern_return_t
readBlockRom( self, str, nFields, line, arg )
    XObj	self;
    char	**str;
    int		nFields, line;
    void	*arg;
{
#ifdef TEST_TYPE_FILTER
  int		n;
  ETHtype	type;
  PState	*ps = (PState *)self->state;

  if ( readHex(str[ROMARG1], &n) ){
    type = n;
    if ( mapBind(ps->typeMap, (char *)&type, 0, 0) != XK_SUCCESS ) {
	xTraceP0(self, TR_ERRORS, "mapBind failure in readBlockRom");
    }
    xTraceP1(self, TR_GROSS_EVENTS, "eth driver blocking type %x", type);
    return XK_SUCCESS;
  }
#else
  xTrace1(ethdrvp, TR_GROSS_EVENTS,
	  "eth driver ignoring type filter on line %d",
	  line);
#endif
  return XK_SUCCESS;
}

static XObjRomOpt xobjOpts[] = {
    { "dev", 3, readDeviceRom },
    { "priority", 3, readPriorityRom },
    { "block", 3, readBlockRom }
};

/*
 *	Support for generic xkernel operations
 */
/*
 *  eth_openenable
 *
 *	The openenable interface allows the driver 
 *	to get the address of the higher level protocol to which it interfaces
 */
static xkern_return_t
eth_openenable(
	XObj	self,
	XObj	hlp,
	XObj	hlptype,
	Part	part)
{
    cthread_t th;

    /*
     * this xSetUp allows the self protocol object to be used as a session
     * in a later xDemux call
     */
    xSetUp(self, hlp);

    /*
     * Since ether2demux does not use any xkernel routines, this should be 
     * allowed to remain a cthread, in spite of the sledgehammer concurrency
     * control otherwise enforced in the xkernel.
     */
    /*
     * The ether reading thread runs forever
     */
    th = cthread_fork((cthread_fn_t)ether2demux, (void *)self);
    cthread_detach(th);

    return XK_SUCCESS;
}


#define checkResult(_r, _msg)						\
    if ( kr != (_r) ) {							\
	sprintf(errBuf,							\
	    "ethdrv %s fatal initialization error: %s %d: %s",		\
	    self->name, (_msg), kr, mach_error_string(kr));		\
	xError(errBuf);							\
	if ( state->interface_port ) {					\
	    device_close(state->interface_port);			\
	}								\
	return XK_FAILURE;						\
    }			



static xkern_return_t
bufInfoHandler( XObj self, Path path, u_int msgSize, int numMsgs )
{
    int			numBuffs;
    xkern_return_t	xkr;

    xTraceP3(self, TR_EVENTS, "bufInfoHandler: %d messages of size %d, path %d",
	     numMsgs, msgSize, pathGetId(path));
    if ( msgSize > 0 ) {
	numBuffs = ((msgSize - 1) / ETHER_INPUT_MAX) + 1;
	xkr = msgReserveContig(path, numBuffs * numMsgs, BUFFER_ALLOC_SIZE, 0);
	if ( xkr == XK_FAILURE ) {
	    xTraceP0(self, TR_ERRORS, "msgReserve fails");
	    return XK_FAILURE;
	}
    }
    return XK_SUCCESS;
}


static xkern_return_t
poolInfoHandler(
    XObj 		self,
    Path 		path,
    u_int 		numBuffs,
    u_int 		numThreads,
    XkThreadPolicy	policy)
{
    EthInputPool	*p;
    char		poolName[80];
    VciType		vci;

    vci = pathGetId(path);
    if ( mapResolve(poolMap, &vci, &p) == XK_SUCCESS ) return XK_SUCCESS;
    sprintf(poolName, "vciPool %d", vci);
    if ( ! numBuffs ) {
	numBuffs = (vci == 0 ? DEF_POOL_BUFFERS : BUFFERS_PER_VCI);
    }
    if ( ! numThreads ) {
	numThreads = (vci == 0 ? DEF_POOL_THREADS : THREADS_PER_VCI);
    }
    xTraceP3(self, TR_EVENTS,
	     "establish pool for path %d, %d threads, %d bufs",
	     pathGetId(path), numThreads, numBuffs);
    p = EthInPoolInit(numBuffs, numThreads, BUFFER_ALLOC_SIZE, path, poolName,
		      policy);
    if ( p == 0 ) {
	xTrace1(ethdrvp, TR_ERRORS, "can't create input pool for VCI %d", vci);
	return XK_FAILURE;
    }
    if ( mapBind(poolMap, &vci, p, 0) != XK_SUCCESS ) {
	xTrace1(ethdrvp, TR_ERRORS, "can't bind input pool for VCI %d", vci);
	return XK_FAILURE;
    }
    return XK_SUCCESS;
}


/*
 *  etherInit
 *
 *  graph.comp: protocol name/device-unit name
 *
 */
static xkern_return_t
etherInit(self)
     XObj self;
{
    kern_return_t kr;
    unsigned 	if_stat_count;
    long	if_addr[4];
    unsigned 	if_addr_count;
    mach_port_t		bootstrapPort = MACH_PORT_NULL;
    mach_port_t		privilegedPort = MACH_PORT_NULL;
    mach_port_t		masterPort = MACH_PORT_NULL;
    mach_port_t 	ledger_wired;
    mach_port_t 	ledger_paged;
    mach_port_t 	security_port;

    mach_port_limits_t	filter_qlimit;
    filter_t 	filter[40];
    int 	filter_index = 0;
    security_token_t	null_sec_token = {{0, 0}};
    char 	*p;
    PState 	*state;
    static int	instanceCount;
    Path	path = self->path;
    
    xTrace0(ethdrvp, TR_GROSS_EVENTS, "etherInit");
    if ( ! (state = (PState *)pathAlloc(xMyPath(self), sizeof(PState))) ) {
	xTraceP0(self, TR_ERRORS, "allocation failure");
	return XK_FAILURE;
    }
    kr = task_get_bootstrap_port(mach_task_self(), &bootstrapPort);
    if (kr != KERN_SUCCESS) {
	xTrace2(ethdrvp, TR_ERRORS,
	    "ethdrv etherInit task_get_bootstrap_port 0x%x %s",
	    kr, mach_error_string(kr));
    } else {
	xTrace1(ethdrvp, TR_DETAILED,
	    "ethdrv etherInit bootstrap port 0x%x", bootstrapPort);
    }

    kr = bootstrap_ports(bootstrapPort,
	    &privilegedPort, &masterPort, &ledger_wired, &ledger_paged,
	    &security_port);
    if (kr != KERN_SUCCESS) {
	xTrace2(ethdrvp, TR_ERRORS,
	    "ethdrv etherInit bootstrap_ports 0x%x %s",
	    kr, mach_error_string(kr));
    } else {
	xTrace1(ethdrvp, TR_DETAILED,
	    "ethdrv etherInit device port 0x%x", masterPort);
    }

    bzero((char *)state, sizeof(PState));
    self->state = state;
    state->ethFilterPriority = FILTER_PRIORITY_DEFAULT;
    state->bcast = stdbcast;
    state->initialized = FALSE;
#ifdef TEST_TYPE_FILTER
    state->typeMap = mapCreate(ETHDRVMAPSIZE, sizeof(ETHtype), path);
    if ( state->typeMap == 0 ) {
	xTraceP0(self, TR_ERRORS, "allocation failure");
	return XK_FAILURE;
    }
#endif
    if ( poolMap == 0 ) {
	poolMap = mapCreate(POOL_MAP_SIZE, sizeof(VciType), path);
    }
    findXObjRomOpts(self, xobjOpts, sizeof(xobjOpts)/sizeof(XObjRomOpt), 0);
    if ( instanceCount++ == 0 ) {
	pathRegisterDriver(self, poolInfoHandler, bufInfoHandler);
    }
    if (!state->devname[0]) {
      /* set default device name "SE0" */
      strcpy(state->devname, DEVICE_STR);
      state->devname[strlen(DEVICE_STR)] = '0';
      state->devname[strlen(DEVICE_STR)+1] = '\0';
      if (p = self->instName) 
	  strncpy(state->devname, p, MAX_DEVICE_STRING);
    }

#if 0
    if ( state->devname[strlen(state->devname)-1] != '0') {
	printf("will not attempt to open device %s because of a bug in the driver\n", state->devname);
	return XK_FAILURE;
    }
#endif
    xTrace1(ethdrvp, TR_DETAILED,
	"ethdrv etherInit master port == %x", masterPort);
    xTrace1(ethdrvp, TR_DETAILED,
	"ethdrv etherInit opening device %s", state->devname);
    kr = device_open(masterPort, MACH_PORT_NULL, D_READ|D_WRITE,
		     null_sec_token, state->devname, &state->interface_port);
    checkResult(D_SUCCESS, "device_open fails");
    xTrace0(ethdrvp, TR_MAJOR_EVENTS,
	"ethdrv etherInit device_open() completed");

    if_stat_count = NET_STATUS_COUNT;
    kr = device_get_status(state->interface_port, NET_STATUS,
	(dev_status_t)&ether_net_status, &if_stat_count);
    checkResult(D_SUCCESS, "device_get_status (NET_STATUS) failed");
    xTrace0(ethdrvp, TR_MAJOR_EVENTS,
	"ethdrv etherInit device_get_status() returned D_SUCCESS");

    if (ether_net_status.header_format != HDR_ETHERNET) {
	printf("ethdrv etherInit unsupported device header format: %d\n",
	    ether_net_status.header_format);
	return XK_FAILURE;
    }
    if (ether_net_status.max_packet_size > ETHER_INPUT_MAX) {
	printf(
	       "ether: invalid device max packet size: %d (max %d)\n",
	       ether_net_status.max_packet_size,ETHER_INPUT_MAX);
	return XK_FAILURE;
    }
    if (ether_net_status.header_size != sizeof(ETHhdr)) {
	printf("ethdrv etherInit invalid device header size: %d\n",
	    ether_net_status.header_size);
	return XK_FAILURE;
    }
    if (ether_net_status.address_size != sizeof(ETHhost)) {
	printf("ethdrv etherInit invalid device address size: %d\n",
	    ether_net_status.address_size);
	return XK_FAILURE;
    }
    if (ether_net_status.mapped_size) {
	printf("ethdrv etherInit error: Ethernet mapped\n");
	return XK_FAILURE;
    }

    if_addr_count = sizeof(if_addr);
    kr = device_get_status(state->interface_port, NET_ADDRESS,
	(dev_status_t)if_addr, &if_addr_count);
    checkResult(D_SUCCESS, "get_status(NET_ADDRESS) failed");

    if_addr[0] = ntohl(if_addr[0]);
    if_addr[1] = ntohl(if_addr[1]);
    xTrace1(ethdrvp, TR_GROSS_EVENTS,
	"ethdrv etherInit interface address (1) %s",
	ethHostStr((ETHhost *)if_addr));
    bcopy((char *)if_addr, (char *)&(state->myetheraddr), sizeof(ETHhost));
    xTrace1(ethdrvp, TR_GROSS_EVENTS,
	"ethdrv etherInit interface address %s",
	ethHostStr(&state->myetheraddr));
    xTrace3(ethdrvp, TR_GROSS_EVENTS,
	"ethdrv etherInit interface address 0x%04x 0x%04x 0x%04x",
	state->myetheraddr.high,
	state->myetheraddr.mid,
	state->myetheraddr.low);

    kr = mach_port_allocate(mach_task_self(),
	MACH_PORT_RIGHT_RECEIVE, &state->filter_port);
    checkResult(KERN_SUCCESS, "cannot allocate filter port");
    xTrace0(ethdrvp, TR_MAJOR_EVENTS,
	"ethdrv etherInit mach_port_allocate() completed");

    filter_qlimit.mpl_qlimit = MACH_PORT_QLIMIT_MAX;
    kr = mach_port_set_attributes(mach_task_self(),
	state->filter_port,
	MACH_PORT_LIMITS_INFO,
	(mach_port_info_t)&filter_qlimit,
	MACH_PORT_LIMITS_INFO_COUNT);
    checkResult(KERN_SUCCESS, "cannot adjust filter limit");
    xTrace0(ethdrvp, TR_MAJOR_EVENTS,
	"ethdrv etherInit mach_port_set_attributes(PORT_QLIMIT) completed");

    xTrace1(ethdrvp, TR_GROSS_EVENTS,
	"ethdrv etherInit using packet filter priority %d",
	state->ethFilterPriority);
    filter[filter_index++] = NETF_PUSHLIT;
    filter[filter_index++] = (filter_t) TRUE;
    kr = device_set_filter(state->interface_port,
	state->filter_port,
	MACH_MSG_TYPE_MAKE_SEND,
	state->ethFilterPriority,
	filter,
	filter_index);
    checkResult(KERN_SUCCESS, "device_set_filter failed");

    self->openenable = eth_openenable;
    self->push = eth_push;
    self->control = eth_control;

    xTrace0(ethdrvp, TR_MAJOR_EVENTS,
	"ethdrv etherInit device_set_filter completed");
    return XK_SUCCESS;
}

/*
 *  ethdrv_init
 *
 *	The protocol initialization entry point
 */
xkern_return_t
ethdrv_init(
	XObj	self)
{
    PState *ps;

    xTrace0(ethdrvp, TR_GROSS_EVENTS, "ethCtlrInit");    
    if ( !self->state || ! ((PState *)self->state)->initialized )  {
      int i;
	if (etherInit(self) == XK_SUCCESS) {
	  ps = 	(PState*)self->state;
	  ps->initialized = TRUE;
	  for (i=0; i<ETHER_OUT_BUFFERS; i++) {
	      ps->ebuffers[i] = pathAlloc(xMyPath(self), BUFFER_ALLOC_SIZE);
	      if ( ! ps->ebuffers[i] ) {
		  xTraceP0(self, TR_ERRORS, "allocation error");
		  return XK_FAILURE;
	      }
	  }
	  ps->ebuffindex = 0;
	  return XK_SUCCESS;
	}
	else return XK_FAILURE;
      }
    xTrace0(ethdrvp, TR_ERRORS, "ether driver already initialized");
    return XK_FAILURE;
}

#define PSTATE ((PState *)(self->state))

/*
 *  eth_control
 *
 *	Control operations
 */
int
eth_control(
	XObj	self,
	int	op,
	char	*buf,
	int	len)
{
  kern_return_t kr;
    
    xTrace1(ethdrvp, TR_FULL_TRACE, "eth_control: operation %x", op);
    if (op == GETMYHOST) {
	checkLen(len, sizeof(ETHhost));
	bcopy((char *)(&((PState *)self->state)->myetheraddr), buf, sizeof(ETHhost));
	return sizeof(ETHhost);
    }
    if (op == ETH_SET_PRIORITY) {
      filter_t filter[40];
      int filter_index = 0;
      PSTATE->ethFilterPriority = *(int *)buf;

      checkLen(len, sizeof(int));
      xTrace1(ethdrvp, TR_ALWAYS, "Ethernet driver using packet filter priority %d",
	      PSTATE->ethFilterPriority);
      filter[filter_index++] = NETF_PUSHLIT;
      filter[filter_index++] = (filter_t) TRUE;
      kr = device_set_filter(PSTATE->interface_port,
			     PSTATE->filter_port,
			     MACH_MSG_TYPE_MAKE_SEND,
			     PSTATE->ethFilterPriority,
			     filter,
			     filter_index);
      if (kr != KERN_SUCCESS) {
	xTrace0(ethdrvp, TR_ERRORS, "Ethernet driver cannot set packet filter priority");
	return -1;
      }
      else return sizeof(int);
    }
  if (op==GETMAXPACKET) {
    checkLen(len, sizeof(int));
    *(int*)buf = MAX_ETH_DATA_SZ;
    return sizeof(int);
  }
  return -1;
}

#undef PSTATE

static boolean_t
msg2Buf(
	char	*msgPtr,
	long	len,
	void	*bufPtr)
{
    bcopy(msgPtr, *(char **)bufPtr, len);
    *(char **)bufPtr += len;
    return TRUE;
}


/* 
 *  eth_push
 *
 *	Take a message from the generic layer and push it out to the net
 */
static xmsg_handle_t
eth_push(
	XObj	s,
	Msg	msg)
{
    ETHhdr  		*hdr = (ETHhdr *)msgGetAttr(msg, 0);
    char		*bufPtr, *buffer, smallbuf[IO_INBAND_MAX];
    int			len, bytes_written;
    kern_return_t 	kr;


    xTrace1(ethdrvp, TR_EVENTS, "ethCtlrXmit destination %s", ethHostStr(&hdr->dst));
    xAssert(hdr);
    xTrace3(ethdrvp, TR_MORE_EVENTS, "outgoing header:  src: %s  dst %s  type %x",
	    ethHostStr(&hdr->src), ethHostStr(&hdr->dst), hdr->type);
    len = msgLen(msg) + sizeof(ETHhdr);
    if (len <= IO_INBAND_MAX) 
      buffer = smallbuf;
    else {
      PState *ps = (PState *)s->state;

      buffer = ps->ebuffers[ps->ebuffindex++];
      if (ps->ebuffindex >= ETHER_OUT_BUFFERS)
	ps->ebuffindex = 0;
    }
    bcopy((char *)hdr, buffer, sizeof(ETHhdr));
    if ( len > ether_net_status.max_packet_size ) return XMSG_ERR_HANDLE;
    xTrace1(ethdrvp, TR_EVENTS, "write_ether sending %d bytes", len);
    /*
     * Place message contents in buffer
     */
    bufPtr = buffer + sizeof(ETHhdr);
    msgForEach(msg, msg2Buf, &bufPtr);
#if	1
    if (len > IO_INBAND_MAX) {
	CLOCK_TRACE("eth_push: writing into ether");
	kr = device_write_request(((PState *)s->state)->interface_port,
		MACH_PORT_NULL, 0, 0, buffer, len);
    } else {
	CLOCK_TRACE("eth_push: writing into ether inband");
	kr = device_write_request_inband(((PState *)s->state)->interface_port,
		MACH_PORT_NULL, 0, 0, buffer, len);
    }
#else	/* ! */
    if (len > IO_INBAND_MAX) {
	CLOCK_TRACE("eth_push: writing into ether");
	kr = device_write(((PState *)s->state)->interface_port,
		0, 0, buffer, len, &bytes_written);
    } else {
	CLOCK_TRACE("eth_push: writing into ether inband");
	kr = device_write_inband(((PState *)s->state)->interface_port,
		0, 0, buffer, len, &bytes_written);
    }
#endif	/* */
    if (kr != KERN_SUCCESS) {
	sprintf(errBuf, "ether.device_write_request() failed: %x %s\n",
	    kr, mach_error_string(kr));
	xError(errBuf);
    }
    return(XMSG_NULL_HANDLE);
}

