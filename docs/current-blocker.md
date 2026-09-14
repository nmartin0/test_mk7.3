# Current state: the kernel boots and runs user code

**The `splx` panic is fixed.** See the commit
"i386/hardclock.c: stop GCC rewriting the interrupt frame via a sibling
call". The long investigation that produced it has been archived to
`docs/archive/splx-investigation.md`, including every eliminated
hypothesis and every instrument trap found along the way — that archive
is worth reading before debugging anything here, because most of the
traps are still live.

---

## Where the kernel gets to

```
Kernel virtual space from 0x0 to 0x40000000.
Available physical space from 0x100000 to 0x3fe0000
Mach 3.0 VERSION(PMK1.1): ... mach_kernel/PRODUCTION (vm)
vm_page_bootstrap: 14705 free pages
fdc0, fd0, fd1, kd0, com0, vga0 configured
realtime clock configured
battery clock configured
Found text region
Found PT_LOAD region with unknown flags
Found data region
I've found: 2 sections
task loaded:check1
check2
check3
task_resume entry
after task resume
start ext2fs.static:
```

Working: memory sizing, VM (14,705 free pages), IPC bootstrap, task and
thread creation, the scheduler, timer interrupts, device
autoconfiguration across floppy, keyboard, serial and VGA, both clocks,
boot module parsing, task creation and `task_resume`.

**The kernel executes user code at ring 3.** From `-d int`:

```
v=0e  cpl=3  IP=0017:08064f8e  SP=001f:bffffe88  CR2=08064f8e
```

`cpl=3`, user selectors `CS=0x17` / `SS=0x1f`, a user stack at
`0xbffffe88`. The 16 page faults in a 12-second run are **demand paging
for a running user process**, which is correct behaviour, not a fault.

Interrupt activity is healthy: 177 records over 12 seconds, against 1,477
before the fix when timer interrupts were hitting a post-panic halt loop.

## Where it stops, and why that is not a kernel problem

The user task eventually faults on a null dereference:

```
v=0e  e=0006  cpl=3  IP=0017:08054cbf  CR2=00000000
```

This is expected and is **not** a kernel defect. The boot module supplied
in testing is OSF's own `src/bootstrap/bootstrap` binary, passed twice as
a stand-in. The boot script invokes it as `ext2fs.static` with Hurd
server arguments it does not understand, so it dereferences a null
pointer. The kernel loaded it, mapped it, scheduled it and ran it
correctly.

## The next decision is architectural, not a bug

`docs/bootstrap-fork.md` is now the live question rather than a deferred
one. As published this tree boots GNU Hurd: `startup.c:517` calls
`bootstrap_create()`, hardcoded to start `ext2fs.static` and
`exec.static` through GNU Mach's boot-script machinery. OSF's own path,
`bootstrap_create_old()`, is intact but sits behind `#if 0`.

**A — boot Hurd.** Obtain real `ext2fs.static` and `exec.static`.
Matches the code as written, no source change. Makes this a Hurd kernel
and brings a GPL userland into a project aiming at permissive licensing.

**B — revive `bootstrap_create_old()`.** Two lines: change
`startup.c:517` and drop the `#if 0` at `bootstrap.c:1255`. Uses OSF's
own bootstrap task, which the tree already builds —
`obj/at386/bootstrap/bootstrap`, 220,656 bytes of i386 ELF. Measured, not
assumed: it compiles clean under GCC 13 and 14, links with every symbol
resolved, and is properly guarded against a missing module.

Both are viable. B matches the project's stated direction and the pieces
are already in hand.

## Known latent defects, none blocking

Recorded so they are not rediscovered as mysteries. None prevents the
kernel booting today.

| where | defect |
|---|---|
| `i386/spl.h` | `spl_t` is `unsigned char` while the spl assembly returns and stores 32 bits. Fixed for `curr_ipl` itself; the typedef remains inconsistent with `mp_v1_1.c`, which declares `curr_ipl[]` as `int`. |
| `i386/pic.c` + `spl.S`, `interrupt.S` | `master_icw`/`master_ocw` are 2-byte `i386_ioport_t` but read with `movl`. Harmless — only `%dx` reaches the `outb` — but it reads two bytes of whatever follows. |
| `i386/AT386/model_dep.c` | `parse_multiboot` is bounded by `mods_count` now, but the surrounding debug `printf`s are left from OSF's own development and clutter the console. |
| `i386/start.S:249` | `EXT(eintstack:)` has the colon inside the macro argument. Resolves correctly by luck. **Do not "fix" it** — see the archive. |
| interrupt dispatch | `set_spl` is reachable by `call` at `0x159e08`, bypassing the bounds check `splx` performs before falling through into it. No current caller passes a bad value, but nothing enforces that. |

## Instrument warnings that still apply

All of these are in `DEBUGGING.md` and all were learned expensively:

- `pkill -x qemu-system-i386` **never matches** — `comm` truncates to 15
  characters. Use `qemu-system-i38` and verify with `ps`.
- Assert `eip == 0xfff0` at attach. Any run that is already past the
  reset vector is talking to a stale guest and is invalid.
- **Only one breakpoint services at a time.** Set one, measure,
  `delete`, set the next.
- Breakpoint **conditions** and **ignore counts** silently do nothing.
- Hardware **watchpoints work well** — but watch *both* the link address
  and the linear address, or gdb falls back to software watchpoints and
  hangs.
- `-d exec` counts are **not** execution counts; fallthrough and block
  chaining make them undercount badly.
