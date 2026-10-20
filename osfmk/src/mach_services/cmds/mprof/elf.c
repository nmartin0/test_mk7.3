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

#include <mach/machine/vm_types.h>

#if linux
#include <elf.h>
#else /* linux */
#include <sys/elf.h>
#include <ddb/nlist.h>
#endif /* linux */

#include "mprof.h"

int elf_recog(char *);
int elf_get_symbols(FILE *, char *);
void elf_load_symbols(FILE *, vm_offset_t, int, vm_offset_t, int);

int
elf_recog(char *hdr)
{
    Elf32_Ehdr *x = (Elf32_Ehdr *)hdr;

    if (debug) {
	register i;
	fprintf(stderr, "EI_MAGIC:"); 
	for (i=0; i<EI_NIDENT; i++)
		fprintf(stderr, " %02x", x->e_ident[i]);
	fprintf(stderr, "\n");
    } 
    return (x->e_ident[EI_MAG0] == ELFMAG0 &&
	    x->e_ident[EI_MAG1] == ELFMAG1 &&
	    x->e_ident[EI_MAG2] == ELFMAG2 &&
	    x->e_ident[EI_MAG3] == ELFMAG3);
}

Elf32_Shdr 	*shdr;
int 		shnum;

int
elf_get_symbols(
    FILE *symfile,
    char *sfname)
{
    char hdr[sizeof (Elf32_Ehdr)];
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *) hdr;
    Elf32_Phdr *phdr, *ph;
    Elf32_Shdr *sh;
    size_t phsize;
    size_t shsize;
    int i, totsyms;

    rewind(symfile);
    if (fread(hdr, 1, sizeof (Elf32_Ehdr), symfile) !=
	sizeof (Elf32_Ehdr)) {
	perror(sfname);
	return (0);
    }
    if (!elf_recog(hdr))
	return (0);

    phsize = ehdr->e_phnum * ehdr->e_phentsize;
    phdr = (Elf32_Phdr *) malloc(phsize);

    fseek(symfile, (off_t) ehdr->e_phoff, SEEK_SET);
    if (fread((char*)phdr, 1, phsize, symfile) != phsize) {
	perror("elf phdr read");
	return (0);
    }

    shnum = ehdr->e_shnum;
    shsize = ehdr->e_shnum * ehdr->e_shentsize;
    shdr = (Elf32_Shdr *) malloc(shsize);

    fseek(symfile, (off_t) ehdr->e_shoff, SEEK_SET);
    if (fread((char*)shdr, 1, shsize, symfile) != shsize) {
		perror("elf shdr read");
		return (0);
	}

    totsyms = 0;
    for (i = 0, sh = shdr; i < ehdr->e_shnum; i++, sh++) {
	vm_offset_t stroff;
	int strsize;

	if ((int)sh->sh_type == SHT_SYMTAB) {
	    totsyms += (vm_size_t) sh->sh_size / sh->sh_entsize;
	}
    }

    symbols = (struct symbol *)malloc(totsyms * sizeof (struct symbol));
    if (!symbols) {
	perror("symbol table alloc");
	return (0);
    }
    for (i = 0, sh = shdr; i < ehdr->e_shnum; i++, sh++) {
	vm_offset_t stroff;
	int strsize;

	if (debug)
		fprintf(stderr, "section %d type %08x flags %08x\n",
			i, sh->sh_type, sh->sh_flags);
	if ((int)sh->sh_type == SHT_SYMTAB) {
	    stroff = 0;
	    strsize = 0;
	    if (sh->sh_link > 0
		&& sh->sh_link < ehdr->e_shnum
		&& (int)shdr[sh->sh_link].sh_type == SHT_STRTAB) {
		stroff = (vm_offset_t) shdr[sh->sh_link].sh_offset;
		strsize = (vm_size_t) shdr[sh->sh_link].sh_size;
	    }
	    elf_load_symbols(
		symfile,
		(vm_offset_t)sh->sh_offset,
		(vm_size_t)sh->sh_size,
		stroff,
		strsize);
	}
    }

    return 1;
}

void
elf_load_symbols(
    FILE *symfile,
    vm_offset_t symoff,
    int symsize,
    vm_offset_t stroff,
    int strsize)
{
    char *buf, *str;
    Elf32_Sym *sstab, *estab;

    buf = (char *)malloc(symsize);
    if (!buf) {
	perror("elf syms alloc");
	return;
    }
    fseek(symfile, symoff, SEEK_SET);
    if (fread(buf, 1, symsize, symfile) != symsize) {
	perror("elf sym read");
	return;
    }
    /*
     * Allocate a buffer (permanently) for the string section --
     * stored symbols will point into it.
     */
    if (stroff && strsize) {
	str = (char *)malloc(strsize);
	if (!str) {
	    perror("elf str alloc");
	    return;
	}
	fseek(symfile, stroff, SEEK_SET);
	if (fread(str, 1, strsize, symfile) != strsize) {
	    perror("elf str read");
	    return;
	}
    }

    sstab = (Elf32_Sym *)buf;
    estab = (Elf32_Sym *)(buf + symsize);

    for (; sstab < estab; ++sstab) {
	switch (ELF32_ST_TYPE(sstab->st_info)) {
	case STT_FILE:
	case STT_NOTYPE:
	case STT_SECTION:
#ifndef linux
	case STT_LOPROC:
	case STT_HIPROC:
#endif /* ndef linux */
	    break;
	case STT_OBJECT:
	    if (sstab->st_shndx > shnum ||
		!(shdr[sstab->st_shndx].sh_flags & SHF_EXECINSTR))
			break;
	case STT_FUNC:
	    symbols[nsymbols].name = &str[sstab->st_name];
	    symbols[nsymbols].pc = sstab->st_value;
	    symbols[nsymbols].count = 0;
	    ++nsymbols;
	    if (debug)
		fprintf(stderr, "Text");
	    break;
	default:
	    printf("ELF32_ST_TYPE %d\n", ELF32_ST_TYPE(sstab->st_info));
	    break;
	}
	if (debug) {
		fprintf(stderr, "\t");
		fprintf(stderr, "%08x %08x %08x %02x %02x %04x", 
			sstab->st_name,
			sstab->st_value,
			sstab->st_size,
			sstab->st_info,
			sstab->st_other,
			sstab->st_shndx);
		fprintf(stderr, " <%s>\n",
			&str[sstab->st_name]);
	}
    }
    /*
     * Free symtab buf (but not strings buf)
     */
    free(buf);
}
