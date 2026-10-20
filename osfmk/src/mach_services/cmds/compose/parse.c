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
 * parse.c,v
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * parse.c,v
 * Revision 1.18.1.3  1994/07/22  20:02:31  menze
 * XObjects use Paths instead of Allocators
 *
 * Revision 1.18.1.2  1994/05/05  20:07:30  menze
 * romopt entries are semi-colon terminated for consistency
 *
 * Revision 1.18.1.1  1994/04/13  01:55:59  menze
 * Added support for 'alloc' field
 *
 * Revision 1.18  1994/04/07  23:32:54  menze
 *   [ 1994/03/08          menze ]
 *   added 'romopt' and 'romfile' entries, allowing rom files and options
 *   to be statically configured at compose/compile time
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "global.h"

#define BUF_SIZE 80

#define strsame(A,B) (!(strcmp((A),(B))))

static int cur;
static int done = 0;
static int debug = 0;
int fileLine = 1;
int filePosition = 0;
static enum {
    DEV_STATE, PROT_STATE, TRACE_STATE
} state;

static char *	DEVICE(void);
static char *	DIRECTORY(void);
static void	EDGE(PROTOCOL *);
static void	FILELIST(PROTOCOL *);
static char *	FILENAME(void);
static void	FILES(PROTOCOL *);
#if 0
static int	NUMBER(void);
#endif
static char *	OPTIONS(void);
static char *	OPTNAME(void);
static void	PATH( PROTOCOL * );
static ProtName	PNAME(void);
static void	PROTOCOLLIST(PROTOCOL *);
static ProtName	PROTOCOLNAME(void);
static char *	PROTOCOLSTRING( void );
static void	PROTS(PROTOCOL *);
static void	ROM(void);
static void 	S(void);
static char *	STRING(void);
static void	TBL( void );
static char *	UPTO_CHAR( char );
static void	TRACE( PROTOCOL * );
static void	TRACEVAR( PROTOCOL *p );
static void	force_char(char);
static void	force_string(char *, char *);
static void	get(void);
static void	skip_blanks(void);
static void	skip_blanks_force( void );


static void
changeState()
{
    switch( state ) {

      case DEV_STATE:
	lastDriver();
	state = PROT_STATE;
	break;

      case PROT_STATE:
	state = TRACE_STATE;
	break;
	
      case TRACE_STATE:
	errorTooManyStates();
	break;
    }
}


void
parse()
{
    if (debug)
	printf("\n>parse\n");
    get();
    S();
}


/*
 * S -> ( EDGE | TBL | @ ) , S | EPSILON
 */
static void
S()
{
    PROTOCOL	p;
    
    if (debug)
      printf("\n>S\n");
    skip_blanks();
    while (!done) {
	if (cur == '@') {
	    force_char('@');
	    skip_blanks();
	    force_char(';');
	    changeState();
	} else if (cur == 'n') {
	    /* 
	     * Must be an EDGE
	     */
	    /* 
	     * Add to the trace variable list for both protocols and
	     * edges which represent only trace variables
	     */
	    bzero((char *)&p, sizeof(PROTOCOL));
	    EDGE(&p);
	    {
		char	varName[160];
		int	instantiate = 0;
		
		if ( p.traceVar ) {
		    if ( strsame(p.traceVar, "private") ) {
			sprintf(varName, "trace%s_%s",
				p.n.name, p.n.instance);
			instantiate = 1;
		    } else {
			sprintf(varName, "trace%s", p.traceVar);
		    }
		} else {
		    sprintf(varName, "trace%s", p.n.name);
		    if ( state == DEV_STATE || state == PROT_STATE ) {
			strcat(varName, "p");
		    }
		}
		p.tv = addTraceVar(varName, p.trace, instantiate,
				   p.device, p.options);
	    }
	    if ( state == DEV_STATE || state == PROT_STATE ) {
		/* 
		 * These are the only states where the edge really
		 * represents a protocol.  In the TRACE_VAR state, the
		 * edge is just a trace variable
		 */
		addInstance(&p);
	    }
	} else if ( state == TRACE_STATE ) {
	    if (cur == 'p') {
		/* 
		 * Must be a table entry
		 */
		TBL();
	    } else if ( cur == 'r' ) {
		ROM();
	    }
	} else {
	    syntaxErrorString("'name' or 'prottbl' or '@'");
	}
	skip_blanks();
    }
}


static void
TBL()
{
    char *	fileName;

    force_string("prottbl", 0);
    skip_blanks();
    force_string("=", 0);
    skip_blanks();
    fileName = FILENAME();
    if (*fileName == 0) {
	syntaxErrorString("filename");
    }
    addProtTbl(fileName);
    skip_blanks();
    force_char(';');
    skip_blanks();
}    


static void
ROM()
{
    char *	fileName;
    char *	opt;

    force_string("rom", 0);
    if ( cur == 'f' ) {
	force_string("file", 0);
	skip_blanks();
	force_char('=');
	skip_blanks();
	fileName = FILENAME();
	if (*fileName == 0) {
	    syntaxErrorString("filename");
	}
	skip_blanks();
	force_char(';');
	skip_blanks();
	addRomFile(fileName);
    } else if ( cur == 'o' ) {
	force_string("opt", 0);
	skip_blanks();
	opt = UPTO_CHAR(';');
	force_char(';');
	skip_blanks();
	addRomOption(opt);
    } else {
	syntaxErrorString("file or opt");
    }
}    




#if 0

/*
 * NUMBER 		-> [0..9]*
 */
static int
NUMBER()
{
    int i = 0;

    if (debug)
	printf("\n>NUMBER\n");

    while (isdigit(cur)) {
	i = i * 10 + cur - '0';
	get();
    }
    return i;
}

#endif

/*
 * EDGE ->
 *   PNAME " " (DIRECTORY " ") (FILES " ") (PROTS " ") (PATH " ")
 *   (OPTIONS " ") (DEVICE " ") ";"
 */
static void
EDGE(
    PROTOCOL	*p)
{
    if (debug)
	printf("\n>EDGE\n");
    p->n = PNAME();
    while (1) {
	if ( cur != ';' ) {
	    skip_blanks_force();
	}
	switch ( cur ) {
	    
	  case ';':
	    force_char(';');
	    return;
	    
	  case 'd':
	    get();
	    if ( cur == 'i') {
		p->path = DIRECTORY();
	    } else if ( cur == 'e' ) {
		p->device = DEVICE();
	    } else {
		syntaxErrorString("'directory' or 'device'");
	    }
	    break;
	    
	  case 'f':
	    FILES(p);
	    break;

	case 'o':
	    p->options = OPTIONS();
	    break;
	    
	  case 'p':
	    get();
	    if ( cur == 'r' ) {
		PROTS(p);
	    } else if ( cur == 'a' ) {
		PATH(p);
	    } else {
		syntaxErrorString("'protocols' or 'path'");
	    }
	    break;
	    
	  case 't':
	    TRACE(p);
	    break;

	  default:
	    syntaxErrorString("'dir', 'files', 'protocols', 'trace', 'path', or ';'");
	}
    }
}


/* 
 * TRACE = "trace=" STRING | "traceVar =" STRING
 */
static void
TRACE(
    PROTOCOL	*p)
{
    force_string("trace", 0);
    if ( cur == 'v' ) {
	TRACEVAR( p );
    } else {
	skip_blanks();
	force_char('=');
	skip_blanks();
	p->trace = STRING();
    }
}


static void
TRACEVAR(
    PROTOCOL	*p)
{
    force_string("var", 0);
    skip_blanks();
    force_char('=');
    skip_blanks();
    p->traceVar = STRING();
}


/*
 * PNAME -> "name=" PROTOCOLNAME
 */
static ProtName
PNAME()
{
    if (debug)
	printf("\n>PNAME\n");
    force_string("name", 0);
    skip_blanks();
    force_char('=');
    skip_blanks();
    return PROTOCOLNAME();
}


/*
 * PATH -> "path=" STRING
 */
static void
PATH( PROTOCOL *p )
{
    if (debug)
      printf("\n>ALLOC\n");
    force_string("ath", "path");
    skip_blanks();
    force_char('=');
    skip_blanks();
    p->x_path = STRING();
}


/*
 * DIRECTORY -> "dir=" STRING
 */
static char *
DIRECTORY()
{
  if (debug)
    printf("\n>DIRECTORY\n");
  force_string("ir", "dir");
  skip_blanks();
  force_char('=');
  skip_blanks();
  return STRING();
}
  

/*
 * FILES -> "files=" FILELIST
 */
static void
FILES(
    PROTOCOL	*p)
{
    if (debug)
	printf("\n>FILE\n");
    force_string("files", 0);
    skip_blanks();
    force_char('=');
    skip_blanks();
    FILELIST(p);
}


/*
 * FILELIST -> FILENAME | FILENAME "," FILELIST
 */
static void
FILELIST(
    PROTOCOL	*p)
{
  if (debug)
    printf("\n>FILELIST\n");
  p->numfiles = 0;
  p->files[0] = FILENAME();
  while (cur == ',') {
    get();
    p->numfiles++;
    p->files[p->numfiles] = FILENAME();
  }
  p->numfiles++;
}


static ProtName
PROTOCOLNAME()
{
    ProtName	p;

    p.name = PROTOCOLSTRING();
    if ( cur == '/' ) {
	get();
	p.instance = PROTOCOLSTRING();
    } else {
	p.instance = "";
    }
    return p;
}


/*
 * PROTOCOLSTRING -> [a-z_0..9]*
 */
static char *
PROTOCOLSTRING()
{
    int len = 0;
    char buf[BUF_SIZE];
    
    if (debug)
      printf("\n>PROTOCOLNAME\n");
    while (islower(cur) || isupper(cur) || isdigit(cur) || (cur == '_')) {
	buf[len++] = cur;
	get();
    }
    buf[len] = 0;
    return (xerox(buf));
}


/*
 * STRING -> [^ \t\n;]*
 */
static char *
STRING()
{
  int len = 0;
  char buf[BUF_SIZE];

  if (debug)
    printf("\n>STRING\n");
  while (cur != ' ' && cur != '\t' && cur != '\n' && cur != ';') {
    buf[len++] = cur;
    get();
  }
  buf[len] = 0;
  return (xerox(buf));
}


/*
 * PROTS --> "protocols=" PROTOCOLLIST
 */
static void
PROTS(
    PROTOCOL	*p)
{
    if (debug)
	printf("\n>PROTS\n");
    force_string("rotocols", "protocols");
    skip_blanks();
    force_char('=');
    skip_blanks();
    PROTOCOLLIST(p);
}


/*
 * PROTOCOLIST -> PROTOCOLNAME "," PROTOCOLLIST
 */
static void
PROTOCOLLIST(
    PROTOCOL	*p)
{
    if (debug)
	printf("\n>PROTOCOLLIST\n");
    p->numdown = 0;
    p->down[0] = PROTOCOLNAME();
    while (cur == ',') {
	get();
	skip_blanks();
	p->numdown++;
	p->down[p->numdown] = PROTOCOLNAME();
    }
    p->numdown++;
}


/*
 * FILENAME -> [./-A-Za-z_0..9]*
 */
static char *
FILENAME()
{
  int len = 0;
  char buf[BUF_SIZE];

  if (debug)
    printf("\n>FILENAME\n");
  while (islower(cur) || isupper(cur) || isdigit(cur) || (cur == '_') ||
	 (cur == '/') || (cur == '.') || (cur == '-')) {
    buf[len++] = cur;
    get();
  }
  buf[len] = 0;
  return (xerox(buf));
}


/*
 * OPTNAME -> [a-z_0..9]*
 */
static char *
OPTNAME()
{
  int len = 0;
  char buf[BUF_SIZE];

  if (debug)
    printf("\n>OPTNAME\n");
  while (islower(cur) || isdigit(cur) || (cur == '_')) {
    buf[len++] = cur;
    get();
  }
  buf[len] = 0;
  return (xerox(buf));
}


/*
 * OPTIONS -> "options=" OPTNAME
 */
static char *
OPTIONS()
{
  if (debug)
    printf("\n>OPTIONS\n");
  force_string("options", 0);
  skip_blanks();
  force_char('=');
  skip_blanks();
  return OPTNAME();
}
  

/*
 * DEVICE -> "device=" OPTNAME
 */
static char *
DEVICE()
{
  if (debug)
    printf("\n>DEVICE\n");
  force_string("evice", "device");
  skip_blanks();
  force_char('=');
  skip_blanks();
  return OPTNAME();
}
  

static void
force_char(
    char	ch)
{
  if (cur == ch) {
    get();
  } else {
    syntaxErrorChar(ch, cur);
  }
}


static void
force_string(char *str, char *msgStr)
{
  char *temp;

  temp = str;
  while (cur == *temp) {
    get();
    temp++;
  }
  if (*temp != 0) {
    syntaxErrorString(msgStr ? msgStr : str);
  }
}


static void
skip_blanks()
{
    while ( (cur == ' ') || (cur == '\t') || (cur == '\n') ) {
	get();
    }
}


static void
skip_blanks_force()
{
    if ( (cur == ' ') || (cur == '\t') || (cur == '\n') ) {
	skip_blanks();
    } else {
	syntaxErrorString("white space");
    }
}


static char *
UPTO_CHAR( char c )
{
    static char buf[200];
    int	i;

    for ( i=0; i < 200 && cur && cur != c; i++ ) {
	buf[i] = cur;
	get();
    }
    if ( i == 200 ) {
	errorRomLineTooLong("graph.comp");
    }
    buf[i] = 0;
    return buf;
}

static void
get()
{
  if (done)
    return;
  cur = getchar();
  if ( cur == '#' ) {
      while ( cur != '\n' && cur != EOF ) {
	  cur = getchar();
      }
  }
  filePosition++;
  if (debug)
    putchar(cur);
  if (cur == EOF) {
    done = 1;
    cur = 0;
  }
  if (cur == '\n') {
    filePosition=0;
    fileLine++;
    cur = ' ';
    if (debug)
      putchar(cur);
  }
}

