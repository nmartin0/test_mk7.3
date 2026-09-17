#
# mach_init -- LITES's first user program.
#
# AI-ONLY NOTE: this directory did not exist. It is added, not modified.
#
# Follows the pattern of mach_services/cmds/mach_perf/monitor/Makefile,
# which is the only other command in this tree and therefore the local
# precedent for PROGRAMS, ILIST and IDIR.
#
# Installed into MACH3_ROOT_SERVERS_IDIR, the same place default_pager
# and the bootstrap task go, because that is what the bootstrap task
# reads and what LITES's server_dir points at.
#
# LIBRARIES: -lmach_sa rather than -lmach, because this is a standalone
# Mach program with no libc. See mach_services/lib/libmach_sa, which
# this project added because several things link -lmach_sa and nothing
# in the tree built it. -lsa_mach follows it for bcopy, bzero and the
# other standalone C functions, which on AT386 live there rather than in
# libmach; libmach has them only as HP700 assembly. libmach_sa is
# repeated because the two archives depend on each other and a static
# archive is scanned once.
#
# -lcthreads before -lmach_sa is deliberate. mach_perf/monitor/Makefile
# carries a note from 1993 that putting it the other way round yields
# the wrong versions of calls such as mig_get_reply_port().
#
PROGRAMS		= mach_init
ILIST			= mach_init
IDIR			= ${MACH3_ROOT_SERVERS_IDIR}
IMODE			= 755

USE_STATIC_LIBRARIES	=
DEPENDENCIES		= YES

mach_init_OFILES	= main.o

LIBS			= -lcthreads -lmach_sa -lsa_mach -lmach_sa

.include <${RULES_MK}>
