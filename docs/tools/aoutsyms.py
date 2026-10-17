#!/usr/bin/env python3
#
# aoutsyms.py -- list the dynamic symbols of a NetBSD 1.0 a.out program.
#
#   python3 docs/tools/aoutsyms.py BINARY [SYMBOL ...]
#
# NetBSD 1.0's programs are QMAGIC a.out, linked against shared
# libraries and stripped, so nm has nothing to read and today's
# binutils do not know the format.  What a stripped dynamic program
# keeps is its run-time linker's table: struct _dynamic at the start
# of the data segment, its section_dispatch_table, and an array of
# struct nzlist (NetBSD's include/link.h).  This walks that table.
#
# Each symbol prints with its type: 0x01 (N_UNDF|N_EXT) is taken from a
# shared library; 0x05 (N_TEXT|N_EXT) is the program's own.  That is
# how docs/deferred.md, S-12, tells which programs call libc's
# setproctitle() and which carry their own.

import struct
import sys


def dynsyms(path):
	b = open(path, 'rb').read()
	text = struct.unpack('<8I', b[:32])[1]
	base = 0x1000		# QMAGIC: file offset 0 is loaded at 0x1000

	def at(va, n):
		return b[va - base:va - base + n]

	sdt = struct.unpack('<3I', at(base + text, 12))[2]
	f = struct.unpack('<14I', at(sdt, 56))
	nz, strs = f[7], f[10]
	out = {}
	p = nz
	while p < strs:
		strx, typ, _, _, val, _ = struct.unpack('<IBBhII', at(p, 16))
		p += 16
		name = at(strs + strx, 256).split(b'\0')[0].decode()
		out[name] = (typ, val)
	return out


if __name__ == '__main__':
	if len(sys.argv) < 2:
		sys.exit('usage: aoutsyms.py BINARY [SYMBOL ...]')
	syms = dynsyms(sys.argv[1])
	for name in sys.argv[2:] or sorted(syms):
		if name in syms:
			typ, val = syms[name]
			print('%-28s type 0x%02x value 0x%08x' % (name, typ, val))
		else:
			print('%-28s absent' % name)
