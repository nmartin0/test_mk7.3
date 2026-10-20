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
 * ether_i.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.3
 * Date: 1993/02/02 00:01:39
 */

#ifndef ether_i_h
#define ether_i_h

#define	EMAXPAK	1266
#define	EADLEN	6

#define	EHLEN	14
#define	EDLEN	EMAXPAK-EHLEN

#define RCVBUFSIZE	50*1024		/* increased size - menze */

struct eheader {
    ETHhost		e_dest;
    ETHhost		e_src;
    short		e_ptype;
};

struct epacket {
    struct eheader	hdr;
    char		data[EDLEN];
};

typedef u_short	PassiveID;

typedef ETHaddr	ActiveID;

#endif	/* !ether_i_h */
