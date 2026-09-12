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
ODE4LINUX=/path/to/ode4linux sh build/bootstrap-ode.sh
ODE4LINUX=/path/to/ode4linux . build/env.sh
```

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

State at the time of the vendor import, from a hand-rolled driver that
has since been abandoned in favour of real ODE:

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
