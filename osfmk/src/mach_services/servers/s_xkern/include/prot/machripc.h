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
 * machripc_internal.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.13
 * Date: 1993/08/12 19:33:09
 */
/*
 * machr version for v3.2 by Hilarie Orman Fri Oct 25 14:51:08 1991
 * initial working version Mon Mar 30 17:22:20 1992
 */

#ifndef	machripc_internal_h
#define	machripc_internal_h

#include <s_xkern/include/prot/hdr_utils.h>

#define MACHR_UDP_PORTNUM 112   /* our UDP service port		    */
#define MACHR_PORT_MAX    256   /* maximum number of active ports expected */

#define MNI_TRANSPORT_DNUM        0    /* the first lower protocol */
#define MNI_RPC_TRANSPORT_DNUM    1    /* the second lower protocol */
#define MNI_REBOOT_DNUM		  2    /* the boot id control protocol */

#define MNI_READAHEAD_LIMIT       5    /* flow control for send rights */
#define MACHIPC_MSG_MAP		 10    /* msgid map size	       */
#define MACHIPC_REPLYMSG_MAP	  5    /* reply msg map size	   */

#define ROUND4(len)   ((len + 3) & ~3)

struct machr_state {
    int		rnum;
    IPhost	source_id;
    /*
     * the following is used to identify this host uniquely;
     * any of its IP addresses will do
     */
    IPhost	local_source_addr;
    XObj	ssr_service;
    XObj	ssr_service_session;
    xkSemaphore	ssr_wait;
    mach_port_t	notification_port;
};

struct machr_session_state {
    int		rnum;
    IPhost	source_id;
    /*
     * the following is used to identify this host uniquely;
     * any of its IP addresses will do
     */
    IPhost	local_source_addr;
    XObj	ssr_service;
    boolean_t	activep;
    boolean_t	rebootedp;
    mach_port_t	mach_request_port;
    mach_port_t	mach_reply_port;
};

/* 
 * mach_netport_desc --
 *   the network description of a port.  Only the receiver needs
 *   the full description.  A send_once or send right needs only the
 *   net_port_number and receiver_host.
 */

#define INITIAL_MAX_MNIPC_SENDERS 20  /* for map initialization */
#define INIT_MAKE_SEND_COUNT       1

enum MNETPORT_VALIDITY
{ 
    MN_VALID = 0,
    MN_INVALID,
    MN_BLOCKED,
    MN_FORWARDING,
    MN_WAITING,
    MN_ADDING_SENDER,
    MN_MOVING_RECEIVER
};

#define XMINT 4
#define XMSHORT 2

typedef int		msg_id_t;
typedef msg_id_t	MsgId;

/*
 * sizeof int
 */
#define MSGID_NETLEN	XMINT
#define MSGID_NETTYPE	XK_INTEGER_32

/*
 * sizeof mnportid_t
 */
#define PORTID_NETLEN	XMINT
#define PORTID_NETTYPE	XK_INTEGER_32

/*
 * sizeof mach_port_right_t
 */
#define PORTRIGHT_NETLEN  XMINT
#define PORTRIGHT_NETTYPE XK_INTEGER_32

/*
 * sizeof port index
 */
#define NETPORTINDEXSIZE XMINT
#define NETPORTINDEXTYPE XK_INTEGER_32

/*
 * sizeof of data index
 */
#define NETDATAINDEXSIZE XMINT
#define NETDATAINDEXTYPE XK_INTEGER_32

typedef unsigned int	mnportid_t;

/*
 * sizeof make send count
 */
#define NETMAKESENDSIZE  XMINT
#define NETMAKESENDTYPE  XK_INTEGER_32

/*
 * sizeof IPhost
 */
#define HOSTNETLEN 4

/* 
 * mach_netport_desc --
 * the host independent descriptor for a Mach port
 */
#define netPortStateStr( state ) 			\
  ((state == MN_VALID) ? "VALID" :			\
   (state == MN_INVALID) ? "INVALID" :			\
   (state == MN_FORWARDING) ? "FORWARDING" :		\
   (state == MN_WAITING) ? "WAITING" :			\
   (state == MN_ADDING_SENDER) ? "ADDING SENDER" :	\
   (state == MN_MOVING_RECEIVER) ? "MOVING RECEIVER" :	\
   "UNKNOWN" )

struct send_request;

struct mach_netport_desc {
    mnportid_t   	net_port_number;
    mach_port_type_t	net_port_rights;
    IPhost    		receiver_host_addr;
    int			make_send_count;
    /*
     * should be using some generic address form; partLists?
     * anyway, right now it is assumed to be IPhost, 32 bits
     */
    /* don't change above this line
     * without checking struct mach_netport_transfer
     */

    enum MNETPORT_VALIDITY  net_port_type;
    enum MNETPORT_VALIDITY  old_net_port_type;

    int			send_once_right_count;
    int			send_right_count;
    msg_id_t		msgid;
    IPhost		orig_host;

    int			amReceiver;
    mach_port_type_t	net_port_local_rights;
    XObj		lower_session;
    enum MNETPORT_VALIDITY  local_status;	/* invalid if dest reboots */
    mach_port_t		real_receive_port;
    mach_port_t		real_send_port;
    struct send_request	*current_read_desc;
    xkSemaphore		queue_sem;	/* wait for queue to be forwarded */
    ReadWriteLock	rwlock;
    ReadWriteLock	*rwlock_ptr;
    Map			senders_map;	/* following are for net rcvr */
    Map			session_map;
    int			sender_count;
    xkSemaphore		listen_sem;
};

typedef struct mach_netport_desc	mnetport;

/*
 * this goes into messages where a port is required
 */
struct mach_netport_transfer {
    mnportid_t		net_port_number; /* global id */
    mach_port_type_t	net_port_rights;/*MACH_PORT_TYPE_(SEND_(ONCE),RECEIVE)*/
    IPhost	 	receiver_host_addr;
    int			make_send_count;
};

typedef struct mach_netport_transfer	mn_netport_t;
typedef mn_netport_t			mportNetRep;

enum MACHIPCTYPE
{ 
    SSR_MSG,
    MACHRIPC_MSG,
    MACHRIPC_RPC_REQUEST_MSG, 
    MACHRIPC_RPC_REPLY_MSG,
    MACHRIPC_FORWARDED_QUEUE_MSG,
    PORT_MGMT_MSG,
    MACHRIPC_NULL_MSG,
    PORT_BLOCK,
    PORT_UNBLOCK
};

#define MACHIPCTYPE_NETLEN	XMINT

/*
 * try to get some network independent markers
 */
typedef u_int	mn_arch_tag_t;

#define MN_ARCH_TAG_NETLEN	XMINT

enum MACH_MSG_DATA_TYPE
{ 
    MPORT_SEND = MACH_MSG_TYPE_PORT_SEND
};

/*
 * mask for getting port completion indicator
 */
#define MNSEQMASK 3

/*
 * extern int mnperm1, mnperm2, mnperm3, mnperm4;
 */

/*
 * machripc <-> machripc
 */
struct machnetipc {
    mn_netport_t	transport_port;
    /* 
     * receiving machine is expected to hold the receive right
     */
    mn_netport_t	reply_port;
    /*
     * don't modify below without checking the code for receiving from net
     */
    short		netport_count;
    short		notinline_count;
    int			sequence_num;	/* unused  */
    msg_id_t		message_id;	/* in conjunction with right Xfer */
    /*
     * don't modify above without checking the code for receiving from net
     */
};

typedef struct machnetipc	machripc_t;

struct mnipchdr
{
    /*
     * see struct machripc_msg defn
     */
    enum SOURCE_BYTE_ARCH	architecture_type;
    enum MACHIPCTYPE		machnetipcmsg_type;
};

struct ssrhdr {
    mn_netport_t 	netreplyport;
    char		xfer_completion;
    msg_id_t		msgid;
};

/*
 * machripc <-> machripc
 */
struct machripc_msg {
    struct mnipchdr	hdr;
    /*
     * incoming data starts here; hdr becomes function parameters
     */
    union {
	struct ssrhdr	ssr_hdr;
	machripc_t	machmsg;
    } content;
};

typedef struct machripc_msg	machnetipc_hdr;

/*
 * header type lengths
 */
#define XMPORT PORTID_NETLEN + PORTRIGHT_NETLEN + HOSTNETLEN + NETMAKESENDSIZE

/*
 * below is arch, type, netport, netport, count, count, seq, msgid
 */
#define MACHNETHDRSIZE	\
	    MN_ARCH_TAG_NETLEN + MACHIPCTYPE_NETLEN + XMPORT + XMPORT +	\
	    XMSHORT + XMSHORT + XMINT + XMINT

/*
 * net rep of msgt_type_t is 4 bytes
 */
#define MACHNETTYPEHDRSIZE	4

/*
 * net rep of msgt_type_long_t
 */
#define MACHNETLONGHDRSIZE MACHNETTYPEHDRSIZE + XMSHORT + XMSHORT + XMINT

/*
 * A generic message structure
 *
 * there is no fixed upper bound to the size of Mach inline messages.
 *  this is used as a convenient default.  the message will be re-read
 *  if it is larger
 */
#define MAX_MACH_INLINE 1024

struct mach_msg_big_t {
    mach_msg_header_t	mmhdr;  /* the Mach portions */
    char		user_data[MAX_MACH_INLINE];
};

#define XK_BASIC_MACH_MSG_MAX				\
	    (ROUND4(sizeof(struct mach_msg_big_t)	\
	          + sizeof(machnetipc_hdr)		\
	          + sizeof(mach_msg_body_t)		\
	    ) + MAX_TRAILER_SIZE)

/*
 *  Active ID structure
 *
 *   used in making SSR requests
 */
typedef struct {
    mach_port_t	localport;
    mach_port_t	remoteport;
    mnetport	*netport;
    IPhost	destination_host;
    IPhost	source_id;
    IPhost	local_source_addr;
    XObj	ssr_service;
    boolean_t	rebootedp;
} Mach_ActiveId;

/*
 * A send request structure, for port listeners
 *
 *   This is created in response to receipt from the net of a new send right;
 *     if it is an rpc reply port, then it is created via calldemux and
 *     that thread is waiting for the return.  Otherwise it associated with
 *     a network sending thread that exists for the lifetime of the send right.
 */
struct send_request {
    mach_port_t		port;
    struct mach_msg_big_t	*msg;
    XObj		self;
    XObj		lower_session;
    mnetport		*netport;
    mnportid_t		port_net_number;
    boolean_t		validity;
    Msg			*reply_msg;
    Msg			request_msg;
    boolean_t		ask_for_dead_name;
    boolean_t		ask_for_nms;
    boolean_t		deallocate;	 /* should deallocate structure */
    int			blocked;
    xkSemaphore		sem_blocked;
    int			sequence_number; /* unused */
    machnetipc_hdr	outmsg;
};

/*
 *  Structures for cleaning up after sending complex messages.
 *  Out-of-line data must be vm_deallocated, locked ports must be unlocked.
 */
struct ool_list {
    char	*data;
    long	size;
};

struct msg_attribute {
    Msg		locked_ports;
    int		num_locked_ports;
    Msg		ool_list_msg;
    int		num_ools;
};

/*
 * Header of the port block message
 */
struct port_block {
    struct mnipchdr	phdr;
    mnportid_t		portnumber;
};

/*
 * Structure for the arguments of the retry send thread. Information
 * stored here is needed upon sending out a port unblock message.
 */
struct retry_send {
    IPhost		sender;
    mach_msg_header_t	*machmsg;
    mnetport		*pd;
};

#endif	/* !machripc_internal_h */
