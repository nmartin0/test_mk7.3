# Workflow

How work gets done on this project. Read this before `DEBUGGING.md`;
that file tells you how to find a bug, this one tells you what to do
with it once you have.

Written for an AI agent. The maintainer is the only person who commits.
You never push, never commit to their repository, and never assume a
change has landed until they say so.

---

## The loop

One bug at a time, in this order:

1. **Run the kernel in QEMU.** It breaks.
2. **Diagnose why it breaks.** Measure; do not guess. `DEBUGGING.md`
   covers the instruments and their traps.
3. **Propose a fix and stop.** Say what you would change and why.
   Do not write it yet.
4. **Discuss.** The maintainer may accept, redirect, or ask for more
   evidence. Redirection is common and is not a failure.
5. **Write the fix**, build it, and verify it does what you claimed.
6. **Send a patch.** The maintainer applies it, pushes, and tells you.
7. **Pull from the remote** and go back to 1.

Step 3 is the one that is easy to skip and the one that matters most.
Several fixes on this branch had to be withdrawn because they were
written before the diagnosis was finished. The cost of proposing first
is one message; the cost of a withdrawn patch is an entire round trip
plus a correction commit in permanent history.

## What may be changed, and when

The vendor import is OSF's code. It is not ours to improve.

**Change OSFMK source only when it blocks the kernel.** "Blocks" means
one of:

- the toolchain refuses to produce an object or a binary, or
- the kernel does not run correctly and the fix is in that file.

It does not mean the code is ugly, non-portable, or would be written
differently today. It does not mean a scan found other instances of the
same pattern elsewhere — fix the one that blocks, note the others.

**Prefer configuration over source.** This tree was written to be
portable across a.out and ELF, K&R and ANSI, and several assemblers. It
usually already has a conditional for whatever you are hitting; the flag
is simply not being passed. Read `osfmk7.3/osfmk/src/osc/Buildconf`
before touching any build setting — it is OSF's own ODE configuration
and already supports an i386 target on a Linux host.

**But do not over-apply that preference.** A global compiler flag that
rescues one variable by relocating every object in the kernel is not
"configuration over source", it is a bigger change wearing a smaller
hat. That exact mistake was made and reverted on this branch:
`-fno-zero-initialized-in-bss` fixed the symptom, moved ~20 KB of
objects, and silenced the console. A two-line source change with a note
was the correct fix. Judge by blast radius, not by which file the change
lands in.

## Before proposing anything: enumerate

Find every instance in the same class before you propose a fix for one
of them.

This is the single most productive rule in the project and the one most
often skipped. Worked examples from this branch:

- A fix was proposed for `mb_info` being wiped by the BSS clear. It was
  wrong: `parse_multiboot()` writes **nine** variables and only one had
  been looked at. Listing all nine revealed the real defect — the clear
  ran three calls *after* the function whose output it was erasing. An
  ordering bug, fixed by moving one statement.
- The assembler rejected two operands in `locore.S`. Fixing those two
  would have produced a second failure immediately. Scanning every
  assembly source found eight, of which five needed fixing and one
  deliberately did not (it is inside `#if NCPUS > 1`, which this
  configuration does not compile).
- `start.S` stores to exactly five C symbols. Enumerating them showed
  two needed `.data` placement, two were `NCPUS > 1` only, and one was
  already safe. Without the enumeration, the second bug would have
  surfaced as a mysterious failure days later.

A scan is cheap. A withdrawn patch is not.

## Research, do not pattern-match

When something old fails on a modern toolchain, find out *why the
behaviour changed*, and say so in the commit message. "GCC does X now"
is not an explanation. "`-fzero-initialized-in-bss` arrived in GCC 3.x
and is on by default; under GCC 2.7 an explicitly zero-initialized
global went to `.data`" is.

Sources worth consulting, in rough order of authority:

- The tree's own comments. OSF frequently documented the hazard. Three
  separate bugs on this branch were pre-announced by comments like
  `/* set by start.s - keep out of bss */` and
  `/* must be in .data section */`.
- The compiler or assembler's own documentation for the option involved.
- The generated code. `objdump` settles arguments that reading cannot.
- Other Mach-lineage kernels, for design questions only — see the
  licensing section below before doing this.

## Licensing

The vendor tree is MIT/X11. Keep it that way.

- **Never copy code from a GPL source into this tree.** GNU Mach and
  Linux are both GPL-2 and both are tempting references for i386 and
  Mach questions.
- The same applies to **XNU and Darwin**, whose `osfmk/` subdirectories
  are descended from this very code and are therefore the closest
  available reference for how OSF Mach was carried forward. Early XNU is
  APSL, which is not compatible with this tree's permissive terms. Read
  it, never copy from it, exactly as with GNU Mach. `xnu-123.5` is the
  earliest tag; `xnu-1456.1.26` has the clearest record of the 64-bit
  port, for if this tree ever grows 64-bit support.
- Consulting them to understand a *design* is fine; copyright protects
  expression, not method. Describing another program's architecture in a
  comment is a statement of fact, not a reproduction.
- **Diagnose from this tree's own evidence first**, and say so. On this
  branch the BSS-ordering bug was found from the kernel's own printed
  output, `readelf`, and OSF's own comment; GNU Mach was consulted
  afterwards only to check the direction against established practice.
  That ordering matters if anyone ever asks.
- Before sending a patch that was informed by reading another project,
  **check mechanically** that no line was copied. Normalise whitespace
  and compare your added lines against the file you read.
- Never remove or alter an existing copyright notice. Never add ours to
  a vendor file. Never assert an SPDX licence identifier the maintainer
  has not chosen.

## Source changes carry their reasoning

Every change to a vendor file gets an `AI-ONLY NOTE` comment at the
site, explaining why the change is required and what was ruled out. The
commit message carries the evidence; the comment carries enough that
someone reading the file in five years does not undo it.

Keep them proportionate. A two-token change dictated by the assembler's
grammar does not need sixty lines of justification.

## Commits

One logical change per commit. The subject line is imperative and names
the file or area. The body explains **why**, with measurements.

A commit message on this project should answer:

- What was the symptom, in the kernel's own words where possible?
- What is the mechanism, with addresses, values, and instruction
  sequences?
- Why is this the right fix, and what was rejected?
- What measurement proves it works?
- What is explicitly *not* fixed here?

Intermediate commits in a porting sequence will not produce a bootable
kernel — each only clears the blocker in front of it. That is expected.
What matters is that each commit is one complete, self-justifying
change.

**Reverts.** If a pushed commit turns out to be wrong, use `git revert`
and explain why in the revert message. Do not fold the undo into the
replacement commit; that hides the fact that something was tried and
failed, and the next agent will try it again.

## Proving a fix is necessary

Never claim a change is required without removing it and watching the
failure return.

For build fixes: revert the change, rebuild, confirm the build fails.
For runtime fixes: revert the change, boot, confirm the regression.
Always include the positive control — the configuration with everything
applied — because a table of failures with no success row proves
nothing.

When a new bug is found that could have caused earlier symptoms,
**re-test the earlier fixes against it**. On this branch all five prior
fixes were re-verified after a large bug was found, each reverted
individually with the new bug held out of the way. All five survived,
but the check was the point.

## Patches

```sh
git format-patch <base>..HEAD --stdout > NN-YYYY-MM-DDxx.patch
```

Generate against **the commit the maintainer last confirmed pushed**,
not your own `HEAD~1`. The two diverge the moment a patch is questioned
rather than applied, and `git am` fails on a single line of mismatched
context.

**Dry-run every patch before sending it.** The repository is public.
Clone it fresh, apply there, build there, and re-measure there:

```sh
git clone <remote> /tmp/verify
cd /tmp/verify && git am /path/to/the.patch && <build> && <measure>
```

A failed `git am` leaves `.git/rebase-apply` behind, and **every
subsequent `git am` fails until `git am --abort`**, with an error that
does not mention the real cause.

**Generating a patch and handing it over are two separate actions.**
Writing the `git am` command in your message is not the same as
attaching the file. This was got wrong on this branch: the patch was
built, verified, described, and never sent.

**Check the remote before sending.** A dry-run clones the remote *before*
the maintainer applies anything, so it always tests the pre-patch state
and cannot tell you a patch has already landed. Compare the remote HEAD
against the patch's expected base immediately before handing it over.

An already-applied patch fails with a plain conflict:

```
error: patch failed: DEBUGGING.md:243
error: DEBUGGING.md: patch does not apply
```

Nothing in that message suggests duplication, and the instinct is to go
looking for a corrupted patch. Check `git log --oneline -1` on the
remote first; if the commit is already there, the correct action is
`git am --abort` and nothing else.

**Check your commit actually happened.** A fresh clone has no git
identity configured; `git commit` fails, and `git format-patch -1` then
cheerfully packages the *previous* commit. The dry-run catches this.

## Talking to the maintainer

- **Lead with the finding, not the narrative.** What broke, what the
  evidence is, what you propose.
- **Say what you measured and what you inferred**, and keep the two
  visibly separate.
- **Report corrections prominently.** If a previous claim was wrong, say
  so plainly and early, including in the commit message if it is already
  in history. On a long debugging effort this happens often; the value
  of the record depends entirely on it being honest.
- **Do not pad with reassurance.** "Everything is working well" is not
  information.
- **When you are past the point of being reliable, say so.** Six or more
  instrument errors accumulated in one session on this branch, several
  of which produced confident wrong answers. Noticing that and saying it
  is more useful than another round of degraded work.
- Numbers, not adjectives. Not "mostly compiles" but "206 of 206
  objects, kernel 1,021,600 bytes". Not "the encodings are equivalent"
  but `66 ed` / `66 ed`.

## Keeping the documents current

Four files carry the project's state. Update them as part of the work,
not afterwards:

| file | holds |
|---|---|
| `WORKFLOW.md` | this file — how work is done |
| `DEBUGGING.md` | how to find out why the kernel misbehaves |
| `docs/current-blocker.md` | the live diagnosis, with eliminated hypotheses |
| `docs/bootstrap-fork.md` | an open architectural decision, deferred |

`docs/current-blocker.md` is the most important one to keep honest. It
should always record **what has been ruled out and why**, so the next
agent does not re-run a disproved experiment. When a hypothesis in it is
disproved, correct it in the same commit that disproves it — a document
asserting three things that are now known false is worse than one that
is merely incomplete.
