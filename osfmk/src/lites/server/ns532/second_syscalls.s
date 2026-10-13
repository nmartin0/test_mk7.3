/* 
 * Mach Operating System
 * Copyright (c) 1994 Johannes Helander
 * Copyright (c) 1994 Ian Dall
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JOHANNES HELANDER AND IAN DALL ALLOW FREE USE OF THIS SOFTWARE IN
 * ITS "AS IS" CONDITION.  JOHANNES HELANDER AND IAN DALL DISCLAIM ANY
 * LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 * USE OF THIS SOFTWARE.
 */
/*
 * HISTORY
 * $Log: second_syscalls.s,v $
# Revision 1.1.1.1  1995/03/02  21:49:42  mike
# Initial Lites release from hut.fi
#
 */
/* 
 *	File:	ns532/second_syscalls.s
 *	Author:	Ian Dall
 *	Date:	July 1994
 *
 *  based on i386 version by
 *	Johannes Helander
 *
 * system calls needed by second server to access the
 * primary server via unix system calls.
 *
 * errno is not used by the server so don't bother with it.
 */

#include <ns532/asm.h>
#include <sys/syscall.h>

#define kernel_trap(trap_name,trap_number,number_args) \
ENTRY(trap_name) \
	movd	trap_number, r0; \
	SVC; \
	bcs 1f; \
	ret 0; \
1:	movqd -1, r0; \
	ret 0;

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
