# The verdict record: walking dev's 131 commits

`dev3` starts from `f8fee21`, the verbatim vendor import. Everything
`test_mk7.3`'s `dev` branch built on top of it — 131 commits — is
reviewed here, one at a time, and adopted only on its merits.

**This file records every verdict, including the ones that take
nothing.** A list of what was declined, and why, is worth as much as
the code that was kept: without it the next person cannot tell whether
something is absent because it was rejected or because nobody looked.

The commit messages carry the same reasoning in the other direction.
`docs/method.md` §6: a commit says what was wrong, what was measured,
**what was declined and why — the alternative donor, the larger change,
the options this one was chosen from** — and what it does NOT do.

So the two records answer different questions. This file answers "what
happened to dev's commit N". The history answers "why is this line the
way it is".

## The four verdicts

**adopt** — canonical, idiomatic, minimal, and the reasoning holds.
Cherry-picked as it stands, or with the message extended to name the
options it was chosen from where the original did not.

**adopt reworked** — the change is right and the shape is not.
Typically: a source change where configuration would serve
(`docs/provenance/pristine.md` has the order), an invention where a
donor exists, or a citation naming the wrong tree. The commit message
says what the original did and why this differs.

**defer** — correct, but premature for our ordering. Named here with
the tier it belongs to in `docs/roadmap.md`, so it is picked up rather
than lost.

**decline** — not taken. The reason is recorded in full. A decline is
not a judgement on the original work: `dev` was reaching a booting
kernel, and this project's constraints are different.

## The tests a commit must pass to be adopted

From `RULES.md`, and each one has cost somebody time:

- **5.5** one logical change. Two unrelated fixes are two commits.
- **5.6** builds and passes on its own, so bisect works and it can be
  reverted alone.
- **5.9** the measurements are in the message.
- **3.8** it says what it does NOT do.
- **3.1** any deviation from the vendor import is justified by the
  message.
- **1.2** every number is a measurement from a run, not an estimate.

And two from `docs/provenance/precedent.md`:

- every citation names a tree **and** a release, tag or commit.
- nothing outside the six trees on disk is precedent. If an idea came
  from reasoning rather than a donor, it is labelled as this project's
  own invention.

## Method

`docs/method.md` §2: **no commit gets a verdict unopened.** Resemblance
between two commits is not evidence about either. The Lite2 tree marked
five batches complete with commits unopened, grouped by resemblance,
and every time it was caught the skipped commits held something — a
false citation, an unadopted convention, a second `NOPIC`.

So, per commit:

1. Open it. Read the diff and the message in full.
2. Name what it changes and which of the four levels in
   `pristine.md` it sits at.
3. Check the message's claims against the tree — counts re-run, cited
   files opened.
4. Search our own trees for the construct before accepting an outward
   citation (`docs/provenance/audit.md`).
5. Record the verdict here, with the reasoning, before moving on.

Batches are agreed with the maintainer before each run, and the
verdicts are reported before anything is cherry-picked.

## Verdicts

Nothing has been walked yet. The table fills in `dev` order, oldest
first.

| # | commit | subject | verdict | reasoning |
|---|---|---|---|---|
| 1 | `4b29812` | first commit | in base | MkLinux DR3 verbatim, confirmed byte-identical against `slp/osfmk-mklinux`, an independent repository. Ancestor of `dev3` via `f8fee21`. Message is two lines with no provenance, superseded by `f8fee21`'s. **Corrected 12 October 2026:** `slp/osfmk-mklinux` is the import's source, not independent of it, and its last commit carries slp's 2011–2012 changes; the import is not DR3 verbatim. `docs/provenance/precedent.md`, "Where the two imports came from". |
| 2 | `f8fee21` | Import OSF Mach Kernel 7.3 verbatim | in base | 2,998 additions, zero deletions: it adds a second copy rather than moving the first, so `dev` carries the kernel twice from here on. `dev3` keeps one. |
| 3 | `6b72de1` | Add build environment, agent rules, principles | adopt reworked | Tested: its `bootstrap-ode.sh` builds `make` alone, as its message says. Its seven Buildconf claims all verified against our tree. Docs declined — they arrive verbatim in `docs/reference/`. Script folded into `docs/tools/bootstrap-ode.sh`. |
| 4 | `ee6f9fd` | Build the ODE toolset and prepare the sandbox | adopt reworked | Tested: builds six tools, not `md`, exactly as its message says. `ar crl` failure on binutils 2.42 reproduced directly. Folded into the same commit; its `md` deferral is corrected by commit 5. |
| 5 | `971b349` | Complete the FIRST pass | adopt reworked | The `SOURCEDIR` finding is correct and is the single line that turns a dead build into a complete export pass: `rc=2` and "don't know how to make build_all" before, `rc=0` and 206 headers after. Mechanism verified in `lib/libode/builddata.c` lines 100–132. Its "250 headers" is ambiguous — measured here as 255 log lines, 206 files. |
| 6 | `deb86e5` | Drop workon; use build(1) directly | adopt reworked | All four claims verified: `workon(1)`'s self-disclaimer quoted exactly, `build(1)`'s `-sb` and `-rc` at `build.1` lines 112 and 117, `build_world` has zero `SECOND` occurrences, and the `USER`/`SHELL` behaviour reproduced in all three states. Its `-rc` choice is declined — see `pristine.md` level 0. |
| 7 | `c36afed` | build: toolchain on a 64-bit Linux host | adopt reworked | Every structural claim verified. One correction: `ANSI_CC` and friends are consumed under `.if defined()` in `osf.std.mk` and defaulted with `?=` in `osf.gcc.mk`, not "documented hooks in `osf.std.mk`" — the real hook is stronger than claimed. One finding is version-specific: `release` builds fine on GCC 13.3.0 here. |
| 8 | `a253c9b` | i386/pio.h: inw/outw instead of the 0x66 hack | adopt reworked | Correct and compile-blocking. Our version proves it: both forms assemble to `66 ef` and `66 ed`. Rewritten with our own `AI-ONLY NOTE` and the byte proof. |
| 9 | `10e3591` | i386/locore.S: match register widths | **adopt as-is** | Better than what this project would have written. It widens the register and keeps the suffix, preserving the encoding; the instinct here was `movw`, which the byte comparison shows adds a `66` prefix the code never had. Its scan-don't-chase method and its deliberate non-fix of `start.S:354` are both adopted as practice. |
| 10 | `321cbc0` | i386/i386_rpc.c: declare written asm as outputs | adopt extended | The diagnosis is right and sharper than ours — GCC constant-folded the operand and emitted `movl %eax,$0`. Extended here: the indirect `call %1` wants `call *%1`, which `dev` left as a warning. Two further defects of the same class found in the same file and deliberately left, both recorded. |
| 11 | `a5c4cec` | keep zero-init globals out of BSS | **declined** | Reverted by `dev` itself in `091e475`. Nothing to adopt; the reasoning on both sides is worth reading when BSS comes up. |
| 12 | `091e475` | Revert of 11 | **declined** | See above. |
| 13 | `fc55282` | i386/AT386/model_dep.c: clear BSS before consuming boot data | adopt reworked | Already on this branch as `37c62e3`; the verdict was never written, which this row repairs. The ordering diagnosis is exact and was re-derived here: the BSS clear ran three calls after `parse_multiboot` and erased all nine variables it had just written, so `i386_init` computed `last_addr` = 0x100000 and the kernel reported no usable RAM. Paths rewritten for our single copy of the kernel. |
| 14 | `60764ed` | Document the debugging methodology and the bootstrap fork | adopt in part | 467 lines of documentation and `tools/vgadump.py`. The `DEBUGGING.md` material is superseded by what `26c449a` later corrects and by `docs/method.md` §4. `vgadump.py` is declined: we read the screen with QEMU's own `screendump`, which handles both framebuffers without a tool. **`docs/bootstrap-fork.md` is the find**, and all three of its claims were verified here rather than taken — see `docs/deferred.md` §2. It is the reason this gap was worth closing. |
| 15 | `1322e21` | Keep the two variables start.S writes out of BSS | adopt reworked | Already on this branch as `86c4359`; verdict never written. *Reworked in 0073:* the attributes on `kpde` (`intel/pmap.c`), `first_avail` and `mb_info` (`model_dep.c`) are gone and the three declarations are DR3's and slp's again; the host build gives GCC `-fno-zero-initialized-in-bss`, its switch for the GCC 3 change that moved them to `.bss` (`docs/tools/mksandbox.sh`), because gcc 2.7.2.1's OSF/1 ELF target rejects section attributes. The finding the note carried stays: OSF's "set by start.s - keep out of bss" is on `first_addr`, which `start.s` does not write, while `first_avail`, which it does (line 487), has no such label. Its `docs/current-blocker.md` is declined — 259 lines of a splx diagnosis that `26c449a` retracts and `f4a62b6` replaces. |
| 16 | `a21730e` | tools: read both VGA framebuffers, not just 0xb8000 | **declined** | Tooling only. It fixes `vgadump.py` to read the graphics framebuffer as well as the text one. We never adopted `vgadump.py`, so there is nothing to fix; QEMU's monitor `screendump` writes a PPM of whatever mode the guest is in. Recorded because the underlying fact is worth keeping: **this kernel's console is not always at 0xb8000**, so a reader that assumes text mode can report an empty screen on a live machine. |
| 17 | `67a5a5f` | Narrow the splx panic to a corrupted stack slot; add tools/pmem.py | **declined** | Its central finding is wrong and `dev` retracts it itself in `26c449a`: the "corrupted stack slot" was two measurements taken in different boots of a non-deterministic failure, compared as though they came from one. The real defect is in `f4a62b6`, adopted here as `5af8713`. `tools/pmem.py` declined with `vmem.py`, see row 19. Worth reading as the worked example behind `docs/method.md` §4's determinism rule. |
| 18 | `a77f81b` | i386/AT386/model_dep.c: bound the module reads by mods_count | adopt reworked | The fix is right and minimal. Reworked for its citation: `dev` argues from the multiboot specification, an outside document, when this tree's own `i386/multiboot.h` says it twice — line 102 "Valid only if MULTIBOOT_MODS is set" and line 146 "the first of 'mods_count' multiboot_module structures". That makes it kind **A** in `imports.md`, upstream corrected to upstream's own intent, which is the most authentic kind and stronger than what the original claimed. All measurements re-taken here; `dev`'s one-module fault counts do not carry over, see `docs/status.md`. |
| 19 | `26c449a` | Add WORKFLOW.md; record non-determinism and correct the splx diagnosis | adopt reworked | Documentation and one tool, no kernel change. Its `WORKFLOW.md` is declined: this project has `RULES.md`, `docs/method.md` and `docs/provenance/` covering the same ground for different constraints. Its `splx` findings are superseded — it retracts one wrong diagnosis and reaches a second that `f4a62b6` then replaces. Its **method** lessons are adopted into `docs/method.md` §4, three of them re-measured here first: the determinism rule, the segment base, and tail calls hiding the caller. One claim is corrected rather than copied, see below. `tools/vmem.py` and `tools/pmem.py` declined: both exist because `dev` read guest memory through the QEMU monitor, which has separate physical and linear commands. gdb reads linear directly and is already this project's instrument, so neither tool has a job here. |
| 20 | `f4a62b6` | i386/hardclock.c: stop GCC rewriting the interrupt frame | adopt reworked | Diagnosis independently reproduced here before the commit was opened, by the same method: a hardware watchpoint on the clobbered slot, three matching writes at different addresses. The remedy differs. `dev` used `__attribute__((optimize("no-optimize-sibling-calls")))` on the function; this tree uses `hardclock.o_CFLAGS` in `conf/template.mk`, which is OSF's own hook used eleven times in that same file, so `i386/hardclock.c` stays byte-identical to the import. Both were built and both remove the poisoning writes. Completeness also differs: `dev` spot-checked a handful of `ivect[]` handlers; this tree derives it from the linked image. See `pristine.md` for the four options and their measured sizes. |
| 21 | `94f8e25` | i386/AT386/lpr.c: declare curr_ipl as int, matching the assembly | **defer** | Right on every count, and inert here. `i386/pic.c:162` defines `int curr_ipl[NCPUS]` and the assembly reads it with `movl`; `lpr.c:221` declares `extern spl_t curr_ipl[]`, a byte array. But `conf/AT386/files:160` makes `lpr.c` "optional lpr device-driver", `conf/AT386/config.devices:65` has the device commented out (`#pseudo-device lpr`), the generated `lpr.h` is `#define NLPR 0`, and no `lpr.o` exists in the build, so the change alters no byte of the kernel. Its only uses are inside the `lpr_tr` trace macro. The same declaration in `kern/sched_prim.c:2560`, which is built, has its one use inside `#if 0` at line 2580 and is equally dead. Taken up if either file becomes live: a `lpr` driver configured, or that `#if 0` removed. |
| 22 | `c1620a0` | Revive bootstrap_create_old() behind a -o boot flag | adopt reworked, with 23 | The revival is right; the shape is not ours. `dev` kept the GNU Hurd path as the default and made OSF's selectable with `-o`, adding a global, a `parse_arguments()` case and a branch in `startup.c`. Three shapes were put to the maintainer — `dev`'s flag; OSF's path as the only path with the `_old` names left in place, built and booted here; and OSF's path under OSF's own names, built and booted here — and the maintainer chose the last. `bootstrap_create_old()` and `user_bootstrap_old()` become `bootstrap_create()` and `user_bootstrap()` again; the graft's functions of those names go behind `#if 0`. `startup.c` and `misc_protos.h` are untouched, and OSF's own call `thread_start(..., user_bootstrap)` in `bootstrap_create()` resolves to OSF's loader again without being edited. `dev`'s c1620a0 alone left that call reaching the graft's loader, which is row 23. |
| 23 | `856290c` | kern/bootstrap.c: give bootstrap_create_old() its matching loader | adopt reworked, with 22 | The diagnosis is exact and is the evidence for row 22's shape: the graft renamed OSF's `user_bootstrap` to `user_bootstrap_old` and wrote its own under the original name, so OSF's `bootstrap_create_old()`, calling the bare name, reached the graft's loader. `dev` repointed the call at `user_bootstrap_old`; restoring the names makes the original call correct as OSF wrote it, with no edit to the call. Its "found by diffing against OSFMK 6.1" is noted but not relied on: 6.1's `bootstrap.c` is a different design (`copy_bootstrap`, a varargs `build_args_and_stack`), and the evidence used here is inside 7.3. |
| 24 | `8a15d03` | kern/bootstrap.c: give the i386 bootstrap task its argc and argv | **declined** | Its premise was wrong, and `dev`'s own next commit shows it. It adds an i386 arm to `user_bootstrap()` that lays `argc`/`argv` on the new task's stack, because `libsa_mach`'s crt0 was seen to stall in `mach_init` and `host_page_size`, and it says "the RPC machinery is entirely sound". It was not: `80ef3b0`, row 25, finds that GCC collapsed `mig_init`'s loop so every kernel RPC but eight returned `MIG_BAD_ID`. And crt0 does not need the stack: under `OLD_COMPAT` (`libsa_mach/AT386/machdep.mk:34`) it uses `kargv` if the kernel left one and otherwise takes the `else` branch, `__get_arguments()`, which asks the kernel by RPC (`bootstrap_arguments`, answered by this file's `do_bootstrap_arguments()`). This tree's i386 `do_bootstrap_compat()` leaves the stack empty — `esp` at `STACK_PTR`, nothing placed there — so on i386 that RPC is the path the code takes. **Measured here, three ways.** Without the arm, as committed: the task runs `mach_init` and stops in `bootstrap_arguments`, its first read of `.rodata` (`0x08069ca0`) landing in the read-only segment the kernel does not map — row 26's subject. With the arm: it skips that RPC and reaches `alloc_stack+0xff`, where `dev` stood. Without the arm, with row 25's defect and row 26's segment both removed for the test (a compiler flag on `ipc_kobject.o`, and the bootstrap relinked with `-z noseparate-code`): the task reaches its own prompt, "Configuration file (or 'builtin'): /dev/boot_device/mach_servers/bootstrap.conf" — the point `dev` reached at row 26 with the arm in place. The arm is not needed. **First committed as adopted, in `eb5a113`, before this review was done; the next commit removes it.** |
| 25 | `80ef3b0` | kern/ipc_kobject.c: read the routine array through a pointer | adopt reworked | The diagnosis is exact and important: `struct rpc_subsystem` in `mach/rpc.h` declares `routine[1 /* Actually, (start-end+1) */]` with `arg_descriptor[]` after it, GCC 13 takes the `[1]` at its word, and `mig_init()`'s loop over each subsystem's routines runs once. `mig_buckets` held 8 of 1,024 entries; every other kernel RPC answered `MIG_BAD_ID`. Reproduced here with gdb against the running kernel: 8 before, 196 after, the same two numbers `dev` reports. **The remedy differs**, chosen by the maintainer from three: `dev`'s source change to the one loop; OSF's per-object `_CFLAGS` hook for `ipc_kobject.o` alone; or `-fno-aggressive-loop-optimizations` in `CARGS`, taken. The class is wider than the loop. Building the whole tree with and without the flag, `objdump -d` differs in 23 objects — 19 in the kernel, among them `rpc.o`, `subsystem.o` and `exception.o`, which index the same arrays, and 4 in the libraries, `mach_subsystem_join.o` and `mach_host_user.o` in each of `libmach` and `libmach_maxonstack`. No vendor file changes; kernel text grows by 20 bytes. Kept beside `-fno-strict-aliasing` in `docs/tools/mksandbox.sh`, with the reason. |
| 26 | `11ecb84` | kern/bootstrap.c: map read-only PT_LOAD segments | adopt reworked, both ways | The diagnosis is right: `do_bootstrap_compat()` matches `p_flags` exactly against R+X and R+W, so today's read-only segments — the ELF headers and `.rodata` — are never mapped, and the task faults on `.rodata`. Two remedies were put to the maintainer, who chose both. **The link:** `LDARGS="-z noseparate-code"` in `docs/tools/mksandbox.sh`, ODE's site hook for links (`osf.std.mk:342`), so our programs link to R+X and R+W; it reaches the kernel's link too, which then has two segments and boots the same. **The loader:** a read-only arm in `do_bootstrap_compat()`, for programs linked any other way. `dev`'s arm also required `p_vaddr > e_entry`, to leave out the ELF header segment, because "mapping that segment as well regresses the boot to zero user-mode entries". **That was not the cause, and it is found here.** `regions[]` held four entries; four PT_LOAD segments plus the stack make five, and nothing checked the bound. Measured: with four entries and the header mapped, a fault storm of over a million kernel page faults; with five, the task reaches its prompt; with the header mapped and `.rodata` left out, four regions in all, the task runs until it reads `.rodata`. So the table is now `NREGIONS`, five, and the loader tests it — with four, it prints "Too many PT_LOAD regions, ignoring one" and the kernel stays up. The header segment is mapped like any other read-only segment, with no heuristic. The graft's `exec_load()`, behind `#if 0`, is not changed. `dev`'s rows 30 and 32 take the same question to `bootstrap/elf.c` and to the header segment's messages. |
| 27 | `419e109` | i386/AT386/model_dep.c: populate the environment from the command line | **defer** | Not needed on the route taken. The maintainer chose an IDE disk over `dev`'s floppy (row 29), and the kernel's compiled-in boot device, `hd0s1`, the first DOS partition of the first IDE disk, finds it with no environment at all. What the row restores is real: OSF's boot loader handed the kernel `env_start` and `env_size` — two of the words in the "New calling convention" in `i386/AT386/asm_startup.h` — and `machine_init()` reads `BOOTDEV`, `BOOTUNIT` and `BOOTPART` from them; the multiboot loader that replaced OSF's passes no environment, so those overrides cannot be set. One part of its reasoning is wrong: the `#if 0` block in `do_bootstrap_compat()` it calls "the code that did" fill the buffer builds the bootstrap *task's* environment — `rootdev_name=`, `startup_name=`, `startup_flags=` — not the kernel's. Taken up when a boot device other than `hd0s1` is wanted. |
| 28 | `b5b7a0e` | i386/AT386/fd.c: drain the reset interrupt in rstout() | **defer** | A floppy-driver defect, measured by `dev` to stop every floppy command after the reset; the floppy is not used on the route taken. Taken up with the floppy. |
| 29 | `c8727a9` | The bootstrap task reads its config off a minix floppy | **declined** | Superseded by `docs/tools/mkbootdisk.sh`, an IDE disk image, rather than a hand-written file system writer (`tools/mkminix.py`). First written for ext2; since the commit after, UFS on OSF's vtoc layout, the file system OSF's bootstrap was written for — the minix reader is GPL in part, a finding `dev` itself makes at `19b0048`, and the bootstrap task no longer builds it (`docs/provenance/precedent.md`, "What OSF booted from"). Its central finding, that the floppy "runs at about 26 seconds per read" and "nothing was ever hung", is right as a measurement; the cause, found here, is QEMU's TCG taking the kernel's interrupts late, not the floppy — `docs/method.md` §4, "Slow is not hung". |
| 30 | `ca2b48d` | bootstrap/elf.c: accept read-only PT_LOAD segments | **defer** | The same question as row 26, in the bootstrap task's loader for the servers it starts. Our servers are linked with `LDARGS=-z noseparate-code` and have no read-only segments, so `default_pager` loads unchanged. It becomes needed for a server linked outside `osf.prog.mk` — Lites, whose own link does not read `LDARGS` — and is taken up then, with row 32's part for the same file. *13 October 2026:* Lites' link now reads `LDARGS` -- `lites/makefile` gives it to the linker it wraps -- so Lites loads unchanged too, and the row stays deferred. Measured first without it: "ELF: Unknown program header flags 0x4" four times, and the startup task terminated. |
| 31 | `cde967c` | MILESTONE: name_server and default_pager load and start | **adopt in part** | The milestone is reached here by the other route: from the IDE disk, under `qemu -icount shift=auto`, the screen ends "(bootstrap): started / (default_pager): started" in under 15 seconds, against `dev`'s 34 minutes by floppy. `name_server` is not on the disk yet: it does not build here, stopping on `-Werror` with GCC 13's `-Wtraditional` and `-Wstringop-overflow` warnings, which is the next item. Its `tools/mkminix.py` changes are declined with row 29. |
| 32 | `7adc430` | Skip the ELF header segment silently in all three loaders | **defer** | Of its three loaders: `do_bootstrap_compat()` now maps the header segment like any read-only segment (verdict 26), so there is nothing to skip; `exec_load()` is the graft's, behind `#if 0`; `bootstrap/elf.c` is taken with row 30. |
| — | `7a4e86d` | Port CMU's mach_init, notices intact | **decline in advance** | Installs CMU's `mach_init` into `MACH3_ROOT_SERVERS_IDIR` "alongside default_pager and the bootstrap task" — two bootstrap mechanisms where the kernel calls one. `kern/bootstrap.c:288` hard-codes `/mach_servers/bootstrap`. Its findings about LITES never reading `ports[SERVICE_SLOT]` are worth keeping. |

## Standing notes for the walk

Recorded in advance, so the first batch does not rediscover them.

**`dev`'s `DEBUGGING.md` has one claim that is half true, and the
half that is false costs time.** It says memory reads "work at either
address, since both map to the same physical page". Measured here, in
one run of one binary: stopped in `parse_multiboot`, `0x001d6e4c`
reads correctly; stopped in a timer interrupt, `0x001c9ff4` returns
"Cannot access memory". The low identity mapping is up early and gone
once the kernel's page tables replace it. A rule that works on first
use and silently stops working is worse than no rule, because the
failure presents as a corrupt pointer. `docs/method.md` §4 carries the
corrected version: always add the base.

**`94f8e25` is already settled, by the `f4a62b6` investigation** —
and deferred at row 21, as this note predicted. It
declares `curr_ipl` as `int` in `i386/AT386/lpr.c`, matching
`i386/pic.c:162`'s `int curr_ipl[NCPUS]` against that file's
`extern spl_t curr_ipl[]`. The diagnosis is right and the fix is
right, but **`lpr.c` is not in the AT386 PRODUCTION configuration**,
so nothing here compiles it. The same wrong-width declaration exists
in `kern/sched_prim.c:2560`, which *is* configured -- but its only use,
at line 2580, sits inside `#if 0`. So the construct is dead in both
places. Expect `defer`, not `adopt`, when the walk reaches it; it
becomes live only if either file is configured in.


**`dev` and `dev3` have different goals.** `dev` was reaching a booting
microkernel with Lites on top, and took the shortest honest path.
This project is building a single server and keeps the imports
pristine. A commit that was right for `dev` can be wrong here without
either being a mistake.

**`dev` carries the kernel twice.** `osfmk/` from the root commit
`4b29812` and `osfmk7.3/osfmk` from `f8fee21`, byte-identical. `dev3`
keeps one. A commit touching the redundant copy needs its paths
rewritten, and that rewriting is "adopt reworked", not "adopt".

**`dev` has no `lites/` tree.** Its Lites work is tooling —
`tools/lites/build-lites.sh`, `lites-compat.h`, `lites-osfmk73.patch`
(1,414 lines across 29 files) and `mig-shim.sh` — applied against an
external checkout. We carry Lites as a vendor import instead, so those
commits become changes to `lites/` or to `docs/tools/`, never a patch
file applied at build time.

**The 29 files that patch touches overlap the ones Tier 1 rewrites.**
`server/serv/user_copy.c`, `serv_syscalls.c`, `server_init.c`,
`server_exec.c`, `xmm_interface.c` and `vn_pager_misc.c` are all in
both sets. Adopting the patch's changes to those files and then
rewriting them is work done twice; the walk should notice when it
reaches them and say so rather than discovering it later.

**`dev`'s single vendor-import change is a build change.** `fb0c4d5`,
`-fno-strict-aliasing` and `-fno-pic`. That is level 2 of four in
`pristine.md` and the shape we want. Whether it is still needed with a
pristine `lites/` is a question the walk must ask rather than assume.

## slp's eighteen commits

**Added 13 October 2026.** The kernel import is `slp/osfmk-mklinux` at
its last commit, not MkLinux DR3 (`docs/provenance/precedent.md`,
"Where the two imports came from"). Its first commit, `0f6f7e6`, with
`i386/start.s~` as `start.s`, is DR3 as far as the MkLinux project's
own CVS can check it, and is the baseline these verdicts are against.
The maintainer's decisions of 12 October govern them: multiboot stays,
as a second way in beside OSF's boot loader; the GNU Hurd graft goes.

| commit | subject | verdict | reasoning |
|---|---|---|---|
| `18c9d31` | assembler files renamed `.s` to `.S`; comments; a multiboot entry in `start.S`; `bts`/`btr` widened | **reverted**, two parts kept | DR3's file names and text restored: `template.mk` copies each `.s` to `.S` and preprocesses it (lines 310-319), and the comments slp rewrote -- 1,232 lines -- are the System V assembler's `/` comments, which GNU as reads when built for i386-elf (`docs/tools/mkbinutils.sh`). The kernel's disassembly is unchanged and the bootstrap task is identical byte for byte. Kept: the multiboot header and the copy of the multiboot information in `start.s`, with `444954a`, as the second way in; and `%dl` to `%edx` in `i_bit_set` and `i_bit_clear`, which no current GNU as accepts, with a note at the site |
| `81429d0` | the old `.s` files removed | **reverted** | with `18c9d31` |
| `0d8ebab` | PCI disabled; `tu` and `fta` commented out; `.S` names in `conf/AT386/files` and `template.mk` | **kept** in `config.devices`, **reverted** elsewhere | Re-tested: PCI is not broken, it is missing. With DR3's `pci 2` the build stops on `device/dma_support.c`, then `busses/pci/pci.h`; with them, `ncr810` needs `scsi_53C810_hdw.c`, and `tu` and `fta` need `if_tu.c` and `if_fta.c`. None is in DR3 or the MkLinux CVS tree. `config.devices` keeps `pci 0` and the three lines out, with a note at the site; `ncr825`, already 0, and every line of `conf/AT386/files` are DR3's again, and the kernel is unchanged. The `.S` names are reverted with `18c9d31` |
| `2913827` | `register` to `register int` | pending | a toolchain adaptation, kept |
| `f4fa636` | `link/tools`; `template.mk` line; two `register int` | pending | the `template.mk` lines are reverted with `18c9d31`. The `LOCORE_DEPS` line was taken to fix a DR3 defect, a missing backslash and `386/cswitch.S`; it fixes slp's own `0d8ebab`, which introduced both while renaming the files, and DR3's line is correct. The two `register int` go with `2913827`. `link/tools` is settled: DR3's tarball has it as a 13-byte file reading `link ../tools`, a symbolic link the archive flattened; the MkLinux CVS checkout has the link, `../tools`, and slp's `../tools/` is now spelt as CVS spells it |
| `4df2e7d` | `config` and `migcom` sources; built objects and binaries | **replaced** | the sources are MK6.1's of 1995; OSF's 1999 sources from the MkLinux CVS repository take their place (kind F), and the objects and binaries, and the copy in `tools/i386/i386_linux/hostbin`, are deleted. `docs/tools/mkhostbin.sh` builds the two programs into the sandbox's `tools_base` by DR3's `hostbin/README` procedure; every MIG stub comes out the same |
| `364472d` | `assym.s` to `assym.S` | **reverted** | with `18c9d31` |
| `444954a` | multiboot; an ELF loader in `do_bootstrap_compat()` | pending | kept as the second way in; OSF's `loader_info` code restored beside it in milestone 1 |
| `7e81018` | `hd.c`: no recalibration, "hangs with qemu" | pending | re-tested under a BIOS boot |
| `9335f94` | kernel-loaded addresses `0xE0000000`-`0xFFFFFFFF` | **reverted** | DR3's `0x0c000000`-`0x1fffffff` restored, with `63279d3`'s second change to the same lines. These are the addresses collocated servers load at; nothing here needs another range |
| `090a2dc` | `libsa_mach` assembler comments | **reverted** | with `18c9d31`; the six files are DR3's again |
| `af5babc` | `i386/multiboot.h`, `mach/elf.h` | kept | the multiboot way in needs them; `mach/elf.h` is OSF's `bootstrap/elf.h`, byte for byte |
| `ebdad63` | `ext2_dir_entry` for modern ext2 | **reverted** | ext2fs is not built (verdict for `772f3ca`); DR3's header restored |
| `8105dd7` | `parse_arguments()` | kept | part of the multiboot way in |
| `63279d3` | the GNU Hurd graft | **removed** | `kern/boot_script.[ch]` deleted, and every graft function in `kern/bootstrap.c` with `set_user_regs()`, `build_args_and_stack()`, `exec_start` and `exec_size`; `parse_multiboot()` reads one module. `kern/bootstrap.c` now differs from DR3 only by `444954a`'s loader and verdict 26. The start.S flag change stays with multiboot |
| `55c7c1c` | the Linux server's Makefiles | not ours | `mklinux/`, not in the import |
| `44e8969` | `usimple_lock_held()` checks commented out | **reverted** | DR3's restored; they run only under `USLOCK_DEBUG` |
| `d4d387a` | Page Global Extensions | **reverted** | not OSF's; DR3's `proc_reg.h`, `pmap.[ch]` and `model_dep.c` lines restored |
