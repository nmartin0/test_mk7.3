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
| 1 | `4b29812` | first commit | in base | MkLinux DR3 verbatim, confirmed byte-identical against `slp/osfmk-mklinux`, an independent repository. Ancestor of `dev3` via `f8fee21`. Message is two lines with no provenance, superseded by `f8fee21`'s. |
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
| 20 | `f4a62b6` | i386/hardclock.c: stop GCC rewriting the interrupt frame | adopt reworked | Diagnosis independently reproduced here before the commit was opened, by the same method: a hardware watchpoint on the clobbered slot, three matching writes at different addresses. The remedy differs. `dev` used `__attribute__((optimize("no-optimize-sibling-calls")))` on the function; this tree uses `hardclock.o_CFLAGS` in `conf/template.mk`, which is OSF's own hook used eleven times in that same file, so `i386/hardclock.c` stays byte-identical to the import. Both were built and both remove the poisoning writes. Completeness also differs: `dev` spot-checked a handful of `ivect[]` handlers; this tree derives it from the linked image. See `pristine.md` for the four options and their measured sizes. |
| — | `7a4e86d` | Port CMU's mach_init, notices intact | **decline in advance** | Installs CMU's `mach_init` into `MACH3_ROOT_SERVERS_IDIR` "alongside default_pager and the bootstrap task" — two bootstrap mechanisms where the kernel calls one. `kern/bootstrap.c:288` hard-codes `/mach_servers/bootstrap`. Its findings about LITES never reading `ports[SERVICE_SLOT]` are worth keeping. |

## Standing notes for the walk

Recorded in advance, so the first batch does not rediscover them.

**`94f8e25` is already settled, by the `f4a62b6` investigation.** It
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
