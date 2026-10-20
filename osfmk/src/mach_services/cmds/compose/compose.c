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
 * compose.c,v
 *
 * x-kernel v3.2
 *
 * Copyright (c) 1993,1991,1990  Arizona Board of Regents
 *
 *
 * compose.c,v
 * Revision 1.58.1.4.1.2  1994/09/07  04:18:50  menze
 * OSF modifications
 *
 * Revision 1.58.1.4  1994/09/01  04:05:26  menze
 * XObj initialization functions return xkern_return_t
 *
 * Revision 1.58.1.3  1994/07/22  20:00:50  menze
 * XObjects use Paths instead of Allocators
 *
 * Revision 1.58.1.2  1994/05/05  20:05:21  menze
 * ROM entries weren't always newline terminated
 *
 * Revision 1.58.1.1  1994/04/13  01:55:09  menze
 * Added support for 'alloc' field
 *
 * Revision 1.58  1994/04/07  23:21:37  menze
 *   [ 1994/03/08          menze ]
 *   added 'romopt' and 'romfile' entries, allowing rom files and options
 *   to be statically configured at compose/compile time
 *
 * Revision 1.57  1994/02/05  00:09:54  menze
 *   [ 1994/01/31          menze ]
 *   Locally compiled protocols retain external-library-requirements
 *   information.
 *
 *   [ 1994/01/24          menze ]
 *   added typecast
 *
 *   [ 1994/01/03          menze ]
 *   Changed order of include files.
 *   Fixed declaration of make_define to conform to its prototype.
 *
 * Revision 1.56  1993/12/13  19:00:35  menze
 * Modifications from UMass:
 *
 *   [ 93/08/12          nahum ]
 *   Fixed #endif emmitted by compose so traceLevel.c builds w/o warnings.
 *
 *   [ 93/06/09          nahum ]
 *   Added fixes to get utils to build under IRIX
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include "global.h"

/* 
 * System prototypes
 */
char *	mktemp( char * );

#define Array(type, size)  struct { type a[(size)]; int n; }

static Array(PROTOCOL, 200)	instance;
static Array(PROTOCOL *, 200) 	local_p;
static Array(PROTOCOL *, 200) 	unique_p;
static Array(TraceVar, 200)	traceVar;
static Array(char *, 50)	protTbl;   /* Names of protocol table files */
static Array(char *, 50)	localdir;  /* dirs containing local prots   */
static Array(char *, 50)	include;   /* dirs containing includes   */
static int 	nerrors = 0;
static int	fastFlag;
static int	makeFileFlag;
static char	*makeFileName = "protocols.sed";
static char	*simFileName;
static char *	libraries[20];
static int	last_driver = -1;
static FILE *	m_file;
static FILE *	h_file;
static FILE *	prot_file;
static FILE *	trace_file;
static FILE *	tbl_file;
static FILE *	tmp_file;
static FILE *	tmp_rom_file;
static FILE *	rom_file;
static char *	tmpFileName = "/tmp/composeNamesXXXXXX";
#define volatileWarning \
  " *\n * Warning: this file is generated from graph.comp and is overwritten\n * every time 'make compose' is run\n"


#define strsame(A,B) (!(strcmp((A),(B))))

static void	checkConsistency( void );
static int	find_index( ProtName * );
static void	gen( void );
static void	gen_code( FILE * );
static void	gen_defs( FILE * );
static void	gen_make( FILE * );
static void	gen_sed( FILE * );
static void	gen_sim_file( FILE * );
static void	gen_trace_file( FILE * );
static void	gen_tbl_file( FILE * );
static void	gen_rom_file( FILE * );
static int	genProtlCode( FILE *, PROTOCOL * );
static void	initTmpFile( void );
static int	localCompilation( PROTOCOL * );
static char*	make_define( char * );
#if 0
static void	print_protocol( PROTOCOL * );
#endif
static char*	suffix( char * );
static void	addOptions( char *, char *, char *, TraceVar * );

static void
usage()
{
    fprintf(stderr, "usage: compose [-h] [-f] [-m mfile] [-s sFile] [library ...]\n");
}


void
main(
    int		argc,
    char	**argv )
{
    extern	char *	optarg;
    extern	int	optind;
    signed char	c;
    int 	i=0; 
    
    while ((c = getopt(argc, argv, "fm:hs:")) != -1 ) {
      switch (c) {
	case 'f':
	  fastFlag = 1;
	  break;
	case 'm':
	  makeFileFlag = 1;
	  makeFileName = optarg;
	  break;
	case 's':
	  simFileName = optarg;
	  break;
	case '?':
	case 'h':
	  usage();
	  exit(1);
      }
    }
    for (; optind < argc; optind++) {
	libraries[i++] = argv[optind];
    }

    libraries[i] = (char *)0;
    parse();
    checkConsistency();
    gen();
    exit(0);
}


static FILE *
createFile(
    char	*name )
{
    FILE *	f;

    if ( ! strcmp(name, "-") ) {
	return stdout;
    }
    unlink(name);
    f = fopen(name, "w");
    if (f == NULL) {
	errorFile(name);
    }
    return f;
}


static void
initTmpFile()
{
    char	cmd[200];
    int		i;

    for ( i=0; libraries[i]; i++ ) {
#ifdef OSF
	sprintf(cmd, "nm -e -p %s | grep _init$ >> %s",
		libraries[i], tmpFileName);
#else
	sprintf(cmd, "nm -g %s | grep '_init$' >> %s",
		libraries[i], tmpFileName);
#endif
	system(cmd);
    }
}


static void
gen()
{
    FILE	*s_file = 0;

    m_file = createFile(makeFileName);
    h_file = createFile("protocols.h");
    prot_file = createFile("protocols.c");
    tbl_file = createFile("protTbl.c");
    trace_file = createFile("traceLevels.c");
    rom_file = createFile("compose_rom");
    mktemp(tmpFileName);
    if ( ! fastFlag ) {
	tmp_file = createFile(tmpFileName);
	if ( ! tmp_file ) {
	    exit (1);
	}
    }
    if ( simFileName ) {
	s_file = createFile(simFileName);
    }
    if ( ! ( m_file && h_file && prot_file && tbl_file && trace_file &&
	     rom_file && ((! simFileName) || s_file) ) ) {
	exit (1);
    }
    
    if ( ! fastFlag ) {
	initTmpFile();
    }
    if ( makeFileFlag ) {
	gen_make(m_file);
    } else {
	gen_sed(m_file);
    }
    fclose(m_file);
    gen_defs(h_file);
    fclose(h_file);
    gen_code(prot_file);
    fclose(prot_file);
    gen_tbl_file(tbl_file);
    fclose(tbl_file);
    gen_trace_file(trace_file);
    fclose(trace_file);
    gen_rom_file(rom_file);
    fclose(rom_file);
    if ( simFileName ) {
	gen_sim_file(s_file);
	fclose(s_file);
    }
    if ( ! fastFlag ) {
	unlink(tmpFileName);
    }
}


static char *
suffix(
    char	*s)
{
    return strsame(&s[strlen(s) - 2], "_s") ? "S" : "c";
}


/*
 * localCompilation -- for protocols with locally specified compilation,
 * checks to see if the files exist and can be accessed, issuing
 * warnings  if they can not.
 *
 * 	RETURNS:
 * 		1 if the protocol specifies local compilation
 *		0 if it does not.
 */
static int
localCompilation(
    PROTOCOL	*p )
{
    char buffer[512], *dir;
    int i;
    
    if ( ! p->path && p->numfiles == 0 ) {
	/* 
	 * Neither files nor path specified, not local compilation
	 */
	return 0;
    }
    /* 
     * This is a locally compiled protocol.
     */
    if (! p->numfiles) {
	p->files[0] = p->n.name;
	p->numfiles = 1;
    }
    if (! p->path) {
	p->path = ".";
    }
    /* 
     * Append a trailing '/' to the 'path' name
     */
    dir = (char *)malloc(strlen(p->path)+2);
    sprintf(dir, "%s/", p->path);
    p->path = dir;
    if (! opendir(p->path)) {
	warnCouldNotAccess(p->path);
    } else {
	for (i=0; i < p->numfiles; i++) {
	    sprintf(buffer, "%s%s.%s", p->path, p->files[i],
		    suffix(p->files[i]));
	    if (access(buffer, R_OK)) {
		warnCouldNotAccess(buffer);
	    }
	}
    }
    return 1;
}


/* 
 * mkLocalProts -- Find out which protocols are to be compiled
 * "locally" (in the user's build directory).  Add an entry to local_p
 * for each of these protocols (but multiple instances have only one
 * entry.)  Also add an entry to localdirs for each local directory in
 * which protocols will be compiled.
 */
static void
mkLocalProts()
{
    int	i, j;
    char cmd[200];

    /*
     * Current directory should always be in the "localdir" list so a
     * compilation rule will be generated for it since
     * some files are compiled there (localinit, protocols) even if no
     * user-specified files are.
     */
    localdir.a[localdir.n++] = "./";
    for (i = 0; i < instance.n; i++) {
	PROTOCOL *p = &instance.a[i];

	if (localCompilation(p)) {
	    /* 
	     * See if this protocol already has a local_p entry
	     */
	    for ( j=0; j < local_p.n; j++ ) {
		if ( strsame(local_p.a[j]->n.name, p->n.name) ) {
		    break;
		}
	    }
	    if ( j != local_p.n ) {
		continue;
	    }
	    local_p.a[local_p.n++] = p;
	    /* 
	     * See if this protocol's local directory is already in
	     * the localdir array.
	     */
	    for (j=0; j < localdir.n; j++) {
		if (strsame(localdir.a[j], p->path)) {
		    break;
		}
	    }
	    if (j == localdir.n) {
		localdir.a[localdir.n++] = p->path;
	    }
	} else {
	    /*
	     * look for protocol init routine in the libraries and warn
	     * if it is not found
	     */
	    if ( ! fastFlag ) {
		for (j=0; libraries[j]; j++) {
		    sprintf(cmd, "grep '%s_init$' %s > /dev/null",
			    p->n.name, tmpFileName);
		    if (! system(cmd)) 
		      break;
		}
		if (! libraries[j]) {
		    warnProtNotFound(p->n.name); 
		}
	    }
	}
    }
}


/* 
 * Make sure all other protocols referenced by a protocol are defined.
 * Also sets the 'index' field of the ProtName structures in the
 * 'down' field of each protocol, as well as the 'index' field of the
 * protocol itself.
 */
static void
checkConsistency()
{
    int i, j;
    int di;
    
    for (i = 0; i < instance.n; i++) {
	for (j = 0; j < instance.a[i].numdown; j++) {
	    di = find_index(&instance.a[i].down[j]);
	    if (di == -1) {
		errorProtlUndefined(instance.a[i].down[j].name,
				    instance.a[i].n.name);
		nerrors++;
	    }
	    instance.a[i].down[j].index = di;
	}
	instance.a[i].n.index = i;
    }
    if (nerrors) exit(1);
}



static void
gen_sed(
    FILE	*fptr)
{
    int i, j;
    PROTOCOL	*p;
    
    mkLocalProts();
    /*
     * Add dependencies for locally compiled files
     */
    fprintf(fptr, "/^# PCDEFINITIONS BEGIN HERE/,/^# PCDEFINITIONS END HERE/c\\\n");
    fprintf(fptr, "# PCDEFINITIONS BEGIN HERE\\\n");
    fprintf(fptr, "# Don't edit these lines, compose uses them\\\n");
    fprintf(fptr, "PCDOTOS = ");
    for (i = 0; i < local_p.n; i++) {
	p = local_p.a[i];
	if (p->numfiles) {
	    for (j = 0; j < p->numfiles; j++) {
		fprintf(fptr, "%s${HOW}/%s.o \\\\\\\n", p->path, p->files[j]);
	    }
	} else {
	    fprintf(fptr, "%s${HOW}/%s.o \\\\\\\n", p->path, p->n.name);
	}
    }
    fprintf(fptr, "\\\n");
    fprintf(fptr, "PCDOTCS = ");
    for (i = 0; i < local_p.n; i++) {
	p = local_p.a[i];
	if (p->numfiles) {
	    for (j = 0; j < p->numfiles; j++) {
		if (suffix(p->files[j])[0] == 'c') {
		    fprintf(fptr, "%s%s.c \\\\\\\n", p->path, p->files[j]);
		}
	    }
	} else {
	    if (suffix(p->n.name)[0] == 'c') {
		fprintf(fptr, "%s%s.c \\\\\\\n", p->path, p->n.name);
	    }
	}
    }
    fprintf(fptr, "\\\n");
    fprintf(fptr, "PCDOTSS = ");
    for (i = 0; i < local_p.n; i++) {
	p = local_p.a[i];
	if (p->numfiles) {
	    for (j = 0; j < p->numfiles; j++) {
		if (suffix(p->files[j])[0] == 'S') {
		    fprintf(fptr, "%s%s.S ", p->path, p->files[j]);
		}
	    }
	} else {
	    if (suffix(local_p.a[i]->n.name)[0] == 'S') {
		fprintf(fptr, "%s%s.S ", p->path, p->n.name);
	    }
	}
    }
    fprintf(fptr, "\\\n");
    fprintf(fptr, "# PCDEFINITIONS END HERE\n");
    
    /*
     * One implicit rule per local directory
     */
    fprintf(fptr, "/^# IMPLICIT RULES BEGIN HERE/,/^# IMPLICIT RULES END HERE/c\\\n");
    fprintf(fptr, "# IMPLICIT RULES BEGIN HERE\\\n");
    fprintf(fptr, "# Don't edit these lines, compose uses them\\\n\\\n");
    for (i = 0; i < localdir.n; i++) {
	fprintf(fptr, "%s$(HOW)/%%.o : %s%%.c\\\n",
		localdir.a[i], localdir.a[i]);
	fprintf(fptr, "\\\t$(DEFAULTRULE)\\\n\\\n");
    }
    fprintf(fptr, "# IMPLICIT RULES END HERE\n");
}


static void
gen_make(
    FILE	*fptr)
{
    int i, j;
    PROTOCOL	*p;
    char	libObjStr[2048];
    char	extLibStr[2048];
    
    mkLocalProts();
    /*
     * Add dependencies for locally compiled files
     */
    fprintf(fptr, "PCDOTOS = ");
    for (i = 0; i < local_p.n; i++) {
	p = local_p.a[i];
	if (p->numfiles) {
	    for (j = 0; j < p->numfiles; j++) {
		fprintf(fptr, "%s${HOW}/%s.o \\\n", p->path, p->files[j]);
	    }
	} else {
	    fprintf(fptr, "%s${HOW}/%s.o \\\n", p->path, p->n.name);
	}
    }
    fprintf(fptr, "\n");
    fprintf(fptr, "PCDOTCS = ");
    for (i = 0; i < local_p.n; i++) {
	p = local_p.a[i];
	if (p->numfiles) {
	    for (j = 0; j < p->numfiles; j++) {
		if (suffix(p->files[j])[0] == 'c') {
		    fprintf(fptr, "%s%s.c \\\n", p->path, p->files[j]);
		}
	    }
	} else {
	    if (suffix(p->n.name)[0] == 'c') {
		fprintf(fptr, "%s%s.c \\\n", p->path, p->n.name);
	    }
	}
    }
    fprintf(fptr, "\n");
    fprintf(fptr, "PCDOTSS = ");
    for (i = 0; i < local_p.n; i++) {
	p = local_p.a[i];
	if (p->numfiles) {
	    for (j = 0; j < p->numfiles; j++) {
		if (suffix(p->files[j])[0] == 'S') {
		    fprintf(fptr, "%s%s.S ", p->path, p->files[j]);
		}
	    }
	} else {
	    if (suffix(local_p.a[i]->n.name)[0] == 'S') {
		fprintf(fptr, "%s%s.S ", p->path, p->n.name);
	    }
	}
    }
    fprintf(fptr, "\n\n");
    
    /* 
     * Create inclusion flags for local directories
     */
    fprintf(fptr, "COMPOSE_INCLUDES = ");
    for (i = 0; i < localdir.n; i++) {
	char	dir[1024];

	/* 
	 * The local directory is stored with a trailing '/' which we
	 * need to strip off.
	 */
	strcpy(dir, localdir.a[i]);
	dir[strlen(dir) - 1] = 0;
	fprintf(fptr, "-I%s ", dir);
    }
    fprintf(fptr, "\n\n");

    /*
     * One implicit rule per local directory
     */
    for (i = 0; i < localdir.n; i++) {
	fprintf(fptr, "%s$(HOW)/%%.o : %s%%.c\n",
		localdir.a[i], localdir.a[i]);
	fprintf(fptr, "\t$(DEFAULTRULE)\n\n");
    }
    /* 
     * Generate Makefile directives to allow linkking with
     * object files directly, rather than using libraries.
     */
    fprintf(fptr, "COMPOSE_LIB_PROTS := ");
    *libObjStr = 0;
    *extLibStr = 0;
    for ( i=0; i < unique_p.n; i++ ) {
	char	tmpStr[100];

	p = unique_p.a[i];
	if ( ! p->path ) {
	    
	    fprintf(fptr, "+%s+",p->n.name);
	    sprintf(tmpStr, " $(%s_FILES)", p->n.name);
	    strcat(libObjStr, tmpStr);
	}
	sprintf(tmpStr, " $(%s_LIBS)", p->n.name);
	strcat(extLibStr, tmpStr);
    }
    fprintf(fptr, "\n\n");
    fprintf(fptr, "COMPOSE_LIB_OBJ = %s\n\n", libObjStr);
    fprintf(fptr, "COMPOSE_EXT_LIBS = %s\n\n", extLibStr);
}


static int
find_index(
    ProtName	*pName )
{
    int 	i;
    ProtName	*instName;

    for (i = 0; i < instance.n; i++) {
	instName = &instance.a[i].n;
	if ( strsame(pName->name, instName->name) &&
	     strsame(pName->instance, instName->instance) ) {
	    return i;
	}
    }
    return -1;
}


static void
end_driver_build(
    FILE	*fptr)
{
  static int done = 0;
  if (! done) {
    fprintf(fptr, "}\n\n");
    fprintf(fptr, "\nvoid\nbuild_pgraph()\n");
    fprintf(fptr, "{\n");
    fprintf(fptr, "  Path path;\n");
    fprintf(fptr, "  unsigned n;\n");
    done = 1;
  }
}


static int
genProtlCode(
    FILE	*fptr,
    PROTOCOL	*p)
{
    PROTOCOL	*llp;
    int		i;
    int		dynamic;

    if ( p->initState == INIT_DONE ) {
	return 0;
    }
    if ( p->initState == INIT_WORKING ) {
	/* 
	 * We have a cycle.
	 */
	errorCycle();
	errorCycleName(p->n.name);
	return -1;
    }
    assert(p->initState == INIT_NOT_DONE);
    p->initState = INIT_WORKING;
    for ( i = 0; i < p->numdown; i++ ) {
	llp = &instance.a[p->down[i].index];
	if ( genProtlCode(fptr, llp) ) {
	    errorCycleName(p->n.name);
	    return -1;
	}
    }
    fprintf(fptr, "\n");
    if (p->tv->ifdef)
	fprintf(fptr, p->tv->ifdef);
    fprintf(fptr, "  /*\n   *  building protocol %s\n   */\n",
	    make_define(p->n.name));
    dynamic = -1;
    for ( i = 0; i < p->numdown; i++ ) {
	if (instance.a[p->down[i].index].tv->ifdef) {
	    dynamic = i;
	    break;
	}
    }
    for ( i = 0; i < p->numdown; i++ ) {
	if ( dynamic < 0 || i < dynamic )
	    fprintf(fptr, "  argv[%d] = protl_tab[%d];\n", i, p->down[i].index);
	else {
	    if ( i == dynamic )
		fprintf(fptr, "  n = %d;\n", dynamic);
	    if ( instance.a[p->down[i].index].tv->ifdef )
		fprintf(fptr, instance.a[p->down[i].index].tv->ifdef);
	    fprintf(fptr, "  argv[n++] = protl_tab[%d];\n", p->down[i].index);
	    if ( instance.a[p->down[i].index].tv->endif )
		fprintf(fptr, instance.a[p->down[i].index].tv->endif);
	}
    }
    fprintf(fptr, "  path = getPathByName(\"%s\");\n",
	    p->x_path ? p->x_path : "");
    if ( p->x_path ) {
	fprintf(fptr, "  if ( ! path ) {\n");
	fprintf(fptr, "      xError(\"Could not find path '%s', using default\");\n",
		p->x_path);
	fprintf(fptr, "  }\n");
    }
    fprintf(fptr, "  protl_tab[%d] = xCreateProtl(%s_init, \"%s\", \"%s\",",
	    p->n.index, p->n.name, p->n.name, p->n.instance);
    fprintf(fptr, "\n#if XK_DEBUG\n\t\t\t      &%s,\n#else /* XK_DEBUG */\n\t\t\t      0,\n#endif /* XK_DEBUG */\n",
	    p->tv->name);
    if ( dynamic < 0 )
	fprintf(fptr, "\t\t\t      %d, argv, path);\n", p->numdown);
    else
	fprintf(fptr, "\t\t\t      n, argv, path);\n");
    fprintf(fptr, "  if ( protl_tab[%d] == ERR_XOBJ ) {\n", p->n.index);
    fprintf(fptr, "      Kabort(\"Could not create %s protocol\");\n",
	    p->n.name);
    fprintf(fptr, "  }\n");
    if (p->tv->ielse) {
	fprintf(fptr, p->tv->ielse);
	fprintf(fptr, "  protl_tab[%d] = ERR_XOBJ;\n", p->n.index);	
    }
    if (p->tv->endif)
	fprintf(fptr, p->tv->endif);
    fprintf(fptr, "\n");
    p->initState = INIT_DONE;
    return 0;
}


static void
gen_code(
    FILE	*fptr)
{
    int i;
    
    fprintf(fptr, "/*\n * protocol initialization calls\n");
    fprintf(fptr, volatileWarning);
    fprintf(fptr, " */\n\n");
    for ( i=0; i < include.n; i++)
	fprintf(fptr, include.a[i]);
    fprintf(fptr, "\n");
    fprintf(fptr, "#include <xkern/include/xkernel.h>\n");
    fprintf(fptr, "#include <xkern/include/upi.h>\n");
    fprintf(fptr, "#include <xkern/include/compose.h>\n");
    fprintf(fptr, "#ifdef MACH_KERNEL\n");
    fprintf(fptr, "#include <uk_xkern/protocols.h>\n");
    fprintf(fptr, "#else  /* MACH_KERNEL */\n");
    fprintf(fptr, "#include <s_xkern/protocols.h>\n");
    fprintf(fptr, "#endif /* MACH_KERNEL */\n");
    fprintf(fptr, "\n\n");
    fprintf(fptr, "XObj\t\tprotl_tab[%d+1];\n", instance.n);
    fprintf(fptr, "static XObj\targv[%d];\n", instance.n);
    fprintf(fptr, "\n\nvoid\nbuild_pgraph_dev()\n");
    fprintf(fptr, "{\n");
    
    for (i = 0; i < instance.n; i++) {
	PROTOCOL	*p = &instance.a[i];
	if (i == (last_driver + 1)) {
	    end_driver_build(fptr);
	}
	if ( genProtlCode(fptr, p) ) {
	    finishErrorCycle();
	}
    }
    end_driver_build(fptr);
    fprintf(fptr, "}\n");
}


static void
gen_defs(
    FILE	*fptr )
{
    int i;
    
    fprintf(fptr, "/*\n * trace variable and init function declarations\n");
    fprintf(fptr, volatileWarning);
    fprintf(fptr, " */\n\n");
    for ( i=0; i < include.n; i++)
	fprintf(fptr, include.a[i]);
    fprintf(fptr, "#include <xkern/include/upi.h>\n\n");
    for ( i=0; i < traceVar.n; i++ ) {
	if (traceVar.a[i].ifdef)
	    fprintf(fptr, traceVar.a[i].ifdef);
	fprintf(fptr, "extern int %s;\n", traceVar.a[i].name);
	if (traceVar.a[i].endif)
	    fprintf(fptr, traceVar.a[i].endif);
    }
    for ( i=0; i < unique_p.n; i++ ) {
	char *name = unique_p.a[i]->n.name;
	if (unique_p.a[i]->tv->ifdef)
	    fprintf(fptr, unique_p.a[i]->tv->ifdef);
	fprintf(fptr, "XObjInitFunc %s_init;\n", name);
	if (unique_p.a[i]->tv->endif)
	    fprintf(fptr, unique_p.a[i]->tv->endif);
    }
}


static void
gen_sim_file(
    FILE	*fptr )
{
    int i;
    PROTOCOL *p;
    
    for ( i=0; i < unique_p.n; i++ ) {
	p = unique_p.a[i];
	fprintf(fptr, "{\"%s\", %s_init, &%s},\n",
		p->n.name, p->n.name, p->tv->name);
    }
    fprintf(fptr, "{0, 0, 0}\n");
}


char *
make_define(
    char	*cptr)
{
    char buf[200];
    int i;

    for (i = 0; i < strlen(cptr); i++) {
	if (islower(cptr[i])) {
	    buf[i] = toupper(cptr[i]);
	} else {
	    buf[i] = cptr[i];
	}
    }
    buf[i] = '\0';
    return (xerox(buf));
}

#if 0

static void
print_protocol(
    PROTOCOL	*p)
{
    int i;

    printf("name = %s\n", p->n.name);
    printf("numfiles = %d\n", p->numfiles);
    for (i = 0; i < p->numfiles; i++) {
	printf("file[%d] = %s\n", i, p->files[i]);
    }
    printf("numdown = %d\n", p->numdown);
    for (i = 0; i < p->numdown; i++) {
	printf("down[%d] = %s\n", i, p->down[i]);
    }
}

#endif


void
addProtTbl(
    char	*name )
{
    char buf[256];

    assert(strlen(name) < 254);
    if ( strsame(name, "DEFAULT") ) {
	warnDefaultPtblNotSupported();
    }
    sprintf(buf, "\"%s\"", name);
    name = xerox(buf);
    protTbl.a[protTbl.n++] = name;
}


#define BUFLEN 200

void
addRomFile( char *file )
{
    FILE	*rf;
    char	buf[BUFLEN];

    rf = fopen(file, "r");
    if (rf == NULL) {
	errorFile(file);
	return;
    }
    buf[BUFLEN - 2] = 0;
    while ( fgets(buf, BUFLEN, rf) ) {
	if ( buf[BUFLEN - 2] ) {
	    /* 
	     * fgets might have thrown something away
	     */
	    errorRomLineTooLong(file);
	} else {
	    addRomOption(buf);
	}
    }
}    


void
addRomOption( char *opt )
{
    if ( ! tmp_rom_file ) {
	tmp_rom_file = tmpfile();
	if ( ! tmp_rom_file ) {
	    exit(1);
	}
    }
    /* 
     * Make sure there's a newline here
     */
    fprintf(tmp_rom_file, opt);
    if ( opt[strlen(opt) - 1] != '\n' ) {
	fprintf(tmp_rom_file, "\n");
    }
}


static void
addOptions(
    char *str,
    char *prefix,
    char *suffix,
    TraceVar *tv )
{
    unsigned int j;
    unsigned int len;
    unsigned int buflen;
    char *buf;
    char *p, *pp, *q;
    char c;

    buflen = 0;
    p = str;
    for (;;) {
	while ( *p == ' ' || *p == '\t' ) {
		p++;
	}
	if ( *p == '\0' ) {
	    break;
	}
	pp = p;
	while ( *p != ',' && *p != '\0' ) {
	    p++;
	}
	c = *p;
	*p = '\0';

	/*
	 * Work out include statement
	 */
	len = strlen("#include \".h\"\n") + strlen(pp) + 1;
	if (buflen < len) {
	    if (buflen > 0)
		free(buf);
	    buf = (char *)malloc(len);
	    buflen = len;
	}
	strcpy(buf, "#include \"");
	strcat(buf, pp);
	strcat(buf, ".h\"\n");
	for ( j=0;  j < include.n; j++ ) {
	    if (strsame(include.a[j], buf)) {
		break;
	    }
	}
	if (j == include.n) {
	    include.a[j] = xerox(buf);
	    include.n++;
	}

	/*
	 * Work out ifdef/endif statement
	 */
	q = buf;
	for (p = pp; *p != '\0'; p++) {
	    if (islower(*p)) {
		*q++ = toupper(*p);
	    } else {
		*q++ = *p;
	    }
	}
	*q = '\0';

	if (tv->ifdef == (char *)0) {
	    len = strlen("#if \n") +
		strlen(prefix) + strlen(pp) + strlen(suffix) + 1;
	    tv->ifdef = (char *)malloc(len);
	    strcpy(tv->ifdef, "#if ");
	} else {
	    len = strlen(tv->ifdef) + strlen (" && ") +
		strlen(prefix) + strlen(pp) + strlen(suffix) + 1;
	    tv->ifdef = (char *)realloc(tv->ifdef, len);
	    strcat(tv->ifdef, " && ");
	}

	if (tv->ielse == (char *)0) {
	    len = strlen("#else /*  */\n") +
		strlen(prefix) + strlen(pp) + strlen(suffix) + 1;
	    tv->ielse = (char *)malloc(len);
	    strcpy(tv->ielse, "#else /* ");
	} else {
	    len = strlen(tv->ielse) + strlen (" */") + strlen (" && ") +
		strlen(prefix) + strlen(pp) + strlen(suffix) + 1;
	    tv->ielse = (char *)realloc(tv->ielse, len);
	    strcat(tv->ielse, " && ");
	}


	if (tv->endif == (char *)0) {
	    len = strlen("#endif /*  */\n") +
		strlen(prefix) + strlen(pp) + strlen(suffix) + 1;
	    tv->endif = (char *)malloc(len);
	    strcpy(tv->endif, "#endif /* ");
	} else {
	    len = strlen(tv->endif) + strlen (" */") + strlen (" && ") +
		strlen(prefix) + strlen(pp) + strlen(suffix) + 1;
	    tv->endif = (char *)realloc(tv->endif, len);
	    strcat(tv->endif, " && ");
	}

	strcat(tv->ifdef, prefix);
	strcat(tv->ifdef, buf);
	strcat(tv->ifdef, suffix);

	strcat(tv->ielse, prefix);
	strcat(tv->ielse, buf);
	strcat(tv->ielse, suffix);
		
	strcat(tv->endif, prefix);
	strcat(tv->endif, buf);
	strcat(tv->endif, suffix);

	if (c == '\0') {
	    break;
	}
	*p++ = c;
    }

    if (buflen > 0)
	free(buf);
}


TraceVar *
addTraceVar(
    char *name,
    char *value,
    int instantiate,
    char *device,
    char *options )
{
    int	i;
    /* 
     * Make sure there are no previous declarations of this trace variable
     * This can happen for multiple protocol instantiations and isn't really
     * an error.
     */
    for ( i=0; i < traceVar.n; i++ ) {
	if ( strsame(traceVar.a[i].name, name) ) {
	    if ( value ) {
		if ( traceVar.a[i].value ) {
		    warnReassignedTraceValue(name);
		} else {
		    traceVar.a[i].value = value;
		}
	    }
	    return &traceVar.a[i];
	}
    }
    traceVar.a[i].name = xerox(name);
    traceVar.a[i].value = value;
    traceVar.a[i].instantiate = instantiate;

    /*
     * Work out conditional statements
     */
    traceVar.a[i].ifdef = (char *)0;
    traceVar.a[i].ielse = (char *)0;
    traceVar.a[i].endif = (char *)0;
    if (device != (char *)0)
	addOptions(device, "N", " > 0", &traceVar.a[i]);
    if (options != (char *)0)
	addOptions(options, "", " > 0", &traceVar.a[i]);

    if (traceVar.a[i].ifdef != (char *)0) {
	strcat(traceVar.a[i].ifdef, "\n");
    }
    if (traceVar.a[i].ielse != (char *)0) {
	strcat(traceVar.a[i].ielse, " */\n");
    }
    if (traceVar.a[i].endif != (char *)0) {
	strcat(traceVar.a[i].endif, " */\n");
    }
    traceVar.n++;
    return &traceVar.a[i];
}


void
addInstance(
    PROTOCOL	*p )
{
    int	i;

    /* 
     * If there are other previous instances of this protocol, make sure
     * this one doesn't have a path or files listing
     */
    for ( i=0; i < instance.n; i++ ) {
	if ( strsame(instance.a[i].n.name, p->n.name) ) {
	    /* 
	     * Previous instance exists
	     */
	    if ( p->path || p->numfiles ) {
		errorLaterInstanceFiles(p);
	    }
	    break;
	}
    }
    p->initState = INIT_NOT_DONE;
    instance.a[instance.n] = *p;
    if ( i == instance.n ) {
	/* 
	 * No previous instance exists, add this protocol to 'unique_p'
	 */
	unique_p.a[unique_p.n++] = &instance.a[instance.n];
    }
    instance.n++;
}


void
lastDriver()
{
    int	i;

    last_driver = instance.n - 1;
    for ( i = last_driver; i >= 0; i-- ) {
	instance.a[i].isDriver = 1;
    }
}


/* 
 * Generate a file which defines the array of protocol tables to be loaded 
 */
static void
gen_tbl_file(
    FILE	*f )
{
    int	i;

    fprintf(f, "/*\n * user-defined protocol tables\n");
    fprintf(f, volatileWarning);
    fprintf(f, " */\n\n");
    fprintf(f, "#include <xkern/include/compose.h>\n\n");
    fprintf(f, "char *\nprotocolTables[%d] = {\n", protTbl.n + 1);
    if ( protTbl.n > 0 ) {
	for ( i = 0; i < protTbl.n; i++ ) {
	    fprintf(f, "    %s,\n", protTbl.a[i]);
	}
    }
    fprintf(f, "    0\n");
    fprintf(f, "};\n");
}


static void
gen_rom_file( FILE *f )
{
    int		i;
    char	buf[200];

    fprintf(f, "# ROM entries from graph.comp and rom file\n");
    fprintf(f, "# This file is automatically generated\n\n");
    
    if ( tmp_rom_file ) {
	rewind(tmp_rom_file);
	while (fgets(buf, sizeof(buf), tmp_rom_file)) {
	    char *p;
	   
	    buf[strlen(buf)-1]=0; /* clear the newline */
	    /* skip blanks & C. */
	    for (p = buf; p < &buf[200] && *p; p++) {
		if (*p != ' ' && *p != '\t' && *p != '\n')
		    break;
	    }
	    if (*p == '#')        /* skip comments */
		continue;
	    
	    if ( strlen(buf) ) {
		fprintf(f, "%s\n", p);
	    }
	}
    }
}


/* 
 * Generate a file which initializes the trace variables
 */
static void
gen_trace_file(
    FILE	*f )
{
    int	i;
    TraceVar	*v;

    fprintf(f, "/*\n * trace variable initialization\n");
    fprintf(f, volatileWarning);
    fprintf(f, " */\n\n");
    for ( i=0; i < include.n; i++)
	fprintf(f, include.a[i]);
    fprintf(f, "\n");
    fprintf(f, "#include <xkern/include/xkernel.h>\n");
    fprintf(f, "#include <xkern/include/xk_debug.h>\n");
    fprintf(f, "#include <xkern/include/compose.h>\n");
    fprintf(f, "#ifdef MACH_KERNEL\n");
    fprintf(f, "#include <uk_xkern/protocols.h>\n");
    fprintf(f, "#else  /* MACH_KERNEL */\n");
    fprintf(f, "#include <s_xkern/protocols.h>\n");
    fprintf(f, "#endif /* MACH_KERNEL */\n");
    fprintf(f, "\n");
    for ( i=0; i < traceVar.n; i++ ) {
	if ( traceVar.a[i].instantiate ) {
	    if (traceVar.a[i].ifdef)
		fprintf(f, traceVar.a[i].ifdef);
	    fprintf(f, "int	%s;\n", traceVar.a[i].name);
	    if (traceVar.a[i].endif)
		fprintf(f, traceVar.a[i].endif);
	}
    }
    fprintf(f, "\n");
    fprintf(f, "void\ninitTraceLevels()\n{\n");
    fprintf(f, "#if XK_DEBUG\n\n");
    for ( i = 0; i < traceVar.n; i++ ) {
	v = &traceVar.a[i];
	if ( v->value ) {
	    if (v->ifdef)
		fprintf(f, v->ifdef);
	    fprintf(f, "    %s = %s;\n", v->name, v->value);
	    if (v->endif)
		fprintf(f, v->endif);
	}
    }
    fprintf(f, "\n#endif /* XK_DEBUG */\n");
    fprintf(f, "}\n");
}


