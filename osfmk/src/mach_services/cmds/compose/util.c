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
 * util.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * $Revision: 1.1.9.2 $
 * $Date: 1995/03/10 22:18:25 $
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>

/* 
 * System prototypes
 */
void *	malloc( unsigned );


char *
xerox(char *str)
{
  char *temp;
  int len;

  assert(str);
  len = strlen(str) + 2;
  temp = (char *) malloc(len);
  strcpy(temp, str);
  return (temp);
}


char *
join(char *str1, char *str2)
{
  char *temp;
  int len;

  if (!str1)
    return (str2);
  if (!str2)
    return (str1);

  len = strlen(str1) + strlen(str2) + 2;
  temp = (char *) malloc(len);
  strcpy(temp, str1);
  strcat(temp, str2);
  return (temp);
}
