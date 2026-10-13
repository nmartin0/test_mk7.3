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
# 25-Oct-94  Johannes Helander (jvh) at Helsinki University of Technology
#	Merged in PA RISC support from Mike Hibler and PC532 support from
#	Ian Dall.
#
#  $Log: Makefile,v $
# Revision 1.3  1995/03/23  01:38:13  law
# Update to 950323 snapshot + utah changes
#
# Revision 1.2  1995/03/02  23:27:39  mike
# Utah fast RPC support
#
# Revision 1.3  1995/03/02  23:06:16  mike
# hack support for LRRPC path
#
# Revision 1.2  1995/02/21  23:37:40  mike
# 0.7 version
#
#
#	File:	emulator/Makefile
#	Author:	Johannes Helander, Helsinki University of Technology, 1994.
#	Date:	May 1994
#

VPATH		= ${target_cpu} extra programs

EXPINC_SUBDIRS	= sys
# EXPLIB_SUBDIRS	= libsyscall

OTHERS		= ${EMULATOR}
DEPENDENCIES	=
MIG_HDRS	=
PROGRAMS	=
# zprintbsd testprog runit
ILIST		= ${EMULATOR}
IDIR		= /special/
NO_STRIP	=

INCFLAGS	= -I${MAKETOP}server -I${MAKETOP}include

# The EXPORTBASE/bsdss directory contains the include files
# in machine

INCDIRS		:= -I${EXPORTBASE} ${INCDIRS}

# Makefile-version defines VERSION

.if exists( ${MAKETOP}Makefile-version)
.include "${MAKETOP}Makefile-version"
.endif

EMULATOR        = emulator.${VERSION}

parisc_DEFINES	= -DUSENRPC

DEFINES		= -DCMU=1 -DTypeCheck=0 -DMACH_IPC_COMPAT=0 -DCOMPAT_43 -DEMULATOR -DMAP_UAREA -DNFS -DLITES -DASYNCH_SIGNALS ${${target_cpu}_DEFINES}
CFLAGS		= ${DEFINES} -g -nostdinc -I-
PC532_CFLAGS	= -mhimem -DGNU_LD2 -Dns532

MIGFLAGS 	= ${DEFINES}
ASFLAGS		= ${${TARGET_MACHINE}ASFLAGS}

${EMULATOR}_LIBS= -nostdlib -llites ${LIBGCC} ${LIBTHREADS} ${LIBMACH_SA}
testprog_LIBS	= -nostdlib ${LIBGCC} -lsyscall ${LIBTHREADS} ${LIBMACH_SA}
runit_LIBS	= -nostdlib ${LIBGCC} -lsyscall ${LIBTHREADS} ${LIBMACH_SA}
zprintbsd_LIBS	= ${LIBCMUCS} ${LIBTHREADS} ${LIBMACH}


ecrt0.o_CFLAGS	+= -DCRT0
strtol.o_CFLAGS += -I${_MAKECONF:H}/emulator/extra
atoi.o_CFLAGS 	+= -I${_MAKECONF:H}/emulator/extra
ctype_.o_CFLAGS += -I${_MAKECONF:H}/emulator/extra
quit.o_CFLAGS += -I${_MAKECONF:H}/emulator/extra
zprintbsd.o_CFLAGS += -I${_MAKECONF:H}/emulator/extra

i386_COMMON_OFILES	= e_linux.o 
parisc_COMMON_OFILES	= 

COMMON_OFILES	= ecrt0.o e_mig_support.o emul_generic.o \
	error_codes.o emul_init.o emul_mapped.o \
	e_machinedep.o emul_misc_asm.o \
	e_bsd.o e_43ux.o bsd_1_user.o \
	emul_mach_user.o e_signal.o signal_server.o e_uname.o \
	e_mapped_time.o e_readwrite.o e_sysvipc.o \
	${${target_cpu}_COMMON_OFILES}

testprog_OFILES	= ${COMMON_OFILES} testprog.o libsys_support.o
runit_OFILES	= ${COMMON_OFILES} runit.o libsys_support.o
zprintbsd_OFILES= zprintbsd.o bsd_1_user.o process_self.o

i386_EMULATOR_OFILES	= e_linux_sysent.o e_linux_trampoline.o \
	e_isc4_sysent.o e_sysv.o e_cmu_43ux_sysent.o e_lite_sysent.o
ns532_EMULATOR_OFILES	= e_cmu_43ux_sysent.o e_lite_sysent.o
parisc_EMULATOR_OFILES	= e_hpbsd_sysent.o e_hpbsd.o e_lite_sysent.o \
	LRbsd_1_user.o LRmach_user.o

EMULATOR_OFILES	= ${COMMON_OFILES} emul_vector.o emul_exec.o \
	e_trampoline.o e_exception.o e_mach_msg_server.o \
	e_stat.o e_bnr.o \
	${${target_cpu}_EMULATOR_OFILES}

# For dependencies
DEP_OFILES	= e_mig_support.o emul_generic.o error_codes.o emul_init.o \
	emul_mapped.o mvfs_interface.o e_machinedep.o emul_misc_asm.o atoi.o \
	strtol.o ctype_.o e_bsd.o e_43ux.o \
	e_linux.o emul_vector.o emul_exec.o \
	e_trampoline.o e_lite_sysent.o e_isc4_sysent.o e_linux_sysent.o \
	e_sysv.o e_linux_trampoline.o e_exception.o e_mach_msg_server.o \
	e_cmu_43ux_sysent.o zprintbsd.o process_self.o \
	testprog.o libsys_support.o e_signal.o e_bnr.o e_stat.o \
	e_uname.o e_mapped_time.o e_readwrite.o \
	e_hpbsd_sysent.o e_hpbsd.o e_sysvipc.o \
	LRbsd_1_user.o LRmach_user.o

.include <${RULES_MK}>

BASE="`./emulator_base`"

I386_LDFLAGS	= -z -T ${BASE}
PC532_LDFLAGS	= -Ttext ${BASE}
PARISC_LDFLAGS	= -Wl,-x -Wl,-N -Wl,-u,'$$START$$' -Wl,-R,${BASE}

${EMULATOR}: ${EMULATOR_OFILES} emulator_base
	@echo emulator base is \"${BASE}\"
	${_LD_} -o ${.TARGET}.out ${_LDFLAGS_} ${EMULATOR_OFILES} ${_LIBS_}  && \
	${MV} ${.TARGET}.out ${.TARGET}
	${CP} ${EMULATOR} ${EXPORTBASE}/special/${EMULATOR}

# Some dependencies

${DEP_OFILES} : bsd_1.h
${DEP_OFILES} : sys/syscall.h
sys/syscall.h : emul_sysent.c

#
#  The sysent tables, mapping trap numbers to actions, are generated
#  automatically on both the emulator and the server side:
#

emul_sysent.c: ../server/kern/makesyscalls.sh ../server/kern/syscalls.master
	-rm -rf tmp sys libjacket libsys
	-mkdir tmp sys libjacket libsys
	cd tmp;/bin/sh ${../server/kern/makesyscalls.sh:P} \
			${../server/kern/syscalls.master:P}

#
#  Mach IPC-based interfaces
#

MAKE_SERV = [ -d serv ] || mkdir serv

BSD_FILES = bsd_1.h serv/bsd_1_user.c

$(BSD_FILES): bsd_types_gen.h serv/bsd_1.defs
	-$(MAKE_SERV)
	$(MIG) $(_MIGFLAGS_) ${DEFINES} -UKERNEL \
		-header bsd_1.h \
		-user serv/bsd_1_user.c \
		-server /dev/null \
		${serv/bsd_1.defs:P}

bsd_1_user.o: serv/bsd_1_user.c
	$(_CC_) $(_CCFLAGS_) -Iserv -c -o bsd_1_user.o ${serv/bsd_1_user.c:P}

SIG_FILES = signal_server.h serv/signal_server.c

$(SIG_FILES): bsd_types_gen.h serv/signal.defs
	-$(MAKE_SERV)
	$(MIG) $(_MIGFLAGS_) ${DEFINES} -UKERNEL \
		-sheader signal_server.h \
		-user /dev/null \
		-server serv/signal_server.c \
		${serv/signal.defs:P}

signal_server.o: serv/signal_server.c
	$(_CC_) $(_CCFLAGS_) -c -o signal_server.o ${serv/signal_server.c:P}

#
# We have to tell Mig to generate a header file, even though we don't really
# want it, because the user file includes the header file from the current
# directory.
#

mach_user.c: mach/mach.defs
	$(MIG) $(_MIGFLAGS_) -DEMULATOR \
		-header mach_interface.h \
		-user mach_user.c \
		-server /dev/null \
		${mach/mach.defs:P}

# Generate a special version of task_suspend
emul_mach_user.c: emul_mach.defs
	${MIG} ${_MIGFLAGS_} \
		-header emul_mach_interface.h \
		-user emul_mach_user.c \
		-server /dev/null \
		${emul_mach.defs:P}

#
# bsd_types_gen needs to be preprocessed as for the target machine but run
# on the host machine.  In order to get the correct predefines it is
# necessary to use the target preprocessor and target include directories.


bsd_types_gen_CCTYPE	= host
HOST_CFLAGS		= ${DEFINES}
HOST_LDFLAGS		= ${LDFLAGS}


xxx_bsd_types_gen.c: serv/bsd_types_gen.c
	${ansi_CPP} -P ${_CCFLAGS_} ${serv/bsd_types_gen.c:P} > xxx_bsd_types_gen.c
	cat /dev/null >> bsd_types_gen.d
	sed 's/^bsd_types_gen\.o/xxx_bsd_types_gen.c/' \
			bsd_types_gen.d > xxx_bsd_types_gen.c.d
	${RM} bsd_types_gen.d

bsd_types_gen: xxx_bsd_types_gen.c
	( LPATH="${_host_LPATH_}"; export LPATH; \
	${HOST_CC} ${_host_FLAGS_} -o bsd_types_gen.X xxx_bsd_types_gen.c)
	${MV} bsd_types_gen.X bsd_types_gen

bsd_types_gen.h : bsd_types_gen
	-$(MAKE_SERV)
	./bsd_types_gen > bsd_types_gen.h

### emulator_base needs special treatment
emulator_base_CCTYPE	= host
xxx_emulator_base.o_CCTYPE	= host
emulator_base_OFILES	= xxx_emulator_base.o
emulator_base_LIBS	= 

# even though we preprocess emulator_base with the target cpp,
# to get appropriate predefines, we need to define ${TARGET_MACHINE},
# because it isn't always predefined.

# The -P switch is supposed to eliminate the #line and #define directives
# from the cpp output.

xxx_emulator_base.c: emulator_base.c
	${ansi_CPP} -P ${_CCFLAGS_} -D${TARGET_MACHINE} -DTARGET_${TARGET_MACHINE} ${emulator_base.c:P} > xxx_emulator_base.c
	cat /dev/null >> emulator_base.d
	sed 's/emulator_base\.o/xxx_emulator_base.c/' \
			emulator_base.d > xxx_emulator_base.c.d
	${RM} emulator_base.d

emulator_base: xxx_emulator_base.c
	( LPATH="${_host_LPATH_}"; export LPATH; \
	${HOST_CC} ${_host_FLAGS_} -o emulator_base.out xxx_emulator_base.c)
	${MV} emulator_base.out emulator_base
