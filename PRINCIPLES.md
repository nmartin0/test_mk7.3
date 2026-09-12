# Principles

Why the decisions here are what they are. `AGENTS.md` is the *how*; this
is the *why*. Every principle below is backed by a decision this project
actually made, not copied from a generic list.

---

## 1. Minimal deviation from upstream, made mechanical

The vendor import is commit one, verbatim. That is not tidiness — it
makes `git diff <import>..HEAD -- osfmk7.3/` a complete and checkable
deviation record. "We changed two files" becomes something you can
verify in one command instead of a claim in a commit message.

The corollary is that a build failure is presumed to be a
misconfiguration until proven otherwise. That presumption has been
correct every time it was tested so far: `ASSEMBLER`, `__NO_UNDERSCORES__`
and the ELF/a.out split were all already handled by the tree, and the
one genuine source problem found (`pio.h`) took three separate
investigations to establish as genuine.

## 2. Verify directly; never assume

Before writing code against a behaviour, confirm it: read the real
header, run the real tool, compare the real bytes.

This project's clearest case: `.byte 0x66; inl` was replaced with `inw`
only after assembling both forms and confirming both produce `66 ed`.
The reasoning "the prefix hack emulates the 16-bit instruction" was
plausible and was not treated as sufficient.

The counter-case is equally instructive. A filename-level survey of
which files could be recovered from other Mach releases produced a
confident figure that turned out to depend entirely on the matching
method. Redone at line level and again with name-agnostic n-gram
matching, the answer held — but it had been asserted before it was
earned.

## 3. The kernel first, because servers are cheap and kernels are not

In a microkernel the OS servers are ordinary user tasks. A server crash
is a task that died: restart it, attach gdb to it, rebuild it without
rebooting. None of that is true of the kernel, where a bug is a triple
fault with no diagnostics unless the debugging apparatus already works.

So the ordering is forced rather than chosen: kernel, serial console,
gdb stub, and only then anything else.

## 4. Stay true to form

The build uses real ODE, OSF's own rule set, and OSF's own `Buildconf`,
rather than a shell driver written from scratch. A hand-rolled driver
reached 178 of 203 objects and was abandoned anyway, because it had
quietly diverged from the real `.S` pipeline in a way that would have
cost assembly-level debug information — exactly what is needed at the
first milestone.

Reimplementing a build system is a good way to rediscover its decisions
badly. `osc/Buildconf` already describes an i386 target on a Linux host,
which is precisely this project's configuration. We are reviving a
supported path, not pioneering an untried one.

## 5. Shim alongside; never edit across lineages

Code from another Mach lineage — Utah/UK Mach, CMU MK83, Lites — never
edits an OSFMK file. It lands in a new file that sits beside the OSFMK
one, carrying both upstream copyright notices and an `AND` licence
expression. Where a shim needs an OSFMK-side hook, the hook is the
smallest possible addition and it appears in the deviation diff with its
justification.

This keeps provenance legible per file, which matters here more than in
most projects: the material in play carries at least five distinct
permissive grants (OSF, CMU, Intel, Olivetti, Arizona) and mixing them
carelessly would make the result impossible to license honestly.

## 6. Licensing is preserved, never rewritten

Existing copyright notices stay exactly as found. SPDX identifiers are
added alongside, never in place of them. This follows SPDX's own rule
that copyright notices are outside the scope of short-form identifiers.

Our own copyright is added only where we contribute original expression.
A notation change dictated by the assembler is not original expression;
a build driver or a shim layer is.

Identifiers are verified against the SPDX licence list before use. The
OSF and CMU texts are permissive but are not plain MIT, and guessing at
an identifier would be worse than omitting one.

## 7. POSIX where we write it

Anything this project writes is POSIX `sh` — no bashisms, no GNU-only
utilities or flags. The reason is not portability for its own sake: if
this system ever stands on its own feet, it must be able to build
itself, and a Mach host running a 4.4BSD-derived personality will not
have GNU userland.

ODE itself does not violate this. It is a DeBoor pmake derivative in
portable C with per-architecture directories already present, including
`BSDARCH` — so bootstrapping it on a future Mach host is a step, not a
barrier. An earlier version of this principle wrongly concluded that ODE
should be dropped; that was corrected once `ode4linux` showed the tools
build cleanly on a modern host.

## 8. No speculative code

Nothing is built for a need that is not concrete yet. There is no QEMU
script in `build/` because there is no kernel image to boot. There is no
build driver because ODE is the build driver. Tooling arrives when the
milestone that needs it does.

## 9. An honest account of what is open

`AGENTS.md` carries the current build state, including the failures and
their causes, so a session starting cold inherits the findings instead of
rediscovering them. The two traps recorded there — MIG headers shadowing
source headers, and `ln -sfn` creating a link inside a directory — each
cost real time and each looked like success while failing.

When something cannot be verified, say so plainly rather than asserting
it. The `i386_rpc.c` inline-asm failure is recorded as undiagnosed
because it is.
