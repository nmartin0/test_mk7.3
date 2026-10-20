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
 * hdr-utils.c
 * Revision 1.2  1993/08/30  23:58:19  toshi
 * *** empty log message ***
 *
 * Revision 1.1  1993/08/26  18:26:14  toshi
 * Initial revision
 */
/*
 *
 * hdr-utils.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * Revision: 1.4
 * Date: 1993/08/12 19:30:03
 */

/*
 *
 *  Support functions for converting between different byte orders
 *
 *    This file represents work in progress
 *
 */

#include <stdio.h>
#include <string.h>
#include <machine/endian.h>
#include <mach.h>
#include <mach/message.h>
#include <xkern/include/xk_debug.h>

#define HDRUTILS

#ifdef hp700
#define BE
#elif defined(mips)
#define LE
#elif defined(i386)
#define LE
#endif

#include <s_xkern/include/prot/hdr_utils.h>  /* HDRUTILS affects hdr-utils.h */

extern int tracemachripcp;

unsigned int nop_1(char *n);
char	nop_8(char *n);
char	twist_8(char *n);
int	nop_32(char *n);
int	swap_32(char *n);
char	unscramble_8(char *n);
short	unscramble_16(char *n);
int	unscramble_32(char *n);
long	unscramble_64(char *n);
short	twist_16(char *n);
int	twist_32(char *n);
long	twist_64(char *n);
float	fnop_32(char *n);
float	fswap_32(char *n);
float	funscramble_32(char *n);
float	ftwist_32(char *n);
char	swap_8(char *n);
short	swap_16(char *n);
int	swap_32(char *n);
long	swap_64(char *n);
short	nop_16(char *n);
long	nop_64(char *n);
float	real_error(int n);
int	arch_unpermute_index(enum SOURCE_BYTE_ARCH arch_type);
void	set_convert_vector(
	    enum SOURCE_BYTE_ARCH arch, convert_func_t * vec);

/*
 * general conversions: i.e. irrespective of the endian of
 * this machine
 */
unsigned int
nop_1(
	char	*n)
{ 
    return *n; 
}

char
nop_8(
	char	*n)
{ 
    return *n; 
}

char
twist_8(
	char	*n)
{ 
    return *n & 0xff; 
}

float
fnop_32(
	char	*n)
{ 
    int some = nop_32(n);
    return *(float *)&some;
}

float
fswap_32(
	char	*n)
{ 
    int some = swap_32(n);
    return *(float *)&some;
}

float
funscramble_32(
	char	*n)
{ 
    int some = unscramble_32(n);
    return *(float *)&some;
}

float
ftwist_32(
	char	*n)
{ 
    int some = twist_32(n);
    return *(float *)&some;
}

long
swap_64(
	char	*n)
{
#ifdef	HASLONGLONGS
    /* make this have two 32-bit quantities */
     return    swap_32(n + 8) | swap_32(n);
#else	/* !HASLONGLONGS */
    return 	0;
#endif	/* !HASLONGLONGS */
}

/* if this is a LE machine */
#ifdef LE
int
nop_32(
	char	*n)
{ 
    return ( ( ( ( (
	               (*(n+3) & 0xff)
	                                 << 8
	           ) | (*(n+2) & 0xff)
	         )                       << 8
	       )     | (*(n+1) & 0xff)
	     )                           << 8
	   )         | (*n     & 0xff) ;
}

short
nop_16(
	char	*n)
{
    return ((*(n+1) & 0xff) << 8) | (*n & 0xff);
}

long
nop_64(
	char	*n)
{ 
#ifdef	HASLONGLONGS
    return	(nop_32(n) | nop_32(n+4) << 32);
#else	/* !HASLONGLONGS */
    return	0;
#endif	/* !HASLONGLONGS */
}

short
twist_16(
	char	*n)
{ 
    return (((*n & 0xff) << 8) | (*(n+1) & 0xff));
}

int
twist_32(
	char	*n)
{ 
    return ( ( ( ( (
	               (*n     & 0xff)
	                               << 8
	           ) | (*(n+1) & 0xff)
	         )                     << 8
	       )     | (*(n+2) & 0xff)
	     )                         << 8
	   )         | (*(n+3) & 0xff)	; 
}

long
twist_64(
	char	*n)
{
#ifdef	HASLONGLONGS
    return 	twist_32(n)<<32 | twist_32(n+8);
#else	/* !HASLONGLONGS */
    return 	0;
#endif	/* !HASLONGLONGS */
}

#endif	/* LE */

/* if this is a BE machine */
#ifdef	BE
int
nop_32(
	char	*n)
{ 
    return ( ( ( ( (
	               (*n     & 0xff)
	                               << 8
	           ) | (*(n+1) & 0xff)
	         )                     << 8
	       )     | (*(n+2) & 0xff)
	     )                         << 8
	   )         | (*(n+3) & 0xff); 
}

short
nop_16(
	char	*n) 
{ 
    return ((*n & 0xff)<<8) | (*(n+1)&0xff); 
}

long
nop_64(
	char	*n)
{
#ifdef	HASLONGLONGS
    return 	(nop_32(n) << 32) | nop_32(n+4);
#else	/* !HASLONGLONGS */
    return 	0;
#endif	/* !HASLONGLONGS */

}

short
twist_16(
	char	*n)
{
    return (((*(n+1) & 0xff) << 8) | (*n & 0xff));
}

int
twist_32(
	char	*n)
{
    return ( ( ( ( (
		       (*(n+3) & 0xff)
	                               << 8
	           ) | (*(n+2) & 0xff)
	         )                     << 8
	       )     | (*(n+1) & 0xff)
	     )                         << 8
	   )         | (*n     & 0xff)	; 
}

long
twist_64(
	char	*n)
{  
#ifdef	HASLONGLONGS
    return 	(twist_32(n) | twist_32(n+8)<<32);
#else	/* !HASLONGLONGS */
    return 	0;
#endif	/* !HASLONGLONGS */
}

#endif	/* BE */

int
swap_32(
	char	*n)
{ 
    return ((*n & 0xffff0000) << 16) | (*(n+2) & 0xffff); 
}

short
swap_16(
	char	*n) 
{ 
    return ((*n & 0xff) << 8) | (*(n+1) & 0xff); 
}

char
swap_8(
	char	*n)
{ 
    return *n & 0xff; 
}

long
unscramble_64(
	char	*n)
{ 
#ifdef	HASLONGLONGS
    return 	unscramble_32(n)<<32 | unscramble_32(n+8);
#else	/* !HASLONGLONGS */
    return 	0;
#endif	/* !HASLONGLONGS */
}

int
unscramble_32(
	char	*n)  
{ 
    return ( ( ( ( (
	               (*n     & 0xff)
	                               << 8
	           ) | (*(n+1) & 0xff)
	         )                     << 8
	       )     | (*(n+2) & 0xff)
	     )                         << 8
	   )         | (*(n+3) & 0xff)	; 
}

short
unscramble_16(
	char	*n)
{ 
    return *n; 
}

char
unscramble_8(
	char	*n)
{ 
    return *n & 0xff; 
}

float
real_error(
	int	n)
{
    xTrace0(machripcp, TR_ALWAYS,
	"machnetipc: data conversion: cannot convert reals");
    return 0.0;
}

long  (*unpermute_int64[4])() = { nop_64, swap_64, twist_64, unscramble_64 };

int   (*unpermute_int32[4])() = { nop_32, swap_32, twist_32, unscramble_32 };

short (*unpermute_int16[4])() = { nop_16, swap_16, twist_16, unscramble_16 };

float (*unpermute_real32[4])()= { fnop_32, fswap_32, ftwist_32,funscramble_32};

int
arch_unpermute_index(
	enum SOURCE_BYTE_ARCH	arch_type)
{
    switch (arch_type) {
	case MN_ARCH_MARKER:  return(0);
	case MN_OTHER_ENDIAN: return(1);
	case MN_WORD_SWAP:    return(2);
	case MN_SCRAMBLE:     return(3);
    }
    xTrace1(machripcp, TR_ERRORS,
	"machnetipc: arch_unpermute_index: bad arch_type 0x%x", arch_type);
    return 0;
}

/*
 *  set_convert_vector (arch, vec)
 *
 *   Sets up a vector of functions for converting from a given
 *   source architecture to the local architecture.
 *   The resulting vector is indexed by the data type ---
 *      
 *     vec[MACH_MSG_TYPE_INTEGER_16] is a function to convert
 *       from an incoming representation of a 16-bit integer
 *       to a local integer.
 */

void
set_convert_vector(
	enum SOURCE_BYTE_ARCH	arch,
	convert_func_t		*vec)
{
    int	archin = arch_unpermute_index(arch);

    vec[XK_BYTE]       = (convert_func_t)nop_8;
    vec[XK_INTEGER_16] = (convert_func_t)unpermute_int16[archin];
    vec[XK_INTEGER_32] = (convert_func_t)unpermute_int32[archin];
    vec[XK_INTEGER_64] = (convert_func_t)unpermute_int64[archin];
    vec[XK_REAL_32]    = (convert_func_t)unpermute_real32[archin];
/*
 *  vec[XK_REAL_64]    = (convert_func_t)unpermute_real64[archin];
 */
    vec[XK_STRING]     = (convert_func_t)nop_8;
}

