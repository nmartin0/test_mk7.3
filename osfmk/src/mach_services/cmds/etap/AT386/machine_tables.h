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

#include <machine/trap.h>

char *exception_names[] = {TRAP_NAMES};
#define N_EXCEPTION_NAMES (sizeof exception_names / sizeof exception_names[0])
#define exception_name(nu) \
	(((nu) == T_PREEMPT) ? "preemption" : \
	 ((nu) < N_EXCEPTION_NAMES) ? exception_names[nu] : NULL)

char *interrupt_names[] = {
	"hardclock", "kdintr", "intnull", "inpoll", "comintr", "comintr",
	"fdintr", "qdintr", "intnull", "ether", "intnull", "intnull",
	"intnull", "mp_intr or fpintr", "hdintr", "intnull"
};
#define N_INTERRUPT_NAMES (sizeof interrupt_names / sizeof interrupt_names[0])
#define interrupt_name(nu) \
	(((nu) < N_INTERRUPT_NAMES) ? interrupt_names[nu] : NULL)

#define RAW_INTERRUPT_NUMBER(n) ((n) + 0x40)
