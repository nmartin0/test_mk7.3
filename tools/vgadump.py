#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nicholas Martin
"""Dump the VGA text console from a running QEMU.

Uses the monitor's pmemsave, which reads GUEST PHYSICAL memory -- paging
never enters into it. gdb reads virtual addresses, which is why reading
the console through gdb either faults or returns zeros once the kernel
enables paging.

Two framebuffer addresses are checked, because OSFMK uses both:

  0xb8000   colour text mode, the usual one
  0xa0000   the graphics window

kd_xga_init probes the adapter during cninit() and the console can end
up writing to 0xa0000. A reader hardcoded to 0xb8000 then reports a
blank screen for a kernel that is printing perfectly well -- which
happened here, and looked convincingly like a regression. Let the tool
pick, and never conclude "blank" from a single address.

  usage: vgadump.py <monitor-socket> <scratch-file> [delay-seconds]
"""
import os
import socket
import sys
import time

FRAMEBUFFERS = (0xB8000, 0xA0000)
COLS, ROWS = 80, 25


def grab(mon, out, addr):
    """pmemsave one 80x25 text page from guest physical addr."""
    if os.path.exists(out):
        os.unlink(out)
    s = socket.socket(socket.AF_UNIX)
    s.connect(mon)
    time.sleep(0.5)
    s.recv(65536)
    s.sendall(f'pmemsave {addr:#x} {COLS * ROWS * 2} "{out}"\n'.encode())
    time.sleep(1.5)
    s.close()
    return open(out, "rb").read() if os.path.exists(out) else b""


def decode(buf):
    """Text page -> list of rstripped lines; cells are (char, attribute)."""
    lines = []
    for r in range(ROWS):
        line = "".join(
            chr(buf[(r * COLS + c) * 2])
            if 32 <= buf[(r * COLS + c) * 2] < 127 else " "
            for c in range(COLS)
        )
        lines.append(line.rstrip())
    return lines


def main():
    mon, out = sys.argv[1], sys.argv[2]
    if len(sys.argv) > 3:
        time.sleep(float(sys.argv[3]))

    best, best_addr, best_score = None, None, 0
    for addr in FRAMEBUFFERS:
        buf = grab(mon, out, addr)
        if len(buf) < COLS * ROWS * 2:
            continue
        lines = decode(buf)
        score = sum(len(l.strip()) for l in lines)
        if score > best_score:
            best, best_addr, best_score = lines, addr, score

    if not best_score:
        print("=== VGA CONSOLE ===")
        print("  (blank at both 0xb8000 and 0xa0000)")
        return

    print(f"=== VGA CONSOLE (framebuffer {best_addr:#x}) ===")
    for line in best:
        if line.strip():
            print("  |" + line)


if __name__ == "__main__":
    main()
