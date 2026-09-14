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


---

# ROOT CAUSE: the MIG dispatch table is never populated

No kernel RPC has ever dispatched in this tree. Everything the bootstrap
task does fails at its first Mach call, and every symptom chased since
is downstream of this.

## The chain, measured end to end

```
mig_buckets empty
  -> every ipc_kobject_server hash lookup misses
  -> the server returns MIG_BAD_ID (-303)
  -> host_page_size() fails; mach_init discards the error with (void)
  -> vm_page_size stays 0
  -> probe_stack computes ~(0-1) == 0, so every size is 0
  -> cthread_stack_size = 0
  -> alloc_stack chains its free list from base 0 and writes to *0
```

The last step is the fault the investigation started from:

```
at alloc_stack+255:  ebx = 0x0  eax = 0x0   CR2 = 0x00000000
vm_page_size       = 0
cthread_stack_size = 0
```

and the RPC failure is directly observable at the call site:

```
at 'call host_page_size':  host_port (eax) = 0x203
after it returns:          eax = 0xfffffed1  (-303, MIG_BAD_ID)
                           vm_page_size = 0
```

`MIG_BAD_ID` is a **server**-side error -- the kernel could not find a
routine for the message id.

## Why the table is empty

`mig_init()` in `kern/ipc_kobject.c:245` builds `mig_buckets` from
`mig_e[]`. Its loop is:

```c
for (j = 0; j < range; j++) {
    if (mig_e[i]->routine[j].stub_routine) {     /* the gate */
        nentry = j + mig_e[i]->start;
        ...insert...
    }
}
```

In the compiled image the insert path at `0x113e1a` writes `.num`,
`.routine` and `.size` correctly at stride 12, and then falls straight
into `add $0x1,%ebp` / `cmp $0xa` -- the **outer** subsystem counter.
The slot it reads for `.size` is `0x28(%eax)`, which is
`routine[0].max_reply_msg`. Only `routine[0]` of each subsystem is ever
examined.

And `routine[0]` is a null placeholder in every MIG-generated table.
From `mach/bootstrap_server.c`:

```c
{
        {0, 0, 0, 0, 0, 0},      /* routine[0] */
        {0, 0, 0, 0, 0, 0},      /* routine[1] */
  { (mig_impl_routine_t) do_bootstrap_ports, ... },   /* routine[2] */
```

So the gate fails on the only entry considered, nothing is inserted, and
the table stays empty.

## What is measured and what is not

Measured, with the instrument validated each time:

| fact | evidence |
|---|---|
| `mig_init()` is reached | breakpoint at `0xc0113d00` fires |
| `mig_buckets` is empty **at mig_init's exit** | read at `0xc0113e63`; so nothing clears it afterwards |
| the memory read is trustworthy | validated against `intpri`, which reads `08 06 00 00`, matching the boot log exactly |
| outer loop is correct | `n = 10`, `start` at `+4`, `end` at `+8`, matching the generated structs |
| hash arithmetic is correct | `MIG_HASH` is identity, `% 1024`, stride 12, linear probe |
| insert code is correct | writes all three fields at the right offsets |
| `ipc_bootstrap()` calls `mig_init()` | `ipc/ipc_init.c:233` |

**Not established:** *why* the compiled loop only examines `routine[0]`.
The source says `for (j = 0; j < range; j++)`. Either `range` is 1 at
runtime for every subsystem, or the generated struct layout disagrees
with `struct routine_descriptor` in `mach/rpc.h` so the kernel walks the
array with the wrong stride. Reading `range` inside the loop at runtime,
and comparing `sizeof(struct routine_descriptor)` against the stride the
compiled code uses, settles it.

## Corrections made during this investigation

Both were caught before being committed, and are recorded so the
reasoning is not repeated.

- **"`mig_init()` has no caller."** Wrong. It is called from
  `ipc/ipc_init.c:233`. The grep that produced that claim covered
  `kern/` and `i386/` but not `ipc/`.
- **"`mig_buckets[596]` is empty" read against a stale address.** The
  first validation used `intpri` at `0x1d3920`, its address in an
  earlier build; adding code shifted it to `0x1d4920`. Re-validated at
  the correct address before the finding was accepted.

## Hypotheses eliminated

All by measurement, none to be retried:

- the bootstrap port has no server -- `ipc_port_alloc_kernel()` is
  `ipc_port_alloc_special(ipc_space_kernel)`, so it is kernel-serviced
- dispatch wiring is missing -- `do_bootstrap_subsystem` is in `mig_e[]`
- the message id range is wrong -- `999999 <= 1000002 < 1000005`, and
  `2600 <= 2644 < 2711`
- the routine tables are wrong -- `routine[3]` is
  `do_bootstrap_arguments`, `routine[44]` is `host_page_size`
- the MIG user stub retries -- it does not; single send, returns on error
- something clears `mig_buckets` after init -- it is already empty when
  `mig_init` returns


---

## Retraction: routine[1] is NOT why the table is empty

The previous section left "why the table is empty" explicitly open. Two
lines of reasoning were then pursued in conversation and both are wrong.
They are recorded here so neither is repeated.

### Wrong: a struct layout mismatch

`sizeof(struct routine_descriptor)` compiled with the kernel's own flags
is **24**, and `routine[]` begins at offset 20 in `struct
rpc_subsystem`, so `routine[0].stub_routine` is at offset 24 -- exactly
the `0x18(%eax)` the compiled `mig_init` reads. The offsets agree. There
is no layout disagreement between `mach/rpc.h` and the MIG-generated
tables.

### Wrong: the `routine[1]` flexible-array idiom

`mach/rpc.h:228` declares

```c
	struct routine_descriptor	/* Array of routine descriptors */
			routine[1       /* Actually, (start-end+1) */
				 ];
```

the pre-C99 flexible-array idiom, and the theory was that modern GCC
takes `routine[1]` at its word, proves `j < 1`, and collapses the inner
loop to a single `j == 0` test -- which would explain everything, since
`routine[0]` is a null placeholder in every MIG-generated table.

**Disproved by direct test.** A reduced case with the same shape --
`struct rd routine[1]`, an outer loop over a table of pointers, an inner
`for (j = 0; j < end - start; j++)` -- compiled with this build's flags
(`-m32 -O2 -std=gnu89 -fno-pic`) produces a *proper* inner loop with a
24 byte stride:

```asm
	addl	$24, %eax
	cmpl	$1, (%eax)
	addl	$24, %eax
	cmpl	%edx, %ebx
	jne	.L4
```

So `routine[1]` does not cause the collapse.

### What that implies about the disassembly reading

In the real `mig_init`, `113d5b` sets `%esi` to 1 immediately after the
`j == 0` path. That is consistent with GCC having **peeled the first
iteration** rather than collapsing the loop. If so, the `je 113d10` that
was read as "null stub_routine abandons the whole subsystem" is only the
peeled-iteration path, and the real loop continues elsewhere in the
function.

That reading was made by interpreting a branch target without following
the other paths -- the same error that has recurred throughout this
project. Treat the earlier claim that "only `routine[0]` is ever
examined" as unproven.

## What still stands

Measured, with the instrument validated each time, and unaffected by the
above:

- `mig_buckets` is empty **at `mig_init`'s exit**, so nothing clears it
  afterwards
- every kobject RPC returns `MIG_BAD_ID` (-303)
- `vm_page_size` stays 0, `cthread_stack_size` stays 0, and
  `alloc_stack+255` writes to address 0 with `ebx = eax = 0`
- `mig_init()` is reached; `ipc_bootstrap()` calls it
- the outer loop, hash arithmetic, insert code and all struct offsets
  are correct

## How to settle it

Do not read more disassembly. Instrument the loop directly: break inside
`mig_init` and read `range` and `mig_e[i]->routine[j].stub_routine` for
the first subsystem, for `j` beyond 0. That distinguishes "the loop does
not run" from "the loop runs and every `stub_routine` reads as null",
which are different bugs with different fixes.
