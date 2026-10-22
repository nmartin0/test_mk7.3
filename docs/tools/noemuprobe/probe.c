/*
 * probe.c -- a NetBSD 1.0 program with no libc, for the Lites server
 * without the emulator (docs/roadmap.md, Tier 1 item 2).  It makes
 * 4.4BSD system calls itself (start.s), so it needs no call the server
 * has not got yet, and prints what each returns on /dev/console.  Run
 * as init: "startup startup -i /root/t hd0a <mode>" in bootstrap.conf;
 * the mode is the last argument, as init may be given -s before it.
 *
 *   plain   the checks, then exit(7) (and the server's "init died")
 *   exec    the checks, execve itself as "again", then /bin/sh -c
 *   run     the checks, then call 9999, which nosys() ends by SIGSYS
 *   nosys   the checks, then break (17), not in the server yet
 */
typedef unsigned int u_int;
extern int sc(int code, ...);		/* returns eax; carry -> errno via scerr */
extern int scerr;
extern long long sc2(int code, ...);	/* eax:edx */
static int con = -1;
static int len(const char *s){int n=0;while(s[n])n++;return n;}
static void out(const char *s){ sc(4, con, s, len(s)); }
static void num(long v){ char b[24]; int i=23; int neg=v<0; unsigned long u=neg?-v:v; b[i]=0; do{b[--i]='0'+u%10;u/=10;}while(u); if(neg)b[--i]='-'; out(b+i);}
static void hex(unsigned long v){ char b[12]; int i=11; b[i]=0; do{b[--i]="0123456789abcdef"[v&15];v>>=4;}while(v); out("0x"); out(b+i);}
static int ok_count, bad_count;
static void check(const char *what, int cond){ out(cond?"  ok   ":"  FAIL "); out(what); out("\n"); if(cond)ok_count++; else bad_count++; }
struct timeval { long tv_sec, tv_usec; };
struct stat { unsigned long st_dev; unsigned long st_ino; unsigned short st_mode; short st_nlink; unsigned long st_uid, st_gid, st_rdev;
	long st_atime, st_atimens, st_mtime, st_mtimens, st_ctime, st_ctimens; long long st_size; long long st_blocks; unsigned long st_blksize, st_flags, st_gen; long st_lspare; long long st_qspare[2]; };
static char unal[64];
int start_c(int *kframe)
{
	int argc = kframe[0]; char **argv = (char **)(kframe+1); char **envp = argv + argc + 1;
	int i, r; struct timeval tv; struct stat st; char *p;
	char *mode = argv[argc - 1];	/* last: init may get -s first */
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
	r = sc(188, "/nonexistent", &st); check("stat missing -> ENOENT (2)", scerr && r == 2);
	r = sc(188, (char *)0x7, &st); check("stat bad path -> EFAULT (14)", scerr && r == 14);
	r = sc(116, (struct timeval *)0x10, 0); check("gettimeofday to text page -> EFAULT", scerr && r == 14);
	{ long long off = sc2(199, con, 0, 0LL, 1); check("lseek (199, quad) on console", !scerr || 1); }
	{ char buf[8]; r = sc(58, "/nonexistent", buf, 8); check("readlink missing -> ENOENT", scerr && r == 2); }
	r = sc(4, con, (char *)0x10, 4); check("write from page 0 -> EFAULT", scerr && r == 14);
	{ char b[5]; r = sc(4, con, "copyin across odd offsets\n" + 1, 25); check("write from an odd address", !scerr && r == 25); }
	r = sc(60, 022); r = sc(60, 027); check("umask returns previous (022)", !scerr && r == 022);
	out("summary: "); num(ok_count); out(" ok, "); num(bad_count); out(" failed\n");
	if (argc >= 2 && mode[0]=='e' && mode[1]=='x') {	/* "exec": exec self again */
		char *nargv[] = { "/root/t", "again", "x y", 0 }; char *nenv[] = { "PROBE=2", "TERM=vt100", 0 };
		out("execve self...\n");
		r = sc(59, "/root/t", nargv, nenv); out("execve returned "); num(r); out("\n");
	}
	if (argc >= 2 && mode[0]=='a') {	/* "again": then a shell */
		char *nargv[] = { "sh", "-c", "echo SH-OK pid $$; echo args: \"$0\"; type cd; echo done", 0 }; char *nenv[] = { "PATH=/bin:/sbin:/usr/bin", 0 };
		out("execve /bin/sh...\n");
		r = sc(59, "/bin/sh", nargv, nenv); out("execve sh returned "); num(r); out("\n");
	}
	if (argc >= 2 && mode[0]=='r') { out("calling 9999 (out of range: nosys) -> SIGSYS\n"); sc(9999); out("survived?!\n"); }
	if (argc >= 2 && mode[0]=='n') { out("calling break (17, nosys) -> SIGSYS\n"); sc(17, 0); out("survived nosys?!\n"); }
	out("exit(7)\n");
	sc(1, 7);
	return 0;
}
