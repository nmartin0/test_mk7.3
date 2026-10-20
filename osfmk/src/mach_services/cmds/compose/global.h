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
 * global.h,v
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * global.h,v
 * Revision 1.18.1.2  1994/07/22  20:01:52  menze
 * XObjects use Paths instead of Allocators
 *
 * Revision 1.18.1.1  1994/04/13  01:55:36  menze
 * Added support for 'alloc' field
 *
 * Revision 1.18  1994/04/07  23:24:47  menze
 *   [ 1994/03/08          menze ]
 *   added 'romopt' and 'romfile' entries, allowing rom files and options
 *   to be statically configured at compose/compile time
 *
 * Revision 1.17  1994/02/04  23:41:43  menze
 *   [ 1994/01/04          menze ]
 *   Name of make variable (naming the platform include file) changed.
 *
 * Revision 1.16  1993/12/17  00:55:19  menze
 * Fixed #endif comment
 *
 */

#ifndef	global_h
#define global_h

#define MAX_FILES 20
#define MAX_PROTOCOL 50

typedef struct {
    char *	name;
    char *	instance;
    int		index;		/* index into instance table */
} ProtName;

typedef struct {
    char *	name;
    char *	value;
    int		instantiate;
    char *	ifdef;
    char *	ielse;
    char *	endif;
} TraceVar;

typedef enum {
    INIT_NOT_DONE,
    INIT_WORKING,
    INIT_DONE
} InitState;

typedef struct protocol {
    ProtName	n;
    char *	device;
    char *	options;
    char *	path;
    char *	trace;	  /* String rep. of initial value of trace variable */
    char *	traceVar; /* String rep. of name of trace variable */
    char *	x_path;   /* String rep. of path object */
    int		numfiles;
    char * 	files[MAX_FILES + 1];
    int 	numdown;
    ProtName	down[MAX_PROTOCOL + 1];
    InitState	initState;
    int		isDriver;
    TraceVar	*tv;
} PROTOCOL;

extern int	fileLine;
extern int	filePosition;

void	addInstance( PROTOCOL * );
void	addProtTbl( char *name );
void	addRomFile( char *name );
void	addRomOption( char *name );
TraceVar	*addTraceVar( char *name, char * value, int instantiate,
			      char *device, char *options );
void	errorCycle( void );
void	errorCycleName( char * );
void	errorFile( char *s );
void	errorLaterInstanceFiles( PROTOCOL * );
void	errorProtlUndefined( char *p1, char *p2 );
void    errorRomLineTooLong(char *);
void	errorTooManyStates( void );
void	finishErrorCycle( void );
char *	join( char *, char * );
void	lastDriver( void );
void	parse( void );
void	syntaxErrorChar( char expected, char got );
void	syntaxErrorString( char *expected );
char *	xerox( char * );
void 	warnCouldNotAccess( char * );
void	warnDefaultPtblNotSupported( void );
void	warnProtNotFound( char * );
void	warnReassignedTraceValue( char * );

#include "compose_mach.h"

#endif	/* global_h */
