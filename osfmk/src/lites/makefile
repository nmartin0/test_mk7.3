#
# AI-ONLY NOTE: this file is not Lites'.  It takes the place of Utah's
# makefile, which Lites 1.1.u3 ships at the top of its tree -- "Utah
# makefile, more BSD like", a driver that runs conf/configure and gmake
# in an object directory, with Utah's paths (/usr/src/lites,
# /usr/obj/lites, --target=parisc, --with-release=/usr/mach4).  This
# one does the same job for this site: "build -here lites" runs it
# inside the OSFMK sandbox, as OSF's step 8 built src/osf1_server.
# ODE's make reads "makefile" before "Makefile" (ode/bin/make/main.c),
# so CMU's ODE Makefiles of 1992-95, at the top and in include/,
# liblites/, emulator/ and server/, stay as Lites shipped them and are
# not run.  Lites' Makeconf, which would make this directory an ODE
# collection of its own, is removed.  docs/provenance/precedent.md,
# "How Lites meets ODE", has both.
#
# It does not describe Lites' build.  Lites' own build does that --
# configure, conf/Makerules and the per-directory Makefile.in and
# Makerules files -- and this file runs it in the object directory,
# against the sandbox's compiler and export area, in a subdirectory
# named for the configuration as the kernel's is:
#
#	obj/<machine>/lites/${LITES_CONFIG}/
#
#	configure --target=${LITES_TARGET} --with-config=${LITES_CONFIG}
#	          --with-release=${EXPORTBASE}
#	gmake
#
# That is how Lites' authors built it against an OSF microkernel.
# conf/Makerules has an "OSF STUFF" block, under OSF_ENVIRONMENT, that
# points CC, LD, AR, RANLIB and SIZE into an OSF sandbox's tools,
# /afs/gr.osf.org/.../sbox/nmk18b9/link/tools/at386/elf/, and the
# osfmach3 option that selects the OSFMK code paths exists only in
# conf/MASTER, the configure build's.  Lites' older ODE configuration,
# server/conf, predates both and was not revived; see
# docs/provenance/precedent.md, "How Lites meets ODE".
#
# Settings, each overridable on the build line as MACH_KERNEL_CONFIG
# is for the kernel:
#
#   LITES_CONFIG  STD+WS+osfmach3.  STD+WS is configure's default
#                 (conf/configure.in, "LITES_CONFIG=STD+WS"); osfmach3
#                 is the option for this kernel (conf/MASTER line 85).
#                 Lites' CMU Makeconf, removed, defaulted to
#                 STD+WS+DEBUG; add +DEBUG on the build line for that.
#   LITES_TARGET  i386-mach, the name Lites' documentation gives its
#                 cross tools (doc/README.lites-1.1.u1, "the i386-mach
#                 cross-build tools").  configure derives
#                 TARGET_MACHINE=i386 from it and expects the tools as
#                 ${LITES_TARGET}-gcc, -ld and so on.
#   GMAKE         Lites' build is GNU make (doc/building, "gmake").
#                 Plain "make" inside an ODE build is ODE's make.
#   LITES_AWK     original-awk, Kernighan's, the awk of 4.4BSD that
#                 server/kern/vnode_if.sh was written for.  configure
#                 takes AWK from the environment ("Let the user override
#                 the test") and would otherwise choose mawk, which
#                 refuses the script whole: its awk programs call a
#                 bail() they never define, an error BWK awk reports
#                 only if the call is reached, and it never is.  With
#                 mawk, vnode_if.c holds no vop_*_desc and the server
#                 does not link.
#
# THE CROSS TOOLS.  configure looks for the target's tools by name and
# uses them itself -- its C preprocessor runs the server configuration
# (conf/MASTER through doconfig.sh) before any Makefile exists -- so
# they cannot be supplied later on the gmake line.  Lites' answer is to
# install them under those names, which OSF_ENVIRONMENT did by pointing
# at the sandbox's own.  This file does the same: it writes
# tools/${LITES_TARGET}-* into the object directory, each a one-line
# exec of what the sandbox already uses, and puts tools/ first on PATH
# for configure and gmake.  Nothing is installed anywhere else.
#
#   gcc     ${_CC_} ${CARGS}, the compiler and flags every other
#           component is built with.  -fno-builtin matters most: Lites
#           has its own two-argument free() and three-argument malloc(),
#           which a compiler with built-ins rejects.
#   ld      ${_LD_} ${LDOPTS}, the kernel link's linker and options.
#   ar, ranlib   ${_AR_}, ${_RANLIB_}.
#   strip, size, nm   the host's; they read 32-bit ELF.
#
# MIG is the one tool given on the gmake line instead.  Lites' Makerules
# (conf/Makerules lines 100-105) builds it as
#
#	MIG := mig -cc "${CPP}"
#
# for the CMU and Utah MIG driver, whose -cc names the preprocessor.
# The sandbox's driver, ${_MIG_}, is OSF's: it names the preprocessor
# with -cpp, runs it with -E itself, and passes any option it does not
# know to the preprocessor -- so -cc reaches gcc and fails.  GNU make
# lets a command-line variable override a makefile's :=, so
#
#	MIG="${_MIG_} -cpp ${LITES_TARGET}-gcc"
#
# replaces that one line's result with the driver every other
# component uses.  MIGCOM goes on the line with it.  osc/Buildconf
# exports it and OSF's driver reads it from the environment, but
# Makerules line 94 reassigns it -- empty, finding no migcom in the
# release -- and GNU make exports a variable it inherited with the
# makefile's value, so the driver would run an empty migcom.  Lites'
# own MIG rules and flags are unchanged.
#
# OSF_ENVIRONMENT is set on the gmake line.  It is Lites' own switch for
# a build inside an OSF sandbox, written by its authors against OSF
# Grenoble's nmk18b9: it names the sandbox's libraries for the server
# and the emulator, and in an OSF sandbox the server takes its start
# code from libsa_mach, entered at __start_mach, as every OSF server
# does (docs/provenance/precedent.md, "How Lites meets ODE").  Its
# block in conf/Makerules also points CC, LD, AR, RANLIB and SIZE at
# Grenoble's tools, /afs/gr.osf.org/.../link/tools/at386/elf/; this
# sandbox's are given on the gmake line in their place, as MIG is --
# the same ${LITES_TARGET}-* commands configure already uses.
#
# One include directory is added, after all of Lites' own:
# ${EXPORTBASE}/include/sa_mach.  osc/Buildconf line 134 gives every
# Mach task in this sandbox
#
#	INCDIRS "-I${export_base}/include -I${export_base}/include/sa_mach"
#
# and --with-release supplies the first.  The second is where a
# standalone Mach task finds <stdarg.h>, which the exported mach_init.h
# includes.  It goes in with -idirafter so nothing in it can hide a
# header Lites provides for itself.
#
# --prefix is the configuration's own release/ subdirectory.  Lites puts
# ${prefix}/include and ${prefix}/include/mach on every compile line
# (conf/Makerules line 66, INSTALL_INCDIR); configure's default prefix,
# /usr/local, would put the host's headers there.  Nothing is installed
# by this file; where the server goes, /mach_servers/startup, is a step
# of its own.
#
# The subdirectory keeps each configuration's configure output apart,
# as the kernel's configurations are kept, and keeps the GNU Makefile
# configure writes out of the directory ODE's make works in.  When this
# file was named Makefile, ODE's make read configure's in its place
# there, and the second build failed on "include
# $(OBJDIR)/conf/Makeconf".
#
# Three variables are removed from gmake's environment.  MAKEFLAGS holds
# ODE make's flags, which GNU make would misread.  INCDIRS and LIBDIRS
# are set by osc/Buildconf as ready-made "-I..." and "-L..." strings,
# and Lites' Makerules uses the same two names for bare directory lists
# that it extends with += and prefixes itself; inherited, they become
# "-I-I/...".  Lites gets the export area from --with-release instead.
#

LITES_CONFIG	?= STD+WS+osfmach3
LITES_TARGET	?= i386-mach
LITES_AWK	?= original-awk
GMAKE		?= gmake

_LITES_TOOLS_	= gcc ld ar ranlib strip size nm
_LITES_CROSS_	= ${_LITES_TOOLS_:S;^;tools/${LITES_TARGET}-;g}

OTHERS		= ${_LITES_CROSS_} ${LITES_CONFIG}/config.status lites

.include <${RULES_MK}>

_LITES_PATH_	= PATH=$$_t_:$$PATH
_LITES_CD_	= _t_=`pwd`/tools && cd
_LITES_ENV_	= env -u MAKEFLAGS -u INCDIRS -u LIBDIRS

tools/${LITES_TARGET}-gcc_EXEC	= ${_CC_} ${CARGS} \
				  -idirafter ${EXPORTBASE}/include/sa_mach
tools/${LITES_TARGET}-ld_EXEC	= ${_LD_} ${LDOPTS}
tools/${LITES_TARGET}-ar_EXEC	= ${_AR_}
tools/${LITES_TARGET}-ranlib_EXEC = ${_RANLIB_}
tools/${LITES_TARGET}-strip_EXEC = strip
tools/${LITES_TARGET}-size_EXEC	= size
tools/${LITES_TARGET}-nm_EXEC	= nm

${_LITES_CROSS_}: ${ALWAYS}
	${MAKEPATH} ${.TARGET}
	echo '#!/bin/sh' > ${.TARGET}.X
	echo 'exec ${${.TARGET}_EXEC} "$$@"' >> ${.TARGET}.X
	chmod +x ${.TARGET}.X
	${MV} ${.TARGET}.X ${.TARGET}

${LITES_CONFIG}/config.status: ${_LITES_CROSS_} configure Makefile.in
	${MAKEPATH} ${.TARGET}
	${_LITES_CD_} ${.TARGET:H} && ${_LITES_PATH_} AWK=${LITES_AWK} \
		sh ${configure:P} \
		--target=${LITES_TARGET} \
		--with-config=${LITES_CONFIG} \
		--with-release=${EXPORTBASE} \
		--prefix=`pwd`/release

lites: ${LITES_CONFIG}/config.status ${ALWAYS}
	${_LITES_CD_} ${LITES_CONFIG} && ${_LITES_PATH_} ${_LITES_ENV_} ${GMAKE} \
		MIG="${_MIG_} -cpp ${LITES_TARGET}-gcc" MIGCOM="${MIGCOM}" \
		OSF_ENVIRONMENT=1 \
		CC="${LITES_TARGET}-gcc -pipe" LD=${LITES_TARGET}-ld \
		AR=${LITES_TARGET}-ar RANLIB=${LITES_TARGET}-ranlib \
		SIZE=${LITES_TARGET}-size
