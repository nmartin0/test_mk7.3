#!/bin/sh
#
# mkbootdisk.sh -- make the disk the bootstrap task reads its servers from.
#
#   sh docs/tools/mkbootdisk.sh <disk.img> <server>...
#
# The disk is laid out the way OSF's MK7.3 laid out an i386 disk, and
# holds the file system OSF's bootstrap task was written to read:
#
#   sector 0       a DOS master boot record with one partition, active,
#                  type 99 (UNIXOS) -- what bios_label.c calls an "old
#                  vtoc layed out disk"
#   base + 29      OSF's vtoc (HDPDLOC), struct evtoc in
#                  i386/AT386/disk.h
#   base + 30      the alternate-sector table, empty, at the byte
#                  offset 15360 and length 2048 of OSF's own sample
#   partition a    root: UFS, holding /mach_servers/
#   partition b    paging, PAGING_MB, unformatted
#   partition c    the whole UNIX partition; the driver computes it
#
# MK7.3 Release Notes section 2.7.4 is the source: the vtoc utility
# partitions the disk, newfs makes the file systems, a is root, b is
# paging, c is the entire disk; section 2.4.2.2's disk_setup does both.
# The boot loader reads hd(0,a)/mach_kernel and /mach_servers/bootstrap
# from partition a.  The kernel's default boot device in machine_init()
# resolves to unit 0, partition a, so no boot argument is needed.
#
# The file system is UFS in the 4.3BSD format OSF's reader expects,
# written by NetBSD's makefs(8) with "-o version=0": the nbmakefs that
# docs/tools/mknbmakefs.sh builds, or NBMAKEFS=<path>.  It contains
#
#   /mach_servers/bootstrap.conf    one "<name> <name>" line per server
#   /mach_servers/<server>...       the files given, by basename
#
# default_pager's line names partition b as its paging device,
# "default_pager default_pager hd0b", as the MK7.3 Release Notes'
# example does ("def_pager /mach_servers/default_pager sd0b") and
# bootstrap.template says it may ("the default_pager can taking a
# paging device name"); without one it has no paging space at all.
# PAGING_DEV=<name> names another, for a disk that is not hd0.
#
# BOOTSTRAP_CONF=<file> uses that file as bootstrap.conf instead.
# ROOT_TREE=<dir> puts the tree in <dir> on the file system first --
# a whole root, such as docs/tools/mklitesroot.sh makes for Lites --
# with owners, modes, device nodes and symbolic links kept; run as
# root for those.  The servers and bootstrap.conf go in on top.
# ROOT_MB sets partition a's size, default 16; PAGING_MB partition b's,
# default 16.  Partition a must fit in one cylinder group, about 40 MB
# at the inode density used here: makefs lays out a larger file system
# in groups that NetBSD 1.0's fsck and Lites' file system code fail on
# (docs/deferred.md, S-1), so the script stops.  Larger file systems
# are made with newfs on the running system, as OSF made its own
# (MK7.3 Release Notes 2.4.2.2).
#
# Boot with docs/tools/boot.sh.
#
set -eu

[ $# -ge 2 ] || { echo "usage: $0 <disk.img> <server>..." >&2; exit 1; }
command -v python3 >/dev/null ||
	{ echo "python3 is required" >&2; exit 1; }
NBMAKEFS=${NBMAKEFS:-${MK_BUILD:-$HOME/.cache/osfmk}/netbsd/tools/bin/nbmakefs}
[ -x "$NBMAKEFS" ] ||
	{ echo "no nbmakefs at $NBMAKEFS; run docs/tools/mknbmakefs.sh" >&2
	  exit 1; }

OUT=$1; shift
ROOT_MB=${ROOT_MB:-16}
PAGING_MB=${PAGING_MB:-16}

BASE=63				# first sector of the UNIX partition
A_START=$((BASE + 64))		# after the vtoc and alternates table
A_SIZE=$((ROOT_MB * 2048))
B_START=$((A_START + A_SIZE))
B_SIZE=$((PAGING_MB * 2048))
END=$((B_START + B_SIZE))
TOTAL=$(( (END + 2047) / 2048 * 2048 ))	# whole megabytes
U_SIZE=$((TOTAL - BASE))

TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT INT TERM
mkdir -p "$TMP/root"
if [ -n "${ROOT_TREE:-}" ]; then
	[ -d "$ROOT_TREE" ] || { echo "no directory $ROOT_TREE" >&2; exit 1; }
	cp -a "$ROOT_TREE/." "$TMP/root/"
fi
mkdir -p "$TMP/root/mach_servers"
for f in "$@"; do
	[ -f "$f" ] || { echo "no such file: $f" >&2; exit 1; }
	cp "$f" "$TMP/root/mach_servers/"
done
if [ -n "${BOOTSTRAP_CONF:-}" ]; then
	cp "$BOOTSTRAP_CONF" "$TMP/root/mach_servers/bootstrap.conf"
else
	for f in "$@"; do
		n=$(basename "$f")
		if [ "$n" = default_pager ]; then
			printf '%s %s %s\n' "$n" "$n" "${PAGING_DEV:-hd0b}"
		else
			printf '%s %s\n' "$n" "$n"
		fi
	done > "$TMP/root/mach_servers/bootstrap.conf"
fi

# version=0 is "newfs -O 0": 4.3BSD directory entries, with a 16-bit
# d_namlen and no d_type (file_systems/ufs/defs.h), and fs_maxsymlinklen
# 0 and fs_inodefmt FS_42INODEFMT, so a 4.4BSD reader (Lites) reads it
# as the old format too.  8 KB blocks and 1 KB fragments: OSF's reader
# takes blocks up to MAXBSIZE, 8192 (file_systems/ufs/fs.h).  One
# inode per 4096 bytes, newfs's own default (NFPI 4 fragments per
# inode, newfs.c); makefs alone gives only as many inodes as the tree
# needs, and the running system runs out at the first files it makes.
"$NBMAKEFS" -t ffs -o version=0,bsize=8192,fsize=1024,density=4096 \
	-s $((A_SIZE * 512)) "$TMP/a.img" "$TMP/root" >/dev/null
NCG=$(python3 -c 'import struct,sys
f=open(sys.argv[1],"rb"); f.seek(8192+44); print(struct.unpack("<i",f.read(4))[0])' "$TMP/a.img")
[ "$NCG" -eq 1 ] || {
	echo "ROOT_MB=$ROOT_MB makes $NCG cylinder groups; makefs's layout of" \
	     "more than one fails under NetBSD 1.0's fsck and Lites" \
	     "(docs/deferred.md, S-1).  Use a smaller ROOT_MB, and newfs" \
	     "on the running system for larger file systems." >&2
	exit 1; }

# The label: MBR, vtoc and alternates table.  The vtoc's geometry is
# nominal -- 16 heads, 63 sectors -- since every address in it is a
# sector number.
python3 - "$TMP/label.img" "$BASE" "$U_SIZE" "$A_START" "$A_SIZE" \
	"$B_START" "$B_SIZE" "$TOTAL" <<'EOF'
import struct, sys
out = sys.argv[1]
base, usize, astart, asize, bstart, bsize, total = map(int, sys.argv[2:])
heads, secs = 16, 63
cyls = total // (heads * secs)
img = bytearray((base + 64) * 512)

# MBR: one partition, active (0x80), type 99 (UNIXOS), LBA fields
img[446:462] = struct.pack("<B3sB3sII", 0x80, b"\0\0\0", 99, b"\0\0\0",
                           base, usize)
img[510:512] = b"\x55\xaa"

# struct evtoc at base + HDPDLOC (29); offsets from i386/AT386/disk.h
# as the i386 ABI lays it out: cyls 24, version 88, alt_ptr 92,
# alt_len 96, sanity 100, xcyls 104, nparts 116, label 120, part 160.
v = bytearray(512)
struct.pack_into("<III", v, 24, cyls, heads, secs)
struct.pack_into("<IIH", v, 88, 1, 15360, 2048)
struct.pack_into("<IIII", v, 100, 0x600DDEEE, cyls, heads, secs)
struct.pack_into("<H", v, 116, 3)
v[120:160] = b"mkbootdisk".ljust(40, b"\0")
parts = [(0x02, 0x200, astart, asize),		# a: V_ROOT, V_VALID
         (0x04, 0x201, bstart, bsize),		# b: paging, + V_UNMNT
         (0x05, 0x200, base, usize)]		# c: V_BACKUP
for i, (tag, flag, start, size) in enumerate(parts):
    struct.pack_into("<HHii", v, 160 + 12 * i, tag, flag, start, size)
img[(base + 29) * 512:(base + 30) * 512] = v

# struct alt_info at base + 30 (alt_ptr 15360): ALT_SANITY, no entries
alt = bytearray(2048)
struct.pack_into("<I", alt, 0, 0xDEADBEEF)
img[(base + 30) * 512:(base + 34) * 512] = alt
open(out, "wb").write(img)
EOF

dd if=/dev/zero of="$OUT" bs=512 count="$TOTAL" status=none
dd if="$TMP/label.img" of="$OUT" conv=notrunc status=none
dd if="$TMP/a.img" of="$OUT" bs=512 seek="$A_START" conv=notrunc status=none

echo "$OUT: $((TOTAL / 2048)) MB; a: UFS ${ROOT_MB} MB, b: paging ${PAGING_MB} MB"
sed 's/^/  bootstrap.conf: /' "$TMP/root/mach_servers/bootstrap.conf"
