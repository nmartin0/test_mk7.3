/*
 * Copyright (c) 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)signal.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Machine-dependent signal definitions
 *
 *	Author:	Ian Dall
 *		September 1994
 *
 * HISTORY
 * 11-Oct-95  Ian Dall (dall@hfrd.dsto.gov.au)
 *	Make sigcontext conform with netbsd. Add more FPE codes.
 *
 */

typedef int sig_atomic_t;

#ifndef _POSIX_SOURCE
#include <machine/trap.h>	/* codes for SIGILL, SIGFPE */
#endif

#define	    ILL_PRIVIN_FAULT	0x1	/* privileged instruction fault */
#define	    ILL_RESOP_FAULT	0x2	/* reserved operand fault */

#define	    FPE_INTOVF_TRAP	0x1	/* integer overflow */
#define	    FPE_INTDIV_TRAP	0x2	/* integer divide by zero */
#define	    FPE_FLTOVF_TRAP	0x3	/* floating overflow */
#define	    FPE_FLTDIV_TRAP	0x4	/* floating/decimal divide by zero */
#define	    FPE_FLTUND_TRAP	0x5	/* floating underflow */
#define	    FPE_DECOVF_TRAP	0x6	/* decimal overflow */
#define	    FPE_SUBRNG_TRAP	0x7	/* subscript out of range */
#define	    FPE_FLTOVF_FAULT	0x8	/* floating overflow fault */
#define	    FPE_FLTDIV_FAULT	0x9	/* divide by zero floating fault */
#define	    FPE_FLTUND_FAULT	0xa	/* floating underflow fault */
#define	    FPE_INVALID_FAULT	0xc	/* floating invalid operand */
#define     FPE_INEXACT_FAULT	0xd	/* floating inexact result */

#define     FPE_FLG_TRAP	0xe     /* flag instruction trap */

/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 */
struct	sigcontext {
	int	sc_onstack;		/* sigstack state to restore */
	int	sc_mask;		/* signal mask to restore */
	int	sc_sp;			/* sp to restore */
	int	sc_fp;			/* fp to restore */
	int	sc_sb;			/* sb to restore */
	int	sc_pc;			/* pc to restore */
	int	sc_ps;			/* psl to restore */
	int	sc_reg[8];		/* The registers */
};
