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
 *  File     :  etap_mview.c
 *
 *  Author   :  Joe CaraDonna
 *
 */

#include "system_tables.h"
#include "config.h"

#include <stdio.h>
#include <mach.h>
#include <math.h>
#include <sys/syscall.h>

#include <mach/etap.h>
#include <mach/etap_events.h>

#include "convert.h"

FILE *fd[PROCESSORS];
int  file_buffer_size = MSTAT_BUFSIZE;

boolean_t	l = FALSE;	/* print everything on one line */

void	print_flags(unsigned short);
void	usage(void);
void	print_kernel_event(mbuff_entry_t);
void	print_user_event(mbuff_entry_t);

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
	boolean_t   p = FALSE;
	struct mbuff_entry record;           /* read record    */
	int	    target_file;

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
			case 'p' : p = TRUE; break;
			case '?' : usage();
			default  : usage();
		}
	}

	/* ===================================== */
	/* generate validity report is requested */
	/* ===================================== */

	if (v) {
		valid_report(filename);
		exit(0);
	}

	if (p) {
		system_table_print(SYS_TABLE_MACH_TRAP);     printf("\n");
		system_table_print(SYS_TABLE_MACH_MESSAGE);  printf("\n");
		system_table_print(SYS_TABLE_UNIX_SYSCALL);  printf("\n");
		system_table_print(SYS_TABLE_INTERRUPT);     printf("\n");
		system_table_print(SYS_TABLE_EXCEPTION);
		exit(0);
	}

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

	target_file = file_num;

	while (target_file--) {

		if (file_num != 1)
			printf("\tscanning processor file %d...\n",target_file);

		printf("Thread ID    Time     Event\n");
		printf("======================================================\n");

		while (1) {

			fread(&record,
			      sizeof(struct mbuff_entry),
			      1,
			      fd[target_file]);

			if (feof(fd[target_file]))
				break;

#define tvalspec_to_usec(tv) \
	((double) (tv).tv_sec * USEC_PER_SEC + \
	 (double) (tv).tv_nsec / NSEC_PER_USEC)

			time = tvalspec_to_usec(record.time);

			printf ("%x: %8.0f: ", record.instance, time);

			if (record.flags & USER_EVENT)
				print_user_event(&record);
			else
				print_kernel_event(&record);
		}
		fclose(fd[target_file]);
	}
	exit(0);
}

void
print_kernel_event(mbuff_entry_t record)
{
	char *text_name;

	switch (record->event) {

	case	ETAP_P_THREAD_LIFE :
		if (record->flags & EVENT_BEGIN)
			printf("thread created    [T:%x A:%x] P:%d\n",
			       record->data[0],
			       record->data[1],
			       record->data[2]);
		else
			printf("thread terminated [T:%x A:%x] P:%d\n",
			       record->data[0],
			       record->data[1],
			       record->data[2]);
		break;

	case	ETAP_P_SYSCALL_MACH :
		if (record->flags & SYSCALL_TRAP)
			text_name = system_table_lookup(SYS_TABLE_MACH_TRAP,
							record->data[0]);
		else
			text_name = system_table_lookup(SYS_TABLE_MACH_MESSAGE,
							record->data[0]);

		if (record->flags & EVENT_BEGIN)
			printf("mach enter: %s [%x]\n",
			       text_name,
			       record->data[0]);
		else
			printf("mach exit :\n");
		break;

	case	ETAP_P_SYSCALL_UNIX :
		text_name = system_table_lookup(SYS_TABLE_UNIX_SYSCALL,
						record->data[0]);

		if (record->flags & EVENT_BEGIN)
			printf("unix enter: %s\n", text_name);
		else
			printf("unix exit : %s\n", text_name);
		break;

	case	ETAP_P_THREAD_CTX :
		if (record->flags & EVENT_END)
			printf("context switch to   %x   ",
			       record->data[0]);
		else	/* EVENT_BEGIN */
			printf("context switch from %x   ",
			       record->data[0]);

		switch (record->data[1]) {
			case	BLOCKED_ON_SEMAPHORE :
				printf("R: semaphore\n"); break;
			case	BLOCKED_ON_LOCK :
				printf("R: lock\n"); break;
			case	BLOCKED_ON_MUTEX_LOCK :
				printf("R: mutex lock\n"); break;
			case	BLOCKED_ON_COMPLEX_LOCK :
				printf("R: complex lock\n"); break;
			case	BLOCKED_ON_PORT_RCV :
				printf("R: port receive\n"); break;
			case	BLOCKED_ON_REAPER_DONE :
				printf("R: reaper thread done\n"); break;
			case	BLOCKED_ON_IDLE_DONE :
				printf("R: idle thread done\n"); break;
			case	BLOCKED_ON_TERMINATION :
				printf("R: termination\n"); break;
			default :
				if (record->data[2])
					printf("R: ast %x\n", record->data[2]);
				else
					printf("R: undefined block\n");
			};
		break;

	case	ETAP_P_INTERRUPT :
		if (record->flags & EVENT_BEGIN) {
			text_name = system_table_lookup(SYS_TABLE_INTERRUPT,
							record->data[0]);
			printf("intr enter: %s\n", text_name);
		} else
			printf("intr exit\n");
		break;

	case	ETAP_P_ACT_ABORT :
		printf("activation abort [A %x : S %x]\n",
		       record->data[1],
		       record->data[0]);
		break;

	case	ETAP_P_PRIORITY :
		printf("priority changed for %x   N:%d O:%d\n",
		       record->data[0],
		       record->data[1],
		       record->data[2]);
		break;

	case	ETAP_P_EXCEPTION :
		text_name = system_table_lookup(SYS_TABLE_EXCEPTION,
						record->data[0]);
		printf("exception: %s\n", text_name);
		break;

	case	ETAP_P_DEPRESSION :
		if (record->flags & EVENT_BEGIN)
			printf("priority depressed\n");
		else {
			if (record->data[0] == 0)
			    printf("priority undepressed : timed out\n");
			else
			    printf("priority undepressed : self inflicted\n");
		}
		break;

	default:
		printf("Unknown event: %d\n", record->event);
	}
}

void
print_user_event(mbuff_entry_t record)
{
	printf("user probe: [%x] data = %x %x %x %x\n",
	       record->event,
	       record->data[0],
	       record->data[1],
	       record->data[2],
	       record->data[3]);
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
}

void
usage(void)
{
	printf("usage: etap_mview [-i input file name] [-v] [-p]\n\n");
	printf("-i user specified input file (default: %s)\n",MBUFF_DATA_FILE);
	printf("-v validity report\n");
	printf("-p print all system tables (e.g., system calls, interrupts)\n");
	printf("\n");
	exit(0);
}
