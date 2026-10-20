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
 *
 * port-maint.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.17
 * Date: 1993/08/12 19:33:34
 */
/*
 *    Port Maintenance for xKernel Mach NetIPC
 *
 *    This module is tasked with maintaining the current set
 *    port rights for which this network node is the receiver or a
 *    sender.  It provides mappings between local ports and
 *    network port descriptors, and between the global network
 *    port numbers and network port descriptors.
 *
 *    This module coordinates with the bootid protocol to keep
 *    a current list of peer machines and their boot id's.
 *    The receiving machine for all send rights and the senders
 *    for all receive rights must be given to bootid.
 *
 *    This module also coordinate port transfers with the port
 *    transfer protocols srx and rrx; subroutine calls are used
 *    to initialize and finalize transfers.
 */

#include <stdio.h>
#include <mach.h>
#include <mach/message.h>
#include <mach/mach_host.h>
#include <mach/notify.h>
#include <cthreads.h>
#include <xkern/include/xkernel.h>
#include <xkern/include/rwlock.h>
#include <xkern/include/prot/ip.h>
#include <xkern/include/prot/bidctl.h>
#include <s_xkern/include/prot/machripc.h>
#include <s_xkern/include/prot/rrx.h>
#include <s_xkern/include/prot/srx.h>
#include <s_xkern/include/prot/machripc_xfer.h>
#include <netmsgserver/xkern/port_maint_i.h>

#define STATEPTR	((struct portm_state *)(myProtl->state))
#define STATESELFPTR	((struct portm_state *)(self->state))
#define SESSNSTATEPTR	((Portm_ActiveId *)(self->state))

/*
 * these are sort of odd; review
 */
#define pass_open_mni(part)					\
	    (xOpenEnable(machnetipcProtl, machnetipcProtl,	\
		MachNetIPCProtl, part))

#define pass_open_transport(part)				\
	    (xOpenEnable(myProtl, myProtl, TransportProtl, part))

#define pass_open_bootid(part)					\
	    (xOpenEnable(myProtl, myProtl, BootIdProtl, part))

#define pass_opendisable_bootid(part)				\
	    (xOpenDisable(myProtl, myProtl, BootIdProtl, part))

#define TYPE_MALLOC(type) (type *)xMalloc(sizeof(type))

#define IP_NULL	{ 0, 0, 0, 0 }

/*
 * necessary because on some architectures the ip addr
 * struct is passed to routines as a struct pointer instead
 * of the ip address itself
 */
#define IP_TO_INT(ip_addr)	(*(int *)&(ip_addr))

#define IPEQUAL(_a, _b)					\
	    ((_a.a == _b.a) && (_a.b == _b.b) &&	\
	     (_a.c == _b.c) && (_a.d == _b.d))
 
int			traceportmaintp;
int			traceportmaint;
/*
 * used to generate unique id's
 */
int			netport_id = NETPORT_INITIAL_NUMBER;
static XObj		BootIdProtlObj;
static IPhost		ip_null = IP_NULL;
/*
 * contributes to protocol copying madness
 */
static XObj		machnetipcProtl;
static struct xobj	myprotl;
static XObj		myProtl = &myprotl;
/*
 *  used to find non-local senders in map
 *  note the static global variable "cmpresult"
 */
static int		cmpresult;

/*
 * Null_Netport defined in mach-simple.c
 */
extern mnetport		Null_Netport;  /* error return value */
extern Map		xkMsgIdMap;

extern void
	machnetipc_start_msg_receive(struct send_request *args);

extern void
	machr_msg_receive(struct send_request *args);

extern void
	port_unblock(mnetport *port);

static void
	getproc(XObj p);

static void
	xAsyncThread(cthread_fn_t func, void *args, char *name);

void	port_mgmt(mn_arch_tag_t arch, Msg *msg);

xkern_return_t
	portmaint_init(XObj self);

static void
	portm_port_free(ReadWriteLock *rwlock, mnetport *portdesc);

void	portm_use_netnum(mach_port_t port, mnportid_t port_number);

mach_port_t
	portm_quick_so_lookup(mnportid_t portid);

xkern_return_t
	portm_port_remove_send_once(mnetport *port_desc);

xkern_return_t
	portm_port_remove_unused(
	    mach_port_t *local_port, mnetport *netport_ptr,
	    IPhost *recvr_ipaddr);

void	portm_generate_nms(mnetport *portd);

void	portm_add_sender(mnetport *portd, IPhost host);

void	portm_delete_local_receive(Event ev, mach_port_t port);

void	portm_delete_local_send(Event ev, mach_port_t port);

mnportid_t
	portm_get_netnum(mach_port_t port, IPhost sender);

mach_port_t
	portm_fast_send_once(mnportid_t nnum, IPhost receiver);

mnetport
	portm_move_netnum(mach_port_t port, mnportid_t netnum);

xkern_return_t
	portm_register(IPhost addr);

xkern_return_t
	portm_register_via_session(XObj session);

mnetport
	convert_to_netport(
	    mach_port_t port, mach_port_type_t netright, IPhost dest,
	    msg_id_t msgid, mnportid_t prev_netport_number);

xkern_return_t
	quick_netport_lookup(unsigned int portid, mnetport **portdesc);

mnetport *
	quick_port_convert(mach_port_t *local_port);

mach_port_t
	convert_netport_to_mach_port(
	    mn_netport_t *netport, XObj self, XObj lower_session,
	    Msg *reply_msg, msg_id_t msgid, IPhost sender);

xkern_return_t
	convert_netport_to_tmp_mach_port(
	    mn_netport_t *netport, unsigned int rtype,
	    IPhost rec_addr, msg_id_t msgid);

static void
	complete_port_transfer(mnetport *netport, Msg *msg  );

static xkern_return_t
	createListener(
	    mnetport *netport_ptr, mach_port_right_t right,
	    XObj lower_session, Msg *reply_msg, IPhost dest);

void	port_kill(mnetport *);

static long
	portm_unbundle(char *, char *, long, void *);

static void
	portm_req_dn_notif(mach_port_t);

void	portm_use_fast_send_once( mach_port_t , boolean_t );

void	portm_move_receiver(mnetport *);

xkern_return_t
	allocLocalRight(mnetport *portd, mach_port_type_t mtype);

static int
	ipcmpfun(void *key, int val, void *arg);


/*
 * convert_to_netport
 *
 *    convert a local Mach port right to network form
 *     and do all necessary bookkeeping, locks, etc.
 *
 *  should return a rwLock'd port
 */
mnetport
convert_to_netport(
	mach_port_t		port,
	mach_port_type_t	netright,
	IPhost			dest,
	msg_id_t		msgid,
	mnportid_t		prev_netport_number)
{
    mnetport		*netport;
    mach_port_type_t	rights;
    Bind		ret;
    kern_return_t	kret;
    mn_netport_t	*port_array[2];

    xTrace2(portmaintp, TR_FULL_TRACE,
	"portmaintp: enter convert_to_netport %x %x", port, netright);

    if (port == MACH_PORT_NULL) return Null_Netport;

    /*
     * do we already have a network representation for this port?
     */
    if (mapResolve(STATEPTR->maplocalports, &port, &netport) != XK_FAILURE) {
	xTrace3(portmaintp, TR_EVENTS,
	    "convert_to_netport found existing structure for port %x (name %x) at addr %x",
	    netport->net_port_number, port, netport);
	xTrace4(portmaintp, TR_EVENTS, "convert_to_netport dest %d.%d.%d.%d",
	    dest.a, dest.b, dest.c, dest.d);
	xTrace4(portmaintp, TR_EVENTS, "convert_to_netport recvr %d.%d.%d.%d",
	    netport->receiver_host_addr.a,
	    netport->receiver_host_addr.b,
	    netport->receiver_host_addr.c,
	    netport->receiver_host_addr.d);

	/*
	 * must hold the right to be distributed or must have the receive right
	 */
	/*
	 * send right .. no receive, we are the receiver node, update msc
	 * send right .. receive, same
	 * sendonce   .. we are the receiver node; will never have receive right
	 * receive    .. we will not be the receiver node; forward queue, delete
	 *               local net_right (not local right), start listener

	if (!(netright & netport->net_port_local_rights) &&
	    !(netport->net_port_local_rights & MACH_PORT_TYPE_RECEIVE)) {
	    xTrace2(portmaintp, TR_ERRORS,
		"portmaint: convert_to_netport: tried to move right %x but only had %x",
		netright, netport->net_port_local_rights);
	    return Null_Netport;
	}

	 */

	/*
	 * is this a three party transfer?  will use protocol if it is
	 */
	if ((!IPEQUAL(dest, netport->receiver_host_addr)) &&
	    (!IPEQUAL(dest, STATEPTR->local_source_addr)) &&
	    (!IPEQUAL(netport->receiver_host_addr,
		      STATEPTR->local_source_addr))) {
	    if (netright == MACH_PORT_TYPE_SEND) {
		port_array[0] = (mn_netport_t *)netport;
		port_array[1] = 0;
		if (srxMoveSendRights(dest, msgid, (mnetport **)port_array)
		    == XK_FAILURE) {
		    xTrace1(portmaintp, TR_ERRORS,
			"portmaint: convert_to_netport could not move send right for port %x",
			netport->net_port_number);
		    return Null_Netport;
		}
		mapBind(xkMsgIdMap, &msgid, 0);
#ifdef PORTLOCKS
		xTrace1(portmaintp, TR_EVENTS,
		    "portmaint: convert_to_netport: Getting READER LOCK on port %x",
		    netport->net_port_number);
		if (readerLock(netport->rwlock_ptr) == XK_SUCCESS) {
		    return *netport;
		} else {
		    return Null_Netport;
		}
#else	/* !PORTLOCKS */
		return *netport;
#endif	/* !PORTLOCKS */

	    } else if (netright == MACH_PORT_TYPE_RECEIVE) {
		xTrace0(portmaintp, TR_EVENTS,
		    "portmaintp: moving 3-party receive right");
		port_array[0] = (mn_netport_t *)netport;
		port_array[1] = 0;
		if (rrxMoveReceiveRights(
			dest, msgid, (mnetport **)port_array)
		    == XK_FAILURE) {
		    xTrace1(portmaintp, TR_ERRORS,
			"portmaint: convert_to_netport could not move send right for port %x",
		    netport->net_port_number);
		    return Null_Netport;
		}
		if (createListener(netport,
			MACH_PORT_TYPE_RECEIVE, ERR_XOBJ, (Msg *)0, dest)
		    == XK_SUCCESS) {
#ifdef PORTLOCKS
		    xTrace1(portmaintp, TR_EVENTS,
			"portmaint: convert_to_netport: Getting READER LOCK on port %x",
			netport->net_port_number);
		    if (readerLock(netport->rwlock_ptr) == XK_SUCCESS) {
			return *netport;
		    } else {
			return Null_Netport;
		    }
#else	/* PORTLOCKS */
		    return *netport;
#endif	/* PORTLOCKS */
		} else { 
		    mapBind(xkMsgIdMap, &msgid, 1); 
		    return Null_Netport;
		}
	    } else {
		xTrace1(portmaintp, TR_ERRORS, "portmaint: convert_to_netport: unknown right %x",
		    netright);
		return Null_Netport;
	    }
	}     /* end of three-party transfer */

	else if (netright == MACH_PORT_TYPE_SEND &&
		IPEQUAL(netport->receiver_host_addr,
		    STATEPTR->local_source_addr)) {
	/*
	 * for send rights, if we are the receiver, must add the
	 * new sending node to the senders list.
	 *
	 * should also check for port lock and wait until clear
	 */
	    if (mapResolve(netport->senders_map, &dest, 0) == XK_SUCCESS) {
		mapUnbind(netport->senders_map, &dest);
	    } else {
		netport->sender_count++;
	    }
	    mapBind(netport->senders_map, &dest, ++netport->make_send_count);
#ifdef PORTLOCKS
	    xTrace1(portmaintp, TR_EVENTS,
		"portmaint: convert_to_netport: Getting READER LOCK on port %x",
		netport->net_port_number);
	    if (readerLock(netport->rwlock_ptr) == XK_SUCCESS) {
		return *netport;
	    } else {
		return Null_Netport;
	    }
#else	/* PORTLOCKS */
	    return *netport;
#endif	/* PORTLOCKS */
	} else if (netright == MACH_PORT_TYPE_RECEIVE &&
	    IPEQUAL(netport->receiver_host_addr, STATEPTR->local_source_addr)) {
	    xTrace0(portmaintp, TR_EVENTS,
		"portmaintp: moving 2or3-party receive right");
	    if (!(netport->net_port_rights & MACH_PORT_TYPE_RECEIVE)) {
		int	refs;

		netport->net_port_rights |= MACH_PORT_TYPE_RECEIVE;

		/*
		 * delete the send right that we had previously
		 */
		if ((kret = mach_port_get_refs(mach_task_self(),
			netport->real_send_port,
			MACH_PORT_RIGHT_SEND,
			(mach_port_urefs_t *)&refs))
		    != KERN_SUCCESS) {
		    xTrace2(portmaintp, TR_ERRORS,
			"portmaintp: convert_netport: could not get ref for port %x code %x",
			netport->real_send_port, kret);
		}
		xTrace2(portmaintp, TR_FULL_TRACE,
		    "portmaintp: convert_netport: refs for port %x are %d",
		    netport->real_send_port, refs);

		if ((kret = mach_port_mod_refs(mach_task_self(),
			netport->real_send_port, MACH_PORT_RIGHT_SEND, -1))
		    != KERN_SUCCESS) {
		    xTrace2(portmaintp, TR_ERRORS,
			"portmaintp: convert_netport: could not mod ref for port %x code %x",
			netport->real_send_port, kret);
		}
	    }

	    netport->real_receive_port = port;
	    netport->net_port_rights |= MACH_PORT_TYPE_SEND;

	    /*
	     * we must include ourselves as a sender; if we later get a
	     * no more senders notification we can send an nms notification
	     * to the new receiver
	     */
	    mapBind(netport->senders_map,
		&(STATEPTR->local_source_addr), ++netport->make_send_count);
	    /*
	     * if we are a sender, we should have make_send_count > 1
	     */
	    netport->sender_count++;

	    /*
	     * this node is the receiver; see if there are senders other
	     * than the intended recipient of this message
	     */
	    cmpresult = 0;
	    mapForEach((void *)netport->senders_map,
		(MapForEachFun)ipcmpfun, (void *)&dest);
	    if (cmpresult) {
		port_array[0] = (mn_netport_t *)netport;
		port_array[1] = 0;
		xTrace3(portmaintp, TR_EVENTS,
		    "portmaintp: convert_to_netport: will move receive rights; netport %x: (readers = %d) (writers = %d)",
		    netport->net_port_number,
		    netport->rwlock.activeReaders,
		    netport->rwlock.activeWriter);
		if (rrxMoveReceiveRights(dest, msgid, (mnetport **)port_array)
		    == XK_FAILURE) {
		    xTrace1(portmaintp, TR_ERRORS,
			"portmaint: convert_to_netport could not move receive right for port %x",
			netport->net_port_number);
		    return Null_Netport;
		}
		mapBind(xkMsgIdMap, &msgid, 1);
		if (createListener(netport,
			MACH_PORT_TYPE_RECEIVE, ERR_XOBJ, (Msg *)0, dest)
		    == XK_SUCCESS) {
#ifdef PORTLOCKS
		    xTrace1(portmaintp, TR_EVENTS,
			"portmaint: convert_to_netport: Getting READER LOCK on port %x",
			netport->net_port_number);
		    if (readerLock(netport->rwlock_ptr) == XK_SUCCESS) {
			return *netport;
		    } else {
			return Null_Netport;
		    }
#else	/* PORTLOCKS */
		    return *netport;
#endif	/* PORTLOCKS */
		} else {
		    return Null_Netport;
		}
	    }
	}
	xTrace1(portmaintp, TR_EVENTS,
	    "portmaintp: convert_to_netport: moving simple send right: 0x%x",
	    netright);
	/* this couldn't really be a send_once right;
	 * we don't know when one is moved
	 */
#ifdef PORTLOCKS
	xTrace1(portmaintp, TR_EVENTS,
	    "portmaint: convert_to_netport: Getting READER LOCK on port %x",
	    netport->net_port_number);
	if (readerLock(netport->rwlock_ptr) == XK_SUCCESS) {
	    return *netport;
	} else {
	    return Null_Netport;
	}
#else	/* PORTLOCKS */
	return *netport;
#endif	/* PORTLOCKS */
    }

    /*
     * a network descriptor must be created; only two parties are involved
     */
    xTrace0(portmaintp, TR_EVENTS, "portmaint creating new net port structure");
    netport = TYPE_MALLOC(mnetport);
    bzero((char *)netport, sizeof(mnetport));
    netport->lower_session = ERR_XOBJ;

    if ((ret = mapBind( STATEPTR->maplocalports, &port, netport))
	!= ERR_BIND) {
	/* 
	 * will make it possible to look up the netport description given
	 *  local name 
	 */
	if ((kret=mach_port_type(mach_task_self(), port, &rights))
	    != KERN_SUCCESS) {
	    xTrace2(portmaintp, TR_ERRORS,
		"portmaintp: convert to netport: could not get type of port %x failure code %x",
		port, kret);
	    return (Null_Netport);
	}
	netport->net_port_number =
	    prev_netport_number ? prev_netport_number : netport_id++;
	netport->receiver_host_addr = STATEPTR->local_source_addr;
	/*
	 * NB, for a new port created locally, this node IS the receiver
	 */
	rwLockInit(&netport->rwlock);
	netport->rwlock_ptr = &netport->rwlock;
	netport->senders_map =
	    mapCreate(INITIAL_MAX_MNIPC_SENDERS, sizeof(IPhost));
	netport->session_map =
	    mapCreate(PORTM_PORT_LOWER_SESSIONS, sizeof(XObj));
	semInit(&netport->listen_sem, 1);

#ifdef PORTLOCKS
	xTrace1(portmaintp, TR_EVENTS,
	    "portmaint: convert_to_netport: Getting READER LOCK on port %x",
	    netport->net_port_number);
	if (readerLock(netport->rwlock_ptr) != XK_SUCCESS) {
	    xTrace0(portmaintp, TR_ERRORS,
	    "portmaint: convert to netport: could not lock new structure??!!");
	    return Null_Netport;
	}
#endif	/* PORTLOCKS */

	xTrace3(portmaintp, TR_FUNCTIONAL_TRACE,
	    "convert_to_netport rights %x send once %x receive %x",
	    rights, MACH_PORT_TYPE_SEND_ONCE, MACH_PORT_TYPE_RECEIVE);
	if ((netright == MACH_PORT_TYPE_SEND && (rights & MACH_PORT_TYPE_SEND))
	   || (netright == MACH_PORT_TYPE_SEND_ONCE &&
	      (rights & (MACH_PORT_TYPE_SEND_ONCE | MACH_PORT_TYPE_RECEIVE)))) {
	    netport->net_port_local_rights = rights;
	    netport->net_port_rights = netright;
	    netport->net_port_type = MN_VALID;
	    netport->real_send_port = port;
	    netport->make_send_count = INIT_MAKE_SEND_COUNT;
	    netport->sender_count++;
	    mapBind(netport->senders_map, &dest, INIT_MAKE_SEND_COUNT);
	    /*
	     * will also bind the netport_id to the netport descriptor
	     */
	    xTrace0(portmaintp, TR_FULL_TRACE,
		"portmaintp: convert_to_netport will attempt bind send or sendonce");
	    if (mapBind(STATEPTR->mapnetdesc,
		    &(netport->net_port_number), netport)
		== ERR_BIND) {
		xTrace1(portmaintp, TR_SOFT_ERROR,
		    "portmaintp: convert_to_netport could not bind port number %x",
		    netport->net_port_number);
		return Null_Netport; /* invalid ! */
	    } else {
		xTrace2(portmaintp, TR_EVENTS,
		    "portmaintp: convert_to_netport bound id %x to addr %x",
		    netport->net_port_number, netport);
		portm_req_dn_notif(port);
		if (!(IPEQUAL(ip_null, dest))) {
		    portm_register(dest);
		}
		return(*netport);
	    }
	}

	if (netright==MACH_PORT_TYPE_RECEIVE &&
	    (rights & MACH_PORT_TYPE_RECEIVE)) {
	    /*
	     *  we are preparing to transfer a receive right ...
	     *  we will no longer be the receiver after the transfer.
	     *  but we must start a listening thread for the send rights
	     *  that are probably outstanding; until an nms note arrives
	     */
	    xTrace1(portmaintp, TR_EVENTS,
		"transferring receive right %x", port);
	    /*
	     * netport->net_port_rights =
	     *	MACH_PORT_TYPE_RECEIVE | MACH_PORT_TYPE_SEND;
	     */
	    netport->net_port_rights = MACH_PORT_TYPE_SEND;
	    netport->net_port_local_rights = MACH_PORT_TYPE_RECEIVE;
	    netport->net_port_type = MN_VALID;  /* ??? */
	    netport->real_receive_port = port;
	    netport->receiver_host_addr = dest;
	    netport->senders_map =
		mapCreate(INITIAL_MAX_MNIPC_SENDERS,sizeof(IPhost));

	    /*
	     * will also bind the netport_id to the netport descriptor
	     */
	    xTrace0(portmaintp, TR_FULL_TRACE,
		"portmaintp: convert_to_netport will attempt bind send or sendonce");
	    if (mapBind( STATEPTR->mapnetdesc,
		    &(netport->net_port_number), netport)
		== ERR_BIND) {
		xTrace1(portmaintp, TR_SOFT_ERROR,
		    "portmaintp: convert_to_netport could not bind port number %x",
		    netport->net_port_number);
		return Null_Netport; /* invalid ! */
	    } else {
		xTrace2(portmaintp, TR_EVENTS,
		    "portmaintp: convert_to_netport bound id %x to addr %x",
		    netport->net_port_number, netport);
		if (!(IPEQUAL(ip_null, dest))) portm_register(dest);
	    }

	    /*
	     * we must include ourselves as a sender; if we later get a
	     * no more senders notification we can send an nms notification
	     * to the new receiver
	     */
	    mapBind(netport->senders_map,
		&(STATEPTR->local_source_addr), ++netport->make_send_count);
	    /*
	     * if we are a sender, we should have make_send_count > 1
	     */
	    netport->sender_count++;
	    if (createListener(netport,
		    MACH_PORT_TYPE_RECEIVE, ERR_XOBJ, (Msg *)0, dest)
		== XK_SUCCESS) {
		return(*netport);
	    } else {
		return Null_Netport;
	    }
	}
    } else {
	/*
	 * somehow we cannot lookup, we cannot add; should never get here
	 */
	xTrace5(portmaintp, TR_ERRORS,
	    "portmaintp: convert local port error in localports map; return code %d port %x port addr %x netport addr %x portid# %d",
	    ret, port, &port, netport, netport->net_port_number);
    }

    /*
     * might mean we did not have the right we were supposed to transfer
     */
    xTrace0(portmaintp, TR_ERRORS,
	"portmaintp: convert to netport: did not have right to be moved or had map error");

    return Null_Netport;
}


static int
ipcmpfun(
	void	*key,
	int	val,
	void	*arg)
{
    IPhost	mhost = *((IPhost *)key);
    IPhost	dhost = *((IPhost *)arg);

    if ( ! (IPEQUAL (mhost, dhost))) { 
	cmpresult = 1; 
	return 0; 
    }
    return MFE_CONTINUE;
}


/*
 * convert_netport_to_mach_port
 *
 * port descriptor from net must be looked up.
 * if new, must create local port to correspond, associate netport
 * and local port in database
 *
 * the return value of this routine is the local port to substitute
 * when transferring this right to a local task; we use the receive
 * right with method MOVE_SEND or MAKE_SEND_ONCE or MOVE_RECEIVE.
 *
 * the ip address of the sending host should be communicated to the
 * bootid protocol; the message should be validated before processing,
 * and the bootid protocol should keep tabs on the host.
 */
mach_port_t
convert_netport_to_mach_port(
	mn_netport_t	*netport,
	XObj		self,
	XObj		lower_session,
	Msg		*reply_msg,
	msg_id_t	msgid,
	IPhost		sender)
{
    mach_port_right_t 	right_type;
    mach_port_t		new_port_right = MACH_PORT_NULL;
    kern_return_t	ret;
    mnetport		*netport_ptr;
    mach_msg_type_name_t  returned_type;

    xTrace0(portmaintp, TR_FULL_TRACE,
	"portmaint: enter convert_netport_to_mach_port");
    xTrace2(portmaintp, TR_EVENTS,
	"portmaint: convert p# %x type %x",
	netport->net_port_number, netport->net_port_rights);
    xTrace4(portmaintp, TR_FULL_TRACE,
	"portmaint: convert from receiver host %d.%d.%d.%d",
	netport->receiver_host_addr.a,
	netport->receiver_host_addr.b,
	netport->receiver_host_addr.c,
	netport->receiver_host_addr.d);

    /*
     * find out if this right is already known on this machine
     */
    if (netport->net_port_number == 0) {
	return MACH_PORT_NULL;
    }

    if (mapResolve(STATEPTR->mapnetdesc,
	    &(netport->net_port_number), &netport_ptr)
	== XK_SUCCESS) {
	/*
	 *  We have received a descriptor that matches ones we already
	 *  have.  This means we are getting an additional right
	 *  for the port.  The local netport structure must be updated,
	 *  and new rights created from old.  In the case of getting
	 *  the receive right, we must create a new local port.
	 *
	 *  This code branch will return; the following branch is only
	 *  for new descriptors.
	 */
	xTrace3(portmaintp, TR_EVENTS,
	    "portmaintp: convert_to_netport: will move receive rights; netport %x: (readers = %d) (writers = %d)",
	    netport_ptr->net_port_number,
	    netport_ptr->rwlock.activeReaders,
	    netport_ptr->rwlock.activeWriter);

	/*
	 * check if this right was being used by a previous unfinished
	 * reply message the xCall of which is still pending
	 */
	if (portm_quick_so_lookup(netport->net_port_number) != MACH_PORT_NULL &&
		netport_ptr->net_port_rights & MACH_PORT_TYPE_SEND_ONCE) {
	    portm_port_remove_send_once(netport_ptr);
	    xTrace1(portmaintp, TR_ERRORS,
		"portmaint: Removed Send Once right for a pending reply for netport id %x",
		netport->net_port_number);
	    goto new_port;
	}

	xTrace2(portmaintp, TR_MORE_EVENTS,
	    "convert_netport: right transferred; already had structure with rights net %x local %x",
	    netport_ptr->net_port_rights,
	    netport_ptr->net_port_local_rights);
	/*
	 * if a send right, this task holds the receive right; we'll need to
	 * generate a send or send_once right to match.
	 */
	if ((netport->net_port_rights & MACH_PORT_TYPE_RECEIVE) &&
	/*
	 *  (netport_ptr->net_port_local_rights & MACH_PORT_TYPE_RECEIVE)) {}
	 */
	    (netport_ptr->net_port_rights & MACH_PORT_TYPE_RECEIVE)) {
	    if (netport_ptr->net_port_type != MN_WAITING) {
		xTrace0(portmaintp, TR_ERRORS,
		    "port maint Got receive right already held; error");
		return MACH_PORT_NULL;  /* invalid */
	    } else {
		xTrace1(portmaintp, TR_ERRORS,
		    "port maint completed receipt of 3 party receive right local port %x",
		    netport_ptr->real_receive_port);
		netport_ptr->net_port_type = MN_FORWARDING;
		new_port_right = netport_ptr->real_receive_port;
		netport_ptr->real_receive_port = MACH_PORT_NULL;
		return new_port_right; /* don't listen! */
	    }
	}
	if (netport->net_port_rights & MACH_PORT_TYPE_RECEIVE) {
	    /*
	     * we had send right(s), got receive via 2-party transfer
	     */
	    xTrace1(portmaintp, TR_FUNCTIONAL_TRACE,
		"got receive right port id %x",
		netport->net_port_number);
	    xTrace3(portmaintp, TR_EVENTS,
		"portmaintp: convert_netport_to_mach_port: got a receive right; netport %x: (readers = %d) (writers = %d)",
		netport_ptr->net_port_number,
		netport_ptr->rwlock.activeReaders,
		netport_ptr->rwlock.activeWriter);

	    netport_ptr->net_port_rights |= MACH_PORT_TYPE_RECEIVE;
	    /*
	     * we will MOVE this; won't have local receive right anymore
	     */
	    netport_ptr->receiver_host_addr = STATEPTR->local_source_addr;

	    /*
	     * new_port_right is MachNetIPC's port to send to the receiver
	     * is the former receive a sender?  are we?
	     * if neither, we should not retain an entry for this port;
	     * let the receiver get the no more senders notification
	     * if he wants it.
	     */
	    if (netport_ptr->senders_map == NULL) {
		netport_ptr->senders_map =
		    mapCreate(INITIAL_MAX_MNIPC_SENDERS, sizeof(IPhost));
		/*
		 * if the old receive is a sender, add him to map 
		 * netport_ptr->sender_count = netport->make_send_count ? 0 : 1;
		 */
	    }
	    /*
	     *	if (netport_ptr->sender_count) {
	     *	    mapBind(netport_ptr->senders_map,
	     *		&sender, netport_ptr->make_send_count);
	     */

	    /*
	     * HASNAIN: assume that old receiver is a sender,
	     * we will get an nms notification from it, if it isnt
	     */

	    netport_ptr->sender_count = 1;

	    /*
	     * use the make send count sent in the message i.e. of netport
	     */
	    mapUnbind(netport_ptr->senders_map, &sender);
	    mapBind(netport_ptr->senders_map,
		&sender, netport->make_send_count);

	    CLOCK_TRACE(
		"convert_netport_to_mach_port: C mach_port_extract_right 1");
	    ret = mach_port_extract_right(
		    mach_task_self(),
		    netport_ptr->real_receive_port,
		    MACH_MSG_TYPE_MAKE_SEND,
		    &netport_ptr->real_send_port,
		    &returned_type);
	    CLOCK_TRACE(
		"convert_netport_to_mach_port: R mach_port_extract_right");
	    if (ret != KERN_SUCCESS) {
		xTrace2(portmaintp, TR_ERRORS,
		    "portmaint: convert_netport Failed to extract send right from port %x code %x",
		    netport_ptr->real_receive_port, ret);
		return MACH_PORT_NULL;
	    }
	    netport_ptr->net_port_local_rights |= MACH_PORT_TYPE_SEND;
	    netport_ptr->make_send_count = netport->make_send_count;
	    /*
	     *	}
	     */

	    new_port_right = netport_ptr->real_receive_port;
	    netport_ptr->real_receive_port = MACH_PORT_NULL;
	    return new_port_right;
	}

	/*
	 * we are getting a send right
	 */
	if (((netport->net_port_rights & MACH_PORT_TYPE_SEND) &&
	     (netport_ptr->net_port_local_rights & MACH_PORT_TYPE_SEND))
	    ||
	    ((netport_ptr->net_port_local_rights & MACH_PORT_TYPE_RECEIVE) &&
	     (netport->net_port_rights &
		(MACH_PORT_TYPE_SEND_ONCE | MACH_PORT_TYPE_SEND)))) {
	    if (IPEQUAL(
		    netport->receiver_host_addr, STATEPTR->local_source_addr)) {
		xTrace1(portmaintp, TR_DETAILED,
		    "portmaint: convert_netport: send right to own port %x",
		    netport->net_port_number);
		if (!netport_ptr->real_send_port) {
		    xTrace1(portmaintp, TR_ERRORS,
			"portmaint: convert_netport: send right to own port %x but no send port",
			netport->net_port_number);
		    return MACH_PORT_NULL;
		}
	    }
#ifdef MAKESENDCOUNT
	    /*
	     * only update receiver msc when issuing rights
	     */
	    else if (netport->make_send_count > 0 &&
		     netport->make_send_count > netport_ptr->make_send_count) {
		netport_ptr->make_send_count = netport->make_send_count;
	    }
#endif	/* MAKESENDCOUNT */

	    if (netport->net_port_rights & MACH_PORT_TYPE_SEND) {
		/*
		 * need to have a map of all lower sessions to close them
		 */
		if (!netport_ptr->real_receive_port) { /* I AM the net rcvr */
		    if (lower_session != ERR_XOBJ) {
			if (netport_ptr->lower_session == ERR_XOBJ) {
			    netport_ptr->lower_session = lower_session;
			} else if (mapResolve(netport_ptr->session_map,
					&lower_session, 0)
				   != XK_SUCCESS) {
			    mapBind(netport_ptr->session_map,
				&lower_session, 0);
			    xDuplicate(lower_session);
			}
		    }
		    CLOCK_TRACE(
			"convert_netport_to_mach_port: C mach_port_mod_refs");
		    if ((ret = mach_port_mod_refs(
			    mach_task_self(),
			    netport_ptr->real_send_port,
			    MACH_PORT_RIGHT_SEND, 1))
			!= KERN_SUCCESS) {
			mach_port_urefs_t	refs;
			xTrace2(portmaintp, TR_ERRORS,
			    "portmaintp: convert_netport: could not mod ref for port %x code %x",
			    netport_ptr->real_send_port, ret);
			if ((ret = mach_port_get_refs(
				mach_task_self(), netport_ptr->real_send_port,
				MACH_PORT_RIGHT_DEAD_NAME, &refs))
			    == KERN_SUCCESS) {
			    xTrace1(portmaintp, TR_ERRORS,
				"Number of dead refs = %d", refs);
			}
			if ((ret = mach_port_get_refs(
				mach_task_self(), netport_ptr->real_send_port,
				MACH_PORT_RIGHT_RECEIVE, &refs))
			    == KERN_SUCCESS) {
			    xTrace1(portmaintp, TR_ERRORS,
				"Number of receive refs = %d", refs);
			}
			return MACH_PORT_NULL;
		    }
		    CLOCK_TRACE(
			"convert_netport_to_mach_port: R mach_port_mod_refs");
		} else {  /* I am NOT the net rcvr */
		    CLOCK_TRACE("convert_netport_to_mach_port: C mach_port_extract_right 2");
		    if ((ret = mach_port_extract_right(
			    mach_task_self(),
			    netport_ptr->real_receive_port,
			    MACH_MSG_TYPE_MAKE_SEND,
			    &netport_ptr->real_send_port,
			    &returned_type))
			!= KERN_SUCCESS) {
			xTrace2(portmaintp, TR_ERRORS,
			    "portmaintp: could not extract right for port %x code %x",
			    netport_ptr->real_receive_port, ret);
			return MACH_PORT_NULL;
		    }
		    CLOCK_TRACE("convert_netport_to_mach_port: R mach_port_extract_right");
		    /*
		     * register our interest in this host
		     * with the bootid protocol
		     */
		    if ((ret = portm_register(netport->receiver_host_addr)
			!= XK_SUCCESS)) {
			xTrace0(portmaintp, TR_ERRORS,
			    "portmaintp: convert_port portm register failed");
			return MACH_PORT_NULL;
		    }
		}
	    }

	    if (netport_ptr->senders_map == NULL) {
		netport_ptr->senders_map =
		    mapCreate(INITIAL_MAX_MNIPC_SENDERS, sizeof(IPhost));
	    }
	    addNewSender(netport_ptr, sender, 0);

	    if (netport_ptr->net_port_type == MN_WAITING &&
		netport_ptr->msgid == msgid &&
		!bcmp((char *)&netport_ptr->orig_host,
		    (char *)&sender, sizeof(IPhost))) {
		complete_port_transfer(netport_ptr, reply_msg);
	    }
	    return netport_ptr->real_send_port;
	}
	if (netport->net_port_rights & MACH_PORT_TYPE_SEND_ONCE) {
	    /*
	     * apparently someone gave out a send once right for this port
	     * and it has come back ...
	     */
	    xTrace2(portmaintp, TR_FUNCTIONAL_TRACE,
		"got send_once right port %x local rights %x",
		netport_ptr->real_send_port,
		netport_ptr->net_port_local_rights);
	    /*
	     * can only use it once
	     */
	    netport_ptr->net_port_local_rights &= ~(MACH_PORT_TYPE_SEND_ONCE);
	    netport_ptr->net_port_type = MN_INVALID;
	    if (netport_ptr->current_read_desc) {
		netport_ptr->current_read_desc->validity = FALSE;
	    }
	    /*
	     * to invalidate a port, must remove it from maps, too
	     */
	    CLOCK_TRACE("convert_netport_to_mach_port: C mach_port_type");
	    if (((ret=mach_port_type(
			mach_task_self(), 
			netport_ptr->real_send_port,
			&right_type))
		  != KERN_SUCCESS)
		|| (!(right_type & MACH_PORT_TYPE_SEND_ONCE))) {
		xTrace3(portmaintp, TR_ERRORS,
		    "portmaint: convert_netport: don't have send once right for port %x, return code %x, right %x",
		    netport_ptr->real_send_port, ret, right_type);
		return MACH_PORT_NULL;
	    } else {
		CLOCK_TRACE("convert_netport_to_mach_port: R mach_port_type");
		return netport_ptr->real_send_port;
	    }
	} else {
	    xTrace0(portmaintp, TR_ERRORS,
		"port maint had existing net right but had neither send nor receive");
	    return MACH_PORT_NULL;
	}
    }

new_port:
    if (mapResolve(STATEPTR->mapsendoncenumbers,
	    &netport->net_port_number, &new_port_right)
	== XK_SUCCESS) {
	xTrace2(portmaintp, TR_EVENTS,
	    "portmaint: convert_netport_to_mach using rpc undo port, local name %x number %x",
	    new_port_right, netport->net_port_number);
	portm_use_netnum(new_port_right, netport->net_port_number);
	return new_port_right;
    }

    /* 
     * port is not known yet; must create all local structure
     */
    CLOCK_TRACE("convert_netport_to_mach_port: creating local structure");
    /*
     * create all structure needed to implement new port
     */
    netport_ptr = TYPE_MALLOC(mnetport);
    bzero((char *)netport_ptr, sizeof(mnetport));
    /*
     * copy the data from the net into malloc'd storage
     */
    netport_ptr->lower_session = ERR_XOBJ;
    netport_ptr->net_port_number = netport->net_port_number;
    netport_ptr->net_port_rights = netport->net_port_rights;
    netport_ptr->receiver_host_addr = netport->receiver_host_addr;
    rwLockInit(&netport_ptr->rwlock);
    netport_ptr->rwlock_ptr = &netport_ptr->rwlock;
    semInit(&netport_ptr->listen_sem, 1);

    CLOCK_TRACE("convert_netport_to_mach_port: C mach_port_allocate");
    if ((ret = mach_port_allocate(mach_task_self(),
		MACH_PORT_RIGHT_RECEIVE, &netport_ptr->real_receive_port))
	!= KERN_SUCCESS) {
	xTrace1(portmaintp, TR_ERRORS,
	    "portmaint convert netport to machport could not allocate local port %x",
	    ret);
	return MACH_PORT_NULL;
    }
    CLOCK_TRACE("convert_netport_to_mach_port: R mach_port_allocate");
    netport_ptr->net_port_local_rights = MACH_PORT_TYPE_RECEIVE;

    xTrace1(portmaintp, TR_EVENTS,
	"portmaint: convert_netport_to_mach absorbing new net port: local port = %x",
	netport_ptr->real_receive_port);

    /*
     * Register interest with bootid control
     *
     * Establish the binding from the local port to the descriptor
     * and from the netport_id to the descriptor.
     *
     * Then start a listener (for send rights)
     */
    if ((ret = portm_register(netport->receiver_host_addr)) != XK_SUCCESS) {
	xTrace0(portmaintp, TR_ERRORS,
	    "portmaintp: convert_netport_to_mach_port: Cannot register receiver host");
    }

    if (mapBind(STATEPTR->mapnetdesc, &netport->net_port_number, netport_ptr)
	== ERR_BIND) {
	xTrace1(portmaintp,TR_ERRORS,
	    "portmaintp: convert_netport error binding net number %x",
	    netport->net_port_number);
	return MACH_PORT_NULL;
    }

    xTrace1(portmaintp, TR_EVENTS,
	"portmaint: convert_netport_to_mach bound netport number %x",
	netport->net_port_number);

    if (mapBind(STATEPTR->maplocalports,
	    &netport_ptr->real_receive_port, netport_ptr)
	== ERR_BIND) {
	xTrace0(portmaintp, TR_ERRORS,
	    "convert_netport error binding netport ptr");
	return MACH_PORT_NULL;
    }
    /* 
     *  Incoming right is a send(once) right
     */
    if (netport->net_port_rights & MACH_PORT_TYPE_SEND) {
	CLOCK_TRACE(
	    "convert_netport_to_mach_port: C mach_port_extract_right 3");
	if ((ret = 
		mach_port_extract_right(mach_task_self(),
		    netport_ptr->real_receive_port,
		    MACH_MSG_TYPE_MAKE_SEND,
		    &netport_ptr->real_send_port,
		    &returned_type))
	    != KERN_SUCCESS) {
	    xTrace2(portmaintp, TR_ERRORS,
		"portmaintp: could not extract right for port %x code %x",
		netport_ptr->real_receive_port, ret);
	    return MACH_PORT_NULL;
	}
	CLOCK_TRACE(
	    "convert_netport_to_mach_port: C mach_port_extract_right 4");
#ifdef MAKESENDCOUNT
	if (netport->net_port_rights & MACH_PORT_TYPE_SEND) {
	    netport_ptr->make_send_count = netport->make_send_count;
	}
#endif	/* MAKESENDCOUNT */
    }

    if ((netport->net_port_rights & MACH_PORT_TYPE_SEND) ||
	(netport->net_port_rights & MACH_PORT_TYPE_SEND_ONCE)) {
	/* 
	 * for a send right, port maint must listen and forward the data
	 * to the remote nethost
	 */
	xTrace3(portmaintp, TR_FULL_TRACE,
	    "convert_netport beginning Mach port send/listen for right received from net: number %x port send %x port recv %x",
	    netport->net_port_number,
	    netport_ptr->real_send_port,
	    netport_ptr->real_receive_port);
	CLOCK_TRACE("convert_netport_to_mach_port: C createListener");
	if (createListener(
		netport_ptr, netport->net_port_rights,
		lower_session, reply_msg, sender)
	    == XK_SUCCESS){
	    CLOCK_TRACE("convert_netport_to_mach_port: R createListener");
	    return netport_ptr->real_receive_port;
	} else {
	    return MACH_PORT_NULL;
	}
    }

    /* 
     *  Incoming right is a receive right
     */
    if (netport->net_port_rights & MACH_PORT_TYPE_RECEIVE) {
	/* 
	 * transfer receive port to the local task via machipc;
	 * just need to record that we are the receiver
	 */
	xTrace0(portmaintp, TR_FUNCTIONAL_TRACE,
	    "portmaint: convert_netport got receive right");
	if (mapBind(
		STATEPTR->mapnetdesc, &(netport->net_port_number), netport_ptr)
	    != ERR_BIND) {
	    netport_ptr->receiver_host_addr = STATEPTR->local_source_addr;
	    netport_ptr->senders_map =
		mapCreate(INITIAL_MAX_MNIPC_SENDERS, sizeof(IPhost));
	    /*
	     * HASNAIN: assume that old receiver is a sender,
	     * we will get an nms notification from it, if it isnt
	     */
	    netport_ptr->sender_count = 1;

	    /*
	     * use the make send count sent in the message i.e. of netport
	     */
	    mapUnbind(netport_ptr->senders_map, &sender);
	    mapBind(netport_ptr->senders_map, &sender,
		netport->make_send_count);


	    ret = mach_port_extract_right(
		    mach_task_self(),
		    netport_ptr->real_receive_port,
		    MACH_MSG_TYPE_MAKE_SEND,
		    &netport_ptr->real_send_port,
		    &returned_type);
	    if (ret != KERN_SUCCESS) {
		xTrace2(portmaintp, TR_ERRORS,
		    "portmaint: convert_netport cannot extract send right: %x code %x",
		    netport_ptr->real_receive_port, ret);
		return MACH_PORT_NULL;
	    }
	    netport_ptr->real_send_port = netport_ptr->real_receive_port;
	    xTrace2(portmaintp, TR_MORE_EVENTS,
		"portmaint: convert_netport binding %x to structure at %x",
		netport->net_port_number, netport_ptr);

	    /*
	     *  portm_req_dn_notif(netport_ptr->real_receive_port, );
	     */
	    xTrace0(portmaintp, TR_MORE_EVENTS,
		"convert_netport returning bound new port");
	    new_port_right = netport_ptr->real_receive_port;
	    netport_ptr->real_receive_port = MACH_PORT_NULL;
	    return new_port_right;
	} else {
	    xTrace0(portmaintp, TR_ERRORS,
		"convert_netport could not bind addr");
	    xFree ((void *)netport_ptr);
	    return MACH_PORT_NULL;
	}
    }

    /*
     * Never did figure out what this thing was ...
     */
    xTrace0(portmaintp, TR_ERRORS, "portmaint: convert_netport unknown error");
    return MACH_PORT_NULL;
}

/*
 *    createListener
 *
 *      used to activate outgoing receive right (for local senders)
 *	and to activate incoming send rights
 */
static xkern_return_t
createListener(
	mnetport		*netport_ptr,
	mach_port_right_t	rights,
	XObj			lower_session,
	Msg			*reply_msg,
	IPhost			dest)
{
    struct send_request	*args = TYPE_MALLOC(struct send_request);

    xTrace0(portmaintp, TR_FULL_TRACE, "portmaintp: createListener: called");

    bzero((char *)args, sizeof(struct send_request));
    /*
     * args will be freed by listener
     */

    args->port = netport_ptr->real_receive_port;
    args->self = machnetipcProtl;  /* we are doing this for machnetipc */
    if (lower_session != ERR_XOBJ) {
	args->lower_session = lower_session;
    } else {
	Part	whom[1];

	partInit(whom, 1);
	partPush(whom[REMOTE_PART], &dest, sizeof(IPhost));

	if ((args->lower_session =
		xOpen(machnetipcProtl, machnetipcProtl,
		    xGetDown(machnetipcProtl, TRANSPORT_DNUM), whom))
	    == ERR_XOBJ) {
	    xTrace0(portmaintp, TR_ERRORS,
		"portmaintp: createListener: could not open lower session");
	    return XK_FAILURE;
	}
    }
    /*
     * for a receive right, we should go into the queue forwarding state;
     * the listener should forward messages until the queue is cleared,
     * then notify the real network receiver that it can begin operation
     */
    args->netport = netport_ptr;
    args->validity = TRUE;
    args->ask_for_dead_name = 1;
    semInit(&args->sem_blocked, 0);

    /*
     * can mark this invalid if remote machine reboots, deallocates, etc.
     */
    netport_ptr->current_read_desc = args;
    if (rights & MACH_PORT_TYPE_SEND_ONCE) {
	/*
	 * replies can start an immediate read
	 */
	mach_port_t		previous_port = MACH_PORT_NULL;
	mach_port_mscount_t	sync = 0;

	/*
	 * set up the xkernel message and get a pointer to the buffer
	 * we expect the receive thread to do msgDestroy()
	 */
	msgConstructAllocate(&args->request_msg, 
	    XK_BASIC_MACH_MSG_MAX, (void *)&args->msg);
	mach_port_request_notification(
	    mach_task_self(),
	    args->port,
	    MACH_NOTIFY_DEAD_NAME,
	    sync,
	    STATEPTR->notification_port,
	    MACH_MSG_TYPE_MAKE_SEND_ONCE,
	    &previous_port);
	args->reply_msg = reply_msg;
	args->deallocate = TRUE;
	/*
	 * immediately begin receive
	 */
	xAsyncThread(
	    (cthread_fn_t)machr_msg_receive,
	    args, "machr msg receive thread");
    } else {  /* either incoming send or outgoing receive */
	args->ask_for_nms = 1;
	args->deallocate = FALSE;
	/*
	 * this will loop,
	 * allocating message storage and waiting for a read signal
	 */
	CreateProcess((gen_func)machnetipc_start_msg_receive,
	    STD_PRIO, 1, (void *)args);
    }
    xTrace0(portmaintp, TR_FULL_TRACE, "portmaintp: createListener: exits");
    return XK_SUCCESS;
}

/*
 *  complete_port_transfer
 *
 *      finish creation of an incoming right from the port transfer protocol;
 *      a port descriptor was created earlier; now start a listening
 *      thread, get notifications, etc.
 */
static void
complete_port_transfer(
	mnetport	*netport,
	Msg		*reply_msg)
{
    struct send_request	*args = netport->current_read_desc;

    xTrace0(portmaintp, TR_EVENTS, "portmaint: complete_port_transfer");

    if (netport->net_port_type != MN_WAITING) {
	xTrace0(portmaintp, TR_EVENTS,
	    "portmaint: complete_port_transfer: port not waiting");
	return;
    }
    args->port = netport->real_receive_port;
    args->validity = TRUE;

    if (netport->net_port_rights & MACH_PORT_TYPE_SEND_ONCE) {
	/*
	 * replies can start an immediate read
	 */
	/*
	 * set up the xkernel message and get a pointer to the buffer
	 * we expect the receive thread to do msgDestroy()
	 */
	msgConstructAllocate(&args->request_msg, 
	    XK_BASIC_MACH_MSG_MAX, (void *)&args->msg);
	portm_req_dn_notif(args->port);
	args->reply_msg = reply_msg;
	xAsyncThread(
	    (cthread_fn_t)machr_msg_receive,
	    args, "machr msg receive thread");
    } else {    /* send right */
	args->ask_for_nms = 1;
	/* this will loop,
	 * allocating message storage and waiting for a read signal
	 */
	CreateProcess((gen_func)machnetipc_start_msg_receive,
	    STD_PRIO, 1, args);
    }
    netport->net_port_type = MN_VALID;
}


/*
 * convert_netport_to_tmp_mach_port
 *
 *   for rights received via port transfer protocols;
 *   local bookkeeping will be completed when a message
 *   containing the port right arrives and convert_netport_to_mach_port
 *   is called
 */
xkern_return_t
convert_netport_to_tmp_mach_port(
	mn_netport_t	*netport,
	unsigned int	rtype,
	IPhost		rec_addr,
	msg_id_t	msgid)
{
    mnetport		*netport_ptr;

    xTrace0(portmaintp, TR_FULL_TRACE,
	"portmaint: enter convert_netport_to_tmp_mach_port");
    xTrace3(portmaintp, TR_FULL_TRACE,
	"portmaint: tmp convert p# %x right %x %x",
	netport->net_port_number, rtype, netport->net_port_rights);
    xTrace4(portmaintp, TR_FULL_TRACE,
	"portmaint: convert from receiver host %d.%d.%d.%d",
	rec_addr.a, rec_addr.b, rec_addr.c, rec_addr.d);

    /*
     * create all structure needed to implement new port
     * but don't start any listening threads yet
     */
    netport_ptr = TYPE_MALLOC(mnetport);
    bzero((char *)netport_ptr, sizeof(mnetport));
    /*
     * copy the data from the net into malloc'd storage
     */
    netport_ptr->lower_session = ERR_XOBJ;
    netport_ptr->net_port_number = netport->net_port_number;
    netport_ptr->net_port_rights = netport->net_port_rights;
    netport_ptr->receiver_host_addr = netport->receiver_host_addr;
    netport_ptr->msgid = msgid;
    netport_ptr->net_port_rights = rtype;
    netport_ptr->net_port_type = MN_WAITING;
    semInit(&netport_ptr->listen_sem, 1);

    if (allocLocalRight(netport_ptr, rtype) == XK_FAILURE) {
	xFree ((char *)netport_ptr);
	return XK_FAILURE;
    }
    mach_port_type(mach_task_self(),
	netport_ptr->real_receive_port,
	&(netport_ptr->net_port_local_rights));
    rwLockInit(&netport_ptr->rwlock);
    netport_ptr->rwlock_ptr = &netport_ptr->rwlock;

    xTrace4(portmaintp, TR_MORE_EVENTS,
	"convert_netport_to_tmp binding %x to structure at %x with local rights %x; receive port = %x",
	netport->net_port_number, netport_ptr,
	netport_ptr->net_port_local_rights, netport_ptr->real_receive_port);

    if (mapBind(STATEPTR->mapnetdesc, &netport->net_port_number, netport_ptr)
	== ERR_BIND) {
	xTrace1(portmaintp,TR_ERRORS,
	    "portmaintp: convert_netport_tmp error binding net number %x",
	    netport->net_port_number);
	return XK_FAILURE;
    }

    /* 
     *  Incoming right is a send right
     */
    if (rtype == MACH_PORT_TYPE_SEND || rtype == MACH_PORT_TYPE_SEND_ONCE) {
	struct send_request	*args = TYPE_MALLOC(struct send_request);

	bzero((char *)args, sizeof(struct send_request));
	args->self = args->lower_session = ERR_XOBJ;
	args->netport = netport_ptr;
	args->validity = FALSE;
	args->ask_for_dead_name = 1;
	semInit(&args->sem_blocked, 0);

	netport_ptr->current_read_desc = args;
	return XK_SUCCESS;
    }

    /* 
     *  Incoming right is a receive right
     */
    if (rtype & MACH_PORT_TYPE_RECEIVE) {
	/*
	 * transfer receive port to the local task via machipc;
	 * just need to record that we are the receiver
	 *
	 * eventually this will get the sender list as well
	 */
	xTrace0(portmaintp, TR_FUNCTIONAL_TRACE,
	    "portmaintp: convert_netport_tmp got receive right");
	if (mapBind(STATEPTR->mapnetdesc,
		&(netport->net_port_number), netport_ptr)
	    != ERR_BIND) {
	    mach_port_mscount_t	sync = 0;
	    mach_port_t		previous_port = MACH_PORT_NULL;

	    netport_ptr->receiver_host_addr = STATEPTR->local_source_addr;
	    netport_ptr->senders_map =
		mapCreate(INITIAL_MAX_MNIPC_SENDERS, sizeof(IPhost));
	    netport_ptr->sender_count = 0;
	    xTrace0(portmaintp, TR_MORE_EVENTS,
		"portmaintp: convert_netport_tmp returning bound new port");
	    mach_port_request_notification(mach_task_self(),
		netport_ptr->real_receive_port,
		MACH_NOTIFY_DEAD_NAME,
		sync,
		STATEPTR->notification_port,
		MACH_MSG_TYPE_MAKE_SEND_ONCE,
		&previous_port);

	    return XK_SUCCESS;
	} else {
	    xTrace0(portmaintp, TR_ERRORS,
		"portmaintp: convert_tmp_netport could not bind addr");
	    xFree ((void *)netport_ptr);
	    return XK_FAILURE;
	}
    }

    /*
     * Never did figure out what this thing was ...
     */
    xTrace0(portmaintp, TR_ERRORS, "convert_netport unknown error");
    return XK_FAILURE;
}

/*
 * quick_port_convert
 *
 *  Convert a local machipc port to a pointer to a network descriptor
 */
mnetport *
quick_port_convert(
	mach_port_t	*local_port)
{
    mnetport	*nport = (mnetport *)NULL;

    xTrace0(portmaintp, TR_FULL_TRACE, "portm: quick_port_convert called");
    if (mapResolve(STATEPTR->maplocalports, local_port, &nport) == XK_SUCCESS) {
	return nport;
    } else {
	xTrace1(portmaintp, TR_ERRORS,
	    "portm: quick_port_convert error on %x", *local_port);
	return &Null_Netport;
    }
}

/*
 * quick_netport_lookup
 *
 *  Convert a port id number to a pointer to a network descriptor
 */
xkern_return_t
quick_netport_lookup(
	mnportid_t	portid,
	mnetport	**portdesc)
{
    xTrace0(portmaintp, TR_FULL_TRACE, "portm: quick_netport_lookup called");
    if (mapResolve(STATEPTR->mapnetdesc, &portid, portdesc) == XK_SUCCESS) {
	return XK_SUCCESS;
    } else {
	xTrace1(portmaintp, TR_ERRORS,
	    "portm: quick_netport_lookup error on %x", portid);
	return XK_FAILURE;
    }
}

/*
 * portm_quick_so_lookup()
 */
mach_port_t
portm_quick_so_lookup(
	mnportid_t	portid)
{
    mach_port_t		mport;

    if (mapResolve(STATEPTR->mapsendoncenumbers, &portid, &mport)
	== XK_SUCCESS){
	return mport;
    }
    return MACH_PORT_NULL;
}

/*
 *  port_mgmt
 *
 *    handles network housekeeping messages: nomoresenders, portdeath
 *
 */
void
port_mgmt(
	mn_arch_tag_t	arch,
	Msg		*msg)
{
    struct port_mgmt_msg	local_data;
    mnetport			*portdesc;
    int				current_mkscount;

    xTrace0(portmaintp, TR_FULL_TRACE, "portm: port_mgmt called");
    xTrace1(portmaintp, TR_DETAILED, "portm: port_mgmt msgLen %d", msgLen(msg));
    if (msgPop(msg, (MLoadFun)portm_unbundle,
	    &local_data, PORTMGMTMSGSIZE, (void *)arch)
	== FALSE) {
	xTrace1(portmaintp, TR_ERRORS,
	    "portm: port_mgmt incoming message too short %d", msgLen(msg));
	return;
    }
    xTrace2(portmaintp, TR_EVENTS,
	"portm: port_mgmt operation %d on port %x",
	local_data.type, local_data.net_port_number);
    if (quick_netport_lookup(local_data.net_port_number, &portdesc)
	== XK_FAILURE) {
	xTrace1(portmaintp, TR_ERRORS,
	    "portmaintp: port_mgmt message for non-existent port %x",
	    local_data.net_port_number);
	return;
    }
    if (local_data.type == XKPM_NOMORESENDERS) {
	if (mapResolve(portdesc->senders_map,
		&local_data.sender, &current_mkscount)
	    == XK_FAILURE) {
	    xTrace4(portmaintp, TR_SOFT_ERRORS,
		"portmaintp: port_mgmt no more senders from non-sender host %d.%d.%d.%d",
		local_data.sender.a, local_data.sender.b,
		local_data.sender.c, local_data.sender.d);
	    return;
	}
	xTrace2(portmaintp, TR_DETAILED,
	    "portmaint: port_mgmt: make send counts %d %d",
	    current_mkscount, local_data.make_send_count);
	/*
	 *  if (current_mkscount >= local_data.make_send_count) {
	 */
	xTrace1(portmaintp, TR_DETAILED,
	    "portmaint: port_mgmt: sender count %d",
	    portdesc->sender_count);
	mapUnbind(portdesc->senders_map, &local_data.sender);
	/*
	 *  if ((mach_port_mod_refs(mach_task_self(),
	 * 		portdesc->real_send_port, MACH_PORT_RIGHT_SEND, -1))
	 * 	!= KERN_SUCCESS) {
	 * 	xTrace2(portmaintp, TR_SOFT_ERRORS,
	 * 	    "portmaint: port_mgmt: could not decrement port ref count on send port %d (x%x)",
	 * 	    portdesc->real_send_port, portdesc->real_send_port);
	 * 	return;
	 *  }
	 */
	if (!--portdesc->sender_count) {
	    port_kill(portdesc);  /* in particular, destroy our send rights */
	    return;
	}
	/*
	 *  }
	 */
    }
    if (local_data.type == XKPM_PORTDEATH) {
	if (IPEQUAL(portdesc->receiver_host_addr, local_data.sender)) {
	    port_kill(portdesc);
	} else {
	    xTrace0(portmaintp, TR_SOFT_ERRORS,
		"portmaintp: port_mgmt: port death notice from non-receiver host");
	}
	return;
    }
    xTrace1(portmaintp, TR_ERRORS,
	"portmaintp: port_mgmt unknown message type %d",
	local_data.type);
}

/*
 *  null_free routine
 */
static void
null_free(
	void	*addr,
	int	size)
{
    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: null free routine");
}

/*
 *   make_portm_message
 *
 *	construct the network form of a port management message
 */
static void
make_portm_message(
	char			*msgstream,
	mnetport		*port,
	enum PORTMAINTTYPE	type,
	mach_port_mscount_t	msc)
{
    int		am = MN_ARCH_MARKER;
    int		mtype = PORT_MGMT_MSG;
    char	ptype = type;

    xTrace3(portmaintp, TR_FULL_TRACE,
	"portmaint: make_portm_message: port %x, type %d, makesendcount %d",
	port, type, msc);
    bcopy((char *)&am, msgstream, MN_ARCH_TAG_NETLEN);
    bcopy((char *)&mtype, msgstream += MN_ARCH_TAG_NETLEN, MACHIPCTYPE_NETLEN);
    bcopy((char *)&port->net_port_number,
	msgstream += MACHIPCTYPE_NETLEN, PORTID_NETLEN);
    bcopy(&ptype, msgstream += PORTID_NETLEN, PORT_MGMT_TYPELEN);
    bcopy((char *)&(STATEPTR->local_source_addr),
	msgstream += PORT_MGMT_TYPELEN, HOSTNETLEN);
    bcopy((char *)&msc, msgstream + HOSTNETLEN, NETMAKESENDSIZE);
}

/*
 *   portm_unbundle
 *
 *	construct the local form of a port management network message
 */
static long
portm_unbundle(
	char	*to,
	char	*from,
	long	len,
	void	*arg)
{
    char			*msgstream = (char *)from;
    struct port_mgmt_msg	*portstruct = (struct port_mgmt_msg *)to;
    mn_arch_tag_t		arch_type = (mn_arch_tag_t)arg;
/*
    Pfi				convec[XK_DATA_TYPE_MAX];
*/
    int	(*convec[XK_DATA_TYPE_MAX])(any_t, ...)		;

    set_convert_vector(arch_type, (convert_func_t *)convec);

    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: portm_unbundle called");
    /*
     *  msgstream += MN_ARCH_TAG_NETLEN + MACHIPCTYPE_NETLEN;
     */
    if (arch_type == MN_ARCH_MARKER) {
	bcopy(msgstream, (char *)&portstruct->net_port_number, PORTID_NETLEN);
	portstruct->type = *(msgstream += PORTID_NETLEN);
	bcopy(msgstream += PORT_MGMT_TYPELEN,
	    (char *)&portstruct->sender, HOSTNETLEN);
	bcopy(msgstream + HOSTNETLEN,
	    (char *)&portstruct->make_send_count, NETMAKESENDSIZE);
    } else {
	bcopy(msgstream, (char *)&portstruct->net_port_number, PORTID_NETLEN);
	portstruct->net_port_number =
	    (int)convec[PORTID_NETTYPE]((char *)&portstruct->net_port_number);
	portstruct->type = *(msgstream += PORTID_NETLEN);
	bcopy(msgstream += PORT_MGMT_TYPELEN,
	    (char *)&portstruct->sender, HOSTNETLEN);
	bcopy(msgstream + HOSTNETLEN,
	    (char *)&portstruct->make_send_count, NETMAKESENDSIZE);
	portstruct->make_send_count =
	    (int)convec[NETMAKESENDTYPE]((char *)&portstruct->make_send_count);
    }
    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: portm_unbundle exits");
    return len;
}

/*
 * portm_generate_nms
 *
 *   local no more sender notification -> network nms
 *
 *   runs in the context of machnetipc
 */
void
portm_generate_nms(
	mnetport	*portd)
{
    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: portm_generate_nms called");

    /*
     *  if ( !( portd->net_port_rights & MACH_PORT_TYPE_RECEIVE ))
     */
    if ( portd->net_port_rights & MACH_PORT_TYPE_SEND ) {
	if (portd->net_port_type != MN_FORWARDING) {
	    if (!IPEQUAL(portd->receiver_host_addr,
		    STATEPTR->local_source_addr)) {
		/*
		 * this node is sender, not receiver
		 */
		Part		whom[1];
		xkern_return_t	ret;
		XObj		ls;

		partInit(whom, 1);
		partPush(whom[REMOTE_PART],
		    &portd->receiver_host_addr, sizeof(IPhost));
		ls = xOpen(machnetipcProtl, machnetipcProtl,
			xGetDown(machnetipcProtl, TRANSPORT_DNUM), whom);

		/*
		 *  if ((ls = portd->lower_session) == ERR_XOBJ)
		 */
		if (ls == ERR_XOBJ) {
		    xTrace0(portmaintp, TR_ERRORS,
			"portmaint: portm_generate_nms: cannot open transport session");
		} else {
		    Msg		msg, rmsg;
		    char	msgdata[PORTMGMTMSGSIZE +
				        MN_ARCH_TAG_NETLEN +
				        MACHIPCTYPE_NETLEN];

		    make_portm_message(msgdata,
			portd, XKPM_NOMORESENDERS, portd->make_send_count);
		    msgConstructEmpty(&rmsg);
		    msgConstructInplace(&msg,
			msgdata, sizeof(msgdata), (MsgCIPFreeFunc)null_free);
		    xTrace0(portmaintp, TR_EVENTS,
			"portmaint: portm_generate_nms: will send nms note");
		    if ((ret = xCall(ls, &msg, &rmsg)) != XK_SUCCESS) {
			xTrace0(portmaintp, TR_ERRORS,
			    "portmaint: portm_generate_nms: cannot send message");
		    }
		    msgDestroy(&msg);
		    xClose(ls);
		    portd->lower_session = ERR_XOBJ;
		}
	    }
	    port_kill(portd);
	} else {
	    portd->net_port_type = MN_INVALID;  /* ?? */
	}
    }
    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: portm_generate_nms exits");
}

/*
 * portm_port_remove_send_once
 *
 *  Remove a port right from the database
 */
xkern_return_t
portm_port_remove_send_once(
	mnetport	*netport_ptr)
{
    xTrace2(portmaintp, TR_FULL_TRACE,
	"portmaint: portm_port_remove_send_once desc addr %x id %x",
	netport_ptr, netport_ptr ? netport_ptr->net_port_number : 0);

    if (mapUnbind(STATEPTR->mapnetdesc, &netport_ptr->net_port_number)
	!= XK_SUCCESS) {
	/*
	 * already removed by an incoming message using the same port
	 */
	xTrace1(portmaintp, TR_EVENTS,
	    "portmaint: portm_port_remove_send_once netport %x has already been removed from maps",
	    netport_ptr->net_port_number);
	return XK_SUCCESS;
    } else {
	/*
	 * this machine is the receiver;
	 * have less interest in whoever we gave the send_once right to
	 */
    }

    if (netport_ptr->real_send_port != MACH_PORT_NULL) {
	xTrace1(portmaintp, TR_DETAILED,
	    "portmaintp: remove_send_once: unbinding send_port %x",
	    netport_ptr->real_send_port);
	mapUnbind(STATEPTR->maplocalports, &netport_ptr->real_send_port);
	/*
	 * this machine is the receiver;
	 * have less interest in whoever we gave the send_once right to
	 */
    }
    if (netport_ptr->real_receive_port != MACH_PORT_NULL) {
	xTrace1(portmaintp, TR_DETAILED,
	    "portmaintp: remove_send_once: unbinding receive_port %x",
	    netport_ptr->real_receive_port);
	mapUnbind(STATEPTR->maplocalports, &netport_ptr->real_receive_port);
	if (mapUnbind(STATEPTR->mapsendonceports,
		&netport_ptr->real_receive_port)
	    == XK_SUCCESS) {
	    portm_use_fast_send_once(netport_ptr->real_receive_port, TRUE);
	} else {
	    mach_port_deallocate(mach_task_self(), netport_ptr->real_receive_port);
	}
	xFree((void *)netport_ptr->current_read_desc);  /* the argument block */
	/*
	 * remote machine is the receiver; have less interest that host
	 */
    }
    xTrace1(portmaintp, TR_DETAILED,
	"portmaintp: remove_send_once: unbinding netport id %x",
	netport_ptr->net_port_number);
    portm_port_free(NULL, netport_ptr);
    return XK_SUCCESS;
}

/*
 * portm_port_remove_unused
 *
 *  Remove a port descriptor from the database
 */
xkern_return_t
portm_port_remove_unused(
	mach_port_t	*local_port,
	mnetport	*netport_ptr,
	IPhost		*recvr_ipaddr)
{
    Part	part[1];

    xTrace1(portmaintp, TR_EVENTS,
	"portm_port_remove_unused: removing id %x",
	netport_ptr->net_port_number);
    mapUnbind(STATEPTR->maplocalports, local_port);
    mapUnbind(STATEPTR->mapnetdesc, netport_ptr->net_port_number);

#define IPPTREQUAL(_h, _g)				\
	    ((_h->a == _g.a) && (_h->b == _g.b) &&	\
	     (_h->c == _g.c) && (_h->d == _g.d))

    if (IPPTREQUAL(recvr_ipaddr, (STATEPTR->local_source_addr))) {
	return XK_SUCCESS;
    }

    partInit(part, 1);
    partPush(part[0], recvr_ipaddr, sizeof(IPhost));

    xOpenDisable(myProtl, myProtl, BootIdProtl, part);
    xTrace0(portmaintp, TR_FULL_TRACE, "portm: port_remove called");

    return XK_SUCCESS;
}

/*
 *   portm_add_sender
 *
 *     existing port gets new sender; currently called only
 *     for reply ports ... port register unnecessary.
 */
void
portm_add_sender(
	mnetport	*portd,
	IPhost		host)
{
    xTrace1(portmaintp, TR_FULL_TRACE,
	"portm_add_sender called for port %x", portd->net_port_number);

    if (portd->sender_count > 0 &&
	mapResolve(portd->senders_map, &host, 0) == XK_SUCCESS) {
	mapUnbind(portd->senders_map, &host);
	mapBind(portd->senders_map, &host, ++portd->make_send_count);
	return;
    }
    mapBind(portd->senders_map, &host, ++portd->make_send_count);
    portd->sender_count++;
}

/*
 * portm_register
 *
 *     note concern for health of remote host
 */
xkern_return_t
portm_register(
	IPhost	ipaddr)
{
    /*
     * register our interest in this host with the bootid protocol
     */
    Part		part[1];
    xkern_return_t	ret = XK_SUCCESS;
    int			count;

    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: portm_register called");
    if (mapResolve(STATEPTR->mapremotehosts, &ipaddr, (void *)&count)
	!= XK_FAILURE) {
	xTrace4(portmaintp, TR_EVENTS,
	    "portmaint: portm_register found addr %d.%d.%d.%d",
	    ipaddr.a, ipaddr.b, ipaddr.c, ipaddr.d);
	mapUnbind(STATEPTR->mapremotehosts, &ipaddr);
	mapBind(STATEPTR->mapremotehosts, &ipaddr, ++count);
	return XK_SUCCESS;
    }

    mapBind(STATEPTR->mapremotehosts, &ipaddr, TRUE);
    xTrace4(portmaintp, TR_EVENTS,
	"portmaint: portm_register addr %d.%d.%d.%d",
	ipaddr.a, ipaddr.b, ipaddr.c, ipaddr.d);
    partInit(part, 1);
    partPush(part[0], &ipaddr, sizeof(IPhost));
    if ((ret = pass_open_bootid(part)) != XK_SUCCESS) {
	xTrace4(portmaintp, TR_ERRORS,
	    "portmaintp openenable failed for addr %d.%d.%d.%d",
	    ipaddr.a, ipaddr.b, ipaddr.c, ipaddr.d);
    }
    xTrace0(portmaintp, TR_FULL_TRACE,
	"portmaint: portm_register returns from register");
    return(ret);
}

/*
 * portm_deregister
 *
 *     note lack of concern for health of remote host
 */
static void
portm_deregister(
	IPhost	ipaddr)
{
    /*
     * register our interest in this host with the bootid protocol
     */
    Part		part[1];
    xkern_return_t	ret = XK_SUCCESS;

    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: portm_deregister called");
    xTrace4(portmaintp, TR_EVENTS,
	"portmaint: portm_deregister addr %d.%d.%d.%d",
	ipaddr.a, ipaddr.b, ipaddr.c, ipaddr.d);
    partInit(part, 1);
    partPush(part[0], &ipaddr, sizeof(IPhost));
    if ((ret = pass_opendisable_bootid(part)) != XK_SUCCESS) {
	xTrace4(portmaintp, TR_ERRORS,
	    "portmaintp opendisenable failed for addr %d.%d.%d.%d",
	    ipaddr.a, ipaddr.b, ipaddr.c, ipaddr.d);
    }
    xTrace0(portmaintp, TR_FULL_TRACE,
	"portmaint: portm_deregister returns from register");
}

/*
 * portm_req_dn_notif
 *
 *  request a dead name notification for the port
 *  done here for convenience, but machnetip gets the notification
 */
static void
portm_req_dn_notif(
	mach_port_t	port)
{
    mach_port_t		previous_port;
    kern_return_t	kret;

    xTrace1(portmaintp, TR_EVENTS,
	"portmaint: portm_req_dn_notif: port %x", port);
    kret =
	mach_port_request_notification(
	    mach_task_self(),
	    port,
	    MACH_NOTIFY_DEAD_NAME,
	    0,
	    ((struct machr_state *)(machnetipcProtl->state))->notification_port,
	    MACH_MSG_TYPE_MAKE_SEND_ONCE,
	    &previous_port);
    if (kret != KERN_SUCCESS) {
	xTrace1(portmaintp, TR_ERRORS,
	    "portmaint: machr_req_dn_notif: failed %x", kret);
    }
}

/*
 * portm_register_via_session
 */
xkern_return_t
portm_register_via_session(
	XObj	lower_session)
{
    /*
     * register our interest in this host with the bootid protocol
     */
    IPhost	remote_addr;

    xTrace0(portmaintp, TR_FULL_TRACE,
	"portmaint: portm_register_via_session called");
    xControl(lower_session, GETPEERHOST, (char *)&remote_addr, sizeof(IPhost));
    return(portm_register(remote_addr));
}

#ifdef PORTLOCKS
/*
 * portm_dummy
 *
 *
 */
static void
portm_dummy(
	ReadWriteLock	*rwlock,
	void		*dummy)
{
    xTrace0(portmaintp, TR_FULL_TRACE, "port_maint: portm_dummy");
}
#endif /* PORTLOCKS */


/*
 *  portm_port_free
 */
static void
portm_port_free(
	ReadWriteLock	*rwlock,
	mnetport	*portdesc)
{
    xTrace1(portmaintp, TR_EVENTS, "port_maint: portm_port_free %x", portdesc);

    if (portdesc->senders_map) mapClose(portdesc->senders_map);
    if (portdesc->session_map) mapClose(portdesc->session_map);
    bzero((char *)portdesc, sizeof(mnetport));
    xFree((char *)portdesc);
}

/*
 *  port_kill
 *		
 *  	release resources held by port
 *  	marks port invalid, locks port, destroys all
 */
void
port_kill(
	mnetport	*portdesc)
{
    xkern_return_t	ret;
    mnportid_t		port_number = portdesc->net_port_number;
    mnetport		*npd;
    kern_return_t	retcode;

    xTrace1(portmaintp, TR_EVENTS, "port_maint: port_kill %x", portdesc);

    xIfTrace(portmaintp, TR_EVENTS) {
	IPhost	recv_host;

	recv_host = portdesc->receiver_host_addr;
	xTrace4(portmaintp, TR_FUNCTIONAL_TRACE,
	    "port_maint: port_kill for host %d.%d.%d.%d",
	    recv_host.a, recv_host.b, recv_host.c, recv_host.d);
    }
    if (portdesc->net_port_type == MN_INVALID) {
	return;
    }

    if (portdesc->current_read_desc) {
	portdesc->current_read_desc->deallocate = 1;
	portdesc->current_read_desc->validity = FALSE;
    }
    portdesc->net_port_type = MN_INVALID;
    if (portdesc->current_read_desc) {
	portdesc->current_read_desc->validity = FALSE;
    }

#ifdef PORTLOCKS
    rwLockDestroy(portdesc->rwlock_ptr, portm_dummy, (void *)0);
    /*
     * rwLockDestroy(portdesc->rwlock_ptr, portm_port_free, portdesc);
     */
#endif	/* PORTLOCKS */

    if (mapResolve(STATEPTR->mapnetdesc, &port_number, &npd) == XK_FAILURE) {
	xTrace0(portmaintp, TR_EVENTS,
	    "portmaint: port_kill: port died during lock request");
	return;
    }
    if (npd != portdesc) {
	xTrace0(portmaintp, TR_EVENTS,
	    "portmaint: port_kill: port reincarnated during lock request");
	portdesc = npd;
    }

    xTrace1(portmaintp, TR_EVENTS,
	"portmaint: port_kill: will unbind netport id %x",
	portdesc->net_port_number);
    if ((ret=mapUnbind(STATEPTR->mapnetdesc, &portdesc->net_port_number))
	!= XK_SUCCESS) {
	xTrace1(portmaintp, TR_ERRORS,
	    "portmaint: cannot unbind portdesc, error code %x", ret);
    }
    xTrace0(portmaintp, TR_ERRORS, "will unbind local port");
    if (portdesc->real_send_port != MACH_PORT_NULL) {
	if ((ret=mapUnbind(STATEPTR->maplocalports, &portdesc->real_send_port))
	    != XK_SUCCESS) {
	    xTrace2(portmaintp, TR_ERRORS,
		"port maint: cannot unbind local send port %x error code %x",
		portdesc->real_send_port, ret);
	}
	xTrace0(portmaintp, TR_ERRORS, "will deallocate local port");
	xTrace1(portmaintp, TR_ERRORS,
	    "port maint: deallocating send right %x", portdesc->real_send_port);
	retcode = mach_port_mod_refs(mach_task_self(),
	    portdesc->real_send_port, MACH_PORT_RIGHT_SEND, -1);
	if (retcode != KERN_SUCCESS) {
	    retcode = mach_port_mod_refs(mach_task_self(),
		portdesc->real_send_port, MACH_PORT_RIGHT_SEND_ONCE, -1);
	    if (retcode != KERN_SUCCESS) {
		xTrace3(portmaintp, TR_ERRORS,
		    "port maint: failed to destroy sendonce right %x ret = %d dec (%x)",
		    portdesc->real_send_port, retcode, retcode);
	    }
	}
    }
    if (portdesc->real_receive_port != MACH_PORT_NULL) {
	if ((ret = mapUnbind(STATEPTR->maplocalports,
		    &portdesc->real_receive_port))
	    != XK_SUCCESS) {
	    xTrace2(portmaintp, TR_ERRORS,
		"port maint: cannot unbind local receive port %x error code %x",
		portdesc->real_receive_port, ret);
	}
	xTrace1(portmaintp, TR_ERRORS,
	    "port maint: deallocating receive right %x",
	    portdesc->real_receive_port);
	retcode = mach_port_destroy(
	    mach_task_self(), portdesc->real_receive_port);
	if (retcode != KERN_SUCCESS) {
	    xTrace3(portmaintp, TR_ERRORS,
		"port maint: failed to destroy receive right %x ret = %d dec (%x)",
		portdesc->real_receive_port, retcode, retcode);
	}
    }
    portm_port_free(NULL, portdesc);
}

/*
 *    port_kill_mapfun
 *			called to releases resources held on behalf of
 *			rebooted host.
 */
static int
port_kill_mapfun(
	char		*key,
	mnetport	*portdesc,
	IPhost		*rebooted_host)
{
    xTrace1(portmaintp, TR_EVENTS,
	"portmaintp: port_kill_mapfun called for portdesc ptr %x", portdesc);

    if (IPEQUAL(portdesc->receiver_host_addr, (*rebooted_host))) {
	/*
	 * need to get writer lock before destroying structure
	 */
	port_kill(portdesc);
	return (MFE_REMOVE | MFE_CONTINUE);
    } else if (portdesc->senders_map) {
	if (mapResolve(portdesc->senders_map, rebooted_host, 0) == XK_SUCCESS) {
	    mapUnbind(portdesc->senders_map, rebooted_host);
	    if (!--portdesc->sender_count) {
		port_kill(portdesc);
	    }
	}
    }
    return MFE_CONTINUE;
}

/*
 *    port_kill_so_mapfun
 *			
 *			get rid of fast send-once rights to or from
 *			rebooted host
 */
static int
port_kill_so_mapfun(
	char	*key,
	IPhost	receiver_host_addr,
	IPhost	*rebooted_host)
{
    mach_port_t		port = (mach_port_t) key;
    kern_return_t	kret;

    xTrace1(portmaintp, TR_EVENTS,
	"portmaintp: port_kill_so_mapfun called for port %x", port);

    if (IPEQUAL(receiver_host_addr, (*rebooted_host))) {
	if ((kret = mach_port_deallocate(mach_task_self(), port))
	    != KERN_SUCCESS) {
	    xTrace1(portmaintp, TR_ERRORS,
		"portmaintp:  port_kill_so_mapfun: port %x deallocation fails",
		port);
	}
	return MFE_REMOVE | MFE_CONTINUE;
    }
    return MFE_CONTINUE;
}

/*
 * deallocate_host_ports --- destroy all port rights associated
 *  with the rebooted host.  Also, abort all sessions in progress
 *  and generate dead name notifications.
 *
 * This runs in the same thread in which the xControl message
 *  arrives, and that is the same thread in which the reboot
 *  message was received by boot_ctl.  This gives us a chance
 *  to clean up without incurring timing problems related to
 *  thread scheduling.
 */
static void
deallocate_host_ports(
	IPhost	rebooted_host)
{
    Map		map = STATEPTR->mapnetdesc;

    xTrace0(portmaintp, TR_MAJOR_EVENTS, "deallocate_host_ports called");
    mapForEach(map, (MapForEachFun)port_kill_mapfun, (void *)&rebooted_host);
    map = STATEPTR->mapsendonceports;
    mapForEach(map, (MapForEachFun)port_kill_so_mapfun, (void *)&rebooted_host);
}

/*
 * portm_control  --- expects reboot messages from bootctl.
 */
static int
portm_control(
	XObj	self,
	int	opcode,
	char	*buf,
	int	len)
{
    BidctlBootMsg	*reboot_msg = (BidctlBootMsg *)buf;

    xTrace2(portmaintp, TR_FULL_TRACE,
	"portm_control called, opcode %d %d", opcode, opcode % MAXOPS);
    xAssert(xIsProtocol(self));
    xAssert(len >= sizeof(BidctlBootMsg));
    if (opcode >= BIDCTL_CTL * MAXOPS && opcode < (BIDCTL_CTL + 1) * MAXOPS) {
	switch (opcode) {
	    case BIDCTL_PEER_REBOOTED:
		xTrace4(portmaintp, TR_ERRORS,
		    "portmaintp: Peer %d.%d.%d.%d rebooted",
		    reboot_msg->h.a, reboot_msg->h.b,
		    reboot_msg->h.c, reboot_msg->h.d);
		deallocate_host_ports(reboot_msg->h);
		return(XK_SUCCESS);
		break;
	    default:
		xTrace1(portmaintp, TR_SOFT_ERROR,
		    "portmaintp: ignoring bootid msg %d", opcode);
		return(XK_SUCCESS);
	}
    } else {
	xTrace0(portmaintp, TR_SOFT_ERROR, "portmaintp: fowarding control op");
	return xControl(BootIdProtlObj, opcode, buf, len);
    }
    return(XK_FAILURE);
}

/*
 *  portm_close_session()
 *
 *	release lower sessions that we were holding for netrcv
 *	  called from mapForEach
 */
static int
portm_close_session(
	void	*key,
	int	mscount,
	void	*arg)
{
    XObj	session = (XObj)key;

    xTrace1(portmaintp, TR_ERRORS,
	"portmaint: portm_close_session called for %x", session);

    xClose(session);
    return MFE_CONTINUE;
}

/*
 *  portm_send_port_death()
 *
 *	deregister bootid interest in senders
 *	  called from mapForEach
 */
static int
portm_send_port_death(
	void	*key,
	int	mscount,
	void	*arg)
{
    IPhost	sender = *((IPhost *)key);
    Msg		msg, rmsg;
    mnetport	*portd = (mnetport *)arg;
    char	msgdata[PORTMGMTMSGSIZE+MN_ARCH_TAG_NETLEN+MACHIPCTYPE_NETLEN];
    Part	whom[1];
    XObj	ls;
    xkern_return_t xret;
    int		count;

    xTrace4(portmaintp, TR_FULL_TRACE,
	"portmaint: portm_send_port_death: notification to host %d.%d.%d.%d",
	sender.a, sender.b, sender.c, sender.d);
    make_portm_message(msgdata, portd, XKPM_PORTDEATH, 0);
    msgConstructInplace(
	&msg, msgdata, sizeof(msgdata), (MsgCIPFreeFunc)null_free);
    msgConstructEmpty(&rmsg);
    /*
     * should use the lower session, if it exists, to guarantee sequencing
     */
    if (!(portd->current_read_desc && 
	(ls = portd->current_read_desc->lower_session) != ERR_XOBJ)){
	partInit(whom, 1);
	partPush(whom[0], &sender, sizeof(IPhost));
	if ((ls = xOpen(machnetipcProtl, machnetipcProtl,
		    xGetDown(machnetipcProtl, TRANSPORT_DNUM), whom))
	    == ERR_XOBJ) {
	    xTrace0(portmaintp, TR_ERRORS,
		"portmaint: portm_send_port_death: cannot open transport session");
	}
    }
    if (ls != ERR_XOBJ &&
	(xret = xCall(ls, &msg, &rmsg)) != XK_SUCCESS) {
	xTrace4(portmaintp, TR_ERRORS,
	    "portmaint: portm_send_port_death: could not send notification to host %d.%d.%d.%d",
	    sender.a, sender.b, sender.c, sender.d);
	xClose(ls);
    } else {
	xTrace0(portmaintp, TR_EVENTS,
	    "portmaint: portm_send_port_death: sent net notification");
    }
    msgDestroy(&msg);
    if (mapResolve(STATEPTR->mapremotehosts, &sender, &count) == XK_SUCCESS) {
	if (--count <= 0) {
	    xOpenDisable(myProtl, myProtl, BootIdProtl, whom);
	} else {
	    mapUnbind(STATEPTR->mapremotehosts, &sender);
	    mapBind(STATEPTR->mapremotehosts, &sender, count);
	}
    }
    return MFE_CONTINUE;
}

/*
 *  portm_delete_local_receive
 *
 *    the receiving task has died or destroyed the port;
 *    we got a notification, and this routine was called to clean up.
 *    This is a definite kill operation; we should not recreate th
 *    port under any circumstances.
 */
void
portm_delete_local_receive(
	Event		ev,
	mach_port_t	port)
{
    mnetport		*portd;
    mnportid_t		portnum;
    xkern_return_t	xret;

    xTrace1(portmaintp, TR_ERRORS,
	"portm_delete_local_receive called with port %x", port);
    if ((xret = mapResolve(STATEPTR->maplocalports, &port, &portd))==XK_SUCCESS
	&&
	(portd->net_port_type != MN_INVALID)
	&&
	(portd->net_port_rights &
	    (MACH_PORT_TYPE_SEND | MACH_PORT_TYPE_SEND_ONCE))) {
	portd->net_port_type = MN_INVALID;
	/*
	 * don't let anyone else have this port
	 */
	xTrace1(portmaintp, TR_EVENTS,
	    "portmaint: portm_delete_local_send: Unbinding netport id %x",
	    portd->net_port_number);
	mapUnbind(STATEPTR->mapnetdesc, &portd->net_port_number);
	mapUnbind(STATEPTR->maplocalports, &portd->real_send_port);
	mach_port_deallocate(mach_task_self(), port);
	xTrace2(portmaintp, TR_DETAILED,
	    "portmaint: portm_delete_local_send: deallocated port %x real_send_port is %x",
	    port, portd->real_send_port);
	portnum = portd->net_port_number;
	if (writerLock(portd->rwlock_ptr) != XK_SUCCESS) {
	    xTrace0(portmaintp, TR_EVENTS,
		"portmaint: portm_delete_local_receive: lock failure");
	    return;
	}

	/*
	 * But.. port was unbound above...
	 *  if (mapResolve(STATEPTR->mapnetdesc, &portnum, &npd)==XK_FAILURE) {
	 *  	xTrace0(portmaintp, TR_EVENTS,
	 *  	    "portmaint: portm_delete_local_receive: port died during lock request");
	 *  	return;
	 *  }
	 */

	/*
	 * it's ok to block in this because we hold the lock
	 */
	mapForEach(portd->senders_map,
	    (MapForEachFun)portm_send_port_death, portd);
	if (portd->lower_session != ERR_XOBJ) xClose(portd->lower_session);
	mapForEach(portd->session_map, (MapForEachFun)portm_close_session, 0);
	portm_port_free(NULL, portd);
    } else {
	xTrace2(portmaintp, TR_SOFT_ERROR,
	    "portmaintp: portm_delete_local_receive: bogus port %x resolve code %x",
	    port, xret);
    }
}

/*
 *  portm_delete_local_send
 *
 *    the sending task has died or destroyed the port;
 *    we got a notification, and this routine was called to clean up.
 */
void portm_delete_local_send(
	Event		ev,
	mach_port_t	port)
{
    mnetport	*portd;
    int		ret;

    xTrace1(portmaintp, TR_ERRORS,
	"portm_delete_local_send called with port %x", port);
    if (mapResolve(STATEPTR->maplocalports, &port, &portd) == XK_SUCCESS
	&&
	(portd->net_port_type != MN_INVALID)
	&&
	(portd->net_port_rights & MACH_PORT_TYPE_RECEIVE)) {
	portd->net_port_type = MN_INVALID;
	/*
	 * don't let anyone else have this port
	 */
	xTrace1(portmaintp, TR_EVENTS,
	    "portm_delete_local_send: Unbinding netport id %x",
	    portd->net_port_number);
	mapUnbind(STATEPTR->mapnetdesc, &portd->net_port_number);
	mapUnbind(STATEPTR->maplocalports, &portd->real_send_port);
	if (writerLock(portd->rwlock_ptr) != XK_SUCCESS) {
	    xTrace0(portmaintp, TR_EVENTS,
		"portm_delete_local_send failed writer lock");
	    return;
	}
	xTrace0(portmaintp, TR_EVENTS,
	    "portm_delete_local_send got writer lock");
	mach_port_deallocate(mach_task_self(), port);
	xTrace2(portmaintp, TR_DETAILED,
	    "portmaint: portm_delete_local_send: deallocated port %x real_receive_port is %x",
	    port, portd->real_receive_port);
	if (portd->current_read_desc) {
	    portd->current_read_desc->validity = FALSE;
	}
	if (portd->lower_session) {
	    xClose(portd->lower_session);
	}
	ret = portm_send_port_death(
	    (void *)&portd->receiver_host_addr, 0, (void *)portd);
    } else {
	xTrace1(portmaintp, TR_SOFT_ERROR,
	    "portmaintp: portm_delete_local_send: bogus port %x", port);
    }
}

/*
 * portm_get_netnum
 *
 *   quick access to netport_num for send_once rights
 *    held by the client side of an rpc
 */
mnportid_t
portm_get_netnum(
	mach_port_t	port,
	IPhost		sender)
{
    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: portm_get_netnum called");
    mapBind(STATEPTR->mapsendonceports, &port, IP_TO_INT(sender));
    mapBind(STATEPTR->mapsendoncenumbers, &netport_id, port);
    return netport_id++;
}

/*
 * portm_fast_send_once
 *
 *   quick creation of reply port for server side of an rpc
 */
mach_port_t
portm_fast_send_once(
	mnportid_t	nnum,
	IPhost		receiver)
{
    register mach_port_t	*portptr;
    mach_port_t			port;
    kern_return_t		kret;
    register int		i;

    xTrace0(portmaintp, TR_FULL_TRACE,
	"portmaint: portm_fast_send_once called");
    portptr = STATEPTR->sendoncearray;
    for (i=0; i<PORTM_SO_PORT_MAX; i++, portptr++) {
	if (*portptr!=MACH_PORT_NULL) break;
    }
    if (i >= PORTM_SO_PORT_MAX) {
	if ((kret = mach_port_allocate(
		mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port))
	    != KERN_SUCCESS) {
	    xTrace0(portmaintp, TR_ERRORS,
		"portmaintp: portm_fast_send_once: port allocation fails");
	    return MACH_PORT_NULL;
	}
    } else { 
	port = *portptr; 
	*portptr = MACH_PORT_NULL; 
    }
    /*
     * so that reboot code can find it
     */
    xTrace1(portmaintp, TR_EVENTS,
	"portmaint: portm_fast_send_once returning port %x", port);
    mapBind(STATEPTR->mapsendonceports, &port, IP_TO_INT(receiver));
    return port;
}

/*
 * portm_use_fast_send_once
 *
 *   quick destruction of reply port for server side of an rpc
 */
void
portm_use_fast_send_once(
	mach_port_t	port, 
	boolean_t	dealloc)
{
    kern_return_t	kret;
    int			i;
    mach_port_t		*portptr = STATEPTR->sendoncearray;

    xTrace0(portmaintp, TR_FULL_TRACE,
	"portmaint: portm_use_fast_send_once called");
    xTrace1(portmaintp, TR_EVENTS,
	"portmaint: portm_use_fast_send_once freeing port %x", port);
    for (i=0; i<PORTM_SO_PORT_MAX; i++, portptr++) {
	if (!*portptr){ 
	    *portptr = port; 
	    break; 
	}
    }
    if (i >= PORTM_SO_PORT_MAX && dealloc) {
	if ((kret = mach_port_deallocate(mach_task_self(), port))
	    != KERN_SUCCESS) {
	    xTrace0(portmaintp, TR_ERRORS,
		"portmaintp: portm_use_fast_send_once: port deallocation fails");
	}
    }
    mapUnbind(STATEPTR->mapsendonceports, &port);
}

/*
 * portm_use_netnum
 *
 *   quick access to netport_num for send_once rights
 *    held by the client side of an rpc
 */
void
portm_use_netnum(
	mach_port_t	port,
	mnportid_t	port_number)
{
    IPhost		hostaddr;
    xkern_return_t	xret;

    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: portm_use_netnum called");

    xret = mapResolve(STATEPTR->mapsendonceports, &port, &hostaddr);
    mapUnbind(STATEPTR->mapsendonceports, &port);
    mapUnbind(STATEPTR->mapsendoncenumbers, &port_number);
    if (xret == XK_SUCCESS) {
	/*
	 *  portm_deregister(hostaddr);
	 */
    }
}


/*
 *  Initialization utilities
 *
 *     getproc
 */
static void
getproc(
	XObj	p)
{
    xAssert(p->type == Protocol);
    p->control = portm_control;
}


/*
 * xAsyncProcess
 *
 * platform dependent asynchronous thread startup
 * the thread cannot perform any xkernel operations
 */
static void
xAsyncThread(
	cthread_fn_t	func,
	void		*args,
	char		*name)
{
    cthread_t	child;

    child = cthread_fork((cthread_fn_t)func, args);
    cthread_set_name(child, name);
    cthread_detach(child);
    xTrace1(processcreation, TR_ERRORS,
	"portm created client thread_id: %d", child);
}


/* 
 * port_maint_init()
 *
 * establish communication with the first lower transport protocol.
 *
 * This is called from machnetipc, although it
 * keeps its own XObj with its state, etc.  Someday it will
 * really be a separate protocol (maybe).
 */
xkern_return_t
portmaint_init(
	XObj	self)
{
    IPhost	ipaddr, nametoip();
    Part	whom[1];
    mach_port_t	notification_port;
    int		i;
    mach_port_t	*portptr;

    xTrace0(portmaintp, TR_FULL_TRACE, "port maint init");
    xTrace0(portmaintp, TR_MAJOR_EVENTS, "port maint init");
    machnetipcProtl = self;
    self = myProtl;
    BootIdProtlObj = BootIdProtl;

    /*
     * THIS IS A PROTOCOL COPY OPERATION; IT IS NOT CREATEPROTL
     */
    *myProtl = *machnetipcProtl;
    xTrace0(portmaintp, TR_MAJOR_EVENTS, "port maint copied machnetipc XObj");
    getproc(myProtl);
    xSetDown(myProtl, MACHNETIPC_DNUM, self);

    notification_port =
	((struct machr_state *)(myProtl->state))->notification_port;
    myProtl->state = TYPE_MALLOC(struct portm_state);
    STATEPTR->notification_port = notification_port;
    xControl(xGetDown(myProtl, TRANSPORT_DNUM),
	GETMYHOST, (void *)&ipaddr, sizeof(IPhost) );
    xTrace4(portmaintp, TR_FUNCTIONAL_TRACE,
	"portmaint ipaddr is %d.%d.%d.%d",
	ipaddr.a, ipaddr.b, ipaddr.c, ipaddr.d);
    myProtl->id = protTblGetId("portmaint");
    xTrace1(portmaintp, TR_FUNCTIONAL_TRACE, "portmaint id %d", myProtl->id);
    myProtl->name = "portmaint";

    STATESELFPTR->local_source_addr = ipaddr;
    /*
     * a hack to get each host using a different port space
     */
    netport_id = NETPORT_INITIAL_NUMBER | (((int)ipaddr.d) << 24);
    xTrace2(portmaintp, TR_FUNCTIONAL_TRACE,
	"port_maint_init netport id %d (%x)", netport_id, netport_id);

    /*
     * establish communication with transport layer; same as machnetipc
     */
    partInit(whom, 1);
    if (pass_open_transport(whom) == XK_FAILURE) {
	xTrace0(portmaintp, TR_ERRORS,
	    "portm server can't openenable lower protocol");
    }

    STATESELFPTR->mapsendonceports =
	mapCreate(2*PORTM_SO_PORT_MAX, sizeof(mach_port_t));
    STATESELFPTR->mapsendoncenumbers =
	mapCreate(2*PORTM_SO_PORT_MAX, sizeof(mnportid_t));
    /*
     * ipaddr to TRUE/FALSE
     */
    STATESELFPTR->mapremotehosts =
	mapCreate(2*PORTM_HOST_MAX, sizeof(IPhost));
    /*
     * port id number to network descriptor
     */
    STATESELFPTR->mapnetdesc =
	mapCreate(2*PORTM_PORT_MAX, sizeof(mnportid_t));
    /*
     * local Mach port name to network descriptor
     */
    STATESELFPTR->maplocalports =
	mapCreate(2*PORTM_PORT_MAX, sizeof(mach_port_t));
    /*
     * this is not a map
     */
    STATESELFPTR->sendoncearray = portptr =
	(mach_port_t *)xMalloc(PORTM_SO_PORT_MAX * sizeof(mach_port_t));

    /*
     * this is a cache of receive rights to use for the server side of RPC's
     */
    for (i = 0; i<PORTM_SO_PORT_MAX; i++, portptr++) {
	kern_return_t	kret;

	if ((kret = mach_port_allocate(mach_task_self(),
		MACH_PORT_RIGHT_RECEIVE, portptr))
	    != KERN_SUCCESS) {
	    xTrace0(portmaintp, TR_ERRORS,
		"portmaintp: init: port allocation fails");
	}
    }
    Null_Netport.net_port_type = MN_INVALID;
    xTrace0(portmaintp, TR_MAJOR_EVENTS, "port maint init done");
    return XK_SUCCESS;
}


/*
 *  allocateLocalRight
 */
xkern_return_t
allocLocalRight(
	mnetport		*portd,
	mach_port_type_t	mtype)
{
    kern_return_t		ret;
    mach_msg_type_name_t	returned_type;

    xTrace1(portmaintp, TR_FULL_TRACE,
	"portmaint: allocLocalRight type %x", mtype);

    /*
     * make sure there's a local receive port; everything needs it
     */
    if (!portd->real_receive_port) {
	if ((ret = mach_port_allocate(mach_task_self(),
		    MACH_PORT_RIGHT_RECEIVE, &portd->real_receive_port))
	    != KERN_SUCCESS) {
	    xTrace0(portmaintp, TR_ERRORS,
		"portmaintp: allocLocalRight: could not allocate local receive port");
	    return XK_FAILURE;
	}
    }

    /*
     * bind the receive port to the netport number
     */
    if (mapBind(STATEPTR->maplocalports,
	    (void *)&portd->real_receive_port, (void *)portd)
	== ERR_BIND) {
	xTrace1(portmaintp, TR_ERRORS,
	    "portmaintp: allocLocalRight: could not bind local receive port %x with netport",
	    portd->real_receive_port);
	return XK_FAILURE;
    }

    if (mtype == MACH_PORT_TYPE_RECEIVE) {
	if (!portd->real_send_port) {
	    if ((ret = mach_port_extract_right(mach_task_self(),
			portd->real_receive_port, MACH_MSG_TYPE_MAKE_SEND,
			&portd->real_send_port, &returned_type))
		!= KERN_SUCCESS) {
		xTrace3(portmaintp, TR_ERRORS,
		    "portmaintp: allocLocalRight: could not extract send right from netport id %x port %x ret %x",
		    portd->net_port_number, portd->real_receive_port, ret);
		return XK_FAILURE;
	    }
	    /*
	     * should only do this is senders count != 0 
	     *	portd->real_send_port = portd->real_receive_port;
	     */
	} else {
	    xTrace0(portmaintp, TR_ERRORS,
		"portmaintp: allocLocalRight: already had a send right for this port");
	    return XK_FAILURE;
	}
    }
    return XK_SUCCESS;
}


/*
 * portm_move_netnum
 *
 *   a quick send_once right is turned into a slow one
 */
mnetport
portm_move_netnum(
	mach_port_t	port,
	mnportid_t	netnum)
{
    IPhost	receiver;

    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: portm_move_netnum called");
    if(mapResolve(STATEPTR->mapsendonceports, &port, &receiver) == XK_SUCCESS) {
	/*
	 * create new mnetport structure
	 */
	return
	    convert_to_netport(
		port, MACH_PORT_TYPE_SEND_ONCE, receiver, 0, netnum);
    }
    xTrace0(portmaintp, TR_ERRORS,
	"portmaint: could not find port in database");
    return Null_Netport;
}


/*
 *   portm_move_receiver
 *
 *    respond to rrx protocol notification that receiver has moved
 */
void
portm_move_receiver(
	mnetport	*portd)
{
    xTrace1(portmaintp, TR_FULL_TRACE,
	"portm_move_receiver called for port %x", portd->net_port_number);
    portd->lower_session = ERR_XOBJ;  /* next send will open new session */
    portd->net_port_rights &= ~MACH_PORT_TYPE_RECEIVE;
    /*
     *  portd->receiver_host_addr = host;
     */
    if (portd->current_read_desc && portd->current_read_desc->blocked) {
	xTrace1(portmaintp, TR_EVENTS,
	    "portm_move_receiver: Unblocking sender on port %x",
	    portd->net_port_number);
	port_unblock(portd);
    }
}


/*
 *  removeSender
 *
 *    interface call for port transfer protocols
 */
void
removeSender(
	mnetport	*netPort,
	IPhost		host)
{
    /*
     * remove from senders map; check count, deregister host interest
     */
    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: removeSender called");

    if (mapUnbind (netPort->senders_map, &host) == XK_SUCCESS) {
	netPort->sender_count--;
	if (!--netPort->sender_count) {
	    port_kill(netPort);
	}
	portm_deregister(host);
    }
}

