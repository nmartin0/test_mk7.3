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

#ifndef linux

#include <stdio.h>
#include <mach_o_format.h>
#include <mach_o_header.h>
#include <sys/types.h>
#include "mprof.h"

rose_get_symbols ()
{
	int rc;
	void *in_bufp;
	mo_header_t mo_header;
	ldc_header_t **ldc_headers;
	int i, n_load_cmds;
	char *cmds;
	int max_nsyms = 0;

	if (debug)
		fprintf(stderr, "rose_get_symbols\n");

	if (!(in_bufp = (void *) malloc(MO_SIZEOF_RAW_HDR))) {
		perror("rose raw header");
		quit(1);
	}
	

	rewind(symbol_file);

	/* read raw header */

	if (fread(in_bufp, 1, MO_SIZEOF_RAW_HDR, symbol_file)
	    != MO_SIZEOF_RAW_HDR) {
    		perror(symbol_file_name);
		return(0);
	}

	/* decode it */

	rc = decode_mach_o_hdr (in_bufp, MO_SIZEOF_RAW_HDR, MOH_HEADER_VERSION,
			   &mo_header);

	if (rc != MO_HDR_CONV_SUCCESS) {
		fprintf(stderr, "decode_mach_o_hdr returns %d\n", rc);
    		return 0;
	}

	/* read load commands */

	if (!(cmds = (char *) malloc(mo_header.moh_sizeofcmds))) {
		perror("rose load commands");
		quit(1);
	}

	if (fseek(symbol_file, mo_header.moh_first_cmd_off, SEEK_SET) != 0) {
	  	perror("rose load commands");
		quit(1);
	}
	if (fread(cmds, 1, mo_header.moh_sizeofcmds, symbol_file)
	    != mo_header.moh_sizeofcmds) {
    		perror("rose laod commands");
		quit(1);
	}
	
	n_load_cmds = mo_header.moh_n_load_cmds;
	if (debug)
		fprintf(stderr, "rose_get_symbols: %d load cmds\n",
			n_load_cmds);
	if (!(ldc_headers = (ldc_header_t **) malloc(n_load_cmds *
					  sizeof(ldc_header_t **)))) {
		perror("rose load commands");
		quit(1);
	}

	/* Store command offsets */

	for (i=0; i < n_load_cmds; i++) {
		ldc_headers[i] = (ldc_header_t *)cmds;
		cmds = cmds + ldc_headers[i]->ldci_cmd_size;
	}

	/* first pass to get number of symbols */

	for (i=0; i < n_load_cmds; i++) 
	    if (ldc_headers[i]->ldci_cmd_type == LDC_SYMBOLS) {
		   symbols_command_t *sym_cmd;
		   sym_cmd = (symbols_command_t *)ldc_headers[i];
		   max_nsyms +=  sym_cmd->symc_nentries;
	    }

	if (!(symbols =
	      (struct symbol *)malloc(max_nsyms * sizeof (struct symbol)))) {
    	    perror("symbol table");
	    quit(1);
	}

	for (i=0; i < n_load_cmds; i++) 
	    if (ldc_headers[i]->ldci_cmd_type == LDC_SYMBOLS) {
		   symbols_command_t *sym_cmd;
		   ldc_header_t *string_hd;

		   sym_cmd = (symbols_command_t *)ldc_headers[i];
		   string_hd = ldc_headers[sym_cmd->symc_strings_section];
		   rose_load_symbols(ldc_headers[i]->ldci_section_off,
				     sym_cmd->symc_nentries,
				     string_hd->ldci_section_off,
				     string_hd->ldci_section_len);
	    }

	return(1);
}

rose_load_symbols(sym_sec, nsyms, string_sec, nchars)
mo_offset_t sym_sec, string_sec;
{
	symbol_info_t *sym_info;
	char *strings;
	int i;

	if (debug)
		fprintf(stderr, "rose_load_symbols: %d symbols, %d bytes string\n", nsyms, nchars);

	/* load symbol section */

	if (!(sym_info =
	      (symbol_info_t *) malloc(sizeof(symbol_info_t) * nsyms))) {
		perror("rose load symbol section");
		quit(1);
	}

	if (fseek(symbol_file, sym_sec, SEEK_SET) != 0) {
	  	perror("rose load symbol section");
		quit(1);
	}
	
	if (fread(sym_info, sizeof(symbol_info_t), nsyms, symbol_file)
	    != nsyms) {
    		perror("rose load symbol section");
		quit(1);
	}

	/* load strings */

	if (!(strings =
	      (char *) malloc(nchars))) {
		perror("rose load string section");
		quit(1);
	}

	if (fseek(symbol_file, string_sec, SEEK_SET) != 0) {
	  	perror("rose load string section");
		quit(1);
	}
	
	if (fread(strings, 1 , nchars, symbol_file)
	    != nchars) {
    		perror("rose load string section");
		quit(1);
	}
	for (i = 0; i < nsyms; i++) {
	    char *name = &strings[sym_info[i].si_name.symbol_name];
	    if (debug)
	        fprintf(stderr, "%d: %x %s %x\n", i,
			sym_info[i].si_value.abs_val,
			name,
			sym_info[i].si_flags );
	    if ((sym_info[i].si_flags & (SI_ABSOLUTE_VALUE_F | SI_CODE_F ))
	           != (SI_ABSOLUTE_VALUE_F | SI_CODE_F ))
	        continue;
	    if (*name == 0)
	        continue;
	    if (!strcmp("gcc_compiled.", name))
		continue;
	    symbols[nsymbols].name = name;
	    symbols[nsymbols].pc = (int)sym_info[i].si_value.abs_val;
	    symbols[nsymbols].count = 0;
	    nsymbols++;		
	}
}
#endif	/* linux */
