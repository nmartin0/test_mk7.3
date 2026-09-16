#!/bin/sh
#
# catch-panic.sh -- attach gdb in the window between LITES starting and
# LITES panicking, and report who called panic().
#
# The window is the whole difficulty. Attaching before LITES runs finds
# kernel space and none of the server's addresses are mapped; attaching
# after the panic finds the task already terminated and its address
# space gone. The banner is the signal that the task exists, so this
# polls the console log for it and attaches immediately.
#
# Usage:
#	sh tools/catch-panic.sh
#
# Expects the environment already set up:
#	MK_BUILD	build directory
#	/tmp/minix.img	boot floppy
#	/tmp/root.img	root disk
#	/tmp/swap.img	paging disk
#
# Reports, from the stack at the moment panic() is entered:
#	fmt		the format argument as the caller passed it
#	caller		the return address, to be looked up with nm
#
set -e

: "${MK_BUILD:?set MK_BUILD first}"

K="$MK_BUILD/obj/at386/mach_kernel/PRODUCTION/mach_kernel.PRODUCTION"
B="$HOME/lites-build/obj/server/startup.Lites.1.1.u3.STD+WS+osfmach3+ext2fs.unstripped"
LOG=/tmp/console.log

[ -r "$K" ] || { echo "no kernel at $K"; exit 1; }
[ -r "$B" ] || { echo "no LITES binary at $B"; exit 1; }

PANIC=$(nm "$B" | awk '$2 == "T" && $3 == "panic" { print "0x" $1 }')
[ -n "$PANIC" ] || { echo "could not find panic in $B"; exit 1; }
echo "panic() is at $PANIC"

pkill -f qemu-system-i386 2>/dev/null || true
sleep 2
rm -f "$LOG"

# No -enable-kvm: the gdb stub is far more reliable under TCG, and
# breakpoints in particular are unreliable with hardware acceleration.
qemu-system-i386 -kernel "$K" \
	-append "-r BOOTDEV=fd BOOTPART=1 -o" \
	-initrd "$MK_BUILD/obj/at386/bootstrap/bootstrap" \
	-drive file=/tmp/minix.img,format=raw,if=floppy \
	-drive file=/tmp/root.img,format=raw,if=ide,index=0 \
	-drive file=/tmp/swap.img,format=raw,if=ide,index=1 \
	-m 128 -display none -no-reboot \
	-serial file:"$LOG" -s &
QEMU=$!

cleanup() { kill $QEMU 2>/dev/null || true; }
trap cleanup EXIT

# Wait for the banner. It is printed by LITES itself, so once it appears
# the task exists and its pages are mapped. Bound the wait: without KVM
# this boot takes tens of minutes, but waiting forever on a failed boot
# helps nobody.
echo "waiting for LITES to start (this is slow without KVM) ..."
waited=0
while ! grep -q 'Copyright' "$LOG" 2>/dev/null; do
	sleep 5
	waited=$((waited + 5))
	if [ $waited -ge 3600 ]; then
		echo "no banner after ${waited}s; giving up"
		tail -5 "$LOG"
		exit 1
	fi
	if [ $((waited % 120)) -eq 0 ]; then
		echo "  ${waited}s, log is $(wc -c < "$LOG") bytes"
	fi
done
echo "banner seen after ${waited}s; attaching"

# The window between the banner and the panic is short, so attach at
# once and let gdb do the waiting at the breakpoint.
gdb -q -batch \
	-ex 'target remote :1234' \
	-ex "break *$PANIC" \
	-ex 'continue' \
	-ex 'echo \n=== at panic ===\n' \
	-ex 'info registers eip esp' \
	-ex 'echo \n--- stack: [0]=return address, [1]=fmt ---\n' \
	-ex 'x/4xw $esp' \
	-ex 'echo \n--- fmt as a string ---\n' \
	-ex 'x/s *(char**)($esp+4)' \
	-ex 'echo \n--- fmt as instructions (is it code?) ---\n' \
	-ex 'x/6i *(char**)($esp+4)' \
	2>&1 | tee /tmp/panic-state.txt

echo
echo "=== who called panic ==="
RET=$(awk '/^0x[0-9a-f]+:/ { print $2; exit }' /tmp/panic-state.txt | sed 's/^0x//')
if [ -n "$RET" ]; then
	echo "return address: 0x$RET"
	nm "$B" | sort | awk -v r="$RET" '
		$2 ~ /^[TtWw]$/ && $1 <= r { last = $1 " " $3 }
		END { print "  called from: " last }'
else
	echo "could not read a return address; see /tmp/panic-state.txt"
fi
