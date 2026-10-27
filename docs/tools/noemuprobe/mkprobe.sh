#!/bin/sh
# mkprobe.sh <out> [<source>] -- build probe.c, or <source> in its
# style, into a NetBSD 1.0 ZMAGIC a.out:
# header and text in the first pages from 0x1000, entry 0x1020, data
# and BSS in the pages after, as NetBSD's ld lays out a static
# program.  Needs
# a host gcc and binutils for i386 (-m32); no NetBSD tools.
set -eu
D=$(cd "$(dirname "$0")" && pwd); OUT=$1; SRC=${2:-$D/probe.c}; T=$(mktemp -d)
gcc -m32 -O1 -ffreestanding -fno-pic -fno-stack-protector -fno-builtin \
	-c "$SRC" -o "$T/probe.o"
as --32 "$D/start.s" -o "$T/start.o"
(cd "$T" && ld -m elf_i386 -T "$D/probe.ld" start.o probe.o -o probe.elf)
objcopy -O binary -j .text "$T/probe.elf" "$T/text"
objcopy -O binary -j .data "$T/probe.elf" "$T/data"
python3 - "$T/text" "$T/data" "$OUT" <<'P'
import struct, sys
t = open(sys.argv[1], 'rb').read(); d = open(sys.argv[2], 'rb').read()
dsz = (len(d) + 0xfff) & ~0xfff	# data pages
tsz = (0x20 + len(t) + 0xfff) & ~0xfff	# text pages, the header in the first
# midmag 0x0086010b, network order: no flags, MID_I386, ZMAGIC
hdr = bytes([0x00, 0x86, 0x01, 0x0b]) + \
      struct.pack('<7I', tsz, dsz, 0, 0, 0x1020, 0, 0)
img = hdr + t
img += b'\0' * (tsz - len(img)) + d + b'\0' * (dsz - len(d))
open(sys.argv[3], 'wb').write(img)
P
rm -rf "$T"
