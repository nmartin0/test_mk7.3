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
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)hertz.c	5.3 (Berkeley) 6/29/88";
#endif /* not lint */

#include <stdio.h>
#include <sys/time.h>

    /*
     *	discover the tick frequency of the machine
     *	if something goes wrong, we return 0, an impossible hertz.
     */
#define	HZ_WRONG	100

hertz()
{
	struct itimerval tim;

	tim.it_interval.tv_sec = 0;
	tim.it_interval.tv_usec = 1;
	tim.it_value.tv_sec = 0;
	tim.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &tim, 0);
	setitimer(ITIMER_REAL, 0, &tim);
	if (tim.it_interval.tv_usec < 2) {
		fprintf(stderr,
			"could not determine HZ, assume %d\n",
			HZ_WRONG);
		return(HZ_WRONG);
	}
	return (1000000 / tim.it_interval.tv_usec);
}
