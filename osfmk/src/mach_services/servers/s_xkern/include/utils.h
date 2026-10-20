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
 * x_util.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.3
 * Date: 1993/02/01 22:40:06
 */

#ifndef utils_h
#define utils_h

#include <xkern/include/xtype.h>

extern xkern_return_t	xkInit( int argc, char **argv );
extern void		Delay( int );
extern void		Kabort( char * );

/*
 * from kern/queue.h
 */
struct queue_entry {
        struct queue_entry      *next;          /* next element */
        struct queue_entry      *prev;          /* previous element */
};

typedef struct queue_entry      *queue_t;
typedef struct queue_entry      queue_head_t;
typedef struct queue_entry      queue_chain_t;
typedef struct queue_entry      *queue_entry_t;



/* 
 * non-ANSI compilers gripe about the _n##U usage while GCC gives
 * warning messages about the u_long cast.
 */
#define UNSIGNED(_n)	_n##U


#endif	/* !utils_h */
