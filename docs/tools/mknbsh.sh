#!/bin/sh
#
# mknbsh.sh -- put NetBSD 1.0's /bin/sh source, with NetBSD's own
# fix for S-22, into a root tree made by docs/tools/mklitesroot.sh
# (with comp10), to be built on the running system.
#
#   sh docs/tools/mknbsh.sh <root> <netbsd-1.0-src>
#
# <netbsd-1.0-src> is NetBSD 1.0's source tree, the top of src/: the
# release's source sets unpacked, or the netbsd-1-0 branch of
# NetBSD's repository (github.com/NetBSD/src), whose bin/sh is the
# release's -- every $Id in NetBSD 1.0's /bin/sh binary is in it.
# Three parts of it are copied, as bin/sh's Makefile finds them:
#
#   /usr/src/netbsd/bin/Makefile.inc   BINDIR /bin, LDSTATIC -static
#   /usr/src/netbsd/bin/sh             the shell
#   /usr/src/netbsd/usr.bin/printf     on bin/sh's .PATH
#
# and docs/tools/netbsd10-sh.diff is applied to bin/sh/jobs.c:
# NetBSD's jobs.c 1.19 (16 October 1996), as NetBSD 1.2.1 took it,
# which relocates the job table's ps pointers when the table grows.
# docs/deferred.md, S-22, has the defect.
#
# Then, on the running system, as root:
#
#   cd /usr/src/netbsd/bin/sh && make NOMAN=1 && make NOMAN=1 install
#
# with NetBSD 1.0's make, cc, yacc and lex, which install the shell
# as /bin/sh, statically linked, as NetBSD built it.  NOMAN=1, as for
# Lite2's ftpd (docs/status.md, "Online"): the sets carry no nroff.
#
set -eu

[ $# -eq 2 ] || { echo "usage: $0 <root> <netbsd-1.0-src>" >&2; exit 1; }
ROOT=$1; NB=$2
[ -d "$ROOT/usr/src" ] || { echo "no $ROOT/usr/src" >&2; exit 1; }
for d in bin/sh usr.bin/printf; do
	[ -d "$NB/$d" ] || { echo "no $NB/$d" >&2; exit 1; }
done
grep -q 'jobs.c,v 1.11 ' "$NB/bin/sh/jobs.c" ||
	{ echo "$NB/bin/sh/jobs.c is not NetBSD 1.0's (1.11)" >&2; exit 1; }

TOOLS=$(cd "$(dirname "$0")" && pwd)
DST="$ROOT/usr/src/netbsd"
[ ! -e "$DST/bin/sh" ] || { echo "$DST/bin/sh exists" >&2; exit 1; }
mkdir -p "$DST/bin" "$DST/usr.bin"
cp -p "$NB/bin/Makefile.inc" "$DST/bin/"
cp -Rp "$NB/bin/sh" "$DST/bin/"
cp -Rp "$NB/usr.bin/printf" "$DST/usr.bin/"
(cd "$DST" && patch -p1 -s) < "$TOOLS/netbsd10-sh.diff"
echo "mknbsh: $DST/bin/sh"
