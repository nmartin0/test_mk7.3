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
 *  Parser for the protocol table file.
 *
 *  Syntax (nonterminals capitalized):
 *
 * 	Entry -> Name  Id   OptionalList
 * 	OptionalList -> { L } | null
 * 	List -> Name Id List | null
 * 	Name -> string
 * 	Id -> integer
 *
 *  A '#' marks a comment until end of line
 */

#include <stdio.h>
#include <xkern/include/domain.h>
#include <xkern/include/xtype.h>
#include <xkern/include/xk_debug.h>
#include <xkern/include/romopt.h>
#include <xkern/include/compose.h>
#include <xkern/include/prottbl.h>

#define BUFSIZE 80


static void	entry( void );
static void	error( ErrorCode );
static int	forceChar( int );
static void	get( void );
static long	id( void );
static int	list( char * );
static char *	name( char * );
static void	optionalList( char * );
static xkern_return_t	ptblRomOpt( char **, int, int, void * );
static void	skipWhite( void );

static int	line = 1;
static int	position = 0;
static ErrorCode	errorCode;
static int	nextChar;
static FILE	*inputFile;
static char	*fileName;



/* 
 * Top-level parsing routine.
 */
ErrorCode
protTblParse(
    char	*f)
{
    fileName = f;
    errorCode = NO_ERROR;
    inputFile = fopen(fileName, "r");
    if ( inputFile == 0 ) {
	error(ERR_FILE);
	/* 
	 * An error return code will trigger 'parse error' messages
	 * which aren't really appropriate, so we'll just return
	 * success. 
	 */
    } else {
	get();
	while ( nextChar != EOF && ! errorCode ) {
	    entry();
	}
	fclose(inputFile);
    }
    return errorCode;
}


static void
get( void )
{
    do {
	nextChar = getc(inputFile);
	position++;
	if (nextChar == '#') {
	    while (nextChar != '\n' && nextChar != EOF) {
		nextChar = getc(inputFile);
		position++;
	    }
	}
	if (nextChar == '\n') {
	    line++;
	    position = 1;
	}
    } while ( nextChar == '#' || nextChar == '\n' );
}


static void
skipWhite( void )
{
    while ( isspace(nextChar) ) {
	get();
    }
}


/* 
 * consumes c from the input stream, consuming white space on either side.
 * Returns non-zero if c is not the next character.
 */
static int
forceChar(
    int 	c)
{
    skipWhite();
    if ( nextChar != c ) {
	return 1;
    }
    get();
    skipWhite();
    return 0;
}
  

static void
error(
    ErrorCode	code)
{
    char 	*errMsg = "UNKNOWN";

    switch ( code ) {
      case ERR_FILE:
	sprintf(errBuf, "could not open protocol table file %s", fileName);
	xError(errBuf);
	return;

      case ERR_NAME:
	errMsg = "expected protocol name";
	break;
    
      case ERR_ID:
	errMsg = "expected id (integer)";
	break;

      case ERR_NAME_TOO_LARGE:
	errMsg = "string exceeded buffer size";
	break;

      case NO_ERROR:
	return;
    }
    sprintf(errBuf, "prot ID parse error: %s at line %d, pos %d\n",
		   errMsg, line, position);
    xError(errBuf);
    errorCode = code;
}


/* 
 * Entry -> Name  Id   OptionalList
 */
static void
entry( void )
{
    char	protName[BUFSIZE];
    long	protId;

    xTrace0(ptbl, TR_DETAILED, "ptbl parse: entry called");
    if ( name(protName) == 0 ) {
	error(ERR_NAME);
	return;
    }
    if ( (protId = id()) == -1 ) {
	error(ERR_ID);
	return;
    }
    protTblAddProt(protName, protId);
    optionalList(protName);
    xTrace2(ptbl, TR_FUNCTIONAL_TRACE,
	"ptbl parse: entry == { name == %s, id == %d }", protName, protId);
}


static void
optionalList(
    char	*n)
{
    xTrace0(ptbl, TR_DETAILED, "ptbl parse: optionalList called");
    if ( nextChar == '{' ) {
	/* 
	 * XXX -- create maps
	 */
	if ( forceChar('{') ) {
	    return;
	}
	if ( list(n) ) {
	    return;
	}
	forceChar('}');
    }
}
  
  
/* 
 * List -> Name Id List | null
 *
 * Returns zero on successful parse, non-zero on error
 */
static int
list(
    char	*llpName)
{
    char	hlpName[BUFSIZE];
    long	hlpId;
    
    xTrace0(ptbl, TR_DETAILED, "ptbl parse: list called");
    if ( name(hlpName) == 0 ) {
	/* 
	 * null list
	 */
	return 0;
    }
    if ( (hlpId = id()) == -1 ) {
	error(ERR_ID);
	return 1;
    }
    protTblAddBinding(llpName, hlpName, hlpId);
    xTrace2(ptbl, TR_FUNCTIONAL_TRACE, "ptbl parse: list element %s -> %d", hlpName, hlpId);
    return list(llpName);
}


/* 
 * returns 0 if a name is not matched
 */
static char *
name(
    char	*buf)
{
    int		i;

    if ( ! isalpha(nextChar) ) {
	return 0;
    }
    i = 0;
    while ( nextChar && ! isspace(nextChar) ) {
	if ( i >= BUFSIZE ) {
	    error(ERR_NAME_TOO_LARGE);
	    return 0;
	}
	buf[i++] = nextChar;
	get();
    }
    skipWhite();
    buf[i] = 0;
    return buf;
}


/* 
 * Returns -1 (not a valid id) if an id is not matched
 */
static long
id( void )
{
    char	buf[80];
    int		i;
    long	n;
    int		res;

    if ( ! isdigit(nextChar) && nextChar != 'x' ) {
	return -1;
    }
    i = 0;
    /* 
     * Check for hex format
     */
    if ( nextChar == 'x' ) {
	buf[i++] = nextChar;
	get();
    }
    while ( nextChar && isdigit(nextChar) ) {
	if ( i >= sizeof(buf) ) {
	    error(ERR_NAME_TOO_LARGE);
	    return 0;
	}
	buf[i++] = nextChar;
	get();
    }
    skipWhite();
    buf[i] = 0;
    if ( buf[0] == 'x') {
	res = sscanf(buf + 1, "%x", &n);
    } else {
	res = sscanf(buf, "%d", &n);
    }
    if ( res != 1 ) {
	/* 
	 * The scan was botched.  We know this is an error, so we
	 * don't have to worry about pushing characters back.
	 */
	return -1;
    }
    return n;
}




