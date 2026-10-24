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
static void out(const char *s){ sc(4, con, s, len(s)); }
static void num(long v){ char b[24]; int i=23; int neg=v<0; unsigned long u=neg?-v:v; b[i]=0; do{b[--i]='0'+u%10;u/=10;}while(u); if(neg)b[--i]='-'; out(b+i);}
static void hex(unsigned long v){ char b[12]; int i=11; b[i]=0; do{b[--i]="0123456789abcdef"[v&15];v>>=4;}while(v); out("0x"); out(b+i);}
static int ok_count, bad_count;
static void check(const char *what, int cond){ out(cond?"  ok   ":"  FAIL "); out(what); out("\n"); if(cond)ok_count++; else bad_count++; }
struct timeval { long tv_sec, tv_usec; };
struct stat { unsigned long st_dev; unsigned long st_ino; unsigned short st_mode; short st_nlink; unsigned long st_uid, st_gid, st_rdev;
	long st_atime, st_atimens, st_mtime, st_mtimens, st_ctime, st_ctimens; long long st_size; long long st_blocks; unsigned long st_blksize, st_flags, st_gen; long st_lspare; long long st_qspare[2]; };
static char unal[128];
extern char _end[];			/* probe.ld: the end of data and BSS */
int start_c(int *kframe)
{
	int argc = kframe[0]; char **argv = (char **)(kframe+1); char **envp = argv + argc + 1;
	int i, r; struct timeval tv; struct stat st; char *p;
	char *mode = argc < 1 ? "" : argv[argc - 1][0] == '-' ? argv[0] : argv[argc - 1];
	for (p = mode; *p; p++) if (*p == '/') mode = p + 1;
	con = sc(5, "/dev/console", 2, 0);	/* open O_RDWR */
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
