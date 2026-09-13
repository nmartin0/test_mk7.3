#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nicholas Martin
"""Dump the VGA text console from a running QEMU.
Uses the monitor's pmemsave, which reads GUEST PHYSICAL memory -- paging
never enters into it. gdb reads virtual addresses, which is why every
earlier attempt through gdb either faulted or returned zeros."""
import socket, sys, time, os
mon, out, delay = sys.argv[1], sys.argv[2], float(sys.argv[3])
if os.path.exists(out): os.unlink(out)
time.sleep(delay)
s = socket.socket(socket.AF_UNIX); s.connect(mon)
time.sleep(0.5); s.recv(65536)
s.sendall(f'pmemsave 0xb8000 4000 "{out}"\n'.encode())
time.sleep(1.5)
s.close()
if not os.path.exists(out): print("no dump"); sys.exit(1)
d = open(out, 'rb').read()
print("=== VGA CONSOLE ===")
blank = True
for r in range(25):
    line = ''.join(chr(d[(r*80+c)*2]) if 32 <= d[(r*80+c)*2] < 127 else ' ' for c in range(80)).rstrip()
    if line.strip(): print("  |" + line); blank = False
if blank: print("  (blank)")
