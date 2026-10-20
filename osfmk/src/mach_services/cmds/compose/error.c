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
 * error.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * $Revision: 1.1.9.2 $
 * $Date: 1995/03/10 22:18:18 $
 */

#include <stdio.h>
#include "global.h"

static char	*cycleArray[MAX_PROTOCOL];
static int	cycleIndex = -1;


void
warnDefaultPtblNotSupported( void )
{
    fprintf(stderr,
	"compose warning: DEFAULT prottbl construction no longer supported\n");
    fprintf(stderr, "\t(line %d)\n", fileLine);
}


void
warnProtNotFound(char *d)
{
  fprintf(stderr, "compose warning: protocol \"%s\" found neither locally nor in libraries\n", d);
  fprintf(stderr, "                 (assuming in library)\n");
}


void
warnCouldNotAccess(char *f)
{
  char buf[200];
  sprintf(buf, "compose warning: could not access %s", f);
  perror(buf);
}


void
warnReassignedTraceValue( char *s )
{
    fprintf(stderr, "compose warning: trace variable %s reassigned\n", s);
}


void
syntaxErrorChar(char expected, char got)
{
  fprintf(stderr,
	  "compose: syntax error: expecting \"%c\" got \"%c\" at line %d, pos %d\n",
	  expected, got, fileLine, filePosition);
  exit(1);
}


void
syntaxErrorString(char *s)
{
  fprintf(stderr,
	  "compose: syntax error: expecting \"%s\" at line %d, pos %d\n",
	  s, fileLine, filePosition);
  exit(1);
}


void
errorFile(char *s)
{
  fprintf(stderr, "compose error: can't open required file \"%s\"\n", s);
}

void
errorRomLineTooLong(char *s)
{
    fprintf(stderr, "compose error: rom file entry in \"%s\" is too long", s);
    exit(1);
}

void
errorProtlUndefined(char *p1, char *p2)
{
  fprintf(stderr,
	  "compose error: undefined protocol \"%s\" referenced by \"%s\"\n",
	  p1, p2);
}


void
errorTooManyStates( void )
{
    fprintf(stderr, "compose error: too many state transition characters\n");
    exit(1);
}

void
errorCycle( void )
{
    fprintf(stderr, "Compose error -- cycle detected in dependency graph:\n");
}

void
errorCycleName( char *n )
{
    cycleArray[++cycleIndex] = n;
}

void
finishErrorCycle( void )
{
    fprintf(stderr, "\t");
    for ( ; cycleIndex > 0; cycleIndex-- ) {
	fprintf(stderr, "%s -> ", cycleArray[cycleIndex]);
    }
    fprintf(stderr, "%s\n", cycleArray[cycleIndex]);
    exit(1);
}


void
errorLaterInstanceFiles( PROTOCOL *p )
{
    fprintf(stderr, "Compose error at line %d, pos %d\n", fileLine,
	    filePosition);
    fprintf(stderr, "\tprotocol %s has a previous instance\n", p->n.name);
    fprintf(stderr,
	    "\tSubsequent instances may only define 'name' and 'protocols'\n");
    exit(1);
}
