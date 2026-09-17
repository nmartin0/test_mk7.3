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

# The server volume. 16 MB is ample for two servers and leaves room to
# add more; mkminix.py sizes its structures from the image it is given.
echo "building $SERVERS"
rm -f "$SERVERS"
dd if=/dev/zero of="$SERVERS" bs=1M count=16 2>/dev/null
python3 "$HERE/mkminix.py" "$SERVERS" "$PAGER=hd1c" "$LITES"

# The root and paging disks, if they are not already there. Neither is
# recreated by default: the root disk in particular may have contents
# worth keeping.
[ -f "$ROOT" ] || {
	echo "creating $ROOT (20 MB, ext2)"
	dd if=/dev/zero of="$ROOT" bs=1M count=20 2>/dev/null
	if command -v mke2fs >/dev/null 2>&1; then MKE2FS=mke2fs
	elif [ -x /sbin/mke2fs ]; then MKE2FS=/sbin/mke2fs
	else echo "mke2fs not found; install e2fsprogs"; exit 1; fi
	"$MKE2FS" -q -F -b 1024 \
		-O ^resize_inode,^dir_index,^ext_attr,^sparse_super \
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
