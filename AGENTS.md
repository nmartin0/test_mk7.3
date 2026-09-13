# AGENTS.md

Operational rules for AI agents working on this tree.

Deliberately short. This holds only what you would get **wrong** without
being told. Everything else — what the code does, how Mach works — read
from the source. `PRINCIPLES.md` is the *why* to this file's *how*.

## What this repository is

A revival of OSF Mach Kernel 7.3 (MkLinux DR3) for i386, built on a
Linux host and run under QEMU. The goal is a booting microkernel, then
servers on top of it.

`osfmk7.3/` is a verbatim vendor import. `build/` is the only code we
write.

## The prime directive: minimal deviation

`git diff <vendor-import>..HEAD -- osfmk7.3/` is the complete record of
what we have changed from upstream. It is expected to stay small, and
every hunk in it must be justified by a commit message.

**Before editing anything under `osfmk7.3/`, establish that the build is
not simply misconfigured.** This tree was written to be portable across
a.out and ELF, K&R and ANSI, and several assemblers. It usually has a
conditional for whatever you are hitting. Two examples that cost real
time before the switch was found:

- Six `.S` files failed with *"bad or irreducible absolute expression"*.
  Cause: `ALIGN` is defined inside `#ifdef ASSEMBLER` in `i386/asm.h`,
  and the `.S.o` rule in `conf/AT386/template.mk` passes `-DASSEMBLER`.
  Not a source bug.
- Symbols came out with a leading underscore. Cause: `i386/asm.h`
  carries both a.out and ELF conventions and selects on
  `__NO_UNDERSCORES__`, which `osc/Buildconf` sets for exactly the
  i386-on-Linux case. Not a source bug.

**Read `osfmk7.3/osfmk/src/osc/Buildconf` before touching build
settings.** It is OSF's own ODE configuration and it already supports an
i386 target on a Linux host. `build/env.sh` is derived from it, line by
line, with the one deliberate departure documented in place.

## Build

```sh
export ODE4LINUX=~/ode4linux
sh build/bootstrap-ode.sh     # once: builds the ODE toolset
sh build/mksandbox.sh         # once: prepares the sandbox
```

OSFMK is built by ODE's `build` front end, which reads
`osfmk7.3/osfmk/src/osc/Buildconf` and derives the environment from it.
`build/env.sh` exists only as a reference transcription of Buildconf and
is NOT the supported path -- see "Where the work stands".

Host packages: `gcc`, `gcc-multilib`, `binutils`, `libc6-i386`,
`qemu-system-x86`, `gdb`.

`libc6-i386` is needed only to run the prebuilt `migcom` and `config`
under `osfmk7.3/osfmk/tools/i386/i386_linux/hostbin`. Building those two
from their in-tree source is a milestone, not a prerequisite.

## You do not push

No credentials. Produce a patch and hand it over:

```sh
git format-patch <base>..HEAD --stdout > /mnt/user-data/outputs/<n>-<date><suffix>.patch
```

Clear the outputs folder first so a stale patch cannot be applied by
mistake. Dry-run onto a fresh branch from the remote before presenting.
Check `grep -c '^From ' <patch>` matches the number of commits you made.
Then give the apply commands, and say what to look at.

## Verification that actually verifies

**A grep is not proof.** Check the claim where it would actually live,
and check the negative case. A filename-level survey of this tree gave a
coverage figure that was wrong until it was redone at line level and
again with name-agnostic n-gram matching.

**Prove a semantic claim, do not assert it.** When `pio.h`'s
`.byte 0x66; inl` was replaced with `inw`, the equivalence was
established by assembling both and comparing bytes (`66 ed` either way),
not by reasoning about prefixes. That evidence belongs in the commit
message.

**Numbers are measurements from a run in this session**, not estimates.
Say "178 of 203 objects compile", and say when a figure is an estimate.

**Before claiming a fix works, reproduce the original failure first.**

## Commit messages

Subject ≤ 72 characters, body wrapped at 72. Explain *why*, name what was
measured, state what you got wrong. Validate before committing:

```sh
awk '{ if (length($0) > 72) print NR": "length($0)" chars" }' msg.txt
```

## Licensing

**Never remove or modify an existing copyright notice.** SPDX is explicit
that copyright notices are outside the scope of license identifiers and
must be left intact.

Add, alongside what is already there:

```
SPDX-FileCopyrightText: 2026 Nicholas Martin
SPDX-License-Identifier: <expression>
```

Combining sources from different upstreams uses `AND`, not `OR` — `OR`
means a choice of licences, `AND` means both sets of terms apply.

Our own copyright goes on original expression only. A two-token
notation change dictated by the assembler is not original expression; a
build driver or a shim layer is.

**The SPDX identifiers for OSF's and CMU's specific texts are not yet
established.** `HPND` and `MIT-CMU` are candidates. Verify against the
SPDX licence list before writing any identifier into a file; a wrong
identifier is worse than none.

## Where the work stands

The kernel comes first and nothing else matters until it boots. In a
microkernel the servers are ordinary user tasks — restartable,
debuggable under gdb, rebuildable without a reboot. The kernel is not.
So: boot to a `ddb>` prompt on serial with gdb attached, then everything
else.

### ODE toolset: working

`build/bootstrap-ode.sh` builds six tools from ode4linux and they run:
`make`, `build`, `workon`, `genpath`, `makepath`, `release`. Two flags
are needed and both go through existing hooks, so neither the ode4linux
clone nor this repository is modified:

- `CENV=-fcommon` -- ode4linux targets GCC 4.8; GCC 10 changed the
  `-fno-common` default. Without it, make fails to link on
  `multiple definition of 'maxJobs'`.
- `DEF_ARFLAGS=cr` -- `osf.std.mk` defaults to `crl`, and **`ar crl` is
  broken in GNU binutils 2.42**: the `l` modifier consumes the archive
  name, so ar tries to open the first object as an archive and reports
  `file format not recognized`. Verified by testing `cr`, `crl`, `crs`
  and `crls` directly. OSFMK's own Buildconf already sets `cr`, so this
  affects only ODE's self-build.

`md` (make depend) does not build -- same `-fno-common` problem, but
inside ODE's own makefiles where `CENV` does not reach. It is only
needed for incremental dependency generation, so it is deferred.

### FIRST pass: working

```
build MAKEFILE_PASS=FIRST   ->  rc=0, 0 errors, 250 headers exported
```

The cause of the earlier `don't know how to make build_all` was NOT a
missing environment, and an earlier note in this file saying so was
wrong. Running `build -verbose` shows every Buildconf variable correctly
set. The real cause: Buildconf sets `SOURCEDIR` to the empty string,
which is right only for a sandbox with a backing chain -- OSF's shared
read-only source tree, reached via `backing_build` in `sb.conf`. We have
no backing chain. With `SOURCEDIR` empty, `MAKESRCDIRPATH` (set from it
in `src/Makeconf`) is empty too, so after ODE make relocates itself into
the object directory it has no path back to the source tree and finds no
Makefile at all. The symptom is misleading -- it reads as a missing
target rather than a missing search path.

Fixed through `Buildconf.local`, which `libode/builddata.c` looks for at
`<sandbox_base>/rc_files/<project>/Buildconf.local` and parses AFTER
Buildconf, so `replace setenv` there wins. It is ODE's designed override
point; no vendor file is modified. `build/mksandbox.sh` generates it.

`md` turned out not to be optional -- the FIRST pass calls it for every
directory it exports from. `bootstrap-ode.sh` now builds it by hand,
because `setup.sh`'s link fails on `_argbreak` being a tentative
definition in both `md.c` and libode, and `CENV` does not reach ODE's own
makefiles. Three things were needed and are worth not rediscovering:
`BUILD_DATE`, `MACHINE` and `OS` must be passed as string macros or
`interface.c` and `par_rc_file.c` do not compile; and ODE's `porting/`
replacements for `strerror`, `strdup`, `strcasecmp`, `getcwd`,
`vfprintf`, `vsprintf` and `waitpid` must be excluded, since glibc
provides all of them and ODE's `strerror.c` references `sys_errlist`
and `sys_nerr`, which glibc removed.

### Previous blocker, resolved -- kept for history

`build` reads Buildconf -- it derives `target_machine=at386` and the
object base correctly, and fails with the right paths when they are
missing. But **it does not export Buildconf's variables into make's
environment**. Under `build`, make sees `project_name` empty, so
`osf.std.mk:100`'s `.include <osf.${project_name}.mk>` becomes
`osf..mk`, `osf.${project_name}.passes.mk` is never included, no
`build_all` target is defined, and the build stops with
`make: don't know how to make build_all`.

Setting `project_name=osc` by hand and invoking make directly collapses
the failure to a single remaining unset variable, `GCC_LATEST`, which
Buildconf also sets. So the chain is broken in one place, not many.

Next step is to find why `workon`/`build` are not propagating the
environment. Candidates, in order: `ode_build_env` in
`rc_files/osc/sb.conf`; whether `workon` is meant to set the environment
and `build` only to locate the sandbox; and whether ODE expects the
sandbox conf rather than Buildconf to carry these.

Note `MAKESYSPATH` accepts a colon-separated list (each entry goes
through `Dir_AddDir` in `parse.c`), but Buildconf's `replace setenv
MAKESYSPATH` overrides anything pre-set in the environment -- tested.
That is why the missing rule files are symlinked into `makedefs` by
`build/mksandbox.sh` rather than added to a search path.

### Earlier state, from a hand-rolled driver since abandoned

- `config` generates 130 option headers for AT386 PRODUCTION.
- 15 MIG stubs generate cleanly.
- **178 of 203 objects compiled**, with zero modifications to
  `osfmk7.3/`.

The 25 that did not, by cause:

| cause | count | note |
|---|---|---|
| MIG headers not yet generated | 9 | `memory_object_user.h`, `exc_user.h`, `device_pager_server.h` |
| export-tree gaps | 5 | `machine/disk.h`, `machine/iobus.h`, `profile/profile-mk.h` |
| `assym.S` not generated | 4 | build by compiling and running `i386/genassym.c` |
| `pio.h` port I/O | 6 | the one proven source change; see below |
| `i386_rpc.c` inline asm | 1 | `operand type mismatch for 'mov'`, undiagnosed |

Two traps found the hard way and worth not rediscovering:

- **Generated MIG headers can shadow real source headers.**
  `mach/memory_object.h` exists in the source tree and carries type
  definitions; a MIG-generated header of the same name on an earlier
  `-I` path broke ~150 objects at once and looked like a type error.
- **`ln -sfn target existing_dir/` creates the link inside the
  directory**, not in place of it. This silently produced 3 KB MIG stubs
  instead of 150 KB ones, which looked like success.

The only source change identified so far is `i386/pio.h`: two inline-asm
statements using the 1995 idiom `.byte 0x66; inl` for 16-bit port I/O,
which modern gas rejects. `inw`/`outw` assemble to identical bytes. Not
yet applied.
