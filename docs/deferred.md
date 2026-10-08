# What was decided, what was deferred, and what the record says to do instead

The Lite2 tree's version of this file is written out of 127 commits.
This one has no commits to read yet, so it records the decisions taken
during design, each with the primary source that settled it, so that
none is reopened from memory.

Where a decision is deferred rather than made, that is said, with what
would settle it.

## 1. Decisions made

### The emulator is removed, not fixed

**Settled by:** Patience, *Redirecting System Calls in Mach 3.0: An
alternative to the emulator*, USENIX Mach III Symposium 1993, pp.
57–74, §2.

OSF kept the emulator through MK 4.1 and removed it for MK 5.0. Their
reasons, in their categories:

- **Security.** The emulator is untrusted code in the application's own
  address space. A program can modify its emulator so an innocuous
  syscall execs a shell, then exec a setuid-root binary, which inherits
  the tampered copy because the emulator is "inherited without its text
  or data being re-initialized".
- **Integrity.** Processes calling `task_terminate` instead of exit, or
  `thread_create` and then making syscalls from unknown threads, or
  `vm_allocate`/`vm_deallocate` behind the server's back.
- **Compatibility and maintenance.** Signal semantics "were not totally
  compatible with the integrated kernel" after almost two years and
  twelve releases, because the emulator switches stacks and the server
  cannot find the real user stack to build a frame. `copyin` only works
  if arranged in advance and `copyout` is deferred to the end of the
  call, so OSF/1 kernel extensions were "not even source compatible".
  Loadable syscall modules cannot work at all, because the vector is
  fixed when `init` loads and inherited task to task.

**What it costs:** fifteen Lites files reference the emulator and need
rework. The system is slower until collocation lands.

### The syscall path is exception-driven, using `catch_exception_raise_state`

**Settled by:** Patience §3–4, and Wells 1994 Fig. 2 for the narrative.

Patience tried plain `catch_exception_raise` and rejected it for three
reasons, none of them obvious from first principles:

- The standard exception message carries send rights to the task and
  thread ports, and "sending port rights in messages is relatively slow
  compared to sending data alone and for system call delivery, speed is
  of the essence".
- A syscall always modifies thread state, so the server would need
  `thread_get_state()` and `thread_set_state()` on every call — "two
  additional Mach system calls being needed for every OSF/1 system
  call".
- Debuggers interpose the exception port, so syscalls cannot share it
  with arithmetic faults.

So: `catch_exception_raise_state`, which carries no ports, sends the
thread state out in the message and takes the modified state back in
the reply, and identifies the thread by the port the message arrived
on. Ports are bound per exception type by
`thread_set_exception_ports(thread, types, port, behavior, flavor)`.

**Recorded because it will be rediscovered otherwise:** anyone
designing this from scratch builds the version Patience discarded.

### `copyin`/`copyout` start on `vm_read_overwrite`

**Settled by:** Patience §5–6.

Both underlying kernel problems were fixed by OSF and both fixes are in
7.3. `vm_write` was changed to accept unaligned addresses and sizes;
`mach_msg_overwrite` was added so the receiver supplies an
already-mapped buffer, giving `vm_read_overwrite` and removing the
`vm_deallocate` from the `copyin` path.

`vm_remap` is the alternative — map the application's memory into the
server and cache the mapping. OSF tried two cache designs, a
system-wide hash with LRU and a per-task linked list with address
hints; both exceeded 90% hit rate and neither won clearly on
performance. `vm_remap` is not location-transparent and the server must
tear down mappings when the application frees memory or it will use
stale ones.

**Start simple and correct.** `vm_remap` is a later optimisation.

### Collocation uses OSF's design, not Utah's

**Settled by:** what is in the tree.

Two designs exist. OSF's (Condict) is short-circuited RPC via
`mach_subsystem_create`, and its i386 kernel side is already written in
`i386_rpc.c`. Utah's INKS (Lepreau et al.) loads the server into the
kernel address space and uses an extended stub generator, KMIG, turning
client stubs into traps numbered by `msgh_id`.

Utah's paper is the better engineering document and should be read.
OSF's code is the one to call.

**Expected gain:** about 13% on a realistic workload. Utah measured a
full kernel build at 1,682s against 1,463s, Andrew 4%, SPEC SDM SDET
about 8% at low load and under 1% at high.

### The locking converts to Mach's primitives

**Settled by:** measurement of OSF/1's own BSD layer — 148
`simple_lock` calls across 25 files, `kern/parallel.h` included by 19
BSD files, 10 Mach files, and files in vm, ufs and net — and by the
finding that this is Encore's Multimax SMP work, contributed into
OSF/1 proper rather than added by a vendor. Multimax was one of OSF's
three reference platforms from the 1.0 release in 1990.

So it is canonical OSF/1 design, not a DEC or Intel accretion.

### ELF, i386, no `exec_with_loader`

**Settled by:** LoVerso's taxonomy in `tclLoadOSF.c`, preserved in
Apple's open source. OSF/1 1.3 and later from OSF RI — which includes
MK6 and MK7 — use ELF, rtld and dlopen. The ECOFF `/sbin/loader` path
belongs to OSF/1 1.0–1.2, to HP's OSF/1 1.0, and to DEC's Alpha line,
which he lists separately.

i386 was one of OSF's three reference platforms from 1990, alongside
the DECstation 3100 and the Encore Multimax.

### Drivers stay in the kernel

**Settled by:** measurement of MkLinux's server-side `blk_dev/`,
`chr_dev/` and `net_dev/`: zero `inb`/`outb`, zero `request_irq`, and
165 calls to the Mach device interface — `device_get_status` 51,
`device_set_status` 42, `device_close` 24, `device_open` 21,
`device_write` 17, `device_read` 10.

The 12-routine synchronous interface in `device/device.defs`,
subsystem 2800, is what OSF's own server used. The async request/reply
pairs and the trap fast path at −94 to −99 are a later optimisation
neither OSF's server nor Lites used.

## 2. Deferred

### The two latent `i386_rpc.c` defects

Block 607 writes to an operand declared `"g"` input; blocks 195, 411,
464, 521 and 567 each do `addl $4, %N` on an operand declared `"r"`
input.  Both are the same class as the defect that blocked the build.

**Deferred because** neither blocks anything and both are in collocated
RPC paths that cannot run until a server is collocated — Tier 1 item 4,
where this file becomes central.  Changing untested assembly to close a
latent hazard is how a latent hazard becomes a live one.

**What would settle it:** a collocated server that exercises these
paths, so a fix can be tested rather than reasoned about.

### `THREAD_STATE_SYSCALL`

Patience §4 adds a thread-state flavour carrying only the registers a
syscall needs — five on i386 against seventeen for the full set — so
the exception message stays small. It is the one Patience mechanism
that did **not** ship in OSFMK 7.3.

**Deferred because** it is a performance optimisation and not a
correctness requirement. Build the path on `i386_THREAD_STATE` first,
get it correct, then shrink the message.

**What would settle revisiting it:** a measurement showing the message
size on the syscall path is material. Until then it is speculative work
and `RULES.md` 3.3 forbids it.

### The personality's base: Lite2 or Lites

The three trees run and the personality could come from either `bsd/`
or from Lites' already-serverised copy.

**Current position:** `bsd/` is the base and Lites is a worked
reference for the roughly 21% that needed serverising — the buffer
cache, init, syscall entry, and the inode-to-memory-object plumbing.
Lites' serverisation is built on its private `synch.h` and its
emulator, both of which this project discards, so inheriting its
approach in those exact files imports the design being replaced.

**Not settled, because** the similarity figures that support it were
measured against stock Lite2, and `bsd/` is `dev3`, which the
maintainer reports is more functional and fixes many bugs. If those
fixes land in `vfs_bio.c`, `init_main.c` or `uipc_syscalls.c` — the
files Lites rewrote most — this becomes a merge rather than a choice.

**What would settle it:** the same similarity measurement, run against
`bsd/` instead of stock, per file.

### Whether the server registers with `name_server`

`bootstrap.conf`'s default list is `name_server`, `default_pager`,
`startup`. Nothing has established whether our server needs to register
with the first of those, or what currently runs on the maintainer's
booting system.

**What would settle it:** reading the running system's
`bootstrap.conf`, and `mach_services/servers/netname/netname.c`, which
is a complete worked OSF server and the obvious place the answer lives.

### The server-to-kernel path

Utah measured 36.8% of all communication going from the server to the
kernel, and concluded that "optimization of the server-kernel path
would be worthwhile". They did not do it.

**Deferred to Tier 2.** Recorded so it is not rediscovered as a
surprise when the collocation numbers come in lower than hoped.

## 2a. Tooling defects, to fix in a later patch

Found while auditing `nmartin0/test_mk7.3` `dev`'s build scripts.  The
first three are fixed in our copy at `docs/tools/bootstrap-ode.sh`;
they are recorded because that tree still has them and because the
reasoning matters.

**`build_md()` swallowed every compiler error.**  `2>/dev/null || true`
in both loops.  The exclusion list is explicit, so a file that stops
compiling should fail the build rather than produce a quietly smaller
`libode.a` that only fails at the link.  Fixed: both loops `exit 1`.

**The `ar` was unchecked.**  Combined with the above, a near-empty
archive was possible.  Fixed.

**The `DEF_ARFLAGS` comment buried its conclusion.**  It matters only
for ODE's self-build: OSFMK's `Buildconf` line 140 already sets `cr`.
Fixed: the comment now leads with that.

**`dev`'s `.gitignore` has five prose lines with no leading `#`.**  Git
treats any non-blank line not starting with `#` as a pattern, so all
five are live ignore rules:

	$ git check-ignore -v "empty."
	.gitignore:5:empty.	empty.

The failure mode is the bad kind — a file that should be tracked is
silently skipped and `git status` says nothing.  Not fixed here because
this tree has no `.gitignore` yet; recorded so ours is written with
`#` from the start.

## 3. Refused

### MkLinux's `osfmach3/` is not used

It is OSF's own evolution of the same CMU skeleton Lites forked, seven
years later, and it already does exception-based syscall delivery:
`gen_trap.c` registers for `EXC_MASK_SYSCALL` via
`task_set_exception_ports`, and `parent_osf1.c` exists for running
under a live OSF/1 server.

It is GPL by position inside `mklinux/src/`, which `COPYING` governs,
and its files carry a bare `Copyright (c) 1991-1998 Open Software
Foundation, Inc.` with an empty comment block and no grant. The
identical OSF vintage carries the full MIT/X11 grant where it sits
under `osfmk/`, which suggests the omission is about placement rather
than intent — but intent is not a licence.

**Read it. Copy nothing.** Write from Patience and the Server Writer's
Guide instead.

### The three encumbered trees are specification only

The DEC OSF/1 tree, the Paragon OSF/1 AD server on cd155, and AdvFS.

Structural facts from them — function inventories, file lists, syscall
numbers, structure layouts — are facts and may be recorded. Their code
is not a donor.

The cd155 release letter is the sharpest statement of why: the software
is subject to source licence agreements with Intel, with the Open
Software Foundation, **and** with Unix Systems Laboratory. Three
licences, which is why the OSF/1 server never escaped licensee-only
distribution.
