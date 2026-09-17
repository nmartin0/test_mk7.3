#!/bin/sh
#
# mkroot-netbsd.sh -- fetch NetBSD 1.0/i386 and build an ext2 root
# filesystem LITES can boot from.
#
# WHY NetBSD 1.0, AND WHY AT ALL
#
# LITES has no userland of its own. Once mach_init runs it execs
# /sbin/init and falls back to /bin/sh, and those come from a real BSD
# system. LITES's own documentation says so: doc/install.freebsd, by
# Helander in December 1994, says to install FreeBSD 2.0 on the machine,
# install the Mach kernel, create /mach_servers and populate it with
# startup, emulator and mach_init. doc/README.netbsd documents the same
# against NetBSD 1.0, which is what this script fetches.
#
# So LITES takes over an existing BSD installation: the BSD system
# provides /sbin/init, /bin/sh, libc and the whole userland, and Mach
# adds three files. There is no hand-rolled libc in the design.
#
# WHY ext2 RATHER THAN FFS
#
# NetBSD's native filesystem is FFS, and doc/README.netbsd carries a
# patch to teach the bootstrap task to read 4.4BSD's FFS directory
# entries, where d_reclen was split into d_type and d_namlen. That is
# the same change ext2's filetype feature makes, which this project
# already handled with -O ^filetype.
#
# That patch is only needed if the userland lives on FFS. Both our
# readers already handle ext2, it is proven working for the root and the
# server volume, and debugfs can populate it without mounting and
# without privileges. So the files are extracted from NetBSD's tar and
# written into ext2 instead.
#
# WHY THE BINARIES STILL WORK
#
# liblites/exec_file.c classifies a.out by the machine id in
# (magic >> 16) & 0xff. NetBSD/i386's id is not in that switch, so its
# QMAGIC binaries fall through to BT_FREEBSD. That does not matter:
# emulator/i386/e_trampoline.c gives BT_386BSD, BT_NETBSD, BT_FREEBSD
# and the default case the same syscall table, e_bsd_sysent.
#
set -e

MIRROR=${MIRROR:-https://archive.netbsd.org/pub/NetBSD-archive/NetBSD-1.0/i386/binary}
WORK=${WORK:-/tmp/netbsd10}
ROOT=${ROOT:-/tmp/root.img}
ROOT_MB=${ROOT_MB:-64}

# The base set is split into 240640-byte pieces named base10.aa through
# base10.bb. Catted together they form a gzipped tar. 28 pieces, about
# 6.7 MB.
PIECES="aa ab ac ad ae af ag ah ai aj ak al am an ao ap aq ar as at au av aw ax ay az ba bb"

for t in mke2fs debugfs; do
	if command -v $t >/dev/null 2>&1; then
		eval "$(echo $t | tr a-z A-Z)=$t"
	elif [ -x /sbin/$t ]; then
		eval "$(echo $t | tr a-z A-Z)=/sbin/$t"
	else
		echo "$t not found; install e2fsprogs"; exit 1
	fi
done

command -v curl >/dev/null 2>&1 || { echo "curl not found"; exit 1; }

mkdir -p "$WORK/sets" "$WORK/tree"

echo "fetching NetBSD 1.0/i386 base set (28 pieces, ~6.7 MB)"
for p in $PIECES; do
	f="$WORK/sets/base10.$p"
	[ -s "$f" ] && continue
	curl -fsS -o "$f" "$MIRROR/base10/base10.$p" || {
		echo "failed to fetch base10.$p"; exit 1; }
	printf '.'
done
echo

# etc10 holds /etc, including the rc scripts and ttys that init reads.
# It is one piece.
echo "fetching etc set"
[ -s "$WORK/sets/etc10.aa" ] || \
	curl -fsS -o "$WORK/sets/etc10.aa" "$MIRROR/etc10/etc10.aa" || true

echo "extracting"
rm -rf "$WORK/tree"
mkdir -p "$WORK/tree"
cat "$WORK"/sets/base10.* | (cd "$WORK/tree" && tar xzf -)
[ -s "$WORK/sets/etc10.aa" ] && \
	cat "$WORK"/sets/etc10.* | (cd "$WORK/tree" && tar xzf -) || true

# What we actually need is small. A full base set is about 25 MB
# extracted; a root that boots to a shell needs far less. Listing it
# explicitly keeps the image small and makes the dependency set visible
# rather than implied.
NEED="sbin/init bin/sh bin/ls bin/cat bin/cp bin/mv bin/rm bin/mkdir
      bin/echo bin/pwd bin/ps bin/date bin/stty sbin/mount sbin/umount"

echo "creating $ROOT (${ROOT_MB} MB, ext2)"
rm -f "$ROOT"
dd if=/dev/zero of="$ROOT" bs=1M count="$ROOT_MB" 2>/dev/null

# ^filetype is the one that matters: in ext2 revision 0 a directory
# entry's name_len is a 16-bit field, and the filetype feature splits it
# into an 8-bit name_len and an 8-bit file_type. LITES's reader is from
# 1995 and expects the 16-bit form. The others are later additions it
# does not know. Do NOT also pass -r 0; that changes inode-size handling
# and the mount then fails with EINVAL.
"$MKE2FS" -q -F -b 1024 \
	-O ^resize_inode,^dir_index,^ext_attr,^sparse_super,^filetype \
	-I 128 "$ROOT"

for d in bin sbin etc dev tmp usr usr/bin usr/lib mach_servers; do
	"$DEBUGFS" -w -R "mkdir /$d" "$ROOT" >/dev/null 2>&1
done

echo "populating"
for f in $NEED; do
	[ -f "$WORK/tree/$f" ] || { echo "  missing: $f"; continue; }
	"$DEBUGFS" -w -R "write $WORK/tree/$f /$f" "$ROOT" >/dev/null 2>&1
done

# /dev/console is what mach_init opens before it can report anything.
# Character major 0 is "console" in LITES's cdevsw -- see
# server/i386/conf.c, where entry 0 is { "console", 0, console_ops }.
# The rest follow the same table: 1 tty, 2 kmem/null, 8 com.
"$DEBUGFS" -w -R "mknod /dev/console c 0 0" "$ROOT" >/dev/null 2>&1
"$DEBUGFS" -w -R "mknod /dev/tty c 1 0"     "$ROOT" >/dev/null 2>&1
"$DEBUGFS" -w -R "mknod /dev/null c 2 2"    "$ROOT" >/dev/null 2>&1
"$DEBUGFS" -w -R "mknod /dev/mem c 2 0"     "$ROOT" >/dev/null 2>&1
"$DEBUGFS" -w -R "mknod /dev/kmem c 2 1"    "$ROOT" >/dev/null 2>&1

echo
echo "$ROOT:"
"$DEBUGFS" -R "ls -l /" "$ROOT" 2>/dev/null | sed 's|^|  |'
echo
echo "next: put startup, emulator and mach_init in /mach_servers,"
echo "then boot with tools/boot-ide.sh"
