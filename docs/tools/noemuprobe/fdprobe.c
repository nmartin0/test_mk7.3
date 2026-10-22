/*
 * fdprobe.c -- the file descriptor table, from a NetBSD 1.0 program
 * with no libc, as probe.c is built (mkprobe.sh <out> fdprobe.c): run
 * as init under either Lites server, "startup startup -i /fdprobe hd0a
 * <mode>" in bootstrap.conf, mode "emu" for the emulator's server
 * (adds fork) or "noemu" for the other.  docs/deferred.md, S-23.
 *
 * Each check prints "ok" or "FAIL"; a summary ends the run, after an
 * execve of itself (mode "after-emu" or "after-noemu") that checks
 * close-on-exec.
 */
typedef unsigned int u_int;
extern int sc(int code, ...);
extern int scerr;
extern long long sc2(int code, ...);
static int con = -1;
static int len(const char *s){int n=0;while(s[n])n++;return n;}
static void out(const char *s){ sc(4, con, s, len(s)); }
static void num(long v){ char b[24]; int i=23; int neg=v<0; unsigned long u=neg?-v:v; b[i]=0; do{b[--i]='0'+u%10;u/=10;}while(u); if(neg)b[--i]='-'; out(b+i);}
static int ok_count, bad_count;
static int last_r;
static void check(const char *what, int cond){ if(cond){ok_count++;return;} out("  FAIL "); out(what); out(" (r "); num(last_r); out(")\n"); bad_count++; }
static int eq(const char *a, const char *b){ while(*a && *a==*b){a++;b++;} return *a==*b; }

struct stat { unsigned long st_dev; unsigned long st_ino; unsigned short st_mode; short st_nlink; unsigned long st_uid, st_gid, st_rdev;
	long st_atime, st_atimens, st_mtime, st_mtimens, st_ctime, st_ctimens; long long st_size; long long st_blocks; unsigned long st_blksize, st_flags, st_gen; long st_lspare; long long st_qspare[2]; };
struct rlimit { long long rlim_cur, rlim_max; };
struct iovec { void *iov_base; u_int iov_len; };
struct msghdr { void *msg_name; u_int msg_namelen; struct iovec *msg_iov; u_int msg_iovlen; void *msg_control; u_int msg_controllen; int msg_flags; };
struct cmsg { u_int len; int level; int type; int fds[16]; };

enum { EXIT=1, FORK=2, READ=3, WRITE=4, OPEN=5, CLOSE=6, WAIT4=7, RECVMSG=27, SENDMSG=28, DUP=41, PIPE=42,
       EXECVE=59, DUP2=90, FCNTL=92, SOCKET=97, SOCKETPAIR=135, FSTAT=189, GETRLIMIT=194, SETRLIMIT=195 };
#define EBADF 9
#define EMFILE 24
#define EMSGSIZE 40

static unsigned long ino(int fd){ struct stat s; if (sc(FSTAT, fd, &s) && scerr) return 0; return scerr ? 0 : s.st_ino; }
static int isopen(int fd){ struct stat s; sc(FSTAT, fd, &s); return !scerr; }
static int opennull(void){ return sc(OPEN, "/dev/null", 2, 0); }

static int send_fds(int s, int *fds, int n)
{
	struct cmsg cm; struct iovec io; struct msghdr m; char byte = 'x'; int i;
	cm.len = 12 + 4 * n; cm.level = 0xffff; cm.type = 1;
	for (i = 0; i < n; i++) cm.fds[i] = fds[i];
	io.iov_base = &byte; io.iov_len = 1;
	m.msg_name = 0; m.msg_namelen = 0; m.msg_iov = &io; m.msg_iovlen = 1;
	m.msg_control = &cm; m.msg_controllen = cm.len; m.msg_flags = 0;
	return sc(SENDMSG, s, &m, 0);
}
static int recv_fds(int s, int *fds, int *n)	/* returns errno or 0 */
{
	struct cmsg cm; struct iovec io; struct msghdr m; char byte; int r, i;
	io.iov_base = &byte; io.iov_len = 1;
	m.msg_name = 0; m.msg_namelen = 0; m.msg_iov = &io; m.msg_iovlen = 1;
	m.msg_control = &cm; m.msg_controllen = sizeof cm; m.msg_flags = 0;
	r = sc(RECVMSG, s, &m, 0);
	if (scerr) return r;
	*n = m.msg_controllen >= 12 ? (int)(cm.len - 12) / 4 : 0;
	for (i = 0; i < *n && i < 16; i++) fds[i] = cm.fds[i];
	return 0;
}

int start_c(int *kframe)
{
	int argc = kframe[0]; char **argv = (char **)(kframe+1);
	char *mode = argv[argc - 1];
	int i, r, a, b, c, fds[16], n;
	char buf[8];

	if (mode[0] == 'a')
		con = 0;		/* kept across the execve; slot 5 must stay free */
	else
		con = sc(OPEN, "/dev/console", 2, 0);
	out("\n== fdprobe "); out(mode); out(" ==\n");

	if (mode[0] == 'a') {		/* after execve: close-on-exec */
		last_r = r; check("close-on-exec fd (5) closed by execve", !isopen(5));
		last_r = r; check("other fd (6) kept across execve", isopen(6));
		r = opennull();
		last_r = r; check("a new open after execve takes the lowest slot (5)", r == 5);
		{ int j; out(" open after execve:"); for (j = 0; j < 8; j++) { out(" "); num(isopen(j)); } out("\n"); }
		goto done;
	}

	/* lowest free slot */
	a = opennull(); b = opennull(); c = opennull();
	last_r = r; check("opens take ascending slots", a >= 0 && b == a + 1 && c == b + 1);
	sc(CLOSE, b); r = opennull(); last_r = r; check("a freed slot is reused first", r == b);
	r = sc(OPEN, "/nonexistent", 0, 0); last_r = r; check("open of a missing file -> ENOENT", scerr && r == 2);
	sc(CLOSE, b); r = opennull(); last_r = r; check("a failed open gives its slot back", r == b);

	/* dup, dup2, F_DUPFD */
	r = sc(DUP, a); last_r = r; check("dup takes the lowest slot", !scerr && r == c + 1);
	last_r = r; check("dup shares the file", ino(r) == ino(a)); sc(CLOSE, r);
	r = sc(DUP, 40); last_r = r; check("dup of a closed fd -> EBADF", scerr && r == EBADF);
	r = sc(DUP2, a, 15); last_r = r; check("dup2 to an empty slot", !scerr && (r == 15 || eq(mode, "emu")) && ino(15) == ino(a));
	if (r != 15) { out(" dup2 returned "); num(r); out(" (the emulator's)\n"); }
	{ int sp[2]; long long v = sc2(PIPE); sp[0] = (int)v; sp[1] = (int)(v >> 32);
	  (void) ino(sp[0]);	/* 4.4BSD numbers a unix socket on first sight, from 0 */
	  r = sc(DUP2, sp[0], 15);
	  last_r = r; check("dup2 onto an open fd replaces it", !scerr && (r == 15 || eq(mode, "emu")) && ino(15) == ino(sp[0]) && ino(15) != ino(a));
	  last_r = r; check("the replaced file's other fd still works", ino(a) != 0);
	  sc(CLOSE, sp[0]); sc(CLOSE, sp[1]); }
	r = sc(DUP2, 15, 15); last_r = r; check("dup2 to itself", !scerr && (r == 15 || eq(mode, "emu")));
	r = sc(DUP2, 99, 16); last_r = r; check("dup2 from a closed fd -> EBADF", scerr && r == EBADF);
	r = sc(DUP2, a, 100000); last_r = r; check("dup2 past the limit -> EBADF", scerr && r == EBADF);
	sc(CLOSE, 15);
	r = sc(FCNTL, a, 0, 12); last_r = r; check("F_DUPFD at or above 12", !scerr && r == 12 && ino(12) == ino(a)); sc(CLOSE, 12);
	r = sc(FCNTL, a, 0, 100000); last_r = r; check("F_DUPFD past the limit -> EINVAL", scerr && r == 22);

	/* socket, socketpair, pipe */
	r = sc(SOCKET, 1, 1, 0); last_r = r; check("socket", !scerr && r >= 0 && isopen(r)); sc(CLOSE, r);
	{ int sv[2] = {-1, -1};
	  r = sc(SOCKETPAIR, 1, 1, 0, sv); last_r = r; check("socketpair: two open fds", !scerr && isopen(sv[0]) && isopen(sv[1]));
	  sc(WRITE, sv[0], "hi", 2); buf[0] = 0; r = sc(READ, sv[1], buf, 2); last_r = r; check("socketpair carries data", r == 2 && buf[0] == 'h');
	  sc(CLOSE, sv[0]); sc(CLOSE, sv[1]); }
	{ long long v = sc2(PIPE); int p0 = (int)v, p1 = (int)(v >> 32);
	  last_r = r; check("pipe: two open fds", !scerr && isopen(p0) && isopen(p1));
	  sc(WRITE, p1, "yo", 2); buf[0] = 0; r = sc(READ, p0, buf, 2); last_r = r; check("pipe carries data", r == 2 && buf[0] == 'y');
	  sc(CLOSE, p0); sc(CLOSE, p1); }

	/* EMFILE, and passing files with too little room (fdavail) */
	{ struct rlimit rl, old; int sv[2], first, last = -1, cnt = 0, have, e;
	  sc(GETRLIMIT, 8, &old);
	  r = sc(SOCKETPAIR, 1, 1, 0, sv);
	  for (i = 0; i < 25; i++) { r = opennull(); if (r > last) last = r; }	/* grow the table past 20 */
	  for (i = 21; i <= last; i++) sc(CLOSE, i);
	  rl.rlim_cur = 30; rl.rlim_max = old.rlim_max; r = sc(SETRLIMIT, 8, &rl);
	  last_r = r; check("setrlimit NOFILE 30", !scerr);
	  first = -1; while ((r = opennull()) >= 0 && !scerr) { if (first < 0) first = r; last = r; cnt++; }
	  last_r = r; check("open until EMFILE stops at fd 29", scerr && r == EMFILE && last == 29);
	  sc(CLOSE, 29); r = opennull(); last_r = r; check("after EMFILE a close frees exactly that slot", r == 29);
	  for (i = 25; i <= 29; i++) sc(CLOSE, i);		/* five usable slots, 25..29 */
	  for (i = 0; i < 10; i++) fds[i] = a;
	  r = send_fds(sv[0], fds, 10); last_r = r; check("sendmsg ten fds", !scerr);
	  e = recv_fds(sv[1], fds, &n); last_r = e;
	  last_r = r; check("receiving ten fds into five slots -> EMSGSIZE, no panic", e == EMSGSIZE);
	  last_r = r; check("those five slots are still free", opennull() == 25); sc(CLOSE, 25);
	  e = recv_fds(sv[1], fds, &n);	/* the refused message's data byte, its files dropped */
	  last_r = e; check("then the refused message's data, without files", e == 0 && n == 0);
	  for (i = 0; i < 3; i++) fds[i] = a;
	  r = send_fds(sv[0], fds, 3); e = recv_fds(sv[1], fds, &n);
	  out(" three: send "); num(r); out(" recv "); num(e); out(" n "); num(n); out(" fd0 "); num(fds[0]); out("\n");
	  last_r = r; check("receiving three fds into five slots", e == 0 && n == 3 && fds[0] == 25 && ino(fds[2]) == ino(a));
	  for (i = 0; i < n; i++) sc(CLOSE, fds[i]);
	  for (i = 20; i < 25; i++) sc(CLOSE, i);
	  for (i = c + 1; i < 21; i++) if (i != 5 && i != 6) sc(CLOSE, i);
	  sc(SETRLIMIT, 8, &old); sc(CLOSE, sv[0]); sc(CLOSE, sv[1]); }

	/* fork: the child sees the parent's files (the emulator's server only) */
	if (eq(mode, "emu")) {
		long long v = sc2(FORK);
		if ((int)(v >> 32) == 1) {		/* child */
			sc(EXIT, ino(a) != 0 && ino(a) == ino(c) ? 0 : 1);
		}
		{ int st = -1; sc(WAIT4, (int)v, &st, 0, 0);
		  last_r = r; check("fork: the child has the parent's files", st == 0); }
	}

	/* close-on-exec: fd 5 marked, fd 6 not, then execve self */
	while ((r = opennull()) < 6 && !scerr) ;
	sc(DUP2, a, 5); sc(DUP2, a, 6);
	r = sc(FCNTL, 5, 2, 1); last_r = r; check("F_SETFD close-on-exec", !scerr);
	r = sc(FCNTL, 5, 1, 0); last_r = r; check("F_GETFD reads it back", !scerr && r == 1);
	for (i = 7; i < 20; i++) sc(CLOSE, i);
	{ char *nargv[] = { "/fdprobe", eq(mode, "emu") ? "after-emu" : "after-noemu", 0 };
	  char *nenv[] = { 0 };
	  out("summary before execve: "); num(ok_count); out(" ok, "); num(bad_count); out(" failed\n");
	  sc(EXECVE, "/fdprobe", nargv, nenv);
	  last_r = r; check("execve returned", 0); }
done:
	out("summary: "); num(ok_count); out(" ok, "); num(bad_count); out(" failed\n");
	sc(EXIT, 7);
	return 0;
}
