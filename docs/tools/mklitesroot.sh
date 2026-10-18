#!/bin/sh
#
# mklitesroot.sh -- make the root tree Lites boots: a NetBSD 1.0
# installation, set up the way Lites' own install notes say.
#
#   sh docs/tools/mklitesroot.sh <dir> <sets-dir> [set...]
#
# <sets-dir> holds NetBSD 1.0's i386 binary sets as they were
# distributed, split into 240640-byte parts: base10.aa ... base10.bb,
# etc10.aa, comp10.aa ... comp10.as.  The sets named, default "base10
# etc10", are unpacked into <dir>, which must not exist; then, as
# osfmk/src/lites/doc/README.lites-1.1.u1, "Installing Lites on the
# x86", says:
#
#   /dev                      NetBSD's own "sh MAKEDEV all", then
#                             Lites' mapped-time devices from that
#                             file's appendix: "mknod time c 25 0",
#                             "mknod timezone c 26 0"
#   /etc/ttys                 the console getty on, ttyv0's off, as
#                             the file shows "On NetBSD-1.0"
#
# /mach_servers is made empty, for the servers.  The notes' link,
# "ln -s /sbin/init /mach_servers/mach_init", is not made: under OSF's
# kernel Lites runs /sbin/init itself, as the OSF/1 server does
# (docs/deferred.md, O-2).
#
# and /etc/fstab is NetBSD's sample /etc/fstab.wd, root on wd0a, which
# is how NetBSD's installer left it.  Lites' device numbers are
# 386BSD's and NetBSD's (server/i386/conf.c), so NetBSD's MAKEDEV
# makes the right nodes: wd0a is block 0, minor 0, which Lites calls
# hd0a.
#
# The servers themselves -- startup, emulator -- and bootstrap.conf
# go in with docs/tools/mkbootdisk.sh, ROOT_TREE=<dir>.
#
# Run as root: the sets hold files of several owners, and /dev holds
# device nodes.  Owners are kept as NetBSD's numbers; MAKEDEV's chown
# and chgrp resolve names on this host, so its nodes are then given
# the NetBSD numbers of the same names, from <dir>/etc.
#
set -eu

[ $# -ge 2 ] ||
	{ echo "usage: $0 <dir> <sets-dir> [set...]" >&2; exit 1; }
[ "$(id -u)" = 0 ] || { echo "$0: run as root" >&2; exit 1; }
ROOT=$1; SETS=$2; shift 2
[ $# -gt 0 ] || set -- base10 etc10
[ ! -e "$ROOT" ] || { echo "$ROOT exists" >&2; exit 1; }

mkdir -p "$ROOT"
for s in "$@"; do
	ls "$SETS/$s".?? >/dev/null 2>&1 ||
		{ echo "no $SETS/$s.??" >&2; exit 1; }
	cat "$SETS/$s".?? | gzip -dc |
		tar -xpf - --numeric-owner -C "$ROOT"
	echo "$ROOT: $s"
done

# /mach_servers, for the servers
mkdir -p "$ROOT/mach_servers"

# /dev
(cd "$ROOT/dev" && sh MAKEDEV all 2>/dev/null) || true
(cd "$ROOT/dev" && mknod time c 25 0 && mknod timezone c 26 0)
python3 - "$ROOT" <<'EOF'
import grp, os, pwd, stat, sys
root = sys.argv[1]
def table(path, idx):
    t = {}
    for line in open(path, encoding="latin-1"):
        f = line.rstrip("\n").split(":")
        if len(f) > idx and not f[0].startswith("#"):
            t[f[0]] = int(f[idx])
    return t
nbuid = table(os.path.join(root, "etc/master.passwd"), 2)
nbgid = table(os.path.join(root, "etc/group"), 2)
dev = os.path.join(root, "dev")
for d, dirs, files in os.walk(dev):
    for n in dirs + files:
        p = os.path.join(d, n)
        st = os.lstat(p)
        if not (stat.S_ISCHR(st.st_mode) or stat.S_ISBLK(st.st_mode)):
            continue
        try: u = nbuid.get(pwd.getpwuid(st.st_uid).pw_name, 0)
        except KeyError: u = 0
        try: g = nbgid.get(grp.getgrgid(st.st_gid).gr_name, 0)
        except KeyError: g = 0
        os.lchown(p, u, g)
EOF
echo "$ROOT/dev: $(find "$ROOT/dev" -type b -o -type c | wc -l) nodes"

# /etc/ttys
sed -e 's/^console\(.*\)off secure/console\1on secure/' \
    -e 's/^ttyv0\(.*\)on secure/#ttyv0\1off secure/' \
    "$ROOT/etc/ttys" > "$ROOT/ttys.$$"
cat "$ROOT/ttys.$$" > "$ROOT/etc/ttys"; rm "$ROOT/ttys.$$"
grep -E '^#?(console|ttyv0)' "$ROOT/etc/ttys"

# /etc/fstab
[ -e "$ROOT/etc/fstab" ] || cp -p "$ROOT/etc/fstab.wd" "$ROOT/etc/fstab"
