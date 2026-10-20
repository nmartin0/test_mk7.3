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
 * srx_i.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.10
 * Date: 1993/02/01 22:34:49
 */
/* 
 * Mach Send Right Transfer protocol (SRX)
 */

#ifndef srx_i_h
#define srx_i_h

#include <xkern/include/prot/bidctl.h>
#include <s_xkern/include/prot/xfer.h>
#include <s_xkern/include/prot/srx.h>

/* ZZZ : Following will be devided into global and local */
#include <s_xkern/include/prot/machripc.h>
#include <s_xkern/include/prot/machripc_xfer.h>


/* 
 * request codes
 */
typedef enum {
    SEND_LOCK_REQ = 1,
    UNLOCK_NO_TRANSFER,
    UNLOCK_WITH_TRANSFER,
    SEND_RIGHT_TRANSFER
} SrxMsgType;

#define srxMsgTypeStr(_t) 					\
   ((_t) == SEND_LOCK_REQ ? "LOCK REQUEST" :			\
    (_t) == UNLOCK_NO_TRANSFER ? "UNLOCK NO XFER" :		\
    (_t) == UNLOCK_WITH_TRANSFER ? "UNLOCK WITH XFER" :		\
    (_t) == SEND_RIGHT_TRANSFER ? "TRANSFER MSG" :		\
    "UNKNOWN")

/* 
 * reply codes
 */
typedef enum {
    SRX_SUCCESS	= 1,
    SRX_FAILURE
} SrxReplyCode;


typedef struct {
    SrxMsgType		type;
    mnetport		*port;
    union {
	struct {
	    XferHost	nsr;
	} lock;
	struct {
	    MsgId	msgId;
	    XferHost	rr;
	    int		sendCount;
	} xfer;
    } u;
} SrxReqOutgoing;

typedef struct {
    SrxMsgType		type;
    NetPortNumber	portNumber;
    XferHost		osr;
    mn_arch_tag_t	archTag;
    union {
	struct {
	    XferHost	nsr;
	} lock;
	struct {
	    MsgId	msgId;
	    XferHost	rr;
	    int		sendCount;
	} xfer;
    } u;
} SrxReqMsg;


typedef struct {
    SrxMsgType		type;
    SrxReplyCode	replyCode;
    union {
	struct {
	    int		sendCount;
	} lock;
    } u;
} SrxRepMsg;


typedef enum {
    SRX_TRANSFERRED,
    SRX_LOCKING_FAILURE,
    SRX_DESTINATION_FAILURE
} srx_return_t;


extern	XferHost	srxMyXferHost;
extern	int		tracesrxp;

xkern_return_t
	srxLoadReply( SrxRepMsg *, Msg * );

xkern_return_t
	srxLoadRequest( SrxReqMsg *, Msg * );

void	srxStoreReply( SrxRepMsg *, Msg * );

void	srxStoreRequest( SrxReqOutgoing *, Msg * );

#endif	/* !srx_i_h */
