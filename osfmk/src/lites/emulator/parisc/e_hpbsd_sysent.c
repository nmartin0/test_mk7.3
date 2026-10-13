/* 
 * Copyright (c) 1994 The University of Utah and
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
 * 	Utah $Hdr: e_hpbsd_sysent.c 1.6 95/03/23$
 */

/*
 *	System call switch table for Utah's 4.3/4.4 BSD hybrid on
 *	hp300/400 and 700/800 series machines.
 */

#include <e_defs.h>
#include <syscall_table.h>

#define SYS_NULL(routine)	{ E_GENERIC, e_nosys, 0, # routine }
#define sysg(routine, nargs)    { nargs, (int (*)()) routine, 0, # routine }
#define SYS_REG(routine)	{ E_CHANGE_REGS, routine, 0, # routine }

int emul_generic();

int e_nosys();
int e_execv(), getpagesize();
int e_vhangup(), e_vlimit();
int e_getdopt(), e_setdopt(), e_send(), e_recv(), e_vtimes();
int e_sigblock(), e_sigsetmask(), e_setreuid(), e_setregid(), e_killpg();
int e_quotactl(), e_creat(), e_access();
int e_sigpause(), e_sysctl();
int e_mapped_gettimeofday();

int e_lite_getdirentries();

int e_bnr_lseek(), e_bnr_stat(), e_bnr_lstat(), e_bnr_fstat();
int e_bnr_truncate(), e_bnr_ftruncate(), e_bnr_getdirentries();
int e_bnr_getrlimit(), e_bnr_setrlimit(), e_bnr_sigvec();
int e_bnr_getpeername();

int e_machine_fork(), e_machine_vfork(), e_hpbsd_wait4();
int e_machine_wait(), e_hpbsd_ptrace(), e_hpbsd_setpgrp(); 
int e_hpbsd_times(), e_hpbsd_profil(), e_hpbsd_ktrace();
int e_hpbsd_ssig(), e_hpbsd_mount(), e_hpbsd_noerror();
int e_hpbsd_nfssvc(), e_machine_sigreturn();
int e_hpbsd_getpid(), e_hpbsd_getuid(), e_hpbsd_getgid();

int e_task_by_pid(), e_pid_by_task(), e_htg_syscall(), e_init_process();

struct sysent e_hpbsd_sysent[] = {
	SYS_NULL(e_indir),				/* 0 */
	sysg(exit, 1),					/* 1 */
	SYS_REG(e_machine_fork),			/* 2 */
	sysg(e_read, 3),				/* 3 */
	sysg(e_write, 3),				/* 4 */
	sysg(e_open, 3),				/* 5 */
	sysg(e_close, 1),				/* 6 */
	sysg(e_hpbsd_wait4, 4),				/* 7 */
	sysg(e_creat, 2),				/* 8 */
	sysg(e_link, 2),				/* 9 */
	sysg(e_unlink, 1),				/* 10 */
	SYS_NULL(e_execv),				/* 11 */
	sysg(e_chdir, 1),				/* 12 */
	sysg(e_fchdir, 1),				/* 13 */
	sysg(e_mknod, 3),				/* 14 */
	sysg(e_chmod, 2),				/* 15 */
	sysg(e_chown, 3),				/* 16 */
	sysg(e_obreak, 1),				/* 17 */
	sysg(e_getfsstat, 3),				/* 18 */
	sysg(e_bnr_lseek, 3),				/* 19 */
	sysg(e_hpbsd_getpid, 0),			/* 20 */
	sysg(e_hpbsd_mount, 4),				/* 21 */
	sysg(e_unmount, 2),				/* 22 */
	sysg(e_setuid, 1),				/* 23 */
	sysg(e_hpbsd_getuid, 0),			/* 24 */
	sysg(e_nosys, 1),				/* 25 was stime */
	sysg(e_hpbsd_ptrace, 5),			/* 26 */
	sysg(e_recvmsg, 3),				/* 27 */
	sysg(e_sendmsg, 3),				/* 28 */
	sysg(e_recvfrom, 6),				/* 29 */
	sysg(e_accept, 3),				/* 30 */
	sysg(e_getpeername, 3),				/* 31 */
	sysg(e_getsockname, 3),				/* 32 */
	sysg(e_access, 2),				/* 33 */
	sysg(e_chflags, 2),				/* 34 */
	sysg(e_fchflags, 2),				/* 35 */
	sysg(e_sync, 0),				/* 36 */
	sysg(e_kill, 2),				/* 37 */
	sysg(e_bnr_stat, 2),				/* 38 */
	sysg(e_hpbsd_setpgrp, 2),			/* 39 */
	sysg(e_bnr_lstat, 2),				/* 40 */
	sysg(e_dup, 1),					/* 41 */
	sysg(e_pipe, 0),	/* two return values	   42 */
	sysg(e_hpbsd_times, 0),				/* 43 */
	sysg(e_hpbsd_profil, 4),			/* 44 */
	sysg(e_hpbsd_ktrace, 4),			/* 45 */
	sysg(e_setgid, 1),				/* 46 */
	sysg(e_hpbsd_getgid, 0),			/* 47 */
	sysg(e_hpbsd_ssig, 2),				/* 48 */
	sysg(e_nosys, 2),				/* 49 */
	sysg(e_nosys, 1),				/* 50 */
	sysg(e_sysacct, 1),				/* 51 */
	sysg(e_nosys, 1),				/* 52 */
	sysg(e_nosys, 3),				/* 53 */
	sysg(e_ioctl, 3),				/* 54 */
	sysg(e_reboot, 1),				/* 55 */
	sysg(e_revoke, 1),				/* 56 */
	sysg(e_symlink, 2),				/* 57 */
	sysg(e_readlink, 3),				/* 58 */
	sysg(e_execve, 3),				/* 59 */
	sysg(e_umask, 1),				/* 60 */
	sysg(e_chroot, 1),				/* 61 */
	sysg(e_bnr_fstat, 2),				/* 62 */
	sysg(e_getkerninfo, 4),				/* 63 */
	sysg(e_getpagesize, 0),				/* 64 */
	sysg(e_hpbsd_noerror, 0),			/* 65 - mremap */
	SYS_REG(e_machine_vfork),			/* 66 */
	sysg(e_nosys, 0),				/* 67 */
	sysg(e_nosys, 0),				/* 68 */
	sysg(e_sbrk, 1),				/* 69 */
	sysg(e_hpbsd_noerror, 0),			/* 70 - sstk */
	sysg(e_hpbsd_noerror, 0),			/* 71 - mmap */
	sysg(e_nosys, 1),				/* 72 was vadvise */
	sysg(e_hpbsd_noerror, 0),			/* 73 munmap */
	sysg(e_mprotect, 3),				/* 74 */
	sysg(e_madvise, 3),				/* 75 */
	sysg(e_vhangup, 0),				/* 76 */
	sysg(e_vlimit, 0),				/* 77 */
	sysg(e_mincore, 3),				/* 78 */
	sysg(e_getgroups, 2),				/* 79 */
	sysg(e_setgroups, 2),				/* 80 */
	sysg(e_getpgrp, 0),				/* 81 */
	sysg(e_setpgid, 2),				/* 82 */
	sysg(e_setitimer, 3),				/* 83 */
	SYS_REG(e_machine_wait),			/* 84 */
	sysg(e_swapon, 1),				/* 85 */
	sysg(e_getitimer, 2),				/* 86 */
	sysg(e_gethostname, 2),				/* 87 */
	sysg(e_sethostname, 2),				/* 88 */
	sysg(e_getdtablesize, 0),			/* 89 */
	sysg(e_dup2, 2),				/* 90 */
	sysg(e_getdopt, 2),				/* 91 */
	sysg(e_fcntl, 3),				/* 92 */
	sysg(e_select, 5),				/* 93 */
	sysg(e_setdopt, 2),				/* 94 */
	sysg(e_fsync, 1),				/* 95 */
	sysg(e_setpriority, 3),				/* 96 */
	sysg(e_socket, 3),				/* 97 */
	sysg(e_connect, 3),				/* 98 */
	sysg(e_accept, 3),				/* 99 */
	sysg(e_getpriority, 2),				/* 100 */
	sysg(e_send, 4),				/* 101 */
	sysg(e_recv, 4),				/* 102 */
	SYS_REG(e_machine_sigreturn),			/* 103 */
	sysg(e_bind, 3),				/* 104 */
	sysg(e_setsockopt, 5),				/* 105 */
	sysg(e_listen, 2),				/* 106 */
	sysg(e_vtimes, 0),				/* 107 */
	sysg(e_bnr_sigvec, 3),				/* 108 */
	sysg(e_sigblock, 1),				/* 109 */
	sysg(e_sigsetmask, 1),				/* 110 */
	sysg(e_sigpause, 1),				/* 111 */
	sysg(e_sigstack, 2),				/* 112 */
	sysg(e_recvmsg, 3),				/* 113 */
	sysg(e_sendmsg, 3),				/* 114 */
	sysg(e_nosys, 2),				/* 115 */
	sysg(e_mapped_gettimeofday, 2),			/* 116 */
	sysg(e_getrusage, 2),				/* 117 */
	sysg(e_getsockopt, 5),				/* 118 */
	sysg(e_nosys, 0),				/* 119 */
	sysg(e_readv, 3),				/* 120 */
	sysg(e_writev, 3),				/* 121 */
	sysg(e_settimeofday, 2),			/* 122 */
	sysg(e_fchown, 3),				/* 123 */
	sysg(e_fchmod, 2),				/* 124 */
	sysg(e_recvfrom, 6),				/* 125 */
	sysg(e_setreuid, 2),				/* 126 */
	sysg(e_setregid, 2),				/* 127 */
	sysg(e_rename, 2),				/* 128 */
	sysg(e_bnr_truncate, 2),			/* 129 */
	sysg(e_bnr_ftruncate, 2),			/* 130 */
	sysg(e_flock, 2),				/* 131 */
	sysg(e_mkfifo, 2),				/* 132 */
	sysg(e_sendto, 6),				/* 133 */
	sysg(e_shutdown, 2),				/* 134 */
	sysg(e_socketpair, 4),				/* 135 */
	sysg(e_mkdir, 2),				/* 136 */
	sysg(e_rmdir, 1),				/* 137 */
	sysg(e_utimes, 2),				/* 138 */
	sysg(e_nosys, 0),				/* 139 */
	sysg(e_adjtime, 2),				/* 140 */
	sysg(e_bnr_getpeername, 3),			/* 141 */
	sysg(e_gethostid, 0),				/* 142 */
	sysg(e_sethostid, 1),				/* 143 */
	sysg(e_bnr_getrlimit, 2),			/* 144 */
	sysg(e_bnr_setrlimit, 2),			/* 145 */
	sysg(e_killpg, 2),				/* 146 */
	sysg(e_nosys, 0),				/* 147 */
	sysg(e_quotactl, 4),				/* 148 */
	sysg(e_nosys, 0),				/* 149 */
	sysg(e_getsockname, 3),				/* 150 */
	sysg(e_nosys, 0),				/* 151 */
	sysg(e_nosys, 0),				/* 152 */
	sysg(e_nosys, 0),				/* 153 */
	sysg(e_nosys, 0),				/* 154 */
	sysg(e_hpbsd_nfssvc, 2),			/* 155 */
	sysg(e_bnr_getdirentries, 4),			/* 156 */
	sysg(e_statfs, 2),				/* 157 */
	sysg(e_fstatfs, 2),				/* 158 */
	sysg(e_nosys, 0),				/* 159 */
	sysg(e_nosys, 0),				/* 160 */
	sysg(e_getfh, 2),				/* 161 */
	sysg(e_nosys, 0),				/* 162 */
	sysg(e_nosys, 0),				/* 163 */
	sysg(e_nosys, 0),				/* 164 */
	sysg(e_nosys, 0),				/* 165 */
	sysg(e_nosys, 0),				/* 166 */
	sysg(e_nosys, 0),				/* 167 */
	sysg(e_nosys, 0),				/* 168 */
	sysg(e_nosys, 0),				/* 169 */
	sysg(e_nosys, 0),				/* 170 */
#if SYSVSHM
	sysg(e_shmsys, 4),				/* 171 */
#else
	sysg(e_nosys, 0),				/* 171 */
#endif
	sysg(e_nosys, 0),				/* 172 */
	sysg(e_nosys, 0),				/* 173 */
	sysg(e_nosys, 0),				/* 174 */
	sysg(e_nosys, 0),				/* 175 */
	sysg(e_nosys, 0),				/* 176 */
	sysg(e_nosys, 0),				/* 177 */
	sysg(e_nosys, 0),				/* 178 */
	sysg(e_nosys, 0),				/* 179 */
	sysg(e_nosys, 0),				/* 180 */
	sysg(e_nosys, 0),				/* 181 */
	sysg(e_nosys, 0),				/* 182 */
	sysg(e_nosys, 0),				/* 183 */
	sysg(e_nosys, 0),				/* 184 */
	sysg(e_nosys, 0),				/* 185 */
	sysg(e_nosys, 0),				/* 186 */
	sysg(e_nosys, 0),				/* 187 */
	sysg(e_nosys, 0),				/* 188 */
	sysg(e_nosys, 0),				/* 189 */
	sysg(e_nosys, 0),				/* 190 */
	sysg(e_nosys, 0),				/* 191 */
	sysg(e_nosys, 0),				/* 192 */
	sysg(e_nosys, 0),				/* 193 */
	sysg(e_nosys, 0),				/* 194 */
	sysg(e_nosys, 0),				/* 195 */
	sysg(e_lite_getdirentries, 4),			/* 196 */
	sysg(e_nosys, 0),				/* 197 */
	sysg(e_nosys, 0),				/* 198 */
	sysg(e_nosys, 0),				/* 199 */
	sysg(e_nosys, 0),				/* 200 */
	sysg(e_nosys, 0),				/* 201 */
	sysg(e_sysctl, 6),				/* 202 */
};

int	e_hpbsd_nsysent = sizeof(e_hpbsd_sysent)/sizeof(struct sysent);

struct sysent   sysent_table		= sysg(e_table, 5);
