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
#  HISTORY
#  $Log: Makefile,v $
# Revision 1.1.1.1  1995/03/02  21:49:39  mike
# Initial Lites release from hut.fi
#
#
#	File:	emulator/Makefile
#	Author:	Johannes Helander, Helsinki University of Technology, 1994.
#

LIBNAME=lites

VPATH		= ${target_cpu}

LIBRARIES	= lib${LIBNAME}.a
DEPENDENCIES	=
MIG_HDRS	=

EXPLIB_TARGETS  = ${LIBRARIES:S/^/export_&/g}
ILIST		= lib${LIBNAME}.a
IDIR		= /lib/
NO_STRIP	=

INCFLAGS	= -I${MAKETOP}server -I${MAKETOP}include

# The EXPORTBASE/bsdss directory contains the include files
# in machine

INCDIRS		:= -I${EXPORTBASE} ${INCDIRS}

# Makefile-version defines VERSION

.if exists( ${MAKETOP}Makefile-version)
.include "${MAKETOP}Makefile-version"
.endif

DEFINES		= -DTypeCheck=0 -DMACH_IPC_COMPAT=0 -DLITES
CFLAGS		= ${DEFINES} -g -nostdinc -I-
PC532_CFLAGS	= -mhimem -DGNU_LD2 -Dns532

MIGFLAGS 	= ${DEFINES}
ASFLAGS		= ${${TARGET_MACHINE}ASFLAGS}

i386_OFILES = ntoh.o memcmp.o in_cksum.o
parisc_OFILES =
ns532_OFILES = misc_asm.s memcmp.o in_cksum.o bcmp.o
mips_OFILES = ntoh.o memcmp.o
alpha_OFILES =

lib${LIBNAME}.a_OFILES = ${${target_cpu}_OFILES} exec_file.o

.include <${RULES_MK}>
