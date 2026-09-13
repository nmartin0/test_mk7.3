#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nicholas Martin
"""Read guest LINEAR memory from a running QEMU via the monitor.

Companion to tools/pmem.py, which reads guest PHYSICAL memory. Use this
one for stacks and anything reached through a register.

  pmem.py   pmemsave   guest physical   globals at known link addresses
  vmem.py   x/Nxw      guest linear     stacks, register-derived pointers

IMPORTANT: this kernel is relocated by segmentation. The data segments
have base 0xC0000000, so a register value is a segment offset, not a
linear address. An ESP of 0x08b78f1c is linear 0xC8B78F1C. Reading the
raw register value returns "Cannot access memory" and looks exactly like
a corrupt stack -- add the segment base first.

Intended to be called from inside gdb while the guest is stopped:

    (gdb) shell python3 tools/vmem.py /tmp/mon 0xc8b78f1c 8

Start QEMU with BOTH interfaces:

    qemu-system-i386 ... -s -S -monitor unix:/tmp/mon,server,nowait

  usage: vmem.py <monitor-socket> <hex-linear-address> <nwords>
"""
import re
import socket
import sys
import time


def main():
    mon, addr, n = sys.argv[1], sys.argv[2], sys.argv[3]
    s = socket.socket(socket.AF_UNIX)
    s.connect(mon)
    time.sleep(0.5)
    s.setblocking(False)
    try:
        s.recv(65536)          # drain the banner
    except Exception:
        pass
    s.setblocking(True)
    s.sendall(f"x/{n}xw {addr}\n".encode())

    # The monitor echoes the command back one character at a time with
    # readline escapes before the reply arrives, so drain for a while.
    out = b""
    s.settimeout(0.6)
    deadline = time.time() + 4
    while time.time() < deadline:
        try:
            out += s.recv(65536)
        except Exception:
            pass
    s.close()

    txt = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", out.decode(errors="replace"))
    hit = False
    for line in txt.splitlines():
        line = line.strip()
        if re.match(r"^[0-9a-f]{8,16}:", line):
            print("   ", line)
            hit = True
    if not hit:
        print("    (no data -- unmapped, or you forgot the 0xC0000000 "
              "segment base)")


if __name__ == "__main__":
    main()
