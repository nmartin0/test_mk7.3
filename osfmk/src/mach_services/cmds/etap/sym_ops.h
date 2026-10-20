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
 *  Symbol table entry definition
 */

#ifndef _ETAP_SYM_OPS_H_
#define _ETAP_SYM_OPS_H_

#include "config.h"

struct st_entry {
    char		f_name[FUNCTION_NAME_SIZE];
    unsigned long	f_addr;
};


/* protos */

int table_compare(
        const void *a,
        const void *b);

void print_symbol_table(
        struct st_entry *table,
        int    size);

struct st_entry* funky_find(
        struct st_entry	*front,
        int		 size,
        unsigned long	 addr,
        unsigned long	*offset);

struct st_entry* symbol_table_load(
        char s_file[],
        int  *entries);


#endif /* _ETAP_SYM_OPS_H_ */
