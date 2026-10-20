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
 * machnetipc.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.24
 * Date: 1993/11/05 01:35:47
 */
/*
 *
 * MACHNETIPC - an embedding of Mach3 IPC in a network
 *
 *
 *   See CAVEATS, below
 *
 *
 *  machripc has three functions: to convert messages to and from network form,
 *    to manage port transfers embedded in messages,
 *    and to interface upwards to the simple server protocol that
 *    bootstraps port transfers between machines
 *  the direct data transfer is the function of the protocol, but
 *  the port transfers are side-effects that alter the state of the
 *  machripc protocol and the port set of the Mach OS.
 *
 *  The interface to the SSR protocol is modelled as transfering a
 *  composite object: the message (just a service number) and a port (to be
 *  used for the reply).
 *
 */
/* 
 *  performs three functions:
 *
 *  for ports with send rights, this must devote a thread to listening.
 *  
 *  for data to be sent to net, convert to network msg form and send
 *  to lower protocol
 *  	note transfers of receive rights to remote machine ...
 *    notify external senders of the change
 *  convert message to network form and push to next protocol
 *
 *  for data coming in from net
 *  	reconstitute ports
 *	establish listening threads
 *  demux on ssr vs. mach vs. port mgmt types
 *  send message to destination Mach port or xPop to ssr protocol
 */
/*
 *   CAVEATS: the network port identifiers are fragile for the time
 *    being.  They are based on the lower 24 bits of the IP address
 *    and an 8 bit sequence number.  This is inadequate and will be
 *    changed in the near future.
 *
 *   Behavior under heavy load or with machine reboots during complex
 *   port transfers have not been tested.
 *
 *   Embedded port right transfers have been tested only lightly.
 *
 *   Support for heterogeneous architectures is not included.  Some
 *   of the header load and store routines attempt to be architecture
 *   independent, but this work is still in progress.
 *
 */

#include <stdio.h>
#include <mach.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <cthreads.h>
#include <xkern/include/xkernel.h>
#include <xkern/include/prot/ip.h>
#include <xkern/include/rwlock.h>
#include <s_xkern/include/prot/machripc.h>
#include <s_xkern/include/prot/rrx.h>
#include <s_xkern/include/prot/srx.h>

#define XK_MACH_MSG_TIMEOUT 10000  /* 10 seconds for an RPC reply */

/*
 * ZZZ : following macros are used by proxy receivers/senders
 *   proxy receiver has to get full options of trailer elements
 *   proxy sender has to send trailer
 * ??? : how are types managed when other types come?
 */

#define PROXY_RCV_OPTS	(MACH_RCV_MSG |				\
	    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |	\
	    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_SENDER))

#define PROXY_SEND_OPTS	(MACH_SEND_MSG | MACH_SEND_TRAILER)

/*
 * ZZZ : for receiveing, msgh_size should INCLUDE trailer length.
 */
#define mach_msg_receiveSafeSize( msg )	mach_msg_overwrite(		\
	    (mach_msg_header_t *)(msg),		/* send_msg	*/	\
	    PROXY_RCV_OPTS,			/* option	*/	\
	    0,					/* send_size	*/	\
	    ((mach_msg_header_t *)(msg))->msgh_size,			\
						/* receive_limit*/	\
	    ((mach_msg_header_t *)(msg))->msgh_local_port,		\
						/* receive_name	*/	\
	    MACH_MSG_TIMEOUT_NONE,		/* timeout	*/	\
	    MACH_PORT_NULL,			/* notify	*/	\
	    MACH_MSG_NULL,			/* receive_msg	*/	\
	    0)					/* receive_msg_size*/

#define mach_msg_receiveSafeSizeTime( msg )	mach_msg_overwrite(	\
	    0,					/* send_msg	*/	\
	    PROXY_RCV_OPTS | MACH_RCV_TIMEOUT,				\
						/* option	*/	\
	    0,					/* send_size	*/	\
	    ((mach_msg_header_t *)(msg))->msgh_size,			\
						/* receive_limit*/	\
	    ((mach_msg_header_t *)(msg))->msgh_local_port,		\
						/* receive_name	*/	\
	    XK_MACH_MSG_TIMEOUT,		/* timeout	*/	\
	    MACH_PORT_NULL,			/* notify	*/	\
	    MACH_MSG_NULL,			/* receive_msg	*/	\
	    0)					/* receive_msg_size*/

#define mach_msg_receive_proxy( msg )	mach_msg_overwrite(		\
	    (mach_msg_header_t *)(msg),		/* send_msg	*/	\
	    PROXY_RCV_OPTS,			/* option	*/	\
	    0,					/* send_size	*/	\
	    ((mach_msg_header_t *)(msg))->msgh_size,			\
						/* receive_limit*/	\
	    ((mach_msg_header_t *)(msg))->msgh_local_port,		\
						/* receive_name	*/	\
	    MACH_MSG_TIMEOUT_NONE,		/* timeout	*/	\
	    MACH_PORT_NULL,			/* notify	*/	\
	    MACH_MSG_NULL,			/* receive_msg	*/	\
	    0)					/* receive_msg_size*/

/*
 * ZZZ : for sending, msgh_size should NOT INCLUDE trailer length.
 */
#define mach_msg_send_proxy( msg )	mach_msg_overwrite(		\
	    (mach_msg_header_t *)(msg),		/* send_msg	*/	\
	    PROXY_SEND_OPTS,			/* option	*/	\
	    ((mach_msg_header_t *)(msg))->msgh_size,			\
						/* send_size	*/	\
	    0,					/* receive_limit*/	\
	    ((mach_msg_header_t *)(msg))->msgh_local_port,		\
						/* receive_name	*/	\
	    MACH_MSG_TIMEOUT_NONE,		/* timeout	*/	\
	    MACH_PORT_NULL,			/* notify	*/	\
	    MACH_MSG_NULL,			/* receive_msg	*/	\
	    0)					/* receive_msg_size*/

/* ZZZ : options for opts, a variable used in mach_msg_send_rcv macro */
#define RPC_OPTS  (PROXY_SEND_OPTS | MACH_SEND_TIMEOUT |	\
	   PROXY_RCV_OPTS | MACH_RCV_TIMEOUT)

#define SEND_OPTS (PROXY_SEND_OPTS | MACH_SEND_TIMEOUT)

/*
 * ZZZ : for sending, msgh_size should NOT INCLUDE trailer length.
 *       for receiveing, message buffer should have enough room
 */
#define	mach_msg_send_rcv( msg )	mach_msg_overwrite(		\
	    (mach_msg_header_t *)(msg),		/* send_msg	*/	\
	    opts,				/* option	*/	\
	    ((mach_msg_header_t *)(msg))->msgh_size,			\
						/* send_size	*/	\
	    ROUND4(((mach_msg_header_t *)(msg))->msgh_size)		\
		+ MAX_TRAILER_SIZE,					\
						/* receive_limit*/	\
	    ((mach_msg_header_t *)(msg))->msgh_local_port,		\
						/* receive_name	*/	\
	    XK_MACH_MSG_TIMEOUT,		/* timeout	*/	\
	    MACH_PORT_NULL,			/* notify	*/	\
	    MACH_MSG_NULL,			/* receive_msg	*/	\
	    0)					/* receive_msg_size*/

#define pass_open_transport(part)					\
	    (xOpenEnable(myProtl, myProtl,				\
		xGetDown(myProtl, MNI_TRANSPORT_DNUM), part))

#define pass_open_rpc_transport(part) 					\
	    (xOpenEnable(myProtl, myProtl,				\
		xGetDown(myProtl, MNI_RPC_TRANSPORT_DNUM), part))

#define active_open_transport(part)					\
	    (xOpen(myProtl, myProtl,					\
		xGetDown(myProtl, MNI_TRANSPORT_DNUM), part))

#define active_open_rpc_transport(part)					\
	    (xOpen(myProtl, myProtl,					\
		xGetDown(myProtl, MNI_RPC_TRANSPORT_DNUM), part))

#define LOCAL_PART	1
#define REMOTE_PART	0

#define ipaddr2int(foo)							\
	    ((foo.a)<<24 + (foo.b)<<16 + (foo.c)<<8  + (foo.d))
#define int2iphost(foo, des)						\
	    { des.a = (foo>>24) & 0xFF; des.b = (foo>>16) & 0xFF;	\
	      des.c = (foo>>8)  & 0xFF; des.d =  foo      & 0xFF; }

#define ONESEND(r)	(r & (MACH_PORT_TYPE_SEND | MACH_PORT_TYPE_SEND_ONCE))
#define IP_NULL		{ 0, 0, 0, 0 };

int		tracemachripcp;

static XObj	myProtl;

static void	getproc(XObj);

void		mach_msg_destroy();

#define IPEQUAL(_a, _b)					\
	    ((_a.a == _b.a) && (_a.b == _b.b) &&	\
	     (_a.c == _b.c) && (_a.d == _b.d))

#define STATEPTR		((struct machr_state *)(myProtl->state))
#define STATESELFPTR		((struct machr_state *)(self->state))
#define SESSNSTATEPTR		((Mach_ActiveId *)(self->state))
#define SESSNSTATEVARPTR(session)	((Mach_ActiveId *)(session->state))

#define TYPE_MALLOC(type)	(type *)xMalloc(sizeof(type))

/* extern definition needed immediately */
long port_hdr_pop_and_swap(char *to, char *from, long len, void *arg);

/* 
 * the item_size_table is defined in machmsg-intr.c and used to
 *  determine the number of bytes for the basic Mach msg data units
 */
extern int item_size_table[];


/*
 *  null_free routine for msgConstructInplace
 */
static void
null_free(
	void	*addr,
	int	size)
{
    xTrace0(machripcp, TR_EVENTS, "machripc: null free routine");
}

/* 
 *  Null_Netport  definition shared with port_maint
 *
*/
mnetport Null_Netport;  /* error return value */

/*
 *  Routines for copying data; ignoring alignment and representation
 *  problems for the moment.
 *
 */

static long
mycopypop(
	char	*to,
	char	*from,
	long	len,
	void	*arg)
{
    if ((int)len == len) {
	xTrace3(machripcp, TR_FULL_TRACE,
	    "copy pop %x to %x len %d", from, to, len);
	bcopy(from, to, len);
	return(len);
    }
    else {
	xTrace1(machripcp, TR_ERRORS,
	    "machr: copypop: Untenable len for copy function %ld", len);
    }
    return((long)0);
}


/*
 *  machnetipc_msg_pop
 *
 *	gets the architecture and message type
 */
static long
machnetipc_msg_pop(
	char	*to,
	char	*from,
	long	len,
	void	*arg)
{
    mn_arch_tag_t arch;
    struct mnipchdr *hdr = (struct mnipchdr *)to;

    if (len < MN_ARCH_TAG_NETLEN + MACHIPCTYPE_NETLEN) {
	xTrace1(machripcp, TR_SOFT_ERROR,
	    "machnetipc: machnetipc_msg_pop: message too short %d", len);
	return 0;
    }
    bcopy(from, (char *)&hdr->architecture_type, MN_ARCH_TAG_NETLEN);
    arch = hdr->architecture_type;
    if (arch == MN_ARCH_MARKER) {
	bcopy(from+MN_ARCH_TAG_NETLEN,
	    (char *)&hdr->machnetipcmsg_type, MACHIPCTYPE_NETLEN);
    }
    else {
	int	i = arch_unpermute_index(arch);

	hdr->machnetipcmsg_type = unpermute_int32[i](from+MN_ARCH_TAG_NETLEN);
    }
    return len;
}

/* 
 * ssrpop
 *	get parameters from the ssr header to direct the
 *	port transfer operations
 */
static long
ssrpop(
	char	*to,
	char	*from,
	long	len,
	void	*(*argf[])())
{
    struct ssrhdr	*ssr = (struct ssrhdr *)to;

    if ((int)len == len) {
	xTrace3(machripcp, TR_FULL_TRACE,
	    "machnetipc: ssrpop %x to %x len %d", from, (int)to, len);

	/* swap netreplyport */
	port_hdr_pop_and_swap(to, from, (long)sizeof(mn_netport_t), argf);
	from += sizeof(mn_netport_t);

	/* copy xfer_completion -note that the char is expanded to an int */
	ssr->xfer_completion = *from;
	from += sizeof(int);

	/* swap msgid */
	bcopy (from, (char *)&(ssr->msgid), MSGID_NETLEN);
	ssr->msgid = (int)argf[MSGID_NETTYPE]((char *)&ssr->msgid);

	return len;
    }
    else {
	xTrace1(machripcp, TR_ERRORS,
	    "machnetipc: ssrpop: Untenable len for copy function %ld", len);
    }
    return (long)0;
}


/* 
 *  machnetipc_pop
 *
 */
static long
machnetipc_pop(
	char	*to,
	char	*from,
	long	len,
	void	*(*argf[])())
{
    struct machnetipc *mnipc = (struct machnetipc *)to;

    if ((int)len == len) {
	xTrace3(machripcp, TR_FULL_TRACE,
	    "machnetipc: ssrpop %x to %x len %d", from, (int)to, len);

	/* swap transport port */
	port_hdr_pop_and_swap(to, from, (long)sizeof(mn_netport_t), argf);
	from += sizeof(mn_netport_t);
	to += sizeof(mn_netport_t);

	/* swap reply port */
	port_hdr_pop_and_swap(to, from, (long)sizeof(mn_netport_t), argf);
	from += sizeof(mn_netport_t);

	bcopy (from, (char *)&(mnipc->netport_count),
	    item_size_table[XK_INTEGER_16]);
	mnipc->netport_count =
	    (int)argf[XK_INTEGER_16]((char *)&mnipc->netport_count);
	from += item_size_table[XK_INTEGER_16];

	bcopy (from, (char *)&(mnipc->notinline_count),
	    item_size_table[XK_INTEGER_16]);
	mnipc->notinline_count =
	   (int)argf[XK_INTEGER_16]((char *)&mnipc->notinline_count);
	from += item_size_table[XK_INTEGER_16];

	bcopy (from, (char *)&(mnipc->sequence_num),
	    item_size_table[XK_INTEGER_32]);
	mnipc->sequence_num =
	    (int)argf[XK_INTEGER_32]((char *)&mnipc->sequence_num);
	from += item_size_table[XK_INTEGER_32];

	/* swap msgid */
	bcopy (from, (char *)&(mnipc->message_id), MSGID_NETLEN);
	mnipc->message_id =
	    (int)argf[MSGID_NETTYPE]((char *)&mnipc->message_id);

	return len;
    }
    else {
	xTrace1(machripcp, TR_ERRORS,
	    "machnetipc: machnetipc_pop: Untenable len for copy function %ld",
	    len);
    }
    return (long)0;
}

/*
 * ssrpush
 *	    get parameters into the ssr header to direct the
 *	      port transfer operations
 */
static void
ssrpush(
	char	*from,
	char	*to,
	long	len,
	void	*arg)
{
    struct machripc_msg	*frombuf = (struct machripc_msg *)from;

    if ((int)len == len) {
	xTrace3(machripcp, TR_FULL_TRACE,
	    "machnetipc: ssrpush %x to %x len %d", from, to, len);
	bcopy(from, to, len);
    }
    else {
	xTrace1(machripcp, TR_ERRORS,
	    "machnetipc: ssrpush: Untenable len for copy function %ld", len);
    }
}

/*
 *   mycopypop_andlie   ,   a simple copy function
 */
static long
mycopypop_andlie(
	char	*to,
	char	*from,
	long	len,
	void	*arg)
{
    if ((int)len == len) {
	xTrace3(machripcp, TR_FULL_TRACE,
	    "copy pop and lie %x to %x len %d", from, to, len);
	bcopy(from, to, len);
	return(0);
    }
    else {
	xTrace1(machripcp, TR_ERRORS,
	    "Untenable len for copy function %ld", len);
    }
    return((long)0);
}

static long
mycopypush(
	char	*from,
	char	*to,
	long	len,
	void	*arg)
{
    if ((int)len == len) {
	xTrace3(machripcp, TR_FULL_TRACE,
	    "copy push %x to %x len %d", from, to, len);
	bcopy(from, to, len);
	return(len);
    }
    else {
	xTrace1(machripcp, TR_ERRORS,
	    "Untenable len for copy function %ld", len);
    }
    return((long)0);
}


/* free ool data in msg */
static long
free_ool(
	char		*to,
	struct ool_list	*ools,
	long		len,
	void		*arg)
{
    int	i;
    int	num_ools = (int)arg;

    for (i=0; i < num_ools; i++) {
	vm_deallocate(mach_task_self(),
	    (vm_address_t)ools[i].data, ools[i].size);
    }
    return len;
}


/*
 *    free function for use with msgConstructInplace
 */
static void
x_free(
	char *ptr,
	int size)
{
    xFree(ptr);
}

Map	xkMsgIdMap;

/* forward declarations */
xkern_return_t
	machripc_init(XObj self);

static XObj
	machr_open(XObj self, XObj hlp, XObj hlptype, Part *P);

static void
	machr_session_init(XObj p, Mach_ActiveId *state);

/* external  */
xkern_return_t
	portmaint_init(XObj self);

/* external */
xkern_return_t
	quick_netport_lookup(mnportid_t portid, mnetport **portdesc);

/* external */
mnetport
	convert_to_netport(
	    mach_port_right_t port, mach_port_type_t right,
	    IPhost dest, msg_id_t msgid, mnportid_t pnn);

/* external */
mach_port_t
	convert_netport_to_mach_port(
	    mn_netport_t *netport, XObj self, XObj lower_session,
	    Msg *msg, msg_id_t msgid, IPhost sender);

/* external */
mach_port_t
	portm_fast_send_once(mnportid_t nnum, IPhost receiver);

/* external */
xkern_return_t
	portm_port_remove_send_once(mnetport *);

void	port_mgmt(mn_arch_tag_t, Msg *);

/* external */
void	portm_use_netnum(mach_port_t port, mnportid_t port_number);

static mach_msg_header_t *
	convert_netmach_msg_to_actual_mach(
	    Msg *netmsg, mach_msg_header_t *outmsg, int port_count,
	    int notinline_count, int xfercompletion,
	    mn_arch_tag_t arch, msg_id_t msgid, IPhost sender,
	    mach_port_right_t lright, boolean_t rpc_p );

static void
	convert_mach_msg_to_netmach_msg(
	    struct mach_msg_big_t *netmsg, Msg *xkmsg, machnetipc_hdr *outmsg,
	    boolean_t reuse_outmsg, mnetport *remote_port, IPhost dest,
	    enum MACHIPCTYPE msgtype);

void	machr_msg_receive(struct send_request *args);

static void
	machr_notification_receive(void);

void	machnetipc_start_msg_receive( struct send_request *args );

static void
	machr_req_dn_notif(mach_port_t port);

static void
	machr_forward_rpc_reply(struct send_request *, int);

static msg_id_t
	msgid_assign(void);

static void
	mach_port_retry_send(Event *, void *);

/* external  */
mnetport *
	quick_port_convert(mach_port_t *local_port);

/* external  */
xkern_return_t
	port_maint_init(XObj self);

/* external  */
xkern_return_t
	portm_register(IPhost );

/* external  */
xkern_return_t
	portm_register_via_session(XObj );

/* external */
mach_msg_header_t *
	xk_ports_and_ool_convert(
	    Msg *netmsg, int mach_netport_count, int mach_notline_count,
	    msg_id_t msgid, IPhost srchost, int xfercompletion);

mach_msg_header_t *
	xk_netmach_msg_to_mach(
	    Msg *netmsg, int mach_netport_count, int mach_notline_count,
	    msg_id_t msgid, IPhost srchost, int xfercompletion,
	    mn_arch_tag_t arch_type);

void	xk_mach_msg_convert (
	    struct mach_msg_big_t *msg, Msg *fulloutmsg,
	    machnetipc_hdr *nethdr, IPhost dest);

void	portm_delete_local_receive( mach_port_t port );

void	portm_delete_local_send( mach_port_t port );

static void
	machr_pop( XObj, Msg *);

static void
	send_port_block(mnetport *pd, IPhost sender,

enum MACHIPCTYPE block_type);

static void
	port_block(mnetport *port);

void	port_unblock(mnetport *port);

void	portm_generate_nms(mnetport *);

mnportid_t
	portm_get_netnum();

void	portm_use_fast_send_once(mach_port_t, boolean_t);

void	portm_add_sender(mnetport *, IPhost);

mach_port_t
	portm_quick_so_lookup( mnportid_t );

mnetport
	portm_move_netnum( mach_port_t,  mnportid_t );


#ifdef MNEVENTDB
extern	traceevent;
int	savedevent;
#endif

#ifdef PORTLOCKS
/*
 *  unlocker
 *
 *       pop a port descriptor address and unlock the port
 */
static long
unlocker(
	char	*to,
	char	*from,
	long	len,
	void	*arg)
{
    mnetport	*lkport;
    mnportid_t	pnum;
    int		num_ports = (int)arg;

    xTrace0(machripcp, TR_FULL_TRACE, "machnetipc: unlocker called");

    while (num_ports--) {
	bcopy(from, (char *)&pnum, sizeof (mnportid_t));
	if (quick_netport_lookup(pnum, &lkport) != XK_FAILURE) {
	    readerUnlock(lkport->rwlock_ptr);
	    xTrace3(machripcp, TR_EVENTS,
		"machnetipc: unlocker called with port %x (readers = %d) (writers=%d)",
		lkport->net_port_number,
		lkport->rwlock.activeReaders,
		lkport->rwlock.activeWriter);
	}
	from += sizeof(mnportid_t);
    }
    xTrace0(machripcp, TR_FULL_TRACE, "machnetipc: unlocker exits");
    return len;
}
#endif	/* PORTLOCKS */

/*
 * xAsyncThread
 *
 * platform dependent asynchronous thread startup
 * the thread cannot perform any xkernel operations until
 * it sets the xk_master_lock
 *
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
	"machripc created client thread_id: %d", child);
}

/* 
 * machripc_init()
 *
 * establish communication with CHAN, or the first lower transport
 *    protocol.
 *
 */
xkern_return_t
machripc_init(
	XObj	self)
{
    IPhost ipaddr, nametoip();
    Part whom[1];
    kern_return_t ret;

    xTrace0(machripcp, TR_FULL_TRACE, "machr init");
    xTrace0(machripcp, TR_MAJOR_EVENTS, "machr init");
    myProtl = self;
    getproc(myProtl);
    self->state = TYPE_MALLOC(struct machr_state);
    /*
     * should do this for each lower protocol?
     */
    xControl(xGetDown(myProtl, MNI_TRANSPORT_DNUM),
	GETMYHOST, (void *)&ipaddr, sizeof(IPhost) );

    ((struct machr_state *)self->state)->local_source_addr = ipaddr;
    partInit(whom, 1);

    if (pass_open_transport(whom) == XK_FAILURE) {
	xTrace0(machripcp, TR_ERRORS,
	    "machr server can't openenable lower protocol");
	_exit(0);  /* for intial debugging only */
    }

    /*
     * make sure ssr gets started OK; a kludge that could be removed
     */
    semInit(&(STATESELFPTR->ssr_wait), 0);

    bzero((char *)&Null_Netport, sizeof(Null_Netport));

    /*
     * get a port to use for notifications
     */
    if ((ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
	    &(STATESELFPTR->notification_port)))
	!= KERN_SUCCESS) {
	xTrace0(machripcp, TR_ERRORS,
	    "Machripc: Could not initialize notification port; bailing out");
	return(XK_FAILURE);
    }
    else {
	xAsyncThread(
	    (cthread_fn_t)machr_notification_receive,
	    0, "xk_notification_thread");
    }

    /*
     * used to note which msgs need to have transfer bits set in hdr
     */
    xkMsgIdMap = mapCreate(MACHIPC_MSG_MAP, sizeof(MsgId));

    portmaint_init(self);

    xTrace0(machripcp, TR_MAJOR_EVENTS, "machr init done");
    return(XK_SUCCESS);
}

/*
 * machr_new_sessn
 *
 * create a session structure so that we can xDemux the message
 */
static XObj
machr_new_sessn(
	XObj	self,
	XObj	hlp)
{
    XObj	s;

    xTrace0(machripcp, TR_FULL_TRACE, "machr_new_sessn");

    /*
     * need only one session for SSR
     */
    s = xCreateSessn((void (*)(XObj))0, hlp, hlp, self, 0, (XObj *)0);
    if ( xIsSession(s) ) {
	machr_session_init(s, (Mach_ActiveId *)0);
    }
    return s;
}

/*
 * calldemux_msg
 * 
 * This data structure is a hack to pass an argument from calldemux
 *  to demux, to machnetipc_netrcv
 * 
 */
static Msg	*calldemux_msg;

/*
 *   machnetipc_netrcv()
 *
 *   A mach message type has been received from the net.
 *
 *   If it is non-rpc, we convert the ports and data for forwarding.
 *   If it is a rpc_request, we convert and forward, but the listening
 *       thread on the reply port will be used to signal a quick return
 *       from the calldemux, using the reply message.
 *   If it is an rpc_reply, we are returning to an xCall, and this
 *       is the reply message.  We know the reply port already, so
 *       all we need to do is convert and forward.  Deallocates the
 *       send_once right netport structure for the reply.
 *
 *   Will wait for queue to unblock, if necessary.
 *   On incoming, will need to check msgid to see if it completes
 *   any port transfers that are in the queue forwarding state.
 *
 *   For rpc requests, the reply port will be a fast send-once
 *   right.  The caller must do all fast send-once right manipulations;
 *   this routine will stay out of the way.
 *
 */
static xkern_return_t
machnetipc_netrcv(
	Msg      		*msg,
	mnetport		*netport,
	enum SOURCE_BYTE_ARCH	arch_type,
	enum MACHIPCTYPE	msgtype,
	boolean_t      		isrpc_request,
	mnportid_t		*rpc_reply_port,
	mach_msg_header_t	**machmsg,
	XObj      		self,
	XObj      		transport_s)
{
    mach_port_t		receive_port = MACH_PORT_NULL;
    mach_port_type_t	local_right = 0;
    struct machnetipc	mach_net_msg;
    mach_msg_header_t	mach_header;
    Msg			*reply_msg = 0;
    msg_id_t		msgid;
    IPhost		sender;
    void		(*convec[XK_DATA_TYPE_MAX])();
    boolean_t		isrpc_reply =
			    (msgtype == MACHRIPC_RPC_REPLY_MSG ? TRUE : FALSE);
#ifdef NOVCACHE
    static mnetport	*prev_mnd = 0;
    static unsigned int	prev_netnum = 0;
    static XObj		prev_transport_s = ERR_XOBJ;
    static IPhost	prev_sender = IP_NULL;
#endif

    CLOCK_TRACE("machnetipc_netrcv: entry");
    xTrace2(machripcp, TR_FULL_TRACE,
	"machnetipc: netrcv called: port addr %x num %x",
	netport, netport ? netport->net_port_number : 0);

    set_convert_vector(arch_type, (convert_func_t *)convec);

#ifdef NOVCACHE
    if (transport_s != prev_transport_s) {
	xControl(transport_s, GETPEERHOST, (char *)&sender, sizeof(IPhost));
	prev_sender = sender;
	prev_transport_s = transport_s;
	xDuplicate(transport_s);
    }
    else {
	sender = prev_sender;
    }
#else
    xControl(transport_s, GETPEERHOST, (char *)&sender, sizeof(IPhost));
#endif

    if (msgLen(msg) < sizeof(struct machnetipc)) {
	xTrace0(machripcp, TR_DETAILED, "machnetipc: netrcv: null msg");
	return(XK_FAILURE);
    }

    /* get the ports, the message description */
    msgPop(msg, (MLoadFun)machnetipc_pop, &mach_net_msg,
	sizeof(struct machnetipc), convec);
    msgid = mach_net_msg.message_id;

    xTrace2(machripcp, TR_EVENTS,
	"machnetipc: Got message: XPort = %x; RPort = %x",
	mach_net_msg.transport_port.net_port_number,
	mach_net_msg.reply_port.net_port_number);

    xTrace3(machripcp, TR_FUNCTIONAL_TRACE,
	"machnetipc: netrcv: msgtype %d msgarch %x id %d",
	msgtype, arch_type, msgid);

    /* ZZZ : following is a nonsense check ! */
    if (isrpc_reply && msgtype != MACHRIPC_RPC_REPLY_MSG) {
	xTrace2(machripcp, TR_ERRORS,
	    "machnetipc: netrcv: type mismatch - expected %x got %x",
	    MACHRIPC_RPC_REPLY_MSG, msgtype);
	return XK_FAILURE;
    }

    /*
     * check the port this is trying to come in on
     */
    if ( isrpc_reply ) {
	/* ZZZ : netport is NULL when it's called from machr_demux ?? */
	if (mach_net_msg.transport_port.net_port_number
	    != netport->net_port_number) {
	    xTrace4(machripcp, TR_ERRORS,
		"machrnetipc: netrcv: received machripc message on bogus netport %x hex (%d dec), expected %x hex (%d dec)",
		mach_net_msg.transport_port.net_port_number,
		mach_net_msg.transport_port.net_port_number,
		netport->net_port_number,
		netport->net_port_number );
	    return XK_FAILURE;
	}
	/*
	 * the real Mach port will be filled in by the caller
	 */
    }
    else {
#ifdef NOVCACHE
	mnetport *mnd;

	if (mach_net_msg.transport_port.net_port_number == prev_netnum) {
	    mnd = prev_mnd;
	}
	else {
	    prev_netnum = mach_net_msg.transport_port.net_port_number;
	    quick_netport_lookup(
		mach_net_msg.transport_port.net_port_number, &mnd);
	    prev_mnd = mnd;
	}
	receive_port = mnd->real_send_port;
	local_right = mach_net_msg.reply_port.net_port_rights;
	mach_header.msgh_remote_port = receive_port;
#endif	/* NOVCACHE */

	if ((receive_port = portm_quick_so_lookup(
		mach_net_msg.transport_port.net_port_number))
	    != MACH_PORT_NULL) {
	    local_right = MACH_PORT_TYPE_SEND_ONCE;
	    mach_header.msgh_remote_port = receive_port;
	}
	else {

/*
	    if ((quick_netport_lookup(
		    mach_net_msg.transport_port.net_port_number, &mnd)
		== XK_SUCCESS)
		&& mnd->real_send_port != MACH_PORT_NULL) {
		receive_port = mnd->real_send_port;
		local_right = mach_net_msg.reply_port.net_port_rights;
		mach_header.msgh_remote_port = receive_port;
		/*
		 * if this is a send once right, we should remove it from
		 * the portmaint maps now
		 *
	    }
	    else {
		xTrace2(machripcp, TR_ERRORS,
		    "machnetipc: netrcv: received machripc message on bogus netport %x hex (%d dec)",
		mach_net_msg.transport_port.net_port_number,
		mach_net_msg.transport_port.net_port_number);
		return XK_FAILURE;
		/*
		 * probably need to deallocate a few things? 
		 *
	}
*/

	    CLOCK_TRACE(
		"netrcv: C convert_netport_to_mach_port for transport_port");
	    if ((receive_port = convert_netport_to_mach_port(
		    &mach_net_msg.transport_port, self,
		    ERR_XOBJ, (Msg *) 0, msgid, sender))
		== MACH_PORT_NULL) {
		xTrace2(machripcp, TR_ERRORS,
		    "machnetipc: netrcv: received machripc message on bogus netport %x hex (%d dec)",
		    mach_net_msg.transport_port.net_port_number,
		    mach_net_msg.transport_port.net_port_number);
		return XK_FAILURE;
		/*
		 * probably need to deallocate a few things?
		 */
	    }
	    CLOCK_TRACE("netrcv: R convert_netport_to_mach_port");
	}
    }

    if (!isrpc_reply) {
	local_right = mach_net_msg.reply_port.net_port_rights;
	mach_header.msgh_remote_port = receive_port;

	/* HASNAIN:
	 * remove the send once port - we have consumed the right
	 */
	if (mach_net_msg.transport_port.net_port_rights
		& MACH_PORT_TYPE_SEND_ONCE) {
	    mnetport *netport_ptr;

	    if (quick_netport_lookup(
		    mach_net_msg.transport_port.net_port_number, &netport_ptr)
		!= XK_FAILURE) {
		xTrace1(machripcp, TR_EVENTS,
		    "machripc: removing send once reply port %x from database",
		    mach_net_msg.transport_port.net_port_number);
		portm_port_remove_send_once(netport_ptr);
	    }
	}
    }
    if (isrpc_request) {	/* incoming fast send-once */
	mach_header.msgh_local_port =
	    portm_fast_send_once(
		mach_net_msg.reply_port.net_port_number, sender);
	local_right = MACH_PORT_TYPE_SEND_ONCE;
	*rpc_reply_port = mach_net_msg.reply_port.net_port_number;
    }
    else if (isrpc_reply) {
	mach_header.msgh_remote_port = MACH_PORT_NULL;
	mach_header.msgh_local_port = MACH_PORT_NULL;
    }
    else {
	/*
	 * sets up listener, if needed
	 */
	CLOCK_TRACE("netrcv: C convert_netport_to_mach_port for reply_port");
	mach_header.msgh_local_port = 
	    convert_netport_to_mach_port(&mach_net_msg.reply_port,
		self, isrpc_request ? transport_s : ERR_XOBJ,
		reply_msg, msgid, sender);
    }

    *machmsg = convert_netmach_msg_to_actual_mach(
	msg,
	&mach_header,
	mach_net_msg.netport_count,
	mach_net_msg.notinline_count,
	(mach_net_msg.sequence_num & MNSEQMASK),
	arch_type,
	msgid,
	sender,
	local_right,
	FALSE);
    CLOCK_TRACE("machnetipc_netrcv: exit");
    return XK_SUCCESS;
}


/*
 * machr_demux
 *
 * lower protocol will call this with message from net
 *
 *  Machripc must convert the entire message from network mach to real mach.
 *  It must also forward the data to the appropriate receive right.
 * 
 * 
 *  The entire converted message is xPop'd as an xkernel message
 *  to SSR, and SSR will remove its header and forward the remainder
 *  on to the registered server task, using mach ipc.
 *
 *
 */
static xkern_return_t
machr_demux(
	XObj	self,
	XObj	transport_s,
	Msg	*msg)
{
    XObj		up_session;
    mach_port_t		net_port;
    long		n;
    boolean_t		mret;
    boolean_t		isrpc_req = FALSE;
    struct mnipchdr	hdr;
    enum SOURCE_BYTE_ARCH	arch_type;
    enum MACHIPCTYPE	msg_type;
    IPhost		sender;
    Msg			*reply_msg = 0;
    mnportid_t		rpc_reply_port;
    unsigned		int opts = SEND_OPTS;
    void		(*convec[XK_DATA_TYPE_MAX])();

    xTrace0(machripcp, TR_MAJOR_EVENTS, "machr demux");

    n = msgLen(msg);
    if (n >= (sizeof(hdr))) {
	msgPop(msg, (MLoadFun)machnetipc_msg_pop, (void *)&hdr,
	    (long)sizeof(hdr), (void *)0);
	arch_type = hdr.architecture_type;
	msg_type = hdr.machnetipcmsg_type;

	/*
	 * might as well just have one for each architecture type all ready
	 */
	set_convert_vector(arch_type, (convert_func_t *)convec);

	xTrace3(machripcp, TR_DETAILED,
	    "machr demux %ld bytes of msg; type %x arch type %x",
	    n, msg_type, arch_type);
    }
    else {
	msg_type = MACHRIPC_NULL_MSG;
    }

    switch(msg_type) {
	case MACHRIPC_NULL_MSG: {
	    xTrace0(machripcp, TR_EVENTS, "machripc: machr_demux: null msg");
	    return XK_SUCCESS;
	    break;
	}

	case SSR_MSG: {
	    xTrace0(machripcp, TR_EVENTS, "machr demux SSR");
	    if (up_session = STATESELFPTR->ssr_service_session) {
		struct machripc_msg	local_buffer;

		n = msgLen(msg);
		mret = msgPop(msg, (MLoadFun)ssrpop,
		    (void *)&local_buffer.content.ssr_hdr,
		    (long)sizeof(local_buffer.content.ssr_hdr), convec);
		/*
		 * SSR messages carry a mach reply port, only
		 */
		xTrace2(machripcp, TR_FULL_TRACE,
		    "machr demux length %d return code %d", n,  mret);
		xControl(transport_s, GETPEERHOST,
		    (char *)&sender, sizeof(IPhost));
		if (local_buffer.content.ssr_hdr.xfer_completion) {
		    srxTransferComplete(sender,
			local_buffer.content.ssr_hdr.msgid);
		}
		if ((net_port = 
			convert_netport_to_mach_port(
			    &local_buffer.content.ssr_hdr.netreplyport,
			    self, ERR_XOBJ, (Msg *)0,
			    local_buffer.content.ssr_hdr.msgid, sender))
		    != MACH_PORT_NULL) {
		    /*
		     * this will transfer the port name via xKernel IPC.
		     * This does not do Mach port transfers; this protocol and
		     * the upper one MUST be in the same Mach task
		     */

		    xTrace1(machripcp, TR_FULL_TRACE,
			"machr demux reply port %x\n", net_port);
		    /*
		     * push the architecture type - need it in
		     * ssr to interpret more network data
		     */
		    msgPush( msg, (MStoreFun)mycopypush, (void *)&arch_type,
			(long)sizeof(enum SOURCE_BYTE_ARCH), (void *)0);

		    /*
		     * push the local port
		     */
		    msgPush(msg, (MStoreFun)mycopypush, (void *)&net_port,
			(long)sizeof(mach_port_t), (void *)0);
		    machr_pop(up_session, msg);
		    return XK_SUCCESS;
		}
		else {
		    xTrace0(machripcp, TR_ERRORS,
			"machr_demux error in port conversion");
		    return XK_FAILURE;
		}
	    }
	    else {
		xTrace0(machripcp, TR_EVENTS,
		    "machr: no SSR protocol available yet");
		semWait(&(STATESELFPTR->ssr_wait));
		xTrace0(machripcp, TR_EVENTS,
		    "machr: SSR protocol available now");
		return XK_SUCCESS;
	    }
	    break;
	}

	case MACHRIPC_RPC_REPLY_MSG: {
	    xTrace0(machripcp, TR_EVENTS,
		"machripc demux MACHRIPC_RPC_REPLY_MSG");
	    goto msg_processing;
	    break;
	}

	case MACHRIPC_RPC_REQUEST_MSG: {
	    xTrace0(machripcp, TR_EVENTS,
		"machripc demux MACHRIPC_RPC_REQUEST_MSG");
	    isrpc_req = TRUE;
	    reply_msg = calldemux_msg;
	    opts = RPC_OPTS;
	    calldemux_msg = (Msg *)NULL;
	    /*
	     * now fall through
	     */
	}

	case MACHRIPC_MSG: {
	    xTrace0(machripcp, TR_EVENTS, "machripc demux MACHRIPC_MSG");
msg_processing:
	    {
		mach_msg_header_t	*machmsg;

		if (machnetipc_netrcv(
			msg, (mnetport *)NULL, arch_type, msg_type,
			isrpc_req, &rpc_reply_port, &machmsg, self, transport_s)
		    != XK_FAILURE) {
		    mach_msg_return_t retm;
		    mach_port_t reply_port = machmsg->msgh_local_port;

		    xTrace2(machripcp, TR_EVENTS,
			"Sending Mach message; Rem Port = %x, Local Port = %x",
			machmsg->msgh_remote_port, machmsg->msgh_local_port);

		    xk_master_unlock();
		    /*
		     * if this times out on send, we must block the port
		     */
		    CLOCK_TRACE(
			"machr_demux: sending mach message, then receive");
		    if ((retm = mach_msg_send_rcv(machmsg))
			      == MACH_MSG_SUCCESS ||
			 retm == MACH_RCV_TOO_LARGE ) {
			CLOCK_TRACE("machr_demux: received reply message");
			if (isrpc_req && reply_port != MACH_PORT_NULL) {
			    struct send_request	rpcargs;

			    bzero((char *)&rpcargs,
				sizeof(struct send_request));
			    rpcargs.port = reply_port;
			    rpcargs.deallocate = FALSE;
			    rpcargs.reply_msg = reply_msg;
			    rpcargs.netport = &Null_Netport;
			    rpcargs.validity = TRUE;
			    rpcargs.lower_session = transport_s;
			    rpcargs.port_net_number = rpc_reply_port;
			    rpcargs.self = self;
			    xk_master_lock();

			    if (retm != MACH_RCV_TOO_LARGE)
				msgConstructInplace(
				    rpcargs.reply_msg, (char *)machmsg,
				    ROUND4(machmsg->msgh_size) +
					MAX_TRAILER_SIZE,
				    (MsgCIPFreeFunc)x_free);
			    msgSetAttr(rpcargs.reply_msg, 0, 0, 0);
			    /*
			     * msgConstructAllocate(
			     *	rpcargs.reply_msg,
			     *	sizeof(struct mach_msg_big_t),
			     *	(char **)&rpcargs->msg);
			     */
			    rpcargs.msg = (struct mach_msg_big_t *)machmsg;
			    /*
			     * xk_master_unlock();
			     */
			    CLOCK_TRACE("machr_demux: C machr_req_dn_notif");
			    machr_req_dn_notif(reply_port);
			    CLOCK_TRACE("machr_demux: R machr_req_dn_notif");
			    /*
			     * machr_msg_receive(rpcargs);
			     */
			    machr_forward_rpc_reply(&rpcargs,
				retm==MACH_RCV_TOO_LARGE ? 1 : 0);
			}
			else {
			    xk_master_lock();
			    xFree((char *)machmsg);
			}

			xTrace2(machripcp, TR_EVENTS,
			    "machr_demux: returning null msg for port %x, remote port %x",
			    rpc_reply_port, machmsg->msgh_remote_port);

			return XK_SUCCESS;
		    }
		    else {
			xk_master_lock();
			if (retm == MACH_SEND_TIMED_OUT) {
			    mnetport *pd;
			    struct port_block pb;
			    struct retry_send *retry_arg;

			    pd = quick_port_convert(&machmsg->msgh_remote_port);
			    pb.phdr.architecture_type = MN_ARCH_MARKER;
			    pb.phdr.machnetipcmsg_type = PORT_BLOCK;
			    pb.portnumber = pd->net_port_number;

			    xControl(transport_s, GETPEERHOST,
				(char *)&sender, sizeof(IPhost));

			    if (msg_type == MACHRIPC_RPC_REPLY_MSG) {
				msgPush(reply_msg, (MStoreFun)mycopypush,
				    &pb, sizeof(pb), 0);
			    }
			    else {
				/*
				 * send a port block to the sender
				 */
				send_port_block(pd, sender, PORT_BLOCK);
			    }

			    /*
			     * will be freed by the retry thread
			     */
			    retry_arg = TYPE_MALLOC(struct retry_send);
			    retry_arg->pd = pd;
			    retry_arg->sender = sender;
			    retry_arg->machmsg = machmsg;

			    /*
			     * when retry_send completes,
			     * it should unblock the port
			     */
			    evDetach(evSchedule((EvFunc)mach_port_retry_send,
				(void *)retry_arg, 0));
			    return XK_SUCCESS;
			}
			xTrace4(machripcp, TR_ERRORS,
			    "machr_demux failed mach send_rcv: %x %s, remote port %x local port %x",
			    retm, mach_error_string(retm),
			    machmsg->msgh_remote_port,
			    machmsg->msgh_local_port);
		    }
		    xTrace4(machripcp, TR_ERRORS,
			"machr_demux failed mach send_rcv: %x %s, remote port %x local port %x",
			retm, mach_error_string(retm),
			machmsg->msgh_remote_port, machmsg->msgh_local_port);
		}
		else {
		    xTrace0(machripcp, TR_ERRORS,
			"machr_demux send failed netrcv");
		}
	    }
	    break;
	}

	case PORT_BLOCK: {
	    xTrace0(machripcp, TR_ERRORS, "machr_demux PORT_BLOCK msg");
	    goto block;
	}

	case PORT_UNBLOCK: {
	    xTrace0(machripcp, TR_ERRORS, "machr_demux PORT_UNBLOCK msg");
block:
	    {
		mnportid_t npid;
		mnetport *mn;

		msgPop(msg, (MLoadFun)mycopypop, &npid, MSGID_NETLEN, 0);
		if (quick_netport_lookup(npid, &mn) != XK_SUCCESS) {
		    xTrace2(machripcp, TR_SOFT_ERROR,
			"machr_demux PORT_(UN)BLOCK failure %x type %d",
			npid, msg_type);
		}
		else {
#ifdef PORTBLOCK
		    if (msg_type == PORT_BLOCK) {
			port_block(mn) ;
		    }
		    else {
			port_unblock(mn) ;
		    }
#endif	/* PORTBLOCK */
		}
	    }
	    return XK_SUCCESS;
	    break;
	}

	case PORT_MGMT_MSG: {
	    xTrace0(machripcp, TR_ERRORS, "machr_demux PORT_MGMT_MSG");
	    port_mgmt(arch_type, msg);
	    return XK_SUCCESS;
	    break;
	}

	default: {
	    xTrace1(machripcp, TR_SOFT_ERROR,
		"machr_demux unknown message type %d", msg_type);
	}

	return XK_FAILURE;
    }

    xTrace0(machripcp, TR_FULL_TRACE, "machr_demux fails");
    return XK_FAILURE;
}


/*
 *  machr_calldemux
 *
 *
 */
static xkern_return_t
machr_calldemux(
	XObj	self,
	XObj	transport_s,
	Msg	*msg,
	Msg	*return_msg)
{
    CLOCK_TRACE("machr_calldemux: entry");
    xTrace0(machripcp, TR_FULL_TRACE, "machripc calldemux called");
    calldemux_msg = return_msg;
    machr_demux(self, transport_s, msg);
    calldemux_msg = (Msg *)NULL;
    xTrace1(machripcp, TR_EVENTS,
	"machripc calldemux returns msg len %d", msgLen(return_msg));
    CLOCK_TRACE("machr_calldemux: exit");
    return XK_SUCCESS;
}


/*
 * machr_pop
 *
 * forward a message up to SSR; called from demux
 *
 */
static void
machr_pop(
	XObj	up_session,
	Msg	*msg)
{
    xTrace0(machripcp, TR_FULL_TRACE, "machr_pop");

    /*
     * remember the upward protocol must be in the same Mach task as
     * this one, because a Mach port is being sent in an
     * xkernel message
     */
    xDemux(up_session, msg);
}


/*
 * machr_openenable
 *
 *   the SSR service is the only thing that needs to open machnetipc.
 *   all other interactions are implicit through Mach port transfers.
 *
 *   we use a single session for all demux's up to the ssr service,
 *      because it only takes incoming messages, and because it
 *      replies without blocking.
 *
 */
static xkern_return_t
machr_openenable(
	XObj	self,
	XObj	hlp,
	XObj	hlptype,
	Part	*p)
{
    xTrace0(machripcp, TR_FULL_TRACE, "machr_openenable");

    if (!STATESELFPTR->ssr_service) {
	STATESELFPTR->ssr_service = hlp;
	semSignal(&(STATESELFPTR->ssr_wait));
	STATESELFPTR->ssr_service_session = machr_new_sessn(self, hlp);
	xTrace0(machripcp, TR_MORE_EVENTS, "machr_openenable returning");
    }
    else {
	xTrace0(machripcp, TR_ERRORS,
	    "machr_openenable called more than once, ignoring");
    }
    return(XK_SUCCESS);
}


/*
 * machr_push
 *
 *  This is only used by upper protocols; the more common case
 *  of outgoing data is an intercepted Mach port send 
 *
 */
static xmsg_handle_t
machr_push(
	XObj	self,
	Msg	*msg)
{
    Part		whom[2];
    IPhost		destination;
    XObj		ls;
    mnetport		outport;
    struct machripc_msg	outmsg;
    mach_port_t		reply_port = SESSNSTATEPTR->localport;
    mach_port_type_t	reply_right;
    kern_return_t	ret;

    xTrace1(machripcp, TR_FULL_TRACE, "machr_push %x", self);

    /*
     * must be from SSR, just convert port to net form and send
     * note that we don't copy the operation code; it's assumed to be REQUEST
     */
    xTrace1(machripcp, TR_FULL_TRACE,
	"machr_push incoming msg len %d", msgLen(msg));
    bzero((char *)&outmsg, sizeof(outmsg));

    if ((ret = mach_port_type(mach_task_self(), reply_port, &reply_right))
	!= KERN_SUCCESS) {
	xTrace2(machripcp, TR_ERRORS,
	    "machr_push: could not get port type for port %x failure code 0x%x",
	    reply_port, ret);
    }
    else {
	xTrace1(machripcp, TR_DETAILED,
	    "machripc: machr_push: got port right of type %x for reply port",
	    reply_right);
    }
    if (reply_right & MACH_PORT_TYPE_SEND) {
	reply_right = MACH_PORT_TYPE_SEND;
    }

    outmsg.hdr.architecture_type = MN_ARCH_MARKER;
    outmsg.hdr.machnetipcmsg_type = SSR_MSG;
    outmsg.content.ssr_hdr.msgid = msgid_assign();
    outport = convert_to_netport(
	reply_port,	/* locks the port */
	ONESEND(reply_right),
	SESSNSTATEPTR->destination_host, 
	outmsg.content.ssr_hdr.msgid, 0);
    if (mapResolve(xkMsgIdMap, &outmsg.content.ssr_hdr.msgid, 0)
	== XK_SUCCESS) {
	outmsg.content.ssr_hdr.xfer_completion = 1;
	mapUnbind(xkMsgIdMap, &outmsg.content.ssr_hdr.msgid);
    }
    if (outport.net_port_type == MN_INVALID) {
	xTrace0(machripcp, TR_ERRORS, "machr_push: port conversion failed");
	return(ERR_XK_MSG);
    }

    outmsg.content.ssr_hdr.netreplyport = *((mn_netport_t *)&outport);
    outmsg.content.ssr_hdr.netreplyport.net_port_rights =
	ONESEND(outmsg.content.ssr_hdr.netreplyport.net_port_rights);
    xTrace3(machripcp, TR_FULL_TRACE,
	"machr_push netport params: number %x rights %x type %x hostaddr.a %d",
	outmsg.content.ssr_hdr.netreplyport.net_port_number,
	outmsg.content.ssr_hdr.netreplyport.net_port_rights,
	outmsg.content.ssr_hdr.netreplyport.receiver_host_addr.a);

    msgPush(msg, (MStoreFun)ssrpush, (void *)&outmsg,
	sizeof(outmsg.content.ssr_hdr) + sizeof(struct mnipchdr), (void *)0);

    partInit(whom, 1);
    destination = SESSNSTATEPTR->destination_host;

    xTrace5(machripcp, TR_FULL_TRACE,
	"machr_push dest %d.%d.%d.%d arch %x",
	destination.a, destination.b, destination.c, destination.d, PERM1);
    partPush(whom[REMOTE_PART],
	&(SESSNSTATEPTR->destination_host), sizeof(IPhost));

    if ((ls = active_open_transport(whom)) != ERR_XOBJ) {
	xkern_return_t xkret;
	Msg rmsg, *locked_ports;

	xTrace0(machripcp, TR_EVENTS, "machr_push doing msgPush, xPush");
	xTrace1(machripcp, TR_FULL_TRACE, "machr_push net %d",
	    outmsg.content.ssr_hdr.netreplyport.receiver_host_addr.a);

	locked_ports = (Msg *)msgGetAttr(msg, 0);
	msgConstructEmpty(&rmsg);
	xkret = xCall(ls, msg, &rmsg);
	xTrace1(machripcp, TR_EVENTS,
	    "machr_push xCall return code %d", xkret);
#ifdef PORTLOCKS
	if (quick_netport_lookup(outport.net_port_number, 0) != XK_FAILURE) {
	    readerUnlock(outport.rwlock_ptr);
	}
	if (locked_ports) {
	    xTrace0(machripcp, TR_DETAILED,
		"machnetipc: machr_push: locked_ports");
	    msgPop(locked_ports, (MLoadFun)unlocker, 0, msgLen(locked_ports), 0);
	}
#endif	/* PORTLOCKS */

	/*
	 * reply should be null
	 */
	machr_demux(self, ls, &rmsg);
	xTrace0(machripcp, TR_EVENTS, "machr_push doing xClose");
	xClose(ls);
	/*
	 * msgDestroy(msg);
	 */
	return(XK_SUCCESS);
    }
    else {
	xTrace0(machripcp, TR_ERRORS,
	    "machnetipc: open of transport service for client request failed");
    }
    return(XK_FAILURE);
}


/*
 * machr_open
 *
 * Open a Mach IPC network session
 *
 *    used only by SSR code to send requests to remote server via SSR
 *    the sessions don't need to be in a map because we
 *      don't need to match incoming messages.
 */
static XObj
machr_open(
	XObj	self,
	XObj	hlp,
	XObj	hlptype,
	Part	*part)
{
    /*
     * where does this get freed ?
     */
    Mach_ActiveId	*maid = TYPE_MALLOC(Mach_ActiveId);
    XObj		newsessn;
    long		portnum;

    xTrace1(machripcp, TR_MAJOR_EVENTS, "machr_open, self %x", self);
    if ((portnum = *(long *)partPop(part[LOCAL_PART]))
	!= relProtNum(hlp->myprotl, myProtl)) {
	xTrace2(machripcp, TR_ERRORS,
	    "machr bogus participant id %d %s\n", portnum, hlp->name);
	return(ERR_XOBJ);
    }
    bzero((char *)maid, sizeof(Mach_ActiveId));
    maid->localport =  *((mach_port_t *)(partPop(part[LOCAL_PART])));
    maid->destination_host = *((IPhost *)(partPop(part[REMOTE_PART])));
    /*
     * could pop the service id off the remote part
     */
    xTrace4(machripcp, TR_FULL_TRACE,
	"machr_open %d-%d-%d-%d",
	maid->destination_host.a, maid->destination_host.b,
	maid->destination_host.c, maid->destination_host.d);
    newsessn = xCreateSessn((void (*)(XObj))0, hlp, hlp, self, 0, (XObj *)0);
    if (newsessn != ERR_XOBJ) {
	xTrace1(machripcp, TR_EVENTS, "machr_open new session %x", newsessn);
	machr_session_init(newsessn, maid);
	newsessn->state = (char *)maid;
	return(newsessn);
    }
    else {
	xTrace0(machripcp, TR_ERRORS, "machr: could not create lower session");
	return(ERR_XOBJ);
    }
}


/*
 * machr_forward_send_request()
 *
 * Intercept msg_send's destinated for remote machines;
 *  convert them to netmsgs, and forward to the appropriate
 *  remote machine.  These are ports for which the machnetipc thread holds the
 *  receive rights; it is obligated to send them on to the actual receiver
 *  on some other machine.
 *
 * The mach msg is in the buffer portion of the xkernel msg
 *
 * This must run under the xk_master_lock
 *
 */
static void
machr_forward_send_request(
	struct send_request	*args,
	int			too_big,
	boolean_t		rpc_undo)
{
    mach_msg_header_t	*msg = (mach_msg_header_t *)args->msg;
    Msg			xkmsg = args->request_msg;	/* must copy */
    mnetport		*remport = args->netport;
    mnetport		*reply_port;
    mnportid_t		remport_number;

    Part		whom[1];
    XObj		ls;
    machnetipc_hdr	outmsg;
    IPhost		iphost;
    xkern_return_t	ret;
    kern_return_t	kret;
    mach_port_t		mach_reply_port;
    mach_port_type_t	reply_right;
    mnportid_t		reply_netport_number = 0;
    boolean_t		is_rpc = FALSE;
    boolean_t		notfirst = args->outmsg.hdr.architecture_type != 0;

    xTrace0(machripcp, TR_FULL_TRACE, "enter machr_forward_send_request");
    xTrace2(machripcp, TR_DETAILED,
	"machr forward send request remport addr %x arg addr %x",
	remport, args);
    xTrace3(machripcp, TR_DETAILED,
	"machr forward send request remport %x readers = %d) (writer = %d)",
	remport->net_port_number,
	remport->rwlock.activeReaders, remport->rwlock.activeWriter);

    bzero((char *)&outmsg, sizeof(machnetipc_hdr));

    if (too_big) {
	mach_msg_return_t	mret;
	char			*newdata;
	Msg			oldmsg = args->request_msg;

	xTrace1(machripcp, TR_EVENTS,
	    "machnetipc: forward_send starting larger recv %d", msg->msgh_size);
	msgConstructAllocate(&xkmsg,
	    ROUND4(msg->msgh_size) + MAX_TRAILER_SIZE, &newdata);
	/*
	 * copy the msg header data
	 */
	bcopy((char *)msg, (char *)newdata, sizeof(mach_msg_header_t));
	msg = (mach_msg_header_t *)newdata;
	msgDestroy(&oldmsg);
	msg->msgh_size = ROUND4(msg->msgh_size) + MAX_TRAILER_SIZE;
	if ((mret = mach_msg_receive_proxy(msg)) != MACH_MSG_SUCCESS) {
	    xTrace2(machripcp, TR_EVENTS,
		"machnetipc: forward_send_request: receive failure %x %s",
		mret, mach_error_string(mret));
	    /*
	     * cleanup
	     */
	    return;
	}
    }
    else if (!rpc_undo) {
	msgTruncate(&xkmsg, msg->msgh_size+MAX_TRAILER_SIZE);
    }

    mach_reply_port = msg->msgh_remote_port;

    /*
     * First determine where to send this message
     */
    if (remport == (mnetport *)NULL) {
	xTrace2(machripcp, TR_ERRORS,
	    "machr forward send failed to find net descriptor for destination port %x, msg %x",
	    msg->msgh_remote_port, msg);
	mach_msg_destroy(msg);
	msgDestroy(&xkmsg);
	return;
    }

    if (rpc_undo) {
	xTrace0(machripcp, TR_EVENTS, "machnetipc: msg_send rpc undo");
	iphost = remport->receiver_host_addr;
	outmsg = args->outmsg;
	/*
	 * outmsg.content.machmsg.reply_port =
	 *	convert_to_netport(... msg->msgh_local_port
	 */
	outmsg.hdr.machnetipcmsg_type = MACHRIPC_MSG;
	args->reply_msg = NULL;
	goto sending_data;
    }

    remport_number = remport->net_port_number;  /* need local copy */
    if (!(remport->net_port_rights & MACH_PORT_TYPE_SEND_ONCE) &&
	!(remport->net_port_rights & MACH_PORT_TYPE_SEND)) {
	xTrace2(machripcp, TR_ERRORS,
	    "machripc forward_send_request does not hold network send right for port %x addr %x",
	    remport->net_port_number, msg->msgh_remote_port);
	mach_msg_destroy(msg);
	msgDestroy(&xkmsg);  /* side-effect xFree's msg */
	return;
    }
    iphost = remport->receiver_host_addr;

    CLOCK_TRACE("machr_forward_send_request: C mach_port_type");
    if (mach_reply_port != MACH_PORT_NULL &&
	(kret = mach_port_type(mach_task_self(), mach_reply_port, &reply_right))
	!= KERN_SUCCESS) {
	xTrace3(machripcp, TR_ERRORS,
	    "machnetipc: forward_send_request: bogus reply right %x on port %x ret %x",
	    reply_right, mach_reply_port, kret);
	mach_port_deallocate(mach_task_self(), mach_reply_port);
	mach_reply_port = MACH_PORT_NULL;
    }
    CLOCK_TRACE("machr_forward_send_request: R mach_port_type");

    xTrace1(machripcp, TR_DETAILED,
	"forward_send_request msglen %d", msgLen(&xkmsg));

    xTrace4(machripcp, TR_FULL_TRACE, 
	"machr forward send request to host %d.%d.%d.%d",
	iphost.a, iphost.b, iphost.c, iphost.d);

    /*
     * CONVERSION: will look at first two mach headers but not copy
     * this will lock the remote port and all embedded ports, except
     * the reply right if this is an rpc request
     */
    /* first time through, the MachNetIPC header (third arg) is filled;
     * thereafter most of it is re-used
     */

    convert_mach_msg_to_netmach_msg(
	(struct mach_msg_big_t *)msg, &xkmsg, &outmsg,
	notfirst, remport, iphost, 0);

    if (reply_right & MACH_PORT_TYPE_SEND_ONCE) {
	xTrace1(machripcp, TR_FUNCTIONAL_TRACE,
	    "machnetipc: forward_send_request: reply right is send once for port %x",
	    mach_reply_port);

	if (mach_reply_port != MACH_PORT_NULL) {
	    /*
	     * get a number, register sender so a reboot will kill the port
	     */
	    reply_netport_number = portm_get_netnum(mach_reply_port, iphost);
	    xTrace2(machripcp, TR_EVENTS,
		"machnetipc: forward_send_request: allocated id %x for the reply port %x",
		reply_netport_number, mach_reply_port);
	}
	outmsg.content.machmsg.reply_port.net_port_number =
	    reply_netport_number;
	is_rpc = TRUE;
    }

sending_data:
    ls = ERR_XOBJ;
    /*
     * once we have opened a session for a port, we must stick with it;
     * otherwise we will lose the sequencing
     */
    if (remport->lower_session != (XObj)ERR_XOBJ) {
	ls = remport->lower_session;
    }
    else {
	/*
	 * get ready for xOpen
	 */
	partInit(whom, 1);
	partPush(whom[REMOTE_PART], &iphost, sizeof(IPhost));
    }

    if ((ls != ERR_XOBJ) ||
	(remport->lower_session = ls =
	    xOpen(myProtl, myProtl, xGetDown(myProtl, 0), whom))
	!= ERR_XOBJ) {
	Msg			xk_return_msg;
	struct msg_attribute	*msg_attr;

	xTrace0(machripcp, TR_EVENTS,
	    "machr_forward_send_request xOpen succeeded");
	msgConstructEmpty(&xk_return_msg);	/* can we pre-compute header? */
	msgPush(&xkmsg, (MStoreFun)mycopypush, (char *)&outmsg,
	    sizeof(machnetipc_hdr), (void *)0);
	xTrace1(machripcp, TR_DETAILED,
	   "machripc pushing message size %d", msgLen(&xkmsg));

	xTrace4(machripcp, TR_EVENTS,
	    "machripc Sending message; RemotePort=%x XPort=%x; LocalPort=%x ReplyPort=%x",
	    msg->msgh_remote_port,
	    outmsg.content.machmsg.transport_port.net_port_number,
	    msg->msgh_local_port,
	    outmsg.content.machmsg.reply_port.net_port_number);

	if (outmsg.hdr.machnetipcmsg_type == MACHRIPC_RPC_REQUEST_MSG) {
	    mnetport	fast_reply_port;

	    xTrace1(machripcp, TR_DETAILED,
		"machripc pushing rpc retmsg at %x", &xk_return_msg);
	    reply_port = &fast_reply_port;
	    reply_port->net_port_number = reply_netport_number;

	    msg_attr = (struct msg_attribute *)msgGetAttr(&xkmsg, 0);

	    xTrace2(machripcp, TR_EVENTS,
		"machripc xCall: push xkmsg at %x and get return_msg at %x",
		&xkmsg, &xk_return_msg);
	    CLOCK_TRACE("machr_forward_send_request: C xCall from RPC_REQ");
	    ret = xCall(ls, &xkmsg, &xk_return_msg);
	    CLOCK_TRACE("machr_forward_send_request: R xCall");

	    /* release port locks */
	    if (msg_attr) {
#ifdef PORTLOCKS
		if (msg_attr->num_locked_ports) {
		    xTrace0(machripcp, TR_DETAILED,
			"machnetipc: machr_forward_send_request: locked_ports");
		    msgPop(&msg_attr->locked_ports, (MLoadFun)unlocker, 0,
			msg_attr->num_locked_ports * sizeof(mnportid_t),
			(void *)msg_attr->num_locked_ports);
		}
		msgDestroy(&msg_attr->locked_ports);
#endif	/* PORTLOCKS */
		if (msg_attr->num_ools) {
		    msgPop(&msg_attr->ool_list_msg, (MLoadFun)free_ool, 0,
			msg_attr->num_ools * sizeof(struct ool_list),
			(void *)msg_attr->num_ools);
		}
		msgDestroy(&msg_attr->ool_list_msg);
		xFree((char *)msg_attr);
	    }

	    if (ret == XK_FAILURE) {
		xTrace0(machripcp, TR_ERRORS, "machripc xCall failed");
	    }
	    else {
		mach_msg_header_t	*retmsg = 0;
		mach_msg_return_t	mret = 0;
		struct mnipchdr		hdr;
		boolean_t		too_short = 0;

		xTrace1(machripcp, TR_DETAILED,
		    "machr_forward_send_request returned from xCall with reply len %d",
		    msgLen(&xk_return_msg));
		if (msgLen(&xk_return_msg) >= sizeof(hdr)) {
		    msgPop(&xk_return_msg, (MLoadFun)machnetipc_msg_pop,
			(void *)&hdr, (long)sizeof(hdr), (void *)0);
		}
		else {
		    xTrace0(machripcp, TR_DETAILED,
			"machnetipc: forward_send_request: rpc reply too short");
		    too_short = 1;
		}

		if (too_short || hdr.machnetipcmsg_type == PORT_BLOCK ||
		    ((ret = machnetipc_netrcv(
			&xk_return_msg,
			reply_port,
			hdr.architecture_type,
			hdr.machnetipcmsg_type,
			FALSE, 0, &retmsg,
			args->self, ls))
		    == XK_FAILURE)) {
		    /*
		     * reply too short means RPC has been aborted
		     */
		    xTrace4(machripcp, TR_EVENTS,
			"machnetipc: forward_send: mach send failed convert code %x msgptr %x with code %x for port %x",
			ret, retmsg, mret,
			retmsg? retmsg->msgh_remote_port:MACH_PORT_NULL);
		    if (retmsg) {
			xFree((void *)retmsg);
		    }
		    /*
		     * must convert the reply port into a full netport structure
		     * this will also retract the fast-send-once registration
		     */
		    portm_move_netnum(mach_reply_port, reply_netport_number);
#ifdef PORTBLOCK
		    if (hdr.machnetipcmsg_type == PORT_BLOCK) {
			port_block(remport);
		    }
#endif	/* PORTBLOCK */

		}
		else {
		    retmsg->msgh_remote_port = mach_reply_port;
		    retmsg->msgh_local_port = MACH_PORT_NULL;
		    retmsg->msgh_bits =
			MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);

		    CLOCK_TRACE(
			"machr_forward_send_request: sending reply mach msg");
		    if ((mret = mach_msg_send_proxy(retmsg))
			!= MACH_MSG_SUCCESS) {
			xTrace2(machripcp, TR_ERRORS,
			    "machnetipc: forward_send: rpc reply message could not be delivered: code %x %s",
			    mret, mach_error_string(mret));
		    }
		}	/* end of RPC reply handling */
		portm_use_netnum(mach_reply_port, reply_netport_number);

	    }	/* end of RPC send success */
	    msgDestroy(&xk_return_msg);	/* on the stack of this thread  */
	    msgDestroy(&xkmsg);	/* allocated in machr-start-rec */
	} /* end of RPC handling */
	else {
	/*
	 * not an RPC; we send and leave, assuming that the return msg is null
	 */
	    xTrace3(machripcp, TR_EVENTS,
		"machripc: forward_send no reply, starting xCall, msg %x retmsg %x ls %x",
		&xkmsg, &xk_return_msg, ls);

	    msg_attr = (struct msg_attribute *)msgGetAttr(&xkmsg, 0);

	    CLOCK_TRACE("machr_forward_send_request: C xCall from NON RPC");
	    if (((ret = xCall(ls, &xkmsg, &xk_return_msg)) != XK_SUCCESS) ||
		(remport->local_status == MN_INVALID)) {
		xTrace2(machripcp, TR_EVENTS,
		    "machnetipc: forward_send_request: xCall fails or destination rebooted: xCall ret = %d local_status = %d",
		    ret, remport->local_status);
#ifdef PORTLOCKS
		if (quick_port_convert(&args->port) != &Null_Netport) {
		    if (remport->local_status != MN_INVALID) {
			readerUnlock(remport->rwlock_ptr);
		    }
		    else {
			xTrace1(machripcp, TR_ERRORS,
			    "machnetipc: forward_send_request: INVALID status on port %x; not unlocked",
			    remport->net_port_number);
		    }
		}
		if (mach_reply_port != MACH_PORT_NULL) {
		    reply_port = quick_port_convert(&mach_reply_port);
		}
		if (mach_reply_port != MACH_PORT_NULL &&
		    reply_port != &Null_Netport) {
		    readerUnlock(reply_port->rwlock_ptr);
		}
#endif	/* PORTLOCKS */
		remport->lower_session = ERR_XOBJ;
		remport->net_port_type = MN_INVALID;
		xClose(ls);
	    }
	    CLOCK_TRACE("machr_forward_send_request: R xCall");

	    /* release port locks */
	    if (msg_attr) {
#ifdef PORTLOCKS
		if (msg_attr->num_locked_ports) {
		    xTrace0(machripcp, TR_DETAILED,
			"machnetipc: machr_forward_send_request: locked_ports");
		    msgPop(&msg_attr->locked_ports, (MLoadFun)unlocker, 0,
			msg_attr->num_locked_ports * sizeof(mnportid_t),
			(void *)msg_attr->num_locked_ports);
		}
		msgDestroy(&msg_attr->locked_ports);
#endif	/* PORTLOCKS */
		if (msg_attr->num_ools) {
		    msgPop(&msg_attr->ool_list_msg, (MLoadFun)free_ool, 0,
			msg_attr->num_ools * sizeof(struct ool_list),
			(void *)msg_attr->num_ools);
		}
		msgDestroy(&msg_attr->ool_list_msg);
		xFree((char *)msg_attr);
	    }

#ifdef PORTLOCKS
#ifdef PORTBLOCK
	    if (msgLen(&xk_return_msg) > 0) {
		port_block(remport);
	    }
#endif	/* PORTBLOCK */
	    if (quick_port_convert(&args->port) != &Null_Netport &&
		!(remport->net_port_rights & MACH_PORT_TYPE_SEND_ONCE)) {
		xTrace2(machripcp, TR_DETAILED,
		    "machnetipc: machr_forward_send_request: unlocking remport: %x; readers = %d",
		    remport->net_port_number, remport->rwlock.activeReaders);
		readerUnlock(remport->rwlock_ptr);
	    }

	    if (mach_reply_port != MACH_PORT_NULL) {
		reply_port = quick_port_convert(&mach_reply_port);
		if (reply_port != &Null_Netport) {
		    readerUnlock(reply_port->rwlock_ptr);
		}
	    }
#endif	/* PORTLOCKS */

#ifdef PORTREFS
	    mach_msg_destroy(msg);  /* think about this ... */
#endif	/* PORTREFS */
	    /*
	     * xFree((char *)remport);
	     */
	    xTrace1(machripcp, TR_EVENTS,
		"machripc: forward_send no reply done for args %x", args);
	    msgDestroy(&xkmsg);   /* as side-effect frees msg */
	    msgDestroy(&xk_return_msg);
	}

	if ( remport->net_port_rights == MACH_PORT_TYPE_SEND_ONCE ) {
	    xTrace2(machripcp, TR_EVENTS,
		"machnetipc: convert_msg_to_net: Removing network send_once right for port number %x mach_port %x",
		remport->net_port_number, remport->real_send_port);
	    /*
	     * remove from database; less interest in old sender
	     * and deallocate the remport structure
	     */
	    xClose(remport->lower_session);
	    portm_port_remove_send_once(remport);
	}
	xTrace1(machripcp, TR_EVENTS,
	"machr_forward_send_request finished for arg %x", args);
    }
    else {
	xTrace0(machripcp, TR_ERRORS,
	    "machr_forward_send_request failed to open session to transport protocol for forward_send_rquest");
    }
}


/*
 *  machnetrpc_undo()
 *
 *
 *
 */
static void
machnetrpc_undo(
	Event			ev, 
	struct send_request	*args)
{
    xTrace0(machripcp, TR_FULL_TRACE, "machnetipc: machnetrpc_undo called");

    machr_forward_send_request(args, FALSE, TRUE);
    xFree((char *)args);
}


/*
 * machr_forward_rpc_reply()
 *
 * Intercept rpc replies,
 *  convert them to netmsgs, and forward to the appropriate
 *  remote machine.  These are ports for which the machr thread holds the
 *  receive rights; it is obligated to send them on to the actual receiver
 *  on some other machine.
 *
 * Must convert from mach message format to network and xkernel structures
 *
 * This routine is called as a result of machr_demux, which recognizes the
 * incoming RPC request.
 *
 */
static void
machr_forward_rpc_reply(
	struct send_request	*rpcargs,
	int			too_big)
{
    mach_msg_header_t	*msg = (mach_msg_header_t *)rpcargs->msg;
    mnetport		remport;

    xTrace1(machripcp, TR_FULL_TRACE,
	"machripc: machr_forward_rpc_reply called, msg ptr %x", msg);

    if (!rpcargs->validity) {
	xTrace0(machripcp, TR_ERRORS, "machnetipc rpc session invalid");
	return;
    }

    if (too_big) {
	Msg			newmsg;
	mach_msg_return_t	mret;
	char			*newdata;

	xTrace1(machripcp, TR_EVENTS,
	    "machnetipc: forward_rcp_reply starting larger recv %d",
	    msg->msgh_size);
	msgConstructAllocate(&newmsg,
	    ROUND4(msg->msgh_size) + MAX_TRAILER_SIZE, &newdata);
	bcopy((char *)msg, newdata, sizeof(mach_msg_header_t));
	msg = (mach_msg_header_t *)newdata;
	rpcargs->msg = (struct mach_msg_big_t *)newdata;
	*rpcargs->reply_msg = newmsg;
	msg->msgh_size = ROUND4(msg->msgh_size) + MAX_TRAILER_SIZE;
	if ((mret = mach_msg_receive_proxy(msg)) != MACH_MSG_SUCCESS) {
	    xTrace2(machripcp, TR_EVENTS,
		"machnetipc: forward_rpc_reply: receive failure %x %s",
		mret, mach_error_string(mret));
	    msgDestroy(&newmsg);
	    return;
	}
    }
    else {
	msgTruncate(rpcargs->reply_msg, msg->msgh_size + MAX_TRAILER_SIZE);
    }

    remport = Null_Netport;
    xControl(rpcargs->lower_session, GETPEERHOST,
	(char *)&remport.receiver_host_addr, sizeof(IPhost));

    /*
     * this sets up the machnetipc header, converts msg, attaches port locks
     */
    convert_mach_msg_to_netmach_msg(
	(struct mach_msg_big_t *)msg,
	rpcargs->reply_msg,
	&rpcargs->outmsg,
	FALSE,
	&remport,
	remport.receiver_host_addr,
	MACHRIPC_RPC_REPLY_MSG);
    xTrace0(machripcp,TR_FULL_TRACE,
	"machripc forward_rpc_reply converted msg");
    xTrace1(machripcp,TR_DETAILED,
	"machr forward_rpc_reply size %d", msg->msgh_size);

    rpcargs->outmsg.content.machmsg.transport_port.net_port_number =
	rpcargs->port_net_number;

    /*
     * if there are message attributes then we need do some cleaning up
     * e.g. ool data or locked ports
     */
    if (msgGetAttr(rpcargs->reply_msg, 0) != NULL
#ifdef PORTLOCKS
	|| msg->msgh_remote_port != MACH_PORT_NULL
#endif	/* PORTLOCKS */
	) {
	struct send_request	*new_rpcargs;
	mnetport		tmp_netport;

	/*
	 * have to abort the rpc and do an ordinary send operation
	 */
	xTrace0(machripcp, TR_ERRORS,
	    "machnetipc: rpc_reply: has locked ports");

	xTrace1(machripcp,TR_DETAILED,
	    "machr forward_rpc_reply size %d", msgLen(rpcargs->reply_msg));

	msgConstructCopy(&rpcargs->request_msg, rpcargs->reply_msg);
	msgTruncate(rpcargs->reply_msg, 0);
	rpcargs->reply_msg = NULL;
	rpcargs->lower_session = ERR_XOBJ;
	rpcargs->deallocate = TRUE;
	xTrace2(machripcp, TR_EVENTS,
	    "machr forward_rpc_reply moving local port %x to remote id %x",
	    msg->msgh_local_port,
	    rpcargs->outmsg.content.machmsg.transport_port.net_port_number);

	tmp_netport =
	    portm_move_netnum(
		msg->msgh_local_port,
		rpcargs->outmsg.content.machmsg.transport_port.net_port_number);
	rpcargs->outmsg.content.machmsg.transport_port =
	    *(mn_netport_t *)&tmp_netport;
	quick_netport_lookup(
	    rpcargs->outmsg.content.machmsg.transport_port.net_port_number,
	    &rpcargs->netport);

	rpcargs->netport->receiver_host_addr = remport.receiver_host_addr;

	new_rpcargs = TYPE_MALLOC(struct send_request);
	*new_rpcargs = *rpcargs;
	evDetach(evSchedule((EvFunc)machnetrpc_undo, (void *)new_rpcargs, 0));
	return;
    }

    /*
     * push the machnetipc header onto it
     */
    msgPush(rpcargs->reply_msg, (MStoreFun)mycopypush,
	(char *)&rpcargs->outmsg, sizeof(machnetipc_hdr), (void *)0);
    xTrace1(machripcp, TR_FULL_TRACE,
	"machripc forward_rpc_reply constructed msg size %d",
	msgLen(rpcargs->reply_msg));

    if (msg->msgh_remote_port != MACH_PORT_NULL)
	portm_use_fast_send_once(msg->msgh_remote_port, TRUE);
    xTrace0(machripcp, TR_FULL_TRACE, "machripc: forward_rpc_reply exits");
}


/*
 *  machr_req_dn_notif
 *
 *     request a dead name notification for the port
 *
 */
static void
machr_req_dn_notif(
	mach_port_t	port)
{
    mach_port_t		previous_port;
    kern_return_t	kret;

    kret = mach_port_request_notification(
	mach_task_self(),
	port,
	MACH_NOTIFY_DEAD_NAME,
	0,
	STATEPTR->notification_port,
	MACH_MSG_TYPE_MAKE_SEND_ONCE,
	&previous_port);
    if (kret != KERN_SUCCESS) {
	xTrace1(machripcp, TR_ERRORS,
	    "machnetipc: machr_req_dn_notif: failed %x", kret);
    }
}


/*
 *  machr_handle_notification()
 *
 *    A send or send-once right has gone away
 *
 */
static void
machr_handle_notification(
	mach_port_t	port,
	mach_msg_id_t	not_type)
{
    if (not_type != MACH_NOTIFY_DEAD_NAME ) {
	xTrace1(machripcp, TR_ERRORS,
	    "machr_handle_notification called with type %x; no action",
	    not_type);
	return;
    }
    xTrace0(machripcp, TR_ERRORS,
	"machr_handle_notification port deleted or dead");
    /*
     * may be local receiver died; must start protocol to notify
     * the senders, delete local structures
     */
    xk_master_lock();
    evDetach(evSchedule((EvFunc)portm_delete_local_receive, (void *)port, 0));
    xk_master_unlock();
}


/*
 * machr_notification_receive()
 *
 * Receives local notifications about ports managed by machnetipc.
 * Is started by init, runs forever.
 *
 */
static void
machr_notification_receive( void )
{
    mach_msg_return_t			ret;
    /*
     * should be only dead_name
     */
    mach_dead_name_notification_t	smsg;

    xTrace1(machripcp, TR_FULL_TRACE,
	"machr_notification_receive starting on port %x",
	STATEPTR->notification_port);

    smsg.not_header.msgh_local_port = STATEPTR->notification_port;
    /*
     * ZZZ : mach_dead_name_notification_t has NDR and includes trailer
     */
    smsg.not_header.msgh_size = ROUND4(sizeof(mach_dead_name_notification_t));

    while ((ret = mach_msg_receive(&smsg)) == MACH_MSG_SUCCESS) {
	xTrace2(machripcp, TR_EVENTS,
	    "machnetipc: machr_notification_receive received notification id %x for port %x",
	    smsg.not_header.msgh_id, smsg.not_port);

	machr_handle_notification(smsg.not_port, smsg.not_header.msgh_id);
	smsg.not_header.msgh_local_port = STATEPTR->notification_port;
	smsg.not_header.msgh_size =
	    ROUND4(sizeof(mach_dead_name_notification_t));
	xTrace0(machripcp, TR_FUNCTIONAL_TRACE,
	    "machnetipc: notification read");
    }
    xTrace1(machripcp, TR_ERRORS, "error from machr mach msg recv %x", ret);
    return;
}


/*
 *
 * machnetipc_start_msg_receive (args)
 *
 *       primary job is to malloc storage for the message and start
 *       an asynchronous thread to do the listening.  It also sets
 *       up notifications.
 *
 *       Entry invariant: args->request_msg is free for use.
 *       Exit invariant: args->request_msg is a valid xkernel msg;
 *		       args->msg is a pointer to a buffer for
 *		       mach_msg_receive and is the xkernel buffer
 *		       area of args->request_msg
 *       Storage: args is re-used until the port dies.
 *
 */
void
machnetipc_start_msg_receive(
	struct send_request	*args)
{
    mach_port_t		previous_port = MACH_PORT_NULL;
    mach_port_t		port = args->port;
    mach_port_mscount_t	sync = 0;
    kern_return_t ret;

    /* the message space will be freed by forward_send_request */

    if (args->ask_for_dead_name) {
	machr_req_dn_notif(port);
	args->ask_for_dead_name = 0;
    }

    if (args->ask_for_nms) {
	xTrace1(machripcp, TR_FULL_TRACE,
	    "machnetipc: requesting nms notification on port %x", port);
	if ((ret = mach_port_request_notification(
		mach_task_self(),
		port,
		MACH_NOTIFY_NO_SENDERS,
		sync,
		port,
		MACH_MSG_TYPE_MAKE_SEND_ONCE,
		&previous_port))
	    != KERN_SUCCESS) {
	    xTrace2(machripcp, TR_FULL_TRACE,
		"machnetipc: failed to request notification on port %x; ret=%d",
		port, ret);
	}
	args->ask_for_nms = 0;
    }

    xTrace0(machripcp, TR_FULL_TRACE,
	"machnetipc: start_rcv: starting machr_msg_rcv");
    msgConstructAllocate(&args->request_msg,
	XK_BASIC_MACH_MSG_MAX, (void *)&args->msg);

    args->netport->net_port_type = MN_VALID;

    xk_master_unlock();
    machr_msg_receive(args);
    /*
     * it is possible to start processing the message right here
     */
    xk_master_lock();

    while ( args->validity ) {
	/*
	 * to get rid of old listeners which could still be hanging around.
	 */
	if (args != args->netport->current_read_desc) {
	    args->validity = FALSE;
	    xTrace2(machripcp, TR_FULL_TRACE,
		"machnetipc: start_rcv for port %x and netport %x exits - stale listener",
		port, args->netport->net_port_number);
	    continue;
	}

	xTrace1(machripcp, TR_FULL_TRACE,
	    "machnetipc: start_rcv for port %x waits", port);
	xTrace1(machripcp, TR_FULL_TRACE,
	    "machnetipc: start_rcv for port %x ends wait", port);

	/*
	 * set up the xkernel message and get a pointer to the buffer
	 */
	msgConstructAllocate(&args->request_msg,
	    XK_BASIC_MACH_MSG_MAX, (void *)&args->msg);
	xTrace0(machripcp, TR_FULL_TRACE,
	    "machnetipc: start_rcv: starting machr_msg_rcv after semwait");
	xk_master_unlock();
	machr_msg_receive(args);
	xk_master_lock();
    }
    xFree((char *)args);
    xTrace1(machripcp, TR_EVENTS,
	"machnetipc: start_rcv for port %x terminates", port);
}


/*
 *   xkmach_check_status
 *
 *	get the Mach kernel's view of the port status
 *
 */
static void
xkmach_check_status(
	struct send_request	*args,
	mach_port_status_t	*pstat)
{
    xTrace0(machripcp, TR_FULL_TRACE,
	"machnetipc: xkmach_check_status: entered");

    if (args->netport->net_port_type != MN_INVALID) {
	kern_return_t	ret;
	mach_msg_type_number_t	infoCnt;

	CLOCK_TRACE("xkmach_check_status: C mach_port_get_attributes");
	if ((ret = mach_port_get_attributes(
		mach_task_self(), args->port,
		MACH_PORT_RECEIVE_STATUS,
		(mach_port_info_t)pstat, &infoCnt))
	    != KERN_SUCCESS) {
	    xTrace4(machripcp, TR_ERRORS,
		"machnetipc: mach_msg_receive: could not get port status for port %x netnum %x ret %x state %x",
		args->port, args->netport->net_port_number,
		ret, args->netport->net_port_type);
	    return;
	}
	CLOCK_TRACE("xkmach_check_status: R mach_port_get_attributes");
	xTrace3(machripcp, TR_DETAILED,
	    "machnetipc: xkmach_check_status: port state is %d (valid is %d) queue len %d",
	    args->netport->net_port_type, MN_VALID, pstat->mps_msgcount);
	/*
	 * this is where we should do queue forwarding
	 * when the forwarding is done, check for nms
	 *      args->netport->net_port_type = MN_VALID;
	 */
    }
}


/*
 * machr_msg_receive (args)
 *
 * The message intermediary receives a message on a port for which
 * the receive rights are on another machine; the send right is local.
 * This is independent of the master xkernel control
 *
 */
void
machr_msg_receive(
	struct send_request	*args)
{
    mach_msg_return_t		ret = 0;
    mach_port_right_t		port = args->port;
    struct mach_msg_big_t	*smsg = args->msg;
    int				too_big = 0;
    mach_port_status_t		pstat;
    int				queue_len;

    xTrace3(machripcp, TR_FULL_TRACE,
	"machr_msg_receive starting on port %x id %x arg addr %x",
	port, args->netport->net_port_number, args);
    xTrace4(machripcp, TR_FULL_TRACE,
	"machr_msg_receive: remote port host %d.%d.%d.%d",
	args->netport->receiver_host_addr.a,
	args->netport->receiver_host_addr.b,
	args->netport->receiver_host_addr.c,
	args->netport->receiver_host_addr.d);

    if (!smsg) {
	xTrace1(machripcp, TR_ERRORS,
	    "machr_msg_receive: no message space in arg block at %x", args);
	return;
    }

    if (port==MACH_PORT_NULL) {
	xTrace0(machripcp, TR_ERRORS,
	    "machr_msg_receive: null port; abandoning");
	args->validity = FALSE;
	return;
    }

    if (!args->reply_msg) {
	xkmach_check_status(args, &pstat);
    }
    queue_len = pstat.mps_msgcount;

    /*
     * check if a port block message came in
     */
    if (args->blocked) {
	xTrace1(machripcp, TR_EVENTS,
	    "machr_msg_receive: BLOCKING sender for port %x",
	    args->netport->net_port_number);
#ifdef PORTBLOCK
	xk_master_lock();
	semWait(&args->sem_blocked);
	xk_master_unlock();
#endif	/* PORTBLOCK */
	xTrace1(machripcp, TR_EVENTS,
	    "machr_msg_receive: After BLOCKING sender for port %x",
	    args->netport->net_port_number);
    }

reread:
    smsg->mmhdr.msgh_local_port = port;
    smsg->mmhdr.msgh_size =
	ROUND4(sizeof(struct mach_msg_big_t)) + MAX_TRAILER_SIZE;
    if (args->reply_msg == (Msg *)NULL) {
	ret = mach_msg_receiveSafeSize(smsg);
    }
    else {
	/*
	 * need to convey possible TOO LARGE error from demux
	 */
	ret = mach_msg_receiveSafeSizeTime(smsg);
    }

    CLOCK_TRACE("machr_msg_receive: received a message");
    xTrace2(machripcp, TR_DETAILED,
	"machnetipc: machr_msg_receive back from receive: ret = %x; reply = %x",
	ret, args->reply_msg);

    if ( ret  == MACH_MSG_SUCCESS
	||
	ret == MACH_RCV_TOO_LARGE
	||
	(ret == MACH_RCV_TIMED_OUT && args->reply_msg != (Msg *)NULL)) {

	xTrace2(machripcp, TR_DETAILED,
	    "machnetipc: machr_msg_receive received message on port %x, msg %x",
	    port, smsg);

	/*
	 * the port has died; port mgr already knows about it
	 */
	if (args->validity == FALSE) return;

	if (ret==MACH_RCV_TIMED_OUT) {
	    /*
	     * return null reply, continue listening in non-rpc state
	     */
	    args->lower_session = ERR_XOBJ;
	    args->reply_msg = (Msg *)NULL;
	    args->deallocate = TRUE;
	    goto reread;
	}

	if (!args->reply_msg) {
	    xkmach_check_status(args, &pstat);
	}

	if (smsg->mmhdr.msgh_id == MACH_NOTIFY_NO_SENDERS) {
	    int	mscount = ((mach_no_senders_notification_t *)smsg)->not_count;

	    xTrace5(machripcp, TR_DETAILED,
		"machripcp: machr_msg_receive detected no more senders (msgh_id %x) on port %x mscount %ld current %d; soflag = %d",
		smsg->mmhdr.msgh_id, port, pstat.mps_mscount,
		mscount, pstat.mps_sorights);

#ifdef NOMORESENDERS
	    if (/* mscount && */
		(mscount == pstat.mps_mscount) &&
		!pstat.mps_sorights) {
		xk_master_lock();
		xTrace1(machripcp, TR_MAJOR_EVENTS,
		    "machnetipc: processing no more senders for port %x",
		    args->netport);
		portm_generate_nms(args->netport);
		xFree((char *)args);
		xk_master_unlock();
		return;
	    }
#endif	/* NOMORESENDERS */

	    {
		mach_port_t	previous_port = MACH_PORT_NULL;

		/*
		 * reregister the nms notification
		 */
		mach_port_request_notification(
		    mach_task_self(),
		    port,
		    MACH_NOTIFY_NO_SENDERS,
		    pstat.mps_mscount,
		    port,
		    MACH_MSG_TYPE_MAKE_SEND_ONCE,
		    &previous_port);
		goto reread;
	    }
	}

	if (smsg->mmhdr.msgh_id == MACH_NOTIFY_SEND_ONCE) {
	    xTrace4(machripcp, TR_MAJOR_EVENTS,
		"machripcp: machr_msg_receive detected send_once deletion (msgh_id %x) on port %x mscount %ld",
		smsg->mmhdr.msgh_id,
		port,
		((mach_no_senders_notification_t *)smsg)->not_count,
		pstat.mps_mscount);
	    args->validity = FALSE;
	}

	xTrace1(machripcp, TR_DETAILED,
	    "machr msg rcv size %d ", smsg->mmhdr.msgh_size);
	/* 
	 * see if the port has changed state; could have died or the 
	 * receive right may have moved off campus
	 */
	/* 
	 * NB:The smsg space will be freed by forward_send_request/reply.
	 */
	if (ret == MACH_RCV_TOO_LARGE) too_big = 1;

	xk_master_lock();

	if (args->reply_msg != (Msg *)NULL) {
	    semWait(&args->netport->listen_sem);
	    machr_forward_rpc_reply(args, too_big);
	    semSignal(&args->netport->listen_sem);
	    xk_master_unlock();
	}
	else {
#ifdef PORTLOCKS
	    /*
	     * don't need to lock fast send-once ports
	     */
	    xTrace3(machripcp, TR_EVENTS,
		"machnetipc machr_msg_receive: Getting READER LOCK on port %x (readers=%d) (writers=%d)",
		args->netport->net_port_number,
		args->netport->rwlock.activeReaders,
		args->netport->rwlock.activeWriter);
	    if (readerLock(args->netport->rwlock_ptr) != XK_SUCCESS) {
		xTrace0(machripcp, TR_ERRORS,
		    "machnetipc: machr_msg_receive: remote port died during message conversion");
		xk_master_unlock();
		return;
	    }
	    xTrace3(machripcp, TR_EVENTS,
		"machnetipc machr_msg_receive: BACK FROM THE READER LOCK on port %x (readers=%d) (writers=%d)",
		args->netport->net_port_number,
		args->netport->rwlock.activeReaders,
		args->netport->rwlock.activeWriter);
#endif	/* PORTLOCKS */

	    /*
	     * check to make sure port is still remote; receive port could have
	     * moved to local machine while we were sitting in the lock above!
	     */
	    if (args->netport->real_receive_port == MACH_PORT_NULL) {
#ifdef PORTLOCKS  
		readerUnlock(args->netport->rwlock_ptr);
		xTrace3(machripcp, TR_EVENTS,
		    "machnetipc machr_msg_receive: BACK FROM THE READER UNLOCK on port %x (readers=%d) (writers=%d)",
		    args->netport->net_port_number,
		    args->netport->rwlock.activeReaders,
		    args->netport->rwlock.activeWriter);
#endif	/* PORTLOCKS */

		xk_master_unlock();

		/*
		 * requeue this message back on the message queue of the
		 * receiving port which is now somewhere on this machine
		 */

		/*
		 * send the message using the send right that we have
		 */
		smsg->mmhdr.msgh_local_port = MACH_PORT_NULL;
		smsg->mmhdr.msgh_remote_port = args->netport->real_send_port;
		/*
		 * ZZZ : ???
		 */
		smsg->mmhdr.msgh_bits =
		    MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
		if (smsg->mmhdr.msgh_bits &
			(unsigned int)(MACH_MSGH_BITS_COMPLEX)) {
		    smsg->mmhdr.msgh_bits |=
			(unsigned int)MACH_MSGH_BITS_COMPLEX;
		}
		/* ZZZ */

		/*
		 * send the message back
		 */
		if ((ret = mach_msg_send((mach_msg_header_t *)smsg))
		    != MACH_MSG_SUCCESS) {
		    xTrace4(machripcp, TR_ERRORS,
			"machnetipc: machr_msg_receive: message could not be delivered: LOSING Message error %x %s; rport = %x; lport = %x",
			ret, mach_error_string(ret),
			smsg->mmhdr.msgh_remote_port,
			smsg->mmhdr.msgh_local_port);
		}

		/*
		 * do we need to free anything... ???
		 */

		/*
		 * exit from this thread
		 */
		args->validity = FALSE;
	    }
	    else {
		semWait(&args->netport->listen_sem);
		machr_forward_send_request(args, too_big, FALSE);
		args->netport->net_port_type = MN_VALID;
		semSignal(&args->netport->listen_sem);
		xk_master_unlock();
	    }
	}

	if (queue_len && !--queue_len) {
	    xTrace1(machripcp, TR_EVENTS,
		"machnetipc: machr_msg_receive: port %x changed from forwarding to valid",
		port);
	    if (args->netport && args->netport != &Null_Netport && 
		args->netport->net_port_type == MN_FORWARDING) {
		args->netport->net_port_type = MN_VALID;
	    }
	}
    }
    else {
	xTrace1(machripcp, TR_ERRORS,
	    "error from machr mach msg recv %x", ret);
	/*
	 * if the port is dead, then xFree the args area
	 */
	if (ret == MACH_RCV_PORT_DIED || MACH_RCV_INVALID_NAME) {
	    xTrace1(machripcp, TR_FUNCTIONAL_TRACE,
		"machnetipc: msg_rcv port %x died", port);
	    args->validity = FALSE;
	}
	else {
	    xTrace2(machripcp, TR_ERRORS,
		"machnetipc: msg_rcv unexpected error %x on port %x",
		ret, port);
	}
    }
    if (args->deallocate) {	/* a send_once right goes away immediately */
	xk_master_lock();
	xFree((char *)args);
	xk_master_unlock();
    }
}


/*
 *  is_send(rright)
 *
 *   determine if an incoming net Mach message has legal header rights
 *
 */
#define is_send(rright)	\
	    ((rright) & ( MACH_PORT_TYPE_SEND_ONCE | MACH_PORT_TYPE_SEND))

/*
 * convert_netmach_msg_to_actual_mach()
 *
 * Convert all transferred ports, out-of-line data, etc.
 * The two header ports have been converted by the caller
 * The incoming message has had the arch and type field and the two mnetport
 * structures popped off.  The "outmsg" is used only for the remote and
 * local ports.  This routine returns a pointer to fully formed outgoing
 * message.
 * 
 */
static mach_msg_header_t *
convert_netmach_msg_to_actual_mach(
	Msg			*netmsg,
	mach_msg_header_t	*outmsg,
	int			port_count,
	int			notinline_count,
	int			xfercompletion,
	mn_arch_tag_t		msgarch,
	msg_id_t		msgid,
	IPhost			srchost,
	mach_port_right_t	local_right,
	boolean_t		rpc_p)
{
    long			xk_msg_length;
    int				mach_msg_length;
    struct mach_msg_big_t	*new_mach_msg;
    mach_port_type_t		remote_right;
    mach_msg_header_t		tmphdr;
    kern_return_t		ret;

    CLOCK_TRACE("convert_netmach_msg_to_actual_mach: entry");
    xTrace0(machripcp, TR_FULL_TRACE, "convert_netmach_to_actual_mach");

    /*
     * ZZZ : length of netmsg includes trailer length
     */
    xk_msg_length = msgLen(netmsg);
    mach_msg_length = xk_msg_length;

    if (outmsg->msgh_remote_port == MACH_PORT_NULL) {
	local_right = MACH_PORT_TYPE_SEND_ONCE;
    }
    else {
	CLOCK_TRACE("convert_netmach_msg_to_actual_mach: C mach_port_type");
	if ((ret=mach_port_type(mach_task_self(),
		outmsg->msgh_remote_port, &remote_right))
	    != KERN_SUCCESS) {
	    xTrace2(machripcp, TR_ERRORS,
		"machripc: netmach_to_actualmach: could not get port type for port %x failure code %x hex",
		outmsg->msgh_remote_port, ret);
	}
	else {
	    xTrace2(machripcp, TR_DETAILED,
		"machripc: netmach_to_actualmach: got port right of type %x for remote port %x",
		remote_right, outmsg->msgh_remote_port);
	}
	CLOCK_TRACE("convert_netmach_msg_to_actual_mach: R mach_port_type");

	/*
	 * ZZZ : Operations Mismatch for local_right(mach_port_right_t):
	 *   1.  MACH_PORT_NULL=0 is mach_port_t
	 *       0 of mach_port_right_t is MACH_PORT_RIGHT_SEND.
	 *   2.  is_send() can be applied for mach_port_type_t,
	 *       a bit map representation of port_rights
	 */
	if (local_right != MACH_PORT_NULL && !is_send(local_right)) {
	    xTrace2(machripcp, TR_ERRORS,
		"machripc: convert_netmach: inappropriate port %x local rights %x",
		outmsg->msgh_local_port, local_right);
	    xk_msg_length = 0;
	    xTrace1(machripcp, TR_ERRORS,
		"machripc: xkmsglen %d", msgLen(netmsg));
	}
    }
    if (xk_msg_length < 0) {
	xTrace2(machripcp, TR_ERRORS,
	    "machnetipc: convert_netmach_msg_to_actual_mach: bogus length %d dec(%x hex)",
	    xk_msg_length, xk_msg_length);
	xk_msg_length = 0;
    }

    xTrace1(machripcp, TR_DETAILED,
	"machripc convert_netmach_msg_to_actual_mach: checking arch type %x",		msgarch);

    if (msgarch == MN_ARCH_MARKER) {
	if (port_count || notinline_count) {
	    /*
	     * must convert ports and ool data
	     */
	    xTrace2(machripcp, TR_DETAILED,
		"machripc: convert_netmach_msg_to_actual_mach: complex - ports %d ool %d",
		port_count, notinline_count);
	    new_mach_msg =
		(struct mach_msg_big_t *)xk_ports_and_ool_convert(
		netmsg, port_count, notinline_count,
		msgid, srchost, xfercompletion);

	    if (!new_mach_msg) {
		return 0;
	    }
	}
	else {
	    if (xfercompletion) {
		if (xfercompletion & 2) {
		    rrxTransferComplete(srchost, msgid);
		}
		srxTransferComplete(srchost, msgid);
	    }
	    msgPop(netmsg, (MLoadFun)mycopypop_andlie,
		(char *)&tmphdr, sizeof(tmphdr), (void *)0);
	    if (tmphdr.msgh_size>0) {
		new_mach_msg = (struct mach_msg_big_t *)xMalloc(
		    ROUND4(tmphdr.msgh_size)+MAX_TRAILER_SIZE);
		bzero((char *)new_mach_msg,
		    ROUND4(tmphdr.msgh_size))+MAX_TRAILER_SIZE;
		msgPop(netmsg, (MLoadFun)mycopypop, (char *)new_mach_msg,
		    ROUND4(tmphdr.msgh_size)+MAX_TRAILER_SIZE, (void *) 0);
		xTrace3(machripcp, TR_DETAILED,
		    "machripc convert_netmach_msg_to_actual_mach: msgLen %d, tmphdr.msgh_size %d, computed data msg_length %d",
		    xk_msg_length, tmphdr.msgh_size, mach_msg_length);
		if (xk_msg_length
		    != ROUND4(tmphdr.msgh_size)+MAX_TRAILER_SIZE) {
		    xTrace2(machripcp, TR_SOFT_ERRORS,
			"machripc convert_netmach_msg_to_actual_mach: msg length mismatch: xk length %d mach length %d",
			xk_msg_length, tmphdr.msgh_size);
		}
	    }
	    else {
		return 0;
	    }
	}
    }
    else {
	/*
	 ***** must convert each item according to architecture *******
	 */
	xTrace2(machripcp, TR_FULL_TRACE,
	    "machripc: netdata different architecture:  ports %d ool %d",
	    port_count, notinline_count);
	new_mach_msg =
	    (struct mach_msg_big_t *)xk_netmach_msg_to_mach(
	    netmsg, port_count, notinline_count, msgid,
	    srchost, xfercompletion, msgarch);
	if (!new_mach_msg) {
	    return 0;
	}
    }

/*
 * ZZZ : These are an operation, not just predicate. is it OK ?
 */
#define XKMOVERIGHT(right)				\
	    (((right) & MACH_PORT_TYPE_SEND_ONCE) ?	\
		MACH_MSG_TYPE_MOVE_SEND_ONCE :		\
		MACH_MSG_TYPE_MOVE_SEND)

#define XKMOVEREPLYRIGHT(right)				\
	    (((right) & MACH_PORT_TYPE_SEND_ONCE) ?	\
		MACH_MSG_TYPE_MAKE_SEND_ONCE :		\
		MACH_MSG_TYPE_MOVE_SEND)

    new_mach_msg->mmhdr.msgh_remote_port = outmsg->msgh_remote_port;
    new_mach_msg->mmhdr.msgh_local_port = outmsg->msgh_local_port;
    new_mach_msg->mmhdr.msgh_bits =
	MACH_MSGH_BITS(XKMOVERIGHT(remote_right),
	XKMOVEREPLYRIGHT(local_right));
    if (port_count || notinline_count) {
	new_mach_msg->mmhdr.msgh_bits |= MACH_MSGH_BITS_COMPLEX;
    }

    xTrace5(machripcp, TR_DETAILED,
	"machripc convert_netmach_msg_to_actual_mach: using msgh bits %x hex for remote port right %x local port right %x remote port %x local port %x",
	new_mach_msg->mmhdr.msgh_bits,
	remote_right, local_right,
	new_mach_msg->mmhdr.msgh_remote_port,
	new_mach_msg->mmhdr.msgh_local_port);
    xTrace2(machripcp, TR_DETAILED,
	"machripc convert_netmach_msg_to_actual_mach: received mach message length is %d dec (%x hex)",
	new_mach_msg->mmhdr.msgh_size, new_mach_msg->mmhdr.msgh_size);

    CLOCK_TRACE("convert_netmach_msg_to_actual_mach: exit");
    return (mach_msg_header_t *)new_mach_msg;
}


/*
 *  msgid_assign()
 *
 *
 */
static msg_id_t
msgid_assign( void )
{
    static msg_id_t	ident = 0;

    xTrace2(machripcp, TR_EVENTS, "msgid_assign %d (0x%x)", ident, ident);
    return ident++;
}


/*
 *  is_machhdr(msg, rright)
 *
 *   determine if a Mach message has expected reply form
 *
 */
#define is_machhdr(port, rright)					\
	    ((port)   == MACH_PORT_NULL ||				\
	    ((rright) & (MACH_PORT_TYPE_SEND_ONCE | MACH_PORT_TYPE_SEND)))

/*
 *  mnnetportcopy_out
 *
 *       put a port descriptor into a network header
 *       this goes in host byte order; receiver will use the
 *       archtype marker to get them into the destination byte order
 */
static void mnportcopy_out(
	char		*msghdr,
	mnetport	*netport)
{
    bcopy((char *)&netport->net_port_number,  msghdr, PORTID_NETLEN);
    bcopy((char *)&netport->net_port_rights,
	msghdr += PORTID_NETLEN, PORTRIGHT_NETLEN);
    bcopy((char *)&netport->receiver_host_addr,
	msghdr + PORTRIGHT_NETLEN, sizeof(IPhost));
}


/*
 * convert_mach_msg_to_netmach_msg
 *	  (netmsg, xmsg, outmsg, reuse_outmsg, remote_port, dest, msgtype)
 *
 *    creates outgoing message header and a network data msg structure
 *
 *    converts all transferred ports (except fast reply port),
 *      out-of-line data, etc. 
 * 
 */
static void
convert_mach_msg_to_netmach_msg (
	struct mach_msg_big_t	*msg,
	Msg			*xkmsg,   /* NB msg *IS* the data in xkmsg */
	machnetipc_hdr		*outmsg,
	boolean_t		reuse_outmsg,
	mnetport		*remote_port,
	IPhost			dest,
	enum MACHIPCTYPE	msgtype)
{
    mnetport		*repport,
			*remport = NULL;
    mach_port_right_t	reply_right;

    CLOCK_TRACE("convert_mach_msg_to_netmach_msg: entry");
    xTrace0(machripcp, TR_FULL_TRACE, "convert_mach_msg_to_netmach_msg");

    if (!reuse_outmsg) {
	outmsg->hdr.architecture_type = PERM1;
	outmsg->hdr.machnetipcmsg_type = MACHRIPC_MSG;
    }

    if (msg->mmhdr.msgh_remote_port == MACH_PORT_NULL) {
	reply_right = 0;
    }
    else {
	CLOCK_TRACE("convert_mach_msg_to_netmach_msg: C mach_port_type");
	if (mach_port_type(
		mach_task_self(),
		msg->mmhdr.msgh_remote_port,
		&reply_right)
	    != KERN_SUCCESS) {
	    xTrace1(machripcp, TR_ERRORS,
		"machripc convert_mach_msg: Could not get local port %x type",
		msg->mmhdr.msgh_local_port);
	    return;
	}
	CLOCK_TRACE("convert_mach_msg_to_netmach_msg: R mach_port_type");
    }
    /*
     * can't transfer receive right here
     */
    reply_right &= ~MACH_PORT_TYPE_RECEIVE;

    if (!is_machhdr( msg->mmhdr.msgh_remote_port, reply_right)) {
	xTrace0(machripcp, TR_ERRORS,
	    "machnetipc convert_mach_to_netmach: using inappropriate right; cannot handle this");

	/*
	 * ZZZ : If the message has descriptors, they should be printed here
	 */
	xTrace3(machripcp, TR_DETAILED,
	    "machripc convert parameters port %x right %x send_once representation %x",
	    msg->mmhdr.msgh_remote_port,
	    reply_right, MACH_PORT_TYPE_SEND_ONCE);
	return;
    }

    if (msgtype != MACHRIPC_RPC_REPLY_MSG) {
	if (!reuse_outmsg) {	/* watch out for receiver changing */
	    mnportcopy_out(
		(char *)&outmsg->content.machmsg.transport_port, remote_port);
	}
	mnportcopy_out(
	    (char *)&outmsg->content.machmsg.reply_port, &Null_Netport);
	if (msgtype != MACHRIPC_RPC_REQUEST_MSG &&
		/* requests have fast-sendonce*/
	    (remport = quick_port_convert(&(msg->mmhdr.msgh_local_port)))
	    == (mnetport *)NULL) {
	    /*
	     * this must be an error; if this task knows the port, it must be
	     * already be a netport
	     */
	    xTrace1(machripcp, TR_ERRORS,
		"machnetipc convert_mach_msg cannot find reply port descriptor %x",
		msg->mmhdr.msgh_remote_port);
	    outmsg->content.machmsg.transport_port =
		*((mn_netport_t *)&Null_Netport);
	    return;
	}
    }
    if (!reuse_outmsg) {
	outmsg->content.machmsg.transport_port.net_port_rights = 
	    ONESEND(outmsg->content.machmsg.transport_port.net_port_rights );
    }
    outmsg->content.machmsg.netport_count = 0;
    xTrace1(machripcp, TR_FULL_TRACE,
	"machnetipc: convert_mach_msg length %d", msg->mmhdr.msgh_size);
    xTrace1(machripcp, TR_FULL_TRACE,
	"machnetipc: convert_mach_msg new length %d", msg->mmhdr.msgh_size);

    /*
     * convert the reply port, favoring RPC-style messages
     */
    if (reply_right == MACH_PORT_TYPE_SEND_ONCE &&
	msgtype != MACHRIPC_RPC_REPLY_MSG) { /* your standard client request */
	outmsg->hdr.machnetipcmsg_type = MACHRIPC_RPC_REQUEST_MSG;
	xTrace0(machripcp, TR_DETAILED,
	    "machnetipc: convert msg to netmsg: RPC Request msg");
	/*
	 * the reply right should be filled in by caller, using fast so
	 */
    }
    else {
	if ( msgtype == MACHRIPC_RPC_REPLY_MSG ) {
	    /*
	     * your typical server reply
	     * reply port must be null for rpc_reply
	     */
	    xTrace0(machripcp, TR_DETAILED,
		"machnetipc: convert msg to netmsg: RPC Reply msg");
	    outmsg->hdr.machnetipcmsg_type = MACHRIPC_RPC_REPLY_MSG;
	    outmsg->content.machmsg.reply_port.net_port_number = 0;
	    /* the transport port will be filled in by caller,
	     * using fast send_once rights
	     */
	}
	else {
	    if ((repport = quick_port_convert(&(msg->mmhdr.msgh_remote_port)))
		== (mnetport *)&Null_Netport) {
		/*
		 * this is a new right
		 */
		mnetport mnport;

		xTrace2(machripcp, TR_FUNCTIONAL_TRACE,
		    "machripc converted new reply port descriptor for port %x right %x",
		    msg->mmhdr.msgh_remote_port, reply_right);
		if (msg->mmhdr.msgh_remote_port != MACH_PORT_NULL) {
		    CLOCK_TRACE(
		       "convert_mach_msg_to_netmach_msg: C convert_to_netport");
		    mnport = convert_to_netport(
			msg->mmhdr.msgh_remote_port,
			ONESEND(reply_right), dest, 0, 0);
		    CLOCK_TRACE(
		       "convert_mach_msg_to_netmach_msg: R convert_to_netport");
		    outmsg->content.machmsg.reply_port =
			*((mn_netport_t *)&mnport);
		    outmsg->content.machmsg.reply_port.net_port_rights =
			ONESEND(
			    outmsg->content.machmsg.reply_port.net_port_rights);
		}
		else {
		    outmsg->content.machmsg.reply_port =
			*((mn_netport_t *)&Null_Netport);
		}
	    }
	    else {
		xTrace3(machripcp, TR_FUNCTIONAL_TRACE,
		    "machnetipc: convert_msg_to_net: found existing reply port descriptor for port %x right %x desc right %x",
		    msg->mmhdr.msgh_remote_port, reply_right, repport->net_port_rights);
		if (repport != &Null_Netport
#ifdef PORTLOCKS
		    && readerLock(repport->rwlock_ptr) == XK_SUCCESS
#endif	/* PORTLOCKS */
			) {
		    if (!(repport->net_port_rights & reply_right)) {
			xTrace0(machripcp, TR_FUNCTIONAL_TRACE,
			    "machnetipc: convert_msg_to_net: needs to add new right to existing right old");
			if ((repport->net_port_rights
			    == MACH_PORT_TYPE_SEND_ONCE) ||
			    (reply_right == MACH_PORT_TYPE_SEND_ONCE)) {
			    xTrace0(machripcp, TR_ERRORS,
				"machnetipc: convert_msg_to_net: Cannot add send_once right");
			}
			else {
			    xTrace0(machripcp, TR_ERRORS,
				"machnetipc:convert_msg_to_net: right addition not supported");
			}
		    }
		    portm_add_sender(repport, dest);
			/* updates the make_send_count */
		    outmsg->content.machmsg.reply_port =
			*((mn_netport_t *)repport);
		}
		else {
		    xTrace0(machripcp, TR_EVENTS,
			"machnetipc: convert_msg_to_net: null reply port or reply port died");
		    repport = &Null_Netport;
		}
	    }
	}
    }
    outmsg->content.machmsg.message_id = msgid_assign();

    if (msg->mmhdr.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
	/*
	 * if necessary, add ports, ooldata
	 */
	xk_mach_msg_convert(msg, xkmsg, outmsg, dest);
    }

/*
    /*
     * NO NEED: It is already done outside this routine - HASNAIN
     * with fast send_once, caller does this 
     *
    if (msgtype != MACHRIPC_RPC_REPLY_MSG &&
	remport->net_port_rights == MACH_PORT_TYPE_SEND_ONCE ) {
	xTrace2(machripcp, TR_DETAILED,
	    "machnetipc: convert_msg_to_net: Removing network send_once right for port number %x mach_port %x",
	    remote_port->net_port_number, remote_port->real_send_port);
	/*
	 * remove from database; less interest in old sender
	 * and deallocate the remport structure 
	 *
	portm_port_remove_send_once(remote_port);
    }
 */

    xTrace0(machripcp, TR_FULL_TRACE,
	"machnetipc: convert msg to netmsg exits");
    CLOCK_TRACE("convert_mach_msg_to_netmach_msg: exit");
}


/*
 *
 * machr_control
 *
 */
static int
machr_control(
	XObj	self,
	int	opcode,
	char	*buf,
	int	len)
{
    xTrace0(machripcp, TR_FULL_TRACE, "machr_control called");
    xAssert(xIsProtocol(self));
    switch (opcode) {
	default:
	    return
		xControl(xGetDown(self, MNI_TRANSPORT_DNUM), opcode, buf, len);
    }
}


/*
 *
 * machr_close
 *
 */
static xkern_return_t
machr_close(
	XObj	self)
{
    xTrace0(machripcp, TR_FULL_TRACE, "machr_close called");
    /*
     * xFree((char *)self);
     */
    return XK_SUCCESS;
}


/*
 *
 * machr_opendone
 *
 */
static xkern_return_t
machr_opendone(
	XObj	self,
	XObj	session,
	XObj	llp,
	XObj	hlpType)
{
    xTrace0(machripcp, TR_FULL_TRACE, "machr_opendone called");
    return XK_SUCCESS;
}


/*
 *  Initialization utilities
 *
 *     getproc, machr_session_init
 *
 */
static void
getproc(
	XObj	p)
{
    xAssert(p->type == Protocol);
    p->push = machr_push;
    p->open = machr_open;
    p->opendone = machr_opendone;
    p->openenable = machr_openenable;
    p->demux = machr_demux;
    p->calldemux = machr_calldemux;
    p->control = machr_control;
}


static void
machr_session_init(
	XObj		p,
	Mach_ActiveId	*state)
{
    /*
     * xAssert(p->type == Protocol);
     */
    xTrace0(machripcp, TR_FULL_TRACE, "machr_session_init");
    p->push = machr_push;
    p->open = machr_open;
    p->close = machr_close;
    p->openenable = machr_openenable;
    p->demux = machr_demux;
    p->calldemux = machr_calldemux;
    p->control = machr_control;
    if (state == 0) {
	(p->state) = (void *)TYPE_MALLOC(Mach_ActiveId);
    }
    else {
	(p->state) = (void *)state;
    }
}


/*
 *   stop sender from flooding a full Mach message queue
 *
 */
static void
port_block(
	mnetport	*port)
{
    xTrace1(machripcp, TR_FULL_TRACE,
	"machnetipc: mach_port_block: port %x", port->net_port_number);
    port->current_read_desc->blocked++;
}


/*
 *   queue has developed more room
 *
 */
void port_unblock(
	mnetport	*port)
{
    xTrace1(machripcp, TR_EVENTS,
	"machnetipc: mach_port_unblock: port %x", port->net_port_number);
    port->current_read_desc->blocked--;

#ifdef PORTBLOCK
    if (port->current_read_desc->blocked == 0) {
	semSignal(&port->current_read_desc->sem_blocked);
    }
#endif	/* PORTBLOCK */

    xTrace1(machripcp, TR_EVENTS,
	"machnetipc: mach_port_unblock: after unblocking port %x",
	port->net_port_number);
}


/*
 *   persistent sending thread
 *
 */
static void
mach_port_retry_send(
	Event	*ev,
	void	*args)
{
    struct retry_send	*retry_args = (struct retry_send *)args;
    mach_msg_header_t	*mmsg = (mach_msg_header_t *)retry_args->machmsg;
    kern_return_t	ret = ~MACH_MSG_SUCCESS;
    mach_port_t		retry_port = MACH_PORT_NULL;
    mach_msg_header_t	m;

    xTrace1(machripcp, TR_FULL_TRACE,
	"machnetipc: mach_port_retry_send: port %x",
	mmsg->msgh_remote_port);

    xk_master_unlock();
    while (ret != MACH_MSG_SUCCESS) {
	if ((ret = mach_msg_send_proxy(mmsg)) != MACH_MSG_SUCCESS) {
	    xTrace3(machripcp, TR_SOFT_ERROR,
		"machnetipc: mach_port_retry_send: failed for port %x code %x %s; delaying...",
		mmsg->msgh_remote_port, ret, mach_error_string(ret));
	    if (retry_port == MACH_PORT_NULL) {
		mach_port_allocate(
		    mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &retry_port);
	    }
	    /*
	     * this is a complicated way of getting a delay
	     */
	    mach_msg(&m, MACH_RCV_MSG|MACH_RCV_TIMEOUT,
		0, sizeof m, retry_port, 1000,  /* units ??? */
		MACH_PORT_NULL);
	}
    }

    xTrace0(machripcp, TR_FULL_TRACE,
	"machnetipc: mach_port_retry_send: send successful");

    if (retry_port != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), retry_port);
    }

    xk_master_lock();

    xTrace0(machripcp, TR_FULL_TRACE,
	"machnetipc: mach_port_retry_send: unblocking sender");

    /*
     * send an unblock port message
     */
    send_port_block(retry_args->pd, retry_args->sender, PORT_UNBLOCK);

    /*
     * is this OK?  what about deallocating the ool data?
     */
    xFree((char *)retry_args->machmsg);
    xFree((char *)retry_args);
    xk_master_unlock();
}


/*
 *  send_port_block
 *      Sends a port block/unblock message (block_type) to
 *      the sender for port pd.
 */
static void
send_port_block(
	mnetport		*pd,
	IPhost			sender,
	enum MACHIPCTYPE	block_type)
{
    Msg			msg;
    Msg			rmsg;
    struct port_block	pb;
    Part		whom[1];
    XObj		ls;

    xTrace5(machripcp, TR_EVENTS,
	"machripc: send_port_block: %d notification to host %d.%d.%d.%d",
	block_type, sender.a, sender.b, sender.c, sender.d);

    pb.phdr.architecture_type = MN_ARCH_MARKER;
    pb.phdr.machnetipcmsg_type = block_type;
    pb.portnumber = pd->net_port_number;

    msgConstructInplace(
	&msg, (char *)&pb, sizeof(pb),
	(MsgCIPFreeFunc)null_free);
    msgConstructEmpty(&rmsg);

    partInit(whom, 1);
    partPush(whom[0], &sender, sizeof(IPhost));
    if ((ls = xOpen(myProtl, myProtl,
	xGetDown(myProtl, MNI_TRANSPORT_DNUM), whom))
	== ERR_XOBJ) {
	xTrace0(machripcp, TR_ERRORS,
	    "machripc: send_port_block: cannot open transport session");
    }
    xTrace3(machripcp, TR_EVENTS,
	"machripc: send_port_block: xCall: if %x, push msg at %x and get rmsg at %x",
	ls, &msg, &rmsg);
    if (ls != ERR_XOBJ && xCall(ls, &msg, &rmsg) != XK_SUCCESS) {
	xTrace4(machripcp, TR_ERRORS,
	    "machripcp: send_port_block: could not send notification to host %d.%d.%d.%d",
	    sender.a, sender.b, sender.c, sender.d);
    }
    else {
	xTrace0(machripcp, TR_EVENTS,
	    "machripc: send_port_block: sent net notification");
    }

    xClose(ls);
    msgDestroy(&msg);
    msgDestroy(&rmsg);
}

