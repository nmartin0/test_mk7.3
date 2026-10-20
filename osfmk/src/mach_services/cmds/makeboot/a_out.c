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
 * Executable file format for i386.
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/exec.h>
#include <sys/vmparam.h>
#include <string.h>

#include <mach/machine/vm_param.h>
#include <mach/boot_info.h>

#include "makeboot.h"

int a_out_recog(int, objfmt_t, void *);
int a_out_load(int, int, objfmt_t, void *);
void a_out_symload(int, int, objfmt_t);
off_t a_out_header_size(void);
void write_a_out_header(int kern_file, int out_file,	
		   struct loader_info *kp,	
		   off_t file_size);

struct objfmt_switch a_out_switch = {
    "a_out",
    a_out_recog,
    a_out_load,
    a_out_symload
};

off_t
a_out_header_size(void)
{
	return sizeof(struct exec);
}

void
write_a_out_header(int kern_file, 
		   int out_file,		/* output file */
		   struct loader_info *kp,	/* kernel load info */
		   off_t file_size)		/* size of output file */
{
	struct exec	out_header;

	memset(&out_header, 0, sizeof(out_header));
	out_header.a_magic = OMAGIC;
	out_header.a_text  = (int) file_size - sizeof(struct exec);
	out_header.a_data = 0;
	out_header.a_syms = 0;
	out_header.a_bss  = 0;
	out_header.a_trsize = 0;
	out_header.a_drsize = 0;
	out_header.a_entry = kp->entry_1;

	write(out_file, (char *)&out_header, sizeof(out_header));
}

int
a_out_recog(int in_file, objfmt_t ofmt, void *hdr)
{
	struct exec *x = *(struct exec **)hdr;

	switch ((int)x->a_magic) {
	case OMAGIC:
	case NMAGIC:
	case ZMAGIC:
	    return 1;
	    break;
	default:
	    return 0;
	}
}

int
a_out_load(int in_file, int is_kernel, objfmt_t ofmt, void *hdr)
{
	struct loader_info *lp = &ofmt->info;
	struct exec *x = (struct exec *)hdr;
	vm_size_t str_size;

	switch ((int)x->a_magic) {
	    case OMAGIC:
		lp->text_start  = 0;
		lp->text_size   = 0;
		lp->text_offset = 0;
		lp->data_start  = USRTEXT;
		lp->data_size	= x->a_text + x->a_data;
		lp->data_offset = sizeof(struct exec);
		lp->bss_size	= x->a_bss;
		break;

	    case NMAGIC:
		if (x->a_text == 0) {
		    return 1;
		}
		lp->text_start	= USRTEXT;
		lp->text_size	= x->a_text;
		lp->text_offset	= sizeof(struct exec);
		lp->data_start	= lp->text_start + lp->text_size;
		lp->data_size	= x->a_data;
		lp->data_offset	= lp->text_offset + lp->text_size;
		lp->bss_size	= x->a_bss;
		break;

	    case ZMAGIC:
		if (x->a_text == 0) {
		    return 1;
		}
		lp->text_start	= USRTEXT;
		lp->text_size	= sizeof(struct exec) + x->a_text;
		lp->text_offset	= 0;
		lp->data_start	= lp->text_start + lp->text_size;
		lp->data_size	= x->a_data;
		lp->data_offset	= lp->text_offset + lp->text_size;
		lp->bss_size	= x->a_bss;
		break;

	    default:
		return 1;
	}

	lp->entry_1 = x->a_entry;
	lp->entry_2 = 0;

	lp->sym_offset[0] = lp->data_offset + x->a_data;

	/*
	 * Read string table size.
	 */
	lseek(in_file, (off_t) (lp->sym_offset[0]+x->a_syms), SEEK_SET);
	read(in_file, (char *)&str_size, sizeof(str_size));

	lp->sym_size[0] = x->a_syms;
	lp->str_size = str_size;

	return 0;
}

void
a_out_symload(int in_file, int out_file, objfmt_t ofmt)
{
	ofmt->info.sym_size[0] += ofmt->info.str_size;
	lseek(in_file, (off_t)ofmt->info.sym_offset[0], SEEK_SET);
	file_copy(out_file, in_file, ofmt->info.sym_size[0]);
}
