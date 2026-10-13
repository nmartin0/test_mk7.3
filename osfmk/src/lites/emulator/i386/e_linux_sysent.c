/* 
 * Mach Operating System
 * Copyright (c) 1994 Johannes Helander
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * JOHANNES HELANDER ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  JOHANNES HELANDER DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/*
 * HISTORY
 * $Log: e_linux_sysent.c,v $
 * Revision 1.2  1995/09/03  21:22:16  gback
 * added support for Linux mount and reboot
 *
 * Revision 1.1.1.1  1995/03/02  21:49:29  mike
 * Initial Lites release from hut.fi
 *
 */
/* 
 *	File:	emulator/e_linux_sysent.c
 *	Author:	Johannes Helander, Helsinki University of Technology, 1994.
 *	Date:	May 1994
 *
 *	System call switch table for Linux compat.
 */

#include <e_defs.h>
#include <syscall_table.h>

#define SYS_NULL(routine)	{ E_GENERIC, e_nosys, 0, # routine }
#define sysg(routine, nargs)    { nargs, (int (*)()) routine, 0, # routine }
#define SYS_REG(routine)	{ E_CHANGE_REGS, routine, 0, # routine }
#define SYSG(routine, nargs)	SYS_NULL(routine)

int emul_generic();

int e_nosys();
int e_execv(), getpagesize();
int e_sigaltstack(), e_vread(), e_vwrite(), e_vhangup(), e_vlimit();
int e_getdopt(), e_setdopt(), e_vtimes();
int e_sigblock(), e_task_by_pid(), e_pid_by_task(), e_init_process();
int e_sigsetmask(), e_setreuid(), e_setregid(), e_quota();
int e_htg_syscall(), e_creat(), e_access();
int e_machine_wait(), e_machine_fork(), e_machine_vfork(), e_bnr_sigvec();

int e_sysv_fstatfs(), e_sysv_statfs(), e_sysv_nice(), e_sysv_stime();
int e_sysv_signal(), e_sysv_alarm(), e_sysv_lstat(), e_sysv_stat();
int e_isc4_lstat(), e_isc4_stat(), e_isc4_sysi86(), e_sysv_time();
int e_linux_reboot(), e_linux_mount();
int e_linux_uselib(), e_linux_mmap(), e_linux_brk(), e_linux_stat();
int e_linux_lstat(), e_linux_fstat(), e_linux_readdir(), e_linux_open();
int e_linux_sigprocmask(), e_linux_sigaction(), e_linux_ioctl();
int e_linux_select(), e_linux_socketcall(), e_linux_uname();
int e_linux_sigreturn(), e_linux_sysvipc(), e_linux_kill(), e_linux_wait4();

int e_bnr_lseek(), e_bnr_stat(), e_bnr_lstat(), e_bnr_fstat();
int e_bnr_truncate(), e_bnr_ftruncate(), e_bnr_getdirentries();
int e_bnr_getrlimit(), e_bnr_setrlimit(), e_bnr_wait4();

int e_mapped_gettimeofday();

struct sysent e_linux_sysent[] = {
	SYS_NULL(e_setup),					/* 0 */
	sysg(exit, 1),						/* 1 */
	SYS_REG(e_machine_fork),					/* 2 */
	sysg(e_read, 3),					/* 3 */
	sysg(e_write, 3),					/* 4 */
	sysg(e_linux_open, 3),					/* 5 */
	sysg(e_close, 1),					/* 6 */
	SYSG(e_sysv_waitpid, 3),				/* 7 */
	sysg(e_creat, 2),					/* 8 */
	sysg(e_link, 2),					/* 9 */
	sysg(e_unlink, 1),					/* 10 */
	sysg(e_execve, 3),					/* 11 */
	sysg(e_chdir, 1),					/* 12 */
	sysg(e_sysv_time, 1),	/* TIME */			/* 13 */
	sysg(e_mknod, 3),					/* 14 */
	sysg(e_chmod, 2),					/* 15 */
	sysg(e_chown, 3),					/* 16 */
	sysg(e_obreak, 1),					/* 17 */
	SYSG(e_linux_stat, 3),	/* STAT */			/* 18 */
	sysg(e_bnr_lseek, 3),					/* 19 */
	sysg(e_getpid, 0),					/* 20 */
	sysg(e_linux_mount, 5),					/* 21 */
	sysg(e_unmount, 2),					/* 22 */
	sysg(e_setuid, 1),					/* 23 */
	sysg(e_getuid, 0),					/* 24 */
	sysg(e_sysv_stime, 1),	/* STIME */			/* 25 */
	sysg(e_ptrace, 4),					/* 26 */
	sysg(e_sysv_alarm, 1),	/* ALARM */			/* 27 */
	sysg(e_linux_fstat, 2),	/* FSTAT */			/* 28 */
	SYSG(e_sysv_pause, 0),	/* PAUSE */			/* 29 */
	sysg(e_utimes, 2),	/* UTIME */			/* 30 */
	SYSG(e_sysv_stty, ?),	/* STTY */			/* 31 */
	SYSG(e_sysv_gtty, ?),	/* GTTY */			/* 32 */
	sysg(e_access, 2),					/* 33 */
	sysg(e_sysv_nice, 1),	/* NICE */			/* 34 */
	SYSG(e_linux_ftime, ?),					/* 35 */
	sysg(e_sync, 0),					/* 36 */
	sysg(e_linux_kill, 2),					/* 37 */
	sysg(e_rename, 2),					/* 38 */
	sysg(e_mkdir, 1),					/* 39 */
	sysg(e_rmdir, 1),					/* 40 */
	sysg(e_dup, 1),						/* 41 */
	sysg(e_pipe, 1),	/* !=BSD: No return value */	/* 42 */
	SYSG(e_sysv_times, 0),	/* TIMES */			/* 43 */
	SYSG(e_sysv_prof, ?),	/* PROF */			/* 44 */
	sysg(e_linux_brk, 1),					/* 45 */
	sysg(e_setgid, 1),	/* SETGID */			/* 46 */
	sysg(e_getgid, 0),					/* 47 */
	sysg(e_sysv_signal, 2),	/* SIGNAL */			/* 48 */ 
	sysg(e_geteuid, 0),					/* 49 */
	sysg(e_getegid, 0),					/* 50 */
	sysg(e_sysacct, 1),					/* 51 */
	SYSG(e_linux_phys, ?),					/* 52 */
	SYSG(e_sysv_lock, 0),					/* 53 */
	sysg(e_linux_ioctl, 3),					/* 54 */
	sysg(e_fcntl, 3),					/* 55 */
	SYSG(e_linux_mpx, ?),					/* 56 */
	sysg(e_setpgid, 2),					/* 57 */
	SYSG(e_linux_ulimit, ?),				/* 58 */
	sysg(e_linux_uname, 1),					/* 59 */
	sysg(e_umask, 1),					/* 60 */
	sysg(e_chroot, 1),					/* 61 */
	SYSG(e_linux_ustat, ?),					/* 62 */
	sysg(e_dup2, 2),					/* 63 */
	sysg(e_getppid, 0),					/* 64 */
	sysg(e_getpgrp, 0),					/* 65 */
	sysg(e_setsid, 0),					/* 66 */
	sysg(e_linux_sigaction, 3),				/* 67 */
	SYSG(e_linux_sgetmask, ?),				/* 68 */
	SYSG(e_linux_ssetmask, ?),				/* 69 */
	sysg(e_setreuid, 2),					/* 70 */
	sysg(e_setregid, 2),					/* 71 */
	sysg(e_sigsuspend, 1),					/* 72 */
	sysg(e_sigpending, 1),					/* 73 */
	sysg(e_sethostname, 2),					/* 74 */
	sysg(e_bnr_setrlimit, 2),				/* 75 */
	sysg(e_bnr_getrlimit, 2),				/* 76 */
	sysg(e_getrusage, 2),					/* 77 */
	sysg(e_mapped_gettimeofday, 2),				/* 78 */
	sysg(e_settimeofday, 2),				/* 79 */
	sysg(e_getgroups, 2),					/* 80 */
	sysg(e_setgroups, 2),					/* 81 */
	sysg(e_linux_select, 1),				/* 82 */
	sysg(e_symlink, 2),					/* 83 */
	SYSG(e_linux_lstat, ?),					/* 84 */
	sysg(e_readlink, 3),					/* 85 */
	sysg(e_linux_uselib, 1),				/* 86 */
	sysg(e_swapon, 1),					/* 87 */
	sysg(e_linux_reboot, 3),				/* 88 */
	sysg(e_linux_readdir, 3),			        /* 89 */
	sysg(e_linux_mmap, 1),	/* args in buffer */		/* 90 */
	sysg(e_munmap, 2),			 		/* 91 */
	sysg(e_bnr_truncate, 2),				/* 92 */
	sysg(e_bnr_ftruncate, 2),				/* 93 */
	sysg(e_fchmod, 2),					/* 94 */
	sysg(e_fchown, 3),					/* 95 */
	sysg(e_getpriority, 2),					/* 96 */
	sysg(e_setpriority, 3),					/* 97 */
	SYSG(e_linux_profil, ?),	       			/* 98 */
	sysg(e_sysv_statfs, 2),					/* 99 */
	sysg(e_sysv_fstatfs, 2),				/* 100 */
	SYSG(e_linux_ioperm, ?),				/* 101 */
	sysg(e_linux_socketcall, 2),				/* 102 */
	SYSG(e_linux_syslog, ?),				/* 103 */
	sysg(e_getitimer, 2),					/* 104 */
	sysg(e_setitimer, 3),					/* 105 */
	sysg(e_linux_stat, 2),					/* 106 */
	sysg(e_linux_lstat, 2),					/* 107 */
	sysg(e_linux_fstat, 2),					/* 108 */
	sysg(e_linux_uname, 1),		/* new uname */		/* 109 */
	SYSG(e_linux_iopl, ?),					/* 110 */
	sysg(e_vhangup, 0),					/* 111 */
	SYSG(e_linux_idle, ?),					/* 112 */
	SYSG(e_linux_vm86, ?),					/* 113 */
	sysg(e_linux_wait4, 4),					/* 114 */
	SYSG(e_linux_swapoff, ?),				/* 115 */
	SYSG(e_linux_sysinfo, ?),				/* 116 */
	sysg(e_linux_sysvipc, 5),				/* 117 */
	sysg(e_fsync, 1),					/* 118 */
	sysg(e_linux_sigreturn, 1),				/* 119 */
	sysg(e_setdomainname, 2),				/* 120 */
	sysg(e_linux_uname, 1),		/* old iuname */	/* 121 */
	sysg(e_linux_uname, 1),					/* 122 */
	SYSG(e_linux_modify_ldt, ?),				/* 123 */
	SYSG(e_linux_adjtimex, ?),				/* 124 */
	sysg(e_mprotect, 3),					/* 125 */
	sysg(e_linux_sigprocmask, 3),				/* 126 */
	SYSG(e_linux_create_module, ?),				/* 127 */
	SYSG(e_linux_init_module, ?),				/* 128 */
	SYSG(e_linux_delete_module, ?),				/* 129 */
	SYSG(e_linux_get_kernel_syms, ?),			/* 130 */
	sysg(e_quotactl, 4),					/* 131 */
	SYSG(e_sysv_getpgid, ?),				/* 132 */
	sysg(e_fchdir, 1),					/* 133 */
	SYSG(e_linux_bdflush, ?),				/* 134 */
};

int	e_linux_nsysent = sizeof(e_linux_sysent)/sizeof(struct sysent);
