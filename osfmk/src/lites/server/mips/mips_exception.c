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

#include <serv/import_mach.h>
#include <mach/exception.h>
#include <mach/mips/mips_instruction.h>
#include <mach/mips/exception.h>
#include <mips/trap.h>
#include <sys/signal.h>

/*
 *	machine_exception translates a mach exception to a unix exception
 *	and code.  This handles all the hardware-specific exceptions for
 *	Mips.  unix_exception() handles the machine-independent ones.
 */

boolean_t
machine_exception(integer_t exception, integer_t code, integer_t subcode,
		  int *unix_signal, int *unix_code)
{
	/*
	 * To get Ultrix-like behavior, we want ADEL/ADES exceptions
	 * to yield SIGBUS and TLBL/TLBS exceptions to yield SIGSEGV.
	 */

	switch(exception) {

	    case EXC_BAD_ACCESS:
		*unix_signal = SIGSEGV;
		break;

	    case EXC_BAD_INSTRUCTION:
		switch (code) {
		    case EXC_MIPS_PRIVINST:
	        	*unix_signal = SIGILL;
			*unix_code = ILL_PRIVIN_FAULT;
			break;
		    case EXC_MIPS_RESOPND:
	        	*unix_signal = SIGILL;
			*unix_code = ILL_RESOP_FAULT;
			break;
		    case EXC_MIPS_RESADDR:
	        	*unix_signal = SIGBUS;
			break;
		    default:
			return(FALSE);
		}
		break;

	    case EXC_ARITHMETIC:
	        *unix_signal = SIGFPE;
		switch (code) {
		    case EXC_MIPS_FLT_OVERFLOW:
			*unix_code = FPE_FLTOVF_FAULT;
			break;
		    case EXC_MIPS_FLT_DIVIDE0:
			*unix_code = FPE_FLTDIV_FAULT;
			break;
		    case EXC_MIPS_FLT_UNDERFLOW:
			*unix_code = FPE_FLTUND_FAULT;
			break;
		    case EXC_MIPS_FLT_UNIMP:
			*unix_code = FPE_UNIMP_FAULT;
			break;
		    case EXC_MIPS_FLT_INVALID:
			*unix_code = FPE_INVALID_FAULT;
			break;
		    case EXC_MIPS_FLT_INEXACT:
			*unix_code = FPE_INEXACT_FAULT;
			break;

		    default:
			return(FALSE);
		}
		break;

	    case EXC_SOFTWARE:
		switch (code) {
		    case EXC_MIPS_SOFT_SEGV:
		        *unix_signal = SIGSEGV;
			break;
		    case EXC_MIPS_SOFT_CPU:
		        *unix_signal = SIGILL;
			*unix_code = ILL_COPR_UNUSABLE;
			break;
		    default:
			return(FALSE);
		}
		break;

	    case EXC_BREAKPOINT:
		*unix_signal = SIGTRAP;
		/*
		 * Now is this a hack or what ? Need to tell later wether
		 * this was a T-bit (single step) trap or not. So we munge
		 * the code in the negative, knowing nobody will really
		 * look at it when negative. [Same trick is played in exec]
		 */
		if (code == BREAK_SSTEP)
			*unix_code = - 4;	/* CAUSESINGLE */
		else
			*unix_code = (subcode >> 16) & 0x3ff;
		break;

	    default:
		return(FALSE);
	}
	return(TRUE);
}
