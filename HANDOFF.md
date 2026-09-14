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

## The immediate next question

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

## The architectural decision, still open

`docs/bootstrap-fork.md`. Both paths now work to the same depth, so the
choice is informed rather than speculative.

**A — boot GNU Hurd.** `bootstrap_create()` is hardcoded to start
`ext2fs.static` and `exec.static`. Two blockers: the boot script mounts
`hd2s2`, an **IDE partition**, and this configuration has no IDE driver
— which is why `ivect[14]`, the primary IDE channel, is `intnull`. And
Hurd servers are built against **GNU Mach's** interfaces, which have
diverged from OSFMK 7.3; a real `ext2fs.static` may not speak this
kernel's RPC dialect at all. **Test that cheaply before investing.**

**B — OSF's multiserver.** Now revived and running at ring 3. Keeps the
project permissively licensed.

Using Hurd as a *build host* for permissive code is clean — GPL governs
distribution of the GPL work, not what you compile with it.

## Known latent defects, none blocking

| where | defect |
|---|---|
| `i386/spl.h` | `spl_t` is `unsigned char` while the spl assembly returns 32 bits. Harmless while IPLs stay in 0..8. |
| `i386/pic.c` + `spl.S`, `interrupt.S` | `master_icw`/`master_ocw` are 2-byte but read with `movl`. Harmless — only `%dx` reaches the `outb`. |
| `i386/AT386/model_dep.c` | OSF's own debug `printf`s in `parse_multiboot` clutter the console. |
| `i386/start.S:249` | `EXT(eintstack:)` — colon inside the macro argument. Resolves correctly by luck. **Do not "fix" it.** |
| interrupt dispatch | `set_spl` is reachable by `call` at `0x159e08`, bypassing the bounds check `splx` does before falling through into it. |

## Instrument warnings — read these before measuring anything

All were learned expensively and all are still live. Full detail in
`DEBUGGING.md`.

- **`pkill -x qemu-system-i386` never matches.** `comm` truncates to 15
  characters; the process is `qemu-system-i38`. A stale QEMU holds port
  1234 and gdb silently attaches to the **old, already-failed guest**.
  This invalidated a whole round of measurements.
- **Assert `eip == 0xfff0` at attach.** Any run not at the reset vector
  is talking to a stale guest and is invalid.
- **Only one breakpoint services at a time.** Set one, measure,
  `delete`, set the next. Two breakpoints silently service only one,
  which reads as "the code between them is unreachable" and produced a
  confident wrong conclusion.
- **Breakpoint conditions and ignore counts silently do nothing.**
- **Hardware watchpoints work well** — but watch *both* the link address
  and the linear address, or gdb falls back to software watchpoints and
  `continue` hangs forever.
- **`-d exec` counts are not execution counts.** Fallthrough and block
  chaining make them undercount by orders of magnitude. Use them for
  *whether* code ran and the *order* of first entry, never for how many
  times.
- **The kernel is relocated by segmentation**, `cs_base = 0xC0000000`.
  Breakpoints need linear addresses; register-derived pointers need
  `+0xC0000000` before the monitor can read them.
- **The console is VGA and may be at `0xa0000`, not `0xb8000`.**
  `tools/vgadump.py` tries both.
- **Check whether the failure is deterministic before reasoning about
  it.** The bug fixed this session reported a different value every
  boot; comparing two measurements from different runs produced an
  impossible conclusion and three rounds of wasted work.

## On method

This session produced about ten corrections, three of them retractions
of already-committed claims. Every one came from trusting an instrument
without validating it first. The findings that stuck came from slowing
down and checking the instrument before the result.

`docs/archive/splx-investigation.md` records the whole hunt including
the retractions. It is worth reading not for the conclusion — which is
fixed — but for the shape of the mistakes.

Two rules that would have prevented most of it:

1. **Validate the instrument on a known-good case before believing a
   result**, especially a negative one.
2. **Enumerate every instance in a class before proposing a fix for one
   of them.** Three patches were withdrawn this session for skipping it.
