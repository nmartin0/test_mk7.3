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
 * usage : mcpboot [-o <output>] <input>
 *
 * output : LanAI program to boot on the MYRICOM board
 * input  : LanAI program (executable)
 *
 * mcpboot produce a bootable image of a LanAI program
 *
 * Caveats : mcpboot only understands COFF executable programs and DAT files.
 */

#include <stdlib.h>
#include <stdio.h>
#include "mcpboot.h"
#include "coff.h"
#include "dat.h"

char *buffer;		/* Current input read buffer */
unsigned int buflen;	/* Current input read length in buffer */

char *progname;		/* Program name */
char *dst;		/* Bootable lanAI program */

struct objfmt_switch objfmt_switch[] = {
    { "COFF",	coff_recog,	coff_boot },	/* COFF format */
    { "DAT",	dat_recog,	dat_boot },	/* DAT format */
    { (char *)0 }
};

/*
 * Forward declarations
 */
static void usage(void
		  );

/*
 * Display mcpboot usage
 */
static void
usage(void)
{
    fprintf(stderr, "usage: %s [-o <output>] <input>\n", progname);
}

/*
 * Main routine
 */
void
main(
    unsigned int argc,
    char **argv)
{
    struct objfmt_switch *fmt;

    progname = argv[0];
    argc--;
    argv++;

    if (argc == 0) {
	usage();
	exit(MCPBOOT_EXIT_USAGE);
    }
    
    /*
     * Parse arguments
     */
    while (argc > 0 && (*argv)[0] == '-') {
	switch ((*argv)[1]) {
	case 'o':
	    if (dst != (char *)0 ||
		((*argv)[2] == '\0' && argc == 1)) {
		usage();
		exit(MCPBOOT_EXIT_USAGE);
	    }

	    if ((*argv)[2] == '\0') {
		if (argc == 1) {
		    usage();
		    exit(MCPBOOT_EXIT_USAGE);
		}
		argc--;
		dst = *++argv;
	    } else
		dst = &(*argv)[3];
	    break;

	default:
	    usage();
	    exit(MCPBOOT_EXIT_USAGE);
	}

	argc--;
	argv++;
    }

    /*
     * Set up input file descriptor
     */
    if (argc != 1) {
	usage();
	exit(MCPBOOT_EXIT_USAGE);
    }

    if (freopen(*argv, "rb", stdin) == (FILE *)0) {
	fprintf(stderr, "%s: Unable to open input '%s'\n", progname, *argv);
	exit(MCPBOOT_EXIT_INVALID_ARGUMENT);
    }

    /*
     * Set up output file descriptor
     */
    if (dst != (char *)0 && freopen(dst, "wb", stdout) == (FILE *)0) {
	fprintf(stderr, "%s: Unable to open output '%s'\n", progname, dst);
	exit(MCPBOOT_EXIT_INVALID_ARGUMENT);
    }

    /*
     * Recognize and load input
     */
    for (fmt = objfmt_switch; fmt->name != (char *)0; fmt++)
	if ((*fmt->recog)()) {
	    fprintf(stderr, "Bootable image built from %s format ",
		    fmt->name); 
	    exit((*fmt->boot)());
	}

    fprintf(stderr, "%s: Unrecognized input object format \n", progname);
    exit(MCPBOOT_EXIT_UNKNOWN_FORMAT);
}
