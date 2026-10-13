/* 
 * Mach Operating System
 * Copyright (c) 1994 Jukka Virtanen
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JUKKA VIRTANEN ALLOWS FREE USE OF THIS SOFTWARE IN ITS 'AS IS'
 * CONDITION.  JUKKA VIRTANEN DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/* 
 * HISTORY
 * 06-Nov-94  Johannes Helander (jvh) at Helsinki University of Technology
 *	Use endian.h
 *
 * $Log: ntoh.c,v $
 * Revision 1.2  1996/03/14  21:08:18  sclawson
 * Jukka's changes.
 *
 * Revision 1.1.1.1  1995/03/02  21:49:40  mike
 * Initial Lites release from hut.fi
 *
 *
 */
/* 
 *	File:	alpha/ntoh.c
 *	Author:	Jukka Virtanen, Helsinki University of Technology, 1994.
 *	Date:	November 1994
 *
 */

#include <machine/endian.h>

#if BYTE_ORDER == LITTLE_ENDIAN

unsigned int
ntohl (unsigned int n)
{
  /* Swap bytes in a word from network order to host order */
  return  ((n & 0xff)        << 24)
    	| ((n & 0xff00)      << 8)
	| ((n & 0xff0000)    >> 8)
	| ((n & 0xff000000)  >> 24);
}

unsigned int
htonl (unsigned int n)
{
  /* Swap bytes in a word from network order to host order */
  return  ((n & 0xff)        << 24)
    	| ((n & 0xff00)      << 8)
	| ((n & 0xff0000)    >> 8)
	| ((n & 0xff000000)  >> 24);
}

unsigned short
htons(unsigned short n)
{
  /* Swap bytes in a short */
  return  ((n & 0xff)   << 8)
        | ((n & 0xff00) >> 8);
}

unsigned short
ntohs(unsigned short n)
{
  /* Swap bytes in a short */
  return  ((n & 0xff)   << 8)
        | ((n & 0xff00) >> 8);
}

#endif /* BYTE_ORDER == LITTLE_ENDIAN */
