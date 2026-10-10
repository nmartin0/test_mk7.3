# Keeping the source trees pristine

The hardest constraint on this project, and the one with the most
worked precedent behind it.

`osfmk7.3/` and `lites/` are verbatim vendor imports. They stay that
way except where we deliberately change code, and every such change is
visible in one command:

	git diff <import>..HEAD -- osfmk/src/mach_kernel

That path and not osfmk/ alone, because the startup server was added
to osfmk/src/mach_services/servers deliberately and would otherwise
dominate the diff.  The kernel proper is where drift must stay
visible.

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

### The per-target `_CFLAGS` hook, for one object file

`makedefs/osf.std.mk` line 330 composes every compile as

	${${.TARGET}_CFLAGS:U${CFLAGS}}

so defining `<object>.o_CFLAGS` gives one object its own flags.  **OSF
uses this itself**, eleven times, in `mach_kernel/conf/template.mk`:

	MIG_CFLAGS=-Dmig_internal= -DTypeCheck=0
	bootstrap_server.o_CFLAGS+=${CFLAGS} ${MIG_CFLAGS}

Prefer it to a kernel-wide flag when a change is a property of one
file.  Two things it costs, both learned the hard way:

- `${CFLAGS}` must be repeated, because defining the target variable
  suppresses the `:U${CFLAGS}` default.
- It **cannot** be set from `Buildconf.local`.  `setenv` goes through
  the shell, and a name containing a dot is not a valid shell variable
  name -- `dash` discards it silently and the flag never arrives.
  Tested: the build completed with zero occurrences of the flag in the
  log.  It has to be a make variable, so `template.mk` is where it
  goes.

The one use so far is `hardclock.o_CFLAGS`, and the three alternatives
were built and measured before it was chosen:

| option | vendor files | text size | why not |
|---|---|---|---|
| `template.mk` per-object hook | 1 (`conf/template.mk`) | 818,058 | **chosen** |
| `__attribute__((optimize))` on the function | 1 (`i386/hardclock.c`) | 818,058 | a source change where configuration serves; GCC documents the attribute as debugging-only and Linux removed it from their tree for dropping command-line flags. Tested here: under GCC 13.3 it kept `-fno-pic` and `-fno-stack-protector`, so the risk is latent and version-dependent, not present |
| `-fno-optimize-sibling-calls` in `CARGS` | 0 | 821,398 | the only option with no vendor change at all, and immune to any asm-to-C contract the audit missed -- but it disables a legitimate optimization in ~118 functions to fix one, and buries the reason in a flag string |
| hook plus a pointer comment in the `.c` | 2 | 818,058 | defensible; declined because the point of the hook is keeping that file byte-identical, and a comment that exists only to apologise for a makefile undercuts it |

Pristine for comparison, with the sibling call present: 818,090 bytes
of text.

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

## What the first build proved about this order

Every one of the nine settings that make OSFMK build is level 1 --
vendor configuration, through a hook the vendor provides:

| setting | hook |
| --- | --- |
| `SOURCEDIR` | `Buildconf.local`, `lib/libode/builddata.c` |
| `MAKESYSPATH` colon list | `bin/make/parse.c` line 2255 |
| `CARGS` | `Buildconf`'s own `CARGS` line for i386-on-Linux |
| `NO_STRICT_ANSI` | `osf.gcc.mk` line 94 |
| `ANSI_CC`, `TRADITIONAL_CC`, `HOST_CC` | `osf.std.mk` lines 120-131 |
| `LDOPTS` | `conf/AT386/template.mk` line 279, `+=` |
| `AT386_LDFLAGS` | `bootstrap/Makefile` line 37 |

Two of those are narrower than the obvious answer.  `NO_STRICT_ANSI`
drops `-pedantic` alone where a blanket `-Wno-error` would have
disabled every diagnostic; `AT386_LDFLAGS` reaches the one link that
assigns `LDFLAGS` with a plain `=`.

Keeping `-Werror` on everything else is what surfaced eleven genuine
type confusions in the vendor tree.  A blanket `-Wno-error` would have
hidden all of them, and did, in the tree this one is walking.

## One worked case, from this project's own mistake

A scratch shell line during the first build ran

	rm -rf "$REPO/osfmk/export"

to replace a symlink.  `osfmk/export/powermac` is 240 vendor files.
The next `git diff` showed 241 files changed and 40,263 deletions
instead of the expected four, and it was restored with
`git checkout -- osfmk/export`.

The destructive step was in a throwaway command, not in any reviewed
patch, which is exactly where this rule earns its keep: without the
deviation diff as a standing check, that deletion would have ridden
quietly into a commit.

The symlink is `osfmk/export/at386`, never `osfmk/export`.

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
