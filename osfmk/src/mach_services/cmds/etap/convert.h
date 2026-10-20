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
 *  File: convert.h
 *
 *  Author: Joe CaraDonna
 *
 *  Function: Header file for convertion modes
 *
 */

#ifndef _ETAP_CONVERT_H_
#define _ETAP_CONVERT_H_

#include <mach/etap.h>

#define LOCK_NAMES       0
#define SUBSYS_NAMES     1
#define ALL_NAMES        2

extern int text_to_short( 
	int		 search, 
	int		*index, 
	char		 object_name_list[MAX_OBJECTS][EVENT_NAME_LENGTH], 
	unsigned short	 object_list[]);

extern void file_name_conv (
	char		*file,
	char		*new,
	int		 cpu);

#endif /* _ETAP_CONVERT_H_ */
