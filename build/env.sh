# SPDX-FileCopyrightText: 2026 Nicholas Martin
#
# Build environment for OSF Mach Kernel 7.3, AT386 target, Linux host.
#
# Source this, do not execute it:   . build/env.sh
#
# NOTHING IS WRITTEN INSIDE THE REPOSITORY. All build output, exported
# headers and host tools live under ${MK_BUILD}, which defaults to a
# directory outside the working tree. The repository stays clean enough
# that `git status` is meaningful after a full build.
#
# Every target value here is taken from osfmk7.3/osfmk/src/osc/Buildconf,
# which is OSF's own ODE build configuration and which already carries
# explicit support for an i386 target on a Linux host. Where a value
# differs from Buildconf the reason is stated. Do not invent settings
# here; read Buildconf first.

# --- where our output goes (NOT in the repo) ---------------------------
# Override MK_BUILD to put it elsewhere. It must not be inside the repo.
: "${MK_BUILD:=${HOME}/.cache/mk7.3}"
export MK_BUILD

# --- where the sources are ---------------------------------------------
# ODE4LINUX must point at a clone of github.com/nmartin0/ode4linux.
# It is READ ONLY to us. It supplies two things the OSFMK tree does not
# contain: an ODE make that builds on a modern Linux host, and sys.mk,
# which ODE make refuses to start without.
: "${ODE4LINUX:?set ODE4LINUX to your ode4linux clone}"

if [ -z "${REPO_ROOT}" ]; then
	REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null) || REPO_ROOT=$(pwd)
fi
export REPO_ROOT
export SOURCE_BASE="${REPO_ROOT}/osfmk7.3/osfmk/src"

case "${MK_BUILD}" in
"${REPO_ROOT}"|"${REPO_ROOT}"/*)
	echo "env.sh: MK_BUILD must be outside the repository" >&2
	return 1 2>/dev/null || exit 1
	;;
esac

# --- ODE ---------------------------------------------------------------
# bootstrap-ode.sh builds into MK_BUILD, leaving the ode4linux clone
# untouched.
export MAKE_ODE="${MK_BUILD}/ode/make"
export RULES_MK=osf.rules.mk

# Buildconf sets MAKESYSPATH to ${source_base}/makedefs. That directory
# holds the osf.*.mk rule set but NOT sys.mk, which ships with the ODE
# installation instead. Both directories are therefore on the path. This
# is the one place we knowingly depart from Buildconf, and it is a path
# addition rather than a changed value.
export MAKESYSPATH="${SOURCE_BASE}/makedefs:${ODE4LINUX}/src/ode/mk"

# --- target ------------------------------------------------------------
# Buildconf: on i386 setenv MACHINE i386 / TARGET_MACHINE AT386 /
#            target_machine at386
# MACHINE is the CPU; TARGET_MACHINE is the platform. AT386 means an
# AT-bus PC, which is what QEMU emulates. Sequent's SQT is also i386 but
# a different platform, which is why the two are separate axes.
export MACHINE=i386
export TARGET_MACHINE=AT386
export target_machine=at386
export HOST_MACHINE=i386

# --- host toolchain ----------------------------------------------------
# Buildconf: on i386 on_os linux target i386
#                replace setenv ELF_CC_EXEC_PREFIX ""
# An empty prefix means "use the host's own compiler" -- no cross
# toolchain is needed for an i386 target on an i386/x86-64 Linux host.
export ELF_CC_EXEC_PREFIX=""
export OBJECT_FORMAT=ELF

# Buildconf: on_os linux replace setenv MIGCC gcc
#            on_os linux replace setenv CPP "cc -E"
export MIGCC=gcc
export CPP="cc -E"

# Buildconf: on_os linux replace setenv _ELF_PIC_ ""
export _ELF_PIC_=""

# Buildconf: on i386 on_os linux target i386 setenv CARGS
#                -D__NO_UNDERSCORES__
# a.out decorated C symbols with a leading underscore; ELF does not.
# i386/asm.h carries both conventions and selects on this macro. CARGS
# reaches the compile line via makedefs/osf.std.mk.
export CARGS="-D__NO_UNDERSCORES__"

# --- sandbox layout (all under MK_BUILD) -------------------------------
export OBJECTDIR="${MK_BUILD}/obj/${target_machine}"
export EXPORTBASE="${MK_BUILD}/export/${target_machine}"
export SOURCEBASE="${SOURCE_BASE}"
export INCDIRS="-I${EXPORTBASE}/include -I${EXPORTBASE}/include/sa_mach"
export MACH3_INCDIRS="${INCDIRS}"

# --- in-tree host tools ------------------------------------------------
# Prebuilt ELF 32-bit binaries shipped in the OSFMK tree. They run on a
# Linux host with 32-bit glibc present (libc6-i386). Source for both is
# in-tree; building them from source is a later milestone so the
# toolchain is reproducible on any host, including a future Mach one.
export HOSTBIN="${REPO_ROOT}/osfmk7.3/osfmk/tools/i386/i386_linux/hostbin"
export MIGCOM="${HOSTBIN}/migcom"
export PATH="${MK_BUILD}/ode:${HOSTBIN}:${PATH}"
