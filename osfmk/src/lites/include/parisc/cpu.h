/* 
 * Copyright (c) 1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the Computer
 * Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 */
/*
 * HISTORY
 * $Log: cpu.h,v $
 * Revision 1.1.1.1  1995/03/02  21:49:38  mike
 * Initial Lites release from hut.fi
 *
 */

/*
 * Exported definitions unique to hp700/PA-RISC cpu support.
 */

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#undef	COPY_SIGCODE		/* copy sigcode above user stack in exec */

/*
 * Supported processors
 */
#define	HP_720		0x200	/* [S] Cobra */
#define	HP_750		0x201	/* [S] Coral */
#define	HP_730		0x202	/* [S] King Cobra */

#define	HP_710		0x300	/* [S] Bushmaster */
#define	HP_705		0x302	/* [S] Bushmaster */

#define	HP_715_33	0x311	/* [S] Scorpio */
#define	HP_715_50	0x310	/* [S] Scorpio */
#define	HP_715T_50	0x312	/* [S] Scorpio (upgrade from 4XXt) */
#define	HP_715S_50	0x314	/* [S] Scorpio (upgrade from 4XXs) */
#define	HP_715_75	0x316	/* [S] Scorpio */
#define	HP_725		0xFFF	/* [S] Scorpio -- is this the real ID? */
#define	HP_735		0x203	/* [S] Hardball */
#define	HP_755		0xFFD	/* [S] Coral II -- is this the real ID? */

#define	HP_712_60	0x600	/* [S] Gekko */
#define	HP_712_80	0x601	/* [S] Gekko */
#define	HP_712_3	0x602	/* [S] Gekko */
#define	HP_712_4	0x605	/* [S] Gekko */

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}
