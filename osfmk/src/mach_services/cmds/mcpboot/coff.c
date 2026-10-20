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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mcpboot.h"
#include "coff.h"

/*
 * Sections to find in the COFF header
 */
struct section {
    const char		*name;		/* section name */
    unsigned int	 paddr;		/* physical address */
    unsigned int	 size;		/* section size */
    unsigned int	 scnptr;	/* file ptr to raw data for section */
    unsigned int	 flags;		/* type and contents flags */
} section[] = {
    { ".text", },
    { ".data", },
    { ".bss", },
    { (char *)0 },
};

/*
 * Recognize a coff format
 */
int
coff_recog(
    void)
{
    FILHDR *hdr;
    char *lbuf;

    /*
     * Allocate buffer to read the COFF file header
     */
    if (buflen < FILHSZ) {
	lbuf = malloc(FILHSZ);
	if (buflen > 0) {
	    memcpy(lbuf, buffer, buflen);
	    free (buffer);
	}
	buffer = lbuf;

	/*
	 * Read COFF file header
	 */
	buflen += fread(&buffer[buflen], 1, FILHSZ - buflen, stdin);
	if (buflen < FILHSZ)
	    return (0);
    }

    /*
     * Validate COFF file header
     */
    hdr = (FILHDR *)buffer;
    return (COFF_INT2(hdr->f_magic) == LANAI3MAGIC);
}

/*
 * Produce a bootable format
 */
int
coff_boot(
    void)
{
    char *lbuf;
    unsigned opthdr;
    unsigned nscns;
    FILHDR *hdr;
    SCNHDR *scn;
    unsigned len;
    unsigned i;
    unsigned first = 1;
    struct section *sp, *ssp;
    char databuf[BUFSIZ];

    /*
     * Initialization
     */
    hdr = (FILHDR *)buffer;
    nscns = COFF_INT2(hdr->f_nscns);
    opthdr = COFF_INT2(hdr->f_opthdr);
    len = FILHSZ + opthdr + nscns * SCNHSZ;

    /*
     * Load all sections
     */
    if (buflen < len) {
	lbuf = malloc(len);
	memcpy(lbuf, buffer, buflen);
	free (buffer);
	buffer = lbuf;

	buflen += fread(&buffer[buflen], 1, len - buflen, stdin);
	if (buflen < len) {
	    fprintf(stderr, "\n%s : Cannot fread(stdin) all sections\n",
		    progname);
	    return (MCPBOOT_EXIT_INVALID_FORMAT);
	}
    }

    /*
     * Find each section and initialize section array
     */
    hdr = (FILHDR *)buffer;
    for (sp = section; sp->name != (char *)0; sp++) {
	scn = (SCNHDR *)((char *)(hdr + 1) + opthdr);
	for (i = 0; i < nscns; i++, scn++) {
	    if (strncmp((char *)scn->s_name, sp->name, 8) == 0) {
		sp->paddr = COFF_INT4(scn->s_paddr);
		sp->size = COFF_INT4(scn->s_size);
		sp->flags = COFF_INT4(scn->s_flags);
		sp->scnptr = COFF_INT4(scn->s_scnptr);
		break;
	    }
	}
    }

    /*
     * Load each section in address order
     */
    for (;;) {
	/*
	 * Look for next section
	 */
	ssp = (struct section *)0;
	for (sp = section; sp->name != (char *)0; sp++) {
	    if (sp->size == 0)
		continue;
	    if (ssp == (struct section *)0 || sp->paddr < ssp->paddr)
		ssp = sp;
	}
	if (ssp == (struct section *)0)
	    break;
	if (first)
	    first = 0;
	else
	    fprintf(stderr, "+");
	fprintf(stderr, "%d", ssp->size);

	/*
	 * Relocate next section
	 */
	if (fseek(stdin, ssp->scnptr, SEEK_SET) != 0) {
	    fprintf(stderr, "\n%s : Cannot fseek(stdin)\n", progname);
	    return (MCPBOOT_EXIT_INVALID_FORMAT);
	}
	if (fseek(stdout, ssp->paddr, SEEK_SET) != 0) {
	    fprintf(stderr, "\n%s : Cannot fseek(stdout)\n", progname);
	    return (MCPBOOT_EXIT_INVALID_FORMAT);
	}

	/*
	 * Load next section
	 */
	while (ssp->size > 0) {
	    len = ssp->size < BUFSIZ ? ssp->size : BUFSIZ;
	    if (ssp->flags & STYP_BSS)
		memset(databuf, '\0', len);
	    else if (fread(databuf, 1, len, stdin) != len) {
		fprintf(stderr, "\n%s : Cannot fread(stdin)\n", progname);
		return (MCPBOOT_EXIT_INVALID_FORMAT);
	    }
	    if (fwrite(databuf, 1, len, stdout) != len) {
		fprintf(stderr, "\n%s : Cannot fread(stdout)\n", progname);
		return (MCPBOOT_EXIT_INVALID_FORMAT);
	    }
	    ssp->size -= len;
	}
    }

    fprintf(stderr, "\n");
    return (MCPBOOT_EXIT_OK);
}
