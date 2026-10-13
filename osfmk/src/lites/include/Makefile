# 
# Mach Operating System
# Copyright (c) 1992 Carnegie Mellon University
# Copyright (c) 1994 Johannes Helander
# All Rights Reserved.
# 
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
# 
# CARNEGIE MELLON AND JOHANNES HELANDER ALLOW FREE USE OF THIS
# SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND JOHANNES
# HELANDER DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
# WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# HISTORY
# $Log: Makefile,v $
# Revision 1.1.1.1  1995/03/02  21:49:32  mike
# Initial Lites release from hut.fi
#
# $EndLog$
#
#	File:	 include/Makefile
#	Authors: Mary Thompson, Johannes Helander
#	Date:	 1992, 1994
#
#  This makefile creates a symlink named machine to the
#  directory ${KERN_MACHINE_DIR} in the exports/usr/include
#  area. ${KERN_MACHINE_DIR} is usually the same as ${target_machine}
#  but is given a variable of its own, for machines where it might be
#  different.
#

EXPINC_SUBDIRS	= ${KERN_MACHINE_DIR} serv sys ufs
EXPORT_USING_TAR=

OTHERS		= machine errno.h

INCLUDES	= ${OTHERS} time.h dirent.h
# dirent.h 

EXPDIR		= /include/

.include <${RULES_MK}>

machine: ${ALWAYS}
	${RM} ${_RMFLAGS_} machine
	${LN} -s ${KERN_MACHINE_DIR} machine

errno.h :
	@${RM} ${_RMFLAGS_} ${.TARGET}
	echo '#include <sys/errno.h>' > ${.TARGET}
	${CHMOD} 644 ${.TARGET}
