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
 * nns.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993  Arizona Board of Regents
 *
 *
 * Revision: 1.5
 * Date: 1993/08/21 01:24:05
 */

#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <mach.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <servers/netname_defs.h>
#include <servers/service.h>
#include <cthreads.h>

#include <xkern/include/xkernel.h>
#include <xkern/include/gc.h>
#include <xkern/include/prot/ip.h>
#include <netmsgserver/xkern/nns.h>
#include <netmsgserver/xkern/nns_i.h>
#include <xkern/include/prot/bidctl.h>

int		tracennsp = 0;
mach_port_t	delayPort;

extern int	xk_thread_limit;

mach_port_t	service;
mach_port_t	notification_port;
static		mach_port_t pset;
static int	threadsCount;
static int	threadsUsed;
static mutex_t	threadMutex;


int	nns_init_bootstrap(void);

int	nns_look_up_netname(char *key);

int	nns_IsMyIpAddr(IPhost ipaddr);

void	netname_init(void);

boolean_t
	netname_server(mach_msg_header_t *, mach_msg_header_t *);

boolean_t
	notify_server(mach_msg_header_t *, mach_msg_header_t *);

kern_return_t
	do_netname_check_in(
	    mach_port_t server, netname_name_t key,
	    mach_port_t sig, mach_port_t name);
extern void
	nns_deregister_host(IPhost);

static void
	getproc_protl(XObj);

static xkern_return_t
	nnsOpenDisableAll( XObj, XObj );

static int
	service_loop(void);

void	create_service_thread(void);

void	exit_thread(void);

#ifdef NNS_USE_GC
static void
	nnsDestroySessn( XObj );
#endif	/* NNS_USE_GC */

long	copy(char *dest, char *src, long len, void *arg);

static Map		SessnMap;
extern Map		IdMap;
extern xkSemaphore	bcastSem;
PSTATE			*pstate;

#define UDPProtl	xGetDown(pstate->self, UDP_I)

#define MAP_SIZE	101

static boolean_t
handle_requests(
	mach_msg_header_t	*request,
	mach_msg_header_t	*reply)
{
    if (request->msgh_local_port == service) {
	xTrace0(nnsp, TR_DETAILED, "NNS handle_requests: Got a request");
	return netname_server(request, reply);
    }
    else if (request->msgh_local_port == notification_port) {
	xTrace0(nnsp, TR_DETAILED, "NNS handle_requests: Got a notification");
	return notify_server(request, reply);
    }
    else {
	xTrace0(nnsp, TR_ERRORS,
	    "NNS handle_requests: Unknown port in handle requests");
	return 1;
    }
}


static int
name_server_init( void )
{
    kern_return_t		ret;
    mach_msg_type_name_t	returned_type;
    mach_port_t			sig_send;

    xTrace0(nnsp, TR_DETAILED,
	"NNS name_server_init: Initializing for the local case");

    netname_init();

    /*
     * allocate notification port
     */
    ret = mach_port_allocate(mach_task_self(),
	MACH_PORT_RIGHT_RECEIVE, &notification_port);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    /*
     * check this server in
     */
    ret = service_checkin(service_port, name_server_port, &service);
    if (ret != KERN_SUCCESS) {
	printf("NNS Error: Could not check in name server service; another name server could be running\n");

	/*
	 * allocate service port
	 */
	ret = mach_port_allocate(mach_task_self(),
				 MACH_PORT_RIGHT_RECEIVE,
				 &service);
	if (ret != KERN_SUCCESS) {
	    xTrace2(nnsp, TR_ERRORS,
		"Error: Could not allocate service port for checkin by the network name server %d(x%x)\n", ret, ret);
	    return -1;
	}

	/*
	 * allocate signature
	 */
	ret = mach_port_extract_right(mach_task_self(),
				      service,
				      MACH_MSG_TYPE_MAKE_SEND,
				      &sig_send,
				      &returned_type);
	if (ret != KERN_SUCCESS) {
	    xTrace2(nnsp, TR_ERRORS,
		"Error: Could not allocate signature for checkin by the net work name server %d(x%x)\n", ret, ret);
	    return -1;
	}

	ret = netname_check_in(name_server_port, NETWORK_NAME_SERVICE,
                           sig_send, service);
	if (ret != NETNAME_SUCCESS) {
	    xTrace2(nnsp, TR_ERRORS,
		"Error: Could not checkin the network name server %d(x%x)\n ",
		ret, ret);
	    return -1;
	}
    }

    ret = mach_port_mod_refs(mach_task_self(),
	mach_task_self(), MACH_PORT_RIGHT_SEND, 3);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    ret = mach_port_mod_refs(mach_task_self(),
	service, MACH_PORT_RIGHT_SEND, 3);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    ret = do_netname_check_in(service, "NameServer",
			      mach_task_self(), service);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    ret = do_netname_check_in(service, NETWORK_NAME_SERVICE,
			      mach_task_self(), service);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    ret = do_netname_check_in(service, "NMMonitor",
			      mach_task_self(), service);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    ret = do_netname_check_in(service, "NMControl",
			      mach_task_self(), service);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    /*
     * Prepare our port set.
     */
    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &pset);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    ret = mach_port_move_member(mach_task_self(), service, pset);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    ret = mach_port_move_member(mach_task_self(), notification_port, pset);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    xTrace0(nnsp, TR_DETAILED,
	"NNS name_server_init: Done with name server initializations");
    return 0;
}


void
create_service_thread( void )
{
    /* this code was taken from process.c and added to prevent
     * weird deadlocking problem!
     */
    xk_master_lock();
    xk_thread_limit++;
    xk_master_unlock();

    cthread_set_kernel_limit(xk_thread_limit);
    cthread_detach(cthread_fork((cthread_fn_t)service_loop, 0));
}


void
exit_thread( void )
{
    xk_master_lock();
    xk_thread_limit--;
    xk_master_unlock();

    cthread_exit(0);
}


static int
service_loop( void )
{
    mach_msg_return_t	mret;
    char		request[MAX_MSG_BUF+MAX_TRAILER_SIZE];
    char		reply[MAX_MSG_BUF+MAX_TRAILER_SIZE];
    mach_msg_header_t	*header = (mach_msg_header_t *)request;

    xTrace0(nnsp, TR_DETAILED, "NNS service_loop: Entering service loop");

    /*
     * Enter service loop.
     */
    header->msgh_local_port = pset;
    header->msgh_size = MAX_MSG_BUF+MAX_TRAILER_SIZE;
    while ((mret = mach_msg_receive(header)) == MACH_MSG_SUCCESS) {
	mutex_lock(threadMutex);
	threadsUsed++;
	/*
	 * create a new thread ?
	 */
	if (threadsUsed == threadsCount && threadsCount < NETNAME_THREADS_MAX) {
	    threadsCount++;
	    create_service_thread();
	    xTrace0(nnsp, TR_DETAILED,
		"NNS service_loop: creating a new thread");
	}
	mutex_unlock(threadMutex);

	xTrace0(nnsp, TR_DETAILED, "NNS service_loop: handling the request");
	handle_requests(
	    (mach_msg_header_t *)request, (mach_msg_header_t *)reply);
	(void)mach_msg_send(reply);

	xTrace0(nnsp, TR_DETAILED,
	    "NNS service_loop: back from handling request");

	mutex_lock(threadMutex);
	threadsUsed--;
	/*
	 * is this an extra thread ?
	 * Is some other thread waiting on the receive?
	 * If so then exit
	 */
	if (threadsCount > NETNAME_THREADS_MIN &&
	    threadsCount - threadsUsed > 1) {
	    threadsCount--;
	    mutex_unlock(threadMutex);
	    xTrace0(nnsp, TR_DETAILED, "NNS service_loop: exiting this thread");
	    exit_thread();
	}
	mutex_unlock(threadMutex);

	xTrace0(nnsp, TR_DETAILED, "NNS service_loop: back to listen mode");
	header->msgh_local_port = pset;
	header->msgh_size = MAX_MSG_BUF+MAX_TRAILER_SIZE;
    }
    xTrace2(nnsp, TR_ERRORS,
	"NNS service_loop: failed mach_msg_receive %x %s",
	mret, mach_error_string(mret));
    return 0;
}



/*
 * nns_init
 */
void
nns_init(
	XObj	self)
{
    Part	part[2];
    IPhost      ipBcast;
    long	serverPort;
    int		i;

    xTrace0(nnsp, TR_DETAILED, "NNS nns_init: started");

    xAssert(xIsProtocol(self));

    getproc_protl(self);
    pstate = X_NEW(PSTATE);
    self->state = (void *)pstate;
    pstate->self = self;

    pstate->udp = UDPProtl;

    serverPort = relProtNum(self, pstate->udp);
    if (serverPort == -1) {
	xTrace0(nnsp, TR_ERRORS,
	    "Could not get relative protocol number of nns w.r.t udp");
	printf("NNS Error: Could not get relative protocol number of nns w.r.t udp; check protocol tables\n");
	return;
    }

    ipBcast = IP_LOCAL_BCAST;

    SessnMap = mapCreate(100, sizeof(IPhost));

    partInit(part, 2);
    partPush(part[0], &ipBcast, sizeof(IPhost));
    partPush(part[0], &serverPort, sizeof(long));
    partPush(part[1], &ipBcast, sizeof(IPhost));
    partPush(part[1], &serverPort, sizeof(long));

    pstate->udp_sessn = xOpen(self, self, pstate->udp, part);
    if (pstate->udp_sessn == ERR_XOBJ) {
	xTrace0(nnsp, TR_ALWAYS,
	    "NNS nns_init: can't open transport protocol");
	xFree((char *) pstate);
	pstate = NULL;
	return;
    }

    partInit(part, 1);
    partPush(part[0], ANY_HOST, 0);
    partPush(part[0], &serverPort, sizeof(long));
    if (xOpenEnable(self, self, pstate->udp, part) == XK_FAILURE) {
	xTrace0(nnsp, TR_ALWAYS,
	    "NNS nns_init: can't openenable transport protocol");
	return;
    }

    if (name_server_init() < 0) {
	xTrace0(nnsp, TR_ALWAYS,
	    "NNS nns_init: can't initialize name server");
	return;
    }

    xk_master_unlock();

    threadMutex = mutex_alloc();
    threadsCount = NETNAME_THREADS_MIN;
    threadsUsed = 0;

    xTrace0(nnsp, TR_DETAILED, "NNS nns_init: creating service threads");
    for (i=0; i < NETNAME_THREADS_MIN; i++) {
	create_service_thread();
    }

    /*
     * create a dummy port to simulate delays
     */
    (void) mach_port_allocate(
	mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &delayPort);

    xk_master_lock();

    /*
     * initialize the boot strap mechanism
     */
#ifdef	MACH_STAND_ALONE
    evDetach(evSchedule((EvFunc)nns_init_bootstrap, (void *)0, 0));
#else	/* MACH_STAND_ALONE */
    if (nns_init_bootstrap() < 0) {
	xTrace0(nnsp, TR_ALWAYS,
	    "NNS nns_init: can't initialize boot strapping");
	return;
    }
#endif	/* MACH_STAND_ALONE */
    xTrace0(nnsp, TR_DETAILED, "NNS nns_init done");
}


/*
 * nnsOpen
 */
static XObj
nnsOpen(
	XObj	self,
	XObj	hlpRcv,
	XObj	hlpType,
	Part	*p)
{
    xTrace0(nnsp, TR_ERRORS, "NNS nns_open called!!!");
    return NULL;
}


/*
 * nnsControlProtl
 */
static int
nnsControlProtl(
	XObj	self,
	int	opcode,
	char	*buf,
	int	len)
{
    BidctlBootMsg	*reboot_msg = (BidctlBootMsg *)buf;

    if (opcode >= BIDCTL_CTL * MAXOPS && opcode < (BIDCTL_CTL + 1) * MAXOPS) {
	switch (opcode) {
	    case BIDCTL_PEER_REBOOTED:
		xTrace4(nnsp, TR_ERRORS,
		    "nns control: Peer %d.%d.%d.%d rebooted",
		    reboot_msg->h.a, reboot_msg->h.b,
		    reboot_msg->h.c, reboot_msg->h.d);
		nns_deregister_host(reboot_msg->h);
		break;
	    default:
		xTrace1(nnsp, TR_SOFT_ERROR,
		    "nns control: ignoring bootid msg %d", opcode);
	}
    }
    return(XK_SUCCESS);
}


/*
 * nnsOpenEnable
 */
static xkern_return_t
nnsOpenEnable(
	XObj	self,
	XObj	hlpRcv,
	XObj	hlpType,
	Part	*p)
{
    xTrace0(nnsp, TR_ERRORS, "NNS nns_open_enable called!!!");
    return XK_FAILURE;
}


/*
 * nnsCloseProtl
 */
static xkern_return_t
nnsCloseProtl(
	XObj	self)
{
    return XK_SUCCESS;
}


long copy(
	char	*dest,
	char	*src,
	long	len,
	void	*arg)
{
    bcopy(src, dest, len);
    return len;
}


/*
 * nnsDemux
 */
/*
 * ARGSUSED
 */
static xkern_return_t
nnsDemux(
	XObj	self,
	XObj	lls,
	Msg	*dg)
{
    BCAST_MSG	bcast;
    Msg		reply_msg;
    XObj	reply_s;
    Part	part[2];
    long	server_port;
    BCAST_REPLY	*reply;
    int		i;

    xTrace1(nnsp, TR_DETAILED, "NNS nns_demux, length = %d", msgLen(dg));

    msgPop(dg, (MLoadFun)copy, &bcast, sizeof(BCAST_MSG), 0);

    if (bcast.mode == REQUEST_MODE) {
	if (nns_IsMyIpAddr(bcast.client)) {
	    xTrace1(nnsp, TR_DETAILED,
		"NNS nns_demux, dropping my own request message for %s",
		bcast.key);
	    return XK_SUCCESS;
	}

	if (nns_look_up_netname(bcast.key)) {
	    /*
	     * look for an already open session for this client
	     */
	    bcast.server = pstate->myaddr;
	    bcast.mode = REPLY_MODE;

	    if (mapResolve(SessnMap, &bcast.client, &reply_s) == XK_FAILURE) {
		server_port = relProtNum(self, pstate->udp);
		if (server_port == -1) {
		    xTrace0(nnsp, TR_ERRORS,
			"Could not get relative protocol number of nns w.r.t udp");
		    return XK_SUCCESS;
		}

		/*
		 * reply to the client requesting the name
		 */
		partInit(part, 2);
		partPush(part[0], &bcast.client, sizeof(IPhost));
		partPush(part[0], &server_port, sizeof(long));
		partPush(part[1], &bcast.server, sizeof(IPhost));
		partPush(part[1], &server_port, sizeof(long));

		reply_s = xOpen(self, self, pstate->udp, part);
		if (reply_s == ERR_XOBJ) {
		    xTrace0(nnsp, TR_ERRORS,
			"NNS nns_demux, Could not open reply sessn");
		    return XK_SUCCESS;
		}
		mapBind(SessnMap, &bcast.client, reply_s);
	    }

	    /*
	     * send the reply over
	     */
	    xTrace1(nnsp, TR_DETAILED,
		"NNS nns_demux, key %s sending reply message", bcast.key);
	    msgConstructInplace(&reply_msg, (char *)&bcast,
		sizeof(BCAST_MSG), (MsgCIPFreeFunc)nns_null_function);
	    xPush(reply_s, &reply_msg);
	    msgDestroy(&reply_msg);
	} else {
	    xTrace1(nnsp, TR_DETAILED,
		"NNS nns_demux, key %s not found dropping message", bcast.key);
	}
    } else if (bcast.mode == REPLY_MODE) {
	/*
	 * add this reply to the list of replies gathered so far
	 */
	if (mapResolve(IdMap, &bcast.id, &reply) == XK_SUCCESS) {
	    xTrace1(nnsp, TR_DETAILED,
		"NNS nns_demux, adding reply to list for %s", bcast.key);

	    /*
	     * look for an empty spot
	     */
	    for (i=0; reply[i].server.a != 0 && i < MAX_BCAST_REPLIES; i++)
		;

	    /*
	     * if the bcast replies are not over the limit
	     * add this reply to the list; else ignore this reply
	     */
	    if (i < MAX_BCAST_REPLIES) {
		reply[i].server = bcast.server;
	    }
	} else {
	    xTrace1(nnsp, TR_ERRORS,
		"NNS nns_demux, Id %d not found dropping message", bcast.id);
	}
    } else {
	xTrace1(nnsp, TR_ERRORS, "NNS nns_demux, Bad mode %d", bcast.mode);
    }

    return XK_SUCCESS;
}


static xkern_return_t
nnsOpenDisable(
	XObj	self,
	XObj	hlpRcv,
	XObj	hlpType,
	Part	*p)
{
    xTrace0(nnsp, TR_MAJOR_EVENTS, "NNS nns_open_disable called!!");
    return XK_FAILURE;
}


static xkern_return_t
nnsOpenDisableAll(
	XObj	self,
	XObj	hlpRcv)
{
    return XK_FAILURE;
}


static void
getproc_protl(
	XObj	s)
{
    xAssert(xIsProtocol(s));
    s->close = nnsCloseProtl;
    s->control = nnsControlProtl;
    s->open = nnsOpen;
    s->openenable = nnsOpenEnable;
    s->opendisable = nnsOpenDisable;
    s->demux = nnsDemux;
    s->opendisableall = nnsOpenDisableAll;
}


