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

for f in "$KERNEL" "$BOOTSTRAP" "$DISK"; do
	[ -r "$f" ] || { echo "missing: $f" >&2; exit 1; }
done

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

exec qemu-system-i386 $ACCEL \
	-kernel "$KERNEL" -initrd "$BOOTSTRAP" \
	-drive file="$DISK",format=raw,if=ide,index=0 \
	-m 64 -no-reboot $SCREEN "$@"
