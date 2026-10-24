/*
 * hold.c -- fuzz the second half of docs/deferred.md, S-23: a call that
 * finds a file by its descriptor and then sleeps holds a reference on
 * it until it ends, so another thread's close() cannot free the file
 * under it.  kern_descrip.c's closef(), closef_last(), fhold_call() and
 * frele_call() are copied in verbatim (FUNCS, made by extract.py); the
 * descriptor table here is the harness's own, as the table code has its
 * own fuzzer (table.c).
 *
 * Several "threads" each begin calls that look a descriptor up through
 * fhold_call() and then sleep; while they sleep, the others open, dup
 * and close descriptors and begin and end calls of their own.  A call
 * ends by using its file -- reading it, as a read() that wakes would --
 * and then gives its references back with frele_call(), as
 * server_thread_deregister() does.  An independent model checks every
 * file's f_count, that each file is closed exactly once and only when
 * its last descriptor and its last hold are gone, its flock() lock
 * released then and only then, that giving a hold
 * back never releases the process's POSIX locks (only close() does),
 * and that a call holding K_NHELD files is refused another.
 *
 * Built with -DOLD, fhold_call() takes no reference and frele_call()
 * does nothing, as before the change: AddressSanitizer then stops the
 * run at the first use of a freed file.
 *
 * run.sh builds and runs it:
 *   ./hold [ops] [seed]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int u_int;
typedef char *caddr_t;
#define register
#undef __P
#define __P(x)		x
#define FILE_PORTS	0
#define DTYPE_VNODE	1
#define DTYPE_SOCKET	2
#define FHASLOCK	0x4000
#define P_ADVLOCK	0x00001
#define F_UNLCK		2
#define F_FLOCK		0x020
#define F_POSIX		0x040
#define SEEK_SET	0
#define K_NHELD		4
#define MAXID		2000000
#define panic(s)	(fprintf(stderr, "PANIC %s\n", s), abort())
#define printf		quiet_printf

struct flock { long long l_start, l_len; int l_pid; short l_type, l_whence; };
struct vnode { int v; };
struct proc { int p_flag; int p_pid; };
struct file;
struct fileops { int (*fo_close)(struct file *, struct proc *); };
struct file {
	int f_flag;
	int f_type;
	int f_count;
	struct fileops *f_ops;
	void *f_data;
	int f_id;			/* harness: which file */
};
typedef struct proc_invocation {
	struct proc *k_p;
	int k_serving;
	int k_nheld;
	struct file *k_held[K_NHELD];
} *proc_invocation_t;

static struct proc_invocation *cur;
#define get_proc_invocation()	cur

static int quiet_printf(const char *f, ...) { (void)f; return 0; }
static long posix_unlocks, flock_unlocks;
static int VOP_ADVLOCK(struct vnode *vp, void *id, int op, struct flock *fl, int flags)
{
	(void)vp; (void)id; (void)op; (void)fl;
	if (flags & F_POSIX) posix_unlocks++;
	if (flags & F_FLOCK) flock_unlocks++;
	return 0;
}
static int closes[MAXID];		/* fo_close() per file id */
static int fo_close(struct file *fp, struct proc *p) { (void)p; closes[fp->f_id]++; return 0; }
static struct fileops fops = { fo_close };
static struct vnode vn;
static void ffree(struct file *fp) { memset(fp, 0xdd, sizeof *fp); free(fp); }

static int closef_last(struct file *, struct proc *);
#ifdef OLD
#define fhold_call	unused_fhold_call
#define frele_call	unused_frele_call
#endif
#include FUNCS
#ifdef OLD
#undef fhold_call
#undef frele_call
static struct file *fhold_call(struct file *fp) { return fp; }
static void frele_call(struct proc *p) { (void)p; }
#endif

/* ---------------- the model ---------------- */
#define NFD	12
#define NTH	5
static struct proc P = { P_ADVLOCK, 1 };
static struct file *tab[NFD];		/* the descriptor table */
static int tabid[NFD];
static struct file *fptr[MAXID];
static int refs[MAXID];		/* descriptors + holds, expected */
static char flocked[MAXID];		/* FHASLOCK on a vnode */
static long expect_flock;		/* last closes of such files */
static int nextid = 1;
static struct proc_invocation inv[NTH];
static int busy[NTH];			/* a call in progress */
static struct file *using[NTH][K_NHELD];
static int nusing[NTH];
static long ops, c_uaf_window, c_refused, c_lastbyhold, c_calls;
static unsigned rs;
static int rnd(int n) { rs = rs * 1103515245 + 12345; return (int)((rs >> 8) % (unsigned)n); }

static void fail(const char *why, int a)
{
	fprintf(stderr, "FAIL after %ld ops: %s (%d)\n", ops, why, a);
	exit(1);
}

static void check(void)
{
	if (flock_unlocks != expect_flock)
		fail("flock released other than at a file's last close", (int)flock_unlocks);
	for (int id = 1; id < nextid; id++) {
		if (refs[id] > 0) {
			if (!fptr[id]) fail("live file gone", id);
			if (fptr[id]->f_count != refs[id]) fail("f_count wrong", id);
			if (closes[id]) fail("closed while referenced", id);
		} else if (closes[id] != 1)
			fail("not closed exactly once", id);
	}
}

static void do_open(void)
{
	int fd = rnd(NFD);
	if (tab[fd] || nextid >= MAXID) return;
	struct file *fp = calloc(1, sizeof *fp);
	fp->f_count = 1; fp->f_ops = &fops; fp->f_data = &vn;
	fp->f_type = rnd(2) ? DTYPE_VNODE : DTYPE_SOCKET;
	fp->f_flag = rnd(3) == 0 ? FHASLOCK : 0;
	fp->f_id = nextid; fptr[nextid] = fp; refs[nextid] = 1;
	flocked[nextid] = (fp->f_flag & FHASLOCK) && fp->f_type == DTYPE_VNODE;
	tab[fd] = fp; tabid[fd] = nextid++;
}

static void do_dup(void)
{
	int a = rnd(NFD), b = rnd(NFD);
	if (!tab[a] || tab[b]) return;
	tab[b] = tab[a]; tabid[b] = tabid[a];
	tab[b]->f_count++; refs[tabid[b]]++;
}

static void do_close(void)
{
	int fd = rnd(NFD);
	if (!tab[fd]) return;
	struct file *fp = tab[fd]; int id = tabid[fd];
	tab[fd] = NULL;
	long pu = posix_unlocks;
	cur = &inv[rnd(NTH)];		/* any thread may close */
	if (--refs[id] == 0) { fptr[id] = NULL; expect_flock += flocked[id]; }
	else {
		for (int t = 0; t < NTH; t++)
			for (int k = 0; k < nusing[t]; k++)
				if (using[t][k] == fp) c_uaf_window++;
	}
	int vnode = fp->f_type == DTYPE_VNODE;
	closef(fp, &P);
	if (vnode ? posix_unlocks != pu + 1 : posix_unlocks != pu)
		fail("close() did not release the POSIX locks once", id);
}

static void begin_call(int t)
{
	if (busy[t]) return;
	busy[t] = 1; nusing[t] = 0; c_calls++;
	cur = &inv[t]; cur->k_p = &P; cur->k_serving = 1;
	int n = 1 + rnd(6);		/* lookups in one call: past K_NHELD too */
	for (int i = 0; i < n; i++) {
		int fd = rnd(NFD), h = cur->k_nheld;
		struct file *fp = fhold_call(tab[fd]);
		if (!tab[fd]) { if (fp) fail("hold of an empty slot", fd); continue; }
#ifndef OLD
		if (h == K_NHELD) {
			if (fp) fail("hold past K_NHELD", h);
			c_refused++;
			continue;
		}
		if (fp != tab[fd] || cur->k_nheld != h + 1) fail("hold not taken", h);
		refs[fp->f_id]++;
#endif
		if (nusing[t] < K_NHELD) using[t][nusing[t]++] = fp;
	}
}

static void end_call(int t)
{
	if (!busy[t]) return;
	cur = &inv[t];
	for (int k = 0; k < nusing[t]; k++) {
		struct file *fp = using[t][k];
		volatile int touch = fp->f_type + fp->f_count;	/* the call wakes and uses it */
		(void)touch;
	}
	long pu = posix_unlocks;
	int ids[K_NHELD], n = cur->k_nheld;
	for (int k = 0; k < n; k++) ids[k] = cur->k_held[k]->f_id;
	frele_call(&P);
	if (posix_unlocks != pu) fail("giving a hold back released POSIX locks", t);
	if (cur->k_nheld != 0) fail("holds left after frele_call", cur->k_nheld);
	for (int k = 0; k < n; k++)
		if (--refs[ids[k]] == 0) {
			fptr[ids[k]] = NULL; c_lastbyhold++;
			expect_flock += flocked[ids[k]];
		}
	busy[t] = 0; nusing[t] = 0; cur->k_p = NULL; cur->k_serving = 0;
}

int main(int argc, char **argv)
{
	long n = argc > 1 ? atol(argv[1]) : 2000000;
	rs = argc > 2 ? (unsigned)atol(argv[2]) : 1;
	for (ops = 0; ops < n; ops++) {
		switch (rnd(6)) {
		case 0: do_open(); break;
		case 1: do_dup(); break;
		case 2: do_close(); break;
		case 3: case 4: begin_call(rnd(NTH)); break;
		case 5: end_call(rnd(NTH)); break;
		}
		if (ops % 64 == 0) check();
	}
	for (int t = 0; t < NTH; t++) end_call(t);
	for (int fd = 0; fd < NFD; fd++) {
		if (!tab[fd]) continue;
		struct file *fp = tab[fd]; int id = tabid[fd];
		tab[fd] = NULL;
		if (--refs[id] == 0) { fptr[id] = NULL; expect_flock += flocked[id]; }
		cur = &inv[0];
		closef(fp, &P);
	}
	check();
	fprintf(stdout, "ok: %ld ops, %ld calls, %d files; closes under a sleeping call's hold %ld, "
		"files last closed by a hold %ld, holds refused at K_NHELD %ld\n",
		ops, c_calls, nextid - 1, c_uaf_window, c_lastbyhold, c_refused);
	return 0;
}
