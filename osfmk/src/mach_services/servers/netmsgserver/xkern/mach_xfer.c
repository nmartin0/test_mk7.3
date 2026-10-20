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
 * mach-xfer.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.9
 * Date: 1993/08/12 19:35:05
 */
/*
 *  Support for port transfers
 *
 *   The glue between the port transfer protocols and the
 *   port manager.
 */

#include <stdio.h>
#include <mach.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <cthreads.h>
#include <xkern/include/xkernel.h>
#include <xkern/include/prot/ip.h>
#include <xkern/include/prot/bidctl.h>
#include <s_xkern/include/prot/machripc.h>
#include <s_xkern/include/prot/machripc_xfer.h>

extern traceportmaintp;

extern mnetport Null_Netport;  /* error return value */

char		*mportNetRepStr(mportNetRep *mn);
void		port_kill(mnetport *);
xkern_return_t	portm_register(IPhost addr);
xkern_return_t	quick_netport_lookup(unsigned int portid, mnetport **portdesc);
xkern_return_t	convert_netport_to_tmp_mach_port(
		    mn_netport_t *netport, unsigned int rtype,
		    IPhost rec_addr, msg_id_t msgid);
void		portm_move_receiver(mnetport *);


char *
mportNetRepStr(
	mportNetRep	*mn)
{
    return("portxxx");
}

static void
addHostRef(
	IPhost	*h)
{
    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: addHostRef");
    portm_register(*h);
}

void
removeSendRight(
	mnetport	*npd)
{
    xTrace1(portmaintp, TR_EVENTS,
	"portmaintp: remove send right for port %d",
	npd->net_port_number);
}

void
removeReceiveRight(
	mnetport	*npd)
{
    xTrace1(portmaintp, TR_EVENTS,
	"portmaintp: remove receive right for port %d",
	npd->net_port_number);
    port_kill(npd);
}

void
receiveRightDeallocated(
	mnetport	*npd)
{
    xTrace1(portmaintp, TR_EVENTS,
	"portmaintp: receiveRightDeallocated receives port death notification of for port %d",
	npd->net_port_number);
    port_kill(npd);
}

void
receiverMoved(
	mnetport	*npd)
{
    xTrace2(portmaintp, TR_EVENTS,
	"portmaintp: receiverMoved notification for port %x (new rcvr %s)",
	npd->net_port_number, ipHostStr(&npd->receiver_host_addr));
    portm_move_receiver(npd);
    addHostRef(&npd->receiver_host_addr);
}

/*
 *  addNewSender
 *
 *     we hold a network receive right; a new sender node is added
 *     to the list
 *
 */
void
addNewSender(
	mnetport *portd, IPhost sender_addr, int mscount )
{
    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: addNewSender");
/*
    xIfTrace(portmaintp, TR_FULL_TRACE) {
	if (!(portd->net_port_rights & MACH_PORT_TYPE_RECEIVE)) {
	    xTrace0(portmaintp, TR_ERRORS,
		"portmaintp: addNewSender: did not have receive right");
	    return;
	}
    }
*/

    /* pre-existing right */
    if (mapResolve(portd->senders_map, &sender_addr, 0) == XK_SUCCESS) {
	/* portd->sender_count++; */
	portm_register(sender_addr);
#ifdef	MAKESENDCOUNT
	mapUnbind(portd->senders_map, &sender_addr);
	mapBind(portd->senders_map, &sender_addr, ++portd->make_send_count);
#endif	/* MAKESENDCOUNT */
	return;
    }
    /* new right */
    if ( mapBind(portd->senders_map, &sender_addr, ++portd->make_send_count)
	 == ERR_BIND)
	return;
    portd->sender_count++;
    portm_register(sender_addr);
    return;
}

/*
 * findNetPort
 *
 *	 port transfer protocols call this to get the 
 *	    netport structure.
 */
xkern_return_t
findNetPort(
	mn_netport_t		*pnportd,
	enum SOURCE_BYTE_ARCH	arch_tag,
	boolean_t		create_flag,
	mnetport		**npd )
{
    mach_port_type_t	right;
    IPhost		receiver_addr;
    mn_netport_t	nportd;
    int			arch_index = arch_unpermute_index(arch_tag);

    xTrace0(portmaintp, TR_FULL_TRACE, "portmaint: findNetPort");

    /*
     * only the netport number needs to be converted, the rest
     * are set by the rrx protocol; the netport number arrives
     * from the sending host
     */
    nportd = *pnportd;
    if (arch_tag != MN_ARCH_MARKER) {
	nportd.net_port_number =
	    unpermute_int32[arch_index]((char *)&pnportd->net_port_number);
    }

    right = nportd.net_port_rights;
    receiver_addr = nportd.receiver_host_addr;

    *npd = NULL;
    if (quick_netport_lookup(nportd.net_port_number, npd) != XK_SUCCESS) {
	if (!create_flag) {
	    return XK_FAILURE;
	}
	convert_netport_to_tmp_mach_port (&nportd, right, receiver_addr, 0);
	quick_netport_lookup(nportd.net_port_number, npd);
    }

    if (right == MACH_PORT_TYPE_SEND) {
	if (nportd.make_send_count > (*npd)->make_send_count) {
	    (*npd)->make_send_count = nportd.make_send_count;
	}
    } else if (right == MACH_PORT_TYPE_RECEIVE) {
	if ((*npd)->senders_map == NULL) {
	    (*npd)->senders_map =
		mapCreate(INITIAL_MAX_MNIPC_SENDERS,sizeof(IPhost));
	}
    }
    return XK_SUCCESS;
}

/*
 * updatePortId
 *       Update the message Id and original rights owner host address
 *       in the port structure. The message Id is of the message that
 *       will complete the port xfer.
 *
 */
void
updatePortId(
	mnetport	*npd,
	msg_id_t	msgId,
	IPhost		orig_host)
{
    npd->msgid = msgId;
    npd->orig_host = orig_host;
}

#ifdef HDRROUTINES

void
unscramble(
	int			*number,
	enum SOURCE_BYTE_ARCH	arch )
{
    if (arch != MN_ARCH_MARKER) {
	xTrace0(portmaintp, TR_ALWAYS,
	    "portmaintp: unscramble: incompatible architectures");
    }
}

/*
 *    mnetportLoad
 */
void
mnetportLoad(
	char			*src,
	mportNetRep		*dst, 
	enum SOURCE_BYTE_ARCH	arch_tag)
{
    if (sizeof(int) != 4) {
	xTrace0(portmaintp, TR_ALWAYS,
	   "portmaint: mnetportLoad: Cannot convert incoming int to local int");
	return;
    }
    if (arch_tag == MN_ARCH_MARKER) {
	bcopy(src, (char*)dst->net_port_number, sizeof(int));
	src += 4;
	bcopy(src, (char*)dst->net_port_rights, sizeof(int));
	src += 4;
	bcopy(src, (char*)dst->receiver_host_addr, sizeof(IPhost));
    }
    dst->net_port_number = unscramble((int *)&src, arch_tag);
    src += 4;
    dst->net_port_rights = unscramble((int *)&src, arch_tag);
    src +=4;
    dst->receiver_host_addr = unscramble((int *)&src, arch_tag);
}

/*
 *    mnetportStore
 */
void
mnetportStore(
	char		*src,
	mportNetRep	*dst)
{
    if (sizeof(int) != 4) {
	xTrace0(portmaintp, TR_ALWAYS,
	    "portmaint: mnetportStore: Cannot convert outgoing int to net int");
	return;
    }
    bcopy((char *)&src->net_port_number, dst, 4);
    dst += 4;
    bcopy((char *)&dst->net_port_rights, dst, 4);
    dst += 4;
    bcopy((char *)&dst->receiver_host_addr, dst, sizeof(IPhost));
}

#endif	/* HDRROUTINES */
