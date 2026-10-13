# 
# Mach Operating System
# Copyright (c) 1994 Johannes Helander
# All Rights Reserved.
# 
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
# 
# JOHANNES HELANDER ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# CONDITION.  JOHANNES HELANDER DISCLAIMS ANY LIABILITY OF ANY KIND
# FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
#
# HISTORY
# $Log: Makefile,v $
# Revision 1.1.1.1  1995/03/02  21:49:30  mike
# Initial Lites release from hut.fi
#
#

LIBRARIES	= libsyscall.a
EXPLIB_TARGETS	= export_libsyscall.a
EXPDIR		= /lib/
IDIR		= ${EXPDIR}
ILIST		= ${LIBRARIES}

VPATH = ../libsys

INCFLAGS	= -I../../server -I..
DEFINES		= -DCMU=1 -DTypeCheck=0 -DMACH_IPC_COMPAT=0 -DCOMPAT_43 -DEMULATOR
# -DMAP_UAREA=1
CFLAGS		= ${DEFINES} -g -Wall

OFILES = \
	accept.o access.o acct.o adjtime.o async_daemon.o bind.o \
	chdir.o chflags.o chmod.o chown.o chroot.o close.o \
	connect.o dup.o dup2.o execve.o fchdir.o fchflags.o fchmod.o \
	fchown.o fcntl.o flock.o fork.o fstat.o fstatfs.o fsync.o \
	ftruncate.o getdirentries.o getdomainname.o getdtablesize.o \
	getegid.o geteuid.o getfh.o getfsstat.o getgid.o getgroups.o \
	gethostid.o gethostname.o getitimer.o getkerninfo.o getlogin.o \
	getpeername.o getpgrp.o getpid.o getppid.o \
	getpriority.o getrlimit.o getrusage.o getsockname.o \
	getsockopt.o gettimeofday.o getuid.o ioctl.o kill.o link.o \
	listen.o lseek.o lstat.o madvise.o mincore.o mkdir.o mkfifo.o \
	mknod.o mmap.o mount.o mprotect.o msync.o munmap.o nfssvc.o \
	open.o pipe.o quotactl.o read.o readlink.o readv.o reboot.o \
	recvfrom.o recvmsg.o rename.o revoke.o rmdir.o sbrk.o select.o \
	sendmsg.o sendto.o setdomainname.o setegid.o seteuid.o \
	setgid.o setgroups.o sethostid.o sethostname.o setitimer.o \
	setlogin.o setpgid.o setpriority.o setrlimit.o setsid.o \
	setsockopt.o settimeofday.o setuid.o shmsys.o shutdown.o \
	sigaction.o sigpending.o sigprocmask.o sigreturn.o sigstack.o \
	sigsuspend.o socket.o socketpair.o stat.o statfs.o swapon.o \
	symlink.o sync.o sysctrace.o table.o truncate.o \
	uname.o unlink.o unmount.o utimes.o vfork.o wait4.o write.o \
	writev.o

# break.o # reserved word in C.
# umask.o getpagesize.o # done in libe.c directly.

.include <${RULES_MK}>
