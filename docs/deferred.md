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

**Settled by:** *Design of the OSF/1 Operating System*, which
describes OSF/1's BSD layer as parallel throughout: UFS "is fully
parallel", its in-memory inodes and inode hash chains "protected with
simple locks" (§11.4.2.1), and the networking code's locks "always
taken with the interrupt level raised" (§12.6.1).

So it is OSF/1's own design, as OSF documented it.

### ELF, i386, no `exec_with_loader`

**Settled by:** LoVerso's taxonomy in `tclLoadOSF.c`, preserved in
Apple's open source. OSF/1 1.3 and later from OSF RI — which includes
MK6 and MK7 — use ELF, rtld and dlopen. The ECOFF `/sbin/loader` path
belongs to OSF/1 1.0–1.2, to HP's OSF/1 1.0, and to DEC's Alpha line,
which he lists separately.

i386 is the platform OSF's MK7.3 release builds and boots (MK7.3
Release Notes, AT386).

### Drivers stay in the kernel

**Settled by:** OSFMK's own device interface. The drivers are in the
kernel (`mach_kernel/i386/AT386/`), and a server reaches them through
the 12-routine synchronous interface in `device/device.defs`,
subsystem 2800, which Lites already uses. The async request/reply
pairs and the trap fast path at −94 to −99 are a later optimisation
Lites does not use.

## 2. Deferred

### The bootstrap fork: Hurd or OSF

**Decided, 12 October 2026: OSF.** The kernel starts OSF's bootstrap
task again, through OSF's `bootstrap_create()` and `user_bootstrap()`
under their own names; the graft's functions stay in
`kern/bootstrap.c` behind `#if 0 /* GNU Hurd bootstrap graft */`, and
`conf/files` no longer builds GNU Mach's `kern/boot_script.c`. Taken
in the verdict walk as rows 22 and 23, `dev`'s `c1620a0` and
`856290c`, reworked; row 24, `8a15d03`, declined.
`docs/provenance/verdicts.md` has the three shapes considered, why
this one, and why the third commit is not needed. The deferral below
said to wait for the server to link. That was too strict: the bootstrap task, and
the `name_server` and `default_pager` it starts, are OSF's own and
need nothing from Lites.

The section is kept as it was written, because it is the evidence.

As published, this tree does not boot the way Mach does. Verified
here, in our own copy:

| | |
|---|---|
| `kern/startup.c:517` | calls `bootstrap_create()` |
| `kern/bootstrap.c:1146` | `bootstrap_create()`, which hardcodes two **GNU Hurd** servers |
| `kern/bootstrap.c:1238` | `boot_script_parse_line(boot_start, boot_size, "ext2fs.static ...")` |
| `kern/bootstrap.c:1239` | `boot_script_parse_line(exec_start, exec_size, "exec.static ...")` |
| `kern/boot_script.c:3` | "Written by Shantanu Goel" — GNU Mach's boot script parser |
| `kern/bootstrap.c:1257` | `bootstrap_create_old()`, the classic Mach path, inside `#if 0` |

Someone grafted Hurd's bootstrap protocol onto OSFMK before this tree
was published. `bootstrap_create_old()` is OSF's own path — allocate a
bootstrap port, create a task and thread, set `TASK_BOOTSTRAP_PORT`,
start the thread at `user_bootstrap` — and it is disabled and
uncalled.

**This explains the wall.** The boot reaches `start ext2fs.static:`
and stops, because the kernel is asking for a Hurd ext2 translator we
do not have and do not want. No amount of defect-fixing moves it
further; it needs a different second task, which is Tier 0's "one
build drives all three".

**Deferred because** the choice is not ours to make yet. Switching to
`bootstrap_create_old()` means having something for it to load: the
build driver, which now exists, and a server it builds, which does not
yet. `nmartin0/test_mk7.3` `dev` commits `c1620a0` and `856290c` put
the old path behind a flag; they are ahead of us in the walk and
should be read when the server links, not before.

**It also qualifies a claim this branch has already made.** `c964add`
corrects `parse_multiboot`'s unbounded module reads and files them in
`docs/provenance/imports.md` as kind A, "upstream's code corrected to
upstream's own intent". The fix is right and the citation to
`i386/multiboot.h` holds: reading past `mods_count` is unsafe however
the array came to be read that way. But the *intent* is not OSF's.
Reading exactly two modules is the Hurd graft assuming its two
servers will always be supplied. The code corrected is the grafter's,
not OSF's, and `imports.md` now says so.

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

### Lites' ODE build (option A)

**Not chosen for now, and not refused.** Lites is built under ODE by
one Makefile that runs its own `configure` build
(`docs/provenance/precedent.md`, "How Lites meets ODE"). The
alternative was to revive the CMU ODE build Lites also carries, which
is the closer likeness to OSF's `src/osf1_server/conf/AT386/template.mk`.

**Why it was not taken first:** it is the most new code inside Lites,
it re-creates a build Lites' authors had left before they ran it on
OSFMK, and B reaches a running system sooner. The maintainer, choosing
B on 11 October 2026: "we might revisit A in the future".

**What it would take**, measured rather than guessed:

- `server/conf/MASTER` and `i386/MASTER`: OSFMK's `config` rejects
  `conftype "i386"`; `machine "i386"` and `cpu "i386"` are what it
  takes, and with that change alone it writes a 605-line ODE Makefile
  with 150 objects and 71 option headers.
- `server/conf/files` and `MASTER`: forward-port `osfmach3` and its
  `makeoptions`, and the 22 sources only the configure build has
  (ext2fs 10, msdosfs 6, minixfs 3, `serv/cprocs.c`,
  `serv/synch_prim.c`, `conf/param.c`); reconcile the two it has that
  the configure build does not (`../conf/param.c`,
  `serv/notify_interface.c`).
- ODE Makefiles for `include/`, `liblites/`, `emulator/`, `bin/` and
  the top, none of which has one.
- `gensym.awk` and Lites' MIG rules as ODE rules; OSFMK's `osf.osc.mk`
  already has MIG rules.

**What would make it worth revisiting:** wanting the server to build
the way OSF's did down to the configuration file — for instance when
Tier 1 item 1 builds two flavours, user and collocated, from one
`conf/`, as OSF's `SERVER_CONFIG` did.

### The toolchain: era compilers, and GCC with Mach patches

**Recorded now, at the maintainer's request, for later.** The build
uses the host's GCC 13.3 with flags restoring the older behaviour
the code assumes (`CARGS` in `docs/tools/mksandbox.sh`). The
maintainer notes that several GCC 4 releases carried patches for Mach
targets, and that the canonical patched GCC and binutils for OSFMK
should eventually be found and used.

What this build already shows about compiler era, from the first
`build -here lites`:

- `conf/gensym.awk` writes a string literal across lines, which the
  preprocessor stopped accepting in **GCC 3.3**: "The preprocessor no
  longer accepts multi-line string literals. They were deprecated in
  3.0, 3.1, and 3.2." (gcc.gnu.org/gcc-3.3/changes.html). A GCC 4
  would reject it too.
- `serv/user_copy.c:218`, "lvalue required as increment operand", is
  a cast used as an lvalue. That GNU extension was removed in the GCC
  4 series; the release that removed it has not yet been checked
  against GCC's notes.
- `kern/kern_sig.c`, five "pointers are not permitted as case values":
  when GCC began rejecting these is not yet established.

So a GCC 4 is not by itself a cure for what Lites' 1995 sources
assume, and the toolchain question is separate from the source fixes:
which compiler OSF used (the `nmk18b9` sandbox's
`link/tools/at386/elf/gcc` in Lites' `conf/Makerules` is one name to
start from), which Mach-patched GCCs exist and what their patches do,
and whether any of it is APSL or GPL — GCC is GPL as a tool, which is
not the same as GPL code in this tree, and that distinction needs to
be written down before a toolchain lands here.

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

### Only public documents and permitted trees guide the design

The OSF/1 server's source was never released. No source that was not
released for others to use, and none under the GPL, is read for this
project, not even as a specification. The design comes from OSF's
public documents and from the trees `docs/provenance/precedent.md`
permits. Where those are silent, the
maintainer decides. Write from Patience and the Server Writer's Guide.
