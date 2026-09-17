#
# libmach_sa -- the standalone build of libmach.
#
# AI-ONLY NOTE: this directory did not exist. It is added, not modified.
#
# Several things link -lmach_sa: LITES's emulator and server
# (emulator/Makerules:37, server/Makerules:41) and OSFMK's own
# mach_perf/monitor/Makefile mentions it. Nothing in the tree built it.
#
# The only standalone libmach here was libmach_sa_p, which is profiled
# and does not build: it references ${MIG3}, which is defined nowhere in
# the tree, and its VPATH omits ${MAKETOP}default_pager/mach so it
# cannot find default_pager_object.defs. That is why it is absent from
# EXPLIB_SUBDIRS in ../Makefile.
#
# So this follows the pattern libmach_p uses instead -- a thin wrapper
# that sets LOCAL_LIBMACH and LOCAL_CFLAGS and includes libmach's
# common.mk -- which is the tree's own convention for a libmach variant
# and is known to work.
#
# -DSTANDALONE is what selects the standalone behaviour, notably making
# mach_init global rather than static (mach_init_sa.c against
# mach_init.c), which is the symbol LITES's emulator links against.
#
USE_STATIC_LIBRARIES	=
LOCAL_LIBMACH		= libmach_sa.a
LOCAL_CFLAGS		= -DSTANDALONE
LOCAL_MIGFLAGS		= -DSTANDALONE
MACH_INIT_OFILE		= mach_init_sa.o

#
# common.mk includes ${LIBMACH_INCLUDE_PREFIX}${TARGET_MACHINE}/machdep.mk
# to pick up the machine-dependent objects -- on AT386 those are
# mach_traps.o, mach_host_priv_self.o and _setjmp.o, which hold the Mach
# trap entry points that every Mach program needs. The variable is never
# set anywhere in the tree, so it defaults to empty and the include only
# works for libmach itself, which has its own AT386 directory. Pointing
# it at libmach is what the hook is for.
#
LIBMACH_INCLUDE_PREFIX	= ../libmach/
EXPLIB_TARGETS		= export_${LOCAL_LIBMACH}
IDIR			= ${MACH3_LIBRARY_IDIR}
ILIST			= ${LOCAL_LIBMACH}
IMODE			= 644

.include "../libmach/common.mk"

.if exists(depend.mk)
.include "depend.mk"
.endif
