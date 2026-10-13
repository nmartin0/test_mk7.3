/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1992 Johannes Helander
 * Copyright (c) 1994 Ian Dall
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND JOHANNES HELANDER AND IAN DALL ALLOW FREE USE
 * OF THIS SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND
 * JOHANNES HELANDER AND IAN DALL DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/* HISTORY
 * 07-Apr-95  Ian Dall (dall@hfrd.dsto.gov.au)
 *	Add subcode handling for fpu traps. Make sure unix_code doesn`t
 *	get altered unless machine_exception succeeds.
 *
 */

#include <mach/boolean.h>
#include <mach/exception.h>
#include <sys/types.h>
#include <mach/kern_return.h>
#include <sys/signal.h>

boolean_t
machine_exception(integer_t exception, integer_t code, integer_t subcode,
		  int *unix_signal, int *unix_code)
{
  integer_t return_signal = 0, return_code = 0;
  switch(exception) {

  case EXC_BAD_INSTRUCTION:
    return_signal = SIGILL;
    switch (code) {
    case EXC_NS532_ILL:
      return_code = ILL_PRIVIN_FAULT;
      break;
    case EXC_NS532_UND:
      return_code = ILL_RESOP_FAULT;
      break;
    default:
      return(FALSE);
    }
    break;

  case EXC_ARITHMETIC:
    return_signal = SIGFPE;
    switch (code) {
    case EXC_NS532_OVF:
      return_code = FPE_INTOVF_TRAP;
      break;
    case EXC_NS532_DVZ:
      return_code = FPE_INTDIV_TRAP;
      break;
    case EXC_NS532_FLG:
      return_code = FPE_FLG_TRAP;
      break;
    case EXC_NS532_SLAVE:
      switch(subcode & 7) {
      case 1:
	return_code = FPE_FLTUND_TRAP;
	break;
      case 2:
	return_code = FPE_FLTOVF_FAULT;
	break;
      case 3:
	return_code = FPE_FLTDIV_TRAP;
	break;
      case 4:
	return_signal = SIGILL;
	return_code = 0;
	break;
      case 5:
	return_code = FPE_INVALID_FAULT;
	break;
      case 6:
	return_code = FPE_INEXACT_FAULT;
	break;
      }
      break;
    default:
      return_code = 0;
      return(FALSE);
    }
    break;

  case EXC_BREAKPOINT:
    return_signal = SIGTRAP;
    break;

  default:
    return(FALSE);
  }
  *unix_signal = return_signal;
  *unix_code = return_code;
  return(TRUE);
}
