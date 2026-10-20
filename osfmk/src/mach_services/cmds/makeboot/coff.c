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
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
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
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */
/*
 * CMU_HISTORY
 * Revision 2.3  92/04/01  19:36:09  rpd
 * 	Fixed to handle kernels with non-contiguous text and data.
 * 	[92/03/13            rpd]
 * 
 * Revision 2.2  92/01/03  20:28:52  dbg
 * 	Created.
 * 	[91/09/12            dbg]
 * 
 */

/*
 * COFF-dependent (really, i860 COFF-dependent) routines for makeboot
 */
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <machine/coff.h>
#include <machine/exec.h>
#include <syms.h>
#include <mach/machine/vm_param.h>
#include <mach/machine/vm_types.h>

#include <mach/boot_info.h>

#include "makeboot.h"

int coff_recog(int, objfmt_t, void *);
int coff_load(int, int, objfmt_t, void *);
void coff_symload(int, int, objfmt_t);
off_t coff_header_size(void);
void write_coff_header(int in_file, int out_file,	
		   struct loader_info *kp,	
		   off_t file_size);


struct objfmt_switch coff_switch = {
    "coff",
    coff_recog,
    coff_load,
    coff_symload
};

int
coff_recog(int in_file, objfmt_t ofmt, void *hdr)
{
    struct exec_hdrs	*x = *(struct exec_hdrs **)hdr;

    if (x->fh.f_magic == I860MAGIC)
	    return (1);
    else
	    return (0);
}


off_t
coff_header_size(void)
{
	return (EXECHSIZE);
}

/*
 * Convert a coff-style file header to a loader_info-style one.
 * On return, *sym_size contains the symbol table size (alone),
 * while the first sym_size member of lp contains the total
 * space needed for symbol table-related data.
 */
int
coff_load(
	int			in_file,
	int			is_kernel,
	objfmt_t 		ofmt,
	void			*hdr)
{
	struct loader_info	*lp = &ofmt->info;
	struct exec_hdrs	*x = (struct exec_hdrs *)hdr;
	struct stat		sbuf;

	if (x->fh.f_magic != I860MAGIC)
	    return (1);
	switch ((int)x->ah.magic) {
	    case ZMAGIC:
		if (x->ah.tsize == 0) {
		    return (1);
		}
		lp->text_start	= x->ah.text_start;
		lp->text_size	= x->ah.tsize;
		lp->text_offset	= x->sh0.s_scnptr;  /* assume .text first */
		lp->data_start	= x->ah.data_start;
		lp->data_size	= x->ah.dsize;
		lp->data_offset	= x->sh1.s_scnptr;  /* assume .data second */
		lp->bss_size	= x->ah.bsize;
		break;
	    case OMAGIC:
	    case NMAGIC:
	    default:
		return (1);
	}
	lp->entry_1 = x->ah.entry;
	lp->entry_2 = 0;
	lp->sym_size[0] = x->fh.f_nsyms;
	lp->sym_offset[0] = x->fh.f_symptr;
	if (lp->sym_offset[0] == 0) {
		lp->sym_size[0] = 0;
	}
	else {
		if (SYMESZ != AUXESZ) {
			fprintf(stderr,
				"can't determine symtab size\n");
			exit(1);
		}
		lp->sym_size[0] *= SYMESZ;
		/*
		 * Compute length of string table.
		 */
		if (fstat(in_file, &sbuf)) {
			perror("can't stat");
			exit(1);
		}
		lp->str_offset = lp->sym_offset[0] + lp->sym_size[0];
		lp->str_size = sbuf.st_size - lp->str_offset;
	}
	lp->format = COFF_F;
	return (0);
}

void
coff_symload(
	int			in_file,
	int			out_file,
	objfmt_t 		ofmt)
{
	ofmt->info.sym_size[0] += ofmt->info.str_size;
	lseek(in_file, (off_t)ofmt->info.sym_offset[0], SEEK_SET);
	file_copy(out_file, in_file, ofmt->info.sym_size[0]);
}

#define round(v, b)	(((v) + (b) - 1) & ~((b) - 1))

void
write_coff_header(
	int                     in_file,        /* input file */
	int			out_file,	/* output file */
	struct loader_info	*kp,		/* kernel load info */
	off_t			file_size)	/* size of output file */
{
	char			scrbuf[512];
	int			left, piece, textra;
	struct exec_hdrs	out_header;
	struct timeval		cur_time;

	gettimeofday(&cur_time, (struct timezone *)0);
	memset((void *)&out_header, 0, sizeof (out_header));
	memset((void *)&scrbuf, 0, sizeof (scrbuf));

	out_header.fh.f_magic    = I860MAGIC;
	out_header.fh.f_nscns    = 3;
	out_header.fh.f_timdat   = cur_time.tv_sec;
	out_header.fh.f_symptr   = 0;
	out_header.fh.f_nsyms    = 0;
	out_header.fh.f_opthdr   = AOUTHSIZE;
	out_header.fh.f_flags	 = 3;	/* XXX -- no reloc, executable */

	out_header.ah.magic	 = ZMAGIC;
	out_header.ah.vstamp	 = 2;

	out_header.ah.tsize	 = kp->text_size; 
	out_header.ah.dsize	 = (int) file_size - sizeof(out_header)
				   - out_header.ah.tsize;
	out_header.ah.bsize	 = kp->bss_size;
	out_header.ah.entry	 = kp->entry_1;
	out_header.ah.text_start = kp->text_start;
	out_header.ah.data_start = kp->data_start;

	strcpy(out_header.sh0.s_name, ".text");
	out_header.sh0.s_paddr	 = kp->text_start;
	out_header.sh0.s_vaddr	 = kp->text_start;
	out_header.sh0.s_size	 = out_header.ah.tsize;
	out_header.sh0.s_scnptr  = kp->text_offset;
	out_header.sh0.s_relptr  = 0;
	out_header.sh0.s_lnnoptr = 0;
	out_header.sh0.s_nreloc	 = 0;
	out_header.sh0.s_nlnno	 = 0;
	out_header.sh0.s_flags	 = STYP_TEXT;

	strcpy(out_header.sh1.s_name, ".data");
	out_header.sh1.s_paddr	 = kp->data_start;
	out_header.sh1.s_vaddr	 = kp->data_start;
	out_header.sh1.s_size	 = out_header.ah.dsize;
	out_header.sh1.s_scnptr  = kp->data_offset;
	out_header.sh1.s_relptr  = 0;
	out_header.sh1.s_lnnoptr = 0;
	out_header.sh1.s_nreloc	 = 0;
	out_header.sh1.s_nlnno	 = 0;
	out_header.sh1.s_flags	 = STYP_DATA;

	strcpy(out_header.sh2.s_name, ".bss");
	out_header.sh2.s_paddr	 = out_header.sh1.s_paddr
	                           + out_header.sh1.s_size;
	out_header.sh2.s_vaddr	 = out_header.sh1.s_vaddr
                                   + out_header.sh1.s_size;
	out_header.sh2.s_size	 = out_header.ah.bsize;
	out_header.sh2.s_scnptr  = 0;
	out_header.sh2.s_relptr  = 0;
	out_header.sh2.s_lnnoptr = 0;
	out_header.sh2.s_nreloc	 = 0;
	out_header.sh2.s_nlnno	 = 0;
	out_header.sh2.s_flags	 = STYP_BSS;

	lseek(out_file, (off_t) 0, SEEK_SET);
	write(out_file, (char *)&out_header, sizeof(out_header));
	return;
}
