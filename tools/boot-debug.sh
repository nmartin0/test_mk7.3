#!/bin/sh
#
# boot-debug.sh -- boot with the in-kernel debugger and an interactive
# console, so you can type at it.
#
# This is the counterpart to boot-ide.sh. That one boots the PRODUCTION
# kernel and captures everything to /tmp/console.log, which is what you
# want when you just need to see what happens. This one boots the DEBUG
# kernel, which has OSFMK's own debugger (ddb) compiled in, and connects
# the serial console to your terminal so the debugger can read what you
# type.
#
# You cannot script this one, because it waits for you. You cannot
# inspect the other one, because there is nothing to type at. So both
# kernels are kept and you pick per question.
#
# Build the DEBUG kernel once:
#
#	sh build/ode.sh -here mach_kernel MACH_KERNEL_CONFIG=DEBUG
#
# Then:
#
#	sh tools/boot-debug.sh
#
# The kernel stops almost immediately with
#
#	inline call to debugger(machine_startup)
#	Stopped	at  0x1bc8bd:	int	$3
#	db8$>
#
# Type "c" to continue booting. Useful commands at that prompt:
#
#	c  or  continue		carry on
#	trace			stack backtrace
#	show all threads	every thread in the system
#	show task <addr>	one task
#	show all ports		Mach ports -- what the stub cannot do
#	examine <addr>		dump memory
#	break <addr>		set a breakpoint
#	step			single step
#	help			the command list
#
# To get back to the prompt after continuing, press Ctrl-A then B in
# the terminal: QEMU sends a serial break, which drops the kernel into
# the debugger.
#
# To quit QEMU entirely, press Ctrl-A then X.
#
set -e

: "${MK_BUILD:?set MK_BUILD first}"

K="$MK_BUILD/obj/at386/mach_kernel/DEBUG/mach_kernel.DEBUG"
BOOTSTRAP="$MK_BUILD/obj/at386/bootstrap/bootstrap"

if [ ! -r "$K" ]; then
	echo "no DEBUG kernel at $K"
	echo "build it with:"
	echo "  sh build/ode.sh -here mach_kernel MACH_KERNEL_CONFIG=DEBUG"
	exit 1
fi
for f in "$BOOTSTRAP" /tmp/servers.img /tmp/root.img /tmp/swap.img; do
	[ -r "$f" ] || { echo "missing: $f -- run boot-ide.sh first"; exit 1; }
done

pkill -f qemu-system-i386 2>/dev/null || true
sleep 1

cat <<'EOT'

  The debugger will stop at machine_startup and show "db8$>".
  Type "c" to continue booting.
  Ctrl-A then B  drops back into the debugger.
  Ctrl-A then X  quits QEMU.

EOT

# -serial mon:stdio puts the guest's serial line on this terminal and
# keeps QEMU's own Ctrl-A escapes working. No -display none here: that
# suppresses output handling we want.
exec qemu-system-i386 -enable-kvm -kernel "$K" \
	-append "-r BOOTDEV=hd BOOTUNIT=2 BOOTPART=2 -o" \
	-initrd "$BOOTSTRAP" \
	-drive file=/tmp/root.img,format=raw,if=ide,index=0 \
	-drive file=/tmp/swap.img,format=raw,if=ide,index=1 \
	-drive file=/tmp/servers.img,format=raw,if=ide,index=2 \
	-m 128 -display none -no-reboot \
	-serial mon:stdio
