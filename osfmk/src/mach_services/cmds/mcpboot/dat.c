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
#include "dat.h"

/*
 * Forward declarations
 */
int
dat_read_half(int *,
	      int stop,
	      unsigned *,
	      unsigned
    );

/*
 * Recognize a DAT format
 */
int
dat_recog(
    void)
{
    char *lbuf;

    /*
     * Allocate buffer to read the DAT file header
     */
    if (buflen < sizeof (DAT_ENTRY) - 1) {
	lbuf = malloc(sizeof (DAT_ENTRY) - 1);
	if (buflen > 0) {
	    memcpy(lbuf, buffer, buflen);
	    free (buffer);
	}
	buffer = lbuf;

	/*
	 * Read DAT file header
	 */
	buflen += fread(&buffer[buflen], 1,
			sizeof (DAT_ENTRY) - 1 - buflen, stdin);
	if (buflen < sizeof (DAT_ENTRY) - 1)
	    return (0);
    }

    /*
     * Validate DAT file header
     */
    return (strncmp(DAT_ENTRY, buffer, sizeof (DAT_ENTRY) - 1) == 0);
}

/*
 * Produce a bootable format
 */
int
dat_boot(
    void)
{
    int c;
    int section;
    int val[17];
    unsigned int n;
    unsigned int i;
    unsigned int len;
    unsigned int start;
    unsigned int state;
    unsigned int first;

    if (fseek(stdin, sizeof (DAT_ENTRY) - 1, SEEK_SET) != 0) {
	fprintf(stderr, "\n%s : Cannot fseek(stdin)\n", progname);
	return (MCPBOOT_EXIT_INVALID_FORMAT);
    }

    first = 1;
    state = section = 0;
    while (!ferror(stdin)) {
	c = getchar();
	if (c == EOF) {
	    if (feof(stdin) && state == 0 && section == DAT_SYMBOL)
		break;
	    fprintf(stderr, "\n%s: Unexpected EOF\n", progname);
	    return (MCPBOOT_EXIT_INVALID_FORMAT);
	}

	switch (state) {
	case 0:
	    if (c != '@') {
		fprintf(stderr, "\n%s: Incorrect section header\n", progname);
		return (MCPBOOT_EXIT_INVALID_FORMAT);
	    }
	    state = 1;
	    continue;

	case 1:
	    switch(c) {
	    default:
		fprintf(stderr, "\n%s:Unknown section\n", progname);
		return (MCPBOOT_EXIT_INVALID_FORMAT);

	    case DAT_TEXT:
	    case DAT_DATA:
	    case DAT_BSS:
	    case DAT_SYMBOL:
		section = c;
		break;
	    }
	    state = 2;
	    continue;

	case 2:
	    if (c != ' ') {
		fprintf(stderr, "\n%s: Incorrect section trailer\n", progname);
		return (MCPBOOT_EXIT_INVALID_FORMAT);
	    }
	    start = 0;
	    if (section == DAT_TEXT ||
		section == DAT_DATA ||
		section == DAT_BSS) {
		/*
		 * Read start specification
		 */
		n = 17;
		switch (dat_read_half(val, ' ', &n, 0)) {
		case -1:
		    return (MCPBOOT_EXIT_INVALID_FORMAT);

		case 0:
		    fprintf(stderr, "\n%s: Too long 'start' specification\n",
			   progname);
		    return (MCPBOOT_EXIT_INVALID_FORMAT);

		default:
		    for (i = 0; i < n; i++)
			start = (start << 8) | val[i];
		    start <<= 2;
		    break;
		}
	    }

	    /*
	     * Read length specification
	     */
	    len = 0;
	    n = 17;
	    switch (dat_read_half(val, '\n', &n, 0)) {
	    case -1:
		return (MCPBOOT_EXIT_INVALID_FORMAT);

	    case 0:
		fprintf(stderr, "\n%s: Too long 'length' specification\n",
			progname);
		return (MCPBOOT_EXIT_INVALID_FORMAT);

	    default:
		for (i = 0; i < n; i++)
		    len = (len << 8) | val[i];
		break;
	    }

	    switch (section) {
	    case DAT_BSS:
		if (fseek(stdout, start >> 1, SEEK_SET) != 0) {
		    fprintf(stderr, "\n%s : Cannot fseek(stdout)\n", progname);
		    return (MCPBOOT_EXIT_INVALID_FORMAT);
		}
		for (i = 0; i < len; i++) {
		    putchar('\0');
		    putchar('\0');
		}
		break;

	    case DAT_TEXT:
	    case DAT_DATA:
		if (fseek(stdout, start >> 1, SEEK_SET) != 0) {
		    fprintf(stderr, "\n%s : Cannot fseek(stdout)\n", progname);
		    return (MCPBOOT_EXIT_INVALID_FORMAT);
		}

		/*
		 * Read data
		 */
		for (i = 0; i < len; i++) {
		    n = 3;
		    switch (dat_read_half(val, '\n', &n, 1)) {
		    case -1:
			return (MCPBOOT_EXIT_INVALID_FORMAT);

		    case 0:
			fprintf(stderr, "\n%s: Too long 'data' specification\n",
				progname);
			return (MCPBOOT_EXIT_INVALID_FORMAT);

		    default:
			if (n != 2) {
			    fprintf(stderr,
				    "\n%s: Too short 'data' specification\n",
				    progname);
			    return (MCPBOOT_EXIT_INVALID_FORMAT);
			}
			putchar(val[0]);
			putchar(val[1]);
			break;
		    }
		}
		break;

	    case DAT_SYMBOL:
		if (fseek(stdin, 0, SEEK_END) != 0) {
		    fprintf(stderr, "\n%s : Cannot fseek(stdin)\n", progname);
		    return (MCPBOOT_EXIT_INVALID_FORMAT);
		}
		state = 0;
		continue;
	    }

	    if (i == len && section != DAT_SYMBOL) {
		if (first)
		    first = 0;
		else
		    fprintf(stderr, "+");
		fprintf(stderr, "%d", i * 2);
		state = 3;
	    }
	    continue;

	case 3:
	    switch (c) {
	    case '@':
		state = 1;
		break;

	    case '\n':
		/*
		 * This state happens with the LanAI2 .dat format
		 */
		state = 0;
		break;

	    default:
		fprintf(stderr, "\n%s: Incorrect trailer specification\n",
			progname);
		return (MCPBOOT_EXIT_INVALID_FORMAT);
	    }
	    continue;
	}
    }

    fprintf(stderr, "\n");
    return (MCPBOOT_EXIT_OK);
}

/*
 * Read half word specification
 */
int
dat_read_half(
    int *val,
    int stop,
    unsigned int *n,
    unsigned int half)
{
    int c;
    unsigned int i;
    unsigned int max;
    unsigned int num;

    max = *n << 1;
    for (i = 0; i < max; i++) {
	c = getchar();
	if (c == EOF) {
	    fprintf(stderr, "\n%s: Unexpected %s on input\n", progname,
		    feof(stdin) ? "EOF" : "ERROR");
	    return (-1);
	}
	if (c == stop) {
	    if ((i & 1) != 0 && half) {
		fprintf(stderr, "\n%s: Odd number of half-bytes\n", progname);
		return (-1);
	    }
	    *n = i >> 1;
	    return (1);
	}

	if (c >= '0' && c <= '9')
	    num = c - '0';
	else if (c >= 'a' && c <= 'f')
	    num = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
	    num = c - 'A' + 10;
	else {
	    fprintf(stderr, "\n%s: Incorrect %d%s half-byte\n", progname,
		    i, i > 1 ? (i > 2 ? (i > 3 ? "st" : "rd") : "nd") : "st");
	    return (-1);
	}

	if ((i & 1) == 0)
	    val[i >> 1] = num << 4;
	else 
	    val[i >> 1] |= num;
    }

    return (0);
}
