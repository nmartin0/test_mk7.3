/* 
 * Mach Operating System
 * Copyright (c) 1995 Jukka Virtanen
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JUKKA VIRTANEN ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION. JUKKA VIRTANEN DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/* 
 * HISTORY
 * $Log: second_syscalls.s,v $
# Revision 1.1.1.1  1995/03/02  21:50:00  mike
# Initial Lites release from hut.fi
#
 */
/* 
 *	File:	alpha/second_syscalls.s
 *	Author:	Jukka Virtanen
 *	Date:	Nov 1994
 *
 * system calls needed by second server to access the
 * primary server via unix system calls.
 *
 * errno is not used by the server so don't bother with it.
 */

#include <mach/alpha/asm.h>
#include <sys/syscall.h>

/*
 * Trap number is placed to v0 and the return value is in v0
 * Palcode value 0x83 taken from mach kernel.
 */	
#define kernel_trap(trap_name,trap_number,number_args) \
LEAF(trap_name,number_args); \
	lda	v0, trap_number(zero); \
	call_pal 0x83;	\
	RET;	\
	END(trap_name)

kernel_trap(second_open, SYS_open, 3)
kernel_trap(second_fstat, SYS_fstat, 3)
kernel_trap(second_access, SYS_access, 3)
kernel_trap(second_lseek, SYS_lseek, 2)
kernel_trap(second_write, SYS_write, 3)
kernel_trap(second_read, SYS_read, 3)
kernel_trap(second_close, SYS_close, 1)
kernel_trap(second_ioctl, SYS_ioctl, 3)
kernel_trap(second_sigvec, SYS_sigvec, 3)
kernel_trap(second__exit, SYS_exit, 1)
