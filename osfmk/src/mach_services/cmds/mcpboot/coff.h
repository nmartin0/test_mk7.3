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

typedef struct filehdr {
    unsigned char	f_magic[2];	/* magic number */
    unsigned char	f_nscns[2];	/* number of sections */
    unsigned char	f_timdat[4];	/* time & date stamp */
    unsigned char	f_symptr[4];	/* file pointer to symtab */
    unsigned char	f_nsyms[4];	/* number of symtab entries */
    unsigned char	f_opthdr[2];	/* sizeof(optional hdr) */
    unsigned char	f_flags[2];	/* flags */
} FILHDR;

#define	FILHSZ		sizeof (FILHDR)

#define	LANAI3MAGIC	0x1300		/* LanAI magic number */

#define	F_RELFLG 	0x0001		/* relocation info stripped */
#define	F_EXEC          0x0002		/* file is executable */
#define	F_LNNO          0x0004		/* line numbers stripped */
#define	F_LSYMS         0x0008		/* local symbols stripped */
#define	F_AR16WR        0x0080		/* file is 16-bit little-endian */
#define	F_AR32WR        0x0100		/* file is 32-bit little-endian */
#define	F_AR32W         0x0200		/* file is 32-bit big-endian */

typedef struct aouthdr {
    unsigned char	magic[2];	/* magic number */
    unsigned char	vstamp[2];	/* version stamp */
    unsigned char	tsize[4];	/* text size in bytes */
    unsigned char	dsize[4];	/* initialized data size */
    unsigned char	bsize[4];	/* uninitialized data size */
    unsigned char	entry[4];	/* entry point */
    unsigned char	text_start[4];	/* base of text used for this file */
    unsigned char	data_start[4];	/* base of data used for this file */
} AOUTHDR;

#define	OMAGIC		0x1301	/* object files, eg as output */
#define	ZMAGIC		0x1302	/* demand load format, eg normal ld output */
#define	STMAGIC		0x1303	/* target shlib */
#define	SHMAGIC		0x1304	/* host   shlib */

typedef struct scnhdr {
    unsigned char	s_name[8];	/* section name */
    unsigned char	s_paddr[4];	/* physical address */
    unsigned char	s_vaddr[4];	/* virtual address */
    unsigned char	s_size[4];	/* section size */
    unsigned char	s_scnptr[4];	/* file ptr to raw data for section */
    unsigned char	s_relptr[4];	/* file ptr to relocation */
    unsigned char	s_lnnoptr[4];	/* file ptr to line numbers */
    unsigned char	s_nreloc[2];	/* number of lreocation entries */
    unsigned char	s_nlnno[2];	/* number of line number entries */
    unsigned char	s_flags[4];	/* type and contents flags */
} SCNHDR;

#define	SCNHSZ	sizeof (SCNHDR)

#define	STYP_REG	0x0000	/* Regular section, either text, data, bss,
				   info or lib type */
#define	STYP_DSECT	0x0001	/* Dummy section type created by the
				   assembler's */
#define	STYP_PAD	0x0008	/* Alignment spacing between regular
				   sections */
#define	STYP_TEXT	0x0020	/* Text section: allocated, relocated and
				   loaded */
#define	STYP_DATA	0x0040	/* Data section: allocated, relocated and
				   loaded */
#define	STYP_BSS	0x0080	/* Bss sections are only allocated */
#define	STYP_INFO	0x0200	/* Comment section: not allocated, not
				   relocated and not loaded */
#define	STYP_LIB	0x0800	/* Section contains shared-library
				   relocation information */

/*
 * Useful macros
 */
#define	COFF_INT2(p)	((((p)[0]) << 8 ) | \
			 ((p)[1]))
#define	COFF_INT4(p)	((((p)[0]) << 24) | \
			 (((p)[1]) << 16) | \
			 (((p)[2]) <<  8) | \
			 ((p)[3]))

/*
 * External declarations
 */
extern int coff_recog(void);

extern int coff_boot(void);
