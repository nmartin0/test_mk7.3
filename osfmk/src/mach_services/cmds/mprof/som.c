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

#include <stdio.h>
#include <mach.h>
#include <sys/types.h>
#include <sys/file.h>
#include <machine/exec.h>
#include "mprof.h"
#include <mach/boot_info.h>

boolean_t som_get_symbols(FILE *, char *);
boolean_t (*machine_get_symbols)(FILE *, char *) = som_get_symbols;

boolean_t 
som_get_symbols (symbol_file, symbol_file_name)
FILE *symbol_file;
char *symbol_file_name;
{
	struct header filehdr;
	unsigned nsyms;
	char *strings;
	int i;

	if (debug)
		fprintf(stderr, "som_get_symbols\n");

	rewind(symbol_file); 

	/* Read exec header */

	if (fread(&filehdr, sizeof(struct header), 1, symbol_file) != 1) {
		perror(symbol_file_name);
		return(0);
	}

	if(filehdr.system_id != CPU_PA_RISC1_0 && filehdr.system_id != CPU_PA_RISC1_1)
		return 0;

	if (debug)
		fprintf(stderr, "som header found\n");

	nsyms = filehdr.symbol_total;

	if (debug)
		fprintf(stderr, "%d symbols\n", nsyms);

	if (fseek(symbol_file, filehdr.symbol_strings_location, SEEK_SET) != 0) {
		perror(symbol_file_name);
		quit(1, "Can not read strings\n");
	}

	if (debug)
		fprintf(stderr, "strings_size: 0x%x %d\n",
			filehdr.symbol_strings_size,
			filehdr.symbol_strings_size);

	if((strings = (char*)malloc(filehdr.symbol_strings_size)) == NULL)
	{
		perror("malloc");
		quit(1, "Could not allocate space for strings\n");
	}

	if (debug) {
		fprintf(stderr, "strings: 0x%x \n",
			strings);
	}

#if 1	/* fread doesn't work if asked to read >8192 bytes. */
	for (i = 0; i < filehdr.symbol_strings_size; i += 8192) {
		if (fread(strings+i,
			  (filehdr.symbol_strings_size - i > 8192) ?
			  	8192 : filehdr.symbol_strings_size - i ,
			  1, symbol_file) != 1) {
			perror(symbol_file_name);
			return(0);
		}
	}
#else
	if (fread(strings, 1, filehdr.symbol_strings_size, symbol_file) !=
	    filehdr.symbol_strings_size) {
		perror(symbol_file_name);
		return(0);
	}
#endif	
	if (fseek(symbol_file, filehdr.symbol_location, SEEK_SET) != 0) {
		perror(symbol_file_name);
		quit(1, "Can not read symbol dictionary\n");
	}

	symbols = (struct symbol *)malloc(nsyms * sizeof (struct symbol));
	if (symbols == NULL) {
		perror("malloc");
		quit(1, "Could not allocate space for symbol table");
	}

	if (debug)
		fprintf(stderr, "symbols: 0x%x \n",
			symbols);

	nsymbols = 0;

	for(i = 0; i < nsyms; i++)
	{
		struct symbol_dictionary_record sym;

		if (fread(&sym, sizeof(sym), 1, symbol_file) != 1) {
			perror(symbol_file_name);
			return(0);
		}		

		if(sym.symbol_type == ST_ENTRY)	
		{
			char *name;
			name = strings + sym.name.n_strx;
			symbols[nsymbols].name = name;
			symbols[nsymbols].pc = sym.symbol_value & ~3;
			symbols[nsymbols].count = 0;
#if 0
			if (debug)
				fprintf(stderr, "%8x <%s> (%x) index %x\n",
					symbols[nsymbols].pc,
					name, name, sym.name.n_strx);
#endif
			nsymbols++;
		}

	}
	return 1;
}
