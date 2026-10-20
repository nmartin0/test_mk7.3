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
/* CMU_HIST */ 
/*
 * Revision 2.3  92/01/03  20:28:35  dbg
 * 	Remove '-sequent' switch.  Recognize Sequent from
 * 	real-to-protected bootstrap text following a.out header.
 * 	[91/08/30            dbg]
 * 
 * 	Added code to glue the kernel and the boot file together, for
 * 	all machines.
 * 	[91/05/29            dbg]
 * 
 * Revision 2.2  91/05/08  13:09:36  dbg
 * 	Created.
 * 	[91/02/26            dbg]
 * 
 */
/* CMU_ENDHIST */
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
/*
 */

/*
 * Build a pure-kernel boot file from a kernel and a bootstrap loader.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>	/* for bzero prototype */
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "makeboot.h"

char *kernel_name;
char *bootstrap_name;
const char *boot_file_name = "mach.boot";

int	kern_file;
int	boot_file;
int	out_file;
int	entry_addr;
int	entry_spec;
int	debug;

#ifdef hp_pa
int	kern_symbols = 0;
int	boot_symbols = 0;
int     hpuxboot = 0;
#else
int	kern_symbols = 1;
int	boot_symbols = 1;
#endif /* hp_pa */
int     bootstrap = 1;

void usage(void);
int check_and_open(char *);
void build_boot(void);

void
usage(void)
{
#ifdef hp_pa
	printf("usage: makeboot %s kernel bootstrap\n",
	       "[-h] [-o boot_file]");
#else
	printf("usage: makeboot %s kernel [ bootstrap ]\n",
	       "[-d] [-T entry_addr] [ -o boot_file ]");
#endif
	exit(1);
}

int
main(int argc, char **argv)
{
	argc--, argv++;	/* skip program name */

	if (argc == 0)
	    usage();

	/*
	 * Parse switches.
	 */
	while (argc > 0 && **argv == '-') {
	    char *s;

	    s = *argv;
	    if (s[1] == 'o') {
		/* output file name */
		argc--, argv++;
		if (argc == 0)
		    usage();
		boot_file_name = *argv;
		argc--, argv++;
	    }
	    else if (s[1] == 'T') {
		/* entry point address */
		argc--, argv++;
		if (argc == 0 || sscanf(*argv, "%x", &entry_addr) != 1)
		    usage();
		entry_spec = 1;
		argc--, argv++;
	    }
	    else if (s[1] == 'd') {
		/* debug turned on */
		argc--, argv++;
		debug = 1;
	    }
#ifdef hp_pa
	    else if (s[1] == 'h') {
		/* hpux boot */
		argc--, argv++;
		hpuxboot = 1;
	    }
#endif
	    else {
		printf("unknown switch: %s\n", s);
		exit(1);
	    }
	}

	if(argc == 1)
	    bootstrap = 0;
	else if (argc != 2)
	    usage();

	kernel_name = argv[0];
	kern_file = check_and_open(kernel_name);

	if(bootstrap)
	{
	    bootstrap_name = argv[1];
	    boot_file = check_and_open(bootstrap_name);
	}

	out_file = open(boot_file_name, O_RDWR|O_CREAT|O_TRUNC, 0777);
	if (out_file < 0) { /* XXX mode */
	    perror(boot_file_name);
	    exit(2);
	}

	build_boot();

	close(out_file);
	close(kern_file);
	if(bootstrap)
	    close(boot_file);

	return (0);
}

int
check_and_open(char *fname)
{
	int f;
	struct stat statb;

	if (stat(fname, &statb) < 0) {
	    perror(fname);
	    exit(2);
	}
	if ((statb.st_mode & S_IFMT) != S_IFREG ||
	    (statb.st_mode & (S_IEXEC|(S_IEXEC>>3)|(S_IEXEC>>6))) == 0) {
		printf("build_boot: %s: not an executable file\n",
			fname);
		exit(2);
	}

	f = open(fname, O_RDONLY, 0);
	if (f < 0) {
	    perror(fname);
	    exit(2);
	}
	return (f);
}

static int
ex_get_header(int in_file, int is_kernel, objfmt_t ofmt)
{
	int i;
	objfmt_switch_t fmt;
	char buf[BUFSIZ], *bufptr = buf;
	int result;

	lseek(in_file, (off_t) 0, SEEK_SET);
	check_read(in_file, buf, sizeof(buf));

	for (i = 0; formats[i] != 0; i++) {
	    fmt = formats[i];
	    ofmt->fmt = fmt;
	    if (!(*fmt->recog)(in_file, ofmt, &bufptr))
		continue;
	    result = (*fmt->load)(in_file, is_kernel, ofmt, bufptr);
	    if (bufptr != buf)
		free(bufptr);
	    return result;
	}
	return 1;
}

/*
 * Create the boot file.
 */
void
build_boot(void)
{
	/*
	 * The symbol table is read from the file.
	 * Machine-dependent code can specify an
	 * optional header to be prefixed to the
	 * symbol table, containing information
	 * that cannot directly be read from the file.
	 */
	int             err;

	struct objfmt		kern_header;
	struct objfmt		boot_header;
	struct loader_info	boot_header_out;
	struct boot_info	boot_info;

	off_t	off;
	off_t	boot_info_off = 0;
	off_t	boot_image_off;

	memset(&kern_header, 0, sizeof(kern_header));
	memset(&boot_header, 0, sizeof(boot_header));
	memset(&boot_header_out, 0, sizeof(boot_header_out));
	memset(&boot_info, 0, sizeof(boot_info));

	/*
	 * Read in the kernel header.
	 */
	if(err = ex_get_header(kern_file, 1, &kern_header))
	{
	    if(err == 1)
		printf("%s: not an executable file\n", kernel_name);
	    if(err == 2)
		usage();
	    exit(4);
	}
	if (entry_spec)
	    kern_header.info.entry_1 = entry_addr;
	
	printf(" loading %s (%s)\n", kernel_name, kern_header.fmt->name);

	/*
	 * Copy its text and data to the text section of the output file.
	 */
	lseek(out_file, (*exec_header_size)(), SEEK_SET);

	lseek(kern_file, (off_t)kern_header.info.text_offset, SEEK_SET);
	file_copy(out_file, kern_file, kern_header.info.text_size);

	lseek(kern_file, (off_t)kern_header.info.data_offset, SEEK_SET);
	file_copy(out_file, kern_file, kern_header.info.data_size);

	if(bootstrap)
	{
	    /*
	     * Allocate the boot_info block.
	     */
	    boot_info_off = lseek(out_file, (off_t) 0, SEEK_CUR);
	    (void) lseek(out_file, (off_t) sizeof(struct boot_info), SEEK_CUR);
	}

	/*
	 * Find the kernel symbol table.
	 */
	if (kern_symbols && kern_header.info.sym_size[0] != 0)
	{
	    vm_size_t sym_size = kern_header.info.sym_size[0];

	    /*
	     * Copy the header to the output file.
	     */
	    write(out_file, &sym_size, sizeof(sym_size));

	    /*
	     * Copy the symbol table to the output file.
	     */
	    (*kern_header.fmt->symload)(kern_file, out_file, &kern_header);

	    /*
	     * Round to an integer boundary in the file.
	     */
	    sym_size = sizeof(sym_size) + kern_header.info.sym_size[0];
	    if (sym_size % sizeof(int) != 0) {
		size_t	pad;
		char	*zeros;

		pad = sizeof(int) - (sym_size % sizeof(int));
		zeros = (char *)malloc(pad);
		bzero(zeros, pad);
		write(out_file, zeros, pad);
		sym_size += pad;
	    }
		
	    /*
	     * Remember the symbol table size.
	     */
	    boot_info.sym_size = sym_size;
	}
	else {
	    boot_info.sym_size = 0;
	}

	if(bootstrap)
	{
	    vm_size_t head_gap, tail_pad;
	    /*
	     * Remember the start of the bootstrap image.
	     */
	    boot_image_off = lseek(out_file, (off_t) 0, SEEK_CUR);

	    /*
	     * Read the header for the bootstrap file.
	     */
	    if(ex_get_header(boot_file, 0, &boot_header))
	    {
		printf("%s: not an executable file\n", bootstrap_name);
		exit(4);
	    }

	    printf(" loading %s (%s)\n", bootstrap_name,
		   boot_header.fmt->name);

	    /*
	     * Copy the text
	     */
	    lseek(boot_file, (off_t)boot_header.info.text_offset, SEEK_SET);
	    head_gap = boot_header.info.text_start - trunc_page(boot_header.info.text_start);
	    file_zero(out_file, head_gap);
	    file_copy(out_file, boot_file, boot_header.info.text_size);
	    boot_header.info.text_start -= head_gap;
	    boot_header.info.text_size += head_gap;
	    tail_pad = round_page(boot_header.info.text_size) - boot_header.info.text_size;
	    file_zero(out_file, tail_pad);
	    boot_header.info.text_size += tail_pad;
	   

	    /*
	     * And the data
	     */
	    lseek(boot_file, (off_t)boot_header.info.data_offset, SEEK_SET);
	    head_gap = boot_header.info.data_start - trunc_page(boot_header.info.data_start);
	    file_zero(out_file, head_gap);
	    file_copy(out_file, boot_file, boot_header.info.data_size);
	    boot_header.info.data_start -= head_gap;
	    boot_header.info.data_size += head_gap;
	    tail_pad = round_page(boot_header.info.data_size) - boot_header.info.data_size;
	    file_zero(out_file, tail_pad);
	    boot_header.info.data_size += tail_pad;
	    if (tail_pad < boot_header.info.bss_size)
		boot_header.info.bss_size -= tail_pad;
	    else
		boot_header.info.bss_size = 0;

	    /*
	     * pad the data to a page boundary, so that the symbol table
	     * will start at page offset in the file.
	     */
	    if (round_page(boot_header.info.data_size) != 
					boot_header.info.data_size) {
		off_t roundup;
		roundup = round_page(boot_header.info.data_size) -
				boot_header.info.data_size;
		lseek(out_file, roundup, SEEK_CUR);
		if (roundup > boot_header.info.bss_size)
			boot_header.info.bss_size = 0;
		else
			boot_header.info.bss_size -= roundup;
		boot_header.info.data_size += roundup;
	    }

	    /*
	     * Symbols for boot program
	     */
	    if (boot_symbols && boot_header.info.sym_size[0] != 0) {
		vm_size_t sym_size = boot_header.info.sym_size[0];

		/*
		 * Copy the header to the output file.
		 */
		write(out_file, &sym_size, sizeof(sym_size));

		/*
		 * Copy the symbol table to the output file.
		 */
		(*boot_header.fmt->symload)(boot_file, out_file, &boot_header);

		/*
		 * Round to an integer boundary in the file.
		 */
		sym_size = sizeof(sym_size) + boot_header.info.sym_size[0];
		if (sym_size % sizeof(int) != 0) {
		    size_t pad;
		    int	zeros = 0;

		    pad = sizeof(int) - (sym_size % sizeof(int));
		    write(out_file, (char *)&zeros, pad);
		    sym_size += pad;
		}

		/*
		 * Remember the symbol table size.
		 */
		boot_header.info.sym_size[0] = sym_size;
		/*
		 * Account for padding the data in the output file
		 * by moving the symbol and string tables.
		 */
		boot_header.info.str_offset += 
			round_page(boot_header.info.sym_offset[0])
			- boot_header.info.sym_offset[0];

		boot_header.info.sym_offset[0] =
			round_page(boot_header.info.sym_offset[0]);
	    }
	    else {
		boot_header.info.sym_size[0] = 0;
	    }

	    /*
	     * Save the size of the boot image.
	     */
	    off = lseek(out_file, (off_t) 0, SEEK_CUR);
	    boot_info.boot_size = off - boot_image_off;

	    /*
	     * Write out a modified copy of the boot header.
	     * Offsets are relative to the end of the boot header.
	     */
	    boot_header_out = boot_header.info;
	    boot_header_out.format = 0;
	    boot_header_out.text_offset = 0;
	    boot_header_out.data_offset = boot_header_out.text_offset + boot_header.info.text_size;
#ifdef hp_pa
	    boot_header_out.sym_offset[0] = 0;
#else
	    boot_header_out.sym_offset[0] = boot_header_out.data_offset + boot_header.info.data_size;
#endif

	    if (debug) {
		printf("format %x\n", boot_header_out.format);
		printf("text start %x size %x offset %x \n",
		       boot_header_out.text_start,
		       boot_header_out.text_size,
		       boot_header_out.text_offset);
		printf("data start %x size %x offset %x \n",
		       boot_header_out.data_start,
		       boot_header_out.data_size,
		       boot_header_out.data_offset);
		printf("bss size %x\n", boot_header_out.bss_size);
		printf("str offset %x size %x\n",
		       boot_header_out.str_offset,
		       boot_header_out.str_size);
		printf("sym offset %x %x %x %x\n",
		       boot_header_out.sym_offset[0],
		       boot_header_out.sym_offset[1],
		       boot_header_out.sym_offset[2],
		       boot_header_out.sym_offset[3]);
		printf("sym size %x %x %x %x\n",
		       boot_header_out.sym_size[0],boot_header_out.sym_size[1],
		       boot_header_out.sym_size[2],boot_header_out.sym_size[3]);
		printf("entry_1 %x entry_2 %x\n",
		       boot_header_out.entry_1, boot_header_out.entry_1);
	    }

	    write(out_file, (char *)&boot_header_out, sizeof(boot_header_out));

	    boot_info.load_info_size = sizeof(boot_header_out);
	}

	/*
	 * Remember the end of the file.
	 */
	off = lseek(out_file, (off_t) 0, SEEK_CUR);

#ifndef hp_pa
	/*
	 * Indicate that we have no strings.
	 */
	{
	    vm_size_t str_size = sizeof(vm_size_t);

	    write(out_file, &str_size, sizeof(str_size));
	}
#endif

	/*
	 * Go back to the start of the file and write out
	 * the bootable file header.
	 */
	lseek(out_file, (off_t) 0, SEEK_SET);
	(*write_exec_header)(kern_file, out_file, &kern_header.info, off);

	if(bootstrap)
	{
	    if (debug)
		printf("sym_size %x, boot_size %x, load_info_size %x\n",
		       boot_info.sym_size,
		       boot_info.boot_size,
		       boot_info.load_info_size);
	    /*
	     * Write out the boot_info block.
	     */
	    lseek(out_file, boot_info_off, SEEK_SET);
	    write(out_file, (char *)&boot_info, sizeof(boot_info));
	}
}

void
check_read(int f, void *addr, size_t size)
{
	if (read(f, addr, size) != size) {
	    perror("read");
	    exit(6);
	}
}

#if DEBUG
#undef file_copy
#undef file_zero
#undef write
#endif /* DEBUG */

/*
 * Copy N bytes from in_file to out_file
 */
void
file_copy(int out_f, int in_f, size_t size)
{
	char	buf[4096];

	while (size >= sizeof(buf)) {
	    check_read(in_f, buf, sizeof(buf));
	    write(out_f, buf, sizeof(buf));
	    size -= sizeof(buf);
	}
	if (size > 0) {
	    check_read(in_f, buf, size);
	    write(out_f, buf, size);
	}
}

/*
 * Copy N bytes from in_f to out_f
 */
void
file_zero(int out_f, size_t size)
{
	char	buf[4096];

	bzero(buf, sizeof(buf));
	while (size >= sizeof(buf)) {
	    write(out_f, buf, sizeof(buf));
	    size -= sizeof(buf);
	}
	if (size > 0) {
	    write(out_f, buf, size);
	}
}

#if DEBUG

#undef lseek

off_t
_lseek(int file, off_t off, int whence) {
  	off_t ret = lseek(file, off, whence);
	printf("lseek(%d, %d, %d) = %d\n", file, (int)off, whence, (int)ret);
	return(ret);
}

#undef file_copy

void
_file_copy(int to, int from, size_t size) {
	printf("file_copy(%d (%d 0x%x), %d (%d 0x%x), %d (0x%x))\n",
	       to, (int)lseek(to, 0, 1), (int)lseek(to, 0, 1),
	       from, (int)lseek(from, 0, 1), (int)lseek(from, 0, 1),
	       (int)size, (int)size);
	file_copy(to, from, size);
}

#undef file_zero

void
_file_zero(int to, size_t size) {
	printf("file_zero(%d (%d 0x%x), %d (0x%x))\n",
	       to, (int)lseek(to, 0, 1), (int)lseek(to, 0, 1),
	       (int)size, (int)size);
	file_zero(to, size);
}

#undef write

ssize_t
_write(int to, const void *addr, size_t size)
{
	printf("write(%d (%d), %x, %d)\n",
	       to, (int)lseek(to, 0, 1),
	       (int)addr,
	       (int)size);
	return(write(to, addr, size));
}
#endif /* DEBUG */
