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
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/elf.h>

#include <mach/machine/vm_types.h>

#include <ddb/nlist.h>

#include "makeboot.h"

int elf_recog(int, objfmt_t, void *);
int elf_load(int, int, objfmt_t, void *);
void elf_symload(int, int, objfmt_t);

struct objfmt_switch elf_switch = {
    "elf",
    elf_recog,
    elf_load,
    elf_symload
};

int
elf_recog(int in_file, objfmt_t ofmt, void *hdr)
{
    Elf32_Ehdr *x = *(Elf32_Ehdr **)hdr;

    return (x->e_ident[EI_MAG0] == ELFMAG0 &&
	    x->e_ident[EI_MAG1] == ELFMAG1 &&
	    x->e_ident[EI_MAG2] == ELFMAG2 &&
	    x->e_ident[EI_MAG3] == ELFMAG3);
}

int
elf_load(int in_file, int is_kernel, objfmt_t ofmt, void *hdr)
{
    struct loader_info *lp = &ofmt->info;
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)hdr;
    Elf32_Phdr *phdr, *ph;
    Elf32_Shdr *shdr, *sh;
    size_t phsize;
    size_t shsize;
    int i;

    lp->entry_1 = (vm_offset_t) ehdr->e_entry;
    lp->entry_2 = 0;

    phsize = ehdr->e_phnum * ehdr->e_phentsize;
    phdr = (Elf32_Phdr *) malloc(phsize);

    lseek(in_file, (off_t) ehdr->e_phoff, SEEK_SET);
    check_read(in_file, (char*)phdr, phsize);

    for (i = 0, ph = phdr; i < ehdr->e_phnum; i++, ph++) {
	switch ((int)ph->p_type) {
	case PT_LOAD:
	    if (ph->p_flags == (PF_R | PF_X)) {
		lp->text_start = trunc_page(ph->p_vaddr);
#ifdef hp_pa
		lp->text_size = (ph->p_vaddr + ph->p_filesz) - lp->text_start;
#else
		lp->text_size =
		    round_page(ph->p_vaddr + ph->p_filesz) - lp->text_start;
#endif
		lp->text_offset = trunc_page(ph->p_offset);
#ifdef hp_pa
	    } else if (ph->p_flags == (PF_R | PF_W)) {
#else
	    } else if (ph->p_flags == (PF_R | PF_W | PF_X)) {
#endif
		lp->data_start = trunc_page(ph->p_vaddr);
		lp->data_size =
		    (vm_size_t)ph->p_vaddr + ph->p_filesz - lp->data_start;
		lp->data_offset = trunc_page(ph->p_offset);
		lp->bss_size = ph->p_memsz - ph->p_filesz;
	    } else
		printf("ELF: Unknown program header flags 0x%x\n",
		       (int)ph->p_flags);
	    break;
	case PT_INTERP:
	case PT_DYNAMIC:
	case PT_NOTE:
	case PT_SHLIB:
	case PT_PHDR:
	case PT_NULL:
	    break;
	}
    }

    shsize = ehdr->e_shnum * ehdr->e_shentsize;
    shdr = (Elf32_Shdr *) malloc(shsize);

    lseek(in_file, (off_t) ehdr->e_shoff, SEEK_SET);
    check_read(in_file, (char*)shdr, shsize);

    for (i = 0, sh = shdr; i < ehdr->e_shnum; i++, sh++) {
	switch ((int)sh->sh_type) {
	case SHT_SYMTAB:
	    lp->sym_offset[0] = (vm_offset_t) sh->sh_offset;
	    lp->sym_size[0] =
		(vm_size_t) sh->sh_size / sh->sh_entsize *
		    sizeof(struct nlist);
	    lp->sym_offset[1] = 0;
	    lp->sym_size[1] = 0;
	    if (sh->sh_link > 0
		&& sh->sh_link < ehdr->e_shnum
		&& (int)shdr[sh->sh_link].sh_type == SHT_STRTAB) {

		lp->str_offset = (vm_offset_t) shdr[sh->sh_link].sh_offset;
		lp->str_size = (vm_size_t) shdr[sh->sh_link].sh_size;
	    }
	    break;
	case SHT_NULL:
	case SHT_STRTAB:
	case SHT_PROGBITS:
	case SHT_RELA:
	case SHT_HASH:
	case SHT_DYNAMIC:
	case SHT_NOTE:
	case SHT_NOBITS:
	case SHT_REL:
	case SHT_SHLIB:
	case SHT_DYNSYM:
	    break;
	}
    }

    return 0;
}

void
elf_symload(int in_f, int out_f, objfmt_t ofmt)
{
    struct loader_info *lp = &ofmt->info;
    char     *buf;
    int      strsize = 0;
    int      symsize;
    struct nlist nl;
    Elf32_Sym *sstab, *estab;

    memset(&nl, 0, sizeof(nl));
    symsize = lp->sym_size[0] / sizeof(struct nlist) * sizeof(Elf32_Sym);
    buf = (char*) malloc(symsize);
    lseek(in_f, (off_t)lp->sym_offset[0], SEEK_SET);
    check_read(in_f, buf, symsize);

    sstab = (Elf32_Sym *)buf;
    estab = (Elf32_Sym *)(buf + symsize);

    while(sstab < estab)
    {
	nl.n_un.n_strx = (long)sstab->st_name + sizeof(int);
	switch (ELF32_ST_TYPE(sstab->st_info)) {
	case STT_FILE:
	    nl.n_type = N_FN;
	    break;
	case STT_OBJECT:
	case STT_FUNC:
	    if (sstab->st_value >= lp->text_start &&
		sstab->st_value < lp->text_start + lp->text_size) {
		nl.n_type = N_TEXT;
		break;
	    }
	    if (sstab->st_value >= lp->data_start &&
		sstab->st_value < lp->data_start + lp->data_size) {
		nl.n_type = N_DATA;
		break;
	    }
	    if (sstab->st_value >= lp->data_start + lp->data_size &&
		sstab->st_value < lp->data_start + lp->data_size + lp->bss_size + sizeof(int)) {
		nl.n_type = N_BSS;
		break;
	    }
	    nl.n_type = N_ABS;
	    break;
	case STT_NOTYPE:
	    if (sstab->st_shndx == SHN_UNDEF) {
		nl.n_type = N_UNDF;
		break;
	    }
	default:
	    printf("ELF32_ST_TYPE %d\n", ELF32_ST_TYPE(sstab->st_info));
	    break;
	}
	switch (ELF32_ST_BIND(sstab->st_info)) {
	case STB_GLOBAL:
	    nl.n_type |= N_EXT;
	    break;
	case STB_LOCAL:
	    break;
	default:
	    printf("ELF32_ST_BIND %d\n", ELF32_ST_BIND(sstab->st_info));
	    break;
	}
	nl.n_other = (char)0;
	nl.n_desc = (short)0;
	nl.n_value = (unsigned long)sstab->st_value;
	write(out_f, &nl, sizeof(struct nlist));
	sstab++;
    }
    free(buf);

    strsize = lp->str_size + sizeof(int);
    write(out_f, &strsize, sizeof(int));

    lseek(in_f, (off_t)lp->str_offset, SEEK_SET);
    file_copy(out_f, in_f, lp->str_size);

    lp->sym_size[0] += strsize;
}
