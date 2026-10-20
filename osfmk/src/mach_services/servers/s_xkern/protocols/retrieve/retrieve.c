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

#include <xkern/include/xkernel.h>
#include <xkern/include/part.h>
#include <xkern/protocols/proxy/xk_mig_sizes.h>

#if XK_DEBUG
int traceretrievep;
#endif /* XK_DEBUG */

/*
 * This simple protocol has the simple mission of retrieving
 * data from a cooperating layer of the protocol graph (possibly
 * in another protection domain) and write it to a file
 * (to be processed with typical UNIX commands, i.e. sort, uniq,
 * awk, etc).
 */


#define RETRIEVE_DUMP_FILE  "/tmp/retrieve"
#define RETRIEVE_DUMP_SIZE  (XK_MAX_CTL_BUF_LEN - 200)
#define RETRIEVE_DUMP_TIMEOUT 100000 

static char filename[200];
static unsigned long msg_size;
static unsigned long timeout;
FILE *logfile;

/* 
 * instName dump_file filename
 */
static xkern_return_t
readFile( XObj self, char **str, int nFields, int line, void *arg )
{
    strcpy(filename, str[2]); 
    return XK_SUCCESS;
}

/* 
 * instName dump_size N
 */
static xkern_return_t
readSize( XObj self, char **str, int nFields, int line, void *arg )
{
    return sscanf(str[2], "%d", &msg_size) < 1 ? XK_FAILURE : XK_SUCCESS;
}

/* 
 * instName dump_timeout N
 */
static xkern_return_t
readTim( XObj self, char **str, int nFields, int line, void *arg )
{
    return sscanf(str[2], "%d", &timeout) < 1 ? XK_FAILURE : XK_SUCCESS;
}

static XObjRomOpt	rpcOpts[] = {
    { "dump_file", 3, readFile },
    { "dump_size", 3, readSize },
    { "dump_timeout", 3, readTim }
};

void
do_retrieve(
	    Event ev,
	    void *arg)  /* a pointer to two generic longs */
{
    XObj self = (XObj)arg;
    xkern_return_t xkr;
    long *sample;
    char *begin;
    char *buffer = (char *)self->state;
    int i;

    xkr = xControl(xGetDown(self, 0), (10103<<16), (char *)buffer, RETRIEVE_DUMP_SIZE);
    xAssert(xkr <= RETRIEVE_DUMP_SIZE);
    if (xkr > 0) {
	for (i = 0, begin = buffer; i < xkr; i += (2*sizeof(long)), begin += (2*sizeof(long))) {
	    sample = (long *)begin;
	    fprintf(logfile, "%d  %d\n", *sample, *(sample+1));
	}
	fflush(logfile);
    } else {
	if (xkr < 0) {
	    xError("Error from the lower layers!\n");
	}
    }

    evSchedule(ev, do_retrieve, self, timeout);
}

xkern_return_t
retrieve_init(
	      XObj  self)
{
    char *buffer;
    int i;
    Event ev;
    extern errno;

    strcpy(filename, RETRIEVE_DUMP_FILE);
    timeout = RETRIEVE_DUMP_TIMEOUT;
    findXObjRomOpts(self, rpcOpts, sizeof(rpcOpts)/sizeof(XObjRomOpt), 0);
    if ((logfile = fopen(filename, "w+")) == NULL) {
	printf ("fopen (%s, w+) failed errno %d(%x)\n", filename, errno, errno);
	return XK_FAILURE;
    }
    buffer = (char *)pathAlloc(self->path, RETRIEVE_DUMP_SIZE);
    if (!buffer) {
	printf ("couldn't allocate local memory\n");
	return XK_FAILURE;
    }
    self->state = buffer;
    ev = evAlloc(self->path);
    if (!ev) {
	printf("couldn't allocate an event\n");
	pathFree(buffer);
	return XK_FAILURE;
    }
    evSchedule(ev, do_retrieve, self, timeout);
    return XK_SUCCESS;
}



