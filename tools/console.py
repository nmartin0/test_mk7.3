#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nicholas Martin
# SPDX-License-Identifier: MIT
#
# console.py -- read and write the guest's serial console.
#
# WHY THIS EXISTS
#
# boot-ide.sh writes the serial line to a file, which is one-way: you can
# watch NetBSD init print "Enter pathname of shell or RETURN for sh:" and
# you cannot answer it. boot-debug.sh gives the line to -serial mon:stdio,
# which does take keystrokes but needs a real terminal, so it is unusable
# from a script or from an agent session that has no tty.
#
# With `-serial unix:/tmp/serial.sock,server,nowait` QEMU exports the line
# as a socket instead. This attaches to that socket, mirrors everything
# the guest says into a log file, and sends anything written to a FIFO.
# One long-running attach; any number of short sends from elsewhere.
#
#   python3 tools/console.py --attach &          # once, after booting
#   python3 tools/console.py --wait-for 'RETURN for sh:' --send ''
#   python3 tools/console.py --wait-for '# ' --send 'echo hi'
#   tail /tmp/console.log
#
# --wait-for blocks until the guest has printed that text. Under TCG the
# single-user prompt is about five minutes after the boot starts, so a
# bare --send on the next line types into a kernel that is still
# loading; and waiting for what the guest SAYS beats sleeping a guessed
# number of seconds.
#
# WHY A FIFO RATHER THAN A SECOND SOCKET CONNECTION
#
# QEMU's chardev accepts one client at a time. If each send opened its own
# connection, the attach would have to drop and reconnect around it, and
# anything the guest printed in between would be lost -- exactly when
# output matters most. The FIFO keeps one reader on the socket for the
# whole boot.
#
# LINE ENDINGS
#
# A serial terminal sends CARRIAGE RETURN when you press return, and that
# is what a tty line discipline expects. Sending a bare newline is a
# common way to have a shell appear dead while it waits for a line that
# never terminates, so --send appends \r by default; use --lf to override
# and --no-eol to send nothing at all.
import argparse
import errno
import os
import select
import socket
import sys
import time

DEFAULT_SOCK = "/tmp/serial.sock"
DEFAULT_LOG = "/tmp/console.log"
DEFAULT_FIFO = "/tmp/console.in"

# Backslash escapes, so control characters can be sent from a shell
# argument: \r \n \t \0 \\ and \xNN, plus \cX for control-X (\cc is ^C,
# \cd is ^D) which is how you interrupt or end input on a real terminal.
def unescape(text):
    out = bytearray()
    i = 0
    while i < len(text):
        ch = text[i]
        if ch != "\\" or i + 1 >= len(text):
            out.extend(ch.encode("latin-1", "replace"))
            i += 1
            continue
        nxt = text[i + 1]
        simple = {"r": 13, "n": 10, "t": 9, "0": 0, "\\": 92, "e": 27}
        if nxt in simple:
            out.append(simple[nxt])
            i += 2
        elif nxt == "x" and i + 3 < len(text):
            try:
                out.append(int(text[i + 2:i + 4], 16))
                i += 4
            except ValueError:
                out.extend(ch.encode())
                i += 1
        elif nxt == "c" and i + 2 < len(text):
            out.append(ord(text[i + 2].upper()) & 0x1F)
            i += 3
        else:
            out.extend(ch.encode())
            i += 1
    return bytes(out)


def make_fifo(path):
    if not os.path.exists(path):
        os.mkfifo(path, 0o600)
    elif not os.path.exists(path) or not os.path.isfile(path):
        pass


def do_send(args):
    make_fifo(args.input)
    data = unescape(args.text)
    if not args.no_eol:
        data += b"\n" if args.lf else b"\r"
    # O_WRONLY on a FIFO blocks until a reader exists, which would hang
    # forever if no attach is running. Fail with a usable message instead.
    # ENXIO means no reader yet. Wait for one rather than failing at
    # once: an attach started moments earlier is the normal case, not an
    # error, and failing instantly turned a race into a wrong diagnosis.
    deadline = time.time() + args.timeout
    fd = None
    while True:
        try:
            fd = os.open(args.input, os.O_WRONLY | os.O_NONBLOCK)
            break
        except OSError as exc:
            if exc.errno != errno.ENXIO:
                raise
            if time.time() >= deadline:
                sys.stderr.write(
                    "console.py: nothing attached to %s after %gs -- start\n"
                    "  python3 tools/console.py --attach &\n"
                    % (args.input, args.timeout))
                return 1
            time.sleep(0.2)
    with os.fdopen(fd, "wb", buffering=0) as fifo:
        fifo.write(data)
    return 0


def do_attach(args):
    make_fifo(args.input)

    # Open the FIFO BEFORE connecting to the socket. Done the other way
    # round there is a window -- interpreter start plus the connect --
    # in which the FIFO has no reader, and a --send issued in it fails
    # with ENXIO. That is not hypothetical: the documented recipe puts
    # --attach and --send in consecutive lines of a paste, so the send
    # lands inside the window every time.
    fifo = os.open(args.input, os.O_RDWR | os.O_NONBLOCK)

    deadline = time.time() + args.connect_timeout
    sock = None
    while time.time() < deadline:
        try:
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect(args.socket)
            break
        except OSError:
            sock.close()
            sock = None
            time.sleep(0.25)
    if sock is None:
        os.close(fifo)
        sys.stderr.write("console.py: could not connect to %s\n" % args.socket)
        return 1

    # O_RDWR above keeps the FIFO open when the last writer goes away.
    # Read-only would make select() report it readable forever after the
    # first send finished, and the loop would spin.
    log = open(args.log, "ab", buffering=0)

    try:
        while True:
            ready, _, _ = select.select([sock, fifo], [], [], 1.0)
            if sock in ready:
                chunk = sock.recv(4096)
                if not chunk:
                    break              # guest went away
                log.write(chunk)
                if args.echo:
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.buffer.flush()
            if fifo in ready:
                data = os.read(fifo, 4096)
                if data:
                    sock.sendall(data)
    except KeyboardInterrupt:
        pass
    finally:
        log.close()
        os.close(fifo)
        sock.close()
    return 0


def do_wait(args):
    """Block until TEXT appears in the log, so a send can be timed.

    Under TCG the single-user prompt arrives roughly five minutes after
    the boot starts. A recipe that attaches and sends in consecutive
    lines therefore sends into a guest that is still loading the kernel,
    and the operator is left looking at a console that never answered.
    Waiting for the text the guest prints is the honest way to sequence
    it -- sleep with a guessed number is the thing this project keeps
    getting wrong.
    """
    want = unescape(args.wait_for)
    deadline = time.time() + args.timeout
    while time.time() < deadline:
        try:
            with open(args.log, "rb") as log:
                if want in log.read():
                    return 0
        except IOError:
            pass                      # not created until the attach runs
        time.sleep(0.5)
    sys.stderr.write("console.py: %r did not appear in %s within %gs\n"
                     % (args.wait_for, args.log, args.timeout))
    return 1


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--socket", default=DEFAULT_SOCK)
    ap.add_argument("--log", default=DEFAULT_LOG)
    ap.add_argument("--input", default=DEFAULT_FIFO)
    ap.add_argument("--attach", action="store_true",
                    help="mirror the console to --log and send from --input")
    ap.add_argument("--send", dest="text", metavar="TEXT",
                    help="send TEXT to an attached console")
    ap.add_argument("--lf", action="store_true",
                    help="terminate with newline rather than carriage return")
    ap.add_argument("--no-eol", action="store_true",
                    help="send TEXT with no line terminator at all")
    ap.add_argument("--echo", action="store_true",
                    help="with --attach, also copy the console to stdout")
    ap.add_argument("--wait-for", metavar="TEXT",
                    help="block until TEXT appears in --log, then exit")
    ap.add_argument("--timeout", type=float, default=600.0,
                    help="seconds for --wait-for, and for --send to find "
                         "an attached console (default 600)")
    ap.add_argument("--connect-timeout", type=float, default=60.0)
    args = ap.parse_args()

    if args.attach and args.text is not None:
        ap.error("--attach and --send are separate operations")
    if args.attach:
        return do_attach(args)
    # --wait-for before --send, so the two compose in one call:
    #   --wait-for 'RETURN for sh:' --send ''
    if args.wait_for is not None:
        rc = do_wait(args)
        if rc != 0 or args.text is None:
            return rc
    if args.text is not None:
        return do_send(args)
    ap.error("one of --attach, --send or --wait-for is required")


if __name__ == "__main__":
    sys.exit(main())
