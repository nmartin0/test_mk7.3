#!/bin/sh
# SPDX-FileCopyrightText: 2026 Nicholas Martin
#
# Build the ODE toolset that OSFMK 7.3 is built with.
#
#   ODE4LINUX=~/ode4linux sh build/bootstrap-ode.sh
#
# OSFMK 7.3 is built by ODE, not GNU make, and not by make alone: its
# makefiles are driven by ODE's `build` front end, which reads
# osfmk7.3/osfmk/src/osc/Buildconf and derives the whole environment from
# it. `workon` establishes the sandbox that `build` runs inside. So the
# toolset, not just make, has to exist.
#
# ODE's own setup.sh bootstraps all of this. We run it in a scratch copy
# under ${MK_BUILD} so that neither this repository nor the ode4linux
# clone is written to. Verified by fingerprinting every file in the clone
# before and after a run: sha1 unchanged.
set -e

: "${ODE4LINUX:?set ODE4LINUX to your ode4linux clone}"
: "${MK_BUILD:=${HOME}/.cache/mk7.3}"

[ -f "${ODE4LINUX}/src/ode/setup/setup.sh" ] || {
	echo "no ode/setup/setup.sh under ${ODE4LINUX} -- is ODE4LINUX right?" >&2
	exit 1
}

SB="${MK_BUILD}/ode-sandbox"
rm -rf "${SB}"
mkdir -p "${SB}"
cp -a "${ODE4LINUX}/src" "${SB}/src"

# setup.sh does not select the per-architecture arch_fmtdep.c for the
# at386_linux context, so make's bootstrap falls through to BSDARCH and
# fails. Upstream ode4linux works around this the same way in build.sh.
cp "${SB}/src/ode/bin/make/LINUXARCH/arch_fmtdep.c" \
   "${SB}/src/ode/bin/make/arch_fmtdep.c"

# CENV is an existing hook appended to CFLAGS by ODE's bootstrap.sh.
#
# -fcommon: ode4linux targets GCC 4.8 (2014); GCC 10 (2020) changed the
# default to -fno-common. Without it the link of make fails on
#   ld: main.o:(.bss+0x28): multiple definition of `maxJobs'
# See https://gcc.gnu.org/gcc-10/porting_to.html .
#
# DEF_ARFLAGS: osf.std.mk defaults to `crl`, and in GNU binutils 2.42
# `ar crl <archive> <objs>` fails with "file format not recognized" --
# the l modifier consumes the archive name, so ar tries to open the first
# object as an archive. `ar cr` works. osf.std.mk uses ?= so the
# environment wins. Note OSFMK's own Buildconf already sets DEF_ARFLAGS
# to cr, so this only affects ODE's self-build.
# -std=gnu89: GCC 14 makes implicit function declarations, implicit int,
# int-conversion and incompatible-pointer-types hard errors. All four
# were valid C89 and are pervasive in this 1990s code -- ODE's libode
# calls gets(), genpath.c calls getcwd() and chdir() without including
# unistd.h, and so on. gnu89 is not a suppression: it is the dialect the
# code is actually written in, and Buildconf names gcc 2.7.2.1 as the
# era compiler. Verified against the failing files with gcc 14.2.0:
# getstab.c 1 error -> 0, genpath.c 2 -> 0, makepath.c 2 -> 0.
#
# Known cost: some of those diagnostics are real bugs, not dialect noise
# -- gets() into a fixed buffer is a genuine overflow. That is ODE's
# code, and fixing it would mean modifying a third-party tree, so it is
# recorded rather than patched.
CENV="-fcommon -std=gnu89"

# CFLAGS carries the same two flags, and it is NOT a duplicate of CENV.
#
# CENV reaches only ODE's bootstrap.sh, which builds make. Everything
# after that is built by ODE's own makefiles, and ODE's src/Makeconf
# assigns CENV with a plain `=` for the at386_linux context:
#
#   CENV= -DNO_STATVFS -DINC_VFS -DUSE_BSIZE -DVA_ARGV_IS_RECAST -DNO_POLL
#
# which clobbers whatever we put in the environment. CFLAGS in the same
# block uses `+=`, so an environment value survives and is appended to.
# CFLAGS is therefore the only hook that reaches the tool compiles.
#
# Without this, on GCC 14 every tool that links libode fails and only
# make survives: libode calls gets(), genpath.c calls getcwd() and
# chdir() without <unistd.h>, and GCC 14 makes implicit declarations
# hard errors. Reproduced with gcc 14.2.0 as the system compiler: 52
# errors and five missing tools before, one after.
CFLAGS="-fcommon -std=gnu89"

DEF_ARFLAGS="cr"
context=at386_linux
OS=linux
export CENV CFLAGS DEF_ARFLAGS context OS

echo "building ODE toolset (log: ${MK_BUILD}/bootstrap-ode.log)"
echo "  this takes well under a minute; it is not hung if it is quiet"
echo "  [1/2] make, libode, genpath, makepath, build, workon, release"

# stdin is closed so that nothing in ODE's setup can block waiting for
# input. Several ODE commands prompt by design (see mksb(1)); setup.sh
# does not, but a silent hang would be indistinguishable from slow work
# and is not worth the risk.
( cd "${SB}/src" && sh ode/setup/setup.sh at386_linux ) \
	> "${MK_BUILD}/bootstrap-ode.log" 2>&1 </dev/null || true

# setup.sh builds md but its link fails: md.c and libode both define
# _argbreak as tentative definitions, which -fno-common (GCC 10+) makes
# a duplicate symbol. CENV does not reach ODE's own makefiles, only its
# bootstrap.sh, so md is rebuilt here by hand. It is not optional -- the
# FIRST pass calls md for every directory it exports from.
#
# Excluded from libode: the porting/ replacements for strerror, strdup,
# strcasecmp, getcwd, vfprintf, vsprintf and waitpid. glibc provides all
# of them, and ODE's strerror.c references sys_errlist and sys_nerr,
# which glibc removed.
#
# BUILD_DATE, MACHINE and OS are string macros that ODE's own makefiles
# pass; par_rc_file.c and interface.c do not compile without them.
build_md() {
	W="${MK_BUILD}/mdbuild"
	rm -rf "${W}"; mkdir -p "${W}/inc"
	ln -sfn "${SB}/src/ode/include" "${W}/inc/ode"
	CF="-O -fcommon -std=gnu89 -w -D_BLD -DNO_STATVFS -DINC_VFS"
	CF="${CF} -DUSE_BSIZE -DVA_ARGV_IS_RECAST -DNO_POLL"
	CF="${CF} -DBUILD_DATE=\"unknown\" -DMACHINE=\"i386\" -DOS=\"linux\""
	CF="${CF} -I${W}/inc"
	( cd "${W}" || exit 1
	  for f in "${SB}/src/ode/lib/libode"/*.c; do
		gcc ${CF} -c -o "$(basename "${f%.c}").o" "$f" 2>/dev/null || true
	  done
	  for f in "${SB}/src/ode/lib/libode/porting"/*.c; do
		b=$(basename "${f%.c}")
		case "$b" in
		strerror|strdup|strcasecmp|getcwd|vfprintf|vsprintf|waitpid)
			continue ;;
		esac
		gcc ${CF} -c -o "$b.o" "$f" 2>/dev/null || true
	  done
	  ar cr libode.a ./*.o
	  gcc ${CF} -c -o md.o "${SB}/src/ode/bin/md/md.c"
	  gcc -fcommon -o md md.o -L. -lode
	) >> "${MK_BUILD}/bootstrap-ode.log" 2>&1 </dev/null
	[ -x "${W}/md" ] && cp "${W}/md" "${BIN}/md"
	rm -rf "${W}"
}

BIN="${SB}/tools/at386_linux/bin"
if [ ! -x "${BIN}/md" ]; then
	echo "  [2/2] md"
	build_md
fi

# Only the tools the build actually invokes are required. Counted from
# a full FIRST pass and kernel build: makepath 302 calls, md 10, plus
# build and make themselves. genpath backs make's object-directory
# searches.
#
# release is NOT required. It is ODE's install tool, it is invoked zero
# times by anything we run, and its own Makefile overrides CFLAGS with
# a plain `=` so our dialect flag cannot reach it -- it fails on GCC 14
# with implicit-int. Fighting that to build a tool we never call would
# be work for nothing. workon is likewise built but deliberately unused
# (see AGENTS.md). Both are reported, neither is fatal.
missing=
for t in make build genpath makepath md; do
	[ -x "${BIN}/${t}" ] || missing="${missing} ${t}"
done

if [ -n "${missing}" ]; then
	echo "missing required tools:${missing}" >&2
	echo "see ${MK_BUILD}/bootstrap-ode.log" >&2
	exit 1
fi

for t in workon release; do
	[ -x "${BIN}/${t}" ] || echo "note: ${t} did not build; it is not used"
done

echo
echo "built: ${BIN}"
echo "  $(cd "${BIN}" && echo *)"
echo "log:   ${MK_BUILD}/bootstrap-ode.log"
