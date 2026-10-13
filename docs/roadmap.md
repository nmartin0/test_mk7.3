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
descends from 4.4BSD-Lite2, a cousin and not the same code. The name
should say reconstruction, not clone.

## Tier 0 — the ground

Measured against the trees as they stand:

| | state |
|---|---|
| OSFMK 7.3 boots | yes: OSF's bootstrap task reads `bootstrap.conf` from UFS on a vtoc-labelled IDE disk, as OSF's own did, and starts `default_pager`; `name_server` does not build yet |
| Lites runs on it | yes, via a 1,414-line patch — but that patch lives in `dev`, not here |
| the 4.4BSD-Lite2 personality runs | yes, standalone, in its own repository |
| the three are one repository | **done** for two of them — the kernel at `osfmk/`, and Lites at `osfmk/src/lites/`, a component of the kernel's own sandbox as OSF's `osf1_server` was |
| one build drives all three | **in part** — `build -here lites` builds Lites under ODE and stops at Lites' own source, 145 server objects in; not yet in `build_world`. The blocking item is now the source fixes |
| the two Utah traps checked for | **done** — trap 1 fires, trap 2 does not on the generic path; `docs/status.md` |

Two of those rows moved since this file was written.

**Lites is in the repository; its place has now been read rather than
inferred.** OSF's MK6.1 and MK7.3 release notes build the OSF/1 server
as `src/osf1_server`, a peer of `mach_kernel` in one ODE sandbox, with
the same build driver run after `mach_services`. Lites is at
`osfmk/src/lites/` accordingly. Two earlier placements were wrong —
inside `mach_services/servers/` and then at the top level beside
`osfmk/` — and `docs/provenance/precedent.md` records both. Every lineage whose layout is documented —
CMU's `src/{mk,ux}`, OSF's separate OSF/1 tree, OSF RI's MkLinux
DR3 — keeps the personality server beside the
microkernel as a peer, built against its exports. The `startup` name
belongs to the runtime slot in `/mach_servers`, not to a source
directory. `docs/provenance/precedent.md`, "Where the server lives,
and why", has the sources.

**The build driver exists; the source fixes are the blocking item.**
`osfmk/src/lites/makefile` runs Lites' own configure build under ODE
(`docs/provenance/precedent.md`, "How Lites meets ODE"; option A,
reviving Lites' CMU ODE build, is held in `docs/deferred.md` §2). The
server stops on errors in six files, listed in `docs/status.md`; `dev`'s
Lites patch has fixes for all but one, and the walk adopts them as it
reaches them. The boot no longer waits on it: since verdicts 22–24 the
kernel starts OSF's bootstrap task instead of the two GNU Hurd servers
a graft had wired in, and `name_server` and `default_pager` — OSF's
own, needing nothing from Lites — are the next things for it to load.
`docs/deferred.md` §2, "The bootstrap fork".

**Trap 1 fires, and it is a constraint on Tier 1 item 2.** C-threads
finds a thread's identity by masking its stack pointer to a fixed
size, and Lites hangs its per-thread state off that. The kernel must
hand the server a C-threads service stack, or `cthread_self()` and
every `get_proc_invocation()` return garbage. The library already has
an `in_kernel` path forcing a 32 KB stack, which is where item 4
should start.

**The repository.** Three imports as separate subdirectories, verbatim
from the copies they were taken from (not their upstreams' releases:
"Intermediate goal", milestone 0, below), each as commit one for its
tree, so `git diff <import>..HEAD -- <dir>/` is a complete deviation
record.

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
libraries: `build -here lites`, joining this list once it succeeds. That answers "where do the Mach libraries live" by OSF's
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

## Intermediate goal — the whole system, online, hosting itself

**Set 12 October 2026, by the maintainer.** Before Tier 1 changes the
server's design, the system as its creators assembled it runs end to
end: OSF's boot loader, OSF's kernel and bootstrap task, OSF's
`name_server` and `default_pager`, Lites as the server, a BSD userland
in multi-user mode, a network interface, and enough of a toolchain
that the system rebuilds its own kernel, servers and Lites. Each step
is taken the way OSF -- or, for Lites, Helander and Utah -- intended,
and where neither said, that is recorded rather than guessed. The
research behind each milestone is in `docs/provenance/precedent.md`,
"What OSF's running system was", and the decisions are in
`docs/deferred.md` §1, "The intermediate goal's choices".

The walk through `dev`'s commits continues as the way there; dev
reached a multi-user login under Lites, by other means, at its commit
`7c428c2`.

**0. The releases, not the copies.** Neither import is its upstream's
release (`docs/provenance/precedent.md`, "Where the two imports came
from"). Lites is restored from Utah's `lites-1.1.u3.tar.gz` (done, 13
October 2026: the tarball unpacked at `osfmk/src/lites`, where OSF's
`src/osf1_server` stood). For the
kernel the nearest readable copy of MkLinux DR3 is `slp/osfmk-mklinux`'s
first commit, `0f6f7e6`, with its `start.s~`; each of slp's eighteen
later commits gets a verdict, as `dev`'s do. The deviation record keeps
one command per tree, against reference copies of the two releases
kept in the repository. The maintainer's copies of the MkLinux CVS
repository and of the Lites tarball check both. The host tools are
built, not carried: `config` and `migcom` from OSF's 1999 sources in
that CVS repository, by DR3's own procedure (`docs/tools/mkhostbin.sh`),
as milestone 5 will build them on the running system. The target's
assembler is chosen to read OSF's sources as written: GNU as for
i386-elf, which keeps the System V assembler's `/` comments
(`docs/tools/mkbinutils.sh`).

**1. Boot as OSF did.** OSF's primary boot and `mach_boot`
(`stand/AT386/boot`) on the vtoc disk, loading `/mach_kernel` and
`/mach_servers/bootstrap` from partition a, through
`i386/AT386/asm_startup.h`. QEMU boots the disk through its BIOS. The
multiboot entry slp added stays, as a second way in (maintainer,
12 October 2026), and `docs/tools/boot.sh` keeps it as an option. The
`hd.c` changes made for a boot without a BIOS (`dev`'s `052a105`,
slp's `7e81018`) are tested again under a BIOS boot before any is
adopted.

**2. The three servers.** `bootstrap.conf` as OSF's template and
release notes give it: `name_server`, `default_pager` on partition b,
`startup` -- Lites -- with the root, partition a. `name_server` builds.
No `mach_init`: OSF's bootstrap task and `name_server` provide what it
did.

**3. Lites, multi-user.** Lites 1.1.u3 configured `STD+WS+osfmach3`,
run as an ordinary task, not collocated. Root on partition a in the
old UFS format OSF's reader and boot loader read and Lites mounts
read-write. Userland: NetBSD 1.0's i386 binaries, the release Lites'
own documents name -- marked as provisional; this layer is expected to
change most later. `/mach_servers/mach_init` is a symbolic link to
`/sbin/init`, as Lites' `doc/README` allows when a name server is
already running.

**4. Online.** The kernel's own `ne` driver, already in the PRODUCTION
configuration, with QEMU's `ne2k_isa`; its two defects fixed first
(the 3c501 interrupt handler installed for it in `autoconf.c`, and
odd-length copies in `if_ne.c`). Lites' 4.4BSD TCP/IP stack opens
`ne0` by its Mach device name; `ifconfig`, `route`, then telnet and ftp.

**5. Hosting itself.** On the running system, ODE's minimal tools and
a GCC of OSF's own line -- gcc 2.7.x, the compiler MK7.2 and MK7.3
were built with, in its OSF/1 i386 ELF configuration -- rebuild the
kernel, the servers and Lites, and the result boots. The first
compiler is NetBSD 1.0's own, running under Lites, as Lites
self-hosted in 1995. Later GCCs come after this is stable.

The kernel configuration stays PRODUCTION, OSF's documented default;
FAST, with the debugger, is for debugging only.

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

**What is ours to write**: the exception-driven system-call entry,
`syscall.c`. Register for `EXC_SYSCALL`, receive the exception,
rebuild a hardware trap frame, dispatch to the unchanged BSD handler,
write the registers back in the reply (Patience §3–4). Lites has
`ux_syscall.c` driven by emulator RPC.

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

OSF/1 used Mach's (*Design of the OSF/1 Operating System*, §11.4.2.1
and §12.6). Replace with `simple_lock` and its family from
`osfmk7.3/.../kern/lock.h`, and add a shared concurrency header
spanning both halves.

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
  `io_done_queue_wait`, unused by Lites. Pairs
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
- **OSF/1 AD's distributed machinery**, which the AD papers
  describe, and NORMA with it: the AD branch, not the MK single
  server.

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
