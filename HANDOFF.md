# Handoff

Read this first, then `WORKFLOW.md`, then `DEBUGGING.md`. Everything
below is committed and pushed; nothing is in flight.

---

## Where the kernel is

It boots. From a clean clone it builds to a 1,025,800 byte i386 ELF and
runs user code at ring 3 on **two independent bootstrap paths**.

```sh
# default: the GNU Hurd path (bootstrap_create)
qemu-system-i386 -kernel mach_kernel.PRODUCTION -initrd bootstrap,bootstrap \
    -display none -no-reboot -m 64 -monitor unix:/tmp/mon,server,nowait

# OSF's own path (bootstrap_create_old), selected with -o
qemu-system-i386 -kernel mach_kernel.PRODUCTION -append "-o" -initrd bootstrap \
    -display none -no-reboot -m 64 -monitor unix:/tmp/mon,server,nowait

python3 tools/vgadump.py /tmp/mon /tmp/vga.bin 10
```

Working: memory sizing, VM (14,705 free pages), IPC bootstrap, task and
thread creation, the scheduler, timer interrupts, device
autoconfiguration (floppy, keyboard, serial, VGA), both clocks, boot
module parsing, task creation, `task_resume`, and **user-mode execution
at cpl=3 with demand paging**.

| path | page faults | user-mode entries |
|---|---|---|
| default (Hurd) | 16 | yes |
| `-o` (OSF) | 12 | yes |

Both stop with the user task failing, not the kernel. On the default
path that is expected: the boot module used in testing is OSF's own
`bootstrap` binary passed twice as a stand-in for `ext2fs.static`, so it
is invoked with Hurd arguments it does not understand.

## The -o path is not failing — it is idle, correctly

Measured after the handoff was first written, on a clean guest.

Over 20 seconds on the `-o` path:

```
timer interrupts (v=40):  260     scheduler running steadily
page faults (v=0e):        12     all from the initial load, none since
any other vector:           0     no faults, no errors
```

and the last kernel blocks executed are `idle_thread_continue` cycling
through `splvm` and `splx` — the idle loop.

So the bootstrap task **loads, runs at ring 3, demand-pages its 12 pages
and then blocks**, and the scheduler correctly goes idle because nothing
is runnable. That is what OSF's bootstrap task should do when there is
nothing to bootstrap: it is waiting on a Mach RPC for a server that does
not exist.

The 12 user faults are an orderly progression — an instruction fetch at
`0x08063e80`, a stack page at `0xbfffffec`, then code and data pages
through `0x0805`–`0x0806`. Nothing anomalous.

**Consequence: there is no bug to chase on this path.** The next step is
to give the bootstrap task something to do — a server to load — rather
than to debug the kernel. `-o` is now a working reference for what a
successful OSF-path boot looks like.

## Earlier framing of this, kept for the record

On the `-o` path the console stops after ELF section scanning:

```
Found text region / Found data region / I've found: 2 sections
```

and then nothing. The task is created and runs at ring 3 (12 cpl=3
entries), so it is user code failing rather than the kernel.

**One hypothesis was investigated and disproved, so do not repeat it.**
`user_bootstrap_old` consumes `boot_region_desc` and `boot_region_count`
without populating them, which looked like the same disconnection
pattern described below. It is not: `SYS_REBOOT_COMPAT` is defined as
`defined(i386) || defined(i860) || defined(hp_pa)`, which is **true** on
i386, so `do_bootstrap_compat()` runs inside `bootstrap_create_old` and
fills the region table. The producer is wired up. That is also what
prints the "Found text region" messages.

So the next step is to find out what the user task does after
`thread_bootstrap_return()` — this is **userland** debugging, a
different problem from everything in `DEBUGGING.md`, which is about the
kernel.

## The framing that keeps paying off

**OSFMK 7.3 retains the original OSF machinery but has it disconnected
in places.** When this tree was adapted to boot GNU Hurd, original
functions were renamed with an `_old` suffix and put behind `#if 0`,
new Hurd equivalents were written, and the wiring between the old halves
was not always kept consistent. Because the old code never compiled, no
diagnostic ever appeared.

That produced the last two bugs found:

- `bootstrap_create_old()` called `thread_start(..., user_bootstrap)`.
  After the rename that bare name resolved to the **Hurd** loader, not
  its own. Both functions were `#if 0`'d, so it never compiled.
  Un-guarding `bootstrap_create_old` alone therefore started OSF's task
  creation at Hurd's loader, and the result was 792,853 page faults.
  Pointing it at `user_bootstrap_old` reduced that to 12.

**The technique:** diff against
`github.com/nmartin0/osfmk6.1`, where the originals are still live and
wired to each other. This found that bug in about ten minutes after days
of indirect searching.

**6.1 is a reference, not a source to port from.** 7.3 already retains
`user_bootstrap_old`, `copy_bootstrap`, `move_bootstrap`,
`ovbcopy_ints`, `load_info_print` and `build_args_and_stack`. Nothing
needed copying. Its licence is compatible anyway — OSF permissive,
marked "OSF Research Institute MK6.1 (unencumbered) 1/31/1995" — should
something be needed later.

Look for more of the same: functions suffixed `_old`, blocks behind
`#if 0`, and old callers referring to names that a rename has since
repointed.

## Superseded sections removed

Everything below the userland survey has been folded into
`docs/current-blocker.md`, which is current. The claims that the floppy
was "premature" and that the task "blocks before any I/O" were both
true when written and are both **false now**: the task runs, initialises
its console, and prints its own diagnostics.

See `docs/current-blocker.md` for the live state and
`docs/lites-survey.md` for the userland assessment.

## Learning to debug an operating system

`docs/METHODOLOGY.md` is a general guide to debugging and developing
kernels -- the instruments and what class of question each answers, the
method, how to read unfamiliar code, how to research, and how to form a
hunch when stuck. The principles apply to any kernel; the worked
examples are drawn from this project because real ones beat invented
ones.

Chapter 8 is the short version, worth pinning somewhere visible.
Chapter 7 is five real investigations with the wrong turns kept in.

`docs/SHELL.md` is its companion: the Unix tools and shell usage that
the debugging work is built on -- grep, sed, awk, pipelines, quoting,
job control, inspecting binaries -- and, at length, the ways a pipeline
can quietly answer a different question than the one you asked. Chapter
7 catalogues the ones that produced wrong conclusions here; chapter 9 is
twenty lines.

## State: it boots multi-user to a login prompt

OSFMK 7.3 boots, LITES mounts an ext2 root read-write, NetBSD 1.0's
init runs `/etc/rc`, spawns getty, and login gives a csh session. The
transcript is at the head of `docs/current-blocker.md`.

```sh
CONSOLE=socket STARTUP_ARGS='-i /init' sh tools/boot-ide.sh
python3 tools/console.py --attach &
python3 tools/console.py --wait-for 'login:' --send 'root'
```

`ROADMAP.md` has what is left. None of it blocks the system running.

## Superseded: a shell runs commands

`/bin/sh` from NetBSD 1.0 executes commands typed at the console under
LITES on OSFMK 7.3. `echo`, `pwd`, `ls`, `date` all work; the
transcript is at the head of `docs/current-blocker.md`.

Reaching it needed three things, all committed: the pid-2 hack in
`wait4()` conditioned on mach_init actually being the first program,
`/mach_servers` populated automatically by `boot-ide.sh`, and
`tools/console.py`, which turns the serial line into a socket that can
be answered instead of a file that can only be read.

```sh
CONSOLE=socket STARTUP_ARGS='-s -i /init' sh tools/boot-ide.sh
python3 tools/console.py --attach &
python3 tools/console.py --wait-for 'RETURN for sh:' --send ''
python3 tools/console.py --wait-for '# ' --send 'echo hi'
```

`--wait-for` blocks until the guest has printed that text, which under
TCG is about five minutes after the boot starts.

The next work is in `ROADMAP.md`, and the shell itself named it: the
root is mounted read-only, so nothing can be written anywhere.

## Superseded: state as of session 6

The stack boots end to end. LITES mounts the ext2 root, execs NetBSD
1.0's unmodified 1994 `/sbin/init`, init opens and acquires
`/dev/console`, forks a shell, and `/bin/sh` runs far enough to try
`/etc/rc`. Zero panics.

What stops it is **one hard-coded pid**. `wait4()` in
`server/kern/kern_exit.c` carries a block whose own comment calls it a
"major hack for BSD init compatibility": if the caller is pid 1 and the
child is pid 2, it hides that child, because under LITES pid 2 is
`mach_init`. We boot `-i /init`, skipping `mach_init`, so init is pid 1
and its first shell takes pid 2 -- and is hidden from its own parent.
`wait()` returns ECHILD, init forks again, and the second child is
refused the console *correctly* because the first still holds it.

Controlled boot with the hack skipped: no ECHILD, no console refusal,
one prompt instead of a repeating cycle, and init sits at
`Enter pathname of shell or RETURN for sh:` with QEMU alive. The full
evidence, and the three ways forward, are at the head of
`docs/current-blocker.md`. **The choice among them is open and belongs
to the maintainer** -- nothing in the tree is changed for it.

Two defects that were blocking any rebuild are fixed and pushed:
`libmach_sa` could not link at all, so the tree did not build LITES from
clean; and `mkroot-netbsd.sh` produced roots with an empty `/dev`.
`boot-ide.sh` now also populates `/mach_servers`, which used to be
hand-typed.

`ENVIRONMENT.md`'s quick start now lists the library builds it omitted,
names the three external trees with their URLs, and gives the sequence
from a clean clone to NetBSD init. Reading it first is the difference
between an hour and a day.

## Where the project is going

`ROADMAP.md` holds the shape: what is done, what is next, the
correctness work that replaces workarounds with the right thing, and
what each reference tree is for. `docs/current-blocker.md` holds the
live detail.
