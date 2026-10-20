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
 * xk_clock.h	- clock_trace data capturing interfaces
 */

#ifndef xk_clock_h
#define xk_clock_h

#define	NMS_PROFILE_SERVER	"nms_profile_server"

#define	CLOCKPROF_PRINT		1
#define	CLOCKPROF_REPLY		2

typedef struct clock_trace_entry {
    tvalspec_t			time;
    char			*name;
} ct_entry_t;

typedef struct clock_prof_reply_msg {
    mach_msg_header_t		Head;
    mach_msg_body_t		Body;
    mach_msg_ool_descriptor_t	Ool_tbl;
    mach_msg_ool_descriptor_t	Ool_ids;
    NDR_record_t		NDR;
    int				Resolution;
} clock_prof_reply_msg_t;

#endif	/* !xk_clock_h */
