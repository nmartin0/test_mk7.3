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

#include <sys/types.h>
#include <sys/file.h>
#include "makeboot.h"

#include <machine/exec.h>

int som_recog(int, objfmt_t, void *);
int som_load(int, int, objfmt_t, void *);
off_t som_header_size(void);
void write_som_header(int, int, struct loader_info *, off_t);

struct objfmt_switch som_switch = {
    "som",
    som_recog,
    som_load,
    0
};

extern int hpuxboot;

int
som_recog(int in_file, objfmt_t ofmt, void *hdr)
{
    struct header *filehdr = *(struct header **)hdr;

    return (filehdr->system_id == CPU_PA_RISC1_0 ||
	    filehdr->system_id == CPU_PA_RISC1_1);
}

int
som_load(int in_file, int is_kernel, objfmt_t ofmt, void *hdr)
{
    struct loader_info *lp = &ofmt->info;
    struct header *filehdr = (struct header *)hdr;
    struct som_exec_auxhdr x;
    
    lseek(in_file, (off_t)(filehdr->aux_header_location), SEEK_SET);
    read(in_file, (char *)&x, sizeof(x));

    lp->text_start  = x.exec_tmem;

    lp->text_offset = x.exec_tfile;
    lp->data_start  = x.exec_dmem;
    lp->data_offset = x.exec_dfile;
    lp->bss_size    = x.exec_bsize;
    lp->text_size   = x.exec_tsize;
    lp->data_size   = x.exec_dsize;

    if(!is_kernel)
    {
	lp->text_size = round_page(lp->text_size);
	lp->data_size = round_page(lp->data_size);
    }
    
    lp->entry_1 = x.exec_flags;
    lp->entry_2 = filehdr->presumed_dp;

    lp->sym_offset[0] = 0;
    lp->sym_size[0] = 0;

    lp->str_offset = 0;
    lp->str_size = 0;

    return 0;
}

off_t
som_header_size(void)
{
    return sizeof(struct som_exec_auxhdr) + sizeof(struct header);
}

#define SPECID 0x20B
#define VERSION_ID      85082112

void
write_som_header(int in_file,			/* input file */
		 int out_file,			/* output file */
		 struct loader_info *kp,	/* kernel load info */
		 off_t file_size)		/* size of output file */
{
	struct som_exec_auxhdr x;
	struct header h;

	lseek(in_file, 0, SEEK_SET);
	read(in_file, (char *)&h, sizeof(struct header));
	h.som_length = file_size;
	h.aux_header_location = sizeof(struct header);

	if(hpuxboot)
	{
		h.a_magic = 0x107;
		h.system_id = 0x210;
	}
	else
	{
		h.a_magic = SPECID;
		h.version_id = VERSION_ID;
	}

	lseek(out_file, 0, SEEK_SET);
	write(out_file, (char *)&h, sizeof(struct header));

	read(in_file, (char *)&x, sizeof(struct som_exec_auxhdr));
	x.som_auxhdr.type = 4;
	x.exec_entry = kp->entry_1;
	x.exec_tfile = exec_header_size();
	x.exec_tsize = kp->text_size;
	x.exec_tmem = kp->text_start;
	x.exec_dfile = exec_header_size() + kp->text_size;
	x.exec_dsize = file_size - x.exec_dfile;
	x.exec_dmem = kp->data_start; 
	x.exec_bsize = 0;
	write(out_file, (char *)&x, sizeof(struct som_exec_auxhdr));
}
