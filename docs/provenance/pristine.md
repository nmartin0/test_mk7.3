# Keeping the source trees pristine

The hardest constraint on this project, and the one with the most
worked precedent behind it.

`osfmk7.3/` and `lites/` are verbatim vendor imports. They stay that
way except where we deliberately change code, and every such change is
visible in one command:

	git diff <import>..HEAD -- osfmk7.3/
	git diff <import>..HEAD -- lites/

That is the deviation record. "We changed two files" becomes something
a reader can verify instead of a claim in a commit message. It only
works if nothing else ever lands in those directories — not build
output, not generated headers, not notes, not tooling.

## Where things go

	osfmk7.3/      vendor.  Pristine except for deliberate changes.
	lites/         vendor.  Same.
	docs/          everything we write about the work.
	docs/tools/    tools we write or modify.
	docs/reference/  other projects' documents and tools, verbatim.

Nothing we author lands outside `docs/` unless it is a change to vendor
code, and a change to vendor code is the last resort rather than the
first.

Note the departure from `test_mk7.3`, which keeps `build/` and `tools/`
at the top level. That is its convention and it is not wrong; ours
puts them under `docs/` so that the top level is the imports and
nothing else, and a stray file is visible at a glance.

## The order of preference for making a change

Four levels. Take the highest that works.

**1. Configuration the vendor already provides.**

ODE's designed answer is `Buildconf.local`, which `libode/builddata.c`
looks for at `<sandbox_base>/rc_files/<project>/Buildconf.local`. It is
the vendor's own mechanism for per-site settings and it is outside the
source proper.

`osfmk7.3/osfmk/src/osc/Buildconf` already describes an i386 target on
a Linux host, which is exactly this project's configuration. Before
changing anything, read it. This tree was written to be portable across
a.out and ELF, K&R and ANSI, and several assemblers, and it usually has
a conditional for whatever you are hitting.

**2. Build-level change.**

A compiler flag, a rule, a configuration name. `test_mk7.3`'s only
commit against the vendor import is of this kind: `fb0c4d5` adds
`-fno-strict-aliasing` and `-fno-pic`, with `nm` evidence for the
second and a measured 61,844 bytes removed from the server.

**3. A symlink out of the tree.**

Where a build insists on writing inside the source directory, the path
is symlinked to somewhere outside it and listed in `.gitignore`. This
is `test_mk7.3`'s mechanism and its `.gitignore` states the reasoning:

> Build output lives outside the tree (see `build/mksandbox.sh`,
> `MK_BUILD`). The entries below are symlinks or generated files created
> by `build/mksandbox.sh`. The build writes through them, so nothing
> lands inside the repository and `git diff` against the vendor import
> stays empty.

Six paths: `osfmk/obj`, `export/at386`, three `makedefs/*.mk`, and
`rc_files/osc/Buildconf.local`.

`mksandbox.sh` also records why the obvious alternative fails, which is
worth not rediscovering: putting the sandbox in `MK_BUILD` with `src`
pointing back here does **not** work, because ODE computes the sandbox
base by walking up from `src`, resolves the symlink, and escapes into
the wrong tree.

**4. A source change.**

Last. It carries an `AI-ONLY NOTE` comment at the site explaining why
the change is required and what was ruled out, proportionate to the
change — a two-token change dictated by the assembler's grammar does
not need sixty lines of justification. The commit message carries the
evidence; the comment carries enough that someone reading the file in
five years does not undo it.

## Establish it is not a misconfiguration first

**Before editing anything under `osfmk7.3/` or `lites/`, establish that
the build is not simply misconfigured.** `test_mk7.3` records two cases
that cost real time and were both configuration:

- Six `.S` files failed with "bad or irreducible absolute expression".
  `ALIGN` is defined inside `#ifdef ASSEMBLER` in `i386/asm.h`, and the
  `.S.o` rule in `conf/AT386/template.mk` passes `-DASSEMBLER`. Not a
  source bug.
- Symbols came out with a leading underscore. `i386/asm.h` carries both
  a.out and ELF conventions and selects on `__NO_UNDERSCORES__`, which
  `osc/Buildconf` sets for exactly the i386-on-Linux case. Not a source
  bug.

Its `PRINCIPLES.md` §1 states the presumption, and records that it has
been correct every time it was tested: the one genuine source problem
found, `pio.h`, took three separate investigations to establish as
genuine.

## The principle of least surprise

A change should be hard to distinguish from the code it sits in. Match
the surrounding idiom, era and conventions — `RULES.md` 3.2, and
`docs/provenance/conventions.md` for which idiom applies where.

This is why the preference order above is not merely about tidiness. A
configuration change surprises nobody reading the source. A source
change in the wrong idiom surprises every later reader, and the surprise
outlasts the reason for it.

## Verifying it

`docs/tools/checkdev3.sh` is the check, and it is written to catch what
a casual look misses: dotfiles, dot-directories, symlinks, submodules,
empty directories, ignored-but-present files, and file modes against
the vendor import.

Two things it found while being written, both of them the checker being
wrong rather than the tree — `RULES.md` 1.13, suspect your instrument
before the code:

- The vendor import ships exactly **one** symlink,
  `osfmk7.3/osfmk/link/tools -> ../tools/`. A check assuming zero
  reported it as an error.
- `find -type f` does not count symlinks, so a tracked-vs-on-disk count
  was off by one for the same reason.

The script now compares the symlink count against the vendor import
rather than against zero, and counts `-type f -o -type l`.
