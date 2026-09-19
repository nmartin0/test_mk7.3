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

# The sets are also mirrored in the reference collection at
# nmartin0/mach_stuff under netbsd-1.0-i386/binary, so MIRROR can be
# pointed at a local path to avoid refetching:
#   MIRROR=file:///path/to/mach_stuff/netbsd-1.0-i386/binary
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
# NetBSD 1.0's tar preserves 1994 permissions, and much of
# usr/share/zoneinfo is r--r--r-- inside r-xr-xr-x directories. A plain
# rm -rf cannot remove a file from a directory it cannot write, so make
# the tree writable before deleting it.
if [ -d "$WORK/tree" ]; then
	chmod -R u+rwX "$WORK/tree" 2>/dev/null || true
	rm -rf "$WORK/tree"
fi
mkdir -p "$WORK/tree"

# --no-same-permissions keeps the same thing from happening on the way
# in: the files land owned by us and writable, which is what we want,
# since the modes that matter are the ones debugfs sets in the image.
# --no-same-owner because the archive's uid/gid are from 1994 and we are
# not root.
TARFLAGS="--no-same-owner --no-same-permissions"
cat "$WORK"/sets/base10.* | (cd "$WORK/tree" && tar xzf - $TARFLAGS)
[ -s "$WORK/sets/etc10.aa" ] && \
	cat "$WORK"/sets/etc10.* | (cd "$WORK/tree" && tar xzf - $TARFLAGS) || true

echo "extracted $(find "$WORK/tree" -type f 2>/dev/null | wc -l) files"

# What we actually need is small. A full base set is about 25 MB
# extracted; a root that boots to a shell needs far less. Listing it
# explicitly keeps the image small and makes the dependency set visible
# rather than implied.
# Verified against the real base10 set, not guessed. Every one of these
# exists at this path, and bin/sh and sbin/init are both **statically
# linked** -- their a.out flags field is 0, where EX_DYNAMIC is 0x20.
# Only usr/libexec/ld.so carries EX_DYNAMIC|EX_PIC (flags 0x30), and
# nothing here needs it. NetBSD 1.0 kept the traditional rule that /bin
# and /sbin are static because /usr may not be mounted at boot.
NEED="sbin/init bin/sh bin/ls bin/cat bin/cp bin/mv bin/rm bin/mkdir
      bin/echo bin/pwd bin/ps bin/date bin/stty bin/test bin/sync
      bin/chmod bin/ln bin/kill bin/sleep bin/df
      sbin/mount sbin/umount sbin/mknod sbin/reboot sbin/halt
      sbin/fsck sbin/dmesg sbin/disklabel"

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

for d in bin sbin etc dev tmp usr usr/bin usr/lib usr/libexec mach_servers; do
	"$DEBUGFS" -w -R "mkdir /$d" "$ROOT" >/dev/null 2>&1
done

echo "populating"
missing=
for f in $NEED; do
	if [ ! -f "$WORK/tree/$f" ]; then
		missing="$missing $f"
		continue
	fi
	"$DEBUGFS" -w -R "write $WORK/tree/$f /$f" "$ROOT" >/dev/null 2>&1
done
[ -n "$missing" ] && {
	echo "  NOT FOUND in the base set:$missing"
	echo "  (the NEED list is a guess at NetBSD 1.0's layout; adjust it)"
}

# Not needed for a shell prompt: /bin/sh and /sbin/init are static, as
# verified from their a.out headers. Installed anyway if present, so
# that anything from /usr/bin added later has what it needs.
for f in usr/libexec/ld.so usr/lib/libc.so.12.0 usr/lib/libc.so.12.20; do
	[ -f "$WORK/tree/$f" ] || continue
	d=$(dirname "/$f")
	"$DEBUGFS" -w -R "mkdir $d" "$ROOT" >/dev/null 2>&1
	"$DEBUGFS" -w -R "write $WORK/tree/$f /$f" "$ROOT" >/dev/null 2>&1
	echo "  installed /$f"
done

# /dev/console is what mach_init opens before it can report anything.
# Character major 0 is "console" in LITES's cdevsw -- see
# server/i386/conf.c, where entry 0 is { "console", 0, console_ops }.
# The rest follow the same table: 1 tty, 2 kmem/null, 8 com.
#
# debugfs's mknod takes a NAME IN THE CURRENT DIRECTORY, not a path.
# Given a path it allocates the inode, reports "Allocated inode: N", exits
# 0, and links it into the cwd under the whole string -- so `mknod
# /dev/console c 0 0` leaves the root directory holding an entry literally
# named "/dev/console", slashes included, and /dev empty. Measured with a
# raw dirent dump; e2fsck calls it "Entry '/dev/console' in / (2) has
# illegal characters in its name."
#
# Earlier versions of this script had no follow-up at all, so every root
# it built had a /dev with nothing in it. The obvious repair -- keep the
# mknod and add `ln <N> /dev/console` -- is worse than it looks: the
# stray root entry stays, `ln` adds the real one, and the inode ends up
# with two links and a link count of 1. e2fsck rejects that too.
#
# Doing the cd first is what actually works: one entry, in /dev, correct
# old-style rdev in i_block[0] (0x202 for c 2 2, which is what LITES's
# ext2_inode_cnv.c reads via di_db[0]), and a clean e2fsck.
#
# Modes: mknod leaves permission bits at 0000. Root bypasses them, and
# everything here runs as uid 0, but they are set anyway so the nodes are
# what NetBSD's MAKEDEV would have produced.
"$DEBUGFS" -w "$ROOT" >/dev/null 2>&1 <<-'EOF'
	cd /dev
	mknod console c 0 0
	mknod tty c 1 0
	mknod null c 2 2
	mknod mem c 2 0
	mknod kmem c 2 1
	sif /dev/console mode 020600
	sif /dev/tty mode 020666
	sif /dev/null mode 020666
	sif /dev/mem mode 020640
	sif /dev/kmem mode 020640
	mknod hd0c b 0 2
	sif /dev/hd0c mode 060640
EOF

# A device node that did not get linked is the failure this whole block
# exists to prevent, and it is silent -- so check rather than assume.
for n in console tty null mem kmem; do
	"$DEBUGFS" -R "stat /dev/$n" "$ROOT" 2>/dev/null |
	    grep -q "Type: character special" || {
		echo "mkroot: /dev/$n was not created" >&2; exit 1; }
done

# hd0c is the root disk itself, block major 0 (server/i386/conf.c entry
# 0 is "hd") minor 2 (partition c), which is what rootdev is. It is
# checked separately because it must be BLOCK special, and because the
# mode matters in a way that is easy to get wrong: 060640 is a block
# device, 0100640 is a regular file. debugfs's sif takes the whole mode
# word including the type bits, so writing the permission bits alone
# silently turns the node into an empty regular file -- which happened
# here, and which `ls` does not make obvious.
"$DEBUGFS" -R "stat /dev/hd0c" "$ROOT" 2>/dev/null |
    grep -q "Type: block special" || {
	echo "mkroot: /dev/hd0c is missing or is not a block device" >&2
	exit 1; }
echo "  /dev: console tty null mem kmem hd0c"

# /etc/fstab, which exists so the root can be remounted read-write.
#
# The root mounts READ-ONLY, and that is not an ext2 limitation: 4.4BSD
# mounts root read-only and /etc/rc remounts it, and this tree's FFS
# does the same thing on the same line (ffs_vfsops.c:109 against
# ext2_vfsops.c:117). Without a remount nothing can be written anywhere,
# including /tmp.
#
#   /sbin/mount -u -w /
#
# mount(8) finds the entry for / in fstab, so the entry has to exist.
#
# THE TYPE SAYS ufs AND THE FILESYSTEM IS ext2, deliberately. NetBSD
# 1.0's mount(8) has never heard of ext2fs -- it handles ufs internally
# and execs mount_<type> for anything else, and no mount_ext2fs exists.
# It does not matter: on an update the kernel skips the type entirely
# (vfs_syscalls.c, the MNT_UPDATE branch goes straight to `update:`)
# and keeps the mount's existing ext2fs_vfsops. The type in this file is
# only how mount(8) decides which helper to run, and ufs is the one that
# needs no helper.
#
# This is therefore a remount-only entry. A fresh `mount -t ufs` of this
# device would be wrong, and would fail on the superblock.
FSTAB=$(mktemp)
printf '/dev/hd0c\t/\tufs\trw\t1\t1\n' > "$FSTAB"
"$DEBUGFS" -w -R "rm /etc/fstab" "$ROOT" >/dev/null 2>&1
"$DEBUGFS" -w -R "write $FSTAB /etc/fstab" "$ROOT" >/dev/null 2>&1
"$DEBUGFS" -w -R "sif /etc/fstab mode 0100644" "$ROOT" >/dev/null 2>&1
rm -f "$FSTAB"
"$DEBUGFS" -R "stat /etc/fstab" "$ROOT" 2>/dev/null |
    grep -q "Type: regular" || {
	echo "mkroot: /etc/fstab was not created" >&2; exit 1; }
echo "  /etc/fstab: remount with  /sbin/mount -u -w /"

# The login chain: getty, login, the password database and the shared
# libraries they need.
#
# These are the first DYNAMICALLY LINKED binaries this project runs.
# /bin and /sbin are static, but /usr/libexec/getty and /usr/bin/login
# are not, so they exercise /usr/libexec/ld.so and the shared libc.
# That works: getty reaches its login prompt and ld.so resolves and
# reports missing libraries by name, which is how the list below was
# arrived at rather than guessed -- ld.so said
#
#   ld.so: login: libskey.so.0.0: No such file or directory
#
# libskey is S/Key one-time passwords, which login is linked against
# whether or not it is used.
#
# root has an EMPTY password in the shipped master.passwd, and its
# shell is /bin/csh, so csh is installed too. pwd.db and spwd.db ship
# built in the etc set, so pwd_mkdb does not need to run here.
LOGIN_FILES="usr/libexec/getty usr/bin/login bin/csh
             usr/lib/libskey.so.0.0 usr/lib/libcrypt.so.0.0
             usr/lib/libutil.so.3.1 usr/lib/libtermcap.so.0.0
             usr/lib/libcurses.so.2.1"

# /etc, which was empty apart from the fstab above. ttys is what
# multi-user init reads to decide what to spawn; gettytab is getty's
# own configuration; the rest is what login wants.
ETC_FILES="etc/master.passwd etc/passwd etc/pwd.db etc/spwd.db
           etc/group etc/shells etc/motd etc/gettytab etc/ttys
           etc/csh.cshrc etc/csh.login"

for f in $LOGIN_FILES $ETC_FILES; do
	[ -f "$WORK/tree/$f" ] || { echo "  not in the sets: $f"; continue; }
	d=$(dirname "/$f")
	[ "$d" = "/" ] || "$DEBUGFS" -w -R "mkdir $d" "$ROOT" >/dev/null 2>&1
	"$DEBUGFS" -w -R "rm /$f" "$ROOT" >/dev/null 2>&1
	"$DEBUGFS" -w -R "write $WORK/tree/$f /$f" "$ROOT" >/dev/null 2>&1
	case "$f" in
	etc/*)	mode=0100644 ;;
	*)	mode=0100755 ;;
	esac
	"$DEBUGFS" -w -R "sif /$f mode $mode" "$ROOT" >/dev/null 2>&1
done

for f in usr/libexec/getty usr/bin/login etc/master.passwd etc/ttys; do
	"$DEBUGFS" -R "stat /$f" "$ROOT" 2>/dev/null |
	    grep -q "Type: regular" || {
		echo "mkroot: /$f was not installed" >&2; exit 1; }
done
echo "  login chain: getty login csh + /etc password database"

# /etc/ttys decides what multi-user init spawns, and the shipped file
# has the console line OFF:
#
#   console "/usr/libexec/getty Pc"  pc3    off secure
#   ttyv0   "/usr/libexec/getty Pc"  pc3    on  secure
#
# That is right for a real PC, where the user is at ttyv0, one of the
# virtual consoles the NetBSD kernel provides. We have no ttyv0: LITES
# gives us /dev/console and nothing else, so with the file unmodified
# init spawns getty on a device that does not exist and nothing appears
# on the console at all.
#
# So the console line is turned on and the ttyv lines left alone --
# they are harmless, since init skips an entry whose device is missing.
TTYS=$(mktemp)
sed 's|^console\(.*\)off secure|console\1on  secure|' \
	"$WORK/tree/etc/ttys" > "$TTYS"
grep -q '^console.*on  secure' "$TTYS" || {
	echo "mkroot: could not enable the console line in /etc/ttys" >&2
	exit 1; }
"$DEBUGFS" -w -R "rm /etc/ttys" "$ROOT" >/dev/null 2>&1
"$DEBUGFS" -w -R "write $TTYS /etc/ttys" "$ROOT" >/dev/null 2>&1
"$DEBUGFS" -w -R "sif /etc/ttys mode 0100644" "$ROOT" >/dev/null 2>&1
rm -f "$TTYS"

# /etc/rc is OURS, not NetBSD's.
#
# Multi-user init runs /bin/sh /etc/rc before it spawns anything from
# ttys, and treats a failure as a reason to drop to single user. The
# shipped rc cannot run here: it fscks the disks, mounts everything in
# fstab, runs swapon, starts the network and syslogd, and calls
# programs from /usr/sbin that this root does not have. Installing it
# would produce a page of failures and then a single-user shell.
#
# This one does the two things that actually need doing on this system:
# remount the root read-write, which BSD also does from rc and which is
# otherwise a command to remember, and say so on the console so a boot
# that reaches rc can be told from one that does not.
RC=$(mktemp)
cat > "$RC" <<'RCEOF'
#	/etc/rc -- minimal, written for LITES on OSF Mach Kernel 7.3.
#
#	NOT NetBSD's own rc, which fscks, mounts everything in fstab,
#	runs swapon, starts the network and needs /usr/sbin programs
#	this root does not carry.

HOME=/; export HOME
PATH=/sbin:/bin:/usr/sbin:/usr/bin; export PATH

echo "rc: remounting / read-write"
/sbin/mount -u -w / || echo "rc: remount failed, / stays read-only"

echo "rc: done"
RCEOF
"$DEBUGFS" -w -R "rm /etc/rc" "$ROOT" >/dev/null 2>&1
"$DEBUGFS" -w -R "write $RC /etc/rc" "$ROOT" >/dev/null 2>&1
"$DEBUGFS" -w -R "sif /etc/rc mode 0100644" "$ROOT" >/dev/null 2>&1
rm -f "$RC"

for f in etc/ttys etc/rc; do
	"$DEBUGFS" -R "stat /$f" "$ROOT" 2>/dev/null |
	    grep -q "Type: regular" || {
		echo "mkroot: /$f was not installed" >&2; exit 1; }
done
echo "  /etc/ttys: console line enabled; /etc/rc: remounts / rw"

echo
echo "$ROOT:"
"$DEBUGFS" -R "ls -l /" "$ROOT" 2>/dev/null | sed 's|^|  |'
echo
echo "next: put startup, emulator and mach_init in /mach_servers,"
echo "then boot with tools/boot-ide.sh"
