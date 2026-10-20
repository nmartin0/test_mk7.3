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
 * process_msg.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 */

#ifndef eth_support_h
#define eth_support_h

#include <xkern/include/list.h>
#include <xkern/include/msg.h>
#include <device/net_status.h>


typedef struct eth_input_buffer {
    struct list_entry	q;
    char                hdr[NET_HDW_HDR_MAX];
    Msg_s               upmsg;
    char                *data;  	/* will contain mach_hdrs for out-of-kernel */
    XObj		driverProtl;	/* The protocol currently using this buffer */
#if XK_DEBUG
    int                 id;
#endif /* XK_DEBUG */
} EthInputBuffer;


/*
 * Packet filter declarations.
 */
struct mach_hdrs {
  mach_msg_header_t	msg_hdr;
  NDR_record_t		NDR;
  char			header[NET_HDW_HDR_MAX];
  mach_msg_type_number_t packetCnt;
  struct packet_header	packet_header;
};


typedef struct {
    mutex_t		lock;
    condition_t		notEmpty;
    list_t		list;
    int                 empty;
} ProtectedQueue;

#define pqEmpty(_pq)	(_pq).empty

#define pqRemove(_pq, _buf)						\
{									\
    mutex_lock( (_pq).lock );						\
    while ( ((_buf) = (EthInputBuffer *)delist_head_strong((_pq).list))	\
	   == 0 ) {							\
	condition_wait( (_pq).notEmpty, (_pq).lock );			\
    }									\
    if ( (_pq).list->head == 0 ) { (_pq).empty = 1; } 			\
    mutex_unlock( (_pq).lock );						\
}

#define pqAppend(_pq, _buf)			\
{						\
    mutex_lock( (_pq).lock );			\
    enlist( (_pq).list, (list_entry_t)(_buf) );	\
    (_pq).empty = 0;				\
    condition_signal( (_pq).notEmpty);		\
    mutex_unlock( (_pq).lock );			\
}

typedef struct {
    ProtectedQueue	inQueue;
    ProtectedQueue	freeQueue;
    char		*name;
    void		*internal;
    XkThreadPolicy_s	policy;
} EthInputPool;


EthInputPool	*EthInPoolInit(
			       u_int numBuffers,
			       u_int numThreads,
			       u_int dataSz,
			       Path,
			       char *,
			       XkThreadPolicy);

void		EthInPoolStats( EthInputPool *p );

#if  XK_DEBUG
void		xkPoolCheck( EthInputPool *p );
#else
#define 	xkPoolCheck( foo )
#endif  /* XK_DEBUG */

#endif /* ! eth_support_h */

