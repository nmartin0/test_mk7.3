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
 *  File     :  etap_cget.c
 *
 *  Author   :  Joe CaraDonna
 *
 *  Function :  This utility reads cumulative data from the
 *              kernel's cumulative buffer and writes it to disk.
 *
 */

#include <stdio.h>
#include <mach.h>

#include "config.h"           /* must be included before /kern header files */
#include <mach/etap.h>

main(int argc, char *argv[])
{
	cumulative_buffer_t	addr = NULL;
	char 			filename[FILE_NAME_SIZE];
	unsigned long		dum;
	unsigned long 		interval_width;
	unsigned long		num_entries;
	int			x,ret;
	FILE			*fd;

	/* ================================== */
	/* Check arguments, look for filename */
	/* ================================== */

	if (argc == 2)  {
		fprintf (stderr,"usage: %s [-o file]\n",argv[0]);
		exit (0);
	}

	if (argc == 3)  {
		if (!strcmp(argv[1],"-o"))
			strcpy(filename,argv[2]);
		else {
			fprintf(stderr,"usage: %s [-o file]\n",argv[0]);
			exit(0);
		}
	}
	else
		strcpy(filename, CBUFF_DATA_FILE);

	/* =================================== */
	/* Get buffer info and execute mapping */
	/* =================================== */

	ret = etap_info (&dum, &dum, &interval_width, &dum, &dum, &dum);

	/* ======================================================== */
	/* Make sure cumulative tracing is configured in the kernel */
	/* ======================================================== */

	if (ret || !interval_width) {
		fprintf (stderr,
		"ETAP cumulative tracing is not configured properly\n");
		exit(1);
	}

	ret = etap_map(ETAP_BUFFER_CUMULATIVE, NULL, &addr);

	if (ret || addr == NULL) {
		fprintf(stderr,"cannot map cumulative buffer into VAS\n");
		exit(1);
	}

	/* ========================================================= */
	/* if records are allocated within the buffer, write to disk */
	/* ========================================================= */
   
	num_entries = addr->next;

	if (num_entries != 0) {

		/* ========= */
		/* open file */
		/* ========= */

		if ((fd = fopen(filename,"w")) == NULL)  {
			fprintf(stderr,"error opening file: %s\n",filename);
			exit(1);
		}
    
		/* ========================= */
		/* Write header info to file */
		/* ========================= */

		fwrite(&num_entries, sizeof (unsigned long), 1, fd);
		fwrite(&interval_width, sizeof (unsigned long), 1, fd);

		/* =============================== */
		/* copy records in cum_buf to disk */
		/* =============================== */

		ret = fwrite(&addr->entry[0],
			     sizeof(struct cbuff_entry),
			     num_entries,
			     fd);

		if (ret != num_entries) {
			fprintf(stderr,"write error to file: %s\n",filename);
			fclose(fd);
			exit(1);
		}

		fclose(fd);
	}

	printf("%lu cumulative buffer entries written to: %s (%lu bytes)\n",
	       ret, filename, (ret * sizeof(struct cbuff_entry)) +
	       (sizeof (unsigned long) * 2));
}
