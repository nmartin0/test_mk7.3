#!/bin/sh
#
# mknbmakefs.sh -- build NetBSD's makefs(8) as a host tool, nbmakefs.
#
#   sh docs/tools/mknbmakefs.sh
#
# docs/tools/mkbootdisk.sh writes the boot disk's file system with it.
#
# Why makefs.  OSF made its disks on a running system: disk_setup ran
# the vtoc utility and newfs, and the files were then copied onto the
# mounted file system (MK7.3 Release Notes section 2.4.2.2;
# stand/AT386/boot/secondary/README, "newfs fd0 ... mount ... cp").
# Those were OSF/1 1.3 commands, not in the source distribution.  No
# BSD tool from before 4.4BSD fills a file system without mounting it:
# the one there was, mkproto, "is believed to contain source code
# proprietary to AT&T" and is absent from Net/2, 386BSD and 4.4BSD-Lite.
# makefs fills an image from a directory, and since NetBSD 11.0 writes
# the 4.3BSD format OSF's reader expects: "makefs -t ffs -o version=0"
# ("allow version=0 4.3BSD FFS file systems to be created", the 11.0
# release announcement).  docs/provenance/precedent.md, "How the disk
# is made", has the rest.  Debian's and Ubuntu's makefs package is a
# 2019 snapshot without version=0, so it is built here.
#
# How.  The way NetBSD builds its host tools on a foreign system:
# build.sh makes nbmake and the nbmake-i386 wrapper, which BUILDING says
# is there "to assist in building subtrees on a cross-compile host", and
# the wrapper builds tools/host-mkdep, compat and binstall, then pax and
# makefs, in the order tools/Makefile gives them.  No compiler or other
# tool of NetBSD's is built.
#
# What.  NetBSD 11.0 exactly -- the commit "Welcome to 11.0!" on the
# netbsd-11 branch of NetBSD's git mirror -- fetched as a sparse,
# shallow checkout of the directories those five tools read, about
# 235 MB instead of 4.4 GB.
#
# Where.  Everything under MK_BUILD/netbsd (MK_BUILD defaults to
# ~/.cache/osfmk, as for docs/tools/mksandbox.sh): src/, obj/, build.log,
# and tools/bin/nbmakefs, where mkbootdisk.sh looks for it.  Nothing is
# written to this repository.  Run again, it fetches nothing and
# rebuilds.
#
set -eu

: "${MK_BUILD:=${HOME}/.cache/osfmk}"
NB="${MK_BUILD}/netbsd"
NB_REPO=https://github.com/NetBSD/src
NB_COMMIT=7d25f256a2826e0a1945771090aceb98f7d2cf72	# "Welcome to 11.0!"

for t in git cc; do
	command -v $t >/dev/null || { echo "$t is required" >&2; exit 1; }
done

mkdir -p "${NB}/src"
cd "${NB}/src"
[ -d .git ] || { git init -q; git remote add origin "${NB_REPO}"; }

# The top-level files (build.sh) and these directories only.  makefs
# compiles every file system it writes, so sys/fs and the newfs_* and
# fsck directories it borrows from are here too; sys/arch only for
# the headers tools/Makefile.nbincludes copies.
git sparse-checkout set --no-cone '/*' '!/*/' \
	/tools/ /share/mk/ /usr.bin/make/ /usr.bin/xinstall/ /bin/pax/ \
	/usr.sbin/makefs/ /usr.sbin/mtree/ /sbin/mknod/ /sbin/fsck/ \
	/sbin/newfs_msdos/ /sbin/newfs_udf/ /sbin/newfs_v7fs/ \
	/sys/sys/ /sys/ufs/ /sys/fs/ /sys/dev/ '/sys/arch/*/include/' \
	/lib/libc/ /lib/libutil/ /common/ /include/ /external/bsd/flex/

if [ "$(git rev-parse -q --verify HEAD || true)" != "${NB_COMMIT}" ]; then
	git fetch -q --depth 1 --filter=blob:none origin "${NB_COMMIT}"
	git checkout -q --detach "${NB_COMMIT}"
fi

# set -e does not reach inside a function whose status is tested, so
# each step says "|| return".
M="${NB}/tools/bin/nbmake-i386"
build() {
	./build.sh -U -u -m i386 -T "${NB}/tools" -O "${NB}/obj" \
	    makewrapper || return
	for d in host-mkdep compat binstall; do
		(cd "tools/$d" && $M obj && $M dependall) || return
	done
	for d in binstall host-mkdep compat; do
		(cd "tools/$d" && $M install) || return
	done
	for d in pax makefs; do
		(cd "tools/$d" && $M obj && $M dependall && $M install) ||
		    return
	done
}

LOG="${NB}/build.log"
echo "mknbmakefs: building in ${NB}; log in ${LOG}"
build > "${LOG}" 2>&1 || {
	tail -20 "${LOG}" >&2
	echo "mknbmakefs: failed; see ${LOG}" >&2
	exit 1
}

echo "mknbmakefs: ${NB}/tools/bin/nbmakefs"
