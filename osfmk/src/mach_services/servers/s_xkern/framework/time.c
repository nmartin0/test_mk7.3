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
 * time.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * $Revision: 1.1.8.1 $
 * $Date: 1995/02/23 15:08:05 $
 */


#include <sys/time.h>
#include <xkern/include/xtime.h>

struct itimerval i_value, i_zero;

void
xGetTime(
	XTime *where)
{
	gettimeofday((struct timeval *)where, 0);
}

#ifdef READ_CLOCK
int
read_clock(msec)        /* returns the number of msec */
long    *msec;          /* since sometime in late 1986 */
{
  struct  timeval   time;
  struct  timezone  zone;

  gettimeofday(&time, &zone);
  *msec = (time.tv_sec - 500000000)*1000 + (time.tv_usec / 1000);
  return(0);
}
#endif /* READ_CLOCK */
