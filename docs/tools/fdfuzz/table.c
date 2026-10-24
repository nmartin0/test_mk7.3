/*
 * table.c -- fuzz Lites' file descriptor table code (kern_descrip.c's
 * fdalloc, fdavail, falloc, fdinstall, fdunreserve, ffree, finishdup,
 * dup, dup2, close, fdcopy, extracted verbatim into FUNCS) against an
 * independent model, with a second "thread" acting at every point where
 * the server can sleep with a system call in progress:
 *
 *   - between falloc() and the end of an open (vn_open() sleeps), which
 *     the harness splits into begin_open / end_open, many in flight;
 *   - inside closef() once a file's last reference is gone (vn_close()
 *     or soclose() sleep), which dup2() reaches with a slot reserved.
 *
 * Build with -DOLD for the code before the change (falloc() enters the
 * file at once; open's failure path stores NULL), to show the fuzzer
 * finds what the change fixes.  Built with AddressSanitizer, any use of
 * a freed file or table, or any access out of bounds, stops the run.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int u_int;
typedef unsigned char u_char;
typedef unsigned short u_short;

#define EBADF	9
#define ENFILE	23
#define EMFILE	24
#define NDFILE	20
#define NDEXTENT 50
#define UF_EXCLOSE	0x01
#define UF_MAPPED	0x02
#define UF_RESERVED	0x04
#define RLIMIT_NOFILE	8
#define FILE_PORTS	0
#define DIAGNOSTIC	1
#define register
#define min(a, b)	((a) < (b) ? (a) : (b))
#define VREF(vp)	((void)0)
#define M_FILEDESC 0
#define M_FILE 1
#define M_WAITOK 0
#define MALLOC(space, cast, size, type, flags) (space) = (cast)malloc(size)
#define FREE(addr, type) free((void *)(addr))
#define bcopy(a, b, n)	memmove((b), (a), (n))
#define bzero(a, n)	memset((a), 0, (n))
#define panic(s)	(fprintf(stderr, "PANIC %s\n", s), abort())

struct ucred { int cr_ref; };
struct file {
	struct file *f_filef, **f_fileb;
	int f_count;
	struct ucred *f_cred;
	int f_id;			/* harness: which file */
};
struct vnode { int v; };
struct filedesc {
	struct	file **fd_ofiles;
	char	*fd_ofileflags;
	struct	vnode *fd_cdir;
	struct	vnode *fd_rdir;
	int	fd_nfiles;
	u_short	fd_lastfile;
	u_short	fd_freefile;
	u_short	fd_cmask;
	u_short	fd_refcnt;
};
struct filedesc0 {
	struct	filedesc fd_fd;
	struct	file *fd_dfiles[NDFILE];
	char	fd_dfileflags[NDFILE];
};
#define OFILESIZE (sizeof(struct file *) + sizeof(char))
struct rlimit { long rlim_cur, rlim_max; };
struct proc {
	struct filedesc *p_fd;
	struct rlimit p_rlimit[10];
	struct ucred *p_ucred;
};
struct dup_args { u_int fd; };
struct dup2_args { u_int from; u_int to; };
struct close_args { int fd; };

int maxfiles = 1000, nfiles, fdexpand;
struct file *filehead;
static struct ucred cred0 = { 1 };
static void crhold(struct ucred *c) { c->cr_ref++; }
static void crfree(struct ucred *c) { c->cr_ref--; }
static void tablefull(const char *s) { (void)s; }
static int munmapfd() { abort(); return 0; }	/* UF_MAPPED is never set */
int closef(struct file *, struct proc *);
void fdinstall(struct filedesc *, int, struct file *);
void fdunreserve(struct filedesc *, int);
int ffree(struct file *);

#define close lites_close
#define dup lites_dup
#define dup2 lites_dup2
#include FUNCS

/* ---------------- the model ---------------- */
#define MAXS 400
enum { FREE, RESV, INST };
static int st[MAXS];		/* state of each slot */
static int fid[MAXS];		/* file id when INST */
static int fcount[100000];	/* expected f_count per file id */
static struct file *fptr[100000];
static int nextid = 1;
static struct proc P;
static long lim;		/* RLIMIT_NOFILE */
static long ops, checks;
static int nesting, teardown, dup2_src = -1;
static long c_nest, c_emfile, c_grow, c_dup2resv, c_dup2lost, c_forkresv, c_openfail, c_install;
static unsigned rs;
static int rnd(int n) { rs = rs * 1103515245 + 12345; return (int)((rs >> 8) % (unsigned)n); }

struct pend { int fd; struct file *fp; int id; } pend[MAXS];
static int npend;

static void fail(const char *why, int a)
{
	fprintf(stderr, "FAIL after %ld ops: %s (%d)\n", ops, why, a);
	exit(1);
}

static int effective_lim(void) { return (int)min(lim, (long)maxfiles); }
static int lowest_free(int from)
{
	for (int i = from; i < effective_lim(); i++)
		if (st[i] == FREE) return i;
	return -1;
}

static void check(void)
{
	struct filedesc *fdp = P.p_fd;
	int maxinst = -1;
	checks++;
	for (int i = 0; i < MAXS; i++) {
		struct file *f = i < fdp->fd_nfiles ? fdp->fd_ofiles[i] : NULL;
		int fl = i < fdp->fd_nfiles ? fdp->fd_ofileflags[i] : 0;
#ifndef OLD
		if (f && (fl & UF_RESERVED)) fail("slot both filled and reserved", i);
		if (st[i] == RESV && (f || !(fl & UF_RESERVED))) fail("reserved slot not reserved", i);
		if (st[i] == FREE && (f || (fl & UF_RESERVED))) fail("free slot not free", i);
#else
		if (st[i] == FREE && f) fail("free slot holds a file", i);
#endif
		if (st[i] == INST) {
			if (!f) fail("installed slot empty", i);
			if (f->f_id != fid[i]) fail("installed slot holds the wrong file", i);
			maxinst = i;
		}
		if (i < fdp->fd_freefile && st[i] == FREE) fail("free slot below fd_freefile", i);
	}
	if (maxinst > fdp->fd_lastfile) fail("file above fd_lastfile", maxinst);
	for (int id = 1; id < nextid; id++)
		if (fcount[id] > 0 && fptr[id]->f_count != fcount[id])
			fail("f_count wrong for file", id);
}

static void do_op(int depth);

/* closef(): the last reference gone, the close sleeps; another thread runs */
int closef(struct file *fp, struct proc *p)
{
	(void)p;
	int id = fp->f_id;
	if (--fp->f_count > 0) return 0;
	fcount[id] = 0;
	if (!teardown && nesting < 3 && rnd(2) == 0) {	/* sleeping in vn_close()/soclose() */
		nesting++; c_nest++; do_op(1); nesting--;
	}
	ffree(fp);
	fptr[id] = NULL;
	return 0;
}

static void begin_open(void)
{
	struct file *fp; int fd;
	int want = lowest_free(0);
	int e = falloc(&P, &fp, &fd);
	if (e) {
		if (e != EMFILE || want != -1) fail("falloc error", e);
		c_emfile++;
		return;
	}
	if (fd != want) fail("falloc did not give the lowest free slot", fd);
	fp->f_id = nextid; fptr[nextid] = fp; fcount[nextid] = 1;
#ifdef OLD
	st[fd] = INST; fid[fd] = nextid;	/* 4.4BSD: in the table at once */
#else
	st[fd] = RESV;
#endif
	pend[npend++] = (struct pend){ fd, fp, nextid++ };
}

static void end_open(int ok)
{
	if (!npend) return;
	int k = rnd(npend);
	struct pend pd = pend[k];
	pend[k] = pend[--npend];
#ifdef OLD
	if (ok) return;
	ffree(pd.fp); fcount[pd.id] = 0; fptr[pd.id] = NULL;
	P.p_fd->fd_ofiles[pd.fd] = NULL;		/* open()'s old failure path */
	st[pd.fd] = FREE;
#else
	if (ok) {
		fdinstall(P.p_fd, pd.fd, pd.fp); c_install++;
		st[pd.fd] = INST; fid[pd.fd] = pd.id;
	} else {
		ffree(pd.fp); fcount[pd.id] = 0; fptr[pd.id] = NULL; c_openfail++;
		fdunreserve(P.p_fd, pd.fd);
		st[pd.fd] = FREE;
	}
#endif
}

static void do_close(void)
{
	int fd = rnd(MAXS + 2) - 1;
	if (nesting && dup2_src >= 0 && rnd(2)) fd = dup2_src;	/* aim at a sleeping dup2() */
	struct close_args a = { fd }; int r = 0;
	int expect_ok = fd >= 0 && fd < MAXS && st[fd] == INST;
	int id = expect_ok ? fid[fd] : 0;
	if (expect_ok) { st[fd] = FREE; fcount[id]--; }
	int e = close(&P, &a, &r);
	if (expect_ok != (e == 0)) fail("close result", fd);
	if (!expect_ok && e != EBADF) fail("close error", e);
}

static void do_dup(void)
{
	int fd = rnd(MAXS < 64 ? MAXS : 64);
	struct dup_args a = { (u_int)fd }; int r = -1;
	int ok = st[fd] == INST;
	int want = lowest_free(0);
	int e = dup(&P, &a, &r);
	if (!ok) { if (e != EBADF) fail("dup of a closed fd", fd); return; }
	if (want == -1) { if (e != EMFILE) fail("dup with no room", e); return; }
	if (e || r != want) fail("dup result", r);
	st[r] = INST; fid[r] = fid[fd]; fcount[fid[fd]]++;
}

static void do_dupfd(void)	/* fcntl(F_DUPFD): fdalloc() + finishdup() */
{
	int fd = rnd(MAXS), m = rnd(effective_lim() + 5), i, r;
	if (st[fd] != INST || m >= effective_lim()) return;
	int want = lowest_free(m);
	int e = fdalloc(&P, m, &i);
	if (want == -1) { if (e != EMFILE) fail("F_DUPFD with no room", e); return; }
	if (e || i != want) fail("F_DUPFD slot", i);
	e = finishdup(P.p_fd, fd, i, &r);
	if (e || r != i) fail("F_DUPFD finishdup", e);
	st[i] = INST; fid[i] = fid[fd]; fcount[fid[fd]]++;
}

static void do_dup2(void)
{
	int a = rnd(MAXS), b = rnd(MAXS);
	struct dup2_args ar = { (u_int)a, (u_int)b }; int r = -1;
	if (st[a] != INST || b >= effective_lim()) {
		int e = dup2(&P, &ar, &r);
		if (e != EBADF) fail("dup2 bad args", e);
		return;
	}
	if (a == b) {
		if (dup2(&P, &ar, &r) || r != b) fail("dup2 same", b);
		return;
	}
#ifndef OLD
	if (st[b] == RESV) {
		if (dup2(&P, &ar, &r) != EBADF) fail("dup2 onto a reserved slot", b);
		c_dup2resv++;
		return;
	}
#endif
	/* while dup2() closes b's file, b is reserved for it */
	if (st[b] == INST) fcount[fid[b]]--;
	st[b] = RESV;
	int saved = dup2_src; dup2_src = a;
	int e = dup2(&P, &ar, &r);
	dup2_src = saved;
	if (st[a] == INST) {		/* a may have changed meanwhile */
		if (e || r != b) fail("dup2 result", e);
		st[b] = INST; fid[b] = fid[a]; fcount[fid[a]]++;
	} else {
		if (e != EBADF) fail("dup2 after its source was closed", e);
		c_dup2lost++;
		st[b] = FREE;
	}
}

static void do_fork(void)
{
	for (int i = 0; i < MAXS; i++) if (st[i] == RESV) { c_forkresv++; break; }
	struct filedesc *child = fdcopy(&P);
	for (int i = 0; i < child->fd_nfiles; i++) {
		struct file *f = child->fd_ofiles[i];
		if (child->fd_ofileflags[i] & UF_RESERVED) fail("child inherits a reservation", i);
		if ((st[i] == INST) != (f != NULL)) fail("child slot differs", i);
		if (f && f->f_id != fid[i]) fail("child file differs", i);
		if (i < child->fd_freefile && !f) fail("child: free slot below fd_freefile", i);
	}
	for (int i = 0; i <= child->fd_lastfile && i < child->fd_nfiles; i++)
		if (child->fd_ofiles[i]) child->fd_ofiles[i]->f_count--;
	if (child->fd_nfiles > NDFILE) free(child->fd_ofiles);
	free(child);
}

static void do_avail(void)
{
	int n = 1 + rnd(60), have = 0;	/* n = 0 in a full table is 4.4BSD's "no": left as is */
	for (int i = 0; i < effective_lim(); i++) if (st[i] == FREE) have++;
	if (fdavail(&P, n) != (have >= n)) fail("fdavail", n);
}

static void do_op(int depth)
{
	ops++;
	switch (rnd(depth ? 6 : 9)) {
	case 0: begin_open(); break;
	case 1: end_open(rnd(4) != 0); break;
	case 2: do_close(); break;
	case 3: do_dup(); break;
	case 4: do_dup2(); break;
	case 5: do_dupfd(); break;
	case 6: do_fork(); break;
	case 7: do_avail(); break;
	case 8: begin_open(); break;
	}
}

int main(int argc, char **argv)
{
	long seeds = argc > 1 ? atol(argv[1]) : 1000, steps = argc > 2 ? atol(argv[2]) : 400;
	for (long seed = 1; seed <= seeds; seed++) {
		rs = (unsigned)seed;
		memset(st, 0, sizeof st); npend = 0; nextid = 1; nfiles = 0; filehead = NULL;
		lim = 20 + rnd(MAXS - 25);		/* small to large: growth, EMFILE */
		struct filedesc0 *f0 = calloc(1, sizeof *f0);
		f0->fd_fd.fd_ofiles = f0->fd_dfiles;
		f0->fd_fd.fd_ofileflags = f0->fd_dfileflags;
		f0->fd_fd.fd_nfiles = NDFILE;
		f0->fd_fd.fd_refcnt = 1;
		P.p_fd = &f0->fd_fd; P.p_ucred = &cred0;
		P.p_rlimit[RLIMIT_NOFILE].rlim_cur = lim;
		for (long s = 0; s < steps; s++) { do_op(0); check(); }
		teardown = 1;
		while (npend) end_open(0);
		check();
		for (int i = 0; i < MAXS; i++) if (st[i] == INST) { struct close_args a = { i }; int r; st[i] = FREE; fcount[fid[i]]--; close(&P, &a, &r); }
		check();
		if (nfiles != 0) fail("files leaked", nfiles);
		if (P.p_fd->fd_nfiles > NDFILE) free(P.p_fd->fd_ofiles);
		free(f0);
		teardown = 0;
	}
	printf("OK: %ld seeds, %ld operations, %ld invariant checks\n", seeds, ops, checks);
	printf("  closes that slept with another thread acting: %ld\n  opens installed: %ld, opens failed and slot given back: %ld\n  dup2 onto a reserved slot: %ld\n  dup2 whose source was closed while it slept: %ld\n  forks with a reservation in flight: %ld\n  EMFILE (table full): %ld, table growths: %d\n", c_nest, c_install, c_openfail, c_dup2resv, c_dup2lost, c_forkresv, c_emfile, fdexpand);
	return 0;
}
