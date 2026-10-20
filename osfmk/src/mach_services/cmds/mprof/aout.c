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

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <stdio.h>
#include <a.out.h>
#include "mprof.h"
#include <mach/boot_info.h>

aout_get_symbols ()
{
	struct	exec exec;	/* exec header for an a.out file */
	unsigned strsz;		/* symbol table size */
	char	*strtab;	/* symbol table */
	unsigned nsyms;
	struct nlist *nlist;
	register i;
	int rc = 0;
	int machboot = 0;

	if (debug)
		fprintf(stderr, "aout_get_symbols\n");

	rewind(symbol_file);

retry:
	/* Read exec header */

	if (fread(&exec, sizeof(exec), 1, symbol_file) != 1) {
		perror(symbol_file_name);
		return(0);
	}

	if (N_BADMAG(exec)) {
		return(0);
	}

	if (exec.a_data == 0 && exec.a_bss == 0 && exec.a_syms == 0) {
		if (debug)
			fprintf(stderr, "mach.boot format\n");
		machboot = 1;
		goto retry;
	}	
	/* get symbol table */

	if (debug)
		fprintf(stderr, "symbol offset %x\n", N_SYMOFF(exec));

	if (fseek(symbol_file, N_SYMOFF(exec), SEEK_SET) != 0) {
		perror(symbol_file_name);
		quit(1, "Can not read symbol table\n");
	}

	if (machboot) /* Add size of general header + boot_info + sym_header */
		fseek(symbol_file,
		      sizeof(struct exec) + sizeof (struct boot_info) +
		      sizeof(int), SEEK_CUR);

	nsyms = exec.a_syms/sizeof (struct nlist);

	if (debug)
		fprintf(stderr, "nsyms %x a_syms %x\n", nsyms, exec.a_syms);

	nlist = (struct nlist *) malloc(exec.a_syms);

	if (nlist == NULL) {
		perror("malloc");
		quit(1, "Could not allocate space for symbol table\n");
	}

	if (fread(nlist, sizeof(struct nlist) , nsyms, symbol_file) != nsyms) {
		perror(symbol_file_name);
		quit(1, "Can not read symbol table\n");
	}

	/* get string table size */

	if (fread(&strsz, sizeof (strsz), 1, symbol_file) != 1) {
		perror(symbol_file_name);
		quit(1, "Can not read string table size\n");
	}

	if (debug)
		fprintf(stderr, "strsz %x \n", strsz);


	/* Allocate and read string table */

	strtab = (char *)malloc(strsz);

	if (strtab == NULL) {
		perror("malloc");
		quit(1, "Could not allocate space for string table");
	}

	strsz -= sizeof(strsz);

	if (fread(strtab+sizeof(strsz), 1 , strsz, symbol_file) != strsz) {
		perror(symbol_file_name);
		quit(1, "Can not read string table\n");
	}

	symbols = (struct symbol *)malloc(nsyms * sizeof (struct symbol));

	if (symbols == NULL) {
		perror("malloc");
		quit(1, "Could not allocate space for symbol table");
	}

	for (i = 0; i < nsyms; i++) {
		if (/*nlist[i].n_type != N_TEXT && */nlist[i].n_type != N_TEXT+N_EXT)
			continue;
		symbols[nsymbols].name = strtab + nlist[i].n_un.n_strx;
		symbols[nsymbols].pc = nlist[i].n_value;
		symbols[nsymbols].count = 0;
		nsymbols++;		
	}

	return(1);
}
