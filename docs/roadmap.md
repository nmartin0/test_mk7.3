# Roadmap

Three tiers, in dependency order. Tier 0 is what must be true before
any of the design work can start. Tier 1 is the design: the server
stops being Lites and starts being an OSF/1-shaped single server. Tier
2 is what the architecture is for, and is listed so that nothing in it
is rediscovered later.

Nothing here is a schedule. Items are listed with what is known about
each, and with the donor where one is known. `docs/deferred.md` holds
the reasoning; this file holds the ordering.

## What we are building

An architecturally faithful reconstruction of the OSF/1 Single Server
on OSFMK 7.3, i386, ELF userland, from permissively licensed components
and OSF's own freely reproducible specifications.

OSF RI built their server by starting from CMU's Mach 3.0/BSD single
server and replacing the BSD personality. We start from the same CMU
code — Lites — and follow the design path OSF published. That is the
sense in which this is canonical.

It will not be OSF's code. Their personality was never released. Ours
descends from 4.4BSD-Lite2 rather than 4.3BSD-Reno: Reno covers 56% of
OSF/1's function inventory and Lite2 covers 43%, a close cousin and not
the same code. The name should say reconstruction, not clone.

## Tier 0 — the ground

Measured against the trees as they stand:

| | state |
|---|---|
| OSFMK 7.3 boots | yes, and reaches `start ext2fs.static:` |
| Lites runs on it | yes, via a 1,414-line patch — but that patch lives in `dev`, not here |
| the 4.4BSD-Lite2 personality runs | yes, standalone, in its own repository |
| the three are one repository | **done** — `osfmk/` plus Lites at `osfmk/src/mach_services/servers/startup/` |
| one build drives all three | not yet, and it is now the blocking item |
| the two Utah traps checked for | **done** — trap 1 fires, trap 2 does not on the generic path; `docs/status.md` |

Two of those rows moved since this file was written.

**Lites is in the repository and in the right place.** It was
imported verbatim as `lites/` and then relocated to
`osfmk/src/mach_services/servers/startup/`, beside OSF's own
`machid`, `netname` and `netmemoryserver`, under the name OSF's
`bootstrap.template` uses for the OSF/1 server. The roadmap's
"not yet" understated it.

**The build driver is now the blocking item, not merely the next
one.** The boot stops at `start ext2fs.static:` because
`bootstrap_create()` is hardcoded to start two GNU Hurd servers and
OSF's own `bootstrap_create_old()` is disabled in `#if 0`. No defect
fix moves it further; it needs a second task of our own to load.
`docs/deferred.md` §2 has the evidence.

**Trap 1 fires, and it is a constraint on Tier 1 item 2.** C-threads
finds a thread's identity by masking its stack pointer to a fixed
size, and Lites hangs its per-thread state off that. The kernel must
hand the server a C-threads service stack, or `cthread_self()` and
every `get_proc_invocation()` return garbage. The library already has
an `in_kernel` path forcing a 32 KB stack, which is where item 4
should start.

**The repository.** Three verbatim imports as separate subdirectories,
each as commit one for its tree, so `git diff <import>..HEAD -- <dir>/`
is a complete deviation record.

**The build.** MkLinux's `build_world` is OSF's own driver and gives
the order:

	build MAKEFILE_PASS=FIRST
	build -here mach_services/lib/libcthreads
	build -here mach_services/lib/libsa_mach
	build -here mach_services/lib/libmach
	build -here mach_services/lib/libmach_maxonstack
	build -here file_systems
	build -here bootstrap
	build -here mach_kernel MACH_KERNEL_CONFIG=PRODUCTION
	makeboot

Our server builds after `mach_kernel`, linking against those four
libraries. That answers "where do the Mach libraries live" by OSF's
precedent rather than by preference: they stay in `osfmk7.3/` and the
server links against them.

**The existing Lites adaptation, and its overlap with Tier 1.**
`test_mk7.3`'s `tools/lites/lites-osfmk73.patch` is 1,414 lines across
29 files, and it is what makes Lites run on OSFMK 7.3. It is not a
vendor change in our sense — it was applied at build time against an
external checkout, where we carry Lites as an import.

Six of its 29 files are files Tier 1 rewrites: `server/serv/user_copy.c`,
`serv_syscalls.c`, `server_init.c`, `server_exec.c`, `xmm_interface.c`
and `vn_pager_misc.c`. Adopting its changes there and then rewriting
them is work done twice. The walk in `docs/provenance/verdicts.md`
should reach those six and say so rather than discover it later.

Its other 23 files are the adaptation proper — `conf/`, `include/`,
`emulator/`, `server/kern/`, `server/libkern/` — and are Tier 0 work
that is already done and needs only to be re-expressed as changes to
`lites/`.

**The two Utah traps, checked before anything is designed.** Both are
greps and both change the shape of Tier 1 if they fire.

1. Does Lites allocate per-thread state at a constant offset from a
   fixed-size C-threads stack, as the OSF/1 server did with `uthread`?
   If yes, the kernel must hand the server its own service stack and
   cannot reuse the kernel stack.
2. Do Lites' generated stubs copy arguments that the BSD service
   routines already copy? Utah found every argument moving twice and
   added a stub-generator option to suppress it.

**`bootstrap.conf`.** Three lines by default — `name_server`,
`default_pager`, `startup`. What our running OSFMK currently starts,
and under what name Lites appears there, has not been recorded.
`name_server` is in the default list and nothing here has established
whether the server needs to register with it.

## Tier 1 — the server becomes OSF-shaped

The five decisions, each settled by a primary source, are in
`docs/deferred.md` with the reasoning. The order below is forced by
dependency, not chosen.

### 1. One build, two servers

Build the server twice from one source: the emulator-based one that
works today and the exception-based one being written. OSF shipped
`startup` and `startup.compute` from one tree and that is the
precedent. Without it, removing the emulator is a flag day and a
failure anywhere is indistinguishable from a failure everywhere.

### 2. The syscall path

**The kernel half is already there.** Patience's mechanisms shipped:
`EXC_SYSCALL`, the three exception behaviours,
`catch_exception_raise_state`, `thread_set_exception_ports`,
`mach_msg_overwrite`, `vm_read_overwrite`, `vm_remap`. Measured in
`docs/method.md` §7.

**What is ours to write**: `uxkern/syscall.c`. Register for
`EXC_SYSCALL`, receive the exception, rebuild a hardware trap frame,
dispatch to the unchanged BSD handler, write the registers back in the
reply. Lites has `ux_syscall.c` driven by emulator RPC; OSF had
`syscall.c` *plus* `syscall_subr.c`, and the split is the tell.

**What is ours to add to the kernel**: `THREAD_STATE_SYSCALL`, a
thread-state flavour carrying only the registers a syscall needs —
five on i386 against seventeen. Patience §4. No donor has it.

**Deferred**: the small flavour is a performance optimisation, not a
correctness requirement. Build on `i386_THREAD_STATE` first.

**`copyin`/`copyout`**: start with `vm_read_overwrite`. `vm_remap` with
a mapping cache is the alternative; OSF measured both above 90% hit
rate and neither won clearly.

**Then** cut over one syscall class at a time, and only then delete
`emulator/` (44 files, 18,503 lines) and `liblites/` (4 files, 934).
Fifteen files reference the emulator and need rework; they are listed
in `docs/deferred.md`.

### 3. The locking

Lites has 603 `spl*()` calls resolving to its own `spl_n()` in
`include/sys/synch.h`, plus 106 `mutex_lock` and 78 `condition_*`,
across roughly 96 files. That model is CMU's, inherited from the UX
server, and documented in Golub et al. 1990: "sleep, wakeup, spl
implemented by using the C Threads package's mutex, condition_wait and
condition_signal".

OSF/1 used Mach's. Replace with `simple_lock` and its family from
`osfmk7.3/.../kern/lock.h`, and add a shared concurrency header
spanning both halves, after `kern/parallel.h`.

This is the change that most moves the system from two programs glued
together to one integrated system, and it is mechanical across ~96
files.

### 4. Collocation

Two designs exist and only one is in our tree. Use OSF's:
`mach_subsystem_create` registers a MIG-generated subsystem,
`mach_port_allocate_subsystem` binds each receiving port, and
`i386_rpc.c` already has the kernel side. Read Utah's INKS paper for
the engineering detail — KMIG, the trap-number scheme, the fallbacks —
but the code to call is OSF's.

Preserve the invariant: **the same binary runs collocated or as a user
task.**

Expect about 13% on a realistic workload. Utah measured a full kernel
build at 1,682s against 1,463s, Andrew at 4%, SPEC SDM SDET about 8% at
low load and under 1% at high.

### 5. The ABI, and the boot path

The syscall surface filled from the compat-layer union and the
documentation; then the kernel-loaded boot path via `KERNEL_LOADED` and
`unix_mapbase`/`unix_mapend`.

## Tier 2 — what the architecture is for

Listed so it is not rediscovered. None of it is scheduled.

- **The server-to-kernel path.** Utah measured 36.8% of all
  communication going from server to kernel and concluded its
  optimisation "would be worthwhile". We inherit that.
- **The async device path.** `device_*_async` traps at −94 to −99 with
  `io_done_queue_wait`, unused by OSF's own server and by Lites. Pairs
  naturally with a batched-completion buffer cache, which is what
  MK7-PA's "Dynamic Buffer Cache" appears to have been.
- **Multiple personalities.** The architecture's stated purpose. Once
  the syscall path is exception-driven and per-task, a second
  personality is a second exception port.

## What is not in scope

- **ECOFF and `/sbin/loader`.** OSF/1 1.3 and later from OSF RI —
  MK6, MK7 — use ELF, rtld and dlopen. The loader path belongs to
  OSF/1 1.0–1.2 and to DEC's Alpha line. `exec_with_loader` is out.
- **Alpha.** i386 was one of OSF's three reference platforms from 1990
  and is where the collocated fast path is already written.
- **AD's distributed machinery.** `fsvr*`, `pfs*`, NORMA, HIPPI. Those
  are the AD branch, not the MK single server.

## The risky stretch

Phases 2 through 4, where the system works but is slower than it
started. Patience measured the no-emulator server as roughly
performance-neutral against a mature emulator-based one, and
collocation is what recovers the rest.

**Do not benchmark between them and conclude the design is wrong.**
That is where OSF was.

The reason to do it is not speed. It is that OSF reused the integrated
kernel's own `syscall()` with fifteen lines of preamble, two of
postamble and a rename; reused ufs, vfs, net and netinet unchanged; and
reached their mature server's robustness within weeks of first
multi-user boot where the emulator version had taken most of a year.
Our personality is BSD-Lite2 code we want to touch as little as
possible. That is the same property.
