#!/bin/sh
#
# boot.sh -- boot the kernel and OSF's bootstrap task under QEMU.
#
#   sh docs/tools/boot.sh <disk.img> [qemu options...]
#
# <disk.img> is the boot disk docs/tools/mkbootdisk.sh writes.  The
# kernel and bootstrap task come from the sandbox's object tree,
# MK_BUILD (default ~/.cache/osfmk, as docs/tools/mksandbox.sh uses).
# Anything after the disk goes to QEMU as it is.
#
# KVM when /dev/kvm is usable, TCG otherwise.  Under TCG -icount
# shift=auto is added: without it this kernel takes device interrupts
# seconds late, because its idle loop never halts and TCG can pass
# over the few instructions in each pass where interrupts are enabled
# -- docs/method.md section 4, "Slow is not hung".  KVM runs the guest
# on the real CPU, which takes a pending interrupt as soon as it is
# enabled, so it needs nothing.
#
# BIOS=1 boots the disk itself, through QEMU's BIOS: OSF's primary
# boot and mach_boot, which mkbootdisk.sh's BOOT and MACH_BOOT install,
# load /mach_kernel and /mach_servers/bootstrap from partition a, and
# MK_BUILD is not read.  The disk is given the geometry its label
# assumes, 16 heads and 63 sectors: the primary reads the partition by
# its CHS address, and left to guess, QEMU's BIOS reads it elsewhere.
#
# The console is the VGA screen.  HEADLESS=1 runs without a window and
# with QEMU's monitor on stdio, where "screendump shot.ppm" saves the
# screen and "quit" ends the run.
#
set -eu

[ $# -ge 1 ] || { echo "usage: $0 <disk.img> [qemu options...]" >&2; exit 1; }
DISK=$1; shift

MK_BUILD=${MK_BUILD:-$HOME/.cache/osfmk}
OBJ=$MK_BUILD/obj/at386
KERNEL=$OBJ/mach_kernel/PRODUCTION/mach_kernel.PRODUCTION
BOOTSTRAP=$OBJ/bootstrap/bootstrap
if [ "${BIOS:-0}" = 1 ]; then
	[ -r "$DISK" ] || { echo "missing: $DISK" >&2; exit 1; }
	CYLS=$(( $(wc -c < "$DISK") / 512 / (16 * 63) ))
	LOAD="-drive file=$DISK,format=raw,if=none,id=d0"
	LOAD="$LOAD -device ide-hd,drive=d0,bus=ide.0,unit=0"
	LOAD="$LOAD,cyls=$CYLS,heads=16,secs=63,bios-chs-trans=none"
else
	for f in "$KERNEL" "$BOOTSTRAP" "$DISK"; do
		[ -r "$f" ] || { echo "missing: $f" >&2; exit 1; }
	done
	LOAD="-kernel $KERNEL -initrd $BOOTSTRAP"
	LOAD="$LOAD -drive file=$DISK,format=raw,if=ide,index=0"
fi

if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
	ACCEL="-accel kvm"
	echo "boot.sh: KVM"
else
	ACCEL="-accel tcg -icount shift=auto"
	echo "boot.sh: no usable /dev/kvm; TCG with -icount shift=auto"
fi

if [ "${HEADLESS:-0}" = 1 ]; then
	SCREEN="-display none -monitor stdio"
else
	SCREEN=""
fi

exec qemu-system-i386 $ACCEL $LOAD \
	-m 64 -no-reboot $SCREEN "$@"
