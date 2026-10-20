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
 * ptbldump.c
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * $Revision: 1.1.10.3 $
 * $Date: 1995/05/14 19:58:11 $
 */

/* 
 * Builds a protocol table with files listed on the command line and
 * then dumps C code which can rebuild the table.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <xkern/include/xtype.h>
#include <xkern/include/prottbl.h>
#include <xkern/include/romopt.h>

/* 
 * These are some bogus definitions to let us limit the number of
 * x-kernel files we have to link against. 
 */
struct xobj *protl_tab[1];
extern char *protIdToStr();

Allocator	xkSystemAllocator;

typedef struct path {
    int		id;
    Allocator   sysAlloc[2];
    Map         allocMap;       /* For protocol-specific allocators */
} Path_s;

static  Path_s  bsPath;
Path            xkSystemPath = &bsPath;

void 
xAssert( boolean_t ex )
{
    if (!ex)
	exit(1);
}

void *
allocGet( Allocator a, u_int size )
{
    return malloc(size);
}

void
xTraceLock()
{
}

void
xTraceUnlock()
{
}

void
allocFree( void *p )
{
    free(p);
}

xkern_return_t
findRomOpts(
    char        *str,
    RomOpt      *opt,
    int         numOpts,
    void        *arg)
{
}

char *	protocolTables[1] = { 0 };

#define DECL 0
#define MAPS 1

char 	*names[2] = { "/tmp/xkDeclsXXXXXX", "/tmp/xkMapsXXXXXX" };

static FILE	*declFile, *mapFile;


void
Kabort(
        char    *s)
{
}


static int
addMapPair(
    void	*key,
    int	 	value,
    void	*arg)
{
    long	hlpId = *(long *)key;
    char	*hlpName;
    
    if ( (hlpName = protIdToStr(hlpId)) == 0 ) {
	fprintf(stderr, "ptbldump: protocol table inconsistency!  Exiting\n");
	exit(1);
    }
    fprintf(mapFile, "\t{ \"%s\", %d },\n", hlpName, value);
    return 1;
}


static int
doEntry(
    void	*key,
    int	 	value,
    void	*arg)
{
    PtblEntry	*e = (PtblEntry *)value;

    /* 
     * decl file entry
     */
    fprintf(declFile, "\t{ \"%s\", %d, ", e->name, e->id);
    if ( e->idMap ) {
	fprintf(declFile, "%sMap",  e->name);
    } else {
	fprintf(declFile, "0");
    }
    fprintf(declFile, " },\n");
    /* 
     * map file entry
     */
    if ( e->idMap ) {
	fprintf(mapFile, "static MapEntry %sMap[] = { \n", e->name);
	mapForEach(e->idMap, (MapForEachFun)addMapPair, 0);
	fprintf(mapFile, "\t{ 0, 0 }\n");
	fprintf(mapFile, "};\n\n");
    }
    return 1;
}


static void
declPre()
{
    fprintf(declFile, "static Entry\tentries[] = {\n");
}


static void
declPost()
{
    fprintf(declFile, "\t{ 0, 0, 0 }\n");
    fprintf(declFile, "};\n");
}


static void
dumpTables()
{

    if ( mktemp(names[DECL]) == 0 ) {
	perror("ptbldump could not get tmp name");
	exit(1);
    }
    if ( mktemp(names[MAPS]) == 0 ) {
	perror("ptbldump could not get tmp name");
	exit(1);
    }
    if ( (declFile = fopen(names[DECL], "w")) == 0 ) {
	perror("ptbldump could not open temp file");
	exit(1);
    }
    if ( (mapFile = fopen(names[MAPS], "w")) == 0 ) {
	perror("ptbldump could not open temp file");
	exit(1);
    }
    declPre();
    mapForEach(ptblNameMap, (MapForEachFun)doEntry, 0);
    declPost();
    fclose(mapFile);
    fclose(declFile);
    if ( fork() == 0 ) {
	execl("/bin/cat", "/bin/cat", names[MAPS], names[DECL], 0);
    }
    wait(0);
    unlink(names[MAPS]);
    unlink(names[DECL]);
}


int
main(
    int 	argc,
    char	**argv)
{
    int	i;

    prottbl_init();
    if ( argc > 1 ) {
	for ( i=1; i < argc; i++ ) {
	    if ( protTblBuild(argv[i])) {
		fprintf(stderr, "ptbldump -- error building protocol table\n");
		exit(1);
	    }
	}
	dumpTables();
    } else {
	fprintf(stderr, "usage: ptbldump ptbl1 [ ptbl2 ... ]\n");
    }
    return 0;
}
