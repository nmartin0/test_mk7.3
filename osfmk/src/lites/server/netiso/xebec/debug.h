/* $Header: /n/fast/usr/lsrc/mach/CVS/lites/server/netiso/xebec/debug.h,v 1.1.1.1 1995/03/02 21:49:58 mike Exp $ */
/* $Source: /n/fast/usr/lsrc/mach/CVS/lites/server/netiso/xebec/debug.h,v $ */

#define OUT stdout

extern int	debug[128];

#ifdef DEBUG
extern int column;

#define IFDEBUG(letter) \
	if(debug['letter']) { 
#define ENDDEBUG  ; (void) fflush(stdout);}

#else 

#define STAR *
#define IFDEBUG(letter)	 //*beginning of comment*/STAR
#define ENDDEBUG	 STAR/*end of comment*//

#endif DEBUG

