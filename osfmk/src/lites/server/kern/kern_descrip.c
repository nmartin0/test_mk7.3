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
 * $Log: kern_descrip.c,v $
 * Revision 1.1.1.1  1995/03/02  21:49:46  mike
 * Initial Lites release from hut.fi
 *
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_descrip.c	8.6 (Berkeley) 4/19/94
 */

#include "compat_43.h"
#include "file_ports.h"
#include "no_emulator.h"
#include "diagnostic.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/unistd.h>
#include <sys/resourcevar.h>

/*
 * Descriptor management.
 */
struct file *filehead;	/* head of list of open files */
int nfiles;		/* actual number of open files */

/*
 * System calls on descriptors.
 */
struct getdtablesize_args {
	int	dummy;
};
/* ARGSUSED */
getdtablesize(p, uap, retval)
	struct proc *p;
	struct getdtablesize_args *uap;
	int *retval;
{

	*retval = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles);
	return (0);
}

/*
 * Duplicate a file descriptor.
 */
struct dup_args {
	u_int	fd;
};
/* ARGSUSED */
dup(p, uap, retval)
	struct proc *p;
	struct dup_args *uap;
	int *retval;
{
	register struct filedesc *fdp;
	u_int old;
	int new, error;

	old = uap->fd;
	/*
	 * XXX Compatibility
	 */
	if (old &~ 077) { uap->fd &= 077; return (dup2(p, uap, retval)); }

	fdp = p->p_fd;
	if (old >= fdp->fd_nfiles || fdp->fd_ofiles[old] == NULL)
		return (EBADF);
	if (error = fdalloc(p, 0, &new))
		return (error);
	return (finishdup(fdp, (int)old, new, retval));
}

/*
 * Duplicate a file descriptor to a particular value.
 */
struct dup2_args {
	u_int	from;
	u_int	to;
};
/* ARGSUSED */
dup2(p, uap, retval)
	struct proc *p;
	struct dup2_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register u_int old = uap->from, new = uap->to;
	register struct file *fp;
	int i, error;

	if (old >= fdp->fd_nfiles ||
	    fdp->fd_ofiles[old] == NULL ||
	    new >= p->p_rlimit[RLIMIT_NOFILE].rlim_cur ||
	    new >= maxfiles)
		return (EBADF);
	if (old == new) {
		*retval = new;
		return (0);
	}
	/*
	 * AI-ONLY NOTE: new is reserved before finishdup() enters the
	 * file.  A slot another call has reserved is EBADF, not taken
	 * (sys/filedesc.h, UF_RESERVED).  A file already at new is
	 * taken out of the table before it is closed, as closef() can
	 * sleep and another thread of the process must not find it
	 * there meanwhile.  fdalloc() gives another slot than new only
	 * if new were taken while it grew the table, which cannot
	 * happen while it keeps the master lock, as it does (its
	 * MALLOC() is the server's malloc()); were that to change, the
	 * slot is given back and the call is EBADF, not 4.4BSD's panic.
	 */
	if (new >= fdp->fd_nfiles) {
		if (error = fdalloc(p, new, &i))
			return (error);
		if (new != i) {
			fdunreserve(fdp, i);
			return (EBADF);
		}
	} else if (fdp->fd_ofileflags[new] & UF_RESERVED) {
		return (EBADF);
	} else if (fp = fdp->fd_ofiles[new]) {
		if (fdp->fd_ofileflags[new] & UF_MAPPED)
			(void) munmapfd(p, new);
		fdp->fd_ofiles[new] = NULL;
		fdp->fd_ofileflags[new] = UF_RESERVED;
		/*
		 * dup2() must succeed even if the close has an error.
		 */
		(void) closef(fp, p);
	} else
		fdp->fd_ofileflags[new] = UF_RESERVED;
	return (finishdup(fdp, (int)old, (int)new, retval));
}

/*
 * The file control system call.
 */
struct fcntl_args {
	int	  fd;
	int	  cmd;
	integer_t arg;
};
/* ARGSUSED */
fcntl(p, uap, retval)
	struct proc *p;
	register struct fcntl_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	register char *pop;
	struct vnode *vp;
	int i, tmp, error, flg = F_POSIX;
	struct flock fl;
	u_int newmin;

	if ((unsigned)uap->fd >= fdp->fd_nfiles ||
	    (fp = fhold_call(fdp->fd_ofiles[uap->fd])) == NULL)
		return (EBADF);
	pop = &fdp->fd_ofileflags[uap->fd];
	switch (uap->cmd) {

	case F_DUPFD:
		newmin = uap->arg;
		if (newmin >= p->p_rlimit[RLIMIT_NOFILE].rlim_cur ||
		    newmin >= maxfiles)
			return (EINVAL);
		if (error = fdalloc(p, newmin, &i))
			return (error);
		return (finishdup(fdp, uap->fd, i, retval));

	case F_GETFD:
		*retval = *pop & 1;
		return (0);

	case F_SETFD:
		*pop = (*pop &~ 1) | (uap->arg & 1);
		return (0);

	case F_GETFL:
		*retval = OFLAGS(fp->f_flag);
		return (0);

	case F_SETFL:
		fp->f_flag &= ~FCNTLFLAGS;
		fp->f_flag |= FFLAGS(uap->arg) & FCNTLFLAGS;
		tmp = fp->f_flag & FNONBLOCK;
		error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		if (error)
			return (error);
		tmp = fp->f_flag & FASYNC;
		error = (*fp->f_ops->fo_ioctl)(fp, FIOASYNC, (caddr_t)&tmp, p);
		if (!error)
			return (0);
		fp->f_flag &= ~FNONBLOCK;
		tmp = 0;
		(void) (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		return (error);

	case F_GETOWN:
		if (fp->f_type == DTYPE_SOCKET) {
			*retval = ((struct socket *)fp->f_data)->so_pgid;
			return (0);
		}
		error = (*fp->f_ops->fo_ioctl)
			(fp, (int)TIOCGPGRP, (caddr_t)retval, p);
		*retval = -*retval;
		return (error);

	case F_SETOWN:
		if (fp->f_type == DTYPE_SOCKET) {
			((struct socket *)fp->f_data)->so_pgid = uap->arg;
			return (0);
		}
		if (uap->arg <= 0) {
			uap->arg = -uap->arg;
		} else {
			struct proc *p1 = pfind(uap->arg);
			if (p1 == 0)
				return (ESRCH);
			uap->arg = p1->p_pgrp->pg_id;
		}
		return ((*fp->f_ops->fo_ioctl)
			(fp, (int)TIOCSPGRP, (caddr_t)&uap->arg, p));

	case F_SETLKW:
		flg |= F_WAIT;
		/* Fall into F_SETLK */

	case F_SETLK:
		if (fp->f_type != DTYPE_VNODE)
			return (EBADF);
		vp = (struct vnode *)fp->f_data;
		/* Copy in the lock structure */
		error = copyin((caddr_t)uap->arg, (caddr_t)&fl, sizeof (fl));
		if (error)
			return (error);
		if (fl.l_whence == SEEK_CUR)
			fl.l_start += fp->f_offset;
		switch (fl.l_type) {

		case F_RDLCK:
			if ((fp->f_flag & FREAD) == 0)
				return (EBADF);
			p->p_flag |= P_ADVLOCK;
			error = VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &fl, flg);
			break;

		case F_WRLCK:
			if ((fp->f_flag & FWRITE) == 0)
				return (EBADF);
			p->p_flag |= P_ADVLOCK;
			error = VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &fl, flg);
			break;

		case F_UNLCK:
			return (VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &fl,
				F_POSIX));

		default:
			return (EINVAL);
		}
		/*
		 * AI-ONLY NOTE: a lock granted after the descriptor was
		 * closed -- by another thread, while this one waited
		 * (F_SETLKW) -- is given back: the close released the
		 * process's locks on the file before this one was made,
		 * and with no descriptor left nothing would release it
		 * (docs/deferred.md, S-23).  Found by an independent
		 * review of the call holds.
		 */
		if (error == 0 && fdp->fd_ofiles[uap->fd] != fp) {
			(void) VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &fl,
					   F_POSIX);
			error = EBADF;
		}
		return (error);

	case F_GETLK:
		if (fp->f_type != DTYPE_VNODE)
			return (EBADF);
		vp = (struct vnode *)fp->f_data;
		/* Copy in the lock structure */
		error = copyin((caddr_t)uap->arg, (caddr_t)&fl, sizeof (fl));
		if (error)
			return (error);
		if (fl.l_whence == SEEK_CUR)
			fl.l_start += fp->f_offset;
		if (error = VOP_ADVLOCK(vp, (caddr_t)p, F_GETLK, &fl, F_POSIX))
			return (error);
		return (copyout((caddr_t)&fl, (caddr_t)uap->arg, sizeof (fl)));

	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Common code for dup, dup2, and fcntl(F_DUPFD).
 */
int
finishdup(fdp, old, new, retval)
	register struct filedesc *fdp;
	register int old, new, *retval;
{
	register struct file *fp;

	/*
	 * AI-ONLY NOTE: new is reserved (fdalloc(), or dup2() below).
	 * old was checked by the caller, but dup2() may have slept
	 * closing new's file, and another thread of the process closed
	 * old meanwhile: then new is given back and the call is EBADF,
	 * as for an old never open.
	 */
	fp = fdp->fd_ofiles[old];
	if (fp == NULL) {
		fdunreserve(fdp, new);
		return (EBADF);
	}
	fdp->fd_ofiles[new] = fp;
	fdp->fd_ofileflags[new] = fdp->fd_ofileflags[old] &~ UF_EXCLOSE;
	fp->f_count++;
	if (new > fdp->fd_lastfile)
		fdp->fd_lastfile = new;
	*retval = new;
	return (0);
}

/*
 * Close a file descriptor.
 */
struct close_args {
	int	fd;
};
/* ARGSUSED */
close(p, uap, retval)
	struct proc *p;
	struct close_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	register int fd = uap->fd;
	register u_char *pf;

	if ((unsigned)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	pf = (u_char *)&fdp->fd_ofileflags[fd];
	if (*pf & UF_MAPPED)
		(void) munmapfd(p, fd);
	fdp->fd_ofiles[fd] = NULL;
	while (fdp->fd_lastfile > 0 && fdp->fd_ofiles[fdp->fd_lastfile] == NULL)
		fdp->fd_lastfile--;
	if (fd < fdp->fd_freefile)
		fdp->fd_freefile = fd;
	*pf = 0;
	return (closef(fp, p));
}

#if NO_EMULATOR
/*
 * AI-ONLY NOTE: 4.4BSD-Lite2's fstat() (kern/kern_descrip.c 8.8,
 * under the same notice as this file), for a process the server runs
 * without the emulator, which made the call itself from
 * bsd_getattr() (emulator/e_templates.h, DEF_FSTAT).  Sockets go to
 * soo_stat(), Lite2's, in Lites' vattr form (kern/sys_socket.c).
 * Not Lite2's panic for another type: open() puts its file in the
 * table before it has a type, and can sleep there (namei, a FIFO),
 * and another thread of the process can call fstat() on it then
 * (docs/deferred.md, S-23).  EBADF, as for a file not yet there.
 */
struct fstat_args {
	int	fd;
	struct	stat *sb;
};
/* ARGSUSED */
fstat(p, uap, retval)
	struct proc *p;
	register struct fstat_args *uap;
	int *retval;
{
	int fd = uap->fd;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct stat ub;
	int error;

	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fhold_call(fdp->fd_ofiles[fd])) == NULL)
		return (EBADF);
	switch (fp->f_type) {

	case DTYPE_VNODE:
		error = vn_stat((struct vnode *)fp->f_data, &ub, p);
		break;

	case DTYPE_SOCKET:
		error = soo_stat((struct socket *)fp->f_data, &ub);
		break;

	default:
		return (EBADF);
	}
	if (error == 0)
		error = copyout((caddr_t)&ub, (caddr_t)uap->sb, sizeof (ub));
	return (error);
}
#endif /* NO_EMULATOR */

/*
 * Return pathconf information about a file descriptor.
 */
struct fpathconf_args {
	int	fd;
	int	name;
};
/* ARGSUSED */
fpathconf(p, uap, retval)
	struct proc *p;
	register struct fpathconf_args *uap;
	int *retval;
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;

	if ((unsigned)uap->fd >= fdp->fd_nfiles ||
	    (fp = fhold_call(fdp->fd_ofiles[uap->fd])) == NULL)
		return (EBADF);
	switch (fp->f_type) {

	case DTYPE_SOCKET:
		if (uap->name != _PC_PIPE_BUF)
			return (EINVAL);
		*retval = PIPE_BUF;
		return (0);

	case DTYPE_VNODE:
		vp = (struct vnode *)fp->f_data;
		return (VOP_PATHCONF(vp, uap->name, retval));

	default:
		panic("fpathconf");
		return EFTYPE;
	}
	/*NOTREACHED*/
}

/*
 * Allocate a file descriptor for the process.
 */
int fdexpand;

fdalloc(p, want, result)
	struct proc *p;
	int want;
	int *result;
{
	register struct filedesc *fdp = p->p_fd;
	register int i;
	int lim, last, nfiles;
	struct file **newofile;
	char *newofileflags;

	/*
	 * Search for a free descriptor starting at the higher
	 * of want or fd_freefile.  If that fails, consider
	 * expanding the ofile array.
	 */
	lim = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles);
	for (;;) {
		last = min(fdp->fd_nfiles, lim);
		if ((i = want) < fdp->fd_freefile)
			i = fdp->fd_freefile;
		for (; i < last; i++) {
			/*
			 * AI-ONLY NOTE: a reserved slot is taken, though
			 * its fd_ofiles[] is NULL; the slot given out is
			 * reserved until fdinstall() or fdunreserve()
			 * (sys/filedesc.h, UF_RESERVED).
			 */
			if (fdp->fd_ofiles[i] == NULL &&
			    (fdp->fd_ofileflags[i] & UF_RESERVED) == 0) {
				fdp->fd_ofileflags[i] = UF_RESERVED;
				if (i > fdp->fd_lastfile)
					fdp->fd_lastfile = i;
				if (want <= fdp->fd_freefile)
					fdp->fd_freefile = i;
				*result = i;
				return (0);
			}
		}

		/*
		 * No space in current array.  Expand?
		 */
		if (fdp->fd_nfiles >= lim)
			return (EMFILE);
		if (fdp->fd_nfiles < NDEXTENT)
			nfiles = NDEXTENT;
		else
			nfiles = 2 * fdp->fd_nfiles;
		MALLOC(newofile, struct file **, nfiles * OFILESIZE,
		    M_FILEDESC, M_WAITOK);
		newofileflags = (char *) &newofile[nfiles];
		/*
		 * Copy the existing ofile and ofileflags arrays
		 * and zero the new portion of each array.
		 */
		bcopy(fdp->fd_ofiles, newofile,
			(i = sizeof(struct file *) * fdp->fd_nfiles));
		bzero((char *)newofile + i, nfiles * sizeof(struct file *) - i);
		bcopy(fdp->fd_ofileflags, newofileflags,
			(i = sizeof(char) * fdp->fd_nfiles));
		bzero(newofileflags + i, nfiles * sizeof(char) - i);
		if (fdp->fd_nfiles > NDFILE)
			FREE(fdp->fd_ofiles, M_FILEDESC);
		fdp->fd_ofiles = newofile;
		fdp->fd_ofileflags = newofileflags;
		fdp->fd_nfiles = nfiles;
		fdexpand++;
	}
}

/*
 * Check to see whether n user file descriptors
 * are available to the process p.
 */
fdavail(p, n)
	struct proc *p;
	register int n;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file **fpp;
	register char *flp;
	register int i, lim, last;

	/*
	 * AI-ONLY NOTE: a reserved slot is not free (sys/filedesc.h,
	 * UF_RESERVED); and slots at or above the limit are not
	 * counted, as fdalloc() above does not give them out
	 * (last = min(fd_nfiles, lim)).  4.4BSD counted them: the table
	 * grows by NDEXTENT whatever the limit, and with RLIMIT_NOFILE
	 * below its size fdavail() said yes where fdalloc() then said
	 * EMFILE, and unp_externalize() panics on that.
	 */
	lim = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles);
	if ((i = lim - fdp->fd_nfiles) > 0 && (n -= i) <= 0)
		return (1);
	last = min(fdp->fd_nfiles, lim);
	fpp = &fdp->fd_ofiles[fdp->fd_freefile];
	flp = &fdp->fd_ofileflags[fdp->fd_freefile];
	for (i = last - fdp->fd_freefile; --i >= 0; fpp++, flp++)
		if (*fpp == NULL && (*flp & UF_RESERVED) == 0 && --n <= 0)
			return (1);
	return (0);
}

/*
 * Create a new open file structure and allocate
 * a file decriptor for the process that refers to it.
 */
falloc(p, resultfp, resultfd)
	register struct proc *p;
	struct file **resultfp;
	int *resultfd;
{
	register struct file *fp, *fq, **fpp;
	int error, i;

	if (error = fdalloc(p, 0, &i))
		return (error);
	if (nfiles >= maxfiles) {
		tablefull("file");
		fdunreserve(p->p_fd, i);	/* AI-ONLY NOTE: below */
		return (ENFILE);
	}
	/*
	 * Allocate a new file descriptor.
	 * If the process has file descriptor zero open, add to the list
	 * of open files at that point, otherwise put it at the front of
	 * the list of open files.
	 */
	nfiles++;
	MALLOC(fp, struct file *, sizeof(struct file), M_FILE, M_WAITOK);
	bzero(fp, sizeof(struct file));
	if (fq = p->p_fd->fd_ofiles[0])
		fpp = &fq->f_filef;
	else
		fpp = &filehead;
	/*
	 * AI-ONLY NOTE: the file is not entered in fd_ofiles[i] here:
	 * slot i stays reserved, and the caller enters the file with
	 * fdinstall() once it is whole, or gives the slot back with
	 * fdunreserve() (sys/filedesc.h, UF_RESERVED).  4.4BSD entered
	 * it here, untyped, for the rest of the open to see.
	 */
	if (fq = *fpp)
		fq->f_fileb = &fp->f_filef;
	fp->f_filef = fq;
	fp->f_fileb = fpp;
	*fpp = fp;
	fp->f_count = 1;
	fp->f_cred = p->p_ucred;
	crhold(fp->f_cred);
#if FILE_PORTS
	mutex_init(&fp->f_lock);
	mutex_set_name(&fp->f_lock, "file_handle_lock");
	fp->f_port = MACH_PORT_NULL;
#endif
	if (resultfp)
		*resultfp = fp;
	if (resultfd)
		*resultfd = i;
	return (0);
}

/*
 * AI-ONLY NOTE: new.  Enter a whole file in a slot fdalloc() reserved,
 * or give such a slot back (sys/filedesc.h, UF_RESERVED).  The
 * slot's other flags are the caller's to set after fdinstall();
 * fdunreserve() leaves the hints as close() leaves them.
 */
void
fdinstall(fdp, fd, fp)
	register struct filedesc *fdp;
	int fd;
	struct file *fp;
{
	fdp->fd_ofiles[fd] = fp;
	fdp->fd_ofileflags[fd] &= ~UF_RESERVED;
	if (fd > fdp->fd_lastfile)
		fdp->fd_lastfile = fd;
}

void
fdunreserve(fdp, fd)
	register struct filedesc *fdp;
	int fd;
{
	fdp->fd_ofileflags[fd] = 0;
	while (fdp->fd_lastfile > 0 && fdp->fd_ofiles[fdp->fd_lastfile] == NULL &&
	    (fdp->fd_ofileflags[fdp->fd_lastfile] & UF_RESERVED) == 0)
		fdp->fd_lastfile--;
	if (fd < fdp->fd_freefile)
		fdp->fd_freefile = fd;
}

/*
 * Free a file descriptor.
 */
ffree(fp)
	register struct file *fp;
{
	register struct file *fq;

	if ((fq = fp->f_filef) && fq->f_fileb)
		fq->f_fileb = fp->f_fileb;
	if (fp->f_fileb)
	    *fp->f_fileb = fq;
	crfree(fp->f_cred);
#if DIAGNOSTIC
	fp->f_filef = NULL;
	fp->f_fileb = NULL;
	fp->f_count = 0;
#endif
	nfiles--;
	FREE(fp, M_FILE);
}

/*
 * Copy a filedesc structure.
 */
struct filedesc *
fdcopy(p)
	struct proc *p;
{
	register struct filedesc *newfdp, *fdp = p->p_fd;
	register struct file **fpp;
	register int i;

	MALLOC(newfdp, struct filedesc *, sizeof(struct filedesc0),
	    M_FILEDESC, M_WAITOK);
	bcopy(fdp, newfdp, sizeof(struct filedesc));
	VREF(newfdp->fd_cdir);
	if (newfdp->fd_rdir)
		VREF(newfdp->fd_rdir);
	newfdp->fd_refcnt = 1;

	/*
	 * If the number of open files fits in the internal arrays
	 * of the open file structure, use them, otherwise allocate
	 * additional memory for the number of descriptors currently
	 * in use.
	 */
	if (newfdp->fd_lastfile < NDFILE) {
		newfdp->fd_ofiles = ((struct filedesc0 *) newfdp)->fd_dfiles;
		newfdp->fd_ofileflags =
		    ((struct filedesc0 *) newfdp)->fd_dfileflags;
		i = NDFILE;
	} else {
		/*
		 * Compute the smallest multiple of NDEXTENT needed
		 * for the file descriptors currently in use,
		 * allowing the table to shrink.
		 */
		i = newfdp->fd_nfiles;
		while (i > 2 * NDEXTENT && i > newfdp->fd_lastfile * 2)
			i /= 2;
		MALLOC(newfdp->fd_ofiles, struct file **, i * OFILESIZE,
		    M_FILEDESC, M_WAITOK);
		newfdp->fd_ofileflags = (char *) &newfdp->fd_ofiles[i];
	}
	newfdp->fd_nfiles = i;
	bcopy(fdp->fd_ofiles, newfdp->fd_ofiles, i * sizeof(struct file **));
	bcopy(fdp->fd_ofileflags, newfdp->fd_ofileflags, i * sizeof(char));
	/*
	 * AI-ONLY NOTE: a slot the parent has reserved is a call of the
	 * parent's still in progress; the child gets the slot free
	 * (sys/filedesc.h, UF_RESERVED).
	 */
	while (--i >= 0)
		if (newfdp->fd_ofileflags[i] & UF_RESERVED) {
			newfdp->fd_ofileflags[i] = 0;
			if (i < newfdp->fd_freefile)
				newfdp->fd_freefile = i;
		}
	fpp = newfdp->fd_ofiles;
	for (i = newfdp->fd_lastfile; i-- >= 0; fpp++)
		if (*fpp != NULL)
			(*fpp)->f_count++;
	return (newfdp);
}

/*
 * Release a filedesc structure.
 */
void
fdfree(p)
	struct proc *p;
{
	register struct filedesc *fdp = p->p_fd;
	struct file **fpp;
	register int i;

	if (--fdp->fd_refcnt > 0)
		return;
	fpp = fdp->fd_ofiles;
	for (i = fdp->fd_lastfile; i-- >= 0; fpp++)
		if (*fpp)
			(void) closef(*fpp, p);
	if (fdp->fd_nfiles > NDFILE)
		FREE(fdp->fd_ofiles, M_FILEDESC);
	vrele(fdp->fd_cdir);
	if (fdp->fd_rdir)
		vrele(fdp->fd_rdir);
	FREE(fdp, M_FILEDESC);
}

/*
 * Internal form of close.
 * Decrement reference count on file structure.
 * Note: p may be NULL when closing a file
 * that was being passed in a message.
 */
static int closef_last __P((struct file *, struct proc *));

closef(fp, p)
	register struct file *fp;
	register struct proc *p;
{
	struct vnode *vp;
	struct flock lf;

	if (fp == NULL)
		return (0);
	/*
	 * POSIX record locking dictates that any close releases ALL
	 * locks owned by this process.  This is handled by setting
	 * a flag in the unlock to free ONLY locks obeying POSIX
	 * semantics, and not to free BSD-style file locks.
	 * If the descriptor was in a message, POSIX-style locks
	 * aren't passed with the descriptor.
	 */
	if (p && (p->p_flag & P_ADVLOCK) && fp->f_type == DTYPE_VNODE) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		lf.l_type = F_UNLCK;
		vp = (struct vnode *)fp->f_data;
		(void) VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &lf, F_POSIX);
	}
	if (--fp->f_count > 0)
		return (0);
	return (closef_last(fp, p));
}

/*
 * AI-ONLY NOTE: closef()'s second half, split out so that a call's
 * hold (fhold_call() below) can be given back without the first
 * half: dropping a hold is not a close, and must not release the
 * process's POSIX locks.
 */
static int
closef_last(fp, p)
	register struct file *fp;
	register struct proc *p;
{
	struct vnode *vp;
	struct flock lf;
	int error;

	if (fp->f_count < 0)
		panic("closef: count < 0");
	if ((fp->f_flag & FHASLOCK) && fp->f_type == DTYPE_VNODE) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		lf.l_type = F_UNLCK;
		vp = (struct vnode *)fp->f_data;
		(void) VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK);
	}
#if FILE_PORTS
	if (MACH_PORT_VALID(fp->f_port)) {
		port_object_shutdown(fp->f_port, TRUE);
		fp->f_port = MACH_PORT_NULL;
	}
#endif	
	if (fp->f_ops)
		error = (*fp->f_ops->fo_close)(fp, p);
	else
		error = 0;
	ffree(fp);
	return (error);
}

/*
 * AI-ONLY NOTE: the second half of docs/deferred.md, S-23.  A process
 * here may have several threads in the server at once, and a call
 * that finds a file by its descriptor and then sleeps -- a read of a
 * pipe, a wait for a lock, any disk I/O -- held no reference on it,
 * so another thread's close() could free the file under it.  As
 * Design of the OSF/1 Operating System, 11.1.2, has it: "the thread
 * obtaining the descriptor increments the reference count on the open
 * file description ..., preventing deallocation.  The reference is
 * released when the operation is completed."  Written here: a lookup
 * passes the file through fhold_call(), which counts it in f_count
 * and notes it in the call's invocation; server_thread_deregister()
 * gives every such reference back with frele_call() when the call
 * ends, however it ends.  Only while the thread is registered for a
 * call (k_serving: system threads keep k_p, and START_SERVER sets it
 * before registering), so that every hold is given back; a call
 * holding more than K_NHELD files is refused its descriptor.
 */
struct file *
fhold_call(fp)
	struct file *fp;
{
	proc_invocation_t pk = get_proc_invocation();

	if (fp == NULL || pk == NULL || !pk->k_serving)
		return (fp);
	if (pk->k_nheld >= K_NHELD) {
		printf("(lites) pid %d: a call holds more than %d files\n",
		       pk->k_p->p_pid, K_NHELD);
		return (NULL);
	}
	fp->f_count++;
	pk->k_held[pk->k_nheld++] = fp;
	return (fp);
}

/*
 * Give back the call's references; a file whose last it was is closed
 * now, as the close() that came first would have closed it.  Under the
 * master lock, for closef()'s sake.
 */
void
frele_call(p)
	struct proc *p;
{
	proc_invocation_t pk = get_proc_invocation();
	struct file *fp;

	while (pk->k_nheld > 0) {
		fp = pk->k_held[--pk->k_nheld];
		pk->k_held[pk->k_nheld] = NULL;
		if (--fp->f_count == 0)
			(void) closef_last(fp, p);
	}
}

/*
 * Apply an advisory lock on a file descriptor.
 *
 * Just attempt to get a record lock of the requested type on
 * the entire file (l_whence = SEEK_SET, l_start = 0, l_len = 0).
 */
struct flock_args {
	int	fd;
	int	how;
};
/* ARGSUSED */
flock(p, uap, retval)
	struct proc *p;
	register struct flock_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	struct flock lf;

	if ((unsigned)uap->fd >= fdp->fd_nfiles ||
	    (fp = fhold_call(fdp->fd_ofiles[uap->fd])) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (EOPNOTSUPP);
	vp = (struct vnode *)fp->f_data;
	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	if (uap->how & LOCK_UN) {
		lf.l_type = F_UNLCK;
		fp->f_flag &= ~FHASLOCK;
		return (VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK));
	}
	if (uap->how & LOCK_EX)
		lf.l_type = F_WRLCK;
	else if (uap->how & LOCK_SH)
		lf.l_type = F_RDLCK;
	else
		return (EBADF);
	fp->f_flag |= FHASLOCK;
	if (uap->how & LOCK_NB)
		return (VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, F_FLOCK));
	return (VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, F_FLOCK|F_WAIT));
}

/*
 * File Descriptor pseudo-device driver (/dev/fd/).
 *
 * Opening minor device N dup()s the file (if any) connected to file
 * descriptor N belonging to the calling process.  Note that this driver
 * consists of only the ``open()'' routine, because all subsequent
 * references to this file will be direct to the other driver.
 */
/* ARGSUSED */
fdopen(dev, mode, type, p)
	dev_t dev;
	int mode, type;
	struct proc *p;
{

	/*
	 * XXX Kludge: set curproc->p_dupfd to contain the value of the
	 * the file descriptor being sought for duplication. The error 
	 * return ensures that the vnode for this device will be released
	 * by vn_open. Open will detect this special error and take the
	 * actions in dupfdopen below. Other callers of vn_open or VOP_OPEN
	 * will simply report the error.
	 */
	p->p_dupfd = minor(dev);
	return (ENODEV);
}

/*
 * Duplicate the specified descriptor to a free descriptor.
 */
dupfdopen(fdp, indx, dfd, mode, error)
	register struct filedesc *fdp;
	register int indx, dfd;
	int mode;
	int error;
{
	register struct file *wfp;
	struct file *fp;
	
	/*
	 * If the to-be-dup'd fd number is greater than the allowed number
	 * of file descriptors, or the fd to be dup'd has already been
	 * closed, reject.  Note, check for new == old is necessary as
	 * falloc could allocate an already closed to-be-dup'd descriptor
	 * as the new descriptor.
	 */
	fp = fdp->fd_ofiles[indx];
	if ((u_int)dfd >= fdp->fd_nfiles ||
	    (wfp = fdp->fd_ofiles[dfd]) == NULL || fp == wfp)
		return (EBADF);

	/*
	 * There are two cases of interest here.
	 *
	 * For ENODEV simply dup (dfd) to file descriptor
	 * (indx) and return.
	 *
	 * For ENXIO steal away the file structure from (dfd) and
	 * store it in (indx).  (dfd) is effectively closed by
	 * this operation.
	 *
	 * Any other error code is just returned.
	 */
	switch (error) {
	case ENODEV:
		/*
		 * Check that the mode the file is being opened for is a
		 * subset of the mode of the existing descriptor.
		 */
		if (((mode & (FREAD|FWRITE)) | wfp->f_flag) != wfp->f_flag)
			return (EACCES);
		fdp->fd_ofiles[indx] = wfp;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		wfp->f_count++;
		if (indx > fdp->fd_lastfile)
			fdp->fd_lastfile = indx;
		return (0);

	case ENXIO:
		/*
		 * Steal away the file pointer from dfd, and stuff it into indx.
		 */
		fdp->fd_ofiles[indx] = fdp->fd_ofiles[dfd];
		fdp->fd_ofiles[dfd] = NULL;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		fdp->fd_ofileflags[dfd] = 0;
		/*
		 * Complete the clean up of the filedesc structure by
		 * recomputing the various hints.
		 */
		if (indx > fdp->fd_lastfile)
			fdp->fd_lastfile = indx;
		else
			while (fdp->fd_lastfile > 0 &&
			       fdp->fd_ofiles[fdp->fd_lastfile] == NULL)
				fdp->fd_lastfile--;
			if (dfd < fdp->fd_freefile)
				fdp->fd_freefile = dfd;
		return (0);

	default:
		return (error);
	}
	/* NOTREACHED */
}
