# What was decided, what was deferred, and what the record says to do instead

The Lite2 tree's version of this file is written out of 127 commits.
This one began before any, recording the decisions taken during
design, each with the primary source that settled it, so that none is
reopened from memory; the work since is recorded with them. Several
sections under "Deferred" now end in what was done, and say so.

Where a decision is deferred rather than made, that is said, with what
would settle it.

## 1. Decisions made

### The emulator is removed, not fixed

**Settled by:** Patience, *Redirecting System Calls in Mach 3.0: An
alternative to the emulator*, USENIX Mach III Symposium 1993, pp.
57–74, §2.

OSF kept the emulator through MK 4.1 and removed it for MK 5.0.
(*Checked 19 October 2026:* neither version number is in Patience's
paper or in the Release Notes held here; the paper says only that the
no-emulator server is the basis of the OSF/1 1.2-based server and of
all OSF RI operating-system work, §9. The dates stand unsourced.)
Their reasons, in their categories:

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

### The intermediate goal's choices

**Decided 12 October 2026, by the maintainer**, for `docs/roadmap.md`,
"Intermediate goal". The evidence for each is in
`docs/provenance/precedent.md`, "What OSF's running system was" and
"Where the two imports came from".

- **The releases, not the copies.** Lites is restored from Utah's
  1.1.u3 tarball, and the kernel from the nearest readable DR3,
  `slp/osfmk-mklinux` `0f6f7e6`, each later change judged as a
  deviation. The maintainer is fetching independent copies to check
  both against.
- **OSF's boot loader, with multiboot kept.** The system boots through
  `stand/AT386/boot` and `asm_startup.h`, as OSF's did. slp's multiboot
  entry stays as a second way in, "so we still have the multiboot
  option for later down the line".
- **NetBSD 1.0's binaries as the userland, provisionally.** The release
  Lites' own documents name. Marked as the layer expected to change
  most: "later on we're probably going to be significantly modifying
  this part". The binaries live only on disk images a script builds,
  never in the source tree; several are GPL programs.
- **The network is the in-tree `ne` driver**, with QEMU's `ne2k_isa`,
  over OSF's documented boards, which QEMU does not emulate.
- **The compiler is OSF's line first.** gcc 2.7.x in its OSF/1 i386
  ELF configuration, bootstrapped on the running system from NetBSD
  1.0's own gcc, as Lites self-hosted in its day; later GCCs once that
  is stable.
- **PRODUCTION** stays the kernel configuration; FAST for debugging.
- **Lites runs as a separate task.** Collocation (`-k`) waits for
  Tier 1 item 4.

## 2. Deferred

### The bootstrap fork: Hurd or OSF

**Decided, 12 October 2026: OSF.** The kernel starts OSF's bootstrap
task again, through OSF's `bootstrap_create()` and `user_bootstrap()`
under their own names; the graft's functions stayed in
`kern/bootstrap.c` behind `#if 0 /* GNU Hurd bootstrap graft */`, and
`conf/files` no longer built GNU Mach's `kern/boot_script.c`. *Since
13 October 2026 the graft is deleted*, with `kern/boot_script.[ch]`,
once its source was identified as `slp/osfmk-mklinux`'s `63279d3`
(`docs/provenance/verdicts.md`, "slp's eighteen commits"). Taken
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
booting system. *Since:* the running system's `bootstrap.conf` is the
template's three lines (roadmap milestone 2), and the question is now
O-6, below.

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
one makefile, in Utah's driver's place, that runs its own `configure`
build (`docs/provenance/precedent.md`, "How Lites meets ODE"). The
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
- ODE Makefiles: *corrected 13 October 2026*, the release has CMU's
  for the top, `include/`, `liblites/`, `emulator/` and `server/`
  (xMach's copy had removed them), so only `bin/` needs one; the
  existing ones are 1992-95 and would need checking against OSFMK's
  `makedefs/`, and the top's `Makeconf` against the sandbox's.
- `gensym.awk` and Lites' MIG rules as ODE rules; OSFMK's `osf.osc.mk`
  already has MIG rules.

**What would make it worth revisiting:** wanting the server to build
the way OSF's did down to the configuration file — for instance when
Tier 1 item 4 builds two flavours, user and collocated, from one
`conf/`, as OSF's `SERVER_CONFIG` build did (corrected 19 October
2026: item 1, done in 0109, builds the emulator and no-emulator
servers, as two configurations of Lites' configure build).

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

### Booting with OSF's own boot loader

**Not needed yet; recorded because it is OSF's.** QEMU loads the kernel
and the bootstrap task directly with `-kernel` and `-initrd`, through
the multiboot entry in `i386/start.s`. OSF booted an
i386 from the disk: a primary boot in block 0, a secondary that reads
the root file system and loads `/mach_kernel` and
`/mach_servers/bootstrap`, with the `boot:` prompt and
`disk(unit, partition)/kernel` syntax of MK7.3 Release Notes §2.5.3.
Both are in the tree, `stand/AT386/boot/primary` and `secondary`, and
the secondary is linked with `libsa_fs`. The boot disk is already in
OSF's layout (vtoc, UFS root in partition a), so the remaining work
would be building `stand/AT386/boot` and installing it on that disk.
Taken up if multiboot ever stands in the way, or for fidelity once the
system runs.

**Tried 14 October 2026**: `build -here stand` stops at once on
"gcc: error: unrecognized command-line option '-m386'", which the
`primary`, `secondary` and `lib` Makefiles all put in `CFLAGS`. In GCC
2.7 `-m386` meant "tune for the 386"; GCC 2.8 replaced it with
`-mcpu=i386`, and today's is `-mtune=i386` (or `-march=i386`).
Since 0052 the three Makefiles say `-mtune=i386`, the same request
in today's spelling (each with an AI-ONLY note). The build then stops
in `lib/disk.c` at `boot/include/boot.h`'s `#include <ufs/fs.h>`:
nothing in this tree exports `ufs/fs.h` -- `file_systems` exports only
`mach/fs/file_system.h`, and finds its own `ufs/fs.h` with `-I.` --
and the HP700 boot includes it the same way, with only
`-I../include`, so OSF's own tree had it exported from somewhere this
one lacks. Past that, the primary has to fit its 512-byte and 8 KB
limits with today's code generation, which is not yet measured.

**Looked for, 16 October 2026.** The boot needs more than `ufs/fs.h`:
`primary/sys.c` and `boot.c` include `<dirent.h>` and
`<sys/reboot.h>`, and `boot.h` `<ufs/fs.h>` and `<ufs/dinode.h>` --
4.3BSD-Reno's layout, OSF/1's. With `-nostdinc` (`osf.gcc.mk`,
`_ansi_NOSTDINC_`) the build sees only the export tree and `sa_mach`,
and nothing in OSF's tree exports any of the four: not here, not in
the MkLinux CVS checkout, not in MkLinux 1.0b2's `osfmk`. In this tree
they exist only in the server's headers (Lites' `include/sys/reboot.h`
and `include/sys/dirent.h`, and `ufs/ffs/fs.h` and `ufs/ufs/dinode.h`
in 4.4BSD's layout) and in `file_systems/ufs`, OSF's own copies of the
UFS headers for `libsa_fs`, which the secondary links. So the boot was
built in a sandbox whose export tree also held the OSF/1 server's
headers -- the server OSF never released, whose slot Lites fills.
Options when milestone 1 is taken up: the boot finds `ufs/*.h` in
`file_systems` and the two `sys` headers in Lites' include tree,
through `INCENV`, as `migcom` finds Lites' `machine/types.h`; or the
server's headers are exported, as OSF's evidently were. The boot is
`MkLinux`-marked and reads ELF, ext2 ("MACH/BSD/OSF1 and LINUX file
systems", `primary/README`), and `-m386`, its own flag, is valid for
gcc 2.7.2.1 on the running system, so it may build there as written.

**Where the headers went, 16 October 2026.** `<sys/reboot.h>` is not
among OSFMK 7.3's kernel headers, though `mach_kernel/sys` has its
neighbours and `i386/kgdb_interface.c` and three `sqt` files still
include it. The boot's history says it was changed to "use mk
includes only (not sys/dir.h ....)" (bernadat, 30 April 1992,
`primary/sys.c` and `boot.c`). So the boot's sources are older than
the tree they came in: the MK headers they were written against left
it, and nothing took their place. Two more needs show once those four
are found:

- `lib/elf.c` includes `<sys/elf.h>` for `Elf32_Shdr`, `Elf32_Sym`,
  `EI_MAG0` and `SHT_SYMTAB`, as does `mach_services/cmds/makeboot`'s
  `elf.c`, a host program. Lites' `sys/elf.h` has no section headers
  and needs `u_int32_t`; the kernel's `mach/elf.h` has all four.
- `lib/asm.s` uses `ENTRY()`, which `machine/asm.h` aligns with
  `ALIGN`, defined only `#ifdef ASSEMBLER`. The kernel's
  `template.mk` and libcthreads' `AT386/machdep.mk` pass
  `-DASSEMBLER`; the boot's Makefiles do not, and gcc 2.7.2.1's
  `osfelf.h` defines only `__LANGUAGE_ASSEMBLY` for `.S` files.

Tried on the Linux host, in a scratch sandbox, nothing written to the
tree: `-idirafter` a directory of links to those six headers, after
every directory the build gives, so none can hide an MK header, and
`-DASSEMBLER`. `lib`'s C files compile; `lib/boot2.s` then stops GNU
as 2.42 at `%ax` with an `l` suffix, as `locore.s` did -- 16-bit code
in the assembler dialect of its day, which binutils 2.7 reads. So the
boot goes next on the running system.

On the running system, the same way, with `-m386` as written and
`-DASSEMBLER` given only to the assembler files (`asm.o_CENV` and its
siblings on `build`'s line): `lib/libboot.a` builds, `boot2.s`
included. The primary then stops in Lites' `sys/dirent.h`, which wants
Lites' `ino_t`; the build's `sys/types.h` is `sa_mach`'s. That is the
gap in one line: the boot was written against an OSF/1 server's
headers, in the same namespace as the MK's, and Lites' are a 4.4BSD
namespace of their own. Options for milestone 1, none tried:

1. The boot's six headers, from OSF's own trees where they exist:
   `ufs/fs.h` and `ufs/dinode.h` from `file_systems/ufs`, `sys/elf.h`
   as the kernel's `mach/elf.h`, found after everything else by
   `-idirafter`; and `dirent.h` and `sys/reboot.h`, which no OSF
   tree here has, written for it -- new files, in `docs/`'s terms
   ours, not OSF's.
2. Lites' `include/` exported whole, as OSF's server headers must have
   been. Its `sys/types.h` would then stand where `sa_mach`'s does for
   every task in the sandbox, which is more than the boot's problem.

**OSF's own answer, found 17 October 2026.** Two OSF precedents, one
for each kind of sandbox:

- *With the OSF/1 server.* MK7.3's `setup/AT386/build.sh` (in MkLinux
  1.0b2's `osfmk`, Robert, 1994-95) gives every component one include
  directory, `INCDIRS="-I${expdir}usr/include"`, where the MK's
  headers and the OSF/1 server's and libc's are exported together, and
  the MK7.3 Release Notes build `stand` last, step 9, after the server
  (2.5.2). Lites' 4.4BSD namespace cannot stand in that tree.
- *Without it.* OSF's "Self-Contained Mach Distribution" (barbou, 28
  December 1995; MkLinux 1.0b2's `osfmk`, whose top `Makefile` builds
  `stand` among `MACH_SUBDIRS` even with `PURE_MACH`) made the same
  boot build from the MK tree alone: `primary/sys.c` "removed include
  of sys/reboot.h, replaced include of dirent.h with ufs/defs.h"
  (`file_systems/ufs/defs.h`, whose `struct dirent` is marked "from
  sys/dirent.h"); `primary/boot.c` and `secondary/mach_boot.c` define
  the `B_*` shifts and masks they use, "from OSF/1's sys/reboot.h";
  `lib/elf.c` includes a local `elf.h`, "copied from OSF/1"; the three
  Makefiles add `-I../../../../file_systems`; and each assembler rule
  passes `-D__LANGUAGE_ASSEMBLY`, the macro `machine/asm.h` then
  tested. `rmelfhdr.c`, a host program, took `MAP_VARIABLE` as 0 under `#ifdef linux`.

DR3's boot is the later, server-dependent text -- `lilo_env.c` and the
`elf_i386.boot` script are newer than 1.0b2 -- in a tree that dropped
`stand` from its `SUBDIRS` and was never built with it: its `boot.h`
declares `int strlen()` against `file_system.h`'s `<string.h>`, and
its `asm.h` tests `ASSEMBLER`, which its boot Makefiles never pass.

Tried on the running system, scratch copy only: the 1995 changes
carried onto DR3's text, plus `ASSEMBLER` beside `__LANGUAGE_ASSEMBLY`
(as `mach_perf`'s `AT386/machdep.mk` passes both), `size_t strlen`,
`-m386` as written, and `rmelfhdr` built with `CENV=-DMAP_VARIABLE=0`
and the kernel's `mach/elf.h` (OSF/1's `sys/elf.h`, guard
`_SYS_ELF_H_`) as `sys/elf.h` in a tools directory. Built: `libboot.a`,
the primary (7,903 bytes, under its 8 KB, `55aa` at 510) and
`mach_boot` (46,195 bytes). `lilo/mklilo`, a Linux host program for
LILO, does not build and is not needed. Under QEMU's BIOS the primary
loads once the partition entry carries its CHS address (start.s reads
16 sectors from it; `mkbootdisk.sh` writes zeros) and the primary is
at sector 0 and at the partition's start; it then goes quiet, not yet
looked into.

**Decided 17 October 2026** (maintainer): the 1995 changes, carried
onto DR3's boot, in 0077, with the repairs above and `-m386` as DR3
wrote it; `docs/provenance/imports.md` lists each. `rmelfhdr` and the
boot are built on the running system (`docs/status.md`, "Hosting
itself"). Next: why the primary goes quiet, and `mkbootdisk.sh`
writing the partition's CHS address and installing the boot.

**Booted, 17 October 2026.** Three things stood between OSF's boot
loader and a running system, none of them in the boot:

- *The disk.* The primary goes quiet because it is not running: the
  MBR's copy loads the partition's first 16 sectors by the partition
  entry's CHS address, and QEMU's BIOS, left to guess the geometry,
  read them from sector 51. Given the geometry the label assumes, 16
  heads and 63 sectors (`boot.sh`, `BIOS=1`), and a partition entry
  carrying its CHS addresses (`mkbootdisk.sh`; it wrote zeros), the
  primary prints "Primary boot", loads `hd(0,a)/stand/mach_boot`, and
  the secondary loads `/mach_kernel` and `/mach_servers/bootstrap`,
  printing sizes as the MK7.3 Release Notes show them.
- *The kernel's way in.* slp's `444954a` put DR3's `asm_startup.h`
  under `#if 0` and made the multiboot copy unconditional, and
  `machine_startup()` called `parse_multiboot()` always: under OSF's
  loader it read the stack arguments' registers as a multiboot block
  and faulted in `strlen()` on a real-mode pointer. `start.s` now
  keeps `%eax` at entry and takes multiboot's path only for
  `0x2BADB002`; otherwise DR3's `asm_startup.h` reads OSF's arguments,
  as DR3 did. `parse_multiboot()` runs only after a multiboot entry,
  and `do_bootstrap_compat()`'s ELF reader returns when OSF's loader
  has given the regions. `asm_startup.h`'s three `testb $x,%edx`
  are `%dl`, the register the `b` suffix tests; binutils 2.7 encodes
  both `f6 c2 ib`.
- *Which kernel.* OSF's `load_elf()` places the first PT_LOAD
  segment's page at its load address. GNU ld 2.42 with
  `-z noseparate-code` begins that segment one page low, at 0xff000,
  with the ELF headers, so a kernel linked on Linux lands 4 KB off;
  binutils 2.7 begins it at 0x100000, file offset 0x1000. So OSF's
  loader boots the kernel the running system builds, with OSF's
  toolchain, and the Linux host's kernel keeps the multiboot entry.
  (Since 0083 the host's kernel links as binutils 2.7's does and OSF's
  loader boots it too: "A kernel linked on the Linux host", below.)

Measured: everything built on the running system -- boot, kernel,
bootstrap task, `name_server`, `default_pager`, Lites -- boots from
OSF's primary to `login:`, multi-user, and passes the
compile-and-install loop (`BIOS=1`; `docs/status.md`, "The boot").

With OSF's loader running, `hd.c` was tested as DR3 wrote it, as the
roadmap required before deciding slp's `7e81018`: it hangs on a 465 MB
disk under both loaders, for a reason in the driver, not the BIOS
(`docs/provenance/verdicts.md`, `7e81018`; 0079). The size turned out
not to matter: below, "`hd.c`: a race, not the disk's size".

**Every CPU enters at `pstart`** (`mp/mp_v1_1.c`, `start_other_cpus()`
points the AP trampoline there), so in a multiprocessor kernel the
other CPUs would also have stored their `%eax` as the multiboot magic,
after the first had used it. Since 0080 the value waits in `%edx`, as
the start lock and `%ax`'s clearing use `%eax`, and only the first CPU
stores it, after the `master_is_up` test sends the rest to
`slave_start`. Two lines of that `NCPUS > 1` path, `movb $1,%eax` in
`start.s` and `movb %edx,wncpu` in `asm_startup.h`, are spelt `%al` and
`%dl` for GNU as 2.42, as `testb` was. With them `locore.o` assembles
for FAST+MP; the rest of FAST+MP did not yet build on the Linux host
(`ipc_port.c`'s debugger code, under today's `-Werror`). Since 0095 it
does, and since 0096 it boots on two CPUs: "The multiprocessor kernel
on the host", below. Measured on the running system (17
October 2026): its binutils 2.7 assembles `movb $1,%eax` and `movb
$1,%al` alike, `b0 01`, and `movb %edx,wncpu` and `movb %dl,wncpu`
alike.

**HEAD on the running system, 17 October 2026** (0080--0085): its
kernel builds, 727,639 bytes, without `-fno-optimize-sibling-calls`
(`GCC_LATEST` 2_7_2_1); its segments at 0x100000 and 0x176cd8, file
offset 0x1000. With its own bootstrap task and servers, from OSF's
primary, it boots multi-user and passes the compile-and-install loop;
by multiboot, with the host's bootstrap task, it boots multi-user.

### The multiboot loader and a System V data segment

`do_bootstrap_compat()` in `kern/bootstrap.c`, slp's `444954a` (verdict
then pending; **adopt reworked** since 0079), maps each PT_LOAD segment from
`trunc_page(p_vaddr)` for `round_page(p_filesz)` (text) or
`round_page(p_memsz)` (data) bytes. That is the whole segment only if
it starts on a page. binutils 2.7's `ld` puts the data segment right
after the text, at an address congruent to its file offset, as the
System V ELF ABI allows; the bootstrap task the running system links
has its data at `0x080639b0`, 0x18b8 bytes, ending at `0x08065268`,
and the loader maps to `0x08065000`. The kernel stops after "I've
found: 2 sections" (measured 16 October 2026; the host's bootstrap
task, page-aligned, boots the same kernel and servers multi-user).
OSF's own loader for the servers, `bootstrap/elf.c`, measures from the
segment's page: `p_vaddr + p_filesz - trunc_page(p_vaddr)`, and the
BSS after it. Options:

1. `444954a`'s loader sized as `bootstrap/elf.c` sizes: from
   `trunc_page(p_vaddr)` to `round_page(p_vaddr + p_memsz)` (data) or
   `p_filesz` (text and read-only). It then takes either `ld`'s
   layout. Its verdict waited on milestone 1 (since 0079: adopt
   reworked).
2. The running system links the bootstrap task with its data on a
   page (`ld -Tdata`), and the loader keeps its assumption. That puts
   a host-tool workaround where the defect is the loader's.
3. Milestone 1 first: OSF's own path, `loader_info` and the boot
   loader, which `444954a` replaced. The running system's bootstrap
   task waits for it.

**Decided 17 October 2026** (maintainer): option 1, in 0076. The
three region sizes are measured from the segment's page; for a
segment that starts on a page they are what they were.

### The first free address after a multiboot loader

`start.s` builds the kernel's page tables at the first free physical
address, in `%ebx`. OSF's loader hands it over, the top of what it
loaded (`asm_startup.h`, "mem top"). On the multiboot path slp's
`444954a` wrote a constant, `0x500000`: about 3 MB past a kernel that
ends at `0x1dd084`, room for a bootstrap module of that size and no
more, with everything between unused by the VM system
(`i386_init()`'s hole runs to `first_avail`). The multiboot
specification gives no such top. Utah's multiboot Mach, the
specification's first kernel, keeps clear of the kernel, the
information block, the command line, the module list and each module
(Mach4, `i386at/model_dep.c`, `init_alloc_aligned()`), and since 0081
`%ebx` is the highest end among those, the offsets coming from Utah's
`multiboot.h` through `genassym`, the kernel's own way of giving C
layouts to its assembler. Measured under QEMU: the page directory at
`0x20c000`, just past the 183,252-byte bootstrap module, where it was
at `0x500000`; multi-user boot.

### A kernel linked on the Linux host, by OSF's loader

Why GNU ld 2.42 puts the kernel's headers a page low is in its own
source. `bfd/elf.c` (`_bfd_elf_map_sections_to_segments`) loads the
ELF and program headers in the first PT_LOAD segment when the link
script says it wants them -- any use of `SIZEOF_HEADERS` sets
`load_phdrs` -- and, "to deal with -Ttext or something similar such
that the first section is not adjacent to the program headers", puts
them in the page below the first section. The default script opens
with `. = SEGMENT_START("text-segment", 0x08048000) + SIZEOF_HEADERS`,
so with the kernel's `-Ttext 0x100000` the headers go to 0xff000 and
the segment starts there. A script without `SIZEOF_HEADERS` leaves
`load_phdrs` clear, and ld falls back to the rule it keeps "for
compatibility with old scripts": headers in the segment only when the
first section's offset in its page leaves room for them. That is what
binutils 2.7 does on the running system: its kernel at 0x100000 has
no headers loaded (file offset 0x1000), its emulator at 0xa0001020
none, and its `default_pager`, whose text follows the headers from
0x08048000, has them.

So `mksandbox.sh` writes ld's own default `elf_i386` script, `-z
noseparate-code` variant, from `ld --verbose`, with only `+
SIZEOF_HEADERS` taken out, into `MK_BUILD` (its notice permits copying
with or without modification), and passes it as `-T` in `LDOPTS`, the
kernel link's hook (`conf/AT386/template.mk`, `LDFLAGS+=${LDOPTS}`),
which Lites' `ld` also takes. Programs linked through `osf.prog.mk`
keep the default script, as they kept binutils 2.7's. Not taken: the
same script in `LDARGS`, which reaches every link and would drop the
headers from `bootstrap`, `default_pager` and `name_server`, where
binutils 2.7 loads them; and changing OSF's `load_elf()` to honour
`p_vaddr`, a vendor change where configuration serves.

Measured: the kernel's segments at 0x100000 (file offset 0x1000) and
0x1a8000, entry 0x100128, `etext`, `edata` and `end` where they were;
the Lites server at 0x08048000 and the emulator at 0xa0001000, both
file offset 0x1000; `bootstrap`, `default_pager` and `name_server`
unchanged in shape, headers loaded at 0x08048000. The whole set built
on the host -- kernel, bootstrap task, `name_server`, `default_pager`,
Lites -- boots multi-user from OSF's primary (`BIOS=1`) and by
multiboot, and passes the compile-and-install loop both ways.

### The multiprocessor kernel on the host

OSF's compiler builds FAST and FAST+MP from this tree unchanged, and
the running system's FAST+MP boots under QEMU with two CPUs, both
running user code (17 October 2026). GCC 13 needed three things,
none in OSF's design:

- Two lines that stopped the build (0095): `ipc_port.c`'s debugger
  code compares a pointer with `MACH_PORT_NULL`, and `mp_desc.c`
  declares `intstack` without `extern`.
- The I/O APIC (0096). `mp_v1_1_take_irq()` writes a redirection
  entry through two plain pointers, select and window: select the high
  word, write the destination, select the low word, write the vector.
  GCC 13 dropped the first select as a dead store; with `-fno-dse` it
  kept it but scheduled the second select ahead of the first window
  write. Either way the destination word was never written, QEMU's I/O
  APIC showed every entry with destination 0, no interrupt reached a
  CPU -- the clock never ticked -- and the kernel stopped after "I've
  found: 2 sections", on one CPU or two. gcc 2.7.2.1 emits the four
  stores in order. `select` and `window` are `volatile` now, as the
  file's own `LF()` macros make the local APIC's registers. And since
  0097 the host's `CARGS` carry `-fno-dse -fno-tree-dse`: GCC 2.7
  removed no store to memory as dead, and any other register OSF's
  code reaches through a plain pointer keeps its stores, though not
  their order.
- QEMU's topology. With `-smp 2`, SeaBIOS's MP table listed one CPU:
  it lists one per package, and QEMU makes two cores of one package.
  `-smp 2,sockets=2,cores=1,threads=1` lists both, and OSF's code
  starts the second ("Starting CPU 01", "cpu 1 active";
  `docs/tools/boot.sh`).

Measured, host build: FAST+MP on two CPUs boots multi-user, both CPUs'
`machine_slot` running with ticks counting, and passes the
compile-and-install loop; on one CPU it boots multi-user; FAST boots
multi-user. On the running system (0099), gcc 2.7.2.1 builds PRODUCTION,
FAST and FAST+MP from the tree with 0095--0098, without a warning, and
its FAST+MP runs on two CPUs, both `machine_slot`s running with ticks
counting; its PRODUCTION, with 0098's `hd.c`, boots multi-user. From
OSF's primary (0101): with the running system's boot, kernel,
bootstrap task, servers and Lites, all built there, and QEMU's
`BIOS=1` with two sockets, both CPUs run user code, ticks counting,
and the compile-and-install loop passes. No CPU count was typed at the
boot prompt ("mach [-sah#]", `model_dep.c`), so the kernel took all
the MP table lists.

### `hd.c`: a race, not the disk's size

0079 found DR3's `hd.c` hanging on a 465 MB disk and booting a 165 MB
one. Traced with QEMU's `ide_bus_exec_cmd`, `ide_sector_read`,
`pic_set_irq` and `pic_interrupt` events, the same kernel (DR3's
driver, `restore_request` set at probe) on the same two disks by
multiboot did the opposite: the 465 MB disk went to `login:` and the
165 MB one stopped after the bootstrap task's "I've found: 2
sections". The sequence, in both:

	0x10  RESTORE        hdprobe(), polled         line raised
	0xec  IDENTIFY       hd_read_id(), polled      line raised
	      one interrupt for the two: "HD: false interrupt"
	0x91  SETPARAMETERS  first hdstart(), polled   line raised
	0x20  READ sector 0  start_rw()

IRQ 14 is edge-triggered and QEMU's 8259 latches an edge in IRR until
the interrupt is taken, so a second rising edge before then is lost
in the first. What follows the READ is a race between the CPU taking
SETPARAMETERS's pending interrupt and QEMU finishing the read on its
I/O thread:

- *Interrupt first* (the 465 MB run here): `hdintr()` finds the flag,
  clears it and calls `start_rw()`, whose READ the drive drops while
  it is busy; the read's own interrupt follows and is handled as a
  read. The boot goes on.
- *Read first* (the 165 MB run here, the 465 MB one in 0079): its edge
  merges with the pending one, one interrupt is taken, the flag turns
  it into "restore done", and the READ issued again is dropped because
  DRQ is set (`hw/ide/core.c`: commands are ignored while BUSY or DRQ
  is set). Nothing raises the line again, and the bootstrap task waits
  on its first read forever.

With the flag clear at probe (0079), the stale SETPARAMETERS interrupt
is taken as the read's: `hdintr()` waits out BUSY ("interrupt
w/controller not done"), and the read's data is there when it reads
it; either order boots. Those two messages are S-6's noise, the
polled commands' interrupts, which `dev`'s `052a105` masks (since
0098, with the probe's RESTORE as well: S-6). On a drive
whose reads take milliseconds, the first order is the usual one,
which may be why DR3's flag did no harm on the hardware OSF had; that
is a reading, not something DR3 says.

### One locore.s for two assemblers

`locore.s` carries dev's `3036fdb` (dev3, from dev `10e3591`): thirteen
operands renamed so GNU as 2.42 takes them, the suffix kept so the
encoding is DR3's. binutils 2.7, the assembler of the running system,
takes ten of the new spellings and DR3's of all thirteen, and rejects
three of the new ones: `movl %ss,%eax`, `movl %eax,%ds` and
`movl %eax,%es` in `trap_set_segs` (measured on the running system,
16 October 2026: DR3's whole `locore.s` assembles, and the kernel
builds with it). GNU as 2.42 rejects DR3's three. No instruction
spelling of the three keeps both the encoding (`8c d0`, `8e d8`,
`8e c0`) and both assemblers: `movw` adds a `66` prefix, and `mov`
without a suffix fails on 2.7 as `movl` does. Options:

1. The three as bytes, with the instruction in the comment, the way
   `locore.s` already writes `movl %eax,%cr4` (`.byte 0x0f,0x22,0xe0`)
   for an assembler that lacked it: DR3's encoding, any assembler.
2. `movw`, which both take, at the cost of the prefix.
3. DR3's text, and the host assembles with binutils 2.7 too. That
   assembler would also read GCC 13's output for every C file, which
   it predates (`.cfi_*`, `.section .note.GNU-stack` and more).

**Decided 17 October 2026** (maintainer): option 1, in 0076. The
host's kernel disassembles the same at `trap_set_segs` (`8c d0`,
`8e d8`, `8e c0`), and the running system builds the kernel from the
tree as it stands.

### Making disks on the running system, as OSF did

**Possible since 13 October 2026; not the project's procedure** --
the host's `mkbootdisk.sh` makes the disks, and the paragraph's "not
yet runs `newfs`" below is from before the 13 October test.
NetBSD 1.0's `newfs`, under Lites, made a 100 MB file system in 13
cylinder groups on a second disk (`newfs /dev/rwd1a qemu100`, with a
`disktab` entry for the vtoc's partition a, because Lites answers
`DIOCGDINFO` with "Inappropriate ioctl"); `fsck` passed it, it
mounted, took 7.7 MB of `/usr`, and passed `fsck` again after
unmounting. Recorded first because it is OSF's procedure: OSF never
built a disk image from a directory. `disk_setup` ran the `vtoc`
utility and `newfs` on the target machine under MK7.3 itself, and
`mk_install` then extracted the release onto the new file systems
(MK7.3 Release Notes §2.4.2.2, §2.4.2.4). The boot floppies came from
`mkbootfloppies` on a running MK7.X (§2.4), and
`stand/AT386/boot/secondary/README` does the same by hand with `newfs`,
`mount` and `cp`. This project makes its disk on the host instead,
with NetBSD 11.0's `makefs -o version=0` (`docs/tools/mknbmakefs.sh`;
`docs/provenance/precedent.md`, "How the disk is made"), because
nothing here yet runs `newfs` or mounts a file system.

Once Lites runs, the same steps can be taken inside the system:
`newfs -O`, mount, copy. Lites runs NetBSD, FreeBSD and 386BSD
binaries, and its `doc/README` says `mount` and `fsck` "should be
taken from Lite"; the old format would then be written by 4.4BSD's own
file system code, in the system being built. The first disk still has
to come from outside, as OSF's came from an earlier MK release, so
`makefs` stays for that.

### A halting idle loop: `POWER_SAVE` and `machine_idle()`

**Not needed yet; recorded because it is OSF's own hook.**
`idle_thread_continue()` in `kern/sched_prim.c` never halts the CPU: it
loops on `splx()`/`splsched()`, with interrupts enabled for a few
instructions per pass. Under QEMU's TCG that makes the kernel take
device interrupts seconds late, and the boot uses `-icount shift=auto`
to compensate (`docs/method.md` §4, "Slow is not hung"). OSF left a
hook for exactly this loop —

	#if	POWER_SAVE
		machine_idle(mycpu);
	#endif /* POWER_SAVE */

— with no `machine_idle()` for i386 and no configuration setting
`POWER_SAVE`. An i386 `machine_idle()` would be `sti; hlt`, the idiom
every emulator and CPU handles, and would also stop the idle CPU
spinning at full power on real hardware. Not taken now because the
emulator flag is configuration, outside the tree, and costs nothing;
taken up if a run without `-icount`, on hardware or under KVM, needs
it.

### Lites' receive threads: `cthread_mach_msg`

**Deferred, 13 October 2026; corrected 14 October 2026.** Lites'
server loop calls `cthread_mach_msg` with a minimum and a maximum of
server threads to keep waiting on its port set, and under `OSFMACH3`
supplies the call itself as a stub that ignores both and calls
`mach_msg` -- its comment says the call was "missing from cthreads"
-- with `cthread_msg_busy` and `cthread_msg_active` stubbed too. This
entry said OSFMK 7.3's cthreads has the call and does the accounting.
It does not: `mach_services/include/cthreads.h` declares the three,
`cthread_mach_msg` taking a `struct cthread_mach_msg_struct`, and
`libcthreads` defines none of them (no source in the tree, no symbol
in the library). The only implementation here is Lites' own
`serv/cprocs.c`, CMU's cproc-based one, built only with `data_synch`.
Without the accounting the server's kernel-thread limit could not
hold, and the system stopped (S-13, fixed in 0067 by lifting the
limit under `OSFMACH3`).

**What would make the accounting worth having:** wanting the limit
back -- fewer kernel threads under load. That means implementing the
three in OSFMK's `libcthreads` to the interface its header declares,
on `cthread_t` where `cprocs.c` has `cproc_t`, and building the struct
at the four call sites in `serv/ux_server_loop.c`.

### Lites' `memory_object_synchronize`

**Deferred, 13 October 2026.** Lites panics in
`seqnos_memory_object_synchronize`, "unimplemented", as it shipped.
OSFMK 7.3 sends it for `vm_msync` on a mapped file (`vm/vm_user.c`
line 795), so an `msync(2)` on a file Lites backs stops the server.
OSF's `default_pager` implements it by answering
`memory_object_synchronize_completed`; Lites would answer it after
writing the range back. Taken up when something calls `msync`.

### Stability, as measured on 13 October 2026

**Not yet a stable multi-user system.** With commits 0039-0042 (the
frame pointer kept, the pid-2 hack off under `OSFMACH3`), a 40 MB root
made by `mkbootdisk.sh` from NetBSD 1.0's `base`, `etc` and `comp`
sets boots multi-user to `login:` and, logged in as root:

- `uname`, `df`, `who`, pipes (`cat /etc/* | sort | uniq | wc`), 300
  file creations, `cp -r`, `tar`, `cksum` of a 3.2 MB file, `dd` of
  10 MB at about 5 MB/s, `halt` ("syncing disks... done");
- NetBSD 1.0's `cc`, `cc -O2`, `cpp`, `cc1`, `as` and `ld`: a program
  that forks 20 children, waits for each and computes fib(25) was
  compiled under Lites and printed "hello from Lites:
  fib(25)=75025 sum=20 pid=88", both values right;
- a second disk made with NetBSD 1.0's `newfs` under Lites, mounted,
  filled and checked (above).

Each run was QEMU TCG, 64 MB of memory. What fails:

- **S-1. File systems made by `makefs` with more than one cylinder
  group -- fixed in `mkbootdisk.sh`.** NetBSD 1.0's `fsck` died in pass
  5, "Check Cyl groups", with a bus error (a fault at address 0x10),
  so `/etc/rc` dropped to single-user; and writing to such a file
  system -- even `cpp` writing its output -- took the Lites server down
  ("(bootstrap): '/dev/boot_device/mach_servers/startup' task
  terminated"). A kernel built with a print in `exception_no_server()`
  showed the server faulting with no exception port, at `0x8053cdb` in
  `ffs_nodealloccg()`, reading `fs_cs(fs, cg)`; 4.4BSD's macro is
  `fs_csp[cg >> fs_csshift][cg & ~fs_csmask]`, and `makefs` leaves
  both fields zero -- today's FFS calls them `fs_old_csmask` and
  `fs_old_csshift`, spares -- so every group after the first indexes a
  null pointer; `fsck`'s 0x10 is `(1 & ~0) << 4`, group 1's entry.
  NetBSD 1.0's `newfs` writes -512 and 9 (`mkfs.c`: `~(i - 1)` and
  log2 `i`, `i` = `fs_bsize / sizeof(struct csum)`). `mkbootdisk.sh`
  now sets them so, in the superblock and in each group's copy; a 200
  MB root in five groups passes `fsck` at boot and takes `cc`, `tar`
  and `dd`. Left open: Lites, like 4.4BSD, trusts the superblock, so a
  bad one still takes the whole server down, not one mount.
- **S-2. No paging space unless `default_pager` is given one.** Without
  an argument it starts with none, and under `cc` the screen fills with
  "(default pager): ps_allocate_cluster: no space in available paging
  segments; swapon suggested" and the server dies.
  `default_pager default_pager hd0b` -- the form of the MK7.3 Release
  Notes' example, `def_pager /mach_servers/default_pager sd0b`, and of
  `bootstrap.template` ("the default_pager can taking a paging device
  name") -- prints "(default_pager): added device hd0b". This is O-8
  below, and it is required, not optional. **Done** in configuration:
  `mkbootdisk.sh` writes `default_pager default_pager hd0b`
  (`PAGING_DEV` for another), and `docs/status.md`'s example has it.
- **S-3. Every bad access was SIGBUS, and its address lost.** Fixed
  in 0053. Lites' `catch_exception_raise` (`server/serv/ux_exception.c`)
  had CMU's arguments, `(exception, code, subcode)`; OSFMK 7.3's
  `exc.defs` passes `(exception, code[], codeCnt)`. Caught with gdb:
  `code` was a pointer, `subcode` the count, 2, and the array held
  `KERN_PROTECTION_FAILURE` and the fault address, so
  `code == KERN_INVALID_ADDRESS` never held and SIGSEGV was never
  sent. Under `OSFMACH3` the routine now takes OSFMK's signature, as
  OSFMK's own servers do (`bootstrap/bootstrap.c`,
  `catch_exception_raise`), and the state routines read the same
  array (S-7). A test program that faults three ways and reports how
  its child died: before, "null", "unmapped" and "text" all killed by
  signal 10; now "unmapped" by 11, SIGSEGV, and "null" and "text" by
  10. NetBSD 1.0 gives 11, 11 and 10, so "null" is S-4's.
- **S-4. A NULL dereference was SIGBUS, not SIGSEGV.** Fixed in
  0060. Every BSD of the time chooses the signal for an unresolved
  user page fault the same way, `(rv == KERN_PROTECTION_FAILURE) ?
  SIGBUS : SIGSEGV` -- 4.4BSD-Lite2's `i386/i386/trap.c`, NetBSD 0.9,
  1.0 and 1.2 (i386, hp300, sparc), FreeBSD 2.0.5 and 2.1 -- and
  Lites' `ux_exception()` is CMU's port of that line. The difference
  is page 0. 4.4BSD-Lite2's `USRTEXT` is 0, text at address 0, so a
  NULL read succeeds and a write is SIGBUS. NetBSD 1.0's `USRTEXT` is
  `CLBYTES` and its native ZMAGIC binaries start at `__LDPGSZ`
  (`sys/exec_aout.h`, `N_TXTADDR`), leaving page 0 unmapped, so a
  NULL dereference is an invalid address: SIGSEGV. FreeBSD's QMAGIC
  does the same. Under OSFMK, `mach_init()` (`libmach/mach_init.c`),
  run in every process by the emulator, maps page 0 `VM_PROT_NONE`
  "so that the program doesn't get it", and the same rule makes the
  reference a protection failure: SIGBUS. Later practice moved toward
  SIGSEGV for every protection fault -- POSIX's `SEGV_ACCERR`; FreeBSD
  7.0's `machdep.prot_fault_translation` -- but that is not the
  system these binaries came from. **The change**, under `OSFMACH3`:
  `ux_exception()` sends SIGSEGV for a protection failure whose
  address is in the first page, when page 0 is the reservation:
  386BSD and Linux binaries load their text at 0, where the
  reservation fails, and a write there stays SIGBUS, as on NetBSD
  (`ux_page0_mapped()`, a `vm_region()` of page 0; 0061). The
  reservation stays; to the BSD
  process the page is absent, as on NetBSD. A test program, faulting
  in a child each way: NULL write, NULL read and a read at 0xffc, 11;
  an unmapped address, 11; a write to text, 10 -- NetBSD 1.0's
  answers in every case. A SIGSEGV handler now catches a NULL
  dereference (exit 111).
- **S-7. A handler for SIGSEGV or SIGBUS was never run.** Fixed in
  0053, OSFMK's way. The test program with `signal(SIGSEGV, h)` and
  `signal(SIGBUS, h)`, `h` calling `_exit(100 + sig)`: the child
  exited with status 0, not 110 or 111. Lites delivered an
  exception's signal by notifying the task's signal port
  (`send_signal()`, `sendsig()`, `signal_notify()` in `server/serv/`)
  and not answering the exception; the emulator's
  `do_signal_notify()` (`emulator/i386/e_signal.c`) then suspends the
  thread, `thread_abort`s it, sets its state to the signal entry and
  resumes it -- Mach 3's way, where an aborted thread waiting in
  `exception_raise` keeps waiting (`exception_raise_continue_slow()`
  in CMU's and Mach4's `kern/exception.c`). OSFMK 7.3's `exception()`
  (`kern/exception.c`) does not loop: any result but `KERN_SUCCESS`
  or `MACH_RCV_PORT_DIED` goes on to `exception_try_task()` and, with
  no other handler, `exception_no_server()`, which terminates the
  task. **The change**, all under `OSFMACH3`: the server registers
  its exception port `EXCEPTION_STATE_IDENTITY` with
  `i386_THREAD_STATE` (`kern/init_main.c`), OSFMK's behaviour in
  which the reply carries the thread's new state, and implements
  `catch_exception_raise_state_identity`, a panic stub until now.
  When the process catches the signal, the reply sends the thread to
  the emulator's signal entry, `e_i_got_a_signal`, with the old state
  on the user stack -- the frame `do_signal_notify()` builds, written
  with `vm_write` by `machine_signal_redirect()`
  (`server/i386/serv_machdep.c`); the entry takes the signal with
  `bsd_take_signal()` as before. The emulator records the entry in the
  shared page, `us_sigtramp` (`include/sys/ushared.h`), when it
  registers its signal port. If the stack cannot take the frame, the
  process dies of SIGILL, as in 4.4BSD-Lite2's i386 `sendsig()` when
  `useracc()` fails ("Process has trashed its stack") and NetBSD
  1.0's, `sigexit(p, SIGILL)` (0061); the old notification there
  would abort the thread the reply resumes. A signal not caught
  leaves the state unchanged and takes its old course. Measured: the
  test program's "null+handler" exits 110 and "unmapped+handler" 111; a
  second, `sig2.c`, catches 200 SIGSEGVs in a row with a handler that
  `longjmp`s out, twice, then takes a SIGALRM in `pause()`; `/etc/rc`
  and the shell are as before. **Left**: an asynchronous notification
  already on its way when a thread faults can still abort it out of the
  exception RPC (Lites' own `SAS_NOTIFY_DONE` window, now narrow); and a
  handler left by `longjmp` never clears `SAS_NOTIFY_DONE`, so later
  asynchronous signals wait for the process's next system call -- Lites'
  own, on Mach4 too. A handler that returned had `%eax` set to 1 by the
  emulator's `take_a_signal()`, meant for the setjmp test of
  `take_signals_and_do_sigreturn()`, which makes it itself; removed in
  0062. Not for Linux binaries: the emulator's own exception port for
  them (`emulator/i386/e_exception.c`) still forwards with
  `exception_raise` and CMU's arguments.
- **S-8. A shell sometimes stopped after a pipeline. Closed, 18
  October 2026, as S-9 and S-13:** not seen in 18,900 pipelines since
  both were fixed (measured below). `cat /etc/* |
  sort | wc -l` prints its count and the shell, NetBSD 1.0's `sh`,
  neither prints a prompt nor runs what follows on the same line --
  `; echo DONE`, the rest of a `while` loop -- in about half the runs.
  Found with gdb on the server's process list at the time: the `sh` has
  no children, live or zombie, and its one server thread sleeps on
  "ttyin"; typing a further command runs it. So the shell has gone
  back to reading the terminal with the rest of its line dropped, as
  it does after an interrupt. Lites' own `doc/README` names this area
  ("tsleep and friends is very fragile code ... Also signal handling
  needs to be reworked"); cause not yet found. `/etc/rc` and plain
  commands are not affected. **Since 0054** (S-9, which stopped the
  server with work queued, and whose "typing a further command runs
  it" this matches): 15 runs of a 60-iteration `while` loop of that
  pipeline, up to three machines at once and the host loaded, all
  finished; two others, run during a kernel build, stopped before the
  first iteration and were not caught with gdb. Seen once more, 14
  October 2026: a `csh` command line typed just after login
  (`echo ... >> /etc/master.passwd; pwd_mkdb ...; echo USEROK`) never
  finished, and nothing typed after it ran; one boot in seven of that
  sequence, none in 75 iterations of it from `sh`, and the run had no
  gdb stub. Every boot since runs with one. Watched, not closed.

  **Measured, 18 October 2026, with `dev3` after 0104**, booted from
  OSF's primary, PRODUCTION, three machines at a time under QEMU on a
  two-CPU host. Each logged in as root and typed, at `csh`, `cat
  /etc/* | sort | wc -l; echo CSHOK` -- the 14 October shape, a csh
  command line just after login -- then, at `sh`, a one-line `while`
  loop of the pipeline with `cc` compiling in a loop in the
  background. Three boots of 300 iterations against one compile loop
  of 60, then nine boots of 2,000 iterations against three such loops
  run back to back: 18,900 pipelines, every one finished, and the
  nine longer boots' csh lines printed CSHOK and each machine answered
  `echo ALIVE` after its loop. The two causes
  found are both fixed: S-9's stall (0054), which gdb saw as the
  shell asleep on "ttyin" with "typing a further command runs it",
  and S-13's (0067), whose shell "echoed and never ran" -- the 14
  October `csh` sighting is from the day before 0067. Reopen if seen,
  with the server's thread state from gdb: every boot has the stub.
- **S-9. The whole system could stop, idle, with work queued.**
  Fixed in 0054. Seen first with the `ne` card (below), then at
  random: `csh` never printing its prompt after `tset`, other
  processes frozen with it, the console still echoing. In Lites'
  server, read with gdb: five cthreads on `cthread_status.run_queue`,
  all 13 kernel threads in use (`kernel_threads` equal to
  `max_kernel_threads`), and a waiter spinning in `cthread_idle()`
  (`mach_services/lib/libcthreads/cthreads.c`). Its loop,
  `while (run queue empty && !p->undepress);`, reads a field that is
  not `volatile` and calls nothing, so today's GCC loads the queue's
  head once, before the loop (the disassembly shows the one load and a
  loop on `undepress` alone); a waiter that saw the queue empty then
  waits only for `cthread_wakeup()`, which finds waiters on
  `waiting_dlq`, and this one is not on it yet. `undepress`, the loop's
  other condition, is declared `volatile` for exactly this, and
  `spin_lock_t` is `volatile int`; the run queue's head was missed.
  The loop now reads it through a `volatile` pointer. Every program
  linked with `libcthreads` -- the bootstrap task, `default_pager`,
  the server -- takes the fix when relinked. The inline
  `spin_try_lock()` and `spin_unlock()` (`include/i386/cthreads.h`)
  had the same exposure, asms without a `"memory"` clobber and so not
  compiler barriers, as their out-of-line versions in
  `libcthreads/i386/lock.s` are by being calls; given one in 0063.
  Lites' makefiles do not
  relink on a changed library, so a sandbox built before 0054 needs
  `startup` removed first.
- **S-10. Disk I/O could stop for good after a spurious IRQ15.**
  Fixed in 0055. Seen with the `ne` card and at random: every process
  asleep in "biowait", QEMU's `info pic` showing the master with IRQ2,
  the cascade, in service (ISR 0x04) and IRQ14 and IRQ15 pending at
  the slave, which the master then never passes on; QEMU counted one
  IRQ15, from the second IDE channel. `i386/interrupt.s` treats an
  IRQ15 the slave does not have in service as spurious and returns
  with no EOI at all -- right for a spurious IRQ7, the master's own,
  but the master had put IRQ2 in service for the slave's request, and
  only an EOI clears it. NetBSD 1.0's `i386/isa/vector.s` sends the
  master its IRQ2 EOI for every slave interrupt. Now a spurious IRQ15
  EOIs the master and nothing else.
- **S-11. A caught signal does not reach a process that makes no
  system calls.** Found reviewing 0053: a program spinning with a
  SIGALRM handler and a 50 ms interval timer took one alarm in 52
  seconds, at its next system call. gdb on the server: `psignal()` ran
  for every alarm, and neither `send_signal()` nor the emulator's
  `do_signal_notify()` ran at all. Utah's choice: `psignal()`'s `SRUN`
  case sends only `if (signal_send_running)`, a debug switch in
  `server/kern/kern_sig.c` that is 0, with their note "XXX there is
  apparently a race here. With this defined we (on the PA) get
  regular segmentation faults ... The downside of commenting this
  section out is that you cannot interrupt (only SIGKILL) a process
  that isn't making system calls ... Need to fix this." Signals with
  a default action are not affected, nor are processes that make
  system calls. **Not changed; for the maintainer**: setting the
  switch is the upstream's own way on; the race they describe is the
  emulator's signal thread suspending and aborting a thread that is
  already taking the signal at a system call's end, and under OSFMK an
  abort of a thread in an exception is S-7's again. A fix would give
  the notification the care 0053's reply has, measured with this
  program first.
- **S-12. NetBSD's `ftpd` and `sendmail` die in `setproctitle()`.**
  `ftp` in fixed on 15 October 2026 by the maintainer's option (a);
  `sendmail` deferred by the maintainer. NetBSD 1.0's libc
  `setproctitle()` (`lib/libc/gen/setproctitle.c`) stores through
  `PS_STRINGS`, the `struct ps_strings` NetBSD's exec puts at the top
  of the stack, 0xf7bfdff0 for its `USRSTACK` of 0xf7bfe000. Under
  Lites the stack ends at `VM_MAX_ADDRESS`, 0xc0000000
  (`include/i386/vmparam.h`), and under OSFMK, as under CMU's Mach
  and Mach4, everything above that is the kernel's: the address
  cannot be mapped for the task. Caught with gdb at the server's
  exception handler: `ftpd`'s first `setproctitle()`, before its
  greeting, faults (`EXC_BAD_ACCESS`, `KERN_INVALID_ADDRESS`,
  0xf7bfdff4, `ps_nargvstr`) and the process dies of SIGSEGV; `ftp`
  sees "421 Service not available". Which programs take libc's is in
  their binaries: a stripped NetBSD 1.0 program keeps its run-time
  linker's symbol table, and `docs/tools/aoutsyms.py` reads it.
  `ftpd` and `sendmail` take `_setproctitle` from the shared libc
  (type 0x01, undefined) -- NetBSD built its `ftpd` with
  `-DHASSETPROCTITLE` -- and `echo QUIT | sendmail -bs` dies of
  SIGSEGV, as `sendmail -bd` does before it listens. *Corrected 15
  October 2026:* this entry first named six programs, four by
  inference. `inetd` and `named` (BIND 4.8.3) define their own
  `_setproctitle` (type 0x05, in the program's text), beside `_Argv`
  and `_LastArg`, and carry its `-%s [%s]` title format: the one
  that rewrites `argv` in place. Measured, `inetd`'s
  forked `echo`, which sets its title first, answers, and `named -b`
  runs and answers `nslookup`. `mailq` and `newaliases` are
  `sendmail` by other names. Utah's answer to this kind of gap is
  in `doc/README`, "BINARIES": "There are limitations to the binary
  compatibility ... route, mount, fsck, nfsd, mount_nfs,
  mount_kernfs, mount_* should be taken from Lite." The options were
  (a) as `doc/README` says, the program from 4.4BSD-Lite2, whose
  `ftpd` sets its title by rewriting `argv` (`sendmail` is not in the
  Lite2 tree: `usr.sbin/sendmail/README`: "The beta version of
  sendmail 2.7.0 was removed at the developers request", sic), built
  on the running system with NetBSD's `cc`; (b) the server completes
  a store to the `PS_STRINGS` page for NetBSD binaries by stepping
  over it -- instruction decoding in the server, for a cosmetic call;
  (c) NetBSD's libc rebuilt with Lites' `USRSTACK`, a change to the
  userland. (a) was taken: Lite2's `ftpd` builds with
  `make NOMAN=1 LDADD=-lcrypt` -- NetBSD 1.0 keeps `crypt()` in
  `libcrypt`, and its own `ftpd` `Makefile` links it so -- and
  installs over NetBSD's (`docs/status.md`, "Online").
- **S-13. The system stopped, idle, with work queued -- again.**
  Fixed in 0067. After S-9's fix, a shell still stopped now and then
  after a pipeline, everything typed after it echoed and never ran:
  one boot in seven of one sequence, and the S-8 symptom. Caught with
  gdb, 14 October 2026, at `(sleep 3; echo hello-echo) | telnet
  localhost 7; telnet localhost 13`: the shell's server thread first
  on `cthread_status.run_queue`, with cron's; `kernel_threads` equal to
  `max_kernel_threads`, 13; four wired threads (network input, device
  reply, timeout, pager) and nine unwired ones running; the CPU in
  the kernel's idle loop. So nine server threads sat in the kernel --
  in `mach_msg` receives on the port set, the only place the server
  waits without bound -- holding every kernel thread a woken thread
  could run on. Lites' `serv/ux_server_loop.c` sets the limit, 13,
  with the note "This must be at least 5 (wired) + 1 (running) +
  receive_max", and receive_max, 6, holds only if `cthread_mach_msg`
  enforces it; under `OSFMACH3` it is Utah's stub and nothing does
  (Lites' receive threads, above). The fix is Lites' own setting for
  where it cannot multiplex, 0, "allowing all cthreads to have Mach
  threads" (`libcthreads/cthreads.c`, `cthread_set_kernel_limit`), as
  under `USEACTIVATIONS`. Likely S-8's cause as well -- "typing a
  further command runs it" is a message arriving and a receiver
  returning -- and S-8 stays watched until runs without it say so.
  They have: S-8 closed, 18 October 2026.
- **S-14. The server died writing a mapped file back.** Fixed in
  0069. A compile and copy loop on the running system --
  `cc -o h hello.c && cp /usr/libexec/ftpd /tmp/f && install -c -s
  ...` -- killed it on the first round, every time: "'.../startup'
  task terminated" from the bootstrap. Caught with printfs in the
  kernel's `exception_try_task()` (scratch only): the server's own
  thread faulted in `memcpy` under `uiomove()` under `ffs_write()`,
  reading at 0x608000, the page after a one-page region at 0x607000,
  with nothing mapped there. The writer was Lites' pager:
  `seqnos_memory_object_data_return()` in `serv/xmm_interface.c`,
  Utah's May 1996 copy, handling a page the kernel paged out
  of a file a program had mapped and written. Two defects there, in
  every copy -- the release, Utah's later file, and xMach's:
  - `wlen = pager->size - offset;` was set for every page, not only
    inside the `if (offset + length > pager->size)` whose message
    prints it, overriding `wlen`'s starting value, `length`, one
    page. So a page that was not the file's last wrote everything to
    the end of the file from a one-page buffer.
  - `vn_pageinout()` takes an `off_t` and had no prototype; the call
    passed a 32-bit `vm_offset_t`, so every later argument was read
    one word over: the offset 4 GB too high, `aresid` pointing into
    `proc0`, and the `proc` pointer garbage. With the first defect
    fixed alone the server died in `psignal()`, called from
    `ffs_write()`'s file-size check with that garbage `proc`.
  The fix, by the maintainer's choice: the line moved into the `if`,
  and a prototype beside `vn_rdwr()`'s in `include/sys/vnode.h` --
  `vn_pageinout()` is Lites' `vn_rdwr()` without the cache-state hook.
  Precedent, read and not copied: 4.4BSD's `vnode_pager_io()`, which
  this pager replaced, writes one page and cuts only a page that
  crosses the file's end (the same in Lite, 8.8, and Lite2, 8.10).
  Measured: fifteen rounds of the loop and twenty of a copy and
  `install` loop pass, and the copies compare equal to their source.
- **S-5. `timed` died of a bus error** at every boot. Fixed in 0058.
  Not the network: with `ne0` configured at boot (`/etc/hostname.ne0`,
  `/etc/mygate`; 0056) it still died. Caught with gdb at the server's
  `catch_exception_raise_state_identity` (0053), which has the
  faulting state: `EXC_BAD_ACCESS`, `KERN_PROTECTION_FAILURE`,
  address 0, at `eip` 0xa0009288 -- in the emulator, not in `timed`:
  `bsd_adjtime()`, the MIG stub, storing its `olddelta` result.
  `timed` calls `adjtime(&ntime, (struct timeval *)0)` (NetBSD 1.0's
  `usr.sbin/timed/timed/timed.c`), and a null `olddelta` is allowed;
  Lites' `e_adjtime()` (`emulator/e_bsd.c`) handed the caller's
  pointer to the stub as its out argument, which the stub always
  writes. It now receives into a local and copies out only if
  `olddelta` is not null, as `e_setitimer()` in the same file does
  with `ovalue`. Measured: the boot says "starting system logger,
  time daemon." and nothing more. Of the other stubs in `e_bsd.c`
  given a caller's pointer as an out argument, `e_getrusage()` passes
  `rusage` the same way, where NetBSD answers a null one with EFAULT,
  not a signal; left.
- **S-6. Driver noise.** "hdintr: interrupt w/controller not done." and
  "HD: false interrupt", already open (`docs/status.md`; `dev`'s
  `052a105`). Explained since 0084: the interrupts of the driver's
  polled commands, RESTORE, IDENTIFY and SETPARAMETERS ("`hd.c`: a
  race, not the disk's size"). **Fixed in 0098**: nIEN, bit 1 of the
  device control register, set around the three polled commands, so
  the drive raises no interrupt for them -- `dev`'s `052a105`, which
  set it for IDENTIFY and SETPARAMETERS, with the probe's RESTORE
  added. Measured: "HD: false interrupt" no longer on the screen; the
  compile-and-install loop passes by multiboot and from OSF's primary.
- **S-15. "dev 0: alternate sectors corrupt"** at every boot, from
  `i386/AT386/bios_label.c` (line 340) as the bootstrap task reads the
  label. `mkbootdisk.sh` writes the alternates table where the vtoc's
  `alt_ptr` says, 15360 bytes into the partition, sector 93, with
  `ALT_SANITY` (0xdeadbeef) first, and the image holds it there; the
  kernel reads something else. Found 17 October 2026 while checking
  S-6's screens, on kernels before and after 0098. **Fixed in 0100**,
  in `mkbootdisk.sh`. Traced with QEMU's IDE events: the read went to
  sectors 81 to 84, not 93. The first SETPARAMETERS gave the drive 4
  heads and 51 sectors -- the label's geometry then, from IDENTIFY,
  which QEMU had guessed from the partition table: the disk was made in
  whole megabytes, so the UNIX partition ended mid-cylinder, at head 3,
  sector 51, of the nominal 16 by 63. `read_vtoc()` then set the label
  to the vtoc's 16 by 63 and read the alternates through it, and the
  drive, told 16 by 63 only at the next `hdstart()`, took the address
  for sector 81. DR3's order -- the label first, SETPARAMETERS later --
  is harmless where the drive's geometry and the vtoc's agree, as they
  did on a disk OSF's own tools partitioned; the disk is made that way
  now, in whole cylinders, so that the partition ends on one as fdisk
  ends it. Measured: IDENTIFY answers 16 by 63, the read is of sectors
  93 to 96, and the message is gone. Disks made before 0100 keep the
  message under multiboot; `boot.sh BIOS=1` gives QEMU the geometry and
  never had it.

- **S-16. The clock is a day behind, on every boot since 1 March
  2000.** Found 19 October 2026: the server prints "Time is set to Sun
  Oct 18" on the 19th, and a program's `gettimeofday` agrees, 86,400
  seconds behind the host, the parent server's and a child's alike.
  The kernel's `bbc_gettime()` (`i386/AT386/bbclock.c`) counts years
  from 1900 -- `yr = (yr < 70) ? yr+100 : yr` -- and its
  `yeartoday()` applies the Gregorian century rule to that count, so
  100, the year 2000, is given 365 days. CMU's rule was "(year%4) ?
  365 : 366" (Mach 3.0 MK83's `kernel/i386at/rtc.c`, and Rhapsody's
  `bsd/dev/i386/rtc.c`), right for 1901 to 2099; the century rule came
  with "Changes from lance@mtxinu.com to correct more leap year
  problems" (92/03/28, a branch revision, 2.6.1.2, in the file's own
  CMU history, which MK83 does not have).
  `bbc_settime()` counts whole years from 1970, which the rule suits,
  but writes the year as `dectohexdec(j - 1900)`, which after 1999 is
  no two-digit BCD year. OSFMK 6.1, DR3, MkLinux 1.0b2, MkLinux's CVS
  tree and `dev` have both. **Fixed in 0113**, in that file, from the
  Mach line itself, with no Rhapsody or XNU code. `yeartoday()` has
  CMU's rule again, as MK83 has it, as Utah's Mach4 UK22 kernel has it
  (`Mach.UK22` in the MachUK22 disk image, disassembled: "testb $3",
  365 or 366), and as this tree's `sqt/clock.h` has it ("LEAPYEAR",
  "(year)%4==0", "good till time becomes negative"). The year written
  is this tree's own line for the same MC146818 chip,
  `chips/mc_clock.c`'s `mc_write()`, "Trim years into 0-99 range":
  `if ((years -= 1900) > 99) years -= 100;`. MK83 and Mach4 UK22 write
  `j - 1900` too. That line brings `mc_clock.c`'s CMU notice into
  `bbclock.c`, after OSF's, as `docs/provenance/conventions.md`
  requires; no notice is removed or moved. Rhapsody's
  `bsd/dev/i386/rtc.c` and XNU (xnu-123.5 to xnu-517) fixed the same
  two places, and were the precedent until the Mach line was searched;
  `nmartin0/4.4BSD-Lite2` `dev3`'s `i386/isa/clock.c` is Berkeley's
  own, not this file, with the same "% 4" rule and no setter.
  Measured: the two functions, compiled on the host around a model of
  the chip, set and read back every day from 1 January 1970 to 18
  January 2038, where the kernel's 32-bit `time_t` ends: 24,854 days,
  none wrong (`docs/tools/bbclockcheck.py`); the unfixed file, the
  same way, fails 27,734 checks: the chip's year written as "100" from
  1 January 2000, and every date read back a day early from 1 March
  2000. Booted
  from OSF's boot loader, the server's `/etc/rc` prints "Mon Oct 19
  14:04:40 PDT 2026" for 21:04 UTC that day, and a program's
  `gettimeofday` reads Mon Oct 19 21:08:27 UTC with the host at
  21:09:01, half a minute later. Lites does not set the chip
  (`host_set_time()` leaves `bbc_settime()` under `#if 0`,
  `kern/posixtime.c`), so the setter is measured on the host only.
- **S-17. A child server leaves its parent's terminal raw.** Found 19
  October 2026 (`docs/status.md`, "A child server"). Lites'
  `second_tty_open()` (`serv/second_cons.c`, its block marked "From
  OSF/1:") reads the terminal's modes with `TIOCGETP`, sets `RAW` and
  clears `ECHO` with `TIOCSETP`, and nothing sets them back; after the
  child ends, the parent's shell sees no end of line and takes no
  command. Lites' file is "Copyright (c) 1992 Carnegie Mellon
  University", "Author: Randall W. Dean, 1992" (CMU's single server,
  which Lites grew from; `doc/FILES` gives it as CMU's), with that one
  block labelled as OSF/1's. **Open.** No public document of OSF's
  says what a second server does with its parent's terminal when it
  halts, and no permitted tree has a fix. Until there is one, the
  procedure in `docs/status.md` saves the modes with `stty -g` before
  a child and puts them back after it. A fix is the maintainer's to
  rule on; one permitted pattern is 4.4BSD-Lite2's curses, which saves
  the terminal's settings when it takes the terminal and restores them
  when it gives it up.
- **S-18. The first key typed after a child server ends is lost.**
  Found 19 October 2026 (`docs/status.md`, "A child server"); traced
  20 October. After a child's `halt` the parent's shell printed no
  prompt until a key was pressed, and that key was gone: "echo"
  arrived as "cho". The child's console reader, a thread of the child
  server (`serv/second_cons.c`, `second_cons_intr()`), was in a
  `read` of the parent's terminal, asleep in the parent's `ttread()`
  (`ttysleep(..., TTIPRI | PCATCH, ...)`), when the child's task died.
  `task_remove_reverse()` (`serv/proc_to_task.c`) sets `P_WEXIT` and
  wakes the process's server threads, but `tsleep_main()`
  (`serv/serv_synch.c`) went back to sleep unless the sleep was
  `sigpause`'s: "must ttysleep but not sigpause on exit". The read
  stayed, held the dead process until a key came, and took that key.
  **Fixed in 0115**, by Lites' own rule: this tree's newer sleeps,
  `serv/synch_prim.c` (the `data_synch` option, marked experimental
  and not built), end every `PCATCH` sleep when the process exits
  (`tsleep_abs()`, `PKW_EXIT`) and keep only the uncatchable ones,
  "analogous to what used to be called uninterruptible waits". The
  `ttysleep` the old exception kept was for draining a dying process's
  terminal, and `proc_died()` does that as `proc0`, "We don't want to
  sleep on the died proc" (`proc_deref()`), never as the dying
  process. `tsleep_main()` now ends a `PCATCH` sleep on `P_WEXIT` and
  returns `EINTR`; a 0 would send `ttread()` round to sleep again.
  4.4BSD-Lite2 has no precedent: its exiting process is one thread,
  which cannot be asleep elsewhere. Measured, on a build that also restored the parent's terminal at a child's halt, since withdrawn (S-17): the parent's prompt comes back as the child halts, with no key pressed, and the next line
  arrives whole; the same after a single-user child's `reboot`.

### The `ne` driver, measured on 14 October 2026

For roadmap milestone 4. With QEMU's `-device
ne2k_isa,iobase=0x300,irq=10`, the kernel probes it: "ne0: at atbus3,
port = 300, spl = 6, pic = 10, [52:54:00:12:34:56]". Lites opens an
interface by its Mach name, so `ifconfig ne0 ...` is the way in. What
happens, unchanged and with fixes tried in a scratch tree only:

- **Unchanged.** The boot reaches `login:`, slowly; `ifconfig ne0`
  never returns. `autoconf.c` installs `at3c501intr`, the 3Com 3c501's
  handler, for `ne` (precedent.md, "Network"), so the card's
  interrupts are never served.
- **With `neintr` installed, and the odd-length copies indexing by
  bytes** (`((char *)up)[len-1]`, not `*(up+len-1)` on an `int *`):
  the boot stops before `/etc/rc` finishes -- gdb on the server's
  process list shows `syslogd`, `update` and `init` asleep in
  "biowait" and "getblk", disk I/O that never completes. `neintr()`
  runs before the interface is opened and initialised, and its receive
  loop, `while (pend != ns->ns_cur)`, advances past a header with a
  bad status only under `#ifdef NEDEBUG`; otherwise it spins at
  `SPL_SIX`, above the disk's interrupts.
- **With that loop also made to advance**: the boot stops earlier, at
  the root `fsck`.

**Working since 0056.** The stops in the last two runs were not the
driver's: they were S-9 (the server's cthreads idle loop) and S-10
(a spurious IRQ15 leaving the master PIC's cascade in service, so the
disk's IRQ14 never came), fixed in 0054 and 0055. `neintr()`
is left as it is: its receive loop still does not advance past a
header with a bad status unless built with `NEDEBUG`, which has not
been seen to happen. 0056 makes the three fixes to the
driver: `autoconf.c`'s `ne` entry names `neintr`; the odd-length
copies in `nefetch()` and `neput()` index the last byte as a byte;
and `nefetch()`'s wait masks the value read, `inb(nec+ds0_isr) &
DSIS_RDC`, as `neput()`'s does, not the port number. With QEMU's
`-netdev user,id=n0 -device ne2k_isa,iobase=0x300,irq=10,netdev=n0`:

	# ifconfig ne0 10.0.2.15 netmask 255.255.255.0 up
	# ifconfig -a
	ne0: flags=43<UP,BROADCAST,RUNNING>
	        inet 10.0.2.15 netmask 0xffffff00 broadcast 10.0.2.255
	# ping -c 3 10.0.2.2
	64 bytes from 10.0.2.2: icmp_seq=1 ttl=255 time=0.000 ms
	64 bytes from 10.0.2.2: icmp_seq=2 ttl=255 time=0.000 ms
	3 packets transmitted, 2 packets received, 33% packet loss

-- the first, most likely, lost while ARP resolved. Roadmap
milestone 4's first step; DNS, routes and `/etc` setup are next.

### Lites against the OSF/1 server: the documented behaviour

**Recorded 13 October 2026, at the maintainer's request.** The
project's aim is that OSF's MK7.3 documents can be followed unchanged
on this system, with Lites in the place of the OSF/1 server. Lites is
kept canonical for Lites and OSFMK for OSFMK; where the two meet, this
is what differs today. Tier 1 (emulator removal, the syscall path,
locking, collocation, the ABI and boot path) is in `docs/roadmap.md`
and §1 above; these are the gaps at the boundary OSF's documents
describe. Sources: MK7.3 Release Notes §2.4.2.1 and §2.5.3,
`bootstrap/bootstrap.template`, and the OSF RI co-location paper,
§5.1 (`docs/provenance/precedent.md`, "What runs first" and "How
Lites gets its arguments from OSF's bootstrap").

- **O-1. OSF's `startup` lines did not work as written. Fixed, 18
  October 2026, in 0107, which supersedes 0106.** `startup startup`,
  `startup: /dev/boot_device/mach_servers/startup -s -a fd0b` and `-k
  startup /mach_servers/osf1_server.INK sd0a` give only flags and a
  root: the OSF/1 server takes "-s (single-user), -a (prompt for root
  device), and a root file system name" (`bootstrap.template`) and
  nothing more. Lites also wanted CMU 3.0's server directory, so this
  project's line was `startup startup hd0a /dev/hd0a/mach_servers`;
  with a root alone Lites built `/dev/hd0a/dev/boot_device/mach_servers`,
  with nothing it returned before reading anything, and with flags
  alone it read past the end of its arguments. 0106 taught Lites to
  find the directory from its own path. That kept a concept the OSF/1
  server does not have: it has no emulator (O-3) and runs `/sbin/init`
  (O-2), so it needs no directory. 0107 takes the OSF/1 server's
  shape instead, under `OSFMACH3` in `server/serv/server_init.c`
  (`imports.md`, E): flags, then a root; a further word, which nothing
  documents, is ignored with a message. The kernel's boot information
  is parsed first the same way, then `bootstrap.conf`'s arguments
  (O-7), so that a root on the line wins; Utah's `-e` and `-i` name
  another emulator or init, now by path. The server directory, its
  two derivations, 0106's `boot_device` renaming and Utah's fixed
  vector for a server started with no arguments ("Boot info
  IGNORED") are gone from the `OSFMACH3` build; Utah's
  `parse_arguments()` stays as it was for the other kernels.
  `mkbootdisk.sh` writes the template's line, `startup startup`.
  Measured from OSF's primary, each to a multi-user login: `startup
  startup`, `startup startup hd0a`, `startup startup -a` (below), and
  the old full line, whose directory word is ignored with "(lites)
  startup argument ignored"; `startup startup -s`, and `-s` at
  `boot:` with the bare line, to the single-user shell; the bare line
  passes the compile-and-install loop.
- **O-2. An OSF-installed root has no `/mach_servers/mach_init`.
  Fixed, 18 October 2026 (0107).** "The OSF/1 server runs /sbin/init.
  There is no need for mach_init as the service server is already
  running in the bootstrap task" (OSF RI co-location paper, §5.1;
  `precedent.md`, "What runs first"). Lites ran
  `<server_dir>/mach_init` and panicked without it, so this project
  kept Lites' link to `/sbin/init`. Under `OSFMACH3` Lites now runs
  `/sbin/init`, its path as `argv[0]` as 4.4BSD's `start_init()` passes
  it, and `mklitesroot.sh` no longer makes the link. Measured: every
  boot above, on a root with `/mach_servers` holding only the servers
  ("(lites): init_program(/sbin/init)").
- **O-3. Lites needs `/mach_servers/emulator`.** OSF/1 MK has no
  emulator; this closes with Tier 1 item 2 and is listed only so that
  `/mach_servers`' contents are on record. Until then its path is
  Lites' compiled default, `/mach_servers/emulator` on the root, and
  `-e` names another.
- **O-4. No root argument meant `hd0a`. Fixed, 18 October 2026
  (0107).** The OSF/1 server falls back to a default root, and Release
  Notes bug 2271 calls its choice "the wrong default root device" when
  it is not the boot partition; Lites hard-coded `default_root[] =
  "hd0a"; /* XXX */`. Now the root, with none on the line, is the
  kernel's boot device, the first word of its `host_get_boot_info`
  reply; with neither, Utah's `hd0a` as before. The "(lites):
  root(hd0a)" printed under the bare line is the kernel's: on i386 the
  kernel names its boot device whenever there is a disk to boot from,
  so the last resort is not reached.
- **O-5. `-a` read differently. Fixed, 18 October 2026 (0107).** OSF:
  `root device: [fd0b]`, with a default (Release Notes, §2.4.2.1).
  Lites asked `[lites root device]? ` while reading its flags, before
  any root was known, and without one. Now it asks after both
  parses, in OSF's words, with the root so far as the default; Return
  keeps it. Measured: `startup startup -a` prints `root device:
  [hd0a]`, and Return boots multi-user on hd0a.
- **O-6. Lites served no bootstrap service to its tasks. Fixed, 19
  October 2026.** The OSF/1 server "implements the bootstrap service
  for its child tasks and is responsible for controlling the
  bootstrap ports of those children ... How an OSF/1 server responds
  is its own policy decision" (co-location paper §5.1). A Lites task's
  bootstrap port is its process port, and OSFMK's programs, which
  start through `libsa_mach` and print through `bootstrap_ports()`,
  got "warning_panic: bad_request_server" and a segmentation fault:
  measured with `host_perf` and `mach_perf`, built from this tree
  (`build -here mach_services/cmds/mach_perf`, which builds clean).
  Now, under `OSFMACH3`, `serv/serv_syscalls.c` answers
  `mach/bootstrap.defs` on that port through OSF libmach's own
  `bootstrap_server()`, as OSF's bootstrap task does, after taking the port's turn by sequence number as every
  request there must (`imports.md`, E). The policy, from the sources:
  `bootstrap_ports()` gives the privileged ports only to privileged
  processes, as OSF's Server Library Interfaces say of
  `mach_privileged_host_port()` and `mach_device_server_port()`, whose
  other route is `task_by_pid(-1)` and `(-2)`: so by that call's test,
  the superuser or group 2 (kmem), and `KERN_NO_ACCESS` otherwise.
  `bootstrap_arguments()` and `bootstrap_environment()` answer empty,
  as `mach_perf`'s own server answers the tasks it makes: OSF's i386
  runtime is built `OLD_COMPAT` (`libsa_mach/AT386/machdep.mk`), so a
  program a Unix server execs takes `argc` and `argv` from its stack
  and never asks, as measured; only a task made with `task_create`
  would. (Proposed first: the exec's own strings, on the reading that
  a second server needs them; the measurement showed it does not.)
  `bootstrap_completed()` is accepted. Message 999999, which
  `bootstrap.defs` keeps only as a skipped "old
  bootstrap_privileged_ports", is not served; OSF's bootstrap task
  does not serve it either. Measured
  from OSF's primary: a `libsa_mach` test program sees its shell
  arguments; as root, 50 rounds of all four calls answer as above, and
  the process exits normally after them, so the port's turns stay in
  step; so they do for `daemon`, not root but put in group 2 on a test
  disk; for `nobody` the program prints nothing, as a refused
  `bootstrap_ports()` leaves `libsa_mach`'s `printf()` without a
  console, and the shell carries on; `host_perf` runs its tests; the
  compile-and-install loop passes.
- **O-7. `-s` at the boot prompt did not reach the server. Fixed,
  18 October 2026.** "-s -- single-user mode (passed to server)", the
  Release Notes say (Boot Options). Measured with OSF's loader:
  `/mach_kernel -s /mach_servers/bootstrap` booted multi-user. Two
  links were missing. The i386 kernel's `parse_arguments()` had no
  `s`; `startup_single_user`, which `machine_boot_info()` turns into
  " -s" for `host_get_boot_info`, was set only by the old boothowto
  convention. And Lites fetched the boot info only when it had no
  arguments, which under OSF's bootstrap task it always has. Now the
  kernel takes `s`, as HP700's port does, and Lites puts the reply's
  flags ahead of `bootstrap.conf`'s (`imports.md`, A and C).
  Measured: the same line prints "(lites) boot flag from the kernel:
  -s" and reaches NetBSD's "Enter pathname of shell or RETURN for
  sh:"; Control-D there goes on to multi-user; a boot without `-s` is
  multi-user as before, and passes the compile-and-install loop.
  The three ways OSF documents, all working: `-s` at `boot:` for one
  boot; `-s` on `bootstrap.conf`'s `startup` line for every boot, as
  OSF's install floppies have it (`startup … -s -a fd0b`); and `-- -a`
  at `boot:`, the bootstrap task asking for each file, to recover from
  a configuration that does not boot.
- **O-8. `default_pager` takes the paging partition.** Now S-2.
- **O-9. `name_server` first in `bootstrap.conf`**, as the template and
  the Release Notes' example have it. It builds since 0046 and starts
  first ("(name_server): started"), and Lites boots after it unchanged.
  `build_world`'s three `build -here` lines for `libservice`,
  `libnetname` and `servers/netname`, commented out as in MkLinux, are
  uncommented since 0051. `mkbootdisk.sh` writes the lines in the
  order its arguments come, so `name_server` goes first.
- **O-10. The Release Notes' procedures assume OSF/1's userland.**
  `disk_setup`, `vtoc`, `newfs`, `net_start`, `mk_install`,
  `/sbin/init.d/inet` do not exist under NetBSD 1.0. The cost of the
  decision in §1, "NetBSD 1.0's binaries as the userland,
  provisionally", recorded so it is visible.

Choices already made at this boundary: 0041, the pid-2 hack off under
`OSFMACH3`, moved Lites toward the OSF/1 server, and on 18 October
0104 (O-7) and 0107 (O-1, O-2, O-4, O-5) did: under `OSFMACH3` Lites
takes the OSF/1 server's arguments and runs `/sbin/init`, the
`bootstrap.conf` line is OSF's template's, and the root needs no
`mach_init` link. 0106 was a step toward this that kept Lites' server
directory; 0107 supersedes it.

### Order of work, and when to self-host

**Recommended 13 October 2026; the maintainer decides.**

1. **Stability first** -- S-2 (configuration) and S-1 (the disk tool),
   both done; S-3 and S-7, done in 0053; S-4, 0060; S-5, 0058; S-8
   (the shell; closed 18 October, as S-9 and S-13). Everything after is tested on top of these;
   a server that dies on a write makes every later result doubtful.
2. **Then the boundary items that are configuration or small**: O-8
   with S-2 (done), O-9 (builds and runs; in `build_world` since 0051), then O-1, O-2, O-4, O-5 and
   O-6, each a contained change in `server/serv/server_init.c` or the
   server's port handling, under `OSFMACH3`. O-6 lets OSFMK's own test
   programs (`mach_perf`) check the system from inside.
3. **Self-hosting after both** -- roadmap milestone 5, and OSF's own
   practice (the Release Notes speak of a "selfhost sup
   collection"). Not before, because:
   - it does not shorten the path to the items above: they are server
     changes, built in minutes by the cross build and tested by booting
     QEMU, where the same edit-build-boot under TCG inside the system
     takes far longer, and a server crash would take the
     development environment down with it;
   - its own prerequisites are open: NetBSD 1.0's `gcc` makes a.out,
     and this kernel and its servers are ELF, so gcc 2.7.2 in its
     OSF/1 i386 ELF configuration and binutils must be built on the
     system first (§1, "The compiler is OSF's line first"); ODE's
     `build`, `migcom` and `config` must be built for NetBSD 1.0; the
     source tree needs room -- a large disk from `mkbootdisk.sh`, or
     `newfs` on the system; and sources need a way in -- the `ne`
     network (milestone 4) or disks written on the host and read by
     `tar` on the system;
   - what it does prove -- `cc` compiling and running a program under
     Lites -- already works (above), so the first step toward it is
     measured, not assumed.

**What happened, 17 October 2026.** Self-hosting came first after all:
the maintainer took the roadmap's milestones in order, and milestone 5
is reached (`docs/status.md`, "Hosting itself"). The O-items in step 2
none is still open but O-3, which closes with Tier 1; O-7, checked
once OSF's boot loader ran (milestone 1, 0079), is fixed, and O-1,
O-2, O-4, O-5 and O-6 with it. They belong to the boundary
between the intermediate goal and Tier 1, and are taken there.

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
this tree had no `.gitignore`; recorded so ours is written with `#`
from the start, as it is (0089).

## 3. Refused

### Only public documents and permitted trees guide the design

The OSF/1 server's source was never released. No source that was not
released for others to use, and none under the GPL, is read for this
project, not even as a specification. The design comes from OSF's
public documents and from the trees `docs/provenance/precedent.md`
permits. Where those are silent, the
maintainer decides. Write from Patience and the Server Writer's Guide.
