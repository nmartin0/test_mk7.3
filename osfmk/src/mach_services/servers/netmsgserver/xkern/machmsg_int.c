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
 * machmsg_int.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.11
 * Date: 1993/08/12 19:31:39
 */

/*
 *
 *  Interpreter for the Mach msg structure
 *   This is called only for messages that have the 'complex' bit set
 *
 *  Support for heterogeneous architectures is not included in this
 *  version.
 *
 */

#include <stdio.h>
#include <mach.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <cthreads.h>
#include <xkern/include/xkernel.h>
#include <xkern/include/rwlock.h>
#include <xkern/include/prot/ip.h>
#include <s_xkern/include/prot/machripc.h>
#include <s_xkern/include/prot/rrx.h>
#include <s_xkern/include/prot/srx.h>

extern int	tracemachripcp;
extern Map	xkMsgIdMap;  /* port transfer completion indicator */

#define VMADDR_SIZE	4

/*
 * Null_Netport  definition shared with port_maint
 */
extern mnetport Null_Netport;  /* error return value */

static long
ool_copy(
	void	*from,
	void	*to,
	long	len,
	void	*arg)
{
    bcopy(from, to, len);
    return len;
}


/* external  */
mnetport convert_to_netport(
	    mach_port_right_t port, mach_port_type_t right,
	    IPhost dest, msg_id_t msgid, mnportid_t pnn);

/* ***** end temporary decl's ***** */

#define PORT_CHUNK_SIZE	(20 * sizeof(mn_netport_t))

/*
int	item_size_table[MAX_MACH_DATA_TYPE] =
	    { 1, 2, 4, 0, 0, 0, 0, 0, 1, 4, 0, 1 };
int	item_size_table[MAX_MACH_DATA_TYPE] =
	    { 4, 2, 4, 0, 0, 0, 0, 0, 1, 1, 4, 0, 1 };
*/
int	item_size_table[XK_DATA_TYPE_MAX] = XK_DATA_SIZE_VECTOR;

/*
 * Message pop and push utilities
 *
 *   Forward declarations
 *
 */

struct datargs {
    int		arch_index;
    int		data_type;
    int		real_number_arch;
};

#ifdef	LOCALMSGAPPEND
/*
 * local message append function (if msgtool does not support it)
 *
 */
struct msgappendargs {
    Msg		msg;
    Msg		*cmsg;
    Msg		rcmsg;
    char	*buffer;
    char	*buffertail;
    long	*blen;
};
#endif	/* LOCALMSGAPPEND */

#ifdef	PORTLOCKS
/*
 *  push a port net number onto a message; entirely local operation;
 *  this will not go out on the net.
 *
 */
static long
pnumcopy(
	char	*from,
	char	*to,
	long	len,
	void	*arg)
{
    bcopy(from, to, sizeof(mnportid_t *));
    return sizeof(mnetport *);
}
#endif	/* PORTLOCKS */

void	xk_mach_msg_convert(
	    struct mach_msg_big_t *msg, Msg *fulloutmsg,
	    machnetipc_hdr *nethdr, IPhost dest);

mach_msg_header_t *
	xk_ports_and_ool_convert(
	    Msg *netmsg, int mach_netport_count, int mach_notinline_count,
	    msg_id_t msgid, IPhost srchost, int xfercompletion);

mach_msg_header_t *
	xk_netmach_msg_to_mach(
	    Msg *netmsg, int mach_netport_count,
	    int mach_notinline_count, msg_id_t msgid, IPhost srchost,
	    int xfercompletion, mn_arch_tag_t arch_type);

mach_port_t
	convert_netport_to_mach_port(
	    mn_netport_t *netport, XObj self, XObj lower_session,
	    Msg *reply_msg, msg_id_t msgid, IPhost sender);

long	port_hdr_pop_and_swap(
	    mn_netport_t *output, char *input, long len, void *(*argf[])());

static long
	port_append_func(
	    mnetport *input, char *output, int len, struct datargs *arg);

static long
	justcopy(char *input, char *output, long len, void *direction);

static long
	justcopy_andlie(char *input, char *output, long len, void *direction);

#ifdef	LOCALMSGAPPEND
static void
	msgAppendDone(struct msgappendargs *msgarg);
#endif	/* LOCALMSGAPPPEND */

static mach_port_right_t
	set_resend_right_type(unsigned int segtype);

static mach_port_type_t	
	set_resend_outward_port_type(unsigned int segtype);

extern xkern_return_t
	quick_netport_lookup( unsigned int portid, mnetport **n);

/*
 *  no_free --- a nop free function for ool buffers
 */
static void 
no_free_func(
	void	*ptr,
	int	size)
{
}

#if	0
static void
xk_mach_msg_convert_oolJoin(
	Msg		*t_msg,
	char		*data_ptr,
	int		data_size,
	struct ool_list	*ools)
{
    Msg		data_msg;

    msgConstructInplace(
	&data_msg, (char *)data_ptr, data_size, (MsgCIPFreeFunc)mnfree_func);
    msgJoin(t_msg, t_msg, &data_msg);
    msgDestroy(&data_msg);
    ools->data = data_ptr;
    ools->size = data_size;
}
#endif	/* 0 */

static void
xk_mach_msg_convert_portAppend(
	Msg			*t_msg,
	mach_port_t		port,
	mach_msg_type_name_t	name,
	IPhost			dest,
	msg_id_t		msgid
#ifdef	PORTLOCKS
	, Msg			*locked_ports
	, struct msg_attribute	*msg_attr
#endif	/* PORTLOCKS */
	)
{
    mnetport	port_desc;

    port_desc = convert_to_netport(
	port, set_resend_right_type(name), dest, msgid, 0);

    /*
     * save the descriptor address for later unlocking
     */
    if (! bcmp((char *)&port_desc, (char *)&Null_Netport, sizeof(port_desc))) {
	xTrace0(machripcp, TR_SOFT_ERRORS,
	    "machnetipc xk_mach_convert convert_to_netport gave null netport");
#ifdef	PORTLOCKS
    }
    else {
	xTrace1(machripcp, TR_EVENTS,
	   "machnetipc: xk_mach_to_netmach: added port %x to locked ports list",
	    port_desc.net_port_number);
	msgPush(locked_ports, (MStoreFun)pnumcopy, &port_desc.net_port_number,
	    sizeof(mnportid_t), 0);
	msg_attr->num_locked_ports++;
#endif	/* PORTLOCKS */
    }

    port_desc.net_port_rights = set_resend_right_type(name);
    if (name != MACH_MSG_TYPE_PORT_RECEIVE &&
	port != MACH_PORT_NULL) ; /* ZZZ ??? */

    msgAppend(t_msg, (MStoreFun)port_append_func,
	(mn_netport_t *)&port_desc, sizeof(mn_netport_t),
	(void *)0, PORT_CHUNK_SIZE);
}

/*
 * xk_mach_msg_convert (msg, netmsg, nethdr, destination)
 *
 *    creates outgoing message body 
 *
 *    convert all transferred ports, out-of-line data, etc.
 */
void
xk_mach_msg_convert (
	struct mach_msg_big_t	*msg,
	Msg			*fulloutmsg,
	machnetipc_hdr		*nethdr,
	IPhost			dest)
{
    mach_msg_descriptor_t	*msg_ptr =
	(mach_msg_descriptor_t *)((char *)msg + sizeof(mach_msg_header_t));
    mach_msg_size_t		desc_num;
    int				desc_idx;
    long			msg_size;

    int				data_item_number;
    Msg				outdata_dict_msg;
    boolean_t			made_datad = FALSE;

    int				port_item_number;
    Msg				port_dict_msg;
    mn_netport_t		*port_buffer;
    boolean_t			made_portd = FALSE;

    msg_id_t			msgid = nethdr->content.machmsg.message_id;
    int				ports; /* port completion indicator */

    struct msg_attribute	*msg_attr;

#define MAX_OOL_INDEX	32
    struct ool_list		ools[MAX_OOL_INDEX];
    Msg				ool_list_msg;
    int				ool_list_index = 0;

#ifdef PORTLOCKS
    Msg				locked_ports;
#endif	/* PORTLOCKS */

    xTrace0(machripcp, TR_FULL_TRACE, "machnetipc: xk_mach_msg_convert");

    msgConstructEmpty(&ool_list_msg);
    bzero((char *)ools, MAX_OOL_INDEX * sizeof(struct ool_list));

#ifdef PORTLOCKS
    msgConstructEmpty(&locked_ports);
#endif	/* PORTLOCKS */

    msg_attr = (struct msg_attribute *)xMalloc(sizeof(struct msg_attribute));
    bzero((char *)msg_attr, sizeof(struct msg_attribute));

    msg_size = msg->mmhdr.msgh_size - sizeof(mach_msg_header_t);
    data_item_number = 0;
    port_item_number = 0;

    desc_num = ((mach_msg_body_t *)msg_ptr)->msgh_descriptor_count;
    msg_ptr = (mach_msg_descriptor_t *)((char *)msg_ptr + sizeof(mach_msg_body_t));
    msg_size -= sizeof(mach_msg_body_t);

    /*
     * skip if desc_num == 0
     */
    for (desc_idx=1; desc_idx <= desc_num; desc_idx++) {
	if (msg_size < sizeof(mach_msg_descriptor_t)) {
	    xTrace1(machripcp, TR_ERRORS,
		"machnetipc: xk_mach_msg_convert: msg_size(%d) is less than size of a descriptor",
		msg_size);
	    return;
	}

	switch( msg_ptr->type.type) {

	    case MACH_MSG_PORT_DESCRIPTOR: {
		/*
		 *	int		item_name;
		 *	int		item_count;
		 *	mach_port_t	*port_ptr;
		 *
		 *	item_name = msg_ptr->port.disposition;
		 *	item_count = 1;
		 *	port_ptr = &msg_ptr->port.name; 
		 */
		xTrace0(machripcp, TR_DETAILED,
		    "machnetipc: xk_mach_to_netmach port item");
		if (made_portd == FALSE) {
		    msgConstructAppend(
			&port_dict_msg, PORT_CHUNK_SIZE, (char **)&port_buffer);
		    made_portd = TRUE;
		}

		xk_mach_msg_convert_portAppend(
		    &port_dict_msg,
		    msg_ptr->port.name,
		    msg_ptr->port.disposition,
		    dest, msgid 
#ifdef PORTLOCKS
		    , &locked_ports
		    , msg_attr
#endif	/* PORTLOCKS */
		    );
		bcopy((char *)&port_item_number,
		    (char *)&msg_ptr->port.name, NETPORTINDEXSIZE);
		port_item_number++;

		break;
	    }
	    case MACH_MSG_OOL_PORTS_DESCRIPTOR: {
		int		 item_name;
		int		 item_count;
		mach_port_t	*port_ptr;
		int		 j;
		Msg		 data_msg;

		item_name = msg_ptr->ool_ports.disposition;
		item_count = msg_ptr->ool_ports.count;
		port_ptr = msg_ptr->ool_ports.address;
		xTrace3(machripcp, TR_DETAILED,
		    "machnetipc: xk_mach_msg_convert ool port items: name=%x count=%d addr=%x",
		    item_name, item_count, port_ptr);
		if (made_portd == FALSE) {
		    msgConstructAppend(
			&port_dict_msg, PORT_CHUNK_SIZE, (char **)&port_buffer);
		    made_portd = TRUE;
		}
		for (j=0; j < item_count; j++) {
		    xk_mach_msg_convert_portAppend(
			&port_dict_msg,
			*port_ptr,
			item_name,
			dest, msgid 
#ifdef PORTLOCKS
			, &locked_ports
			, msg_attr
#endif	/* PORTLOCKS */
			);

		    bcopy((char *)&port_item_number,
			(char *)port_ptr, NETPORTINDEXSIZE);
		    port_item_number++;
		    port_ptr++;
		} /* end of for(every port in a array) loop */

		/* Same code with OOL DATA item processing */
		/* difference is just length of the item   */

#define	XK_MACH_MSG_CONVERT_OOL(addr_,len_)				\
	    if (made_datad == FALSE) {					\
		msgConstructEmpty(&outdata_dict_msg);			\
		made_datad = TRUE;					\
	    }								\
	    if (ool_list_index == MAX_OOL_INDEX) {			\
		msgAppend(&ool_list_msg, (MStoreFun)ool_copy, ools,	\
		      MAX_OOL_INDEX * sizeof(struct ool_list), 0,	\
		      MAX_OOL_INDEX * sizeof(struct ool_list));		\
		bzero((char *)ools, MAX_OOL_INDEX * sizeof(struct ool_list)); \
		ool_list_index = 0;					\
	    }								\
	    ools[ool_list_index].data = (addr_);			\
	    ools[ool_list_index].size = (len_);				\
	    ool_list_index++;						\
	    msg_attr->num_ools++;					\
	    msgConstructInplace(					\
		&data_msg, (char *)(addr_),				\
		(len_), (MsgCIPFreeFunc)no_free_func);			\
	    msgJoin(&outdata_dict_msg, &outdata_dict_msg, &data_msg);	\
	    msgDestroy(&data_msg);					\
	    bcopy((char *)&data_item_number,				\
		  (char *)&(addr_), NETDATAINDEXSIZE);			\
	    data_item_number++;

		XK_MACH_MSG_CONVERT_OOL (
		    msg_ptr->ool_ports.address,
		    msg_ptr->ool_ports.count * sizeof(mach_port_t *));

		break;
	    }
	    case MACH_MSG_OOL_DESCRIPTOR: {
		Msg		 data_msg;

		xTrace2(machripcp, TR_DETAILED,
		    "machnetipc: xk_mach_msg_convert ool data items: addr=%x size=%x",
		    msg_ptr->out_of_line.address, msg_ptr->out_of_line.size);

		XK_MACH_MSG_CONVERT_OOL (
		    msg_ptr->out_of_line.address,
		    msg_ptr->out_of_line.size
		    );
		break;
	    }
	    default: {
		xTrace1(machripcp, TR_ERRORS,
		    "machnetipc: mach_msg_convert unknown descriptor type %d",
		    msg_ptr->type.type);

	    }
	}	/* end of switch(descriptor type) */
	msg_ptr++;
	msg_size -= sizeof(mach_msg_descriptor_t);
    }	/* end of for(every descriptor) loop */

    /*
     * UNTYPED_DATA processing : None
     * It is included in the received message (msg),
     * and it has been already set in the Msg (fulloutmsg)
     */

    /*
     * TRAILER processing : None
     * It follows the received message (msg), and it should be
     * included in the Msg (fulloutmsg) with the received message
     */

    /*
     * misuse the sequence num field to indicate whether or not
     * a port transfer completion notice will be required
     */
    if (mapResolve(xkMsgIdMap, &msgid, &ports) == XK_SUCCESS) {
	((machnetipc_hdr *)nethdr)->content.machmsg.sequence_num = 1<<ports;
	mapUnbind(xkMsgIdMap, &msgid);
    }

    if (port_item_number ) {
	xTrace1(machripcp, TR_DETAILED,
	    "machnetipc: convert_to_netmsg adds %d ports", port_item_number);
	((machnetipc_hdr *)nethdr)->content.machmsg.netport_count =
	    port_item_number;
	xIfTrace(machripcp, TR_DETAILED) {
	    msgShow(&port_dict_msg);
	    msgShow(fulloutmsg);
	}
	msgJoin(fulloutmsg, &port_dict_msg, fulloutmsg);
	msgDestroy(&port_dict_msg);

	xIfTrace(machripcp, TR_DETAILED) {
	    msgShow(fulloutmsg);
	}
    }
    if (data_item_number) {
	xTrace0(machripcp, TR_DETAILED,
	    "machnetipc: convert_to_netmsg adds data");
	msgJoin(fulloutmsg, fulloutmsg, &outdata_dict_msg);
	msgDestroy(&outdata_dict_msg);
	((machnetipc_hdr *)nethdr)->content.machmsg.notinline_count =
	    data_item_number;
	msgAppend(&ool_list_msg, (MStoreFun)ool_copy, ools,
	    (ool_list_index+1) * sizeof(struct ool_list), 0,
	    (ool_list_index+1) * sizeof(struct ool_list));
    }

#ifdef	PORTLOCKS  
    msg_attr->locked_ports = locked_ports;
#endif	/* PORTLOCKS */
    msg_attr->ool_list_msg = ool_list_msg;
    msgSetAttr(fulloutmsg, 0, (void *)msg_attr, sizeof(struct msg_attribute));

    /* mach_msg_destroy(msg); */
    xIfTrace(machripcp, TR_DETAILED) {
	printf("machnetipc: convert mach to netmach msg dump:\n");
	msgShow(fulloutmsg);
    }
    xTrace0(machripcp, TR_FULL_TRACE,
	"machripc: full convert to netmsg exits");
}


static xkern_return_t
xk_ports_and_ool_convert_disposition_check(
	mach_msg_descriptor_t	*msg_ptr)
{
    switch(msg_ptr->port.disposition) {
	case MACH_MSG_TYPE_PORT_SEND_ONCE: {
	    msg_ptr->port.disposition = MACH_MSG_TYPE_MAKE_SEND_ONCE;
	    return XK_SUCCESS;
	}
	case MACH_MSG_TYPE_PORT_SEND: {
	    msg_ptr->port.disposition = MACH_MSG_TYPE_MOVE_SEND;
	    return XK_SUCCESS;
	}
	case MACH_MSG_TYPE_PORT_RECEIVE: {
	    msg_ptr->port.disposition = MACH_MSG_TYPE_MOVE_RECEIVE;
	    return XK_SUCCESS;
	}
	case MACH_MSG_TYPE_MAKE_SEND_ONCE: {
	    /* ZZZ : This is an error of kernel ?? */
	    /*       MACH_MSG_TYPE_PORT_SEND_ONCE is set in Original */
	    xTrace1(machripcp, TR_ERRORS,
		"machnetipc: xk_ports_and_ool_convert: MACH_MSG_TYPE_MAKE_SEND_ONCE 0x%x shold not come here",
		msg_ptr->port.disposition);
	    msg_ptr->port.disposition = MACH_MSG_TYPE_MAKE_SEND_ONCE;
	    return XK_SUCCESS;
	}
	case MACH_MSG_TYPE_MAKE_SEND:
	case MACH_MSG_TYPE_COPY_SEND: {
	    /* ZZZ : This is an error of kernel */
	    xTrace1(machripcp, TR_ERRORS,
		"machnetipc: xk_ports_and_ool_convert: MACH_MSG_TYPE_MAKE/COPY_SEND 0x%x shold not come here",
		msg_ptr->port.disposition);
	    msg_ptr->port.disposition = MACH_MSG_TYPE_PORT_SEND;
	    return XK_SUCCESS;
	}
	default: {
	    /* ZZZ : This is an error */
	    xTrace1(machripcp, TR_ERRORS,
		"machnetipc: xk_ports_and_ool_convert: Unknown mach_msg_type_name_t 0x%x",
		msg_ptr->port.disposition);
	    /* ZZZ : Must do memory clean up */
	    return XK_FAILURE;
	}
    }
}

static void
xk_ports_and_ool_convert_work_free(
	char	**addr)
{
    if (addr==NULL) return;
    if (*addr==NULL) return;
    xFree(*addr);
    *addr = NULL;
}

#define	XK_PORTS_AND_OOL_FREE()					\
    xk_ports_and_ool_convert_work_free((char **)&port_dict);	\
    xk_ports_and_ool_convert_work_free((char **)&local_port_dict);

/* ZZZ : Is it necessary to free vm_allocate()'d are for ool_data ? */

/*
 * xk_ports_and_ool_convert
 *
 * Converts incoming message from compatible architecture to local form.
 * Convert all transferred ports, out-of-line data, etc.
 * The two header ports have been converted by the caller
 * The incoming message has had the type field and the two mnetport
 * structures popped off.
 */
mach_msg_header_t *
xk_ports_and_ool_convert(
	Msg		*netmsg,
	int		mach_netport_count,
	int		mach_notinline_count,
	msg_id_t	msgid,
	IPhost		srchost,
	int		xfercompletion)
{
    mach_msg_descriptor_t	*msg_ptr;
    mach_msg_size_t	desc_num;
    int			desc_idx;

    long		xk_msg_length;
    int			mach_msg_length;
    mach_msg_header_t	outmsg;
    mach_msg_header_t	*new_mach_msg;
    int			outsegcount = 0;
    mn_netport_t	*port_dict = NULL;
    mach_port_t		*local_port_dict = NULL;

    xTrace0(machripcp, TR_FULL_TRACE, "xk_ports_and_ool_convert");

    /* ZZZ : REMARK : length of netmsg includes trailer length */
    xk_msg_length = msgLen(netmsg);

    xTrace1(machripcp, TR_EVENTS,
	"machnetipc: xk_ports_and_ool_convert: port seg count %d",
	mach_netport_count);
    xIfTrace(machripcp, TR_FULL_TRACE) {
	msgShow(netmsg);
    }

    if (mach_netport_count) {
	int port_size = mach_netport_count * sizeof(mn_netport_t);
	int i;

	port_dict = (mn_netport_t *)xMalloc(port_size);
	local_port_dict =
	    (mach_port_t *)xMalloc(mach_netport_count * sizeof(mach_port_t));
	msgPop(netmsg, (MLoadFun)justcopy, port_dict, port_size, (void *)1);

	xTrace0(machripcp, TR_FULL_TRACE,
	    "machnetipc: xk_ports_and_ool_convert: popped port dictionary");
	if (xfercompletion) {
	    if (xfercompletion & 2) {
		rrxTransferComplete(srchost, msgid);
	    }
	    srxTransferComplete(srchost, msgid);
	}

	for (i=0; i<mach_netport_count; i++) {
	    xTrace2(machripcp, TR_FULL_TRACE,
		"machnetipc: xk_ports_and_ool_convert: port %x right %x",
		port_dict[i].net_port_number,
		port_dict[i].net_port_rights);
	    local_port_dict[i] = 
		convert_netport_to_mach_port(
		    &port_dict[i],
		    0,		/* self */
		    ERR_XOBJ,	/* 0 HASNAIN: lower_session */
		    0,		/* reply_msg */
		    msgid,
		    srchost);
	}
    }

    /*
     * get all the contiguous msg into one clump
     */
    msgPop(netmsg, (MLoadFun)justcopy_andlie, &outmsg,
	sizeof(mach_msg_header_t), (void *)1);

    if (outmsg.msgh_size > 0) {
	/* ZZZ : new_mach_msg might pick up garbage at the end of trailer */
	new_mach_msg =
	    (mach_msg_header_t *)xMalloc(
		ROUND4(outmsg.msgh_size)+MAX_TRAILER_SIZE);
	/* copy entire message into new_mach_msg */
	/* ZZZ : mach_msg_header_t portion is read again */
	msgPop(netmsg, (MLoadFun)justcopy, (char *)new_mach_msg,
	    ROUND4(outmsg.msgh_size)+MAX_TRAILER_SIZE, (void *)1);

	msg_ptr = (mach_msg_descriptor_t *)
	    (((char *)new_mach_msg) + sizeof(mach_msg_header_t));

	mach_msg_length = outmsg.msgh_size - sizeof(mach_msg_header_t);
    }
    else {
	xTrace0(machripcp, TR_ERRORS,
	    "machnetipc: ports_and_ool_convert: length 0");
	XK_PORTS_AND_OOL_FREE();
	return 0;
    }

    xTrace1(machripcp, TR_EVENTS,
	"machnetipc: convert_ports_and_ool_data: length %d", mach_msg_length);

    /* ZZZ : This check has to be done earlier */
    if (xk_msg_length < sizeof(mach_msg_header_t)) {
	xTrace2(machripcp, TR_ERRORS,
	    "machnetipc: convert_ports_and_ool_data: msg: bogus length 0x%x",
	    xk_msg_length, xk_msg_length);
	xk_msg_length = 0;
	XK_PORTS_AND_OOL_FREE();
	return 0;
    }

    /*
     * scan through once for all msginline conversions
     */

    /* ZZZ : This function is called when the message is COMPLEX */
    /*       Is following check necessary ?    - Yes	     */
    if (!(outmsg.msgh_bits && MACH_MSGH_BITS_COMPLEX)) {
	xTrace1(machripcp, TR_ERRORS,
	    "machnetipc: convert_ports_and_ool_data: msg: no cplx flag in 0x%x",
	    outmsg.msgh_bits);
	XK_PORTS_AND_OOL_FREE();
	return 0;
    }

    desc_num = ((mach_msg_body_t *)msg_ptr)->msgh_descriptor_count;
    mach_msg_length -= sizeof(mach_msg_body_t);
    msg_ptr = (mach_msg_descriptor_t *)
	(((char *)msg_ptr) + sizeof(mach_msg_body_t));

    for (desc_idx=1; desc_idx<=desc_num; desc_idx++) {
	if (mach_msg_length < sizeof(mach_msg_descriptor_t)) {
	    xTrace2(machripcp, TR_ERRORS,
		"machnetipc: convert_ports_and_ool_data: msg: mach_msg_length(%d/%d) < size of descriptor",
		mach_msg_length, outmsg.msgh_size);
	    XK_PORTS_AND_OOL_FREE();
	    return 0;
	}

	/*	mach_netport_count  num of all transfered ports */
	/*	local_port_dict	    points converted mach_port_t array */
	switch(msg_ptr->type.type) {
	    case MACH_MSG_PORT_DESCRIPTOR: {
		int		 local_port_idx;
		mach_port_t	 local_port;

		xTrace4(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert: port descriptor=%x name=0x%x disp=%d type=%d",
		    msg_ptr, msg_ptr->port.name, msg_ptr->port.disposition,
		    msg_ptr->port.type);

		if (xk_ports_and_ool_convert_disposition_check(msg_ptr)
			== XK_FAILURE) {
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}

		local_port_idx = (int)msg_ptr->port.name;
			/* Sender has set the number */
		xTrace1(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert: port index %d",
		    local_port_idx);
		if (0 <= local_port_idx &&
		    local_port_idx < mach_netport_count) {
		    msg_ptr->port.name = local_port_dict[local_port_idx];
		}
		else {
		    msg_ptr->port.name = MACH_PORT_NULL;
		}
		xTrace2(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert: port %x is in msg at %x",
		    msg_ptr->port.name, msg_ptr);
		break;
	    }
	    case MACH_MSG_OOL_PORTS_DESCRIPTOR: {
		int		 local_port_idx;
		mach_port_t	*port_array_ptr;
		vm_size_t	 port_array_size;
		kern_return_t	 kr;
		int		 i;

		xTrace4(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert: ool_ports descriptor=%x addr=0x%x count=%d disp=%d",
		    msg_ptr, msg_ptr->ool_ports.address,
		    msg_ptr->ool_ports.disposition,
		    msg_ptr->ool_ports.disposition);
		xTrace4(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert: ool_ports descriptor=%x copy=%d dealloc=%d type=%d",
		    msg_ptr, msg_ptr->ool_ports.copy,
		    msg_ptr->ool_ports.deallocate, msg_ptr->ool_ports.type);

		if (xk_ports_and_ool_convert_disposition_check(msg_ptr)
			== XK_FAILURE) {
		    /* ZZZ : ERROR message can be skipped */
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}

		/* ZZZ : outsegcount is accumulated just for
		 * error check purpose
		 */
		if (outsegcount >= mach_notinline_count) {
		    xTrace2(machripcp, TR_ERRORS,
			"machnetipc: xk_ports_and_ool_convert: ool_ports: actual ool count=%d exceeds input=%d",
			outsegcount, mach_notinline_count);
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}
		else {
		    outsegcount++;
		}

/* ZZZ : New allocation option is not supported yet */
#define	VM_ALLOCATE_ANYWHERE	1

		port_array_size =
		    msg_ptr->ool_ports.count * sizeof(mach_port_t);
		if ((kr = vm_allocate(
		    mach_task_self(),
		    (vm_address_t *)&port_array_ptr,
		    port_array_size,
		    VM_ALLOCATE_ANYWHERE))
		    != KERN_SUCCESS) {
		    xTrace1(machripcp, TR_ERRORS,
			"machnetipc: xk_ports_and_ool_convert: ool_ports: memory shortage size=0x%x",
			port_array_size);
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}
		msgPop(netmsg, (MLoadFun)justcopy, port_array_ptr,
		    port_array_size, (void *)1);

		/* ZZZ : local_port_dict[] cannot be diverted from work to
		 * real use
		 *  for (i=0; i<msg_ptr->ool_ports.count; i++) {
		 *		port_array_ptr[i] =
		 *		    local_port_dict[port_array_ptr[i]];
		 *	}
		 * or ...
		 *	local_port_idx = (int)port_array_ptr[0];
		 *	for (i=0; i<msg_ptr->ool_ports.count; i++) {
		 *		port_array_ptr[i] =
		 *		    local_port_dict[local_port_idx+i];
		 *	}
		 * or ... : which way is fastest ?
		 */

		bcopy((char *)&local_port_dict[(int)*port_array_ptr],
		    (char *)port_array_ptr,
		    port_array_size);
		msg_ptr->ool_ports.address = port_array_ptr;
		msg_ptr->ool_ports.deallocate = 1;
		msg_ptr->ool_ports.copy = MACH_MSG_VIRTUAL_COPY;
		break;
	    }
	    case MACH_MSG_OOL_DESCRIPTOR: {
		vm_address_t	ool_data_ptr;
		vm_size_t	ool_data_size;
		kern_return_t	kr;

		xTrace3(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert: ool_data descriptor=%x addr=0x%x size=0x%x",
		    msg_ptr, msg_ptr->out_of_line.address,
		    msg_ptr->out_of_line.size);
		xTrace4(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert: ool_data descriptor=%x copy=%d dealloc=%d type=%d",
		    msg_ptr, msg_ptr->out_of_line.copy,
		    msg_ptr->out_of_line.deallocate, msg_ptr->out_of_line.type);

		/*
		 * ZZZ : outsegcount is accumulated just for error check purpose
		 */
		if (outsegcount >= mach_notinline_count) {
		    xTrace2(machripcp, TR_ERRORS,
			"machnetipc: xk_ports_and_ool_convert: ool_data: actual ool count=%d exceeds input=%d",
			outsegcount, mach_notinline_count);
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}
		else {
		    outsegcount++;
		}

		ool_data_size = msg_ptr->out_of_line.size;
		if ((kr = vm_allocate(
			mach_task_self(),
			(vm_address_t *)&ool_data_ptr,
			ool_data_size,
			VM_ALLOCATE_ANYWHERE))
		    != KERN_SUCCESS) {
		    xTrace1(machripcp, TR_ERRORS,
			"machnetipc: xk_ports_and_ool_convert: ool_data: memory shortage size=0x%x",
			ool_data_size);
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}
		msgPop(netmsg, (MLoadFun)justcopy, (void *)ool_data_ptr,
		    ool_data_size, (void *)1);
		msg_ptr->out_of_line.address = (void *)ool_data_ptr;
		msg_ptr->out_of_line.deallocate = 1;
		msg_ptr->out_of_line.copy = MACH_MSG_VIRTUAL_COPY;
		break;
	    }
	    default: {
		xTrace2(machripcp, TR_ERRORS,
		    "machnetipc: xk_ports_and_ool_convert: invalid descriptor=0x%x type=%d",
		    msg_ptr, msg_ptr->type.type);
		XK_PORTS_AND_OOL_FREE();
		return 0;
	    }
	} /* End of switch(descriptor type) */
	msg_ptr++;
	mach_msg_length -= sizeof(mach_msg_descriptor_t);
    } /* End of for(every descriptor) loop */

    /* ZZZ : outsegcount is accumulated just for error check purpose */
    xIfTrace(machripcp, TR_ERRORS) {
	if (outsegcount < mach_notinline_count) {
	    xTrace2(machripcp, TR_ERRORS,
		"machnetipc: xk_ports_and_ool_convert: actual ool count=%d is short of input=%d",
		outsegcount, mach_notinline_count);
	}
    }

    /* ZZZ : Leave Untyped data as it is */
    /* ZZZ : Leave Trailer as it is */
    xTrace0(machripcp, TR_DETAILED,
	"machnetipc: xk_ports_and_ool_convert: end of descriptors processing");

    XK_PORTS_AND_OOL_FREE();
    xTrace0(machripcp, TR_EVENTS,
	"machnetipc: xk_ports_and_ool_convert: normal return");
    return (mach_msg_header_t *)new_mach_msg;
}

/*
 * set_resend_outward_port_type
 *
 *    message going to net needs standard port type
 */

static mach_port_type_t
set_resend_outward_port_type(
	mach_port_type_t	segtype)  /* mach msg name type */
{
    switch ( segtype ) {
	case MACH_MSG_TYPE_MOVE_SEND_ONCE:
	case MACH_MSG_TYPE_MAKE_SEND_ONCE:
	    return MACH_MSG_TYPE_PORT_SEND_ONCE;

	case MACH_MSG_TYPE_COPY_SEND:
	case MACH_MSG_TYPE_MAKE_SEND:
	    return MACH_MSG_TYPE_PORT_SEND;

	case MACH_MSG_TYPE_MOVE_RECEIVE:
	    return MACH_MSG_TYPE_PORT_RECEIVE;
	default:
	    return segtype;
    }
}

/*
 * set_resend_outward_port_type
 *
 *    message going to net needs standard port type
 */

static mach_port_right_t
set_resend_right_type(
	mach_port_type_t	segtype)  /* mach msg name type */
{
    switch ( segtype ) {
	case MACH_MSG_TYPE_MOVE_SEND_ONCE:
	case MACH_MSG_TYPE_MAKE_SEND_ONCE:
	    return MACH_PORT_TYPE_SEND_ONCE;

	case MACH_MSG_TYPE_COPY_SEND:
	case MACH_MSG_TYPE_MAKE_SEND:
	case MACH_MSG_TYPE_MOVE_SEND:
	    return MACH_PORT_TYPE_SEND;

	case MACH_MSG_TYPE_MOVE_RECEIVE:
	    return MACH_PORT_TYPE_RECEIVE;

	default:
	    xTrace1(machripcp, TR_ERRORS,
		"machnetipc: set_resend_right_type: unknown seg type: %x",
		segtype);
	    return segtype;
    }
}


#if	XK_HETERO_SUPPORT
/*
 *  port_hdr_pop_and_swap
 */

long port_hdr_pop_and_swap(
	mn_netport_t	*outptr,
	char		*inptr,
	long		len,
	void		*(*argf[])())
{
    int	i;

    xTrace0(machripcp, TR_FULL_TRACE,
	"machnetipc: port_hdr_pop_and_swap starts");

    /* GENERALIZE for size of elements */

    for (i=0; i < len; i += sizeof(mn_netport_t)) {
	bcopy(inptr, (char *)&outptr->net_port_number, PORTID_NETLEN);
	outptr->net_port_number =
	    (int)argf[PORTID_NETTYPE]((char *)&outptr->net_port_number);
	inptr += PORTID_NETLEN;

	bcopy (inptr, (char *)&outptr->net_port_rights, PORTRIGHT_NETLEN);
	outptr->net_port_rights =
	    (int)argf[PORTRIGHT_NETTYPE]((char *)&outptr->net_port_rights);
	inptr += PORTRIGHT_NETLEN;

	bcopy (inptr, (char *)&outptr->receiver_host_addr, HOSTNETLEN);
	inptr += HOSTNETLEN;

	bcopy (inptr, (char *)&outptr->make_send_count, NETMAKESENDSIZE);
	outptr->make_send_count =
	    (int)argf[NETMAKESENDTYPE]((char *)&outptr->make_send_count);
	inptr += NETMAKESENDSIZE;

	outptr++;
    }

    return(len);
}

#else	/* !XK_HETERO_SUPPORT */

long port_hdr_pop_and_swap(
	mn_netport_t	*outptr,
	char		*inptr,
	long		len,
	void		*(*argf[])())
{
    int	i;

    xTrace0(machripcp, TR_FULL_TRACE,
	"machnetipc: port_hdr_pop_and_swap non_hetero starts");

    /* GENERALIZE for size of elements */

    for (i=0; i < len; i += sizeof(mn_netport_t)) {
	bcopy(inptr, (char *)&outptr->net_port_number, PORTID_NETLEN);
	inptr += PORTID_NETLEN;

	bcopy (inptr, (char *)&outptr->net_port_rights, PORTRIGHT_NETLEN);
	inptr += PORTRIGHT_NETLEN;

	bcopy (inptr, (char *)&outptr->receiver_host_addr, HOSTNETLEN);
	inptr += HOSTNETLEN;

	bcopy (inptr, (char *)&outptr->make_send_count, NETMAKESENDSIZE);
	inptr += NETMAKESENDSIZE;

	outptr++;
    }
    return(len);
}

#endif	/* XK_HETERO_SUPPORT */

/*
 *  port_append_func
 *
 *    move a brief description of the port to the outgoing message
 *
 */
static long
port_append_func(
	mnetport	*input,
	char		*output,
	int		len,
	struct datargs	*arg)
{
    bcopy((char *)input, output, sizeof(mn_netport_t));
    return sizeof(mn_netport_t);
}

#ifdef LOCALMSGAPPEND
static void
msgAppendLocal(
	struct msgappendargs	*msgarg,
	void			(*func)(),
	void			*data,
	long			length,
	long			newlength)
{
    char	*fmsg;
    char	*cmsg;
    char	*buffer;
    long	blen;
    char	*buffertail;

    fmsg = &msgarg->msg;
    cmsg =  msgarg->cmsg;
    buffer = msgarg->buffer;
    blen = msgarg->blen;
    buffertail = msgarg->buffertail;

    if (!buffer) {
	msgConstructAllocate(fmsg, newlength, &msgarg->buffer);
	msgarg->blen = newlength;
	msgarg->buffertail = msgarg->buffer;
	msgarg->cmsg = fmsg;
    }
    if (buffertail + length > buffer + blen) {
	msgTruncate(cmsg, buffertail - buffer);
	if (cmsg != fmsg) msgJoin(fmsg, fmsg, cmsg);
	msgConstructAllocate(&msgarg->rcmsg, newlength, &msgarg->buffer);
	msgarg->blen = newlength;
	msgarg->buffertail = msgarg->buffer;
	msgarg->cmsg = &msgarg->rcmsg;
	cmsg = msgarg->cmsg;
	buffer = msgarg->buffer;
    }
    bcopy(data, buffer, length);
}

static void
msgAppendDone(
	struct msgappendargs	*msgarg)
{
    if (!msgarg->buffer) {
	msgConstructEmpty(&msgarg->msg);
    }
    else {
	msgTruncate(msgarg->cmsg, msgarg->buffertail - msgarg->buffer);
	msgJoin(&msgarg->msg, &msgarg->msg, msgarg->cmsg);
	msgarg->buffer = 0;
    }
}
#endif	/* LOCALMSGAPPEND */


/*
 * justcopy
 *
 *
 */
static long 
justcopy(
	char	*input,
	char	*output,
	long	len,
	void	*direction)
{
    xTrace5(machripcp, TR_DETAILED,
	"justcopy: %x %x %ld (0x%lx) dir %d",
	input, output, len, len, direction);

    if (!direction) {
	bcopy(input, output, len);
    }
    else {
	bcopy(output, input, len);
    }
    return len;
}

/*
 * justcopy_andlie
 *
 *
 */
static long
justcopy_andlie(
	char	*input,
	char	*output,
	long	len,
	void	*direction)
{
    xTrace4(machripcp, TR_DETAILED,
	"justcopy_andlie: %x %x dir %d len %ld",
	input, output, direction, len);

    if (!direction) {
	bcopy(input, output, len);
    }
    else {
	bcopy(output, input, len);
    }
    return 0;
}

#define	UNPERMUTE_INT32(int32)	unpermute_int32[arch_index]((char *)&(int32));
#define	UNPERMUTE_INT32_INPLACE(int32)	(int)(int32) = UNPERMUTE_INT32(int32);

/*
 * xk_netmach_msg_to_mach
 *
 * Converts incoming message from other architecture to local form.
 * Convert all transferred ports, out-of-line data, byte swaps, etc.
 * The two header ports have been converted by the caller
 * The incoming message has had the type field and the two mnetport
 * structures popped off.
 * 
 */
mach_msg_header_t *
xk_netmach_msg_to_mach(
	Msg		*netmsg,
	int		mach_netport_count,
	int		mach_notinline_count,
	msg_id_t	msgid,
	IPhost		srchost,
	int	    	xfercompletion,
	mn_arch_tag_t	arch_type)
#if	XK_HETERO_SUPPORT
{
    mach_msg_descriptor_t	*msg_ptr;
    mach_msg_size_t	desc_num;
    int			desc_idx;
    long		xk_msg_length;
    int			mach_msg_length;
    mach_msg_header_t	outmsg;
    mach_msg_header_t	*new_mach_msg;
    int			outsegcount = 0;
    mn_netport_t	*port_dict = NULL;
    mach_port_t		*local_port_dict = NULL;
    void		(*convec[MAX_MACH_DATA_TYPE])();
    int			arch_index = arch_unpermute_index(arch_type);

    xTrace0(machripcp, TR_FULL_TRACE, "xk_ports_and_ool_convert_hetero");

    /*
     * might as well just have one for each architecture type all ready
     */
    set_convert_vector(arch_type, (convert_func_t *)convec);

    xk_msg_length = msgLen(netmsg);

    xTrace1(machripcp, TR_EVENTS,
	"machnetipc: xk_ports_and_ool_convert_hetero: port seg count %d",
	mach_netport_count);
    xIfTrace(machripcp, TR_FULL_TRACE) {
	msgShow(netmsg);
    }

    if (mach_netport_count) {
	int	port_size = mach_netport_count * sizeof(mn_netport_t);
	int	i;

	port_dict = (mn_netport_t *)xMalloc(port_size);
	local_port_dict =
	    (mach_port_t *)xMalloc(mach_netport_count * sizeof(mach_port_t));

	/*
	 **** byte swap ports: check again ****
	 */
	msgPop(netmsg, (MLoadFun)port_hdr_pop_and_swap,
	    port_dict, port_size, convec);

	xTrace0(machripcp, TR_FULL_TRACE,
	    "machnetipc: xk_ports_and_ool_convert_hetero: popped port dictionary");
	if (xfercompletion) {
	    if (xfercompletion & 2) rrxTransferComplete(srchost, msgid);
	    srxTransferComplete(srchost, msgid);
	}

	for (i=0; i<mach_netport_count; i++) {
	    xTrace2(machripcp, TR_FULL_TRACE,
		"machnetipc: xk_ports_and_ool_convert_hetero: port %x right %x",
		port_dict[i].net_port_number,
		port_dict[i].net_port_rights);
	    local_port_dict[i] = 
		convert_netport_to_mach_port(
		    &port_dict[i],
		    0,		/* self */
		    ERR_XOBJ,	/* lower_session */
		    0,		/* reply_msg */
		    msgid,
		    srchost);
	}
    }

    /*
     * get all the contiguous msg into one clump
     */
    msgPop(netmsg, (MLoadFun)justcopy_andlie, &outmsg,
	sizeof(mach_msg_header_t), (void *)1);

    /*
     **** byte swap message header - I just need the size field *****
     */
    if (arch_index) {
	outmsg.msgh_size =
	    unpermute_int32[arch_index]((char *)&outmsg.msgh_size);
    }

    if (outmsg.msgh_size > 0) {
	new_mach_msg = (mach_msg_header_t *)
	    xMalloc(ROUND4(outmsg.msgh_size)+MAX_TRAILER_SIZE);
	msgPop(netmsg, (MLoadFun)justcopy, (char *)new_mach_msg,
	    ROUND4(outmsg.msgh_size)+MAX_TRAILER_SIZE, (void *)1);

	/*
	 * update the header fields: bits, local and remote port
	 * will be set outside this routine
	 */
	new_mach_msg->msgh_size = outmsg.msgh_size;
	new_mach_msg->msgh_id =
	    unpermute_int32[arch_index]((char *)&outmsg.msgh_id);
	msg_ptr = (mach_msg_descriptor_t *)
	    (((char *)new_mach_msg) + sizeof(mach_msg_header_t));
	mach_msg_length = outmsg.msgh_size - sizeof(mach_msg_header_t);
    }
    else {
	xTrace0(machripcp, TR_ERRORS,
	    "machnetipc: xk_ports_and_ool_convert_hetero: length 0");
	XK_PORTS_AND_OOL_FREE();
	return 0;
    }

    xTrace1(machripcp, TR_EVENTS,
	"machnetipc: xk_ports_and_ool_convert_hetero: length %d",
	mach_msg_length);
    if (xk_msg_length < sizeof(mach_msg_header_t)) {
	xTrace2(machripcp, TR_ERRORS,
	    "machnetipc: xk_ports_and_ool_convert_hetero: msg: bogus length %d dec(%x hex)",
	    xk_msg_length, xk_msg_length);
	xk_msg_length = 0;
	XK_PORTS_AND_OOL_FREE();
	return 0;
    }

    /*
     * scan through once for all msginline conversions
     */

    UNPERMUTE_INT32_INPLACE(
	((mach_msg_body_t *)msg_ptr)->msgh_descriptor_count);
    desc_num = ((mach_msg_body_t *)msg_ptr)->msgh_descriptor_count;
    mach_msg_length -= sizeof(mach_msg_body_t);
    msg_ptr = (mach_msg_descriptor_t *)
	(((char *)msg_ptr) + sizeof(mach_msg_body_t));

    for (desc_idx=1; desc_idx<=desc_num; desc_idx++) {
	if (mach_msg_length < sizeof(mach_msg_descriptor_t)) {
	    xTrace2(machripcp, TR_ERRORS,
		"machnetipc: xk_ports_and_ool_convert_hetero: msg: mach_msg_length(%d/%d) < size of descriptor",
		mach_msg_length, outmsg.msgh_size);
	    XK_PORTS_AND_OOL_FREE();
	    return 0;
	}

	/*
	 *	mach_netport_count  num of all transfered ports
	 *	local_port_dict	    points converted mach_port_t array
	 * ZZZ : Is it true that msg_ptr->type.type doesn't need conversion ?
	 */
	switch(msg_ptr->type.type) {
	    case MACH_MSG_PORT_DESCRIPTOR: {
		int		 local_port_idx;
		mach_port_t	 local_port;

		xTrace4(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert_hetero: port descriptor=%x name=0x%x disp=%d type=%d",
		    msg_ptr, msg_ptr->port.name, msg_ptr->port.disposition,
		    msg_ptr->port.type);

		UNPERMUTE_INT32_INPLACE(msg_ptr->port.name);
		/*
		 * ZZZ : msg_ptr->port.disposition doesn't need conversion
		 */
		if (xk_ports_and_ool_convert_disposition_check(msg_ptr)
		    == XK_FAILURE) {
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}

		local_port_idx = (int)msg_ptr->port.name;
		    /* Sender has set the index */
		xTrace1(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert_hetero: port index %d",
		    local_port_idx);
		if (0 <= local_port_idx
		    &&   local_port_idx < mach_netport_count) {
		    msg_ptr->port.name = local_port_dict[local_port_idx];
		}
		else {
		    msg_ptr->port.name = MACH_PORT_NULL;
		}
		xTrace2(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert_hetero: port %x is in msg at %x",
		    msg_ptr->port.name, msg_ptr);
		break;
	    }

	    case MACH_MSG_OOL_PORTS_DESCRIPTOR: {
		int		local_port_idx;
		mach_port_t	*port_array_ptr;
		vm_size_t	port_array_size;
		kern_return_t	kr;
		int		i;

		UNPERMUTE_INT32_INPLACE(msg_ptr->ool_ports.address);
		UNPERMUTE_INT32_INPLACE(msg_ptr->ool_ports.count);

		xTrace4(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert_hetero: ool_ports descriptor=%x addr=0x%x count=%d disp=%d",
		    msg_ptr,
		    msg_ptr->ool_ports.address,
		    msg_ptr->ool_ports.disposition,
		    msg_ptr->ool_ports.disposition);
		xTrace4(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert_hetero: ool_ports descriptor=%x copy=%d dealloc=%d type=%d",
		    msg_ptr,
		    msg_ptr->ool_ports.copy,
		    msg_ptr->ool_ports.deallocate,
		    msg_ptr->ool_ports.type);

		if (xk_ports_and_ool_convert_disposition_check(msg_ptr)
		    == XK_FAILURE) {
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}

		/*
		 * ZZZ : outsegcount is accumulated just for error check purpose
		 */
		if (outsegcount >= mach_notinline_count) {
		    xTrace2(machripcp, TR_ERRORS,
			"machnetipc: xk_ports_and_ool_convert_hetero: ool_ports: actual ool count=%d exceeds input=%d",
			outsegcount, mach_notinline_count);
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}
		else {
		    outsegcount++;
		}

		port_array_size =
		    msg_ptr->ool_ports.count * sizeof(mach_port_t);
		if ((kr = vm_allocate(
			mach_task_self(), (vm_address_t *)&port_array_ptr,
			port_array_size, VM_ALLOCATE_ANYWHERE))
		    != KERN_SUCCESS) {
		    xTrace1(machripcp, TR_ERRORS,
			"machnetipc: xk_ports_and_ool_convert_hetero: ool_ports: memory shortage size=0x%x",
			port_array_size);
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}
		msgPop(netmsg, (MLoadFun)justcopy, port_array_ptr,
		    port_array_size, (void *)1);
		/*
		 *ZZZ:local_port_dict[] cannot be diverted from work to real use
		 *
		 *  for (i=0; i<msg_ptr->ool_ports.count; i++) {
		 *	port_array_ptr[i] =
		 *	    local_port_dict[UNPERMUTE_INT32(port_array_ptr[i])];
		 *	}
		 *
		 * or ...
		 *  local_port_idx = (int)UNPERMUTE_INT32(port_array_ptr[0]);
		 *	for (i=0; i<msg_ptr->ool_ports.count; i++) {
		 *	    port_array_ptr[i] =
		 *		local_port_dict[local_port_idx+i];
		 *	}
		 *
		 * or ...
		 * Don't use port index stored in msgPop()ed port_array
		 *	Which way is the fastest ?
		 */
		local_port_idx = UNPERMUTE_INT32(port_array_ptr[0]);
		bcopy((char *)&local_port_dict[local_port_idx],
		    (char *)port_array_ptr,
		    port_array_size);
		msg_ptr->ool_ports.address = port_array_ptr;
		msg_ptr->ool_ports.deallocate = 1;
		break;
	    }

	    case MACH_MSG_OOL_DESCRIPTOR: {
		vm_address_t	ool_data_ptr;
		vm_size_t	ool_data_size;
		kern_return_t	kr;

		UNPERMUTE_INT32_INPLACE(msg_ptr->out_of_line.size);
		xTrace3(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert_hetero: ool_data descriptor=%x addr=0x%x size=0x%x",
		    msg_ptr,
		    msg_ptr->out_of_line.address,
		    msg_ptr->out_of_line.size);
		xTrace4(machripcp, TR_DETAILED,
		    "machnetipc: xk_ports_and_ool_convert_hetero: ool_data descriptor=%x copy=%d dealloc=%d type=%d",
		    msg_ptr,
		    msg_ptr->out_of_line.copy,
		    msg_ptr->out_of_line.deallocate,
		    msg_ptr->out_of_line.type);

		if (outsegcount >= mach_notinline_count) {
		    xTrace2(machripcp, TR_ERRORS,
			"machnetipc: xk_ports_and_ool_convert_hetero: ool_data: actual ool count=%d exceeds input=%d",
			outsegcount, mach_notinline_count);
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}
		else {
		    outsegcount++;
		}

		ool_data_size = msg_ptr->out_of_line.size;
		if ((kr = vm_allocate(
		    mach_task_self(),
		    (vm_address_t *)&ool_data_ptr,
		    ool_data_size,
		    VM_ALLOCATE_ANYWHERE))
		    != KERN_SUCCESS) {
		    xTrace1(machripcp, TR_ERRORS,
			"machnetipc: xk_ports_and_ool_convert_hetero: ool_data: memory shortage size=0x%x",
			ool_data_size);
		    XK_PORTS_AND_OOL_FREE();
		    return 0;
		}
		msgPop(netmsg, (MLoadFun)justcopy,
		    (void *)ool_data_ptr, ool_data_size, (void *)1);
		msg_ptr->out_of_line.address = (void *)ool_data_ptr;
		msg_ptr->out_of_line.deallocate = 1;
		break;
	    }

	    default: {
		xTrace2(machripcp, TR_ERRORS,
		    "machnetipc: xk_ports_and_ool_convert_hetero: invalid descriptor=0x%x type=%d",
		    msg_ptr, msg_ptr->type.type);
		XK_PORTS_AND_OOL_FREE();
		return 0;
	    }
	}	/* End of switch(descriptor type) */
	msg_ptr++;
	mach_msg_length -= sizeof(mach_msg_descriptor_t);
    }	/* End of for(every descriptor) loop */

    /*
     * ZZZ : outsegcount is accumulated for error check purpose
     */
    xIfTrace(machripcp, TR_ERRORS) {
	if (outsegcount < mach_notinline_count) {
	    xTrace2(machripcp, TR_ERRORS,
		"machnetipc: xk_ports_and_ool_convert_hetero: actual ool count=%d is short of input=%d",
		outsegcount, mach_notinline_count);
	}
    }

    xTrace0(machripcp, TR_DETAILED,
	"machnetipc: xk_ports_and_ool_convert_hetero: end of descriptors processing");

    /*
     * Leave Untyped data as it is
     */

    /*
     * Convert fields in Trailer
     */
    {
	mach_msg_format_0_trailer_t *	trailer_ptr;

	trailer_ptr = (mach_msg_format_0_trailer_t *)
	    ((char *)new_mach_msg + ROUND4(outmsg.msgh_size));
	UNPERMUTE_INT32_INPLACE(trailer_ptr->msgh_trailer_type);
	UNPERMUTE_INT32_INPLACE(trailer_ptr->msgh_trailer_size);

	if (trailer_ptr->msgh_trailer_type == MACH_MSG_TRAILER_FORMAT_0) {
	    if (trailer_ptr->msgh_trailer_size
		>= sizeof(mach_msg_seqno_trailer_t)) {
		UNPERMUTE_INT32_INPLACE(trailer_ptr->msgh_seqno);
	    }
	    if (trailer_ptr->msgh_trailer_size
		>= sizeof(mach_msg_security_trailer_t)) {
		UNPERMUTE_INT32_INPLACE(trailer_ptr->msgh_sender.val[0]);
		UNPERMUTE_INT32_INPLACE(trailer_ptr->msgh_sender.val[1]);
	    }
	}
	else {
	    xTrace1(machripcp, TR_ERRORS,
		"machnetipc: xk_ports_and_ool_convert_hetero: invalid trailer type %x",
		trailer_ptr->msgh_trailer_type);
	    /*
	     * ZZZ : do nothing, because we don't check it in homogeneous case
	     */
	}
    }

    XK_PORTS_AND_OOL_FREE();
    xTrace0(machripcp, TR_EVENTS,
	"machnetipc: xk_ports_and_ool_convert_hetero: normal return");
    return (mach_msg_header_t *)new_mach_msg;
}
#else	/* !XK_HETERO_SUPPORT */
{
    xTrace0(machripcp, TR_ERRORS,
	"machnetipc: xk_ports_and_ool_convert_hetero: not supported");
}
#endif	/* !XK_HETERO_SUPPORT */

