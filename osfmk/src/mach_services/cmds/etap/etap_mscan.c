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
 *  File     :  etap_mscan.c
 *
 *  Author   :  Joe CaraDonna
 *
 */

#include <stdio.h>
#include <mach.h>
#include <math.h>

#include "config.h"
#include <mach/etap.h>
#include <mach/etap_events.h>
#include "convert.h"

FILE *fd[PROCESSORS];
int  file_buffer_size = MSTAT_BUFSIZE;

boolean_t	l = FALSE;	/* print everything on one line */

void	print_flags(unsigned short);
void	usage(void);

int
main(int argc, char *argv[])
{
	char	    filename [FILE_NAME_SIZE];        /* input filename       */
	char        newname  [CONV_FILE_NAME_SIZE];   /* converted filename   */
	char	    *cp;
	int         file_num;                /* will contain file header info */
	double	    time;
	int         ret;   
	int         x;  
	boolean_t   v = FALSE;	
	struct mbuff_entry record;           /* read record    */
	
	strcpy(filename, MBUFF_DATA_FILE);

	/* =================== */
	/* parse argument list */
	/* =================== */

	for (x=1; x < argc; x++) {

		cp = argv[x];
		if (*cp != '-')
			usage();

		switch (*(++cp)) {
			case 'i' : strcpy(filename, argv[++x]); break;
			case 'v' : v = TRUE; break;
			case 'l' : l = TRUE; break;	
			case '?' : usage();
			default  : usage();
		}
	}

	/* ===================================== */ 
	/* generate validity report is requested */
	/* ===================================== */

	if (v)
		valid_report(filename);

	/* ================================== */
	/* open data files and assign buffers */
	/* ================================== */

	/* open first file and read header */
	file_name_conv(filename,newname,0);

	if ((fd[0] = fopen(newname,"r")) == NULL)  {
		fprintf(stderr,"error opening file: %s\n",newname);
		exit(1);
	}

	/* set file buffer */
	if (setvbuf(fd[0], NULL, _IOFBF, file_buffer_size)) {
		fprintf(stderr,"file buffer assignment error\n");
		exit(1);
	}

	fread(&file_num, sizeof(int), 1, fd[0]);

	/* open other processor files if any exist */
	for (x=1; x < file_num; x++) {

		/* append cpu ID to file name */
		file_name_conv(filename, newname, x);

		/* open file */
		if ((fd[x] = fopen(newname,"r")) == NULL)  {
			fprintf(stderr,"error opening file: %s\n",newname);
			exit(1);
		}

		/* assign file buffer */
		if (setvbuf(fd[x], NULL, _IOFBF, file_buffer_size)) {
			fprintf(stderr,"file buffer assignment error\n");
			exit(1);
		}

		/* make sure all headers have the same value */
		fread(&ret, sizeof(int), 1, fd[x]);
		if (ret != file_num) {
			fprintf(stderr,"data file mismatch: %s\n",newname);
			exit(1);
		}
	}

  	/* ================================== */
	/* scan data from each processor file */
	/* ================================== */

	while (file_num--) {

		printf("\tscanning processor file %d...\n",file_num);

		while (1) {

			fread(&record,
			      sizeof(struct mbuff_entry),
			      1,
			      fd[file_num]);

			if (feof(fd[file_num]))
				break;

			if (l)
				printf ("Event:%x Instance:%x PC:%x ",
					record.event,
					record.instance,
					record.pc);
			else
				printf("Event=%4x Instance=%x PC=%x\n",
				       record.event,
				       record.instance,
				       record.pc);

			if (l)
				printf("Data:%x %x %x %x ",
				       record.data[0],
				       record.data[1],
				       record.data[2],
				       record.data[3]);
			else
				printf("\tData : %8x %8x %8x %8x\n",
				       record.data[0],
				       record.data[1],
				       record.data[2],
				       record.data[3]);

#define tvalspec_to_usec(tv) \
	((double) (tv).tv_sec * USEC_PER_SEC + \
	 (double) (tv).tv_nsec / NSEC_PER_USEC)

			time = tvalspec_to_usec(record.time);

			if (l)
				printf("Time:%1.0f ", time);
			else
				printf("\tTime : %1.0f\n", time);

			print_flags(record.flags);
		}
		fclose(fd[file_num]);
	}

	exit(0);
}

void
print_flags(unsigned short flags)
{
	if (l)
		printf("Flags:");
	else
		printf("\tFlags: ");

	if (flags & USER_EVENT)
		printf("USER_EVENT ");
	if (flags & KERNEL_EVENT)
		printf("KERNEL_EVENT ");
	if (flags & EVENT_END)
		printf("EVENT_END ");
	if (flags & EVENT_BEGIN)
		printf("EVENT_BEGIN ");

	printf("\n");
}

valid_report (char filename[])
{
	FILE *lost;
	int  x = 0;
	float percent; 
  
	struct lost_stat {
		int written;
		int lost;
	};

	struct lost_stat ls;

	strcat(filename,".lost");
	if ((lost = fopen(filename,"r")) == NULL)  {
		fprintf(stderr,"error opening file: %s\n",filename);
		exit (1);
	}

	printf("-------------------------------------------------------------------------\n");
	printf("   cpu       written        lost          valid\n");
	printf("-------------------------------------------------------------------------\n");
 
	while (!feof(lost)) {
		fread(&ls, sizeof(struct lost_stat), 1, lost);
		if (!feof(lost)) {
			if (ls.written == 0)
				printf("   %2d   |     -na-    |     -na-    |      -na-\n",x++); 
			else {
				percent = ((float)ls.written /
					   (ls.written+ls.lost))*100;
				printf("   %2d   | %10d  | %10d  |  %7.1f%%\n",
				       x++,ls.written, ls.lost,percent);
			}
		}
	}
    
	printf("-------------------------------------------------------------------------\n");
  
	fclose(lost);

	exit(0);
}

void
usage(void)
{
	printf("usage: etap_mscan [-i input file name] [-v]\n\n");
	printf("-i user specified input file (default: %s)\n",MBUFF_DATA_FILE);
	printf("-v validity report\n");
	printf("-l display event information on a single line\n");
	exit(0);
}
