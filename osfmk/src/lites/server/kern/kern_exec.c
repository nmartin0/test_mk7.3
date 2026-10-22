/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)kern_exec.c	8.1 (Berkeley) 6/10/93
 */

#include "no_emulator.h"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/proc.h>
#if NO_EMULATOR
#include <serv/server_defs.h>
#include <sys/systm.h>
#include <sys/syslimits.h>
#endif

/*
 * exec system call
 */
struct execve_args {
	char	*fname;
	char	**argp;
	char	**envp;
};
#if NO_EMULATOR
/*
 * AI-ONLY NOTE: a body, ours, for the server without the emulator
 * (docs/roadmap.md, Tier 1 item 2); Lite's was "deleted", as above.
 * The emulator's execve copies the path, arguments and environment
 * out of the program and asks the server for the rest (bsd_execve(),
 * s_execve()); here the server does the copying, with copyinstr(),
 * into the form s_execve() takes from the emulator: the strings, the
 * arguments' then the environment's, each NUL-ended, in the calling
 * task, with their counts.  kludge_out_args() (serv/server_exec.c)
 * makes the same for init.  ARG_MAX bounds them, as in 4.4BSD.
 * s_execve() starts the new program on the calling thread
 * (serv/server_exec.c), so it returns EJUSTRETURN: the registers are
 * the new program's, and the system call leaves them so
 * (i386/syscall.c).
 */
execve(p, uap, retval)
	struct proc *p;
	struct execve_args *uap;
	int *retval;
{
	char fname[MAXPATHLEN];
	char **vec[2];
	int counts[2];
	vm_offset_t strings, arg_addr, ap;
	vm_size_t strings_size, size;
	u_int len, cc;
	mach_port_t task = p->p_task;
	kern_return_t kr;
	int error, i, n;

	error = copyinstr(uap->fname, fname, sizeof fname, &len);
	if (error)
		return (error == ENOENT ? ENAMETOOLONG : error);

	strings_size = round_page(ARG_MAX);
	kr = vm_allocate(mach_task_self(), &strings, strings_size, TRUE);
	if (kr != KERN_SUCCESS)
		return (ENOMEM);

	cc = 0;
	vec[0] = uap->argp;
	vec[1] = uap->envp;
	for (i = 0; i < 2; i++) {
		counts[i] = 0;
		if (vec[i] == 0)
			continue;
		for (n = 0; ; n++) {
			if (copyin((caddr_t)(vec[i] + n), (caddr_t)&ap,
				   sizeof ap)) {
				error = EFAULT;
				goto out;
			}
			if (ap == 0)
				break;
			if (cc >= ARG_MAX) {
				error = E2BIG;
				goto out;
			}
			error = copyinstr((void *)ap, (void *)(strings + cc),
					  ARG_MAX - cc, &len);
			if (error) {
				if (error == ENOENT)
					error = E2BIG;
				goto out;
			}
			cc += len;
		}
		counts[i] = n;
	}

	/* in the calling task, where s_execve() reads them */
	size = round_page(cc ? cc : 1);
	arg_addr = 0;
	kr = vm_allocate(task, &arg_addr, size, TRUE);
	if (kr == KERN_SUCCESS)
		kr = vm_write(task, arg_addr, strings, size);
	if (kr != KERN_SUCCESS) {
		error = ENOMEM;
		goto out;
	}
	(void) vm_deallocate(mach_task_self(), strings, strings_size);
	strings = 0;

	error = s_execve(p, fname, arg_addr, size, counts[0], counts[1]);
	if (error == 0)
		return (EJUSTRETURN);
	/* not started: the old program goes on, if it is still there */
	if (p->p_task == task && !(p->p_flag & P_WEXIT))
		(void) vm_deallocate(task, arg_addr, size);
out:
	if (strings)
		(void) vm_deallocate(mach_task_self(), strings, strings_size);
	return (error);
}
#else /* NO_EMULATOR */
/* ARGSUSED */
execve(a1, a2, a3)
	struct proc *a1;
	struct execve_args *a2;
	int *a3;
{

	/*
	 * Body deleted.
	 */
	return (ENOSYS);
}
#endif /* NO_EMULATOR */
