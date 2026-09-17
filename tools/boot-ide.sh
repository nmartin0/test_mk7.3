#!/bin/sh
#
# boot-ide.sh -- build a minix server volume on an IDE disk and boot
# from it, instead of from a floppy.
#
# Why: the boot spends almost all its wall time reading the ~1 MB server
# through an emulated 1.44 MB floppy controller with realistic timing.
# That is about 500 seconds per boot, and hardware acceleration does not
# help because it is I/O bound rather than CPU bound. On IDE the same
# read takes seconds. Over the number of boots a debugging session
# needs, this is the single largest speedup available.
#
# How it works:
#
#   BOOTDEV/BOOTUNIT/BOOTPART in the kernel command line choose the
#   device that becomes /dev/boot_device. model_dep.c concatenates
#   BOOTDEV and BOOTUNIT into a name, looks it up, then adds BOOTPART:
#
#       dev_name_lookup("hd2") -> ops = hd, unit = 2 * d_subdev
#       dev_set_indirection("boot_device", ops, unit + BOOTPART)
#
#   d_subdev is 16 for hd (i386/AT386/conf.c), so BOOTUNIT=2 BOOTPART=2
#   gives minor 2*16 + 2 = 34, which is hd2c -- unit 2, partition c.
#
#   Partition c is the whole disk: getvtoc() reads sector 0 as a DOS
#   partition table and, when that fails as it does on an unpartitioned
#   image, falls back to making partition c the whole disk.
#
#   The minix reader is device-agnostic. Every access in
#   file_systems/minixfs/minixfs.c goes through
#   device_read(fp->f_dev.dev_port, ...) on whatever port the bootstrap
#   task was handed, so nothing about it is floppy-specific.
#
# Disk layout this sets up:
#
#   hd0  ext2   LITES root filesystem
#   hd1  raw    paging, given to default_pager as hd1c
#   hd2  minix  /mach_servers, booted from
#
# Usage:
#	sh tools/boot-ide.sh            build the volume and boot
#	sh tools/boot-ide.sh -n         build the volume only
#
set -e

: "${MK_BUILD:?set MK_BUILD first}"

HERE=$(cd "$(dirname "$0")" && pwd)
K="$MK_BUILD/obj/at386/mach_kernel/PRODUCTION/mach_kernel.PRODUCTION"
BOOTSTRAP="$MK_BUILD/obj/at386/bootstrap/bootstrap"
PAGER="$MK_BUILD/obj/at386/default_pager/default_pager"
LITES="$HOME/lites-build/obj/server/startup.Lites.1.1.u3.STD+WS+osfmach3+ext2fs"

SERVERS=/tmp/servers.img
ROOT=/tmp/root.img
SWAP=/tmp/swap.img

for f in "$K" "$BOOTSTRAP" "$PAGER" "$LITES"; do
	[ -r "$f" ] || { echo "missing: $f"; exit 1; }
done

# Both filesystems are ext2 now, so find the tools once. They live in
# /sbin on Debian, which is not on a normal user's PATH.
if command -v mke2fs >/dev/null 2>&1; then MKE2FS=mke2fs
elif [ -x /sbin/mke2fs ]; then MKE2FS=/sbin/mke2fs
else echo "mke2fs not found; install e2fsprogs"; exit 1; fi

if command -v debugfs >/dev/null 2>&1; then DEBUGFS=debugfs
elif [ -x /sbin/debugfs ]; then DEBUGFS=/sbin/debugfs
else echo "debugfs not found; install e2fsprogs"; exit 1; fi

# The server volume, an ext2 filesystem.
#
# This was a minix volume built by tools/mkminix.py, for two reasons
# that both turned out to be avoidable.
#
# The first was practical: mkfs.minix left Debian 13, and the kernel's
# minix reader is particular about the magic number, so the image had to
# be written by hand.
#
# The second is a licence problem. file_systems/minixfs contains three
# GPL files -- minix_ffs_compat.c, minix_ffs_compat.h and minix_fs.h --
# and minixfs/machdep.mk builds minix_ffs_compat.o into libsa_fs.a, so
# the bootstrap task binary linked GPL code. file_systems/ext2fs is
# entirely GPL-free.
#
# The i386 bootstrap task already builds all three readers and tries
# them in order (file_systems/AT386/machdep.mk and fs_switch.c):
#
#	AT386_OFILES = ${UFS_OFILES} ${EXT2FS_OFILES} ${MINIXFS_OFILES}
#	&ufs_ops, &ext2fs_ops, &minixfs_ops,
#
# so ext2 needs no kernel change at all. stock mke2fs and debugfs build
# and populate it, the same tools the root filesystem already uses, and
# all three disks are now one filesystem type.
#
# ^filetype and the rest: see the root filesystem below for why.
echo "building $SERVERS"
rm -f "$SERVERS"
dd if=/dev/zero of="$SERVERS" bs=1M count=16 2>/dev/null
"$MKE2FS" -q -F -b 1024 \
	-O ^resize_inode,^dir_index,^ext_attr,^sparse_super,^filetype \
	-I 128 "$SERVERS"

"$DEBUGFS" -w -R "mkdir /mach_servers" "$SERVERS" >/dev/null 2>&1
"$DEBUGFS" -w -R "write $PAGER /mach_servers/default_pager" \
	"$SERVERS" >/dev/null 2>&1
"$DEBUGFS" -w -R "write $LITES /mach_servers/startup" \
	"$SERVERS" >/dev/null 2>&1

# bootstrap.conf gives each server its arguments, and for LITES that is
# the only way it gets a root device.
#
# get_config_info() has two paths. With argc == 0 it uses the
# compiled-in argv_space table, whose third entry is the root device.
# With any argument at all it takes the other path and calls
# parse_arguments(argc, argv) instead, and argv_space is never read.
#
# A server always has at least one argument here, its own name, because
# bootstrap.conf names it. So argc is 1, the argv_space path is dead,
# and parse_arguments does:
#
#	pname = argv[0]; argv++, argc--;	/* argc becomes 0 */
#	if (argc == 0) return;			/* returns at once */
#
# leaving rootdev at its uninitialised zero, which is major 0 minor 0 --
# device "hd", unit 0, partition "a". LITES then asks the kernel for
# hd0a, which does not exist on an unpartitioned disk, and the mount
# fails with D_NO_SUCH_DEVICE (0x9c6).
#
# Naming the device here makes argc 2, so parse_arguments reaches the
# end and sets rootdev from it. Editing argv_space has no effect,
# because that path never runs.
#
# Full paths are used, as the recovered MkLinux bootstrap.conf does.
BSCONF=$(mktemp)
cat > "$BSCONF" <<EOT
default_pager /mach_servers/default_pager hd1c
startup /mach_servers/startup hd0c
EOT
"$DEBUGFS" -w -R "write $BSCONF /mach_servers/bootstrap.conf" \
	"$SERVERS" >/dev/null 2>&1
rm -f "$BSCONF"

# The root and paging disks, if they are not already there. Neither is
# recreated by default: the root disk in particular may have contents
# worth keeping.
[ -f "$ROOT" ] || {
	echo "creating $ROOT (20 MB, ext2)"
	dd if=/dev/zero of="$ROOT" bs=1M count=20 2>/dev/null
	# ^filetype is the one that matters, and it is not obvious.
	#
	# In ext2 revision 0 a directory entry's name_len is a 16-bit
	# field. The filetype feature splits it into an 8-bit name_len
	# and an 8-bit file_type, and mke2fs enables it by default.
	# LITES's reader is from 1995 and expects the 16-bit form, so it
	# reads the "." entry -- name_len 1, file_type 2 for a directory
	# -- as name_len 0x0201, which is 513, and rejects the directory:
	#
	#   bad directory entry: reclen is too small for name_len
	#   offset=0, inode=2, rec_len=12, name_len=513
	#   /: bad dir ino 2 at offset 0: mangled entry
	#
	# The others are turned off for the same reason, being later
	# additions the reader does not know. Do NOT also pass -r 0: that
	# forces revision 0 and changes inode-size handling, after which
	# the mount fails with EINVAL.
	"$MKE2FS" -q -F -b 1024 \
		-O ^resize_inode,^dir_index,^ext_attr,^sparse_super,^filetype \
		-I 128 "$ROOT"
}
[ -f "$SWAP" ] || {
	echo "creating $SWAP (32 MB, raw)"
	dd if=/dev/zero of="$SWAP" bs=1M count=32 2>/dev/null
}

[ "$1" = "-n" ] && { echo "built; not booting"; exit 0; }

pkill -f qemu-system-i386 2>/dev/null || true
sleep 2
rm -f /tmp/console.log

echo "booting from hd2c ..."
qemu-system-i386 -enable-kvm -kernel "$K" \
	-append "-r BOOTDEV=hd BOOTUNIT=2 BOOTPART=2 -o" \
	-initrd "$BOOTSTRAP" \
	-drive file="$ROOT",format=raw,if=ide,index=0 \
	-drive file="$SWAP",format=raw,if=ide,index=1 \
	-drive file="$SERVERS",format=raw,if=ide,index=2 \
	-m 128 -display none -no-reboot \
	-serial file:/tmp/console.log "$@" &

# Report progress rather than sleeping blindly, so a stuck boot can be
# told from a slow one by whether the log is growing.
last=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12; do
	sleep 10
	now=$(wc -c < /tmp/console.log 2>/dev/null || echo 0)
	echo "  ${i}0s: $now bytes"
	if grep -q 'panic\|Copyright' /tmp/console.log 2>/dev/null; then
		break
	fi
	if [ "$now" = "$last" ] && [ "$i" -gt 6 ]; then
		echo "  (log has stopped growing)"
		break
	fi
	last=$now
done

echo
tail -25 /tmp/console.log
