# Environment and toolchain

Everything needed to reproduce the build and get to the current state.
If you are an agent picking this up cold, this file plus `WORKFLOW.md`
and `DEBUGGING.md` should let you rebuild and reach the live blocker
without rediscovering anything.

---

## Quick start

```sh
git clone https://github.com/nmartin0/test_mk7.3.git
cd test_mk7.3

export ODE4LINUX=~/ode4linux          # the ODE toolset source
export MK_BUILD=~/.cache/mk7.3        # all build output goes here

sh build/bootstrap-ode.sh             # once: build ODE's tools
sh build/mksandbox.sh                 # once: prepare the sandbox
sh build/ode.sh MAKEFILE_PASS=FIRST   # export headers, run MIG
sh build/ode.sh -here mach_kernel MACH_KERNEL_CONFIG=PRODUCTION
```

Result: `$MK_BUILD/obj/at386/mach_kernel/PRODUCTION/mach_kernel.PRODUCTION`,
about 1,021,600 bytes, `ELF 32-bit LSB executable, Intel 80386`.

Boot it:

```sh
qemu-system-i386 -kernel mach_kernel.PRODUCTION \
    -initrd bootstrap,bootstrap -display none -no-reboot -m 64 \
    -monitor unix:/tmp/mon,server,nowait &
python3 tools/vgadump.py /tmp/mon /tmp/vga.bin 8
```

There is **no serial console**; output goes to VGA. See `DEBUGGING.md`.

## Host toolchain

Versions this was last built and verified with:

```
gcc        13.3.0  (also verified against 14)
binutils   2.42    (ld, as, objdump, nm, readelf)
qemu       8.2.2   (qemu-system-i386)
gdb        15.1
python     3.12.3
```

Nothing else is required. There is **no cross-compiler**: the host gcc
targets i386 with `-m32`, so `gcc-multilib` (or the distribution
equivalent providing 32-bit startup files and headers) must be present.

## External dependencies, not in this repository

| path | what | modified? |
|---|---|---|
| `~/ode4linux` | ODE toolset source (Warkentin, 2014) | **no** |
| `$MK_BUILD` | all build output, default `~/.cache/mk7.3` | n/a |

The repository stays clean after a full build. `mksandbox.sh` writes
symlinks that `.gitignore` covers; `git status` should be empty.

---

## ODE: the thing that makes this unusual

OSFMK is not built with `make`. It is built with **ODE**, the Open
Development Environment — OSF's own build system, contemporary with the
kernel. Understanding this is most of the work of getting a build going.

ODE builds inside a **sandbox**: a directory holding `src`, `obj`,
`export` and the `rc_files` that configure it. OSFMK ships the sandbox
layout already (`osfmk7.3/osfmk/{src,rc_files,link,tools}`) plus a
`sandboxrc.template`.

### The tools ODE provides

`build/bootstrap-ode.sh` builds seven binaries from the ode4linux source
into `$MK_BUILD/ode-sandbox/tools/at386_linux/bin`:

| tool | role |
|---|---|
| `make` | ODE's own make — **not** GNU make; BSD-derived, different syntax |
| `build` | the driver: reads `Buildconf`, sets the environment, invokes `make` |
| `genpath`, `makepath` | compute the sandbox search paths |
| `md` | dependency generator; the FIRST pass calls it for every exported directory |
| `release`, `workon` | ODE's own source-control and install tools — **not used here** |

`workon` is deliberately not used: per its own manual it is for ODE
source control, which this project does not use. `release` does not
build under GCC 14 (its Makefile overrides `CFLAGS` with `=`), which is
reported but not fatal, because nothing invokes it.

### Flags the bootstrap needs, and why

- `CENV="-fcommon -std=gnu89"` reaches only ODE's `bootstrap.sh`, which
  builds `make`. `gnu89` because GCC 14 makes implicit declarations hard
  errors and this code is legitimately C89.
- `CFLAGS="-fcommon -std=gnu89"` **must also be set.** ODE's
  `src/Makeconf` assigns `CENV` with a plain `=` for the `at386_linux`
  context, clobbering the environment value. `CFLAGS` uses `+=` in the
  same block, so it survives. Without this, GCC 14 kills five of the
  seven tools.
- `DEF_ARFLAGS=cr`. `osf.std.mk` defaults to `crl`, and binutils 2.42's
  `ar` treats the `l` modifier as consuming the archive name.

### `build(1)` and `Buildconf`

`build` reads `osfmk7.3/osfmk/src/osc/Buildconf` and propagates its
environment to `make`. **Read that file before changing any build
setting** — it is OSF's own configuration and already has i386-on-Linux
support, including setting `__NO_UNDERSCORES__` for exactly this case.

`build/mksandbox.sh` generates `rc_files/osc/Buildconf.local`, which is
*not* committed. It carries the settings this port needs:

```
replace setenv SOURCEDIR ${source_base}
replace setenv CARGS "-D__NO_UNDERSCORES__ -m32 -std=gnu89 -fcommon \
                      -fno-stack-protector -fno-pic -Wno-error"
replace setenv ANSI_CC        "gcc -m32 -fno-builtin -Wno-error"
replace setenv TRADITIONAL_CC "gcc -m32 -fno-builtin -Wno-error"
replace setenv HOST_CC        "gcc -m32 -fno-builtin -Wno-error"
replace setenv LDOPTS "-m elf_i386"
```

Each was forced by a specific failure:

| setting | without it |
|---|---|
| `SOURCEDIR` | `MAKESRCDIRPATH` is empty, make relocates to the obj dir and reports `don't know how to make build_all` — a missing search path, not a missing target |
| `-m32` | builds a 64-bit kernel; surfaces as a pointer-to-int cast in the IPC tables |
| `ANSI_CC` etc. | the `genassym` rule calls the compiler directly and drops the standard flags, so `genassym` builds 64-bit and bakes 8-byte pointer offsets into `assym.S` |
| `-m elf_i386` | `ld` defaults to `elf_x86_64` and rejects the objects |
| `-fcommon` | `multiple definition of vm_page_queue_free_lock` from tentative definitions |
| `-std=gnu89` | `multiple definition of get_cr0`: `extern __inline__` means "no out-of-line copy" in gnu89 and "emit one here" in C99 |
| `-fno-stack-protector -fno-pic` | pulls in `__stack_chk_fail_local` and `_GLOBAL_OFFSET_TABLE_` |

### Two things `mksandbox.sh` fixes with symlinks

1. ODE derives `obj` and `export` from the sandbox base, which would put
   build output inside the repository. They are symlinked to
   `$MK_BUILD`. Symlinking the other way round does **not** work: ODE
   computes the object directory with relative paths (`../../../obj/…`),
   the shell resolves the symlink, and the walk escapes into the wrong
   tree.
2. `osfmk7.3/osfmk/src/makedefs` is an *incomplete* ODE installation.
   `osf.rules.mk` includes `osf.man.mk` and `osf.doc.mk`, and ODE make
   needs `sys.mk`; none are present, because OSF expected them from the
   installed ODE. They are symlinked in from ode4linux. This completes
   an installation rather than modifying OSFMK.

`MAKESYSPATH` needs two directories: `makedefs` has the rules but not
`sys.mk`.

---

## Mach-specific tooling

### MIG — the Mach Interface Generator

Mach's IPC interfaces are declared in `.defs` files and compiled by MIG
into client stubs, server stubs and headers. The vendor tree ships
prebuilt host binaries at
`osfmk7.3/osfmk/tools/i386/i386_linux/hostbin/`:

```
mig       the shell driver
migcom    the compiler proper
config    the kernel configuration tool
makeboot  builds a bootable image
```

MIG runs during `MAKEFILE_PASS=FIRST`. If the generated stubs come out
suspiciously small (3 KB rather than ~150 KB), check that no symlink was
created *inside* a directory rather than in place of it — `ln -sfn` does
that silently, and the result looks like success.

**Generated MIG headers can shadow real source headers of the same
name.** `mach/memory_object.h` exists in the source tree; a generated
one earlier on the `-I` path once broke ~150 objects at once and
presented as a type error.

### `config` — kernel configuration

`config` reads `conf/AT386/PRODUCTION` plus the `config.*` fragments and
`conf/AT386/files`, and generates the per-configuration makefile and
headers into the `PRODUCTION` object directory. The `#define`s it emits
(`NCPUS`, `MACH_KPROF`, `MP_V1_1`, …) decide which `#if` arms of the
source are live. Read them from the generated headers in
`$MK_BUILD/obj/at386/mach_kernel/PRODUCTION/*.h`, not from the source
defaults — the source has multiple contradictory definitions in places.

For this configuration: `NCPUS 1`, `MP_V1_1 0`, `AT386 1`. A very large
fraction of the tree is `#if NCPUS > 1` and is not compiled; do not
spend time fixing code in those arms.

### `genassym` — struct offsets for assembly

`i386/genassym.c` is compiled **for the target**, run, and its output
becomes `assym.S`, which gives the assembly code struct offsets like
`TH_KERNEL_STACK` and `KSS_EIP`.

Its `offsetof` macro takes a **pointer** type:

```c
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE)0)->MEMBER)
```

so `offsetof(thread_t, kernel_stack)` is correct usage, because
`thread_t` is `struct thread_shuttle *`.

The rule that builds it in `conf/AT386/template.mk` invokes the compiler
**without** the standard flags, which is why `ANSI_CC`/`TRADITIONAL_CC`/
`HOST_CC` must be set. If `genassym` is built for the wrong word size
the offsets are silently wrong and the kernel misbehaves with no
diagnostic.

To verify the offsets are right, recompile the same `offsetof`
expressions with the kernel's own flags and compare against the
generated `assym.S`. This was done and they match; do not re-suspect
them.

### `build_world`

`osfmk7.3/build_world` is OSF's own build order:

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

There is **no SECOND pass**. All of the library and `bootstrap` steps
build successfully; `bootstrap` produces a 220,656-byte i386 ELF at
`$MK_BUILD/obj/at386/bootstrap/bootstrap`, which is useful as a boot
module for testing even though this tree's active bootstrap path expects
Hurd servers (see `docs/bootstrap-fork.md`).

---

## Debugging tooling built for this project

Three small tools in `tools/`, all driven from a running QEMU:

| tool | reads | use for |
|---|---|---|
| `vgadump.py` | VGA text buffer via `pmemsave` | the console; tries both `0xb8000` and `0xa0000` |
| `pmem.py` | guest **physical** via `pmemsave` | globals at known link addresses |
| `vmem.py` | guest **linear** via `x/Nxw` | stacks, register-derived pointers |

Start QEMU with **both** control interfaces so gdb and the monitor can
be used together — break in gdb, then `shell` out to read memory while
the guest is stopped:

```sh
qemu-system-i386 ... -s -S -monitor unix:/tmp/mon,server,nowait
```

```
(gdb) shell python3 tools/pmem.py /tmp/mon 0x1e0a08 4
(gdb) shell python3 tools/vmem.py /tmp/mon 0xc8b78f1c 8
```

This combination exists because **gdb alone frequently cannot read
kernel memory** at a breakpoint even when registers read fine.

`DEBUGGING.md` covers the traps in detail. The three that cost the most:

- the kernel is relocated by **segmentation** (`cs_base = 0xC0000000`),
  so breakpoints need linear addresses and register-derived pointers
  need `+0xC0000000` before the monitor can read them;
- **gdb breakpoint conditions silently do not work** against this stub;
- the current failure is **non-deterministic**, so every value in a
  chain of reasoning must come from one stopped guest.

---

## Where the project is

The kernel boots and reaches device autoconfiguration:

```
Kernel virtual space from 0x0 to 0x40000000.
Available physical space from 0x100000 to 0x3fe0000
Mach 3.0 VERSION(PMK1.1) ... mach_kernel/PRODUCTION (vm)
vm_page_bootstrap: 14705 free pages
fdc0, fd0, fd1, kd0, com0, vga0 ... configured
realtime clock configured
battery clock configured
intnull(14)
boot_script_task_create
boot_script_task_create
panic: splx(old 57, new 8): logic error in locore.s
```

320 distinct functions entered. VM, IPC, task and thread creation, timer
interrupts and full device autoconfiguration all work. The live blocker
and everything ruled out for it are in `docs/current-blocker.md`.

Deviation from the vendor import is five files:

```
i386/AT386/model_dep.c   BSS clear ordering; mb_info and first_avail in
                         .data; module reads bounded by mods_count
intel/pmap.c             kpde in .data
i386/i386_rpc.c          asm operands that are written declared as outputs
i386/locore.S            register widths matched to instruction suffixes
i386/pio.h               inw/outw instead of the 0x66 prefix hack
```

Every one has an `AI-ONLY NOTE` at the site and a commit message with
the evidence. All five were re-verified as necessary after the most
recent bug was found, each reverted individually.
