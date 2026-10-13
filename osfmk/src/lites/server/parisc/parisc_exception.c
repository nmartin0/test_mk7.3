/* 
 * Copyright (c) 1990,1991,1992,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *	Utah $Hdr: parisc_exception.c 1.14 95/03/03$
 */
/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */

#define EXPORT_BOOLEAN

#include <mach/boolean.h>
#include <mach/exception.h>
#include <sys/types.h>
#include <mach/kern_return.h>
#include <sys/signal.h>

boolean_t
machine_exception(integer_t exception, integer_t code, integer_t subcode,
		  int *unix_signal, int *unix_code)
{
	switch(exception) {

	    case EXC_BAD_INSTRUCTION:
	        *unix_signal = SIGILL;
		switch (code) {
		    case EXC_PARISC_INVALID_SYSCALL:
			*unix_signal = SIGSYS;
			*unix_code = 0;
			break;
		    case EXC_PARISC_UNIPL_INST:
			*unix_code = ILL_UNIMPL_INST;
			break;
		    case EXC_PARISC_PRIVINST:
			*unix_code = ILL_PRIV_OP;
			break;
		    case EXC_PARISC_PRIVREG:
			*unix_code = ILL_PRIV_REG;
			break;
		    default:
			return(FALSE);
		}
		break;

	    case EXC_BAD_ACCESS:
		switch (code) {
		    case EXC_PARISC_VM_PROT_READ:
		    case EXC_PARISC_BADSPACE:
			*unix_signal = SIGSEGV;
			break;
		    case EXC_PARISC_UNALIGNED:
			*unix_signal = SIGBUS;
			break;
		    default:
			return(FALSE);
		}
		break;

	    case EXC_ARITHMETIC:
		/*
		 * XXX  This is a potential Superfund site!  Call the EPA!
		 */
	        *unix_signal = SIGFPE;
		switch (code) {
		    case EXC_PARISC_OVERFLOW:
			*unix_code = ILL_OVFLO;
			break;
		    case EXC_PARISC_ZERO_DIVIDE:
			*unix_code = ILL_COND;
			break;
		    case EXC_PARISC_FLT_INEXACT:
		    case EXC_PARISC_FLT_ZERO_DIVIDE:
		    case EXC_PARISC_FLT_UNDERFLOW:
		    case EXC_PARISC_FLT_OVERFLOW:
		    case EXC_PARISC_FLT_NOT_A_NUMBER:
			*unix_code = ILL_EXCEP;
			break;
		    default:
			return(FALSE);
		}
		break;

	    case EXC_SOFTWARE:
		return(FALSE);
		break;

	    case EXC_BREAKPOINT:
		*unix_signal = SIGTRAP;
		break;

	    default:
		return(FALSE);
	}
	return(TRUE);
}
