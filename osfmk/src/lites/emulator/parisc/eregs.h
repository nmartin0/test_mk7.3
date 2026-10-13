/* 
 * Copyright (c) 1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: eregs.h 1.4 95/03/03$
 */

/*
 * Caller register state and auxilliary information.
 * Used by the emulator routines to examine/modify user state.
 */
struct emul_regs {
	int	*argp;	/* pointer to *after* arg0 frame slot */
	int	flags;	/* see below */
	int	rp;	/* stub caller return offset (r2) */
	int	scnum;	/* syscall number and error indicator (r22) */
	int	dp;	/* caller data pointer (r27) */
	int	*reto;	/* return offset for syscall stub (r31) */
	int	rets;	/* return space for syscall stub (sr0) */
	int	ret1;	/* syscall second return value (r29) */
};

/* flags */
#define	SB_USESR4	0x1	/* use sr4 for return space */
#define	SB_RESTART	0x2	/* reload registers for restarted syscall */
#define SB_NEW_SP_IN_RET1 0x4	/* New stack pointer in ret1 */
