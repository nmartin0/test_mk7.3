# test_mk7.3

A revival of **OSF Mach Kernel 7.3** (MkLinux DR3) for i386, built on a
Linux host and run under QEMU.

The immediate goal is a booting microkernel with a serial console and a
gdb stub. OS servers come after that, because in a microkernel they are
ordinary user tasks and can be built and debugged at runtime.

## Layout

| path | what it is |
|---|---|
| `osfmk7.3/` | verbatim vendor import of OSF MK 7.3. Modified only where necessary and always with justification. |
| `build/` | everything we write. |
| `AGENTS.md` | operational rules — read before changing anything. |
| `HANDOFF.md` | **start here** — current state, next question, and the traps. |
| `WORKFLOW.md` | how work is done here. |
| `ENVIRONMENT.md` | toolchain, ODE, MIG, and how to reproduce the build. |
| `DEBUGGING.md` | how to find out why the kernel misbehaves; read before debugging. |
| `docs/` | design notes, open decisions, and the current state. |
| `docs/archive/` | solved investigations, kept for their eliminated hypotheses and instrument traps. |
| `tools/` | debugging helpers. |
| `PRINCIPLES.md` | why the decisions are what they are. |

`git diff <vendor-import>..HEAD -- osfmk7.3/` is the complete record of
our deviation from upstream. It is expected to stay small.

## Prerequisites

```sh
apt-get install gcc gcc-multilib binutils libc6-i386 qemu-system-x86 gdb
```

And a clone of [ode4linux](https://github.com/nmartin0/ode4linux). OSFMK
is built by ODE make, not GNU make. Its own rule set is complete and
in-tree; ode4linux supplies the two things the OSFMK tree lacks — a make
binary that builds on a modern Linux host, and `sys.mk`.

`libc6-i386` is needed only to run the prebuilt `migcom` and `config`
shipped under `osfmk7.3/osfmk/tools/i386/i386_linux/hostbin`. Building
both from their in-tree source is a planned milestone.

## Build

```sh
export ODE4LINUX=/path/to/ode4linux
sh build/bootstrap-ode.sh      # once: builds ODE make
. build/env.sh                 # sets the AT386-on-Linux environment
```

`build/env.sh` is derived line by line from
`osfmk7.3/osfmk/src/osc/Buildconf`, OSF's own ODE configuration, which
already carries explicit support for an i386 target on a Linux host. The
single deliberate departure is documented in place.

Current build state, including what compiles and what does not, is
recorded in `AGENTS.md`.

## Licensing

`osfmk7.3/` is OSF Mach Kernel 7.3. Every source file carries OSF's
MIT/X11-style notice granting use, copying, modification and
distribution for any purpose without fee. Files also carry, variously,
Carnegie Mellon, Intel, Olivetti and University of Arizona notices —
all permissive, all preserved as found.

Note that `osfmk7.3/osfmk/src/mach_kernel/conf/copyright.osf` contains a
restrictive academic-use template. It is **not applied to any source
file**; 1,272 kernel sources carry the permissive notice and none carry
the restrictive one. Do not be misled by it.

The licence for our own contributions in `build/` is not yet decided.
Files carry `SPDX-FileCopyrightText: 2026 Nicholas Martin` without a
licence identifier until it is.
