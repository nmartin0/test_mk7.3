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
and OSF's own published documents.

OSF RI built their server by starting from CMU's Mach 3.0/BSD single
server and replacing the BSD personality. We start from the same CMU
code — Lites — and follow the design path OSF published. That is the
sense in which this is canonical.

It will not be OSF's code. Their personality was never released. Ours
descends from 4.4BSD-Lite2, a cousin and not the same code. The name
should say reconstruction, not clone.

**How the design is decided.** *Restated 23 October 2026, by the
maintainer.* OSF's public documents are the specification, written up
in two files: `docs/osf1-interface.md`, what cuts across calls -- the
boot, the system-call path, threads and signals, the Mach seam, the
defects OSF documented -- and `docs/osf1-calls.md`, each call as the
OSF/1 1.0 manual gives it, with its flags, defaults and errors. The
baseline is OSF/1 R1.2 as VSX and VSE measured it. Code comes from the
permitted trees, in `docs/provenance/precedent.md`'s order: this tree,
OSFMK 6.1, Rhapsody and Darwin 0, XNU, then our own; the BSD donor is
`nmartin0/4.4BSD-Lite2`, branch `dev3`. No source that was not
released for others to use, and none under the GPL, is read, not even
as a specification. Where the documents are silent (their GAP entries)
or wrong (ERR), the maintainer decides, and the decision is recorded
where the gap is.

## Tier 0 — the ground

Measured against the trees as they stand:

| | state |
|---|---|
| OSFMK 7.3 boots | yes: OSF's primary boot and `mach_boot` load the kernel and bootstrap task from a vtoc-labelled IDE disk (milestone 1, 0079), and the bootstrap task reads `bootstrap.conf` from UFS, as OSF's own did, and starts `name_server`, `default_pager` and the server, in the template's order; multiboot stays as a second way in |
| Lites runs on it | yes, multi-user with NetBSD 1.0's userland: Utah's 1.1.u3 in this tree, each change to it in `docs/provenance/imports.md` |
| the 4.4BSD-Lite2 personality runs | yes, standalone, in its own repository |
| the three are one repository | **done** for two of them — the kernel at `osfmk/`, and Lites at `osfmk/src/lites/`, a component of the kernel's own sandbox as OSF's `osf1_server` was |
| one build drives all three | **done** for the kernel and Lites -- `build_world` builds the kernel, the bootstrap task, `name_server`, `default_pager` and Lites (0051, 0059), Lites with `build -here lites`, OSF's step 8; the server runs NetBSD 1.0's userland multi-user (`docs/status.md`) |
| the two Utah traps checked for | **done** — trap 1 fires, trap 2 does not on the generic path; `docs/status.md` |
| a specification to test against | **written**, from public documents only: `docs/osf1-interface.md`, about 300 items (0140), and `docs/osf1-calls.md`, 4,431 items from 265 pages of the OSF/1 1.0 manual (0141); each item says how a program checks it. The tests themselves are not yet written |

Those rows were brought up to date on 17 October 2026; the
milestones below carry their own dates.

**Lites is in the repository; its place has now been read rather than
inferred.** OSF's MK6.1 and MK7.3 release notes build the OSF/1 server
as `src/osf1_server`, a peer of `mach_kernel` in one ODE sandbox, with
the same build driver run after `mach_services`. Lites is at
`osfmk/src/lites/` accordingly. Two earlier placements were wrong —
inside `mach_services/servers/` and then at the top level beside
`osfmk/` — and `docs/provenance/precedent.md` records both. Every
lineage whose layout is documented — CMU's `src/{mk,ux}`, OSF's
separate OSF/1 tree, OSF RI's MkLinux DR3 — keeps the personality
server beside the microkernel as a peer, built against its exports.
The `startup` name belongs to the runtime slot in `/mach_servers`, not
to a source directory. `docs/provenance/precedent.md`, "Where the
server lives, and why", has the sources.

**Lites builds, starts and runs a userland** (milestone 3).
`osfmk/src/lites/makefile` runs Lites' own configure build under ODE
(`docs/provenance/precedent.md`, "How Lites meets ODE"; option A,
reviving Lites' CMU ODE build, is held in `docs/deferred.md` §2), and
`build_world` runs it. Utah's 1.1.u3 needed fixes for today's compiler
and for OSFMK 7.3's interfaces, each recorded in
`docs/provenance/imports.md`; its `OSF_ENVIRONMENT` switch, brought to
7.3, links it the way OSF linked its own servers. Started by OSF's
bootstrap task as `startup`, it mounts its root and execs
`/mach_servers/mach_init`, which a root built by
`docs/tools/mkbootdisk.sh` from NetBSD 1.0's binaries linked to
`/sbin/init` -- since 0107, `/sbin/init` itself (O-2; `docs/status.md`).

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

`makeboot` is PowerMac's step and is not found on AT386. Our server
builds after `mach_kernel`, linking against those four
libraries: `build -here lites`, in this list since 13 October 2026.
AT386's boot loader is the MK7.3 Release Notes' last step, `build
-here stand`; it builds on the running system only, whose binutils
read its 16-bit code (0077), so it is not in this list. That answers
"where do the Mach libraries live" by OSF's
precedent rather than by preference: they stay in `osfmk7.3/` and the
server links against them.

**The existing Lites adaptation.** `test_mk7.3`'s
`tools/lites/lites-osfmk73.patch` (1,414 lines, 29 files) made Lites
run on OSFMK 7.3 at build time against an external checkout. It is
history now: Utah's tarball is this tree's Lites, and each change made
to it is recorded in `docs/provenance/imports.md` with its precedent.
Six of the patch's files are ones Tier 1 rewrites --
`server/serv/user_copy.c`, `serv_syscalls.c`, `server_init.c`,
`server_exec.c`, `xmm_interface.c` and `vn_pager_misc.c` -- so changes
there are kept to what the running system needs.

**The two Utah traps, checked before anything is designed.** Both are
greps and both change the shape of Tier 1 if they fire.

1. Does Lites allocate per-thread state at a constant offset from a
   fixed-size C-threads stack, as Utah recorded of the OSF/1 server's
   `uthread`?
   If yes, the kernel must hand the server its own service stack and
   cannot reuse the kernel stack.
2. Do Lites' generated stubs copy arguments that the BSD service
   routines already copy? Utah found every argument moving twice and
   added a stub-generator option to suppress it.

**`bootstrap.conf`.** Three lines by default — `name_server`,
`default_pager`, `startup` — and the system now runs with exactly
those, Lites as `startup` (milestone 2). Whether the server should
register with `name_server` and serve its tasks the bootstrap
service, as the OSF/1 server did, is O-6 in `docs/deferred.md`.

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

**The bootstrapping Lites stays on 4.4BSD-Lite.** *Decided 15 October
2026, by the maintainer.* Lites 1.1.u3 is a Lite1 server: 201 of the
347 Berkeley files it carries have older SCCS versions than Lite2's,
none newer, about 790 CSRG deltas between them, and no Lites -- Utah's
or xMach's -- was ever moved to Lite2. For this goal it is brought to
Lite2 only file by file and only where the running system needs it,
each such change taking Berkeley's own delta from CSRG's history.
The real Lites update, to Lite2 proper, is its own work order after
this goal: built on, and run as a child server under, this one as its
parent. *Measured possible, 19 October 2026:* Helander's second-server
procedure (`lites/doc/README`) runs a second Lites from the running
one's shell, on a disk of its own, multi-user, with nothing changed in
the tree (`docs/status.md`, "A child server"; S-16 and S-17 in
`docs/deferred.md`). The BSD donor for the update is
`nmartin0/4.4BSD-Lite2`, branch `dev3`, the maintained Lite2.

**State on 17 October 2026**, milestone by milestone: 0 done; 1 done,
OSF's boot loader booting what the running system builds; 2 done; 3
done, with S-11 kept as Lites has it, for item 2's step 5 (S-8 closed,
18 October); 4 done, with
`sendmail` deferred (S-12); 5 done, the system rebuilding itself
identically but for three build stamps.
At the boundary with Tier 1, Lites against the OSF/1 server's
documented behaviour (`docs/deferred.md`), all fixed but O-3, which
closes with Tier 1 item 2: O-7, `-s` at the boot prompt; O-1, O-2,
O-4 and O-5, the OSF/1 server's arguments, root and first program;
O-6, the bootstrap service for its tasks.
Each milestone below ends with its own line. The system at this point
is kept as the `restored` branch, a checkpoint not committed to again
(`docs/restored.md`).

**Since then, to 21 October 2026**, nothing on this list reopened.
Child servers run as Helander designed, nested and side by side, and
four of the five defects they showed are fixed or settled (S-18 to
S-21, 0115-0124) and S-17, the parent's terminal, is open; named
pipes are built in (0119); six bug fixes from the
MkLinux project's 1999 tree are taken (0125). S-22, NetBSD 1.0's
shell failing with sixteen background jobs, is fixed in the userland
with NetBSD's own 1996 lines (`docs/deferred.md`).

**22 and 23 October 2026.** Tier 1 item 2's step 4 began (below). The
rule for sources was restated, as "How the design is decided" above
has it, and the work brought into line with it: S-17, the parent's
terminal after a child server halts, is open again, with no public
document to settle it; S-21 is kept as Lites has it, by the
maintainer's choice; and words after the root on the `startup` line
are ignored with a message, since nothing documents them (0107;
`docs/osf1-interface.md`, GAP-2). The OSF/1 interface was written up
from public documents as the specification (0140, 0141).

**0. The releases, not the copies.** Neither import is its upstream's
release (`docs/provenance/precedent.md`, "Where the two imports came
from"). Lites is restored from Utah's `lites-1.1.u3.tar.gz` (done, 13
October 2026: the tarball unpacked at `osfmk/src/lites`, where OSF's
`src/osf1_server` stood). For the
kernel, Apple's `DR3.osfmk.src.tgz` from the MkLinux DR3 CD, which
`slp/osfmk-mklinux`'s first commit, `0f6f7e6`, with its `start.s~`,
matches byte for byte (20 October 2026, `docs/provenance/sources.md`);
each of slp's eighteen
later commits gets a verdict, as `dev`'s do. The deviation record keeps
one command per tree, against reference copies of the two releases
kept in the repository. The maintainer's copies of the MkLinux CVS
repository and of the Lites tarball check both. The host tools are
built, not carried: `config` and `migcom` from OSF's 1999 sources in
that CVS repository, by DR3's own procedure (`docs/tools/mkhostbin.sh`),
as milestone 5 builds them on the running system. The target's
assembler is chosen to read OSF's sources as written: GNU as for
i386-elf, which keeps the System V assembler's `/` comments
(`docs/tools/mkbinutils.sh`). *State:* done. Lites restored; all
eighteen of slp's commits have verdicts, the last two, `444954a`
(multiboot's loader) and `7e81018` (`hd.c` under QEMU), once OSF's
boot loader ran (17 October 2026; `docs/provenance/verdicts.md`).

**1. Boot as OSF did.** OSF's primary boot and `mach_boot`
(`stand/AT386/boot`) on the vtoc disk, loading `/mach_kernel` and
`/mach_servers/bootstrap` from partition a, through
`i386/AT386/asm_startup.h`. QEMU boots the disk through its BIOS. The
multiboot entry slp added stays, as a second way in (maintainer,
12 October 2026), and `docs/tools/boot.sh` keeps it as an option. The
`hd.c` changes made for a boot without a BIOS (`dev`'s `052a105`,
slp's `7e81018`) are tested again under a BIOS boot before any is
adopted: `7e81018` adopted reworked (0079), `052a105` adopted reworked
(0098). *State:* done, 17 October 2026. OSF's boot, carried onto
DR3's text with OSF's own 1995 changes for an MK without the OSF/1
server (0077), is built on the running system; `mkbootdisk.sh` installs
it and `boot.sh BIOS=1` boots it (0079); the kernel takes OSF's
arguments again beside multiboot's (0079); and the running system's
kernel, bootstrap task, servers and Lites come up multi-user from
OSF's primary and pass the compile-and-install loop. `hd.c` is DR3's
but for one flag at probe time (0079). The set linked on the Linux
host boots from OSF's primary as well, its kernel linked as binutils
2.7 links it (0083; `docs/deferred.md`, "A kernel linked on the Linux
host, by OSF's loader").

**2. The three servers.** `bootstrap.conf` as OSF's template and
release notes give it: `name_server`, `default_pager` on partition b,
`startup` -- Lites -- with the root, partition a. `name_server` builds.
No `mach_init` server: OSF's bootstrap task and `name_server` provide
what it did. (Lites execed `/mach_servers/mach_init`, a link to
`/sbin/init`, until 0107 had it run `/sbin/init` itself: O-2.) *State:* done; `build_world`
builds all three (0051, 0059), and
`mkbootdisk.sh` writes the template's `bootstrap.conf`.

**3. Lites, multi-user.** Lites 1.1.u3 configured `STD+WS+osfmach3`,
run as an ordinary task, not collocated. Root on partition a in the
old UFS format OSF's reader and boot loader read and Lites mounts
read-write. Userland: NetBSD 1.0's i386 binaries, the release Lites'
own documents name -- marked as provisional; this layer is expected to
change most later. `/mach_servers/mach_init` was a symbolic link to
`/sbin/init`, as Lites' `doc/README` allows when a name server is
already running; since 0107 Lites runs `/sbin/init` itself, as the
OSF/1 server does (O-2). *State:* done. `/etc/rc` runs to a multi-user
login with NetBSD 1.0's daemons; programs fault, catch and return as
on NetBSD (S-3, S-4, S-5, S-7, 0053-0062); `cc` builds and runs
programs. S-11, Utah's `signal_send_running`, is kept as Lites has
it and done again in Tier 1 item 2, step 5.

**4. Online.** The kernel's own `ne` driver, already in the PRODUCTION
configuration, with QEMU's `ne2k_isa`; its two defects fixed first
(the 3c501 interrupt handler installed for it in `autoconf.c`, and
odd-length copies in `if_ne.c`). Lites' 4.4BSD TCP/IP stack opens
`ne0` by its Mach device name; `ifconfig`, `route`, then telnet and ftp.
*State:* the driver's fixes made (0056); `ne0` configured at boot
from `/etc/hostname.ne0`, the default route from `/etc/mygate`, and
`ping` answered. Measured 14 October 2026 under QEMU's user network
(`docs/status.md`, "Online"): telnet in, through `inetd` and a pty,
and out; ftp out, with `passive`; the resolver, against a server at
10.0.2.2; since 15 October, ftp in, with 4.4BSD-Lite2's `ftpd` built
on the running system (S-12). Done, with `sendmail` deferred.

**5. Hosting itself.** On the running system, ODE's minimal tools and
a GCC of OSF's own line -- gcc 2.7.x, the compiler MK7.2 and MK7.3
were built with, in its OSF/1 i386 ELF configuration -- rebuild the
kernel, the servers and Lites, and the result boots. The first
compiler is NetBSD 1.0's own, running under Lites, as Lites
self-hosted in 1995. Later GCCs come after this is stable.
*State:* done. Begun with NetBSD 1.0's `cc`, which compiles, links and installs
programs under Lites, among them Lite2's `ftpd` (S-12), and the
compile-and-install loop that once killed the server runs (S-14). On
the running system, 15 October: ODE's minimal tools, binutils 2.7 and
gcc 2.7.2.1 for OSF/1 i386 ELF, built (`docs/status.md`, "Hosting
itself"). 16 and 17 October: `build_world` there builds every
component, and what it built boots multi-user and runs the
compile-and-install loop clean -- after 0076 wrote three `locore.s`
instructions as bytes both assemblers take and sized multiboot's
loader for a System V data segment (`docs/deferred.md`). Done, 17
October: booted from OSF's primary with nothing running that it had
not built, the system built itself again, and 589 of 592 object files
came out identical; the other three carry the build's date and time
(`docs/status.md`, "Rebuilding itself"). After this, as the milestone
says, a later GCC -- deferred by the maintainer, 18 October 2026, for
S-8 and the O-items first.

The kernel configuration stays PRODUCTION, OSF's documented default;
FAST, with the debugger, is for debugging only.

## Tier 1 — the server becomes OSF-shaped

The five decisions, each settled by a public source, are in
`docs/deferred.md` with the reasoning. The order below is forced by
dependency, not chosen.

### The tests, alongside every item

*Added 23 October 2026.* The conformance suite is built from the two
specification files, one test per item, from each item's `Test:`
line, and run against both servers of item 1, so that the
`noemulator` server is measured against the emulator's at every step
of item 2. It stands in for VSX and VSE, which are not to hand, and
adds what they cannot see: the multithreaded, Mach and boot
behaviour, and the hostile programs of `docs/osf1-interface.md`
SYS-9, none of which may hang, crash or slowly exhaust the server.
`[DEC]` items are a guide and `[Fortran]` items are out
(`docs/osf1-calls.md`, "How to read it"). S-23's descriptor-table
fuzzer joins it in `docs/tools/`.

Wanted from the maintainer before the tests that depend on them:
rulings on six of the seven disagreements in the documents, ERR-18 to
ERR-23 (`SIGCONT`, stop signals in orphaned groups, per-thread pending
signals, which calls restart, `setpgid` with group 0, acquiring a
controlling terminal; ERR-24, interrupted partial transfers, is
settled from the documents), and decisions on
the open GAPs that are policy rather than fact -- GAP-10, the
task-level system-call port; GAP-11, what the bootstrap service gives
an unprivileged task; GAP-12, whether ordinary processes may make
Mach calls. Still to read for more of the interface:
`docs/osf1-interface.md`, "Not yet read".

### 1. One build, two servers

Build the server twice from one source: the emulator-based one that
works today and the exception-based one being written. Without it,
removing the emulator is a flag day and a failure anywhere is
indistinguishable from a failure everywhere.

*Corrected 19 October 2026.* This said OSF shipped "`startup` and
`startup.compute` from one tree"; no source has `startup.compute`,
and the line, from the roadmap's first draft, was never sourced. The
precedent is two other things. Patience built the no-emulator server
and measured it against the mature emulator-based one, both running,
before it became the basis of OSF's servers ("Redirecting System Calls
in Mach 3.0", §7 and §9). And OSF's own server is "built in two flavors"
from one tree, "collocated, as osf1_server.<config>.INK ... and as
osf1_server.<config> built to run in user space" (MK7.3 Release
Notes, step 8) -- that pair is collocation's, item 4.

*Done, 19 October 2026 (0109).* A Lites configuration option,
`noemulator` (`NO_EMULATOR`, `conf/MASTER`), and a second
configuration of the one tree, `STD+WS+osfmach3+noemulator`, which
`build_world` builds beside `STD+WS+osfmach3` into its own object
directory; Lites' configuration names are OSF's `SERVER_CONFIG`'s
counterpart. Nothing reads the option yet, so the two servers differ
only in their names; each boots and passes the compile-and-install
loop. Item 2 fills the second in.

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

*Corrected 19 October 2026: not one class at a time within a task.*
`i386/locore.s` sends a system call to the emulation vector when the
task has one and the vector's entry is set; any other positive call
number goes to `mach_call_range`, which, when the task has a vector
at all, jumps to `syscall_failed`, an illegal instruction, and only
when it has none raises `EXC_SYSCALL`. A task with the emulator
cannot send some calls by exception. So the cut-over is whole, in
the `noemulator` server of item 1, while the emulator's server keeps
running beside it; classes can still be brought up one at a time
inside the new server, each one measured against the old.

**The plan, 19 October 2026**, every piece under `NO_EMULATOR`, each
from the sources above:

1. *Exec without the emulator.* The server builds the process image
   a BSD kernel builds -- arguments, environment and `ps_strings` on
   the user stack, the entry registers -- which the emulator's
   `emul_init.c` builds today. Patience §7: the no-emulator server
   reused "mature OSF/1 Integrated Kernel code" so that "the internal
   environment became indistinguishable from the integrated OSF/1
   system". The integrated kernel's code for this is 4.4BSD's
   `execve()` and i386 `setregs()`, but 4.4BSD-Lite2's
   `kern/kern_exec.c` has no body ("Body deleted.", `return
   (ENOSYS)`), and its i386 `setregs()` alone does not build a stack.
   The donor for the stack is therefore Lites' own: the layout
   `emul_init.c` builds now, which NetBSD 1.0's programs already run
   on, moved into the server; Lite2's `setregs()` for the registers.
   The maintainer is to provide a better donor than Lite2.
2. *Entry and return.* Each process's thread gets an `EXC_SYSCALL`
   port of its own, set with `thread_set_exception_ports`, behaviour
   `EXCEPTION_STATE` and flavour `i386_THREAD_STATE`; the server finds
   the process by the port the message arrives on and answers in
   `catch_exception_raise_state`, as recorded in `docs/deferred.md`
   ("The syscall path is exception-driven"). Not the port Lites now
   gives every exception, with `EXCEPTION_STATE_IDENTITY`: debuggers
   interpose that one, and its messages carry port rights (Patience
   §3). *Added 23 October 2026:* a thread a program makes for itself
   with `thread_create` has no thread-level port, so the server also
   needs a task-level `EXC_SYSCALL` port, or the same in effect
   (`docs/osf1-interface.md`, SYS-8 to SYS-10; how it is set up is
   GAP-10). The dispatch is the integrated kernel's `syscall()`,
   "with 15 extra lines of preamble, 2 postamble and a name change"
   (§7):
   Lite2's i386 `trap.c` `syscall()` against Lites' own `sysent`.
3. *`copyin` and `copyout`.* `copyin` by `vm_read_overwrite`, which
   7.3 has as trap -30 (`kern/ipc_mig.c`); `copyout` by `vm_write`,
   which 7.3 takes unaligned (`vm_map_copy_overwrite_unaligned`), so
   the reply-message staging the emulator needs goes (§5.1).
4. *What the emulator does itself* -- mapped time, cached values, the
   per-binary-type tables (`emulator/i386/e_trampoline.c`) -- moves
   into the server.
5. *Fork's return in the child, signals and exit.* Signal frames on
   the user stack and `sigreturn`: Lite2's i386 `sendsig()` and
   `sigreturn()`. A signal for a thread that makes no system calls
   (S-11) is delivered as the OSF Mach 3 Kernel Interfaces'
   `thread_abort_safely` describes: between a suspend and a resume,
   retried when the abort fails.

Items 1 to 3 are the first that can be tested, together: the
`noemulator` server running one program, then `init`, then a shell.

*Steps 1 to 3 done, 19 October 2026 (0131).* The `noemulator`
server runs a NetBSD 1.0 program as init with no emulator in the
task: `lites/server/i386/syscall.c`, `kern/noemul_sysent.c`,
`kern/kern_exec.c`'s `execve()`, and `NO_EMULATOR` arms in
`serv/server_exec.c`, `user_copy.c`, `kern_subr.c`; the rows are in
`docs/provenance/imports.md`, kinds C, D and E. The emulator's server
compiles to the same machine code as before in every file touched
(`objdump -d`, compared). Measured on the boot disk, the probe in
`docs/tools/noemuprobe/` (a static NetBSD a.out without libc, so it
needs nothing step 4 adds): argv and envp on the stack; `open`,
`write`, `getpid`, `umask`, `readlink`, `lseek` with its quad; an
error's carry flag and errno (`EBADF`, `ENOENT`); calls 0 and 198;
`gettimeofday` and `stat` (188) written out, at an odd address too;
`EFAULT` for a bad path, a buffer in page 0 and a write into text;
`execve` of itself with new arguments and environment, open
descriptors kept; `exit`; and an unknown call killed by `SIGSYS`, as
4.4BSD's `nosys()` does. The emulator's server, given the same
probe, stops with `SIGSEGV` at the bad path, as its `useracc()` says
it will.

*Step 4 begun, 22 October 2026.* What a NetBSD 1.0 shell needs, in
the order it needs it, measured from the programs' own system call
sites: `__sysctl` (the page size, for `malloc`), `break`, `fstat`,
`readv` and `writev`, then `fork`; an interactive shell then needs
step 5, as Ctrl-C is a caught signal. `__sysctl` is in (0132, 0133),
`break` (0134), `fstat` (0135, 0136), and `readv` and `writev`
(0144, 0145), with OSF/1's limits. An independent review of each found
what `docs/deferred.md` S-23 records: the table's open window, now
closed for both servers (0137); its second half, a reference held
across a sleeping call, is open before item 2 is done. Testing
`readv` found a read into a bad address hung (0144), and that its data
is lost (S-24, settled from the documents: as 4.4BSD).

What differs from the plan above:

- *The table is 4.4BSD's, not Lites' `sysent`.* Lites' own table is
  numbered for its emulator (`stat` at 38 is 4.4BSD's `stat`, which a
  NetBSD program calls at 188), so the server gets a second one, in
  4.4BSD's numbers -- NetBSD 1.0's -- from Lite2's `syscalls.master`,
  pointing at the server's own functions. Generated by
  `docs/tools/mknoemulsysent.py`; 53 calls were `nosys` at first,
  47 after 0145.
- *The BSD donor is `nmartin0/4.4BSD-Lite2`, branch `dev3`*, the
  maintained Lite2 the maintainer pointed to on 19 October; it is the
  first place to look for BSD parts of Lites. Its `syscalls.master` is
  Lite2's unchanged; its `kern_exec.c` has a written `execve()`, read
  for the argument conventions.
- *No `ps_strings`.* The stack is the emulator's, which has none, and
  NetBSD 1.0's programs have run on it since 1995.
- *Only NetBSD's binaries.* The emulator runs 386BSD's and FreeBSD's
  through the same table with conversions of their older structures;
  without those they get ENOEXEC.
- *A caught signal ends the process* (`kern_sig.c`, `postsig()`)
  until step 5 builds the handler's frame; pending signals are taken
  as 4.4BSD's `userret()` takes them, on the way out of each call.

*Next, step 4, as of 23 October 2026.* `__sysctl`, `break` and
`fstat` are in; next `readv` and `writev`, then `select`, then
`fork`, which `init` needs; their bodies are in `dev3`
(`kern/sys_generic.c`, `kern/kern_fork.c`), against which Lites'
server copies are to be brought. Then S-23's second half, a
reference held on a file for the length of a call that sleeps
(*Design of the OSF/1 Operating System*, 11.1.2), before item 2 is
called done; then step 5. Each call is checked against its items in
`docs/osf1-calls.md`.

### 3. The locking

Lites has 603 `spl*()` calls resolving to its own `spl_n()` in
`include/sys/synch.h`, plus 106 `mutex_lock` and 78 `condition_*`,
across roughly 96 files. That model is CMU's, from its Unix server,
and documented in Golub et al. 1990: "sleep, wakeup, spl
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
task.** That is Utah's INKS design; OSF's build instead links the same
objects twice, a user server and `osf1_server.<config>.INK`, the
collocated one (MK7.3 Release Notes, step 8), as `mach_perf`'s
programs are linked here today, the `.INK` at `0xd0000000`. Which of
the two to follow is settled when this item starts.

*Tried, 20 October 2026:* Lites linked kernel-loaded stops at its first
port, because it names ports by address and a kernel-loaded task's
fast IPC space refuses chosen names (`docs/deferred.md`, "Collocation
uses OSF's design"). This item starts by changing that.

Expect about 13% on a realistic workload. Utah measured a full kernel
build at 1,682s against 1,463s, Andrew at 4%, SPEC SDM SDET about 8% at
low load and under 1% at high.

### 5. The ABI, and the boot path

The syscall surface filled to `docs/osf1-calls.md`: every call the
OSF/1 1.0 manual gives, with its flags, defaults and errors, OSF/1's
where they differ from 4.4BSD-Lite2's; then the kernel-loaded boot path
via `KERNEL_LOADED` and `unix_mapbase`/`unix_mapend`
(`docs/osf1-interface.md`, COLO).

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
- **Distributed IPC: DIPC and FLIPC.** *Added 21 October 2026, by the
  maintainer.* OSFMK 7.3's own: its `+NORMA+net`
  configurations shipped with MK7.3a (Release Notes, 28 October 1998),
  messages and memory carried between kernels over the x-kernel's
  KKT. What it takes, from `docs/provenance/sources.md`: the three
  spots that stop a `NORMA` kernel building, from MkLinux 1.0b2's
  text, and the block whose two names DR3 never defined (it tests a
  bit nothing sets); the `ne` driver attached to the x-kernel as the
  WD driver is (`xkwd`, `if_ns8390.c`); each kernel given its address
  and node number; then two kernels under QEMU, and `cmk1.1`'s FLIPC
  tests. Independent of Tier 1.

## What is not in scope

- **ECOFF and `/sbin/loader`.** OSF/1 1.3 and later from OSF RI —
  MK6, MK7 — use ELF, rtld and dlopen. The loader path belongs to
  OSF/1 1.0–1.2 and to DEC's Alpha line. `exec_with_loader` is out.
- **Alpha.** i386 is the platform OSF's MK7.3 release builds and
  boots, and is where the collocated fast path is already written.
- **DEC's own interfaces and the Fortran library.** What the 1.0
  manual marks as DEC's (its "Digital" pages and ENVIRONMENT NOTES,
  `[DEC]` in `docs/osf1-calls.md`) is a guide, not a requirement; the
  `libU77` pages (`[Fortran]`) are out.
- **OSF/1 AD's distributed machinery**, which the AD papers
  describe: the AD branch, not the MK single server. *Corrected 21 October
  2026:* NORMA stood in this list; the kernel's DIPC and FLIPC are
  OSFMK 7.3's own, now in Tier 2.

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
