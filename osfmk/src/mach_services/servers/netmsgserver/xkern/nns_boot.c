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
 * nns_boot.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993  Arizona Board of Regents
 *
 *
 * Revision: 1.4
 * Date: 1993/08/21 01:30:21
 */

#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <mach.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <servers/netname.h>
#include <netdb.h>

#include <xkern/include/xkernel.h>
#include <xkern/include/gc.h>
#include <xkern/include/prot/ip.h>
#include <netmsgserver/xkern/nns.h>
#include <netmsgserver/xkern/nns_i.h>
#include <netmsgserver/xkern/ssr.h>

#include <xkern/include/prot/bidctl.h>

#define BOOTIDProtl	xGetDown(pstate->self, BOOTID_I)

extern int	tracennsp;

#define MAXHOSTNAMELEN	100
#define NNS_SSR_SERVICE	10

/*
 * a full mach postcard
 */
struct mach_postcard_msg {
    mach_msg_header_t		mmhdr;
    enum ssr_operation_type	op;
    int				id;
};

/*
 * a full mach postcard server request via server registry request: 
 * mach hdr, ssr data, postcard server request
 */
struct mach_server_postcard_msg {
    mach_msg_header_t		mmhdr;
    struct ssrdata		ssrd;
    enum ssr_operation_type	op;
    int				id;
};

extern mach_port_t	delayPort;
extern PSTATE		*pstate;

mach_port_t		nns_ssr_port;
extern mach_port_t	service;
Map			IdMap;


#ifdef	MACH_STAND_ALONE
/*
 * ssr server registry port - initialized by calling netname_look_up
 */
static mach_port_t	server_registry_port = MACH_PORT_NULL;
/*
 * NNS will wait 10 second if SSR has not checked in its name.
 */
#define BOOTSTRAP_WAIT	10*1000
#else	/* !MACH_STAND_ALONE */
/*
 * ssr server registry port - initialized during ssr_init
 */
extern mach_port_t	server_registry_port;
#endif	/* !MACH_STAND_ALONE */

static xkSemaphore	registrationSem;
static xkSemaphore	broadcastSem;
static mutex_t		idMutex;
static mutex_t		nameMutex;
static Map		HostPortMap;

int	gethostname();

static int
	nns_register_bidctl(IPhost ipaddr);

void	nns_null_function(void *, int);

int	nns_IsIpAddr(char *ipaddr);

int	nns_get_host_ip_addr(char *name, char *ipaddr);

mach_port_t
	nns_look_up_host(IPhost host);

mach_port_t
	nns_register_host (IPhost host);

void	nns_deregister_host(IPhost host);

int	nns_init_bootstrap(void);

mach_port_t
	nns_execute_broadcast(char *key);

static void
	nns_delayFunc(int interval);


void
nns_null_function(
	void	*addr,
	int	size)
{
}

/*
 * delay interval ms
 */
static void
nns_delayFunc(
	int	interval)
{
    mach_msg_header_t	m;

    cthread_yield();
    (void)mach_msg(&m, MACH_RCV_MSG|MACH_RCV_TIMEOUT,
	0, sizeof m, delayPort, interval, MACH_PORT_NULL);
}


static int
get_unique_id( void )
{
    static int	id = 0;
    static int	ret;

    mutex_lock(idMutex);
    ret = ++id;
    mutex_unlock(idMutex);

    return ret;
}


int
nns_IsIpAddr(
	char	*ipaddr)
{
    int		a, b, c, d;

    return sscanf(ipaddr, "%d.%d.%d.%d", &a, &b, &c, &d) == 4;
}

int
nns_get_host_ip_addr(
	char	*name,
	char	*ipaddr)
{
    struct hostent	*ans;
    int			i;

    mutex_lock(nameMutex);
    ans = gethostbyname(name);
    mutex_unlock(nameMutex);
    if (! ans) {
	return -1;
    }

    for (i=0;i<sizeof(IPhost);i++) {
	ipaddr[i] = ans->h_addr[i];
    }
    return 0;
}


static int
get_my_ip_addr(
	char	*ipaddr)
{
    char	name[MAXHOSTNAMELEN];

    if (gethostname(name, MAXHOSTNAMELEN) < 0) {
	return -1;
    }
    return nns_get_host_ip_addr(name, ipaddr);
}

mach_port_t
nns_look_up_host(
	IPhost	host)
{
    mach_port_t	port;

    /*
     * assume host name is an IP address
     */
    xk_master_lock();
    if (mapResolve(HostPortMap, &host, &port) == XK_SUCCESS) {
	xk_master_unlock();
	return port;
    }
    xk_master_unlock();
    return MACH_PORT_NULL;
}


/*
 * nns_register
 *     register a machine
 */
mach_port_t
nns_register_host (
	IPhost	host)
{
    struct mach_server_postcard_msg	reqmsg;
    kern_return_t			ret;
    static mach_port_t			remote_port = MACH_PORT_NULL;
    int					unique_id;
    int					i;

    xTrace4(nnsp, TR_DETAILED,
	"NNS nns_register_host: Registering host %d.%d.%d.%d",
	host.a, host.b, host.c, host.d);

    xk_master_lock();
    semWait(&registrationSem);
    xk_master_unlock();

    /*
     * after waking up it is possible
     * a previous thread could have already registered the host
     */
    remote_port = nns_look_up_host(host);
    if (remote_port != MACH_PORT_NULL) {
	xk_master_lock();
	semSignal(&registrationSem);
	xk_master_unlock();
	return remote_port;
    }

#ifdef	MACH_STAND_ALONE
    if (server_registry_port == MACH_PORT_NULL) {
	xTrace0(nnsp, TR_EVENTS,
		"NNS nns_register_host: bootstrap has not yet done");
	return MACH_PORT_NULL;
    }
#endif	/* MACH_STAND_ALONE */

    reqmsg.mmhdr.msgh_remote_port = server_registry_port;
    reqmsg.mmhdr.msgh_local_port = nns_ssr_port;
    reqmsg.mmhdr.msgh_bits =
	MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
    reqmsg.mmhdr.msgh_size = sizeof(struct mach_server_postcard_msg);
    reqmsg.ssrd.service_id = NNS_SSR_SERVICE;
    reqmsg.ssrd.operation = REQUEST;
    reqmsg.ssrd.destination_host = host;
    reqmsg.op = htonl(REQUEST);
    unique_id = get_unique_id();
    reqmsg.id = htonl(unique_id);

    /*
     * send the request to server
     */
    if ((ret = mach_msg_send(&reqmsg)) != MACH_MSG_SUCCESS) {
	xTrace2(nnsp, TR_ERRORS,
	    "NNS nns_register_host: Error: Failed to send request message ret = x%x %s",
	    ret, mach_error_string(ret));
#ifdef	MACH_STAND_ALONE
	server_registry_port  = MACH_PORT_NULL;
#endif	/* MACH_STAND_ALONE */
	return MACH_PORT_NULL;
    }
    xTrace1(nnsp, TR_DETAILED,
	"NNS nns_register_host: Sent request to server for the name server port : ID = %d",
	ntohl(reqmsg.id));

    for (i=0; i < HOST_WAKEUPS; i++) {
	nns_delayFunc(HOST_TIMEOUT);
	xk_master_lock();
	if (mapResolve(IdMap, &unique_id, &remote_port) == XK_SUCCESS) {
	    mapUnbind(IdMap, &unique_id);
	    xk_master_unlock();
	    break;
	}
	remote_port = MACH_PORT_NULL;
	xk_master_unlock();
    }

    /*
     * get the lock again - needed below
     */
    xk_master_lock();

    if (remote_port == MACH_PORT_NULL) {
	xTrace0(nnsp, TR_ERRORS,
	    "NNS nns_register_host: Failed to get remote server's name port");
    } else {
	ret = mach_port_mod_refs(mach_task_self(), remote_port,
	    MACH_PORT_RIGHT_SEND, 1);
	if (ret != KERN_SUCCESS) {
	    xTrace0(nnsp, TR_DETAILED,
		"NNS nns_register_host: Failed to modify references to the remote port");
	} else {
	    mapBind(HostPortMap, &host, remote_port);
	    nns_register_bidctl(host);

	    xTrace0(nnsp, TR_EVENTS,
		"NNS nns_register_host: Returning from Host look up");
	}
    }

    semSignal(&registrationSem);
    xk_master_unlock();
    return remote_port;
}


void 
nns_deregister_host(
	IPhost	host)
{
    if (mapUnbind(HostPortMap, &host) != XK_SUCCESS) {
	xTrace4(nnsp, TR_ERRORS,
	    "nns_deregister_host: failed to deregister host %d.%d.%d.%d",
	    host.a, host.b, host.c, host.d);
    }
    xTrace4(nnsp, TR_EVENTS,
	"nns_deregister_host: deregistered host %d.%d.%d.%d",
	host.a, host.b, host.c, host.d);
}


/*
 * receive_requests
 *
 *
 */

static int
receive_requests_and_reply( void )
{
    struct {
	struct local_ssr_msg ssr_msg;
	char		     trailer[MAX_TRAILER_SIZE];
    }				msg;
    struct mach_postcard_msg	*repmsg;
    kern_return_t		ret;

    repmsg = (struct mach_postcard_msg *)&msg;

    while (1) {
	bzero((char *)&msg, sizeof(msg));
	repmsg->mmhdr.msgh_size =
	    sizeof(struct local_ssr_msg) + MAX_TRAILER_SIZE;
	repmsg->mmhdr.msgh_local_port = nns_ssr_port;
	if ((ret=mach_msg_receive(repmsg)) == KERN_SUCCESS) {
	    repmsg->op = ntohl(repmsg->op);
	    if (repmsg->op == REQUEST) {
		xTrace1(nnsp, TR_DETAILED,
		    "NNS receive_requests_and_reply: Receive REQUEST to port=%x",
		    service);
		/*
		 * reply with our name server port
		 */
		repmsg->mmhdr.msgh_local_port = service;
		repmsg->mmhdr.msgh_bits = MACH_MSGH_BITS(
		    MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
		repmsg->mmhdr.msgh_size = sizeof(struct mach_postcard_msg);
		repmsg->op = htonl(REPLY);

		if ((ret = mach_msg_send(repmsg)) != MACH_MSG_SUCCESS) {
		    xTrace0(nnsp, TR_ERRORS,
			"NNS receive_requests_and_reply: Error: Failed to send reply message");
		}
		xTrace0(nnsp, TR_DETAILED,
		    "NNS receive_requests_and_reply: Server Replied with name server port");
	    } else if (repmsg->op == REPLY) {
		repmsg->id = ntohl(repmsg->id);
		xTrace2(nnsp, TR_DETAILED,
		    "NNS receive_requests_and_reply: Receive REPLY from %d port=%x",
		    repmsg->id, repmsg->mmhdr.msgh_remote_port);
		
		xk_master_lock();
		mapBind(IdMap, &repmsg->id, repmsg->mmhdr.msgh_remote_port);
		xk_master_unlock();
	    } else {
		xTrace1(nnsp, TR_ERRORS,
		    "NNS receive_requests_and_reply: Error: got a bad operation: %d!!",
		    repmsg->op);
	    }
	}
    }
}


/*
 * nns_init_bootstrap 
 *
 *
 */

int 
nns_init_bootstrap( void )
{
    struct local_ssr_msg	regmsg;
    kern_return_t		ret;

    idMutex = mutex_alloc();
    nameMutex = mutex_alloc();
    semInit(&registrationSem, 1);
    semInit(&broadcastSem, 1);

    if (get_my_ip_addr((char *)&pstate->myaddr) < 0) {
	xTrace0(nnsp, TR_ERRORS,
	    "NNS nns_init_bootstrap: Error: Failed to get my ip addr");
	return -1;
    }

    HostPortMap = mapCreate(100, sizeof(IPhost));
    IdMap = mapCreate(100, sizeof(int));

    if ((ret = mach_port_allocate(mach_task_self(),
	    MACH_PORT_RIGHT_RECEIVE,
	    &nns_ssr_port))
	!= KERN_SUCCESS ) {
	xTrace0(nnsp, TR_ERRORS,
	    "NNS nns_init_bootstrap: Error: Failed to allocate nns port");
	return -1;
    }

    xTrace0(nnsp, TR_DETAILED,
	"NNS nns_init_bootstrap: Registering nns with ssr");

#ifdef MACH_STAND_ALONE
    if (server_registry_port == MACH_PORT_NULL) {
	xk_master_unlock();
	while (ret = do_netname_look_up(
			service, "", SSR_SERVER_NAME, &server_registry_port)
		    == NETNAME_NOT_CHECKED_IN) {
            xTrace2(nnsp, TR_EVENTS,
                "NNS nns_init_bootstrap: %s %s",
                SSR_SERVER_NAME, mach_error_string(ret));
            nns_delayFunc(BOOTSTRAP_WAIT);
        }
	xTrace2(nnsp, TR_EVENTS,
	    "NNS nns_init_bootstrap: got %s port %x",
	    SSR_SERVER_NAME, server_registry_port);
	xk_master_lock();
    } else {
	xTrace2(nnsp, TR_ERRORS,
	    "NNS nns_init_bootstrap: %s port already has a value=%x",
	    SSR_SERVER_NAME, server_registry_port);
	return -1;
    }
#endif  /* MACH_STAND_ALONE */

    /*
     * register nns with ssr
     */
    regmsg.mmhdr.msgh_remote_port = server_registry_port;
    regmsg.mmhdr.msgh_local_port = nns_ssr_port;
    regmsg.mmhdr.msgh_bits = 
	MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
    regmsg.mmhdr.msgh_size = sizeof(regmsg);
    regmsg.ssrd.operation = REGISTER;
    regmsg.ssrd.service_id = NNS_SSR_SERVICE;
    if ((ret = mach_msg_send(&regmsg)) != MACH_MSG_SUCCESS) {
	xTrace0(nnsp, TR_ERRORS,
	    "NNS nns_init_bootstrap: Error: Failed to send register message");
	return -1;
    }
    xTrace0(nnsp, TR_DETAILED,
	"NNS nns_init_bootstrap: starting thread for requests");

    /*
     * start a listening thread to look for requests on the nns_ssr_port
     */
    cthread_detach(cthread_fork((cthread_fn_t)receive_requests_and_reply, 0));

    return 0;
}


mach_port_t
nns_execute_broadcast(
	char	*key)
{
    Msg		msg;
    BCAST_MSG	bcast;
    int		id, i;
    BCAST_REPLY	reply[MAX_BCAST_REPLIES+1];
    mach_port_t	remote_port = MACH_PORT_NULL;

    /*
     * create and send a broadcast message
     */
    bcast.id = id = get_unique_id();
    strcpy(bcast.key, key);
    bcast.client = pstate->myaddr;
    bcast.mode = REQUEST_MODE;

    xk_master_lock();
    semWait(&broadcastSem);

    /*
     * initialize dummy list header
     */
    bzero((char *)reply, (MAX_BCAST_REPLIES+1)*sizeof(BCAST_REPLY));

    /*
     * add id to map
     */
    mapBind(IdMap, &id, &reply);

    /*
     * send bcast request
     */
    msgConstructInplace(&msg, (char *)&bcast,
	sizeof(BCAST_MSG), (MsgCIPFreeFunc)nns_null_function);
    xPush(pstate->udp_sessn, &msg);
    msgDestroy(&msg);
    xk_master_unlock();

    /*
     * wait for replies
     */
    for (i=0; i < BCAST_WAKEUPS; i++) {
	nns_delayFunc(BCAST_TIMEOUT);
	if (reply[0].server.a != 0) {
	    break;
	}
    }

    xTrace0(nnsp, TR_DETAILED,
	"NNS nns_execute_broadcast: Back from waiting for broadcast replies");

    /*
     * free this id
     */
    xk_master_lock();
    xAssert(mapUnbind(IdMap, &id) == XK_SUCCESS);
    xk_master_unlock();

    /*
     * no - replies received...
     */
    if (reply[0].server.a == 0) {
	xTrace0(nnsp, TR_DETAILED,
	    "NNS nns_execute_broadcast: Found no repiles for the broadcast");
	remote_port = MACH_PORT_NULL;
    } else {
	/*
	 * check which host is already registered with us
	 * - if so return it's ns port
	 */
	for (i = 0; reply[i].server.a != 0; i++) {
	    if ((remote_port = nns_look_up_host(reply[i].server))
		!= MACH_PORT_NULL) {
		break;
	    }
	}

	if (reply[i].server.a == 0) {
	    /*
	     * if none registered pick first host that can be registered
	     */
	    for (i=0; reply[i].server.a != 0; i++) {
		remote_port = nns_register_host(reply[i].server);
		if (remote_port != MACH_PORT_NULL) {
		    break;
		}
	    }
	}
    }

    xk_master_lock();
    semSignal(&broadcastSem);
    xk_master_unlock();

    return remote_port;
}


static int
nns_register_bidctl(
	IPhost	ipaddr)
{
    Part		part[1];
    xkern_return_t	ret;

    partInit(part, 1);
    partPush(part[0], &ipaddr, sizeof(IPhost));
    if ((ret = xOpenEnable(pstate->self,pstate->self,BOOTIDProtl,part))
	!= XK_SUCCESS) {
	xTrace4(nnsp, TR_ERRORS,
	    "nns_register_bidctl: open enable on bidclt failed for addr %d.%d.%d.%d",
	    ipaddr.a, ipaddr.b, ipaddr.c, ipaddr.d);
	return -1;
    }
    xTrace4(nnsp, TR_FULL_TRACE,
	"nns_register_bidctl: registered host %d.%d.%d.%d with bidctl",
	ipaddr.a, ipaddr.b, ipaddr.c, ipaddr.d);
    return(0);
}

