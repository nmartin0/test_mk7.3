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

char *syscallnames[] = {
	"syscall",		/* 0 = syscall */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",		/* 4 = write */
	"old open",		/* 5 = old open */
	"close",		/* 6 = close */
	"wait4",		/* 7 = wait4 */
	"old creat",		/* 8 = old creat */
	"link",			/* 9 = link */
	"unlink",		/* 10 = unlink */
	"execv",		/* 11 = execv */
	"chdir",		/* 12 = chdir */
	"fchdir",		/* 13 = fchdir */
	"mknod",		/* 14 = mknod */
	"chmod",		/* 15 = chmod */
	"chown",		/* 16 = chown */
	"break",		/* 17 = break */
	"old getfsstat"		/* 18 = old getfsstat */
	"lseek",		/* 19 = lseek */
	"getpid",		/* 20 = getpid */
	"mount",		/* 21 = mount */
	"umount",		/* 22 = umount */
	"setuid",		/* 23 = setuid */
	"getuid",		/* 24 = getuid */
	"exec_with_loader",	/* 25 = exec_with_loader */
	"ptrace",		/* 26 = ptrace */
	"recvmsg",		/* 27 = recvmsg */
	"sendmsg",		/* 28 = sendmsg */
	"recvfrom",		/* 29 = recvfrom */
	"accept",		/* 30 = accept */
	"getpeername",		/* 31 = getpeername */
	"getsockname",		/* 32 = getsockname */
	"access",		/* 33 = access */
	"chflags",		/* 34 = chflags */
	"fchflags",		/* 35 = fchflags */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"old stat",		/* 38 = old stat */
	"setpgid",		/* 39 = setpgid */
	"old lstat",		/* 40 = old lstat */
	"old dup",		/* 41 = old dup */
	"socketpipe",		/* 42 = socketpipe */
	"set_program_attributes",/* 43 = set_program_attributes */
	"profil",		/* 44 = profil */
	"open",			/* 45 = open */
	"sigaction",		/* 46 = sigaction */
	"getgid",		/* 47 = getgid */
	"sigprocmask",		/* 48 = sigprocmask */
	"getlogin",		/* 49 = getlogin */
	"setlogin",		/* 50 = setlogin */
	"acct",			/* 51 = acct */
	"sigpending",		/* 52 = sigpending */
	"dup",			/* 53 = dup */
	"ioctl",		/* 54 = ioctl */
	"reboot",		/* 55 = reboot */
	"revoke",		/* 56 = revoke */
	"symlink",		/* 57 = symlink */
	"readlink",		/* 58 = readlink */
	"execve",		/* 59 = execve */
	"umask",		/* 60 = umask */
	"chroot",		/* 61 = chroot */
	"old fstat",		/* 62 = old fstat */
	"getpgrp",		/* 63 = getpgrp */
	"getpagesize",		/* 64 = getpagesize */
	"mremap",		/* 65 = mremap */
	"vfork",		/* 66 = vfork */
	"stat",			/* 67 = stat */
	"lstat",		/* 68 = lstat */
	"sbrk",			/* 69 = sbrk */
	"sstk",			/* 70 = sstk */
	"mmap",			/* 71 = mmap */
	"ovadvise",		/* 72 = ovadvise */
	"munmap",		/* 73 = munmap */
	"mprotect",		/* 74 = mprotect */
	"madvise",		/* 75 = madvise */
	"old vhangup",		/* 76 = old vhangup */
	"obs old",		/* 77 = obsolete old kmodcall */
	"mincore",		/* 78 = mincore */
	"getgroups",		/* 79 = getgroups */
	"setgroups",		/* 80 = setgroups */
	"old getpgrp",		/* 81 = old getpgrp */
	"setpgrp",		/* 82 = setpgrp */
	"setitimer",		/* 83 = setitimer */
	"old wait",		/* 84 = old wait */
	"table",		/* 85 = table */
	"getitimer",		/* 86 = getitimer */
	"gethostname",		/* 87 = gethostname */
	"sethostname",		/* 88 = sethostname */
	"old getdtablesize",	/* 89 = old getdtablesize */
	"dup2",			/* 90 = dup2 */
	"fstat",		/* 91 = fstat */
	"fcntl",		/* 92 = fcntl */
	"select",		/* 93 = select */
	"poll",			/* 94 = poll */
	"fsync",		/* 95 = fsync */
	"setpriority",		/* 96 = setpriority */
	"socket",		/* 97 = socket */
	"connect",		/* 98 = connect */
	"old accept",		/* 99 = old accept */
	"getpriority",		/* 100 = getpriority */
	"old send",		/* 101 = old send */
	"old recv",		/* 102 = old recv */
	"sigreturn",		/* 103 = sigreturn */
	"bind",			/* 104 = bind */
	"setsockopt",		/* 105 = setsockopt */
	"listen",		/* 106 = listen */
	"plock",		/* 107 = plock */
	"old sigvec",		/* 108 = old sigvec */
	"old sigblock",		/* 109 = old sigblock */
	"old sigsetmask",	/* 110 = old sigsetmask */
	"sigsuspend",		/* 111 = sigsuspend */
	"sigstack",		/* 112 = sigstack */
	"old recvmsg",		/* 113 = old recvmsg */
	"old sendmsg",		/* 114 = old sendmsg */
	"obs vtrace",		/* 115 = obsolete vtrace */
	"gettimeofday",		/* 116 = gettimeofday */
	"getrusage",		/* 117 = getrusage */
	"getsockopt",		/* 118 = getsockopt */
	"#119",			/* 119 = nosys */
	"readv",		/* 120 = readv */
	"writev",		/* 121 = writev */
	"settimeofday",		/* 122 = settimeofday */
	"fchown",		/* 123 = fchown */
	"fchmod",		/* 124 = fchmod */
	"old recvfrom",		/* 125 = old recvfrom */
	"setreuid",		/* 126 = setreuid */
	"setregid",		/* 127 = setregid */
	"rename",		/* 128 = rename */
	"truncate",		/* 129 = truncate */
	"ftruncate",		/* 130 = ftruncate */
	"flock",		/* 131 = flock */
	"setgid",		/* 132 = setgid */
	"sendto",		/* 133 = sendto */
	"shutdown",		/* 134 = shutdown */
	"socketpair",		/* 135 = socketpair */
	"mkdir",		/* 136 = mkdir */
	"rmdir",		/* 137 = rmdir */
	"utimes",		/* 138 = utimes */
	"obs 4.2",		/* 139 = obsolete 4.2 sigreturn */
	"adjtime",		/* 140 = adjtime */
	"old getpeername",	/* 141 = old getpeername */
	"gethostid",		/* 142 = gethostid */
	"sethostid",		/* 143 = sethostid */
	"getrlimit",		/* 144 = getrlimit */
	"setrlimit",		/* 145 = setrlimit */
	"old killpg",		/* 146 = old killpg */
	"setsid",		/* 147 = setsid */
	"quotactl",		/* 148 = quotactl */
	"oldquota",		/* 149 = oldquota */
	"old getsockname",	/* 150 = old getsockname */
	"#151",			/* 151 = nosys */
	"#152",			/* 152 = nosys */
	"#153",			/* 153 = nosys */
	"#154",			/* 154 = nosys */
	"nfssvc",		/* 155 = nfssvc */
	"getdirentries",	/* 156 = getdirentries */
	"statfs",		/* 157 = statfs */
	"fstatfs",		/* 158 = fstatfs */
	"obs vfsunmount",	/* 159 = obsolete vfsunmount */
	"async_daemon",		/* 160 = async_daemon */
	"getfh",		/* 161 = getfh */
	"getdomainname",	/* 162 = getdomainname */
	"setdomainname",	/* 163 = setdomainname */
	"#164",			/* 164 = nosys */
	"obs quotactl",		/* 165 = obsolete quotactl */
	"obs exportfs",		/* 166 = obsolete exportfs */
	"obs vfsmount",		/* 167 = obsolete vfsmount */
	"#168",			/* 168 = nosys */
	"#169",			/* 169 = nosys */
	"#170",			/* 170 = nosys */
	"#171",			/* 171 = nosys */
	"#172",			/* 172 = nosys */
	"#173",			/* 173 = nosys */
	"#174",			/* 174 = nosys */
	"#175",			/* 175 = nosys */
	"#176",			/* 176 = nosys */
	"#177",			/* 177 = nosys */
	"#178",			/* 178 = nosys */
	"#179",			/* 179 = nosys */
	"#180",			/* 180 = nosys */
	"#181",			/* 181 = nosys */
	"#182",			/* 182 = nosys */
	"#183",			/* 183 = nosys */
	"#184",			/* 184 = nosys */
	"#185",			/* 185 = nosys */
	"#186",			/* 186 = nosys */
	"#187",			/* 187 = nosys */
	"#188",			/* 188 = nosys */
	"#189",			/* 189 = nosys */
	"#190",			/* 190 = nosys */
	"#191",			/* 191 = nosys */
	"getfsstat",		/* 192 = getfsstat */
	"#193",			/* 193 = nosys */
	"#194",			/* 194 = nosys */
	"#195",			/* 195 = nosys */
	"#196",			/* 196 = nosys */
	"#197",			/* 197 = nosys */
	"#198",			/* 198 = nosys */
	"swapon",		/* 199 = swapon */
	"#200",			/* 200 = nosys */
	"msgget",		/* 201 = msgget */
	"msgrcv",		/* 202 = msgrcv */
	"msgsnd",		/* 203 = msgsnd */
	"#204",			/* 204 = nosys */
	"semget",		/* 205 = semget */
	"semop",		/* 206 = semop */
	"uname",		/* 207 = uname */
	"lchown",		/* 208 = lchown */
	"shmat",		/* 209 = shmat */
	"#210",			/* 210 = nosys */
	"shmdt",		/* 211 = shmdt */
	"shmget",		/* 212 = shmget */
	"mvalid",		/* 213 = mvalid */
	"getaddressconf",	/* 214 = getaddressconf */
	"msleep",		/* 215 = msleep */
	"mwakeup",		/* 216 = mwakeup */
	"msync",		/* 217 = msync */
	"signal",		/* 218 = signal */
	"utc_gettime",		/* 219 = utc_gettime */
	"utc_adjtime",		/* 220 = utc_adjtime */
	"syscall_num",		/* 221 = syscall_num */
	"security",		/* 222 = security */
	"kloadcall",		/* 223 = kloadcall */
	"#224",			/* 224 = nosys */
	"#225",			/* 225 = nosys */
	"#226",			/* 226 = nosys */
	"#227",			/* 227 = nosys */
	"#228",			/* 228 = nosys */
	"#229",			/* 229 = nosys */
	"#230",			/* 230 = nosys */
	"#231",			/* 231 = nosys */
	"#232",			/* 232 = nosys */
	"afs_syscall",		/* 233 = afs_syscall */
	"getpgid",		/* 234 = getpgid */
	"getsid",		/* 235 = getsid */
	"msgctl",		/* 236 = msgctl */
	"semctl",		/* 237 = semctl */
	"shmctl",		/* 238 = shmctl */
	"sigsend",		/* 239 = sigsend */
	"sigsendset",		/* 240 = sigsendset */
	"#241",			/* 241 = nosys */
	"#242",			/* 242 = nosys */
	"#243",			/* 243 = nosys */
	"#244",			/* 244 = nosys */
	"#245",			/* 245 = nosys */
	"#246",			/* 246 = nosys */
	"#247",			/* 247 = nosys */
	"#248",			/* 248 = nosys */
	"#249",			/* 249 = nosys */
	"getprocthreads",	/* 250 = getprocthreads */
	"#251",			/* 251 = nosys */
	"#252",			/* 252 = nosys */
	"#253",			/* 253 = nosys */
	"debug_report_load",	/* 254 = debug_report_load */
	"debug_report_unload",	/* 255 = debug_report_unload */
	"etap_info",		/* 256 = etap_info */
	"etap_map",		/* 257 = etap_map */
	"#258",			/* 258 = nosys */
	"subsys_info",		/* 259 = subsys_info */
	"kmodcall",		/* 260 = kmodcall */
	"uxthread_start",	/* 261 = uxthread_start */
	"uxthread_stop",	/* 262 = uxthread_stop */
	"uxthread_self",	/* 263 = uxthread_self */
	"uxthread_yield",	/* 264 = uxthread_yield */
	"uxthread_terminate",	/* 265 = uxthread_terminate */
	"uxthread_post",	/* 266 = uxthread_post */
	"uxthread_wait",	/* 267 = uxthread_wait */
};

#define N_SYSCALL_NAMES (sizeof syscallnames / sizeof syscallnames[0])
#define syscall_name(nu) (((nu) < N_SYSCALL_NAMES) ? syscallnames[nu] : NULL)
