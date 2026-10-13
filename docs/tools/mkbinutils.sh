#!/bin/sh
#
# mkbinutils.sh -- build the target's assembler: GNU as for i386-elf.
#
#   sh docs/tools/mkbinutils.sh
#
# Run once, after docs/tools/mksandbox.sh and before build_world.  It
# installs one program, as, into the sandbox's ${tools_base}/elf/,
# where the compiler finds it (mksandbox.sh, ANSI_CC and
# TRADITIONAL_CC, "-B").
#
# Why.  OSF's i386 assembler sources -- the kernel's in i386/, and
# libsa_mach's -- are written for the System V assembler, where "/"
# begins a comment:
#
#	movl	%ss,%ax			/ switch to kernel data seg
#
# GNU as keeps that convention for its i386-elf target and drops it for
# i386-linux, where the same "/" is a division and the line an error
# (gas/config/tc-i386.c, SVR4_COMMENT_CHARS: every ELF i386 target but
# GNU, Linux, Haiku and the BSDs).  slp/osfmk-mklinux rewrote some 1,200
# comments to assemble with Linux's as and renamed every file .S
# (18c9d31, 81429d0, 364472d, 090a2dc); built for i386-elf, as reads
# OSF's files as written.  Buildconf already names the place for the
# target's tools, ELF_CC_EXEC_PREFIX, ${tools_base}/elf/, and
# osf.gcc.mk hands it to the compiler as -B.
#
# Which.  GNU binutils 2.42, the release the host's own as is on the
# Ubuntu 24.04 this was built on, so that the only difference from the
# host's assembler is its target.  The configuration OSF's own
# compiler used, gcc 2.7.2 with the binutils of its day for OSF/1 i386
# ELF, is milestone 5's (docs/roadmap.md).
#
# Where.  Everything under MK_BUILD/binutils (MK_BUILD defaults to
# ~/.cache/osfmk, as for mksandbox.sh): the tarball, src/, obj/,
# build.log.  Nothing is written to this repository.  Give
# BINUTILS_TARBALL to use a copy already fetched; it is checked against
# the SHA-256 below either way.
#
set -eu

: "${MK_BUILD:=${HOME}/.cache/osfmk}"
BU="${MK_BUILD}/binutils"
V=2.42
URL=https://ftp.gnu.org/gnu/binutils/binutils-${V}.tar.xz
SHA256=f6e4d41fd5fc778b06b7891457b3620da5ecea1006c6a4a41ae998109f85a800
ELF="${MK_BUILD}/tools/i386/i386_linux/elf"

for t in cc tar xz sha256sum; do
	command -v $t >/dev/null || { echo "$t is required" >&2; exit 1; }
done

# GNU make, which binutils needs, by name: in the shell build_world
# runs in, "make" is ODE's.
GMAKE=
for m in gmake /usr/bin/make make; do
	if "$m" --version 2>/dev/null | grep -q "GNU Make"; then
		GMAKE=$m; break
	fi
done
[ -n "${GMAKE}" ] || { echo "GNU make is required" >&2; exit 1; }
[ -d "${MK_BUILD}/tools/i386/i386_linux" ] || {
	echo "no ${MK_BUILD}/tools -- run docs/tools/mksandbox.sh first" >&2
	exit 1; }

mkdir -p "${BU}"
T="${BINUTILS_TARBALL:-${BU}/binutils-${V}.tar.xz}"
if [ ! -f "${T}" ]; then
	command -v curl >/dev/null || { echo "curl is required" >&2; exit 1; }
	curl -fSL -o "${T}" "${URL}"
fi
echo "${SHA256}  ${T}" | sha256sum -c --quiet - || {
	echo "${T} is not binutils ${V}" >&2; exit 1; }

rm -rf "${BU}/src" "${BU}/obj"
mkdir -p "${BU}/src" "${BU}/obj"
tar -C "${BU}/src" -xJf "${T}"

# set -e does not reach inside a function whose status is tested, so
# each step says "|| return".  MAKEINFO=true: the manuals are not
# wanted, and makeinfo need not be installed.
build() {
	cd "${BU}/obj" &&
	"${BU}/src/binutils-${V}/configure" --target=i386-elf \
	    --disable-nls --disable-werror --disable-gdb --disable-gdbserver \
	    --disable-sim --disable-gold --disable-gprofng || return
	"${GMAKE}" MAKE="${GMAKE}" MAKEINFO=true all-gas || return
}

LOG="${BU}/build.log"
echo "mkbinutils: building in ${BU}; log in ${LOG}"
(build) > "${LOG}" 2>&1 || {
	tail -20 "${LOG}" >&2
	echo "mkbinutils: failed; see ${LOG}" >&2
	exit 1
}

mkdir -p "${ELF}"
cp "${BU}/obj/gas/as-new" "${ELF}/as"
echo "mkbinutils: ${ELF}/as"
"${ELF}/as" --version | head -1
