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

#include <mach/boolean.h>
#include <mach/exception.h>
#include <mach/kern_return.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <alpha/trap.h>
#include <mach/alpha/alpha_instruction.h>

/*
 *	machine_exception translates a mach exception to a unix exception
 *	and code.  This handles all the hardware-specific exceptions for
 *	Alpha.  unix_exception() handles the machine-independent ones.
 */

boolean_t
machine_exception(integer_t exception, integer_t code, integer_t subcode,
		  int *unix_signal, int *unix_code)
{
	switch(exception) {

	    case EXC_BAD_ACCESS:
		*unix_signal = SIGSEGV;
		break;

	    case EXC_BAD_INSTRUCTION:
		switch (code) {
		    case EXC_ALPHA_PRIVINST:
	        	*unix_signal = SIGILL;
			*unix_code = ILL_PRIVIN_FAULT;
			break;
		    case EXC_ALPHA_RESOPND:
	        	*unix_signal = SIGILL;
			*unix_code = ILL_RESOP_FAULT;
			break;
		    case EXC_ALPHA_RESADDR:
	        	*unix_signal = SIGBUS;
			break;
		    default:
			return(FALSE);
		}
		break;

	    case EXC_ARITHMETIC:
	        *unix_signal = SIGFPE;
		*unix_code = code;
		break;

	    case EXC_BREAKPOINT:
		*unix_signal = SIGTRAP;
		break;

	    default:
		return(FALSE);
	}
	return(TRUE);
}

