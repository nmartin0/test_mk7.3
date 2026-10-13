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
# Revision 1.1.1.1  1995/03/02  21:49:33  mike
# Initial Lites release from hut.fi
#
#
#	File:	include/sys/Makefile
#	Author:	Johannes Helander, Helsinki University of Technology, 1994.
#

IDIR		= /include/sys/

INCLUDES = acct.h gprof.h parallel.h synch.h uio.h buf.h ioctl.h \
	param.h syslimits.h un.h ioctl_compat.h proc.h \
	syslog.h unistd.h cdefs.h ipc.h protosw.h systm.h unpcb.h \
	clist.h kernel.h ptrace.h table.h user.h conf.h \
	queue.h ushared.h dir.h reboot.h \
	termios.h ux_exception.h resource.h \
	time.h vadvise.h macro_help.h resourcevar.h timeb.h \
	malloc.h times.h vlimit.h  \
	shared_lock.h domain.h \
	errno.h mbuf.h signal.h tty.h vnode.h exec.h \
	mman.h signalvar.h ttychars.h fcntl.h mount.h socket.h \
	ttydefaults.h msgbuf.h socketvar.h ttydev.h \
	wait.h file.h mtio.h types.h zalloc.h filedesc.h \
	namei.h stat.h ucred.h dirent.h ttycom.h ioccom.h filio.h sockio.h \
	select.h som.h coff.h


ILIST		= ${INCLUDES}

.include <${RULES_MK}>
