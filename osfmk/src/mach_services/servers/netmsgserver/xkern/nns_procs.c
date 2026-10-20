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
 * Revision 1.5  1993/11/15  22:35:04  hkaram
 * Bug fix in removing ports that have already been deleted
 * by portmaint protocol
 *
 * Revision 1.4  1993/10/02  00:02:11  menze
 * Added more strict prototypes
 *
 * Revision 1.3  1993/08/12  19:43:26  hkaram
 * Added support for multithreaded nns.
 * Cleaned up warning mesgs
 *
 * Revision 1.2  1993/08/05  16:12:33  hkaram
 * changed interface to remove_port to allow
 * for specification of right of port to be removed
 * from database.
 *
 * Revision 1.2  1992/05/12  14:34:48  devrcs
 * 	Created for OSF/1 MK
 * 	[1992/05/04  05:51:53  condict]
 *
 * Revision 2.4  91/03/27  17:29:40  mrt
 * 	Changed mach.h include
 * 
 * Revision 2.3  91/03/19  12:40:00  mrt
 * 	Changed to new copyright
 * 
 * Revision 2.2  90/09/12  16:33:51  rpd
 * 	Initial check-in.
 * 	[90/09/12  15:53:44  rpd]
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <stdio.h>
#include <strings.h>
#include <mach.h>
#include <mach_error.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <servers/netname.h>
#include <servers/netname_defs.h>
#include <xkern/include/xkernel.h>
#include <xkern/include/prot/ip.h>
#include <netmsgserver/xkern/nns.h>
#include <netmsgserver/xkern/nns_i.h>

extern int	strncasecmp();

boolean_t	Debug = 0;
char		*program = "NNS";

extern int	tracennsp;

extern mach_port_t	notification_port;
extern mach_port_t	service;
extern PSTATE		*pstate;

void	quit(int status, char * format, ...);

mach_port_t
	nns_execute_broadcast(char *key);

int	nns_IsIpAddr(char *ipaddr);

int	nns_get_host_ip_addr(char *name, char *ipaddr);

mach_port_t
	nns_look_up_host(IPhost host);

mach_port_t
	nns_register_host(IPhost host);

int	nns_look_up_netname(char *key);

void	netname_init(void);

kern_return_t
	do_netname_check_in(
	    mach_port_t server, netname_name_t key,
	    mach_port_t sig, mach_port_t name);

int	nns_IsMyIpAddr(IPhost iphost);

kern_return_t
	do_netname_look_up(
	    mach_port_t server, netname_name_t host,
	    netname_name_t key, mach_port_t *namep);

kern_return_t
	do_netname_check_out(
	    mach_port_t server, netname_name_t key, mach_port_t sig);

kern_return_t
	do_netname_version( mach_port_t server, netname_name_t version);

kern_return_t
	do_mach_notify_port_deleted( mach_port_t notify, mach_port_t name);

kern_return_t
	do_mach_notify_msg_accepted(mach_port_t notify, mach_port_t name);

kern_return_t
	do_mach_notify_port_destroyed(mach_port_t notify, mach_port_t port);

kern_return_t
	do_mach_notify_no_senders(
	    mach_port_t notify, mach_port_mscount_t mscount);

kern_return_t
	do_mach_notify_send_once(mach_port_t notify);

kern_return_t
	do_mach_notify_dead_name(mach_port_t notify, mach_port_t name);


typedef struct port_record {
    struct port_record	*next;
    mach_port_t		port;
    mach_port_urefs_t	urefs;
} *port_record_t;

/*
 *  We use a simple self-organizing list to keep track of
 *  registered ports.
 */

typedef struct name_record {
    struct name_record	*next;
    netname_name_t	key;
    mach_port_t		name, sig;
} *name_record_t;

static port_record_t	ports;
static name_record_t	list;
static mutex_t		netnameMutex;

#define	MAX_UREFS	1000	/* max urefs we will hold for a port */
#define MORE_UREFS	1000	/* when we need urefs, how many to make */

static void
add_reference(
	mach_port_t	port)
{
    port_record_t	this, *last;
    mach_port_t		previous;
    kern_return_t	kr;

    if (!MACH_PORT_VALID(port)) {
	return;
    }

    mutex_lock(netnameMutex);
    for (last = &ports, this = ports;
	this != NULL;
	last = &this->next, this = *last) {
	if (this->port == port) {
	    /*
	     * we already know about this port
	     */
	    if (++this->urefs > MAX_UREFS) {
		kr = mach_port_mod_refs(mach_task_self(), port,
		    MACH_PORT_RIGHT_SEND, 1 - this->urefs);
		if (kr == KERN_SUCCESS) {
		    this->urefs = 1;
		} else if (kr == KERN_INVALID_RIGHT) {
		    ; /* port is a dead name now */
		} else {
		    quit(1, "%s: add_reference: mach_port_mod_refs: %s\n",
			program, mach_error_string(kr));
		}
	    }

	    /*
	     * remove the record and move it to the front
	     */
	    *last = this->next;
	    goto insert;
	}
    }

    /*
     * we haven't seen this port before
     */
    this = (port_record_t) malloc(sizeof *this);
    this->port = port;
    this->urefs = 1;

    kr = mach_port_request_notification(mach_task_self(), port,
	MACH_NOTIFY_DEAD_NAME, TRUE,
	notification_port, MACH_MSG_TYPE_MAKE_SEND_ONCE,
	&previous);
    if ((kr != KERN_SUCCESS)/* || (previous != MACH_PORT_NULL) */) {
	quit(1, "%s: add_reference: mach_port_request_notification: %s\n",
	    program, mach_error_string(kr));
    }

insert:
    this->next = ports;
    ports = this;
    mutex_unlock(netnameMutex);
}

static void
sub_reference(
	mach_port_t	port)
{
    port_record_t	this, *last;
    kern_return_t	kr;

    if (!MACH_PORT_VALID(port)) {
	return;
    }

    /*
     * we must know about this port
     */
    mutex_lock(netnameMutex);
    for (last = &ports, this = ports;
	this != NULL;
	last = &this->next, this = *last) {
	if (this->port == port) {
	    break;
	}
    }

    /*
     * move it to the front
     */

    *last = this->next;
    this->next = ports;
    ports = this;

    /*
     * make more user-references if we don't have enough
     */
    if (this->urefs == 1) {
	kr = mach_port_mod_refs(mach_task_self(), port,
	    MACH_PORT_RIGHT_SEND, MORE_UREFS);
	if (kr == KERN_INVALID_RIGHT) {
	    kr = mach_port_mod_refs(mach_task_self(), port,
		MACH_PORT_RIGHT_DEAD_NAME, MORE_UREFS);
	}
	if (kr != KERN_SUCCESS) {
	    quit(1, "%s: sub_reference: mach_port_mod_refs: %s\n",
		program, mach_error_string(kr));
	}
	this->urefs += MORE_UREFS;
    }
    this->urefs--;
    mutex_unlock(netnameMutex);
}

static void
remove_port(
	mach_port_t		port,
	mach_port_right_t	right)
{
    port_record_t	this, *last;
    kern_return_t	kr;

    xTrace1(nnsp, TR_DETAILED,
	"NNS remove_port: removing port %x - dead name notification", port);

    mutex_lock(netnameMutex);
    for (last = &ports, this = ports;
	this != NULL;
	last = &this->next, this = *last) {
	if (this->port == port) {
	    break;
	}
    }

    if (this == NULL) {
	mutex_unlock(netnameMutex);
	return;
    }

    /*
     * deallocate the dead name - dont check the return
     * code because the port could be a proxy for a remote
     * port and may already have been removed by the port
     * maintenance protocol
     */
    (void)mach_port_mod_refs(mach_task_self(), port, right, -this->urefs);

    /*
     * remove the record
     */
    *last = this->next;
    free((char *) this);
    mutex_unlock(netnameMutex);
}

static boolean_t
netname_find(
	netname_name_t	key,
	name_record_t	*thisp,
	name_record_t	**prevp)
{
    register name_record_t	*prev, this;

    mutex_lock(netnameMutex);
    for (this = *(prev = &list);
	 this != NULL;
	 this = *(prev = &this->next)) {
	if (strncasecmp(this->key, key, 80) == 0) {
	    *thisp = this;
	    *prevp = prev;
	    mutex_unlock(netnameMutex);
	    return TRUE;
	}
    }

    mutex_unlock(netnameMutex);
    return FALSE;
}

int nns_look_up_netname(
	char	*key)
{
    name_record_t	*prev, this;

    return netname_find(key, &this, &prev);
}

void
netname_init( void )
{
    list = NULL;
    ports = NULL;
    netnameMutex = mutex_alloc();
}

static void
netname_remove(
	mach_port_t	name)
{
    register name_record_t	*prev, this;

    if (Debug) {
	printf("%s: netname_remove(%x)\n", program, name);
    }

    mutex_lock(netnameMutex);

    prev = &list;
    this = list;

    while (this != NULL) {
	if ((this->name == name) ||
	    (this->sig == name)) {
	    register name_record_t	last = this;

	    this = *prev = this->next;
	    free(last);
	} else {
	    this = *(prev = &this->next);
	}
    }

    mutex_unlock(netnameMutex);
}

kern_return_t
do_netname_check_in(
	mach_port_t server,
	netname_name_t key,
	mach_port_t sig,
	mach_port_t name)
{
    name_record_t	*prev, this;

    xTrace2(nnsp, TR_DETAILED,
	"NNS do_netname_check_in: Entered Checkin: service %s sig = %x",
	key, sig);

    if (Debug) {
	printf("%s: netname_check_in(%.80s, name=%x, sig=%x)\n",
	    program, key, name, sig);
    }

    if (netname_find(key, &this, &prev)) {
	if (this->sig == sig) {
	    mutex_lock(netnameMutex);
	    this->name = name;
	    *prev = this->next;
	    mutex_unlock(netnameMutex);
	} else {
	    xTrace2(nnsp, TR_ERRORS,
		"NNS do_netname_check_in: net name not yours service = %s; sig = %x",
		key, sig);
	    return NETNAME_NOT_YOURS;
	}
    } else {
	this = (name_record_t) malloc(sizeof *this);
	if (this == NULL) {
	    return KERN_RESOURCE_SHORTAGE;
	}

	this->name = name;
	this->sig = sig;
	(void) strncpy(this->key, key, 80);
    }

    mutex_lock(netnameMutex);
    this->next = list;
    list = this;
    mutex_unlock(netnameMutex);

    /*
     * we take responsibility for the refs in the request
     */
    add_reference(sig);
    add_reference(name);

    xTrace4(nnsp, TR_DETAILED,
	"NNS do_netname_check_in: Checked in service %s sig = %x port = %d (%x)",
	key, sig, name, name);

    return NETNAME_SUCCESS;
}


int nns_IsMyIpAddr(
	IPhost	iphost)
{
    return ! bcmp((char *)&iphost, (char *)&pstate->myaddr, sizeof(IPhost));
}

static kern_return_t
local_look_up(
	netname_name_t	key,
	mach_port_t	*namep)
{
    name_record_t	*prev, this;

    /*
     * local case
     */
    if (netname_find(key, &this, &prev)) {
	mach_port_t	name = this->name;

	mutex_lock(netnameMutex);
	*prev = this->next;
	this->next = list;
	list = this;
	mutex_unlock(netnameMutex);

	if (Debug) {
	    printf("%s: netname_look_up(%.80s) => %x\n", program, key, name);
	}

	/*
	 * return a ref for the port
	 */
	xTrace3(nnsp, TR_DETAILED,
	    "NNS local_look_up: Found service %s port = %d (0x%x)",
	    key, name, name);

	sub_reference(*namep = name);

	return NETNAME_SUCCESS;
    }

    if (Debug) {
	printf("%s: netname_look_up(%.80s) failed\n", program, key);
    }

    xTrace1(nnsp, TR_DETAILED,
	"NNS local_look_up: Did not find service %s", key);

    return NETNAME_NOT_CHECKED_IN;
}

kern_return_t
do_netname_look_up(
	mach_port_t	server,
	netname_name_t	host,
	netname_name_t	key,
	mach_port_t	*namep)
{
    IPhost		iphost;
    mach_port_t		remote_port = MACH_PORT_NULL;
    kern_return_t	ret;
    mach_port_t		remote_service_port;
    int			broadcast = 0;

    xTrace2(nnsp, TR_DETAILED,
	"NNS do_netname_look_up: Host name = %s Service name = %s", host, key);

    if (*host != '\0') {

	/*
	 * check for broadcast
	 */
	if (*host == '*') {

	    /*
	     * look in local host first
	     */
	    ret = local_look_up(key, namep);
	    if (ret == NETNAME_SUCCESS) {
		return NETNAME_SUCCESS;
	    }

	    /*
	     * broadcast on LAN
	     */
	    remote_port = nns_execute_broadcast(key);

	    broadcast = 1;
	}

	if (! broadcast) {
	    if (nns_IsIpAddr(host)) {
		str2ipHost(&iphost, host);
	    } else if (nns_get_host_ip_addr(host, (char *)&iphost) < 0) {
		return NETNAME_NOT_CHECKED_IN;
	    }
	}

	if (broadcast || !nns_IsMyIpAddr(iphost)) {
	    /*
	     * look up host in table else, try to register it
	     */
	    if (! broadcast) {
		remote_port =  nns_look_up_host(iphost);
		if (remote_port == MACH_PORT_NULL) {
		    remote_port = nns_register_host(iphost);
		}
	    }

	    if (remote_port == MACH_PORT_NULL) {
		return NETNAME_NOT_CHECKED_IN;
	    }

	    /*
	     * forward request to server
	     */
	    xTrace0(nnsp,TR_DETAILED,
		"NNS do_netname_look_up: Forwarding request to server host");

	    ret = netname_look_up(remote_port, "", key, &remote_service_port);
	    if (ret != NETNAME_SUCCESS) {
		xTrace4(nnsp, TR_ERRORS,
		    "NNS do_netname_look_up: Failed to get service on host %s remote port %d(%x) %s",
		    host, ret, ret, mach_error_string(ret));
		return NETNAME_NOT_CHECKED_IN;
	    } else {
		xTrace3(nnsp, TR_DETAILED,
		    "NNS do_netname_look_up: Got the service %s on remote host: port = %d (0x%x)",
		    key, remote_service_port, remote_service_port);
		*namep = remote_service_port;
		return NETNAME_SUCCESS;
	    }
	}
    }

    /*
     * local case
     */
    if (name_server_port != service) {
	xTrace0(nnsp, TR_DETAILED,
	    "NNS do_netname_look_up: Asking to local name_server");
	return netname_look_up(name_server_port, "", key, namep);
    } else {
	return local_look_up(key, namep);
    }
}

kern_return_t
do_netname_check_out(
	mach_port_t	server,
	netname_name_t	key,
	mach_port_t	sig)
{
    name_record_t	*prev, this;

    xTrace2(nnsp, TR_DETAILED,
	"NNS do_netname_check_out: Entered Checkout: service %s sig = %x",
	key, sig);

    if (Debug) {
	printf("%s: netname_check_out(%.80s, sig=%x)\n", program, key, sig);
    }

    if (netname_find(key, &this, &prev)) {
	if (this->sig == sig) {
	    remove_port(this->name, MACH_PORT_RIGHT_SEND);

	    mutex_lock(netnameMutex);
	    *prev = this->next;
	    free((char *) this);
	    mutex_unlock(netnameMutex);

	    /*
	     * we take responsibility for the ref in the request
	     */
	    add_reference(sig);

	    xTrace2(nnsp, TR_DETAILED,
		"NNS do_netname_check_out: Checked out service %s sig = %x",
		key, sig);

	    return NETNAME_SUCCESS;
	} else {
	    xTrace2(nnsp, TR_ERRORS,
		"NNS do_netname_check_out: netname not yours: service %s sig = %x",
		key, sig);
	    return NETNAME_NOT_YOURS;
	}
    } else {
	return NETNAME_NOT_CHECKED_IN;
    }
}

kern_return_t
do_netname_version(
	mach_port_t server,
	netname_name_t version)
{
    if (Debug) {
	printf("%s: netname_version()\n", program);
    }

    (void) strcpy(version, "Network Name Service 3.0");
    return KERN_SUCCESS;
}

kern_return_t
do_mach_notify_port_deleted(
	mach_port_t	notify,
	mach_port_t	name)
{
    netname_remove(name);
    remove_port(name, MACH_PORT_RIGHT_DEAD_NAME);
    return KERN_SUCCESS;
}

kern_return_t
do_mach_notify_msg_accepted(
	mach_port_t	notify,
	mach_port_t	name)
{
    quit(1, "%s: do_mach_notify_msg_accepted\n", program);
    return KERN_FAILURE;
}

kern_return_t
do_mach_notify_port_destroyed(
	mach_port_t	notify,
	mach_port_t	port)
{
    quit(1, "%s: do_mach_notify_port_destroyed\n", program);
    return KERN_FAILURE;
}

kern_return_t
do_mach_notify_no_senders(
	mach_port_t		notify,
	mach_port_mscount_t	mscount)
{
    quit(1, "%s: do_mach_notify_no_senders\n", program);
    return KERN_FAILURE;
}

kern_return_t
do_mach_notify_send_once(
	mach_port_t	notify)
{
    quit(1, "%s: do_mach_notify_send_once\n", program);
    return KERN_FAILURE;
}

kern_return_t
do_mach_notify_dead_name(
	mach_port_t	notify,
	mach_port_t	name)
{
    if (Debug) {
	printf("%s: do_mach_notify_dead_name: %x\n", program, name);
    }

    netname_remove(name);
    remove_port(name, MACH_PORT_RIGHT_DEAD_NAME);
    return KERN_SUCCESS;
}
