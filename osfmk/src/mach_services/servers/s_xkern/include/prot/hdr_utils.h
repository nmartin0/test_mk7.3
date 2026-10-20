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
 *
 * hdr-utils.h
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.2
 * Date: 1993/06/24 01:04:59
 */

/*
 *  Support functions for converting between different byte orders
 */

#ifndef	HDR_UTILS_H
#define	HDR_UTILS_H

#define MAX_MACH_DATA_TYPE	MACH_MSG_TYPE_STRING_C+1

#define MNBYTESIZE	8

#define MN_ARCH_MARKER	\
	    (1<<(3*MNBYTESIZE) | ((2<<(2*MNBYTESIZE) | 3 <<MNBYTESIZE) | 4))

#define MN_OTHER_ENDIAN	\
	    (2<<(3*MNBYTESIZE)|((1<<(2*MNBYTESIZE) | 4 <<MNBYTESIZE) | 3))

#define MN_WORD_SWAP	\
	    (4<<(3*MNBYTESIZE)|((3<<(2*MNBYTESIZE) | 2 <<MNBYTESIZE) | 1))

#define MN_SCRAMBLE	\
	    (3<<(3*MNBYTESIZE)|((4<<(2*MNBYTESIZE) | 1 <<MNBYTESIZE) | 2))

enum SOURCE_BYTE_ARCH
{ 
    PERM1=MN_ARCH_MARKER,
    PERM2=MN_OTHER_ENDIAN,
    PERM3=MN_WORD_SWAP,
    PERM4=MN_SCRAMBLE
};

enum REAL_NUMBER_TYPE
{ 
    FLOAT,
    DOUBLE,
    DOUBLELONG
};

enum MACHINE_TYPE
{ 
    VAX,
    SUN3,
    MIPS,
    SUN4,
    HPRISC,
    M68020,
    INTELn86,
    ALPHA
};

enum XK_DATA_TYPE
{
    XK_BYTE,
    XK_INTEGER_16,
    XK_INTEGER_32,
    XK_INTEGER_64,
    XK_REAL_32,
    XK_REAL_64,
    XK_STRING,
    XK_DATA_TYPE_MAX
};

enum XK_DATA_SIZE
{
    XK_BYTE_SIZE	= 1,
    XK_INTEGER_16_SIZE	= 2,
    XK_INTEGER_32_SIZE	= 4,
    XK_INTEGER_64_SIZE	= 8,
    XK_REAL_32_SIZE	= 4,
    XK_REAL_64_SIZE	= 8,
    XK_STRING_SIZE	= 1
};

#define XK_DATA_SIZE_VECTOR	\
{				\
    XK_BYTE_SIZE,		\
    XK_INTEGER_16_SIZE,		\
    XK_INTEGER_32_SIZE,		\
    XK_INTEGER_64_SIZE,		\
    XK_REAL_32_SIZE,		\
    XK_REAL_64_SIZE,		\
    XK_STRING_SIZE		\
}

typedef any_t		(*convert_func_t)(any_t, ...);

#ifndef HDRUTILS

extern long	(*unpermute_int64[4])();
extern int	(*unpermute_int32[4])();
extern short	(*unpermute_int16[4])();
extern float	(*unpermute_real32[4])();

extern int	arch_unpermute_index( enum SOURCE_BYTE_ARCH );

extern long	nop_64(char *);

extern int	nop_32(char *);

extern short	nop_16(char *);

extern char	nop_8(char *);

extern void	set_convert_vector(enum SOURCE_BYTE_ARCH, convert_func_t *);

#endif	/* !HDRUTILS */
#endif	/* HDR_UTILS_H */

