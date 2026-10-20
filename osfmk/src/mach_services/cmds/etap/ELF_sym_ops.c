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
 */
/*
 * cmk1.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "sym_ops.h"

#define ELF_NM "elfnm -p"


struct st_entry *
symbol_table_load(char *s_file, int *entries)
{
    FILE *f;
    char type, name[1024];
    struct st_entry *ent;
    int readi, writei, addr, n, next_entry, alloced_entries;

    strcat(strcpy(name, ELF_NM " "), s_file);
    if ((f = popen(name, "r")) == NULL)
	return NULL;
    next_entry = alloced_entries = 0;
    ent = NULL;
    while ((n = fscanf(f, "%x%1s%s", &addr, &type, name)) == 3) {
	if (type != 'T')
	    continue;
	if (next_entry >= alloced_entries) {
	    alloced_entries = alloced_entries * 2 + 1024;
	    ent = realloc(ent, alloced_entries * sizeof *ent);
	    if (ent == NULL) {
		perror("st_load: realloc:");
		return NULL;
	    }
	}
	strcpy(ent[next_entry].f_name, name);
	ent[next_entry].f_addr = addr;
	next_entry++;
    }
    if (n != EOF) {
	while ((n = getc(f)) != '\n' && n != EOF)
	    putchar(n);
	putchar('\n');
	fprintf(stderr, "st_load: unexpected format in nm output\n");
	return NULL;
    }
    if (next_entry == 0) {
	fprintf(stderr, "no symbols found\n");
	return NULL;
    }
    qsort(ent, next_entry, sizeof *ent, table_compare);
    /* Suppress duplicate entries. */
    writei = 1;
    for (readi = 1; readi < next_entry; readi++) {
	if (ent[readi].f_addr != ent[readi - 1].f_addr) {
	    if (readi != writei)
		ent[writei] = ent[readi];
	    writei++;
	}
    }
    *entries = next_entry == 0 ? 0 : writei;
    return ent;
}


int
table_compare(const void *a, const void *b)
{
    return ((struct st_entry *)a)->f_addr - ((struct st_entry *)b)->f_addr;
}


struct st_entry *
funky_find(struct st_entry *front, int last, u_long addr, u_long *offset)
{
    int first, mid;

    first = 0;
    while ((mid = (first + last) / 2) > first) {
	if (front[mid].f_addr > addr)
	    last = mid;
	else
	    first = mid;
    }
    *offset = addr - front[first].f_addr;
    return &front[first];
}


#if 0	/* Test harness. */
int
main(int argc, char **argv)
{
    struct st_entry *ent, *found;
    int i, nent;
    u_long offset;

    if (argc != 2) {
	fprintf(stderr, "Usage: test kernel-object-name\n");
	return 1;
    }
    ent = symbol_table_load(argv[1], &nent);
    if (ent == NULL)
	return 1;
    for (i = 0; i < nent; i++) {
	found = funky_find(ent, nent, ent[i].f_addr, &offset);
	if (found != &ent[i] || offset != 0) {
	    fprintf(stderr, "find %s (value %x) found %s (value %x)\n",
		    ent[i].f_name, ent[i].f_addr, found->f_name, found->f_addr);
	    return 1;
	}
	found = funky_find(ent, nent, ent[i].f_addr + 1, &offset);
	if (found != &ent[i] || offset != 1) {
	    fprintf(stderr, "find %s (value %x + 1) found %s (value %x)\n",
		    ent[i].f_name, ent[i].f_addr, found->f_name, found->f_addr);
	    return 1;
	}
    }
    printf("%d entries successfully checked\n", nent);
    return 0;
}
#endif	/* 0 */
