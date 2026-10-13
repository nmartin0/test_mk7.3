# 
# Mach Operating System
# Copyright (c) 1993,1992 Carnegie Mellon University
# All Rights Reserved.
# 
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
# 
# CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
# ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
# 
# Carnegie Mellon requests users of this software to return to
# 
#  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
#  School of Computer Science
#  Carnegie Mellon University
#  Pittsburgh PA 15213-3890
# 
# any improvements or extensions that they make and grant Carnegie Mellon
# the rights to redistribute these changes.
# 
# 
# HISTORY
# $Log: Makefile,v $
# Revision 1.1.1.1  1995/03/02  21:49:37  mike
# Initial Lites release from hut.fi
#
# Revision 2.4  93/08/10  18:22:21  mrt
# 	Added dependencies on bsd_types_gen.h for proxy_socket.h
# 	and net_proxy.h
# 	[93/08/09            mrt]
# 
# Revision 2.3  93/08/07  16:49:34  mrt
# 	Exported files for proxy networking.
# 	[93/06/13            cmaeda]
# 
# Revision 2.2  93/02/16  15:56:04  mrt
# 	Updated for ODE make.
# 	[92/12/25            mrt]
# 

DEPENDENCIES	=

IDIR		= /include/serv/
VPATH		= ${MAKETOP}server/serv
INCFLAGS	+= -I.. -I${MAKETOP}server -I${MAKETOP}
INCDIRS		:= -I${EXPORTBASE}/server ${INCDIRS}

MIGFLAGS = -DCOMPAT_43 -DEMULATOR
MIG_HDRS = bsd_1.h

INCLUDES	= bsd_types.h bsd_types.defs bsd_1.defs ${MIG_HDRS}
ILIST		= ${INCLUDES}

bsd_1.h : bsd_types_gen.h

# bsd_types_genX needs to be preprocessed as for the target machine but run
# on the host machine.  In order to get the correct predefines it is
# necessary to use the target preprocessor and target include directories.

PROGRAMS	= bsd_types_gen

bsd_types_gen_OFILES	= xxx_bsd_types_gen.o
bsd_types_gen_CCTYPE	= host
xxx_bsd_types_gen.o_CCTYPE	= host

.include <${RULES_MK}>

xxx_bsd_types_gen.c : bsd_types_gen.c
	${ansi_CPP} -P ${_CCFLAGS_} ${bsd_types_gen.c:P} > \
		 xxx_bsd_types_gen.c
	cat /dev/null >> bsd_types_gen.d
	sed 's/^bsd_types_gen\.o/xxx_bsd_types_gen.c/' \
			bsd_types_gen.d > xxx_bsd_types_gen.c.d
	${RM} bsd_types_gen.d
	
bsd_types_gen.h : bsd_types_gen
	./bsd_types_gen > bsd_types_gen.h



