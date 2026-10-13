/* 
 * Copyright (c) 1994, The University of Utah and
 * the Center for Software Science at the University of Utah (CSS).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSS ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSS DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSS requests users of this software to return to css-dist@cs.utah.edu any
 * improvements that they make and grant CSS redistribution rights.
 *
 */

#include <sys/som.h>

/* Size of a page in an object file. */
#define	__LDPGSZ	4096

/*
 * XXX to keep the emulator happy (building).
 */
#define	exec	som_exechdr
#define OMAGIC 0407
#define NMAGIC 0410
#define ZMAGIC 0413
#define QMAGIC 0314
