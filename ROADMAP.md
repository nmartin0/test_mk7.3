# ROADMAP

Where this project is, what is left, and what the reference trees told
us to do about it. Updated as things land; `docs/current-blocker.md`
holds the live detail, this holds the shape.

---

## Done

**1. The kernel boots.** OSFMK 7.3 on QEMU/i386, twelve deviations from
the MkLinux base, each recorded in `HANDOFF.md` with its reason.

**2. A multiserver userland runs.** Bootstrap task loads and starts
`default_pager` and LITES from a filesystem it reads itself.

**3. LITES runs and mounts a root filesystem.** Builds, links, loads,
prints its banner, mounts ext2 on `hd0c`, reads the root directory, and
reaches its first-program exec.

**Supporting work that turned out to matter as much:**

- `libmach_sa` -- a library several things link against and **nothing in
  any tree builds**. Now built, following `libmach_p`'s pattern.
- The emulator links and runs; `emul_exec_open` and `emul_exec_start`
  both succeed.
- The boot cycle went from ~500 s to ~20 s by moving the servers off the
  emulated floppy onto IDE.
- The whole stack builds and boots in the development sandbox, so
  changes can be tested where they are written.

---

## Next: finish step 4, a shell prompt

### 4a. The first program: build Mach 4's `mach_init`

**A real, permissively licensed `mach_init` exists**, in
`user-mach4/etc/mach_init/` of the reference collection. This changes
4a from "write one" to "port one", and it is the right one:

```
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, ...
```

A CMU Mach licence **with an explicit grant** -- permissive and
compatible with this tree, unlike the MkLinux personality sources.

And its history is decisive:

```
 * 22-Jan-94  Johannes Helander (jvh) at Helsinki University of Technology
 *	Primarily try to exec /sbin/init. Only if that fails run
 *	/etc/init. But if that fails as well, try running /bin/sh on the
 *	console.
```

**Johannes Helander is the author of LITES.** This is the first program
maintained by the same person who wrote the personality we are booting,
doing exactly what LITES's `init_program_path` expects.

`main.c` is 332 lines of init logic; `service.c` is 575 lines
implementing the service-port protocol. It builds with

```make
LIBS   = -lservice -lthreads -lmach -lcmucs
LDFLAGS += -static
```

`libservice` is in OSFMK at `mach_services/lib/libservice`, `-lthreads`
maps to `libcthreads` and `libmach` we already build; `libcmucs` is in
`user-mach4/lib/`. It uses pre-ANSI `varargs.h`, which will need the
same treatment as LITES's `stdarg.h` did.

**Caveat to settle first:** it is a Mach 4 program. `main.c`'s init
logic is portable, but `service.c` speaks Mach 4's service and name port
protocol, which may not match OSFMK 7.3's. Establish which parts carry
over before committing to the whole thing.

### 4a-alt. Or write a minimal one

The blocker is that LITES execs `/mach_servers/mach_init` and no such
program exists in any tree for i386.

MkLinux's `mach_init` turns out not to be a Mach component at all -- its
strings and source show an ordinary POSIX program that opens a console,
tries `/etc/init`, `/bin/init`, `/sbin/init`, and falls back to spawning
a shell. So this is a program to **write**, not to find. The MkLinux
source is read-only reference (it carries no licence grant), but the
design is that of Version 7 UNIX `init` and needs no borrowing.

**Expect to hit first:** binary classification. The emulator currently
reports our i386 ELF as `BT=20` (`hpelf`) because
`liblites/exec_file.c` only recognises an ELF as its own when the entry
is above `0x10000000`, and ours is at `0x8049320`. A first program built
as i386 ELF will be misclassified the same way. xMach's tree shows the
shape of the fix -- an `else` branch reading the program headers.

Also in that file, and in both trees: `switch ((tmp >> 16) && 0x3ff)`,
`&&` where `&` was meant.

### 4b. Populate the root filesystem

`debugfs` writes into an ext2 image with no mount and no privileges, so
this needs no new tooling. What goes in is whatever the first program
looks for.

`tools/boot-ide.sh` should grow the population step so the working
configuration is reproducible rather than hand-typed.

---

## Correctness work, not blocking

These replace things that work with things that are right. None is
urgent; all are cheap and remove a workaround.

**Use ext2 for the server volume, retire `mkminix.py` -- and with it,
GPL code from the bootstrap task.**

This started as a convenience and is now a licensing correction.
`file_systems/minixfs/` contains three **GPL-licensed** files:

```
file_systems/minixfs/minix_ffs_compat.c
file_systems/minixfs/minix_ffs_compat.h
file_systems/minixfs/minix_fs.h
```

and `minixfs/machdep.mk` builds one of them into the library:

```make
MINIXFS_OFILES = minixfs.o minix_ffs_compat.o
```

So `libsa_fs.a`, and therefore **the bootstrap task binary**, contains
GPL code today. `ext2fs/` is entirely GPL-free, and the bootstrap task
already builds an ext2 reader and tries it before minix.

Dropping minix removes the GPL dependency, removes a hand-written tool
(`mkminix.py`, written only because `mkfs.minix` left Debian 13), and
makes all three disks one filesystem type built with stock `mke2fs` and
`debugfs`.

The other GPL files in the tree are `file_systems/POWERMAC/COPYLEFT/hfs/`
-- nineteen of them, helpfully named, and **not built on i386**.
Those are the only GPL sources in `src/`.

**Originally:** The i386
bootstrap task builds UFS, ext2 **and** minix readers
(`file_systems/AT386/machdep.mk`) and tries them in order
(`AT386/fs_switch.c`). `mkminix.py` exists only because `mkfs.minix`
was dropped from Debian 13. An ext2 server volume would use stock
`mke2fs` and `debugfs`, make all three disks one filesystem type, and
remove a hand-written tool from the critical path.

**Teach LITES's ext2 reader about `filetype`, retire `-O ^filetype`.**
Our kernel's reader already handles it -- `file_systems/ext2fs/ext2_fs.h`
splits `name_len` into `unsigned char name_len` plus
`unsigned char file_type`. LITES's `server/ufs/ext2fs/ext2_fs.h` still
has `__u16 name_len`. The two readers in this system disagree about the
on-disk format; the fix is in-tree, permissively licensed, and makes
stock `mke2fs` defaults work.

**Fix `exec_file.c`'s `&&`.** Present in both LITES trees.

---

## Later

**Collocate LITES in the kernel.** A recovered `bootstrap.conf` shows
MkLinux ran its personality **inside the kernel's address space**:

```
-k -S 524288000 startup /dev/boot_device/mach_servers/vmlinux
```

`-k` sets `SERVER_IN_KERNEL_F` and `-S` gives the collocated map size
(`bootstrap.c:784`). Configured entirely from `bootstrap.conf`, no code
change. Worth trying once LITES boots properly -- it removes the IPC
boundary between kernel and personality.

**The kernel will not boot with 512 MB.** It prints the `cnvmem` line
and stops. 64 MB and 128 MB work, so the limit is between 128 and 512;
probably `vm_page_bootstrap` or `pmap` not scaling. Bracketed, not
investigated.

**ddb for live inspection.** `MACH_KERNEL_CONFIG=DEBUG` builds the
in-kernel debugger; `tools/boot-debug.sh` runs it. It understands Mach's
own types -- tasks, threads, ports, VM maps -- which the gdb stub
cannot reach. Use it when a question needs live kernel state.

---

## What the reference trees are for

Full detail in `docs/lites-survey.md`. In short:

| tree | use |
|---|---|
| MkLinux `osfmk/` | **our base.** Ahead of DR3: has multiboot, the QEMU floppy fix, `INTEL_PTE_GLOBAL` |
| DR3 | later release but **behind our base** for i386; do not move to it |
| `pmk1.1` | third Mach 3.0 PMK tree, cross-check only |
| `linux/arch/osfmach3_i386` | a working personality on this kernel and architecture -- the closest analogue. **No licence grant: read only** |
| xMach LITES | post-u3 fixes, notably ELF classification. **Read only** |
| XNU, GNU Mach | incompatible licences. Design only, never copy |

**The rule, without exception:** find what the build asks for, find what
provides it, and if nothing does, build that. Never alias one name to a
different thing. Audit any guess before it becomes load-bearing.
