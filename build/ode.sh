#!/bin/sh
# SPDX-FileCopyrightText: 2026 Nicholas Martin
#
# Run ODE's build(1) against the OSFMK sandbox.
#
#   sh build/ode.sh MAKEFILE_PASS=FIRST
#   sh build/ode.sh -here mach_kernel MACH_KERNEL_CONFIG=PRODUCTION
#
# Everything after the script name is passed to build(1) untouched.
#
# This is a thin wrapper, not a build system. build(1) reads
# osfmk7.3/osfmk/src/osc/Buildconf and derives the whole environment
# itself; all this does is put the tools on PATH, change to the sandbox
# source directory, and name the sandbox rc file.
#
# WHY THERE IS NO workon(1) HERE
#
# OSFMK_BUILD.README says to run `workon -sb osfmk` first, but workon(1)
# itself says:
#
#   "workon is part of the source control mechanism for the OSF
#    Development Environment (ODE) and is normally not be used if ODE
#    source control is not used."
#
# We use git, not ODE source control. build(1) takes -sb and -rc in its
# own right (see build(1) SYNOPSIS), and running it directly gives an
# identical result -- verified: MAKEFILE_PASS=FIRST returns 0 with the
# same 250 exported headers either way. Dropping workon also drops its
# requirement for SHELL. USER is still required -- build(1) itself
# checks for it and aborts with "USER not found in environment".
#
# WHY ODE IS NOT PREPENDED TO THE CALLER'S PATH
#
# osfmk7.3/set_ode_path.sh documents the rule: ODE's tools belong early
# in PATH only inside a workon shell, and after the system tools
# otherwise, so that a plain `make` does not silently become ODE make.
# Since we do not use workon, PATH is set for this script's own
# invocation only and the caller's shell is left alone.
set -e

: "${ODE4LINUX:?set ODE4LINUX to your ode4linux clone}"
: "${MK_BUILD:=${HOME}/.cache/mk7.3}"

# build(1) aborts if USER is unset. OSFMK_BUILD.README says to export it
# and suggests $LOGNAME; do the same rather than make the caller do it.
if [ -z "${USER}" ]; then
	USER=${LOGNAME:-$(id -un 2>/dev/null)}
	USER=${USER:-builder}
	export USER
fi

REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null) || REPO_ROOT=$(pwd)
BIN="${MK_BUILD}/ode-sandbox/tools/at386_linux/bin"
SRC="${REPO_ROOT}/osfmk7.3/osfmk/src"
RC="${MK_BUILD}/sandboxrc"

[ -x "${BIN}/build" ] || {
	echo "no build(1) at ${BIN} -- run build/bootstrap-ode.sh first" >&2
	exit 1
}
[ -f "${RC}" ] || {
	echo "no sandbox rc at ${RC} -- run build/mksandbox.sh first" >&2
	exit 1
}

# build(1) resolves the target path relative to the current directory,
# so it must be run from inside the sandbox source tree.
cd "${SRC}"

PATH="${BIN}:${PATH}"
export PATH

exec "${BIN}/build" -sb osfmk -rc "${RC}" "$@"
