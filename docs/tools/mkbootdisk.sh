#!/bin/sh
#
# mkbootdisk.sh -- make the disk the bootstrap task reads its servers from.
#
#   sh docs/tools/mkbootdisk.sh <disk.img> <server>...
#
# The bootstrap task (osfmk/src/bootstrap) reads
# /dev/boot_device/mach_servers/bootstrap.conf and then each server it
# names from /dev/boot_device/mach_servers/.  The kernel sets
# boot_device in machine_init(), i386/AT386/model_dep.c: with no BOOTDEV
# in the environment it is "hd0s1", the first DOS partition of the first
# IDE disk.  So this writes a raw disk image with one DOS partition
# holding an ext2 file system:
#
#   /mach_servers/bootstrap.conf    one "<name> <name>" line per server
#   /mach_servers/<server>...       the files given, by basename
#
# and the kernel's own default finds it; no kernel change and no boot
# argument.  Boot with
#
#   qemu-system-i386 -kernel mach_kernel.PRODUCTION -initrd bootstrap \
#       -drive file=<disk.img>,format=raw,if=ide,index=0 ...
#
# BOOTSTRAP_CONF=<file> uses that file as bootstrap.conf instead of the
# generated one.  DISK_MB sets the image size, default 8.
#
# Why ext2, and why these mke2fs options.  The bootstrap task's
# file_systems/AT386/fs_switch.c lists ufs, ext2fs and minixfs.  Of the
# three, only ext2 can be populated from a directory by a standard tool
# without privileges: "mke2fs -d" (e2fsprogs 1.43 and later).  The rest
# keep the file system in the shape its 1990s reader expects:
#
#   -r 0         ext2 revision 0, the original format.  Revision 1 adds
#                fields and feature flags; whether bootstrap's ext2fs
#                reads it has not been tested.
#   -b 1024      1 KB blocks, the old default.
#   -I 128       128-byte inodes, revision 0's fixed size.
#   -O none      no optional features.
#
# The partition starts at sector 63, the classic first-track offset, and
# is type 0x83.  The MBR is written with printf and dd, so nothing beyond
# e2fsprogs and coreutils is needed.
#
set -eu

[ $# -ge 2 ] || { echo "usage: $0 <disk.img> <server>..." >&2; exit 1; }
command -v mke2fs >/dev/null || { echo "mke2fs (e2fsprogs) is required" >&2; exit 1; }

OUT=$1; shift
DISK_MB=${DISK_MB:-8}
START=63

TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT INT TERM
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
		printf '%s %s\n' "$n" "$n"
	done > "$TMP/root/mach_servers/bootstrap.conf"
fi

TOTAL=$((DISK_MB * 2048))		# sectors
COUNT=$((TOTAL - START))
KB=$((COUNT / 2))

mke2fs -q -F -t ext2 -r 0 -b 1024 -I 128 -O none \
	-d "$TMP/root" "$TMP/part.img" "$KB" >/dev/null 2>&1

# little-endian 32-bit value as printf octal escapes
le32() {
	v=$1; s=""
	for i in 1 2 3 4; do
		s="$s\\$(printf '%03o' $((v % 256)))"
		v=$((v / 256))
	done
	printf '%s' "$s"
}

dd if=/dev/zero of="$OUT" bs=512 count="$TOTAL" status=none
# partition entry 1: active, CHS fields zero (LBA is what is read),
# type 0x83, start, count
printf "\\200\\000\\000\\000\\203\\000\\000\\000$(le32 $START)$(le32 $COUNT)" |
	dd of="$OUT" bs=1 seek=446 conv=notrunc status=none
printf '\125\252' | dd of="$OUT" bs=1 seek=510 conv=notrunc status=none
dd if="$TMP/part.img" of="$OUT" bs=512 seek="$START" conv=notrunc status=none

echo "$OUT: ${DISK_MB} MB, ext2 in partition 1 at sector $START"
sed 's/^/  bootstrap.conf: /' "$TMP/root/mach_servers/bootstrap.conf"
