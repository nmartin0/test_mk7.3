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
 *  ssr.h
 *
 *  x-kernel v3.2
 *
 *  Types shared between SSR protocol and server/client tasks
 */

#ifndef	ssr_h
#define	ssr_h

#ifndef SSRMODULE
extern mach_port_t	server_registry_port;
#endif

#define SSR_SERVER_NAME	"simple_server"

#define SERVICE_MAX	 25
#define SERVICE_MIN	  1
#define MAX_SSR_DATA	500
#define SS_NAME_SIZE    100

enum ssr_operation_type
{ 
    REGISTER,
    UNREGISTER,
    SHUTDOWN,
    REQUEST,
    REPLY,
    ABSORB,
    REQUEST_SEND_PORT,
    REQUEST_SIMPLE_REPLY,
    PRIORITY
};

enum ssr_service_type
{ 
    POSTCARD=SERVICE_MIN,
    SS_TELNET,
    SS_FTP,
    SS_HURRAY,
    SS_COMPLEX,
    SS_MOVE_RECEIVE,
    SS_OOL
};

/*
 * size of a byte in bits
 */
#define MNBYTESIZE	8

struct ssrdata {
    enum ssr_service_type	service_id;
    enum ssr_operation_type	operation;
    int		sequence_num;	    /* optional; server should copy to reply */
    IPhost	destination_host;
    int		data_length;	    /* if there is data to forward */
};

struct local_ssr_msg {
    mach_msg_header_t	mmhdr;		/* the Mach portions */
    struct ssrdata	ssrd;		/* for the registry  */
    char		data[MAX_SSR_DATA];
					/* forwarded to the server */
};

/*
 * the server registry forwards replies that look like this
 * the server will cast them to its local type (postcard, udp, etc.)
 */
struct server_msg {
    mach_msg_header_t   mmhdr;
    char		server_data[MAX_SSR_DATA];
};

#endif	/* !ssr_h */
