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
 * nns_internal.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993  Arizona Board of Regents
 *
 *
 * Revision: 1.4
 * Date: 1993/11/08 16:34:21
 */

#ifndef	nns_internal_h
#define	nns_internal_h

/*
 * number of max/min threads to process requests
 */
#define NETNAME_THREADS_MIN	5
#define NETNAME_THREADS_MAX	20

/*
 * max size of service buffer
 */
#define MAX_MSG_BUF		256

/*
 * maximum bcast replies honored
 */
#define MAX_BCAST_REPLIES	20

/*
 * the indices of the lower level protocols
 */
#define UDP_I			0
#define BOOTID_I		1

/*
 * message types
 */
#define REQUEST_MODE		1
#define REPLY_MODE		2

/*
 * network name service name
 */
#define NETWORK_NAME_SERVICE	"NetworkNameServer"

/*
 * broadcast timout in ms
 */
#define BCAST_TIMEOUT		100
#define BCAST_WAKEUPS		200

/*
 * specific host timeout in ms
 */
#define HOST_TIMEOUT		100
#define HOST_WAKEUPS		200

typedef struct nns_bcast
{
    char	mode;
    int		id;
    IPhost	client;
    IPhost	server;
    char	key[80];
} BCAST_MSG;

typedef struct nns_bcast_replies
{
    IPhost	server;
} BCAST_REPLY;

typedef struct pstate
{
    XObj	self;
    XObj	udp;
    XObj	udp_sessn;
    IPhost	myaddr;
} PSTATE;

void    nns_null_function(void *, int);




#endif	/* !nns_internal_h */
