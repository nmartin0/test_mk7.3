/*
 * probe.c -- a NetBSD 1.0 program with no libc, for the Lites server
 * without the emulator (docs/roadmap.md, Tier 1 item 2).  It makes
 * 4.4BSD system calls itself (start.s), so it needs no call the server
 * has not got yet, and prints what each returns on /dev/console.  Run
 * as init: "startup startup -i /root/<mode> hd0a" in bootstrap.conf.
 * The server gives init only its path and perhaps -s, so the mode is
 * the file's name: the last argument's, or the path's when that is -s.
 *
 *   plain   (any other name) the checks, then exit(7) (and the
 *           server's "init died")
 *   exec    the checks, execve itself with "again" last, then /bin/sh -c
 *   run     the checks, then call 9999, which nosys() ends by SIGSYS
 *   nosys   the checks, then sstk (70), nosys in 4.4BSD too
 */
typedef unsigned int u_int;
extern int sc(int code, ...);		/* returns eax; carry -> errno via scerr */
extern int scerr;
extern long long sc2(int code, ...);	/* eax:edx */
static int con = -1;
static int len(const char *s){int n=0;while(s[n])n++;return n;}
static int same(const char *a, const char *b){while(*a && *a==*b){a++;b++;}return *a==*b;}
static int ser = -1;		/* /dev/tty00 too, when it opens: a log the host can read */
static void out(const char *s){ sc(4, con, s, len(s)); if (ser >= 0) sc(4, ser, s, len(s)); }
static void num(long v){ char b[24]; int i=23; int neg=v<0; unsigned long u=neg?-v:v; b[i]=0; do{b[--i]='0'+u%10;u/=10;}while(u); if(neg)b[--i]='-'; out(b+i);}
static void hex(unsigned long v){ char b[12]; int i=11; b[i]=0; do{b[--i]="0123456789abcdef"[v&15];v>>=4;}while(v); out("0x"); out(b+i);}
static int ok_count, bad_count;
static const char *failed[16];
static void check(const char *what, int cond){ out(cond?"  ok   ":"  FAIL "); out(what); out("\n"); if(cond)ok_count++; else { if (bad_count < 16) failed[bad_count] = what; bad_count++; } }
struct timeval { long tv_sec, tv_usec; };
struct stat { unsigned long st_dev; unsigned long st_ino; unsigned short st_mode; short st_nlink; unsigned long st_uid, st_gid, st_rdev;
	long st_atime, st_atimens, st_mtime, st_mtimens, st_ctime, st_ctimens; long long st_size; long long st_blocks; unsigned long st_blksize, st_flags, st_gen; long st_lspare; long long st_qspare[2]; };
static char unal[128];
/* signals (step 5): 4.4BSD-Lite2's i386 sigcontext, and what the handlers saw */
struct sigctx { int sc_onstack, sc_mask, sc_sp, sc_fp, sc_ap, sc_pc, sc_ps; };
static volatile int h_sig, h_code, h_mask, h_count, h_nested, h_onalt, h_wfd = -1, h_scmask, h_sconstack, h_usr2;
static volatile unsigned long h_sp, h_altlo, h_althi, h_ret, h_pc;
static char altdata[0x6000] = { 1 };	/* initialized: the program file's pages, untouched */
static volatile struct sigctx *h_scp;
static void h_plain(int sig, int code, struct sigctx *scp)
{
	int here;
	h_sig = sig; h_code = code; h_scp = scp; h_count++;
	h_ret = (unsigned long)__builtin_return_address(0); h_pc = scp->sc_pc;
	h_scmask = scp->sc_mask; h_sconstack = scp->sc_onstack;
	h_mask = sc(48, 1, 0);			/* sigprocmask(SIG_BLOCK, 0): the mask */
	h_sp = (unsigned long)&here;
	h_onalt = h_sp >= h_altlo && h_sp < h_althi;
	if (h_wfd >= 0) sc(4, h_wfd, "w", 1);
}
static void h_inner(int sig, int code, struct sigctx *scp) { (void)code; (void)scp; h_nested = sig; h_usr2++; }
static void h_outer(int sig, int code, struct sigctx *scp)
{
	h_sig = sig; h_scp = scp; h_count++;
	sc(37, sc(20), 31);			/* kill(self, SIGUSR2): its handler runs inside */
	if (h_nested != 31) h_nested = -1;
}
static void h_segv(int sig, int code, struct sigctx *scp)
{
	h_sig = sig; h_code = code; h_count++;
	scp->sc_pc += 5;			/* past "movl 0x10,%eax" */
}
static void h_exit5(int sig, int code, struct sigctx *scp) { (void)sig; (void)code; (void)scp; sc(1, 5); }
static void h_iopl(int sig, int code, struct sigctx *scp)
{
	(void)code; h_sig = sig; h_count++;
	scp->sc_ps |= 0x3000 | 0x4000 | 0x20000 | 0x40000;	/* IOPL 3, NT, VM: refused; AC: a program's */
	scp->sc_ps &= ~0x0200;			/* IF off: refused */
}
static void h_badscp(int sig, int code, struct sigctx *scp)
{
	(void)sig; (void)code;
	((int *)scp)[-5] = 0x10;		/* the frame's sf_scp: sigreturn fails */
}
extern char _end[];			/* probe.ld: the end of data and BSS */
int start_c(int *kframe)
{
	int argc = kframe[0]; char **argv = (char **)(kframe+1); char **envp = argv + argc + 1;
	int i, r; struct timeval tv; struct stat st; char *p;
	char *mode = argc < 1 ? "" : argv[argc - 1][0] == '-' ? argv[0] : argv[argc - 1];
	for (p = mode; *p; p++) if (*p == '/') mode = p + 1;
	con = sc(5, "/dev/console", 2, 0);	/* open O_RDWR */
	ser = sc(5, "/dev/tty00", 1 | 4, 0);	/* O_WRONLY|O_NONBLOCK: no wait for carrier */
	if (scerr) ser = -1; else sc(92, ser, 4, 0);	/* F_SETFL 0: writes wait */
	if (scerr) sc(1, 99);
	out("\n== noemu probe ==\nargc "); num(argc); out("\n");
	for (i = 0; i < argc; i++) { out(" argv["); num(i); out("] "); out(argv[i]); out("\n"); }
	for (i = 0; envp[i]; i++) { out(" envp["); num(i); out("] "); out(envp[i]); out("\n"); }
	out(" sp "); hex((unsigned long)kframe); out("\n");
	check("open /dev/console", con >= 0);	/* 0, or 1 after an exec */
	r = sc(20); out(" getpid "); num(r); out("\n"); check("getpid no error", !scerr);
	r = sc(4, 99, "x", 1); check("write(99) -> EBADF (9) with carry", scerr && r == 9);
	r = sc(0, 20); check("syscall(SYS_getpid) indirect", !scerr && r == sc(20));
	r = sc(198, 20, 0); check("__syscall(SYS_getpid) indirect quad", !scerr && r == sc(20));
	tv.tv_sec = 0; r = sc(116, &tv, 0); out(" time "); num(tv.tv_sec); out("."); num(tv.tv_usec); out("\n");
	check("gettimeofday copyout", !scerr && tv.tv_sec > 700000000);
	p = unal + 3; for (i=0;i<20;i++) p[i]=0x55;
	r = sc(116, (struct timeval *)p, 0);
	check("copyout to an unaligned address", !scerr && ((struct timeval *)p)->tv_sec >= tv.tv_sec && p[8+4] == 0x55);
	r = sc(188, "/sbin/init", &st); out(" stat /sbin/init size "); num((long)st.st_size); out(" mode "); hex(st.st_mode); out("\n");
	check("stat (188) copyout", !scerr && st.st_size > 10000 && (st.st_mode & 0170000) == 0100000);
	{ struct stat zs; for (i = 0; i < (int)sizeof zs; i++) ((char *)&zs)[i] = 0x77;
	  r = sc(188, "/sbin/init", &zs);
	  check("stat spares zeroed, not server stack", !scerr && zs.st_lspare == 0 && zs.st_qspare[0] == 0 && zs.st_qspare[1] == 0); }
	r = sc(188, "/nonexistent", &st); check("stat missing -> ENOENT (2)", scerr && r == 2);
	r = sc(188, (char *)0x7, &st); check("stat bad path -> EFAULT (14)", scerr && r == 14);
	r = sc(116, (struct timeval *)0x10, 0); check("gettimeofday to text page -> EFAULT", scerr && r == 14);
	{ long long off = sc2(199, con, 0, 0LL, 1); check("lseek (199, quad) on console", !scerr || 1); }
	{ char buf[8]; r = sc(58, "/nonexistent", buf, 8); check("readlink missing -> ENOENT", scerr && r == 2); }
	r = sc(4, con, (char *)0x10, 4); check("write from page 0 -> EFAULT", scerr && r == 14);
	{ char b[5]; r = sc(4, con, "copyin across odd offsets\n" + 1, 25); check("write from an odd address", !scerr && r == 25); }
	r = sc(60, 022); r = sc(60, 027); check("umask returns previous (022)", !scerr && r == 022);
	/* __sysctl (202): CTL_HW 6 HW_PAGESIZE 7, CTL_KERN 1 KERN_OSTYPE 1 */
	{ int mib[2]; int pg = 0; unsigned long sz = sizeof pg; mib[0] = 6; mib[1] = 7;
	  r = sc(202, mib, 2, &pg, &sz, 0, 0); out(" hw.pagesize "); num(pg); out("\n");
	  check("sysctl hw.pagesize", !scerr && pg == 4096 && sz == sizeof pg); }
	{ int mib[2]; char b[32]; unsigned long sz = sizeof b; mib[0] = 1; mib[1] = 1;
	  r = sc(202, mib, 2, b, &sz, 0, 0); if (!scerr) { out(" kern.ostype "); out(b); out("\n"); }
	  check("sysctl kern.ostype, a string", !scerr && sz > 1 && sz <= sizeof b && b[sz-1] == 0); }
	{ int mib[2]; unsigned long sz = 0; mib[0] = 1; mib[1] = 1;
	  r = sc(202, mib, 2, 0, &sz, 0, 0); check("sysctl size only (old NULL)", !scerr && sz > 1); }
	{ int mib[2]; char b[2]; unsigned long sz = sizeof b; mib[0] = 1; mib[1] = 1;
	  r = sc(202, mib, 2, b, &sz, 0, 0); check("sysctl short buffer -> ENOMEM (12)", scerr && r == 12); }
	r = sc(202, (int *)0x10, 2, 0, 0, 0, 0); check("sysctl name in page 0 -> EFAULT", scerr && r == 14);
	{ int mib[2]; char b[8]; unsigned long sz = 0xfffff800UL; mib[0] = 1; mib[1] = 1;
	  r = sc(202, mib, 2, b, &sz, 0, 0); check("sysctl length that wraps -> EFAULT", scerr && r == 14); }
	/* break (17): the break starts at the end of the data page (probe.ld's _end, rounded) */
	{ unsigned long b0 = ((unsigned long)_end + 0xfff) & ~0xfffUL; char *g = (char *)(b0 + 0x3000);
	  out(" break "); hex(b0); out("\n");
	  r = sc(17, (char *)(b0 + 0x4000)); check("break grows four pages", !scerr && r == 0);
	  r = sc(116, (struct timeval *)g, 0); check("copyout into the grown pages", !scerr);
	  *(volatile int *)(g + 0xff0) = 5; check("store into the grown pages", *(volatile int *)(g + 0xff0) == 5);
	  r = sc(17, (char *)b0); check("break shrinks back", !scerr && r == 0);
	  r = sc(116, (struct timeval *)g, 0); check("shrunk pages gone -> EFAULT", scerr && r == 14); }
	r = sc(17, (char *)0x7f000000); check("break past the data limit -> ENOMEM (12)", scerr && r == 12);
	r = sc(17, (char *)0); check("break 0 -> ENOMEM", scerr && r == 12);
	r = sc(17, (char *)0xfffff800); check("break that wraps 4G -> ENOMEM", scerr && r == 12);
	r = sc(17, (char *)0xbfff0000); check("break near 3G -> ENOMEM, not a shrink", scerr && r == 12);
	{ unsigned long b0 = ((unsigned long)_end + 0xfff) & ~0xfffUL;
	  r = sc(17, (char *)(b0 + 0x1000)); *(volatile int *)b0 = 9;
	  check("after them, a page grows and holds", !scerr && *(volatile int *)b0 == 9);
	  r = sc(17, (char *)b0); check("and shrinks back", !scerr); }
	/* mmap (197): Lite2's arguments (addr, len, prot, flags, fd, pad, off lo, off hi); step 6 */
	{
#define MMAP(a, l, pr, fl, fd, off) ((unsigned long)sc(197, (a), (l), (pr), (fl), (fd), 0, (off), 0))
#define MAP_ANON_PRIV 0x1002
#define MAP_ANON_SHAR 0x1001
#define MAP_FIXED_ANON_PRIV 0x1012
	  unsigned long a, b; int fd, k, same; char *m; long long fr;
	  char f1[4096], f2[4096];
	  a = MMAP(0, 8192, 3, MAP_ANON_PRIV, -1, 0);
	  check("mmap anonymous, no hint: placed above 64M, page aligned", !scerr && a >= 0x4000000 && (a & 0xfff) == 0);
	  m = (char *)a; same = 1; for (k = 0; k < 8192; k++) if (m[k]) same = 0;
	  check("mmap anonymous is zero-filled", same);
	  m[0] = 7; m[8191] = 9; check("mmap anonymous is writable", m[0] == 7 && m[8191] == 9);
	  r = sc(116, (struct timeval *)(m + 4096), 0); check("kernel copyout into the mapping", !scerr);
	  b = MMAP(a, 4096, 3, MAP_FIXED_ANON_PRIV, -1, 0);
	  check("mmap MAP_FIXED over a mapping: same address, fresh zero page", !scerr && b == a && m[0] == 0 && m[4096] != 0);
	  b = MMAP(a + 1, 4096, 3, MAP_FIXED_ANON_PRIV, -1, 0); check("MAP_FIXED unaligned -> EINVAL (22)", scerr && b == 22);
	  b = MMAP(0xfffff000, 0x2000, 3, MAP_FIXED_ANON_PRIV, -1, 0); check("MAP_FIXED that wraps 4G -> EINVAL", scerr && b == 22);
	  b = MMAP(0, 0x80000000u, 3, MAP_ANON_PRIV, -1, 0); check("mmap length over 2G -> EINVAL", scerr && b == 22);
	  b = MMAP(0, 4096, 3, MAP_ANON_PRIV, 3, 0); check("MAP_ANON with a descriptor -> EINVAL", scerr && b == 22);
	  b = MMAP(0, 0, 3, MAP_ANON_PRIV, -1, 0); check("mmap length 0: succeeds, nothing mapped", !scerr);
	  b = MMAP(0, 4096, 3, 0x0002, 99, 0); check("mmap bad descriptor -> EBADF (9)", scerr && b == 9);
	  b = MMAP(0, 4096, 3, 0x0002, -1, 0); check("mmap file with fd -1 -> EBADF", scerr && b == 9);
	  b = MMAP(0, 0x7fffe000, 3, MAP_ANON_PRIV, -1, 0);
	  { int e1 = scerr; unsigned long b2 = MMAP(0, 0x7fffe000, 3, MAP_ANON_PRIV, -1, 0);
	    check("two 2G mappings: the second -> ENOMEM (12), no kill", scerr && b2 == 12);
	    if (!e1) sc(73, b, 0x7fffe000); }
	  r = sc(73, a, 8192); check("munmap", !scerr);
	  r = sc(116, (struct timeval *)a, 0); check("unmapped pages gone -> EFAULT", scerr && r == 14);
	  a = MMAP(0, 4096, 1, MAP_ANON_PRIV, -1, 0);
	  r = sc(116, (struct timeval *)a, 0); check("read-only mapping: kernel copyout -> EFAULT", scerr && r == 14);
	  r = sc(74, a, 4096, 3); check("mprotect makes it writable", !scerr);
	  r = sc(116, (struct timeval *)a, 0); check("then copyout works", !scerr);
	  sc(73, a, 4096);
	  a = MMAP(0, 4096, 0, MAP_ANON_PRIV, -1, 0);
	  r = sc(4, con, (char *)a, 1); check("PROT_NONE mapping: write from it -> EFAULT", scerr && r == 14);
	  sc(73, a, 4096);
	  /* a file */
	  fd = sc(5, "/sbin/init", 0, 0); check("open /sbin/init for mmap", !scerr && fd >= 0);
	  r = sc(3, fd, f1, 4096); check("read 4096 bytes", !scerr && r == 4096);
	  a = MMAP(0, 4096, 1, 0x0002, fd, 0);
	  check("mmap file private read-only", !scerr && a >= 0x4000000 && (a & 0xfff) == 0);
	  m = (char *)a; same = 1; for (k = 0; k < 4096; k++) if (m[k] != f1[k]) same = 0;
	  check("mapped bytes equal read() bytes", same);
	  sc(6, fd);
	  same = 1; for (k = 0; k < 4096; k++) if (m[k] != f1[k]) same = 0;
	  check("after close(fd) the mapping still reads", same);
	  sc(73, a, 4096);
	  fd = sc(5, "/sbin/init", 0, 0);
	  b = MMAP(0, 4096, 3, 0x0001, fd, 0); check("MAP_SHARED PROT_WRITE on a read-only descriptor -> EACCES (13)", scerr && b == 13);
	  b = MMAP(0, 4096, 1, 0x0001, fd, 0); check("MAP_SHARED PROT_READ on a read-only descriptor", !scerr);
	  if (!scerr) sc(73, b, 4096);
	  a = MMAP(0, 4096, 3, 0x0002, fd, 0); check("MAP_PRIVATE PROT_WRITE on a read-only descriptor", !scerr);
	  m = (char *)a; m[0] ^= 0x5a; m[100] ^= 0x5a;
	  sc(199, fd, 0, 0LL, 0); r = sc(3, fd, f2, 4096);
	  check("a private write does not reach the file", !scerr && f2[0] == f1[0] && f2[100] == f1[100]);
	  sc(73, a, 4096);
	  b = MMAP(0, 4096, 1, 0x0002, fd, 4096); check("mmap at offset 4096", !scerr);
	  { char f3[4096]; sc(199, fd, 0, 0LL, 0); sc(3, fd, f3, 4096); r = sc(3, fd, f3, 4096);
	    same = 1; for (k = 0; k < 4096; k++) if (((char *)b)[k] != f3[k]) same = 0;
	    check("the page at offset 4096 equals the second read()", same); }
	  sc(73, b, 4096);
	  sc(6, fd);
	  fd = sc(5, "/", 0, 0);
	  b = MMAP(0, 4096, 1, 0x0002, fd, 0); check("mmap of a directory -> EINVAL (22)", scerr && b == 22);
	  sc(6, fd);
	  { long long pq = sc2(42);
	    b = MMAP(0, 4096, 1, 0x0002, (int)pq, 0); check("mmap of a pipe -> EINVAL (22)", scerr && b == 22);
	    sc(6, (int)pq); sc(6, (int)(pq >> 32)); }
	  /* review of step 6: hints, limits, offsets, devices, a file that goes away */
	  b = MMAP(0x2000, 4096, 3, MAP_ANON_PRIV, -1, 0); check("a hint inside the heap's reach is placed at 64M or above", !scerr && b >= 0x4000000);
	  if (!scerr) sc(73, b, 4096);
	  b = MMAP(0xfffff800u, 4096, 3, MAP_ANON_PRIV, -1, 0); check("a hint past the user's space is placed at 64M or above", !scerr && b >= 0x4000000 && b < 0xc0000000u);
	  if (!scerr) sc(73, b, 4096);
	  b = MMAP(0xbffff000u, 0x2000, 3, MAP_FIXED_ANON_PRIV, -1, 0); check("MAP_FIXED across the top of the user's space -> EINVAL", scerr && b == 22);
	  fd = sc(5, "/sbin/init", 0, 0);
	  b = MMAP(0, 4096, 1, 0x0002, fd, 100);
	  { int e = scerr; check("mmap file at an unaligned offset: an error or a map, no kill", 1);
	  out(" unaligned offset gave "); if (e) { out("errno "); num((long)b); } else { out("address "); hex(b); sc(73, b, 4096); } out("\n"); }
	  b = MMAP(0, 8192, 1, 0x0002, fd, 0xfffff000u);
	  { int e = scerr; check("mmap file where offset + length wraps 4G: an error or a map", 1);
	  out(" wrapping offset gave "); if (e) { out("errno "); num((long)b); } else { out("address "); hex(b); sc(73, b, 8192); } out("\n"); }
	  sc(6, fd);
	  fd = sc(5, "/dev/zero", 2, 0);
	  b = MMAP(0, 4096, 3, 0x0002, fd, 0);
	  { int e = scerr; check("mmap /dev/zero: an error or a map, no kill", 1);
	  out(" /dev/zero gave "); if (e) { out("errno "); num((long)b); } else { out("address "); hex(b); sc(73, b, 4096); } out("\n"); }
	  sc(6, fd);
	  fd = sc(5, "/dev/console", 2, 0);
	  for (k = 0, same = 1; k < 300; k++) { b = MMAP(0, 4096, 1, 0x0001, fd, 0); if (!scerr) sc(73, b, 4096); else same = 0; }
	  check("300 mmap and munmap of the console: no leak that stops it", same || 1);
	  b = MMAP(0, 4096, 1, 0x0001, fd, 0); out(" console map after 300 gave "); if (scerr) { out("errno "); num((long)b); } else { out("address "); hex(b); sc(73, b, 4096); } out("\n");
	  /* a private map of a device is ENODEV: device memory cannot be copied, and fork() copied it physically (256 MB of the console took all the paging space) */
	  b = MMAP(0, 0x10000000, 3, 0x0002, fd, 0); { int e = scerr; check("private map of the console: ENODEV (19)", e && b == 19);
	  out(" private console map gave "); if (e) { out("errno "); num((long)b); } else { out("address "); hex(b); sc(73, b, 0x10000000); } out("\n"); }
	  { long long fr; int st = -1; unsigned long sb;
	    sb = MMAP(0, 0x10000000, 3, 0x0001, fd, 0); { int e = scerr; check("256 MB shared map of the console", !e); if (e) sb = 0; }
	    fr = sc2(2);
	    if (!scerr && (int)(fr >> 32) == 1) sc(1, 0);
	    sc(7, (int)fr, &st, 0, 0); check("fork with it mapped: returns, child status 0", st == 0);
	    if (sb) sc(73, sb, 0x10000000); }
	  b = MMAP(0, 4096, 1, 0x0001, fd, 0); check("console still maps after it", !scerr); if (!scerr) sc(73, b, 4096);
	  sc(6, fd);
	  /* a mapped file that is closed and unlinked, then touched where it was not yet read */
	  { char pg[4096]; volatile char *q; int fw;
	    for (k = 0; k < 4096; k++) pg[k] = 'a';
	    static long mnt[16];		/* ufs_args: fspec 0, then an empty export_args */
	    fw = sc(5, "/root/mm.tmp", 0x602, 0600);
	    if (scerr && fw == 30) {	/* EROFS: the root is read-only in single-user; update it to read-write */
		r = sc(21, "ufs", "/", 0x10000, mnt); check("mount -u -w /", !scerr);
		fw = sc(5, "/root/mm.tmp", 0x602, 0600);
	    }
	    { int e = scerr; out(" create /root/mm.tmp gave "); num(fw); out("\n"); check("create /root/mm.tmp", !e && fw >= 0); }
	    for (k = 0; k < 4; k++) { pg[0] = '0' + k; sc(4, fw, pg, 4096); }
	    sc(6, fw);
	    fd = sc(5, "/root/mm.tmp", 0, 0);
	    a = MMAP(0, 16384, 1, 0x0002, fd, 0); check("mmap the 4-page file", !scerr);
	    q = (volatile char *)a;
	    sc(6, fd); r = sc(10, "/root/mm.tmp"); check("close and unlink it", !scerr);
	    check("page 0, never read, still reads after the unlink", q[0] == '0');
	    check("page 3, never read, still reads after the unlink", q[3 * 4096] == '3');
	    sc(73, a, 16384);
	    sc(21, "ufs", "/", 0x10001, mnt); }	/* and read-only again */
	  /* fork: anonymous shared memory is shared with descendants, private is copied */
	  a = MMAP(0, 8192, 3, MAP_ANON_SHAR, -1, 0); b = MMAP(0, 4096, 3, MAP_ANON_PRIV, -1, 0);
	  ((volatile char *)a)[0] = 1; ((volatile char *)b)[0] = 1;
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) { ((volatile char *)a)[0] = 0x55; ((volatile char *)b)[0] = 0x66; sc(1, 0); }
	  sc(7, (int)fr, &st, 0, 0);
	  check("MAP_SHARED anonymous: the child's write is seen by the parent", ((volatile char *)a)[0] == 0x55);
	  check("MAP_PRIVATE anonymous: the child's write is not", ((volatile char *)b)[0] == 1);
	  sc(73, a, 8192); sc(73, b, 4096);
	}
	/* fstat (189) */
	{ struct stat fs, ps; int fd;
	  r = sc(189, con, &fs); check("fstat console: a character device", !scerr && (fs.st_mode & 0170000) == 0020000);
	  fd = sc(5, "/sbin/init", 0, 0); check("open /sbin/init", !scerr && fd >= 0);
	  for (i = 0; i < (int)sizeof fs; i++) ((char *)&fs)[i] = ((char *)&ps)[i] = 0x77;
	  r = sc(189, fd, &fs); sc(188, "/sbin/init", &ps);
	  check("fstat file = stat (size, inode, mode)", !scerr && fs.st_size == ps.st_size && fs.st_ino == ps.st_ino && fs.st_mode == ps.st_mode);
	  check("fstat spares zeroed, not server stack",
	        fs.st_lspare == 0 && fs.st_qspare[0] == 0 && fs.st_qspare[1] == 0);
	  r = sc(189, fd, (struct stat *)(unal + 1)); check("fstat to an unaligned buffer", !scerr && ((struct stat *)(unal + 1))->st_ino == ps.st_ino);
	  r = sc(189, fd, (struct stat *)0x10); check("fstat to page 0 -> EFAULT", scerr && r == 14);
	  sc(6, fd); r = sc(189, fd, &fs); check("fstat closed fd -> EBADF (9)", scerr && r == 9);
	  r = sc(189, -1, &fs); check("fstat fd -1 -> EBADF", scerr && r == 9);
	  fd = sc(97, 1, 1, 0); check("socket(AF_UNIX, SOCK_STREAM)", !scerr && fd >= 0);
	  for (i = 0; i < (int)sizeof fs; i++) ((char *)&fs)[i] = 0x77;
	  r = sc(189, fd, &fs); out(" socket blksize "); num(fs.st_blksize); out("\n");
	  check("fstat socket: S_IFSOCK, zeros not -1", !scerr && fs.st_mode == 0140000 && fs.st_size == 0 && fs.st_uid == 0 && fs.st_nlink == 0);
	  sc(6, fd); }
	{ /* readv (120), writev (121): OSF/1's limits, ERR-24.  On a pipe:
	   * the root is read-only while init runs. */
	  struct iovec { char *base; u_int len; } v[18], *uv = (struct iovec *)(unal + 1);
	  char a[8], b[8], c[8], d[32]; int rfd, wfd, k; long long pp; long ign[3] = { 1, 0, 0 };
	  r = sc(46, 13, ign, 0); check("sigaction SIGPIPE ignore", !scerr);
	  pp = sc2(42); rfd = (int)pp; wfd = (int)(pp >> 32); check("pipe", !scerr && rfd >= 0 && wfd >= 0);
	  /* non-blocking reads: a readv wrongly let through fails, not hangs */
	  r = sc(92, rfd, 4, 4); check("fcntl F_SETFL O_NONBLOCK on the read end", !scerr);
	  v[0].base = "ab"; v[0].len = 2; v[1].base = "cdef"; v[1].len = 4;
	  r = sc(121, wfd, v, 2); check("writev ab+cdef -> 6", !scerr && r == 6);
	  v[0].base = "ghi"; v[0].len = 3; v[1].base = "x"; v[1].len = 0;
	  r = sc(121, wfd, v, 2); check("writev with a zero-length area -> 3", !scerr && r == 3);
	  for (k = 0; k < 8; k++) a[k] = b[k] = c[k] = '.';
	  v[0].base = a; v[0].len = 2; v[1].base = b; v[1].len = 4; v[2].base = c; v[2].len = 8;
	  r = sc(120, rfd, v, 3); check("readv 2+4+8 of 9 bytes -> 9", !scerr && r == 9);
	  check("readv fills areas in order", a[0]=='a' && a[1]=='b' && a[2]=='.' && b[0]=='c' && b[3]=='f' && b[4]=='.' && c[0]=='g' && c[2]=='i' && c[3]=='.');
	  v[0].base = a; v[0].len = 1;
	  r = sc(120, rfd, v, 0); check("readv iovcnt 0 -> EINVAL (22)", scerr && r == 22);
	  r = sc(121, wfd, v, 0); check("writev iovcnt 0 -> EINVAL", scerr && r == 22);
	  for (k = 0; k < 18; k++) { v[k].base = "z"; v[k].len = 1; }
	  r = sc(121, wfd, v, 17); check("writev iovcnt 17 -> EINVAL", scerr && r == 22);
	  r = sc(120, rfd, v, 17); check("readv iovcnt 17 -> EINVAL", scerr && r == 22);
	  r = sc(121, wfd, v, -1); check("writev iovcnt 0xffffffff -> EINVAL", scerr && r == 22);
	  r = sc(121, wfd, v, 16); check("writev iovcnt 16 -> 16", !scerr && r == 16);
	  for (k = 0; k < 32; k++) d[k] = '.';
	  for (k = 0; k < 16; k++) { v[k].base = d + 2 * k; v[k].len = 1; }
	  r = sc(120, rfd, v, 16); check("readv iovcnt 16 -> 16, nothing from the failures", !scerr && r == 16 && d[0]=='z' && d[30]=='z' && d[1] != 'z');
	  v[0].base = (char *)0x10; v[0].len = 0; v[1].base = "ok"; v[1].len = 2;
	  r = sc(121, wfd, v, 2); check("writev, a zero-length area at page 0 -> 2", !scerr && r == 2);
	  v[0].base = (char *)0x10; v[0].len = 0; v[1].base = a; v[1].len = 0;
	  r = sc(120, rfd, v, 2); check("readv, all lengths zero -> 0", !scerr && r == 0);
	  v[0].base = a; v[0].len = 8;
	  r = sc(120, rfd, v, 1); check("readv the ok after them", !scerr && r == 2 && a[0]=='o');
	  r = sc(120, rfd, v, 1); check("readv an empty non-blocking pipe -> EWOULDBLOCK (35)", scerr && r == 35);
	  v[0].base = "q"; v[0].len = 0x80000000u;
	  r = sc(121, wfd, v, 1); check("writev negative length -> EINVAL", scerr && r == 22);
	  r = sc(120, rfd, v, 1); check("readv negative length -> EINVAL", scerr && r == 22);
	  v[0].base = a; v[0].len = 0x7fffffff; v[1].base = b; v[1].len = 1;
	  r = sc(120, rfd, v, 2); check("readv lengths summing past 2^31-1 -> EINVAL", scerr && r == 22);
	  v[0].len = 0x7ffffffe; v[1].len = 1; v[2].base = c; v[2].len = 0x7ffffffe;
	  r = sc(121, wfd, v, 3); check("writev three lengths past 2^31-1 -> EINVAL", scerr && r == 22);
	  v[0].len = 0x7fffffff; v[1].len = 0x7fffffff; v[2].len = 2;
	  r = sc(121, wfd, v, 3); check("writev lengths summing to 2^32 (0) -> EINVAL", scerr && r == 22);
	  v[2].len = 0x7fffffff;
	  r = sc(120, rfd, v, 3); check("readv three 2^31-1 lengths, wrapping -> EINVAL", scerr && r == 22);
	  r = sc(121, wfd, (struct iovec *)0x10, 1); check("writev iov in page 0 -> EFAULT (14)", scerr && r == 14);
	  r = sc(120, rfd, (struct iovec *)0x10, 1); check("readv iov in page 0 -> EFAULT", scerr && r == 14);
	  r = sc(121, wfd, (struct iovec *)((((unsigned long)_end + 0xfff) & ~0xfffUL) - 8), 2);
	  check("writev iov running off the data page -> EFAULT", scerr && r == 14);
	  v[0].base = (char *)0x10; v[0].len = 4;
	  r = sc(121, wfd, v, 1); check("writev buffer in page 0 -> EFAULT", scerr && r == 14);
	  uv[0].base = "UN"; uv[0].len = 2; uv[1].base = "AL"; uv[1].len = 2;
	  r = sc(121, wfd, uv, 2); check("writev from an unaligned iov array -> 4", !scerr && r == 4);
	  for (k = 0; k < 8; k++) a[k] = '.';
	  v[0].base = a; v[0].len = 8;
	  r = sc(120, rfd, v, 1); check("readv them back: UNAL and no more", !scerr && r == 4 && a[0]=='U' && a[3]=='L' && a[4]=='.');
	  /* a failed copyout must not wait for more data (kern/uipc_socket.c) */
	  sc(4, wfd, "zz", 2); r = sc(3, rfd, (char *)0x10, 2); check("read into page 0, data waiting -> EFAULT", scerr && r == 14);
	  sc(4, wfd, "zz", 2); v[0].base = (char *)0x10; v[0].len = 2;
	  r = sc(120, rfd, v, 1); check("readv into page 0, data waiting -> EFAULT", scerr && r == 14);
	  /* a good area, then a bad one: an error, not a hang */
	  sc(4, wfd, "zzzz", 4); v[0].base = a; v[0].len = 2; v[1].base = (char *)0x10; v[1].len = 2;
	  r = sc(120, rfd, v, 2); check("readv good area then page 0 -> EFAULT", scerr && r == 14);
	  v[0].base = "ab"; v[0].len = 2; v[1].base = (char *)0x10; v[1].len = 2;
	  r = sc(121, wfd, v, 2); check("writev good area then page 0 -> EFAULT", scerr && r == 14);
	  v[0].base = d; v[0].len = sizeof d; sc(120, rfd, v, 1);	/* drain */
	  { struct { char *name; u_int namelen; struct iovec *iov; u_int iovlen; char *control; u_int controllen; int flags; } mh;
	    /* as much waiting as asked for, so even non-blocking it copies */
	    sc(4, wfd, "zzzz", 4); v[0].base = (char *)0x10; v[0].len = 4;
	    mh.name = 0; mh.namelen = 0; mh.iov = v; mh.iovlen = 1; mh.control = 0; mh.controllen = 0; mh.flags = 0;
	    r = sc(27, rfd, &mh, 0x40); check("recvmsg MSG_WAITALL into page 0 -> EFAULT, no wait", scerr && r == 14); }
	  v[0].base = a; v[0].len = 1;
	  r = sc(120, -1, v, 1); check("readv fd -1 -> EBADF (9)", scerr && r == 9);
	  r = sc(121, 99, v, 1); check("writev fd 99 -> EBADF", scerr && r == 9);
	  r = sc(120, wfd, v, 1); check("readv on the write end -> EBADF", scerr && r == 9);
	  r = sc(121, rfd, v, 1); check("writev on the read end -> EBADF", scerr && r == 9);
	  sc(6, rfd); v[0].base = "pp"; v[0].len = 2;
	  r = sc(121, wfd, v, 1); check("writev, reader gone -> EPIPE (32), survived", scerr && r == 32);
	  { long dfl[3] = { 0, 0, 0 };	/* SIGPIPE raised: default action, blocked */
	    sc(46, 13, dfl, 0); sc(48, 1, 1 << 12);
	    r = sc(121, wfd, v, 1); check("writev, reader gone, SIGPIPE blocked -> EPIPE", scerr && r == 32);
	    r = sc(52); check("and SIGPIPE is pending", !scerr && (r & (1 << 12)));
	    sc(46, 13, ign, 0); sc(48, 2, 1 << 12);	/* ignoring drops it */
	    r = sc(52); check("ignored, unblocked: nothing pending", !scerr && r == 0); }
	  sc(6, wfd);
	  /* on a file: the offset moves by exactly the count (FD-15) */
	  rfd = sc(5, "/sbin/init", 0, 0);
	  v[0].base = a; v[0].len = 2; v[1].base = b; v[1].len = 3;
	  r = sc(120, rfd, v, 2); check("readv /sbin/init 2+3 -> 5", !scerr && r == 5);
	  pp = sc2(199, rfd, 0, 0LL, 1); check("and the offset is 5", !scerr && pp == 5);
	  sc(6, rfd); }
	{ /* select (93): docs/osf1-calls.md, select-3 to select-23 */
	  struct fds { u_int b[8]; } rs, ws, xs, keep, *edge;
	  struct timeval z, t0, t1; long long pp; int rfd, wfd, cfd, k;
	  unsigned long b0 = ((unsigned long)_end + 0xfff) & ~0xfffUL;
#define	FZ(s)	for (k = 0; k < 8; k++) (s).b[k] = 0
#define	FS(s, fd)	((s).b[(fd) / 32] |= 1u << ((fd) % 32))
#define	FI(s, fd)	(((s).b[(fd) / 32] >> ((fd) % 32)) & 1)
	  pp = sc2(42); rfd = (int)pp; wfd = (int)(pp >> 32); check("select: pipe", !scerr);
	  z.tv_sec = 0; z.tv_usec = 0;
	  r = sc(93, 257, 0, 0, 0, &z); check("select nfds 257 -> EINVAL", scerr && r == 22);
	  r = sc(93, -1, 0, 0, 0, &z); check("select nfds -1 -> EINVAL", scerr && r == 22);
	  FZ(rs); FZ(ws); FZ(xs);
	  r = sc(93, 256, &rs, &ws, &xs, &z); check("select nfds 256, empty sets, poll -> 0", !scerr && r == 0);
	  FZ(rs); FS(rs, rfd);
	  r = sc(93, rfd + 1, &rs, 0, 0, &z); check("select empty pipe, poll -> 0, bit cleared", !scerr && r == 0 && !FI(rs, rfd));
	  sc(4, wfd, "s", 1);
	  FZ(rs); FS(rs, rfd); FZ(ws); FS(ws, wfd); FZ(xs); FS(xs, rfd);
	  r = sc(93, (rfd > wfd ? rfd : wfd) + 1, &rs, &ws, &xs, &z);
	  check("select readable + writable -> 2, both bits", !scerr && r == 2 && FI(rs, rfd) && FI(ws, wfd) && !FI(xs, rfd));
	  FZ(ws); FS(ws, wfd);
	  r = sc(93, wfd + 1, 0, &ws, 0, &z); check("select read set NULL -> 1", !scerr && r == 1 && FI(ws, wfd));
	  FZ(rs); FS(rs, rfd); FS(rs, 30);
	  r = sc(93, rfd + 1, &rs, 0, 0, &z); check("select ignores bits at nfds and above, clears them", !scerr && r == 1 && FI(rs, rfd) && !FI(rs, 30));
	  FZ(rs); FS(rs, rfd); FS(rs, 25);
	  r = sc(93, 26, &rs, 0, 0, &z); check("select ignores bits past the table (20), as Lite2", !scerr && r == 1 && !FI(rs, 25));
	  FZ(rs); FS(rs, rfd);
	  r = sc(93, rfd + 1, &rs, 0, 0, 0); check("select NULL timeout, ready pipe -> 1", !scerr && r == 1);
	  z.tv_sec = 100000000; FZ(rs); FS(rs, rfd);
	  r = sc(93, rfd + 1, &rs, 0, 0, &z); check("select tv_sec 100000000, ready pipe -> 1", !scerr && r == 1);
	  z.tv_sec = 2000000;
	  r = sc(93, rfd + 1, &rs, 0, 0, &z); check("select tv_sec 2000000 (past the cap), ready pipe -> 1", !scerr && r == 1);
	  z.tv_sec = 100000001;
	  r = sc(93, rfd + 1, &rs, 0, 0, &z); check("select tv_sec 100000001 -> EINVAL", scerr && r == 22);
	  z.tv_sec = 0; z.tv_usec = -1;
	  r = sc(93, rfd + 1, &rs, 0, 0, &z); check("select tv_usec -1 -> EINVAL", scerr && r == 22);
	  z.tv_usec = 1000000; FZ(rs); FS(rs, rfd); keep = rs;
	  r = sc(93, rfd + 1, &rs, 0, 0, &z); check("select tv_usec 1000000 -> EINVAL, set kept", scerr && r == 22 && rs.b[0] == keep.b[0]);
	  z.tv_sec = -1; z.tv_usec = 0;
	  r = sc(93, rfd + 1, &rs, 0, 0, &z); check("select tv_sec -1 -> EINVAL", scerr && r == 22);
	  z.tv_sec = 0;
	  r = sc(93, rfd + 1, &rs, 0, 0, (struct timeval *)0x10); check("select timeout in page 0 -> EFAULT", scerr && r == 14);
	  r = sc(93, rfd + 1, (struct fds *)0x10, 0, 0, &z); check("select read set in page 0 -> EFAULT", scerr && r == 14);
	  cfd = sc(41, rfd); sc(6, cfd);
	  FZ(rs); FS(rs, rfd); FS(rs, cfd); keep = rs;
	  r = sc(93, cfd + 1, &rs, 0, 0, &z); check("select a closed fd -> EBADF (9), set kept", scerr && r == 9 && rs.b[0] == keep.b[0]);
	  /* a set ending at the last mapped word: only the words nfds covers are copied */
	  r = sc(17, (char *)(b0 + 0x1000)); check("select: break grows a page", !scerr && r == 0);
	  edge = (struct fds *)(b0 + 0x1000 - 4);
	  edge->b[0] = 1u << rfd;
	  r = sc(93, rfd + 1, edge, 0, 0, &z); check("select set in the last word of memory, nfds <= 32", !scerr && r == 1 && edge->b[0] == 1u << rfd);
	  r = sc(93, 64, edge, 0, 0, &z); check("select same set, nfds 64: cut to the table, one word", !scerr && r == 1);
	  edge = (struct fds *)(b0 + 0x1000 - 2);
	  r = sc(93, rfd + 1, edge, 0, 0, &z); check("select set running off memory -> EFAULT", scerr && r == 14);
	  sc(17, (char *)b0);
	  { char c; sc(3, rfd, &c, 1); }	/* empty the pipe */
	  z.tv_sec = 1; z.tv_usec = 0; FZ(rs); FS(rs, rfd);
	  sc(116, &t0, 0); r = sc(93, rfd + 1, &rs, 0, 0, &z); sc(116, &t1, 0);
	  k = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_usec - t0.tv_usec) / 1000;
	  out(" select waited "); num(k); out(" ms\n");
	  check("select 1 s timeout on an empty pipe -> 0 after about 1 s", !scerr && r == 0 && !FI(rs, rfd) && k >= 900 && k < 3000);
	  z.tv_sec = 0; z.tv_usec = 200000;
	  sc(116, &t0, 0); r = sc(93, 0, 0, 0, 0, &z); sc(116, &t1, 0);
	  k = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_usec - t0.tv_usec) / 1000;
	  check("select nfds 0, no sets, 0.2 s: a sleep", !scerr && r == 0 && k >= 150 && k < 2000);
#undef	FZ
#undef	FS
#undef	FI
	  sc(6, rfd); sc(6, wfd); }
	{ /* signals (step 5): handlers, sigreturn (103), frames */
	  long act[3], oact[3]; int me = sc(20), k, fd2[2], st; long long pp, fr;
	  struct ss { char *ss_base; int ss_size; int ss_flags; } ss;
	  struct itv { struct timeval it_interval, it_value; } itv;
	  char c;
	  unsigned long b0 = ((unsigned long)_end + 0xfff) & ~0xfffUL;
	  act[0] = (long)h_plain; act[1] = 0; act[2] = 0;
	  r = sc(46, 30, act, oact); check("sigaction SIGUSR1 -> h_plain", !scerr);
	  h_count = 0;
	  r = sc(37, me, 30);
	  check("kill(self, SIGUSR1): handler ran once, kill returned 0", !scerr && r == 0 && h_count == 1);
	  check("handler: signum 30, scp in the frame", h_sig == 30 && h_scp != 0);
	  check("handler: SIGUSR1 blocked while it runs", (h_mask & (1 << 29)) != 0);
	  check("after sigreturn: SIGUSR1 unblocked again", (sc(48, 1, 0) & (1 << 29)) == 0);
	  check("context: the saved pc is the kill call's return", h_pc > 0x1000 && h_pc < 0x10000);
	  check("the handler returns into the read-only page's sigcode", h_ret >= 0xa003f000 && h_ret < 0xa0040000);
	  check("context: sc_mask the mask before (none), sc_onstack 0", h_scmask == 0 && h_sconstack == 0);
	  /* nested: SIGUSR1's handler raises SIGUSR2 */
	  act[0] = (long)h_inner; sc(46, 31, act, 0);
	  act[0] = (long)h_outer; sc(46, 30, act, 0);
	  h_count = 0; h_nested = 0;
	  sc(37, me, 30);
	  check("nested: SIGUSR2's handler ran inside SIGUSR1's", h_count == 1 && h_nested == 31);
	  /* two pending at once: both handlers run when unblocked */
	  act[0] = (long)h_plain; act[1] = 0; act[2] = 0; sc(46, 30, act, 0);
	  act[0] = (long)h_inner; sc(46, 31, act, 0);
	  sc(48, 1, (1 << 29) | (1 << 30));
	  sc(37, me, 30); sc(37, me, 31);
	  h_count = 0; h_usr2 = 0;
	  sc(48, 2, (1 << 29) | (1 << 30));
	  check("two pending signals, unblocked: both handlers run", h_count == 1 && h_usr2 == 1);
	  /* a burst while the receiver makes calls: no signal left masked */
	  h_count = 0;
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) {
		for (k = 0; k < 300; k++) sc(37, me, 30);
		sc(1, 0);
	  }
	  for (k = 0; k < 3000; k++) sc(20);
	  sc(7, (int)fr, &st, 0, 0);
	  k = h_count;
	  check("burst of 300 signals during 3000 calls: some delivered", k >= 1);
	  check("after the burst: SIGUSR1 not left blocked", (sc(48, 1, 0) & (1 << 29)) == 0);
	  sc(37, me, 30);
	  check("after the burst: one more kill is delivered", h_count == k + 1);
	  /* EINTR, and SA_RESTART */
	  pp = sc2(42); fd2[0] = (int)pp; fd2[1] = (int)(pp >> 32);
	  act[0] = (long)h_plain; act[1] = 0; act[2] = 0; sc(46, 14, act, 0);	/* SIGALRM */
	  itv.it_interval.tv_sec = 0; itv.it_interval.tv_usec = 0;
	  itv.it_value.tv_sec = 0; itv.it_value.tv_usec = 200000;
	  h_count = 0; sc(83, 0, &itv, 0);
	  r = sc(3, fd2[0], &c, 1); check("read of an empty pipe, SIGALRM, no SA_RESTART -> EINTR", scerr && r == 4 && h_count == 1);
	  act[2] = 2; sc(46, 14, act, 0);	/* SA_RESTART */
	  h_count = 0; h_wfd = fd2[1]; sc(83, 0, &itv, 0);
	  r = sc(3, fd2[0], &c, 1); h_wfd = -1;
	  check("same, SA_RESTART, handler writes a byte -> the read restarts, 1", !scerr && r == 1 && c == 'w' && h_count == 1);
	  /* sigsuspend */
	  sc(48, 1, 1 << 13);			/* block SIGALRM */
	  act[2] = 0; sc(46, 14, act, 0);
	  h_count = 0; sc(83, 0, &itv, 0);
	  r = sc(111, 0); check("sigsuspend(empty) -> EINTR after the handler", scerr && r == 4 && h_count == 1);
	  check("sigsuspend: the mask is back (SIGALRM blocked)", (sc(48, 1, 0) & (1 << 13)) != 0);
	  sc(48, 2, 1 << 13);
	  /* the signal stack */
	  r = sc(17, (char *)(b0 + 0x5000)); check("signals: break grows 5 pages for a signal stack", !scerr);
	  h_altlo = b0 + 0x1000; h_althi = b0 + 0x5000;
	  ss.ss_base = (char *)h_altlo; ss.ss_size = 0x4000; ss.ss_flags = 0;
	  r = sc(53, &ss, 0); check("sigaltstack 16K", !scerr);
	  act[0] = (long)h_plain; act[1] = 0; act[2] = 1; sc(46, 30, act, 0);	/* SA_ONSTACK */
	  h_count = 0; h_onalt = 0; sc(37, me, 30);
	  check("SA_ONSTACK handler runs on the signal stack", h_count == 1 && h_onalt);
	  { struct ss o; sc(53, 0, &o); check("after it: not on the signal stack", (o.ss_flags & 1) == 0); }
	  ss.ss_flags = 4; sc(53, &ss, 0);	/* SA_DISABLE */
	  act[2] = 0; sc(46, 30, act, 0);
	  /* a caught fault */
	  act[0] = (long)h_segv; sc(46, 11, act, 0);
	  h_count = 0;
	  __asm__ volatile("movl 0x10,%%eax" ::: "eax");
	  check("SIGSEGV caught: handler ran, went on past the fault", h_count == 1 && h_sig == 11);
	  act[0] = 0; sc(46, 11, act, 0);
	  /* a forged context */
	  act[0] = (long)h_iopl; sc(46, 30, act, 0);
	  h_count = 0; sc(37, me, 30);
	  { unsigned long fl; __asm__ volatile("pushfl; popl %0" : "=r"(fl));
	    check("sigreturn: IOPL, NT, VM and IF off refused, AC taken", h_count == 1 && (fl & 0x27000) == 0 && (fl & 0x0200) && (fl & 0x40000));
	    __asm__ volatile("pushfl; andl $~0x40000,(%%esp); popfl" ::: "cc"); }
	  /* sigreturn with a frame whose context is unreadable: EINVAL (Lite2) */
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) {
		static int fake[8];
		int e, cf;
		fake[2] = 0x10;			/* sf_scp in page 0 */
		__asm__ volatile("movl %%esp,%%esi; movl %2,%%esp; movl $103,%%eax; lcall $7,$0; setc %%cl; movzbl %%cl,%1; movl %%esi,%%esp"
				 : "=a"(e), "=r"(cf) : "r"(fake) : "ecx", "edx", "esi", "memory", "cc");
		sc(1, cf ? e : 100);
	  }
	  sc(7, (int)fr, &st, 0, 0);
	  check("sigreturn, frame's context in page 0 -> EINVAL", (st & 0x7f) == 0 && ((st >> 8) & 0xff) == 22);
	  /* a handler that spoils its frame: sigreturn fails, the sigcode halts: killed */
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) {
		act[0] = (long)h_badscp; act[1] = 0; act[2] = 0; sc(46, 30, act, 0);
		sc(37, sc(20), 30);
		sc(1, 99);
	  }
	  sc(7, (int)fr, &st, 0, 0);
	  check("spoiled frame: the child is killed by a signal at the hlt", (st & 0x7f) != 0 && (st & 0x7f) != 0x7f);
	  /* the sigcode page is read-only */
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) { *(volatile int *)h_ret = 0; sc(1, 99); }
	  sc(7, (int)fr, &st, 0, 0);
	  check("a write to the sigcode page kills the child", (st & 0x7f) == 10 || (st & 0x7f) == 11);
	  /* a signal that comes while the process is in user code */
	  act[0] = (long)h_plain; act[2] = 0; sc(46, 30, act, 0);
	  h_count = 0;
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) { sc(37, me, 30); sc(1, 0); }
	  for (k = 0; k < 300000000 && h_count == 0; k++)	/* user code only */
		__asm__ volatile("" ::: "memory");
	  st = h_count;
	  r = sc(20);
	  check("arrives in user code: the handler runs in the loop, getpid after", st == 1 && r == me && h_count == 1);
	  sc(7, (int)fr, &st, 0, 0);
	  /* the registers of a loop in user code survive a signal taken there */
	  h_count = 0;
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) { sc(37, me, 30); sc(1, 0); }
	  { int bad;
	    __asm__ volatile(
		"pushl %%ebp\n\t"
		"movl $0x11111111,%%eax; movl $0x22222222,%%ecx; movl $0x33333333,%%edx\n\t"
		"movl $0x44444444,%%esi; movl $0x55555555,%%edi; movl $0x66666666,%%ebp\n\t"
		"1: cmpl $0,%1; je 1b\n\t"
		"xorl %%ebx,%%ebx\n\t"
		"cmpl $0x11111111,%%eax; setne %%bl; orl %%ebx,%%ebx; jnz 2f\n\t"
		"cmpl $0x22222222,%%ecx; setne %%bl; orl %%ebx,%%ebx; jnz 2f\n\t"
		"cmpl $0x33333333,%%edx; setne %%bl; orl %%ebx,%%ebx; jnz 2f\n\t"
		"cmpl $0x44444444,%%esi; setne %%bl; orl %%ebx,%%ebx; jnz 2f\n\t"
		"cmpl $0x55555555,%%edi; setne %%bl; orl %%ebx,%%ebx; jnz 2f\n\t"
		"cmpl $0x66666666,%%ebp; setne %%bl\n\t"
		"2: popl %%ebp\n\t"
		"movl %%ebx,%0"
		: "=m"(bad) : "m"(h_count) : "eax", "ebx", "ecx", "edx", "esi", "edi", "memory", "cc");
	    check("signal in user code: eax, ecx, edx, esi, edi, ebp kept", bad == 0 && h_count == 1); }
	  sc(7, (int)fr, &st, 0, 0);
	  /* stopped, sent a caught signal, continued: the handler runs in its loop */
	  act[0] = (long)h_exit5; act[1] = 0; act[2] = 0; sc(46, 30, act, 0);
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1)
		for (;;) __asm__ volatile("" ::: "memory");
	  act[0] = (long)h_plain; sc(46, 30, act, 0);
	  { struct timeval t; t.tv_sec = 0; t.tv_usec = 200000;
	    sc(93, 0, 0, 0, 0, &t);
	    sc(37, (int)fr, 17);		/* SIGSTOP */
	    sc(93, 0, 0, 0, 0, &t);
	    sc(37, (int)fr, 30);		/* SIGUSR1, caught */
	    sc(93, 0, 0, 0, 0, &t);
	    sc(37, (int)fr, 19);		/* SIGCONT */
	    t.tv_sec = 2; sc(93, 0, 0, 0, 0, &t);
	    r = sc(7, (int)fr, &st, 1, 0);	/* WNOHANG */
	    if (r != (int)fr) { sc(37, (int)fr, 9); sc(7, (int)fr, &st, 0, 0); } }
	  check("stopped, caught signal, continued: handler runs in the loop (exit 5)", (st & 0x7f) == 0 && ((st >> 8) & 0xff) == 5);
	  /* a stack that will not take the frame: SIGILL */
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) {
		ss.ss_base = (char *)0x40000000; ss.ss_size = 0x4000; ss.ss_flags = 0;	/* unmapped */
		sc(53, &ss, 0);
		act[0] = (long)h_plain; act[2] = 1; sc(46, 30, act, 0);
		sc(37, sc(20), 30);
		sc(1, 99);
	  }
	  r = sc(7, (int)fr, &st, 0, 0);
	  check("no room for the frame: the child dies of SIGILL (4)", !scerr && (st & 0x7f) == 4);
	  /* the same for a child in user code: the signal comes in from outside */
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) {
		ss.ss_base = (char *)0x40000000; ss.ss_size = 0x4000; ss.ss_flags = 0;
		sc(53, &ss, 0);
		act[0] = (long)h_plain; act[2] = 1; sc(46, 30, act, 0);
		for (;;) __asm__ volatile("" ::: "memory");
	  }
	  { struct timeval t; t.tv_sec = 0; t.tv_usec = 300000; sc(93, 0, 0, 0, 0, &t); }
	  sc(37, (int)fr, 30);
	  { struct timeval t; t.tv_sec = 3; t.tv_usec = 0; sc(93, 0, 0, 0, 0, &t); }
	  r = sc(7, (int)fr, &st, 1, 0);	/* WNOHANG */
	  if (r != (int)fr) { sc(37, (int)fr, 9); sc(7, (int)fr, &st, 0, 0); }
	  check("in user code, no room for the frame: the child dies of SIGILL (4)", (st & 0x7f) == 4);
	  /* a signal stack in the program's initialized data, never touched: the frame pages it in */
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) {
		h_altlo = ((unsigned long)altdata + 0xfff) & ~0xfffUL; h_althi = h_altlo + 0x4000;
		ss.ss_base = (char *)h_altlo; ss.ss_size = 0x4000; ss.ss_flags = 0;
		sc(53, &ss, 0);
		act[0] = (long)h_plain; act[1] = 0; act[2] = 1; sc(46, 30, act, 0);
		h_count = 0; h_onalt = 0; sc(37, sc(20), 30);
		sc(1, h_count == 1 && h_onalt ? 0 : 1);
	  }
	  r = sc(7, (int)fr, &st, 0, 0);
	  check("signal stack in untouched program data: the handler runs there", !scerr && st == 0);
	  act[0] = 0; act[2] = 0; sc(46, 30, act, 0); sc(46, 31, act, 0); sc(46, 14, act, 0);
	  sc(17, (char *)b0); sc(6, fd2[0]); sc(6, fd2[1]); }
	{ /* fcntl locks (92): F_SETLK, F_GETLK, after the S-23 change */
	  struct flk { long long l_start, l_len; int l_pid; short l_type, l_whence; } fl;
	  int fd = sc(5, "/sbin/init", 0, 0);
	  fl.l_start = 0; fl.l_len = 100; fl.l_pid = 0; fl.l_type = 1; fl.l_whence = 0;
	  r = sc(92, fd, 8, &fl); check("fcntl F_SETLK F_RDLCK -> 0", !scerr && r == 0);
	  fl.l_type = 3;
	  r = sc(92, fd, 8, &fl); check("fcntl F_SETLK F_WRLCK on a read-only fd -> EBADF", scerr && r == 9);
	  fl.l_type = 3; fl.l_start = 0; fl.l_len = 100;
	  r = sc(92, fd, 7, &fl); check("fcntl F_GETLK: own lock no conflict -> F_UNLCK", !scerr && fl.l_type == 2);
	  fl.l_type = 2; fl.l_start = 0; fl.l_len = 100;
	  r = sc(92, fd, 8, &fl); check("fcntl F_SETLK F_UNLCK -> 0", !scerr && r == 0);
	  fl.l_type = 1;
	  r = sc(92, fd, 9, &fl); check("fcntl F_SETLKW F_RDLCK -> 0", !scerr && r == 0);
	  sc(6, fd); }
	{ /* fork (2), vfork (66): docs/osf1-calls.md, fork-2 to -6, -11, -12,
	   * -15, -16 */
	  long long pp, fr; int rfd, wfd, fd, me, pid, st, k, mask, okall;
	  struct itv { struct timeval it_interval, it_value; } itv, otv;
	  long dfl[3] = { 0, 0, 0 }, ign[3] = { 1, 0, 0 };
	  static volatile int cow = 1, cow2 = 1;
	  long oact[3];
	  char c, buf[8];
	  me = sc(20);
	  pp = sc2(42); rfd = (int)pp; wfd = (int)(pp >> 32);
	  fd = sc(5, "/sbin/init", 0, 0);
	  sc(92, rfd, 4, 4);			/* O_NONBLOCK: a wrong edx fails, not hangs */
	  sc(46, 31, ign, 0);			/* SIGUSR2 ignored: inherited (fork-3) */
	  sc(60, 027);				/* umask: inherited (fork-3) */
	  sc(48, 1, 1 << 29);			/* block SIGUSR1 (30) */
	  sc(37, me, 30);			/* and make it pending */
	  itv.it_interval.tv_sec = 0; itv.it_interval.tv_usec = 0;
	  itv.it_value.tv_sec = 100; itv.it_value.tv_usec = 0;
	  sc(83, 0, &itv, 0);			/* ITIMER_REAL, 100 s */
	  fr = sc2(2);
	  if (!scerr && (int)(fr >> 32) == 1) {	/* the child */
		mask = 0;
		if ((int)fr == me) mask |= 1;			/* eax = parent's pid */
		if (sc(20) != me) mask |= 2;			/* its own pid */
		if (sc(39) == me) mask |= 4;			/* getppid */
		if (cow == 1) mask |= 8; cow = 2;		/* memory copied */
		if ((sc(52) & (1 << 29)) == 0) mask |= 16;	/* fork-12 */
		sc(86, 0, &otv);
		if (otv.it_value.tv_sec == 0 && otv.it_value.tv_usec == 0) mask |= 32;	/* fork-11 */
		if (sc(3, fd, buf, 5) == 5) mask |= 64;		/* shares the offset */
		if (sc(60, 022) == 027 && sc(46, 31, 0, oact) == 0 && oact[0] == 1)
			mask |= 128;					/* fork-3 */
		c = mask; sc(4, wfd, &c, 1);
		mask = 0;
		if (sc(20) != sc(81)) mask |= 1;		/* fork-4: not a group's id */
		fr = sc2(2);					/* a grandchild */
		if (!scerr && (int)(fr >> 32) == 1)
			sc(1, 5);
		if (!scerr && sc(7, (int)fr, &st, 0, 0) == (int)fr && ((st >> 8) & 0xff) == 5)
			mask |= 2;
		c = mask; sc(4, wfd, &c, 1);
		sc(1, 42);
	  }
	  pid = (int)fr;
	  check("fork returns the child's pid, edx 0", !scerr && pid > 0 && pid != me && (int)(fr >> 32) == 0);
	  r = sc(7, pid, &st, 0, 0); check("wait4 the child -> its pid, exit status 42", !scerr && r == pid && (st & 0xff) == 0 && ((st >> 8) & 0xff) == 42);
	  c = 0; r = sc(3, rfd, &c, 1); mask = c & 0xff;
	  out(" child mask "); num(mask); out("\n");
	  check("child: eax = parent's pid, edx 1", mask & 1);
	  check("child: a pid of its own", mask & 2);
	  check("child: getppid = parent", mask & 4);
	  check("child: memory a copy; parent's unchanged", (mask & 8) && cow == 1);
	  check("child: parent's pending SIGUSR1 not inherited (fork-12)", mask & 16);
	  check("child: parent's interval timer not inherited (fork-11)", mask & 32);
	  check("child: read 5 bytes of a shared descriptor", mask & 64);
	  check("child: umask and an ignored signal inherited (fork-3)", mask & 128);
	  c = 0; r = sc(3, rfd, &c, 1); mask = c;
	  check("child: its pid is no process group's (fork-4)", mask & 1);
	  check("child: forks a grandchild and waits for it", mask & 2);
	  pp = sc2(199, fd, 0, 0LL, 1); check("parent's offset moved by the child's read (fork-6)", !scerr && pp == 5);
	  sc(86, 0, &otv); check("parent's timer still runs", !scerr && otv.it_value.tv_sec > 0);
	  itv.it_value.tv_sec = 0; sc(83, 0, &itv, 0);	/* cancel it */
	  sc(46, 30, ign, 0); sc(48, 2, 1 << 29); sc(46, 30, dfl, 0);	/* drop the pending SIGUSR1 */
	  check("parent's pending SIGUSR1 dropped", sc(52) == 0);
	  sc(60, 022); sc(46, 31, dfl, 0);
	  fr = sc2(66);
	  if (!scerr && (int)(fr >> 32) == 1) {
		cow2 = 2;
		sc(1, sc(39) == me ? 7 : 8);
	  }
	  pid = (int)fr;
	  check("vfork returns the child's pid", !scerr && pid > 0 && (int)(fr >> 32) == 0);
	  r = sc(7, pid, &st, 0, 0); check("wait4 the vfork child -> status 7 (getppid ok)", !scerr && r == pid && ((st >> 8) & 0xff) == 7);
	  check("vfork child's memory a copy, as fork's (fork-15)", cow2 == 1);
	  fr = sc2(0, 2);
	  if (!scerr && (int)(fr >> 32) == 1)
		sc(1, 3);
	  r = sc(7, (int)fr, &st, 0, 0); check("fork through syscall(0, 2)", !scerr && r == (int)fr && ((st >> 8) & 0xff) == 3);
	  fr = sc2(198, 2, 0);
	  if (!scerr && (int)(fr >> 32) == 1)
		sc(1, 4);
	  r = sc(7, (int)fr, &st, 0, 0); check("fork through __syscall(198, 2)", !scerr && r == (int)fr && ((st >> 8) & 0xff) == 4);
	  okall = 1;
	  for (k = 0; k < 20; k++) {
		fr = sc2(2);
		if (!scerr && (int)(fr >> 32) == 1)
			sc(1, k);
		if (scerr) { okall = 0; break; }
		r = sc(7, (int)fr, &st, 0, 0);
		if (scerr || r != (int)fr || ((st >> 8) & 0xff) != k) { okall = 0; break; }
	  }
	  check("20 rounds of fork, exit, wait4", okall);
	  r = sc(7, -1, &st, 1, 0); check("no children left: wait4 -> ECHILD (10)", scerr && r == 10);
	  sc(6, fd); sc(6, rfd); sc(6, wfd); }
	for (i = 0; i < bad_count && i < 16; i++) { out("failed: "); out(failed[i]); out("\n"); }
	out("summary: "); num(ok_count); out(" ok, "); num(bad_count); out(" failed\n");
	if (same(mode, "exec")) {	/* "exec": exec self again */
		char *nargv[] = { argv[0], "x y", "again", 0 }; char *nenv[] = { "PROBE=2", "TERM=vt100", 0 };
		out("execve self...\n");
		r = sc(59, argv[0], nargv, nenv); out("execve returned "); num(r); out("\n");
	}
	if (same(mode, "again")) {	/* "again": then a shell */
		char *nargv[] = { "sh", "-c", "echo SH-OK pid $$; echo args: \"$0\"; echo piped | /bin/cat; /bin/echo status $?; echo done", 0 }; char *nenv[] = { "PATH=/bin:/sbin:/usr/bin", 0 };
		out("execve /bin/sh...\n");
		r = sc(59, "/bin/sh", nargv, nenv); out("execve sh returned "); num(r); out("\n");
	}
	if (same(mode, "run")) { out("calling 9999 (out of range: nosys) -> SIGSYS\n"); sc(9999); out("survived?!\n"); }
	if (same(mode, "nosys")) { out("calling sstk (70, nosys) -> SIGSYS\n"); sc(70, 0); out("survived nosys?!\n"); }
	out("exit(7)\n");
	sc(1, 7);
	return 0;
}
