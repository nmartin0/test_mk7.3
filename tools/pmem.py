#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nicholas Martin
"""Read guest PHYSICAL memory from a running QEMU via the monitor.

Intended to be called from inside gdb while the guest is stopped:

    (gdb) shell python3 tools/pmem.py /tmp/mon 0x1e0a08 4 /tmp/out.bin

gdb frequently cannot read kernel addresses at a breakpoint -- both the
link address and the linear address return "Cannot access memory" at
moments when reading registers works fine. pmemsave takes a guest
physical address and is unaffected by paging or segmentation, so this
works where gdb's own memory reads do not.

Start QEMU with BOTH interfaces:

    qemu-system-i386 ... -s -S -monitor unix:/tmp/mon,server,nowait

  usage: pmem.py <monitor-socket> <hex-address> <nbytes> <scratch-file>
"""
import os
import socket
import struct
import sys
import time


def main():
    mon, addr, n, out = (
        sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3]), sys.argv[4],
    )
    if os.path.exists(out):
        os.unlink(out)
    s = socket.socket(socket.AF_UNIX)
    s.connect(mon)
    time.sleep(0.3)
    s.recv(65536)
    s.sendall(f'pmemsave {addr:#x} {n} "{out}"\n'.encode())
    time.sleep(0.8)
    s.close()
    if not os.path.exists(out):
        print("  pmemsave produced nothing")
        return
    d = open(out, "rb").read()
    words = struct.unpack("<" + "I" * (len(d) // 4), d[: len(d) // 4 * 4])
    print(f"  {addr:#x}:", " ".join(f"{w:#010x}" for w in words))


if __name__ == "__main__":
    main()
