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
`make`, `build`, `workon`, `genpath`, `makepath`, `release`, plus `md`
built separately. (`workon` is built but deliberately unused -- see the
idiom review below.) Two flags
are needed and both go through existing hooks, so neither the ode4linux
clone nor this repository is modified:

- `CENV=-fcommon` -- ode4linux targets GCC 4.8; GCC 10 changed the
  `-fno-common` default. Without it, make fails to link on
  `multiple definition of 'maxJobs'`.
- `-std=gnu89` -- GCC 14 makes implicit function declarations, implicit
  int, int-conversion and incompatible-pointer-types **hard errors**.
  All four were valid C89 and are pervasive here: libode calls `gets()`,
  `genpath.c` calls `getcwd()` and `chdir()` without `<unistd.h>`.
  This is not a suppression -- gnu89 is the dialect the code is written
  in, and Buildconf names gcc 2.7.2.1 as the era compiler. Verified with
  gcc 14.2.0 against the exact failing files: `getstab.c` 1 error -> 0,
  `genpath.c` 2 -> 0, `makepath.c` 2 -> 0.
  Chosen over pinning an older gcc because a dialect flag describes the
  source while a version pin describes an accident of the host -- and on
  a future self-hosting Mach system the compiler will be natively gnu89
  and need no flag at all. Known cost: some of those diagnostics are
  real bugs, not dialect noise. `gets()` into a fixed buffer is a
  genuine overflow. It is ODE's code, so it is recorded, not patched.
- `DEF_ARFLAGS=cr` -- `osf.std.mk` defaults to `crl`, and **`ar crl` is
  broken in GNU binutils 2.42**: the `l` modifier consumes the archive
  name, so ar tries to open the first object as an archive and reports
  `file format not recognized`. Verified by testing `cr`, `crl`, `crs`
  and `crls` directly. OSFMK's own Buildconf already sets `cr`, so this
  affects only ODE's self-build.

`md` (make depend) does not build -- same `-fno-common` problem, but
inside ODE's own makefiles where `CENV` does not reach. It is only
needed for incremental dependency generation, so it is deferred.

### Idiomatic ODE usage -- checked against the manuals

Read `ode4linux/src/ode/man/man1/{build,workon,mksb}.1` and
`osfmk7.3/{OSFMK_BUILD.README,set_ode_path.sh,build_world}` before
changing how the tools are invoked. Four things came out of that review:

**Do not use `workon`.** `OSFMK_BUILD.README` says to, but `workon(1)`
itself says it "is part of the source control mechanism ... and is
normally not be used if ODE source control is not used." We use git.
`build(1)` takes `-sb` and `-rc` in its own right, and running it
directly gives an identical result -- verified, `MAKEFILE_PASS=FIRST`
returns 0 with the same 250 exported headers either way. It also drops
workon's requirement for `SHELL`. `USER` is still needed -- `build(1)`
checks for it directly and aborts with "USER not found in environment";
`build/ode.sh` sets it from `$LOGNAME` if absent, as
`OSFMK_BUILD.README` suggests. Note that an EMPTY `USER` passes the
check, so a test using `env USER=` proves nothing.

**Do not prepend ODE to the caller's PATH.** `set_ode_path.sh` states
the rule: ODE's tools belong early in PATH only inside a workon shell,
and after the system tools otherwise, so a plain `make` does not
silently become ODE make. `build/ode.sh` sets PATH for its own
invocation only.

**`-rc` is a documented alternative to `~/.sandboxrc`.** `build(1)`
FILES lists `${HOME}/.sandboxrc`, and `-rc` overrides it. Using `-rc`
keeps the sandbox rc out of `$HOME`.

**There is no SECOND pass in the canonical order.** `build_world` is
OSFMK's own script and goes straight from FIRST to per-directory
targets:

```
build MAKEFILE_PASS=FIRST
build -here mach_services/lib/libcthreads
build -here mach_services/lib/libsa_mach
build -here mach_services/lib/libmach
build -here mach_services/lib/libmach_maxonstack
build -here file_systems
build -here bootstrap
build -here mach_kernel MACH_KERNEL_CONFIG=PRODUCTION
makeboot
```

`makeboot` there is PowerMac-specific -- it produces the Mach_Kernel
image for a MacOS Extensions folder. AT386 has its own
`conf/AT386/config.makeboot` and a boot path under `stand/AT386`, so
that last step will differ for us.

`build -here <dir>` taking a directory as the target, and
`build VAR=value <target>`, are both documented idioms -- see
build(1) FLAGS and EXAMPLES.

### Host contamination audit

Done properly once; redo it after any change to `CARGS` or the include
paths. Three checks, all empirical.

**1. Headers.** `gcc -H` on a kernel source, looking for `/usr/include`
or GCC's internal include directory: **zero hits**. Every header comes
from the exported OSFMK tree or the source tree. `-nostdinc` is
complete rather than merely restrictive here, because OSFMK ships its
own `sa_mach/stdarg.h`, `sa_mach/string.h` and `sa_mach/types.h` --
nothing needs GCC's freestanding headers.

**2. Compiler-injected symbols.** `nm -u` across every built object,
subtracting what the objects define between them. This found two real
defects, both from modern distro GCC defaults OSF could not have
anticipated:

- `__stack_chk_fail_local` -- from `-fstack-protector-strong`, on by
  default. Lives in libssp/libc, which does not exist here.
- `_GLOBAL_OFFSET_TABLE_` -- from `-fPIE`, on by default. A kernel is
  loaded at a fixed address and has no dynamic linker.

Both present on every non-trivial object before the fix, both gone
after adding `-fno-stack-protector -fno-pic` to `CARGS`. Re-audited
across the full build: no injected symbols of any class remain.

**3. libgcc helpers.** Zero `__udivdi3`-family references in the
current objects. If any appear later they must be resolved
deliberately, not by linking host libgcc.

`memcpy`, `memset`, `bcopy`, `bzero` appear as undefined and that is
correct -- the kernel defines them itself in `i386/bcopy.S`
(`memcpy`, `bcopy`) and `i386/bzero.S` (`memset`, `bzero`). Those are
assembly objects the build has not reached yet. GCC emits calls to
`memcpy` for large struct assignment regardless of `-fno-builtin`, so
these references are expected and the kernel satisfies them.

**Link stage.** `_LD_` resolves to `ld`, not to the GCC driver, so no
crt files, no `-lc` and no `-lgcc` are added implicitly -- `ld` links
only what it is given. **Predicted issue, not yet hit:** `ld` on an
x86-64 host defaults to `elf_x86_64` output and will need
`-m elf_i386`, which is the link-stage counterpart of the `-m32`
problem and the same class of 1998-assumption. `LDFLAGS` for ELF is
`-Ttext ${TEXTORG} -e pstart` in `conf/AT386/template.mk`, with
`LDFLAGS+=${LDOPTS}` as the hook.

### Kernel build: 122 objects, then i386_rpc.c

```
sh build/ode.sh -here mach_kernel MACH_KERNEL_CONFIG=PRODUCTION
->  122 objects, then:
    i386/i386_rpc.c:215: Error: operand type mismatch for `mov'
    (also 409, 462, 519)
```

**Measure only in a clean clone of this repository.** The figure was
briefly reported as 74 with a `memory_object.h` failure. That was
measured in a working copy whose *vendor import itself* had been
polluted: the tree had been built in before it was committed, so 160
generated files under `src/mach_kernel/PRODUCTION` were captured into
the "pristine" import. A stale `PRODUCTION/mach/memory_object.h` there
shadowed the real source header and produced a failure that does not
exist in a clean checkout.

Two lessons, both cheap and both learned expensively:

- **The vendor import must be made from a fresh upstream clone**, never
  from a directory anything has been built in. Verify with
  `git ls-files 'osfmk7.3/**/PRODUCTION/*' | wc -l`, which must be 0.
- **Measurements are only meaningful in a clean clone of the pushed
  repository.** When in doubt, clone from the remote and measure there.
  It is public; there is no reason to guess at its state.

### Kernel build configuration

```
sh build/ode.sh -here mach_kernel MACH_KERNEL_CONFIG=PRODUCTION
```

Two configuration findings, both added to `CARGS` in `Buildconf.local`,
which is where Buildconf already puts i386-on-Linux compiler arguments:

- **`-m32`.** Buildconf assumes a 32-bit host, as every host was in
  1998. Without it we were building a 64-bit kernel. It surfaced as
  `cast from pointer to integer of different size` in `ipc_table.h` --
  a real defect, not a warning to wave away.
- **`-Wno-error`.** `conf/template.mk:84` sets `-Werror` against gcc
  2.7.2.1's warning set. Modern GCC adds ~25 years of diagnostics OSF
  never saw, so keeping it tightens their configuration rather than
  preserving it.

Full warning inventory across the kernel is 13 in 4 classes:
`-Wpointer-compare` (5), `-Wpedantic` (3), `-Wexpansion-to-defined` (3),
`-Woverflow` (2). Worth auditing, not yet done.

The `-Woverflow` pair is understood and deliberately NOT fixed:
`mach_port_qos_t` in `mach/port.h` declares `boolean_t name:1` where
`boolean_t` is `int`, so a signed one-bit field stores `TRUE` as `-1`.
It has behaved that way since 1998 and works, because `-1` is truthy.
**That struct crosses the IPC boundary** -- changing the field
signedness would change the wire format. Leave it.

### Source modifications to osfmk7.3 (the complete list)

**`i386/pio.h`** -- two lines, in `inw` and `outw`. The 1995 idiom
`.byte 0x66; inl` is rejected by GNU as 2.42. Replaced with matching
mnemonics, verified byte-identical (`66 ed`, `66 ef`). Reasoning is in
the AI-ONLY NOTES block at the foot of the file.

**Do not add a preprocessor conditional to keep both encodings.** It was
tried and reverted. No predefined macro exposes the assembler's version
-- GCC's documented set has none, the only assembler-related one being
`__GCC_HAVE_DWARF2_CFI_ASM` -- so such a switch could only ever be set
by hand, which is not a portability mechanism. A `__GNUC__` test would
additionally be vacuous, since the block already sits inside
`#if defined(__GNUC__)`. Where assembler capability genuinely must be
probed, the established practice is a build-time test that assembles a
snippet, not an `#ifdef`. Decisively, the other six accessors in this
same file -- `inl`, `inb`, `outl`, `outb` -- already use plain
mnemonics with identical `"=a"`/`"d"` constraints, so the fix restores
consistency rather than introducing a style; and Linux does not
conditionalise this either (`arch/x86/include/asm/shared/io.h`,
`BUILDIO`). Superseded code belongs in git and in the notes block, not
in live conditionals nothing can select.

That is the entire list. Everything else so far has been configuration.

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

That diagnosis was itself WRONG and is kept only as a record of the
wrong turn. `build -verbose` shows every Buildconf variable correctly
set; I had conflated a hand-run of make (no `project_name`) with a run
under `build`. The real cause was `SOURCEDIR`, resolved above.

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
