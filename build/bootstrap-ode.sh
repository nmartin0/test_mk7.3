#!/bin/sh
# SPDX-FileCopyrightText: 2026 Nicholas Martin
#
# Build an ODE make that runs on a modern Linux host.
#
#   ODE4LINUX=~/ode4linux sh build/bootstrap-ode.sh
#
# OSFMK 7.3 is built by ODE make, not GNU make. Its own rule set
# (osfmk7.3/osfmk/src/makedefs) is complete, so the only things missing
# from the OSFMK tree are the make binary itself and sys.mk. Both come
# from ode4linux (Andrei Warkentin, 2014), which is OSF's ODE with a
# LINUXARCH port added.
#
# NOTHING IS WRITTEN INSIDE THE REPOSITORY OR INSIDE THE ODE4LINUX
# CLONE. The make sources are copied into ${MK_BUILD} and built there,
# so both trees stay pristine and `git status` stays meaningful.
set -e

: "${ODE4LINUX:?set ODE4LINUX to your ode4linux clone}"
: "${MK_BUILD:=${HOME}/.cache/mk7.3}"

SRC="${ODE4LINUX}/src/ode/bin/make"
[ -f "${SRC}/bootstrap.sh" ] || {
	echo "no bootstrap.sh under ${SRC} -- is ODE4LINUX right?" >&2
	exit 1
}

WORK="${MK_BUILD}/ode"
SB="${MK_BUILD}/ode-src"

rm -rf "${SB}"
mkdir -p "${SB}/src/ode" "${WORK}"

# Copy only what the bootstrap reads: the make sources, ODE's headers,
# and libode (bootstrap.sh takes cond.c from there).
cp -a "${ODE4LINUX}/src/ode/bin" "${SB}/src/ode/bin"
cp -a "${ODE4LINUX}/src/ode/include" "${SB}/src/ode/include"
cp -a "${ODE4LINUX}/src/ode/lib" "${SB}/src/ode/lib"

MAKEDIR="${SB}/src/ode/bin/make"

# bootstrap.sh does not select the per-architecture arch_fmtdep.c for the
# at386_linux context, so it falls through to BSDARCH and fails. Upstream
# ode4linux works around this the same way in its own build.sh.
cp "${MAKEDIR}/LINUXARCH/arch_fmtdep.c" "${MAKEDIR}/arch_fmtdep.c"

# bootstrap.sh computes EXPORTPATH as
# ${MAKETOP}/../export/${context}/usr/include and expects ODE's own
# headers reachable there as <ode/...>.
EXPORT_INC="${SB}/export/at386_linux/usr/include"
mkdir -p "${EXPORT_INC}"
ln -sfn "${SB}/src/ode/include" "${EXPORT_INC}/ode"

# CENV is an existing hook in bootstrap.sh, appended to CFLAGS.
#
# -fcommon is required because ode4linux targets GCC 4.8 (2014) and GCC
# 10 (2020) changed the default to -fno-common. Without it the link fails
# on
#   ld: main.o:(.bss+0x28): multiple definition of `maxJobs'
# See https://gcc.gnu.org/gcc-10/porting_to.html . The flag keeps the
# tentative-definition behaviour the code was written against rather than
# editing the sources.
CENV="-fcommon"
context=at386_linux
OS=linux
MAKETOP="${SB}/src/"
MAKESUB="ode/bin/make/"
export CENV context OS MAKETOP MAKESUB

( cd "${MAKEDIR}" && sh bootstrap.sh >"${MK_BUILD}/bootstrap-ode.log" 2>&1 ) || {
	echo "bootstrap failed; last 20 lines of ${MK_BUILD}/bootstrap-ode.log:" >&2
	tail -20 "${MK_BUILD}/bootstrap-ode.log" >&2
	exit 1
}

[ -x "${MAKEDIR}/make" ] || {
	echo "bootstrap produced no make binary; see ${MK_BUILD}/bootstrap-ode.log" >&2
	exit 1
}

cp "${MAKEDIR}/make" "${WORK}/make"
rm -rf "${SB}"

echo "built: ${WORK}/make"
echo "log:   ${MK_BUILD}/bootstrap-ode.log"
