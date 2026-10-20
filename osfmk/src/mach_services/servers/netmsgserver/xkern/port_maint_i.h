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
 * port-maint_internal.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.6
 * Date: 1993/10/21 17:08:11
 */
/*
 * machr version for v3.2 by Hilarie Orman begun Wed Apr  1 13:57:13 1992
 */

#ifndef	port_maint_internal_h
#define	port_maint_internal_h

/* 
 * global data for PORTMAINT;
 * need data structure for assigned/free port nums
 */

#define PORTM_PORT_MAX		512
#define PORTM_SO_PORT_MAX	64
#define PORTM_HOST_MAX		128

/*
 * mapsize for a receiver port's lower sessions
 */
#define PORTM_PORT_LOWER_SESSIONS	12

#define NETPORT_INITIAL_NUMBER	0x4567

#define PORTM_UDP_PORTNUM	113

/*
 * this must match machnetipc
 */
#define TRANSPORT_DNUM		0
#define TRANSPORT_RPC_DNUM	1
#define BOOTID_DNUM		2
#define MACHNETIPC_DNUM		3

#define BootIdProtl	(xGetDown(myProtl, BOOTID_DNUM))
#define TransportProtl	(xGetDown(myProtl, TRANSPORT_DNUM))
#define MachNetIPCProtl	(xGetDown(myProtl, MACHNETIPC_DNUM))

struct portm_state {
    int		rnum;
    Map		maplocalports;
    Map		mapnetdesc;
    Map		mapremotehosts;
    Map		mapsendonceports;
    Map		mapsendoncenumbers;
    mach_port_t	*sendoncearray;
    mach_port_t	notification_port;
    IPhost	source_id;
    /*
     * the following is used to identify this host uniquely;
     * any of its IP addresses will do
     */
    IPhost	local_source_addr;
};

/*
 * The active map is keyed on the pair of ports and the lower level IP
 * session.
 */
typedef struct {
    mach_port_t	localport;
    mach_port_t	remoteport;
    IPhost	destination_host;
    int		rcnt;
} Portm_ActiveId;

/*
 *  Port maintenance network messages
 */
enum PORTMAINTTYPE
{ 
    XKPM_NOMORESENDERS = 0,
    XKPM_PORTDEATH
};

#define PORT_MGMT_TYPELEN	1

/*
 *  one port manager to another: port dead, no more senders
 *
 */
struct port_mgmt_msg {
    mnportid_t		net_port_number;
    enum PORTMAINTTYPE	type;
    IPhost		sender;
    int			make_send_count;
};

#define PORTMGMTMSGSIZE		\
	    PORTID_NETLEN+PORT_MGMT_TYPELEN+HOSTNETLEN+NETMAKESENDSIZE

#endif	/* !port_maint_internal_h */
