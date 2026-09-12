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

BIN="${SB}/tools/at386_linux/bin"
missing=
for t in make build workon genpath makepath release; do
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
echo
echo "md (make depend) is NOT built: it hits the same -fno-common issue"
echo "inside ODE's own makefiles, where CENV does not reach. It is only"
echo "needed for incremental dependency generation, so it is deferred."
