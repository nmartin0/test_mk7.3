#!/bin/sh
# SPDX-FileCopyrightText: 2026 Nicholas Martin
# SPDX-License-Identifier: MIT
#
# mkiso.sh -- build a bootable ISO carrying the kernel and bootstrap.
#
# WHAT THIS IS, AND WHAT IT IS NOT
#
# Every boot so far has used QEMU's -kernel, which loads a multiboot
# image directly and hands it -initrd as a module. That is a debugging
# convenience and not how a machine starts. This builds an ISO that a
# real bootloader boots: GRUB is on the disc, it reads a config, it
# multiboot-loads the kernel and passes bootstrap as a module. Boot it
# on hardware and the same thing happens.
#
# The kernel is genuinely multiboot -- magic 0x1BADB002 appears at
# offset 4488 of mach_kernel.PRODUCTION with flags 0x2 and a checksum
# that sums to zero -- so this needs no shim or wrapper.
#
# THE ISO IS NOT SELF-CONTAINED, and cannot be yet. The root filesystem
# still has to arrive as a disk. LITES can mount ext2 and kernfs and
# nothing else: cd9660 is in the filesystem table at slot 14 but is NOT
# built (obj/server has cd9660.h and no objects), so an ISO9660 root is
# not something this server can read. MFS, for a ramdisk root, is in
# the same position. Until one of those is built, the disc boots the
# system and the disks hold it.
#
# DEVICE NUMBERING MATTERS HERE, and -cdrom is a trap. The bootstrap is
# told which device holds /mach_servers with BOOTUNIT, and QEMU's
# -cdrom is shorthand for IDE index 2 -- the servers volume. Using it
# collides outright:
#
#   qemu-system-i386: -drive file=/tmp/servers.img,...,index=2:
#   drive with bus=1, unit=0 (index=2) exists
#
# So the CD is attached explicitly at index 3 and the disks keep 0, 1
# and 2, leaving hd2 meaning what BOOTUNIT=2 says. A wrong ordering
# fails less obviously than the collision above: the bootstrap reads a
# filesystem that is not there.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
: "${MK_BUILD:=$HOME/.cache/mk7.3}"

K="$MK_BUILD/obj/at386/mach_kernel/PRODUCTION/mach_kernel.PRODUCTION"
BOOTSTRAP="$MK_BUILD/obj/at386/bootstrap/bootstrap"
ISO=${ISO:-/tmp/mk73.iso}
STAGE=$(mktemp -d)

for f in "$K" "$BOOTSTRAP"; do
	[ -r "$f" ] || {
		echo "missing: $f" >&2
		echo "  produced by: sh build/ode.sh -here ..." >&2
		exit 1; }
done
command -v grub-mkrescue >/dev/null || {
	echo "grub-mkrescue not found: apt-get install grub-pc-bin xorriso" >&2
	exit 1; }

mkdir -p "$STAGE/boot/grub"
cp "$K" "$STAGE/boot/mach_kernel"
cp "$BOOTSTRAP" "$STAGE/boot/bootstrap"

# The command line is the same one tools/boot-ide.sh passes through
# QEMU's -append, because it goes to the same place: the kernel's
# multiboot command line.
#
#   -r            serial console (see DEBUGGING.md); without it output
#                 goes to VGA and /tmp/console.log stays empty
#   BOOTDEV=hd    look for the server on an IDE disk
#   BOOTUNIT=2    unit 2, the servers volume
#   BOOTPART=2    partition c
#   -o            boot the servers listed in bootstrap.conf
cat > "$STAGE/boot/grub/grub.cfg" <<'EOF'
set timeout=3
set default=0

menuentry "OSF Mach Kernel 7.3 + LITES (serial console)" {
	multiboot /boot/mach_kernel -r BOOTDEV=hd BOOTUNIT=2 BOOTPART=2 -o
	module /boot/bootstrap bootstrap
	boot
}

menuentry "OSF Mach Kernel 7.3 + LITES (VGA console)" {
	multiboot /boot/mach_kernel BOOTDEV=hd BOOTUNIT=2 BOOTPART=2 -o
	module /boot/bootstrap bootstrap
	boot
}

menuentry "OSF Mach Kernel 7.3 + LITES (single user)" {
	multiboot /boot/mach_kernel -r -s BOOTDEV=hd BOOTUNIT=2 BOOTPART=2 -o
	module /boot/bootstrap bootstrap
	boot
}
EOF

rm -f "$ISO"
grub-mkrescue -o "$ISO" "$STAGE" >/dev/null 2>&1 || {
	echo "grub-mkrescue failed" >&2; rm -rf "$STAGE"; exit 1; }
rm -rf "$STAGE"

[ -s "$ISO" ] || { echo "no ISO produced" >&2; exit 1; }
echo "built $ISO ($(wc -c < "$ISO") bytes)"
echo
echo "boot it with the disks the system still needs:"
echo "  qemu-system-i386 -boot d \\"
echo "    -drive file=/tmp/root.img,format=raw,if=ide,index=0 \\"
echo "    -drive file=/tmp/swap.img,format=raw,if=ide,index=1 \\"
echo "    -drive file=/tmp/servers.img,format=raw,if=ide,index=2 \\"
echo "    -drive file=$ISO,format=raw,if=ide,index=3,media=cdrom \\"
echo "    -m 128 -display none -no-reboot \\"
echo "    -serial unix:/tmp/serial.sock,server,nowait"
