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
 * 	Utah $Hdr: e_hpux_sysent.c 1.1 95/09/01$
 */

/*
 *	System call switch table for HP-UX 9.X.
 *	It is specific to the hp700/800 line but should be close
 *	for 300/400s as well.
 */

#include <e_defs.h>
#include <syscall_table.h>

errno_t	e_access(), e_bind(), e_chdir(), e_chmod(), e_chown(),
	e_chroot(), e_close(), e_connect(), e_creat(), e_fchdir(),
	e_fchmod(), e_fchown(), e_fsync(), e_getdtablesize(),
	e_getgroups(), e_gethostid(), e_getitimer(), e_hpux_mmap(),
	e_link(), e_listen(), e_madvise(), e_mapped_gettimeofday(),
	e_mkdir(), e_mknod(), e_mprotect(), e_munmap(), e_nosys(),
	e_obreak(), e_pipe(), e_recv(), e_rename(), e_revoke(),
	e_rmdir(), e_select(), e_send(), e_setgid(), e_setgroups(),
	e_sethostid(), e_setitimer(), e_setregid(), e_setreuid(),
	e_settimeofday(), e_setuid(), e_shmat(), e_shmctl(),
	e_shmdt(), e_shmget(), e_shutdown(), e_sigaction(),
	e_sigpause(), e_sigpending(), e_sigprocmask(), e_sigsuspend(),
	e_socket(), e_socketpair(), e_symlink(), e_sync(), e_table(),
	e_umask(), e_unlink(), e_vhangup();

errno_t	e_hpux_fork(), e_hpux_sigreturn();

errno_t e_bnr_accept(), e_bnr_ftruncate(), e_bnr_getdirentries(),
	e_bnr_getpeername(), e_bnr_getrlimit(), e_bnr_getsockname(),
	e_bnr_lseek(), e_bnr_recvfrom(), e_bnr_recvmsg(),
	e_bnr_sendmsg(), e_bnr_sendto(), e_bnr_setrlimit(),
	e_bnr_truncate();

errno_t e_hpux_alarm(), e_hpux_dup(), e_hpux_dup2(), e_hpux_execv(),
	e_hpux_execve(), e_hpux_fstat(), e_hpux_fstatfs(),
	e_hpux_ftime(), e_hpux_getdomainname(), e_hpux_getgid(),
	e_hpux_getpgrp2(), e_hpux_getpid(), e_hpux_getrusage(),
	e_hpux_getsockopt(), e_hpux_getuid(), e_hpux_kill(),
	e_hpux_lockf(), e_hpux_lstat(), e_hpux_mpctl(), e_hpux_nice(),
	e_nosys(), e_hpux_poll(), e_hpux_rtprio(), e_hpux_setdomainname(),
	e_hpux_setpgrp(), e_hpux_setpgrp2(), e_hpux_setsockopt(),
	e_hpux_sigblock(), e_hpux_sigpause(), e_hpux_sigsetmask(),
	e_hpux_sigsetreturn(), e_hpux_sigsetstatemask(), e_hpux_sigvec(),
	e_hpux_stat(), e_hpux_statfs(), e_hpux_stime(), e_hpux_sysconf(),
	e_hpux_time(), e_hpux_ulimit(), e_hpux_utime(), e_hpux_utssys(),
	e_hpux_wait(), e_hpux_wait3(), e_hpux_waitpid(), e_hpux_open(),
	e_hpux_ioctl(), e_hpux_fcntl(), e_hpux_times(), e_hpux_read(),
	e_hpux_readv(), e_hpux_sigstack(), e_hpux_write(),
	e_hpux_writev();

errno_t	e_hpux_semctl(), e_hpux_semget(), e_hpux_semop(), e_hpux_shmat(),
	e_hpux_shmctl(), e_hpux_shmctl_old(), e_hpux_shmdt(), e_hpux_shmget();

#define SYS_NULL(routine)	{ E_GENERIC, e_nosys, 0, # routine }
#define sysg(routine, nargs)    { nargs, (int (*)()) routine, 0, # routine }
#define SYS_REG(routine)	{ E_CHANGE_REGS, routine, 0, # routine }

struct sysent e_hpux_sysent[] = {
	SYS_NULL(e_indir),				/* 0 */
	sysg(exit, 1),					/* 1 */
	SYS_REG(e_hpux_fork),				/* 2 */
	sysg(e_hpux_read, 3),				/* 3 */
	sysg(e_hpux_write, 3),				/* 4 */
	sysg(e_hpux_open, 3),				/* 5 */
	sysg(e_close, 1),				/* 6 */
	sysg(e_hpux_wait, 1),				/* 7 */
	sysg(e_creat, 2),				/* 8 */
	sysg(e_link, 2),				/* 9 */
	sysg(e_unlink, 1),				/* 10 */
	sysg(e_hpux_execv, 2),				/* 11 */
	sysg(e_chdir, 1),				/* 12 */
	sysg(e_hpux_time, 1),				/* 13 */
	sysg(e_mknod, 3),				/* 14 */
	sysg(e_chmod, 2),				/* 15 */
	sysg(e_chown, 3),				/* 16 */
	sysg(e_obreak, 1),				/* 17 */
	sysg(e_nosys, 0),				/* 18 */
	sysg(e_bnr_lseek, 3),				/* 19 */
	sysg(e_hpux_getpid, 0),				/* 20 */
	sysg(e_nosys, 0),				/* 21 is mount */
	sysg(e_nosys, 0),				/* 22 is unmount */
	sysg(e_setuid, 1),				/* 23 */
	sysg(e_hpux_getuid, 0),				/* 24 */
	sysg(e_hpux_stime, 1),				/* 25 */
	sysg(e_nosys, 0),				/* 26 is ptrace */
	sysg(e_hpux_alarm, 1),				/* 27 */
	sysg(e_nosys, 0),				/* 28 */
	sysg(e_sigpause, 1),				/* 29 */
	sysg(e_hpux_utime, 2),				/* 30 */
	sysg(e_nosys, 0),				/* 31 is stty */
	sysg(e_nosys, 0),				/* 32 is gtty */
	sysg(e_access, 2),				/* 33 */
	sysg(e_hpux_nice, 1),				/* 34 */
	sysg(e_hpux_ftime, 1),				/* 35 */
	sysg(e_sync, 0),				/* 36 */
	sysg(e_hpux_kill, 2),				/* 37 */
	sysg(e_hpux_stat, 2),				/* 38 */
	sysg(e_hpux_setpgrp, 1),			/* 39 */
	sysg(e_hpux_lstat, 2),				/* 40 */
	sysg(e_hpux_dup, 1),				/* 41 */
	sysg(e_pipe, 0),	/* two return values	   42 */
	sysg(e_hpux_times, 1),				/* 43 */
	sysg(e_nosys, 0),				/* 44 is profil */
	sysg(e_nosys, 0),				/* 45 is ki_call */
	sysg(e_setgid, 1),				/* 46 */
	sysg(e_hpux_getgid, 0),				/* 47 */
	sysg(e_nosys, 0),				/* 48 */
	sysg(e_nosys, 0),				/* 49 */
	sysg(e_nosys, 0),				/* 50 */
	sysg(e_nosys, 0),				/* 51 is acct */
	sysg(e_nosys, 0),				/* 52 */
	sysg(e_nosys, 0),				/* 53 */
	sysg(e_hpux_ioctl, 3),				/* 54 */
	sysg(e_nosys, 0),				/* 55 is reboot */
	sysg(e_symlink, 2),				/* 56 */
	sysg(e_hpux_utssys, 3),				/* 57 */
	sysg(e_readlink, 3),				/* 58 */
	sysg(e_hpux_execve, 3),				/* 59 */
	sysg(e_umask, 1),				/* 60 */
	sysg(e_chroot, 1),				/* 61 */
	sysg(e_hpux_fcntl, 3),				/* 62 */
	sysg(e_hpux_ulimit, 2),				/* 63 */
	sysg(e_nosys, 0),				/* 64 */
	sysg(e_nosys, 0),				/* 65 */
	SYS_REG(e_hpux_fork),				/* 66 */
	sysg(e_nosys, 0),				/* 67 */
	sysg(e_nosys, 0),				/* 68 */
	sysg(e_nosys, 0),				/* 69 */
	sysg(e_nosys, 0),				/* 70 */
	sysg(e_hpux_mmap, 6),				/* 71 */
	sysg(e_nosys, 0),				/* 72 */
	sysg(e_munmap, 2),				/* 73 */
	sysg(e_mprotect, 3),				/* 74 */
	sysg(e_madvise, 3),				/* 75 */
	sysg(e_vhangup, 0),				/* 76 */
	sysg(e_nosys, 0),				/* 77 is swapoff */
	sysg(e_nosys, 0),				/* 78 */
	sysg(e_getgroups, 2),				/* 79 */
	sysg(e_setgroups, 2),				/* 80 */
	sysg(e_hpux_getpgrp2, 1),			/* 81 */
	sysg(e_hpux_setpgrp2, 2),			/* 82 */
	sysg(e_setitimer, 3),				/* 83 */
	sysg(e_hpux_wait3, 3),				/* 84 */
	sysg(e_nosys, 0),				/* 85 is swapon */
	sysg(e_getitimer, 2),				/* 86 */
	sysg(e_nosys, 0),				/* 87 */
	sysg(e_nosys, 0),				/* 88 */
	sysg(e_nosys, 0),				/* 89 */
	sysg(e_hpux_dup2, 2),				/* 90 */
	sysg(e_nosys, 0),				/* 91 */
	sysg(e_hpux_fstat, 2),				/* 92 */
	sysg(e_select, 5),				/* 93 */
	sysg(e_nosys, 0),				/* 94 */
	sysg(e_fsync, 1),				/* 95 */
	sysg(e_nosys, 0),				/* 96 */
	sysg(e_socket, 3),				/* 97 (old) */
	sysg(e_connect, 3),				/* 98 (old) */
	sysg(e_bnr_accept, 3),				/* 99 (old) */
	sysg(e_nosys, 0),				/* 100 */
	sysg(e_send, 4),				/* 101 (old) */
	sysg(e_recv, 4),				/* 102 (old) */
	sysg(e_nosys, 0),				/* 103 */
	sysg(e_bind, 3),				/* 104 (old) */
	sysg(e_hpux_setsockopt, 5),			/* 105 (old) */
	sysg(e_listen, 2),				/* 106 (old) */
	sysg(e_nosys, 0),				/* 107 */
	sysg(e_hpux_sigvec, 3),				/* 108 */
	sysg(e_hpux_sigblock, 1),			/* 109 */
	sysg(e_hpux_sigsetmask, 1),			/* 110 */
	sysg(e_hpux_sigpause, 1),			/* 111 */
	sysg(e_hpux_sigstack, 2),			/* 112 */
	sysg(e_bnr_recvmsg, 6),				/* 113 (old) */
	sysg(e_bnr_sendmsg, 3),				/* 114 (old) */
	sysg(e_nosys, 0),				/* 115 */
	sysg(e_mapped_gettimeofday, 2),			/* 116 */
	sysg(e_hpux_getrusage, 2),			/* 117 */
	sysg(e_hpux_getsockopt, 5),			/* 118 (old) */
	sysg(e_nosys, 0),				/* 119 */
	sysg(e_hpux_readv, 3),				/* 120 */
	sysg(e_hpux_writev, 3),				/* 121 */
	sysg(e_settimeofday, 2),			/* 122 */
	sysg(e_fchown, 3),				/* 123 */
	sysg(e_fchmod, 2),				/* 124 */
	sysg(e_bnr_recvfrom, 6),			/* 125 (old) */
	sysg(e_setreuid, 2),				/* 126 */
	sysg(e_setregid, 2),				/* 127 */
	sysg(e_rename, 2),				/* 128 */
	sysg(e_bnr_truncate, 2),			/* 129 */
	sysg(e_bnr_ftruncate, 2),			/* 130 */
	sysg(e_nosys, 0),				/* 131 */
	sysg(e_hpux_sysconf, 1),			/* 132 */
	sysg(e_bnr_sendto, 6),				/* 133 (old) */
	sysg(e_shutdown, 2),				/* 134 (old) */
	sysg(e_socketpair, 4),				/* 135 (old) */
	sysg(e_mkdir, 2),				/* 136 */
	sysg(e_rmdir, 1),				/* 137 */
	sysg(e_nosys, 0),				/* 138 */
	SYS_REG(e_hpux_sigreturn),			/* 139 */
	sysg(e_nosys, 0),				/* 140 is setcore */
	sysg(e_bnr_getpeername, 3),			/* 141 (old) */
	sysg(e_gethostid, 0),				/* 142 */
	sysg(e_sethostid, 1),				/* 143 */
	sysg(e_bnr_getrlimit, 2),			/* 144 */
	sysg(e_bnr_setrlimit, 2),			/* 145 */
	sysg(e_nosys, 0),				/* 146 */
	sysg(e_nosys, 0),				/* 147 */
	sysg(e_nosys, 0),				/* 148 is quotactl */
	sysg(e_nosys, 0),				/* 149 */
	sysg(e_bnr_getsockname, 3),			/* 150 (old) */
	sysg(e_nosys, 0),				/* 151 is privgrp */
	sysg(e_hpux_rtprio, 2),				/* 152 */
	sysg(e_nosys, 0),				/* 153 is plock */
	sysg(e_nosys, 0),				/* 154 */
	sysg(e_hpux_lockf, 3),				/* 155 */
	sysg(e_hpux_semget, 3),				/* 156 */
	sysg(e_hpux_semctl, 4),				/* 157 */
	sysg(e_hpux_semop, 3),				/* 158 */
	sysg(e_nosys, 0),				/* 159 is msgget */
	sysg(e_nosys, 0),				/* 160 is msgctl */
	sysg(e_nosys, 0),				/* 161 is msgsnd */
	sysg(e_nosys, 0),				/* 162 is msgrcv */
	sysg(e_hpux_shmget, 3),				/* 163 */
	sysg(e_hpux_shmctl_old, 3),			/* 164 */
	sysg(e_hpux_shmat, 3),				/* 165 */
	sysg(e_hpux_shmdt, 1),				/* 166 */
	sysg(e_nosys, 0),				/* 167 */
	sysg(e_nosys, 0),				/* 168 */
	sysg(e_nosys, 0),				/* 169 */
	sysg(e_nosys, 0),				/* 170 */
	sysg(e_nosys, 0),				/* 171 */
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
	sysg(e_sigprocmask, 3),				/* 185 */
	sysg(e_sigpending, 1),				/* 186 */
	sysg(e_sigsuspend, 1),				/* 187 */
	sysg(e_sigaction, 3),				/* 188 */
	sysg(e_nosys, 0),				/* 189 */
	sysg(e_nosys, 0),				/* 190 is nfssvc */
	sysg(e_nosys, 0),				/* 191 is getfh */
	sysg(e_hpux_getdomainname, 2),			/* 192 */
	sysg(e_hpux_setdomainname, 2),			/* 193 */
	sysg(e_nosys, 0),				/* 194 is asyncd */
	sysg(e_bnr_getdirentries, 4),			/* 195 */
	sysg(e_hpux_statfs, 2),				/* 196 */
	sysg(e_hpux_fstatfs, 2),			/* 197 */
	sysg(e_nosys, 0),				/* 199 is vfsmount */
	sysg(e_nosys, 0),				/* 199 */
	sysg(e_hpux_waitpid, 3),			/* 200 */
	sysg(e_nosys, 0),				/* 201 */
	sysg(e_nosys, 0),				/* 202 */
	sysg(e_nosys, 0),				/* 203 */
	sysg(e_nosys, 0),				/* 204 */
	sysg(e_nosys, 0),				/* 205 */
	sysg(e_nosys, 0),				/* 206 */
	sysg(e_nosys, 0),				/* 207 */
	sysg(e_nosys, 0),				/* 208 */
	sysg(e_nosys, 0),				/* 209 */
	sysg(e_nosys, 0),				/* 210 */
	sysg(e_nosys, 0),				/* 211 */
	sysg(e_nosys, 0),				/* 212 */
	sysg(e_nosys, 0),				/* 213 */
	sysg(e_nosys, 0),				/* 214 */
	sysg(e_nosys, 0),				/* 215 */
	sysg(e_nosys, 0),				/* 216 */
	sysg(e_nosys, 0),				/* 217 */
	sysg(e_nosys, 0),				/* 218 */
	sysg(e_nosys, 0),				/* 219 */
	sysg(e_nosys, 0),				/* 220 */
	sysg(e_nosys, 0),				/* 221 */
	sysg(e_nosys, 0),				/* 222 */
	sysg(e_nosys, 0),				/* 223 */
	sysg(e_hpux_sigsetreturn, 3),			/* 224 */
	sysg(e_hpux_sigsetstatemask, 2),		/* 225 */
	sysg(e_nosys, 0),				/* 226 is bfactl */
	sysg(e_nosys, 0),				/* 227 is cs */
	sysg(e_nosys, 0),				/* 228 is cds */
	sysg(e_nosys, 0),				/* 229 */
	sysg(e_nosys, 0),				/* 230 is pathconf */
	sysg(e_nosys, 0),				/* 231 is fpathconf */
	sysg(e_nosys, 0),				/* 232 */
	sysg(e_nosys, 0),				/* 233 */
	sysg(e_nosys, 0),				/* 234 is nfs_fcntl */
	sysg(e_nosys, 0),				/* 235 is getacl */
	sysg(e_nosys, 0),				/* 236 is fgetacl */
	sysg(e_nosys, 0),				/* 237 is setacl */
	sysg(e_nosys, 0),				/* 238 is fsetacl */
	sysg(e_nosys, 0),				/* 239 is pstat */
	sysg(e_nosys, 0),				/* 240 is getaudid */
	sysg(e_nosys, 0),				/* 241 is setaudid */
	sysg(e_nosys, 0),				/* 242 is getaudproc */
	sysg(e_nosys, 0),				/* 243 is setaudproc */
	sysg(e_nosys, 0),				/* 244 is getevent */
	sysg(e_nosys, 0),				/* 245 is setevent */
	sysg(e_nosys, 0),				/* 246 is audwrite */
	sysg(e_nosys, 0),				/* 247 is audswitch */
	sysg(e_nosys, 0),				/* 248 is audctl */
	sysg(e_nosys, 0),				/* 249 is getaccess */
	sysg(e_nosys, 0),				/* 250 is fsctl */
	sysg(e_nosys, 0),				/* 251 is ulconnect */
	sysg(e_nosys, 0),				/* 252 is ulcontrol */
	sysg(e_nosys, 0),				/* 253 is ulcreate */
	sysg(e_nosys, 0),				/* 254 is uldest */
	sysg(e_nosys, 0),				/* 255 is ulrecv */
	sysg(e_nosys, 0),				/* 256 is ulrecvcn */
	sysg(e_nosys, 0),				/* 257 is ulsend */
	sysg(e_nosys, 0),				/* 258 is ulshutdown */
	sysg(e_nosys, 0),				/* 259 is swapfs */
	sysg(e_nosys, 0),				/* 260 is fss */
	sysg(e_nosys, 0),				/* 261 */
	sysg(e_nosys, 0),				/* 262 */
	sysg(e_nosys, 0),				/* 263 */
	sysg(e_nosys, 0),				/* 264 */
	sysg(e_nosys, 0),				/* 265 */
	sysg(e_nosys, 0),				/* 266 */
	sysg(e_nosys, 0),				/* 267 is tsync */
	sysg(e_getdtablesize, 0),			/* 268 */
	sysg(e_hpux_poll, 3),				/* 269 */
	sysg(e_nosys, 0),				/* 270 is getmsg */
	sysg(e_nosys, 0),				/* 271 is putmsg */
	sysg(e_fchdir, 1),				/* 272 */
	sysg(e_nosys, 0),				/* 253 */
	sysg(e_nosys, 0),				/* 254 */
	sysg(e_bnr_accept, 3),				/* 275 */
	sysg(e_bind, 3),				/* 276 */
	sysg(e_connect, 3),				/* 277 */
	sysg(e_bnr_getpeername, 3),			/* 278 */
	sysg(e_bnr_getsockname, 3),			/* 279 */
	sysg(e_hpux_getsockopt, 5),			/* 280 */
	sysg(e_listen, 2),				/* 281 */
	sysg(e_recv, 4),				/* 282 */
	sysg(e_bnr_recvfrom, 3),			/* 283 */
	sysg(e_bnr_recvmsg, 6),				/* 284 */
	sysg(e_send, 4),				/* 285 */
	sysg(e_bnr_sendmsg, 3),				/* 286 */
	sysg(e_bnr_sendto, 6),				/* 287 */
	sysg(e_hpux_setsockopt, 5),			/* 288 */
	sysg(e_shutdown, 2),				/* 289 */
	sysg(e_socket, 3),				/* 290 */
	sysg(e_socketpair, 4),				/* 291 */
	sysg(e_nosys, 0),				/* 292 */
	sysg(e_nosys, 0),				/* 293 */
	sysg(e_nosys, 0),				/* 294 */
	sysg(e_nosys, 0),				/* 295 */
	sysg(e_nosys, 0),				/* 296 */
	sysg(e_nosys, 0),				/* 297 */
	sysg(e_nosys, 0),				/* 298 is ipccreate */
	sysg(e_nosys, 0),				/* 299 is ipcname */
	sysg(e_nosys, 0),				/* 300 is ipcnamerase */
	sysg(e_nosys, 0),				/* 301 is ipclookup */
	sysg(e_nosys, 0),				/* 302 is ipcselect */
	sysg(e_nosys, 0),				/* 303 is ipcconnect */
	sysg(e_nosys, 0),				/* 304 is ipcrecvcn */
	sysg(e_nosys, 0),				/* 305 is ipcsend */
	sysg(e_nosys, 0),				/* 306 is ipcrecv */
	sysg(e_nosys, 0),				/* 307 is ipcgetnode */
	sysg(e_nosys, 0),				/* 308 is ipcsetnode */
	sysg(e_nosys, 0),				/* 309 is ipccontrol */
	sysg(e_nosys, 0),				/* 310 is ipcshutdown */
	sysg(e_nosys, 0),				/* 311 is ipcdest */
	sysg(e_hpux_semctl, 0),				/* 312 is semctl */
	sysg(e_nosys, 0),				/* 313 is msgctl */
	sysg(e_hpux_shmctl, 3),				/* 314 */
	sysg(e_hpux_mpctl, 3),				/* 315 is mpctl */
	sysg(e_nosys, 0),				/* 316 is exportfs */
	sysg(e_nosys, 0),				/* 317 is getpmsg */
	sysg(e_nosys, 0),				/* 318 is putpmsg */
	sysg(e_nosys, 0),				/* 319 is strioctl */
	sysg(e_nosys, 0),				/* 320 is msync */
	sysg(e_nosys, 0),				/* 321 is msleep */
	sysg(e_nosys, 0),				/* 322 is mwakeup */
	sysg(e_nosys, 0),				/* 323 is msem_init */
	sysg(e_nosys, 0),				/* 324 is msem_remove */
};

int	e_hpux_nsysent = sizeof(e_hpux_sysent)/sizeof(struct sysent);

struct sysent	hpux_sysent_table		= sysg(e_table, 5);
