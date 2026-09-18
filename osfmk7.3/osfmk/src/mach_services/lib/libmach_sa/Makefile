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
# -DSTANDALONE is what selects the standalone behaviour. Nothing in the
# libmach sources tests it; common.mk is the only file in the tree that
# does, and only to choose MACH_INIT_OFILE.
#
# AI-ONLY NOTE: MACH_INIT_OFILE is deliberately NOT overridden here, so
# common.mk's default of mach_init.o applies.
#
# An earlier revision of this file set it to mach_init_sa.o, on the
# stated grounds that mach_init_sa.c makes mach_init global rather than
# static and that this is the symbol LITES links against. That is wrong
# in both halves, and it makes the library unlinkable:
#
#   - Nothing in LITES references mach_init. Measured across every
#     object of the server, the emulator and liblites: zero undefined
#     references to it. Entry is through _mach_init_routine, a common
#     symbol in crt0.o that mach_init.c and mach_init_sa.c both define,
#     so either file satisfies the reference.
#
#   - mach_init.c is the only definition of _rpc_glue_vector. This
#     library's own ms_thread_switch.o and ms_thread_depress_abort.o
#     members reference it, so building with mach_init_sa.o produces an
#     archive that cannot satisfy itself:
#       ld: ...libmach_sa.a(MS_TH_switch.o): undefined reference to
#           `_rpc_glue_vector'
#
# Note what this does NOT do: it does not merely quiet a link error.
# Supplying a definition alone would be worse than the error, because
# CAN_SHCIRCUIT() in mach/i386/rpc.h dereferences the pointer
# unconditionally (_rpc_glue_vector->name), and cthreads calls
# thread_switch, so a null vector is a fault on the first cthread yield.
# mach_init.c initialises the vector from TASK_USER_DATA; mach_init_sa.c
# never does, and also skips mach_init_ports(). Taking mach_init.o is
# what makes the short-circuit path safe, not just resolvable.
#
USE_STATIC_LIBRARIES	=
LOCAL_LIBMACH		= libmach_sa.a
LOCAL_CFLAGS		= -DSTANDALONE
LOCAL_MIGFLAGS		= -DSTANDALONE

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
