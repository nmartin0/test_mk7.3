#!/bin/sh
#
# mkselfhost.sh -- put what the running system needs to build itself
# into a root tree made by docs/tools/mklitesroot.sh (with comp10).
#
#   ODE_RPM=<ode-2.3.4-5f.src.rpm> GNU_DIR=<dir> \
#       sh docs/tools/mkselfhost.sh <root>
#
# Roadmap milestone 5, "Hosting itself".  OSF built natively, with "the
# FSF gcc and the ODE minimal tools set" (MK7.3 Release Notes, 2.5.2);
# this stages the sources of both, and of the system, for building on
# the running system itself with NetBSD 1.0's cc:
#
#   /usr/src/ode/src   ODE 2.3.4, OSF's 1995 sources as MkLinux
#                      packaged them: ode4linux.tar.gz from
#                      ode-2.3.4-5f.src.rpm, with the package's own
#                      patches geoffk, sbinfo and setup, as its spec
#                      applies them.  Not fwb: it comments out make's
#                      strndup() for glibc, and NetBSD 1.0's libc has
#                      none; nor ppcbootstrap, which the spec applies
#                      only on PowerPC.  ODE's setup.sh is run there
#                      with its own context for this host, 386bsd.
#   /usr/src/osfmk     this repository's osfmk/, as git archive gives
#                      it: the sandbox, built there with ODE; and
#   /usr/src/build_world  beside it, where osfmk/src's ../../ finds it.
#   /usr/src/gnu       the GNU sources, as distributed, from GNU_DIR:
#                      gcc-2.7.2.1.tar.gz, binutils-2.7.tar.gz and
#                      make-3.75.tar.gz.  GPL programs: they go on the
#                      disk image only, never into this tree.
#
# The root grows by about 50 MB before anything is built, and the
# builds need some 200 MB more: make the disk with ROOT_MB=400.
#
set -eu

[ $# -eq 1 ] || { echo "usage: $0 <root>" >&2; exit 1; }
ROOT=$1
: "${ODE_RPM:?set ODE_RPM to ode-2.3.4-5f.src.rpm}"
: "${GNU_DIR:?set GNU_DIR to the directory with the GNU tarballs}"
[ -d "$ROOT/usr/src" ] || { echo "no $ROOT/usr/src" >&2; exit 1; }

REPO=$(cd "$(dirname "$0")/../.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# ODE: the package's sources and patches, from the source RPM.
if command -v rpm2cpio >/dev/null && command -v cpio >/dev/null; then
	(cd "$TMP" && rpm2cpio "$ODE_RPM" | cpio -id --quiet)
else
	(cd "$TMP" && 7z x -y "$ODE_RPM" >/dev/null &&
	 7z x -y ./*.cpio >/dev/null)
fi
mkdir "$TMP/x"
tar -xzf "$TMP/ode4linux.tar.gz" -C "$TMP/x"
for p in geoffk sbinfo setup; do
	(cd "$TMP/x" && patch -s -p0 < "$TMP/ode-$p.patch")
done
find "$TMP/x" -name '*.orig' -exec rm -f {} +
rm -rf "$ROOT/usr/src/ode"
mkdir -p "$ROOT/usr/src/ode"
cp -a "$TMP/x/ode4linux/src" "$ROOT/usr/src/ode/src"

# The system's sources.
rm -rf "$ROOT/usr/src/osfmk"
git -C "$REPO" archive HEAD osfmk build_world | tar -x -C "$ROOT/usr/src"

# The GNU sources.
mkdir -p "$ROOT/usr/src/gnu"
for t in gcc-2.7.2.1 binutils-2.7 make-3.75; do
	cp "$GNU_DIR/$t.tar.gz" "$ROOT/usr/src/gnu/$t.tar.gz"
	chmod 644 "$ROOT/usr/src/gnu/$t.tar.gz"
done

du -sh "$ROOT/usr/src/ode" "$ROOT/usr/src/osfmk" "$ROOT/usr/src/gnu"
