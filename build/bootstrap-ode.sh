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
CENV="-fcommon"
DEF_ARFLAGS="cr"
context=at386_linux
OS=linux
export CENV DEF_ARFLAGS context OS

( cd "${SB}/src" && sh ode/setup/setup.sh at386_linux ) \
	> "${MK_BUILD}/bootstrap-ode.log" 2>&1 || true

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
	) >> "${MK_BUILD}/bootstrap-ode.log" 2>&1
	[ -x "${W}/md" ] && cp "${W}/md" "${BIN}/md"
	rm -rf "${W}"
}

BIN="${SB}/tools/at386_linux/bin"
[ -x "${BIN}/md" ] || build_md
missing=
for t in make build workon genpath makepath release md; do
	[ -x "${BIN}/${t}" ] || missing="${missing} ${t}"
done

if [ -n "${missing}" ]; then
	echo "missing tools:${missing}" >&2
	echo "see ${MK_BUILD}/bootstrap-ode.log" >&2
	exit 1
fi

echo "built: ${BIN}"
echo "  $(cd "${BIN}" && echo *)"
echo "log:   ${MK_BUILD}/bootstrap-ode.log"
