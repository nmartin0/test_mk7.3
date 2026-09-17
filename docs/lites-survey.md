# LITES 1.1.u3 as the UNIX personality

Survey only. Nothing has been built or ported yet.

Source: `github.com/nmartin0/lites-1.1.u3`, 12 MB.

## Why it is the right candidate

The bootstrap task's built-in configuration names three servers:

```c
name_server name_server
default_pager default_pager
unix startup -s
```

`name_server` and `default_pager` both build from this tree. `unix` is
the BSD4.3 UX server, which was always distributed separately because it
was licence-encumbered, and is not present in either OSFMK 7.3 or 6.1.

LITES is the free 4.4BSD-Lite-based replacement.

## It has first-class OSF Mach support

`conf/MASTER`:

```
options     osfmach3  OSFMACH3  1  osfmach3.h
makeoptions osfmach3  TARGET_CFLAGS+=-D_ANSI_C_SOURCE -DOSF_LEDGERS=1 \\
                                     -DUNTYPED_IPC=1 -D__STDC__=1
```

88 `#if OSFMACH3` sites across `server/`, `include/`, `liblites/` and
`emulator/`. Both defines are OSF-specific:

- **`OSF_LEDGERS`** -- ledgers are an OSF addition GNU Mach does not
  have. They are the `root_wired_ledger`/`root_paged_ledger` ports our
  `do_bootstrap_ports` returns.
- **`UNTYPED_IPC`** -- the NDR message format OSFMK 7.x uses, as
  against the older typed IPC.

## The message format matches 7.3 exactly

This is the sharpest compatibility test and the two sides agree.

LITES, `server/serv/ux_syscall.c:81`:

```c
#if UNTYPED_IPC
	mach_msg_format_0_trailer_t *trailer;
#else
	static mach_msg_type_t bsd_rep_int_type = { ... };   /* old typed IPC */
```

OSFMK 7.3: `mach/ndr.h` defines `NDR_record_t`, the generated stubs
carry 232 `NDR_record` references, and `ipc_kobject_server` declares
`mach_msg_format_0_trailer_t *trailer` -- the identical type.

So LITES's OSF arm targets untyped/NDR IPC with format-0 trailers, which
is what 7.3 speaks, not the typed IPC of MK6.x and CMU Mach 3.

## Interface compatibility is structural, not lucky

LITES ships **5** `.defs` files and they are all its own interfaces:
`bsd_1`, `bsd_types`, `Nbsd_1`, `signal`, `emul_mach`. It ships **no**
Mach `.defs` -- no `device.defs`, no `mach.defs`.

`conf/Makerules:96`:

```make
MIG := $(wildcard ${INSTALL_BINDIR}/mig ${MACH_RELEASE_DIR}/bin/mig)
MIG := $(firstword ${MIG} mig)
```

It locates `mig` in the **target Mach's** release directory and
generates every Mach RPC stub from the target kernel's own `.defs`.
Whatever message ids, struct layouts and trailer formats OSFMK 7.3 uses,
LITES's stubs are produced to match, because they are produced from it.

This is the inverse of the Hurd situation:

| | LITES | Hurd servers |
|---|---|---|
| form | source, built against the target kernel | prebuilt binaries |
| Mach stubs | generated from *our* `.defs` by *our* `mig` | compiled against GNU Mach |
| dialect risk | structurally eliminated | open, untested |

We have the toolchain: `osfmk7.3/osfmk/tools/i386/i386_linux/hostbin/`
holds `mig` and `migcom`.

## Surveyed: nmartin0/mach_stuff

A 454 MB collection of extracted tarballs. What is in it, and what it is
worth.

### Directly useful

**`linux/arch/osfmach3_i386/`** -- a complete Linux personality running
on OSFMK, **on i386**. Three copies are present (`linux/`,
`mklinux-2.0.38-pre9/src/`, `Change/DR3/mklinux/src/`). This solves the
same problem LITES does, against the same kernel, on our architecture,
and it shipped and worked. It is the closest published analogue to this
project and the first place to look for any question about how a
personality talks to this kernel.

It already settled one: `arch/osfmach3_i386/Makefile:69` reads

```make
LDFLAGS = -e __start_mach -static -nostdlib
```

confirming that `__start_mach`, from `libsa_mach`'s crt0, is the correct
entry for a personality server here -- which had been reasoned out
independently and is now corroborated.

**`new_release_kernel/mach_servers/bootstrap.conf`** -- a real
bootstrap.conf from a working system:

```
# bootstrap.conf
-w default_pager default_pager 
-k startup vmlinux 
```

confirming the `[-flags] symtab_name path` format, and that the
bootstrap task's own flags come first.

**`pmk1.1/`** -- a third Mach 3.0 PMK tree, same version as ours,
differing from MkLinux in files we have patched (`hd.c`, `fd.c`,
`ipc_kobject.c`, `model_dep.c`). Useful as a cross-check, though it
carries the same bugs: its `getvtoc` still sizes the whole-disk
partition from `cmos_parm`, and its floppy code matches MkLinux's.

### Dates our tree

`DR2.1u6-wip971126.src.patch` (170k lines) and `u5-u6.patch` are MkLinux
DR2.1 update patches. Their one generic kernel change is to
`device/dev_name.c`, adding `lenunit = cp - name;` -- **which our tree
already has**. So our OSFMK is at or past DR2.1u6d. The rest of their
kernel changes are PPC and HP700 specific.

### What is not there

**No i386 `mach_init` program**, source or binary. The only one is
`new_release_kernel/mach_servers/mach_init`, which is PA-RISC, and the
`usr/` tree is a PA-RISC Linux userland. So the current step 4 blocker
is not solved here.

**No `libmach_sa`.** Neither `osfmk/` nor `pmk1.1/` has it; both ship
only the profiled, broken `libmach_sa_p`. This confirms that adding it
was necessary rather than a local workaround, and that the gap is
upstream.

### Complete inventory of the reference collection

Everything in `nmartin0/mach_stuff`, examined. Marked by what it is
worth to this project.

| item | what it is | worth |
|---|---|---|
| `DR3 (2)/` | full DR3 kernel tree, 28 MB | **behind our base** -- see below |
| `DR3_aswell/` | DR3 mklinux personality + build README | build procedure only; **no licence grant** |
| `DR3_powermac/` | prebuilt PowerPC export tree | wrong architecture; confirms no `libmach_sa` |
| `DR3/` | PowerPC host tools (`mig`, `migcom`, `config`, `makeboot`) | wrong architecture |
| `Change/DR3/` | 13 files, i386 | `elf.c`, `Buildconf` useful -- below |
| `Change/mklinux-1.0b2/` | i386 bootloader sources | identical to ours but for the copyright header |
| `fdsrc/` | MkLinux Project floppy driver, 2001, SWIM3 + Darwin IOKit | PowerMac hardware; not our 82077 |
| `System.map*` (4) | PowerPC Linux kernel symbol maps | nothing |
| `bootstrap.conf` | a real config with `-k -S` | **significant** -- see collocation below |
| `DR2.1u6-*.patch` | MkLinux DR2.1 u5 to u6 | dates our tree at or past u6d |
| `X11R6.3/`, `usr/`, `var/` | PA-RISC userland and X11 | nothing |

### The canonical build order, confirmed

`DR3 (2)/build_world` is the upstream build script, and our sequence
matches it:

```sh
build MAKEFILE_PASS=FIRST
build -here mach_services/lib/libcthreads
build -here mach_services/lib/libsa_mach
build -here mach_services/lib/libmach
build -here mach_services/lib/libmach_maxonstack
build -here file_systems          # for the bootstrap task
build -here bootstrap
build -here mach_kernel MACH_KERNEL_CONFIG=PRODUCTION
makeboot
#build -here default_pager        # commented out upstream
```

Two things worth noting. `file_systems` before `bootstrap` is required,
which this project worked out the hard way from a `-lsa_fs` link
failure. And **`default_pager` is commented out by default upstream**,
which explains why it is less exercised than the rest -- consistent with
its exported `default_pager_object.h` having been shipped without the
`default_pager_types.h` include.

`Change/DR3/osfmk/src/osc/Buildconf` carries i386-specific ODE settings,
including

```
on i386 setenv CARGS -D__NO_UNDERSCORES__
```

confirming that `-D__NO_UNDERSCORES__` is the canonical i386 flag, which
this project passes by hand in `ASFLAGS`.

`MKLINUX_BUILD.README` confirms the personality is built against the
microkernel's **export tree** of headers and libraries, distributed
separately as `DR3.osfmk.export.tgz`. That is exactly the role
`MACH_RELEASE_DIR` plays in `build-lites.sh`.

### Third sweep: the DR3 trees

The collection gained four DR3 trees (`DR3`, `DR3 (2)`, `DR3_aswell`,
`DR3_powermac`), four `System.map` files and a second `bootstrap.conf`.
DR3 is a later release than the DR2.1u6d our tree dates to, so the
obvious question was whether to move to it.

**The answer is no. Our MkLinux base is ahead of DR3 for this work.**
Measured on the files this project has patched:

| file | DR3 vs MkLinux |
|---|---|
| `kern/bootstrap.c` | MkLinux adds `multiboot.h`, `mb_info`, `boot_script.h` -- **the `-kernel` boot path we use**. DR3 has none of it |
| `intel/pmap.c` | MkLinux adds `INTEL_PTE_GLOBAL`. DR3 lacks it |
| `i386/AT386/hd.c` | MkLinux carries `/* XXX This hangs with qemu, disable it for now */`. DR3 is the unmodified original that hangs |
| `fd.c`, `ipc_kobject.c`, `bootstrap/elf.c` | identical |

So DR3 is the earlier state and MkLinux is a QEMU- and multiboot-aware
descendant of it. Moving to DR3 would lose the boot path.

**DR3 still has every bug we fixed.** Its `getvtoc` sizes the whole-disk
partition from `cmos_parm`, and its geometry selection is the same
two-arm form with no non-zero check. So those fixes are genuine
improvements over the final release, not local workarounds.

**DR3 still has no `libmach_sa`** -- only `libmach_sa_p`. That gap now
holds across `osfmk`, `pmk1.1`, `osfmk_2` and all four DR3 trees.

### A richer bootstrap.conf, and a mode we have not considered

The new top-level `bootstrap.conf` is more informative than the first:

```
-w default_pager /dev/boot_device/mach_servers/default_pager
-k -S 524288000 startup /dev/boot_device/mach_servers/vmlinux
```

Two things are new. Servers are named by **full path** rather than bare
name. And `-S 524288000` is a flag taking a numeric argument, which
`bootstrap.c:784` documents:

```c
case 'S':
    /* collocated server mapsize - implies -k */
```

`-k` sets `SERVER_IN_KERNEL_F`. So MkLinux ran its Linux personality
**collocated in the kernel's address space**, with a 500 MB map, rather
than as a separate task. That is a mode this project has not
considered for LITES. Not something to act on now, but worth knowing it
exists and is configured entirely from `bootstrap.conf`.

The four `System.map` files are PowerPC Linux kernel symbol maps
(`_stext` at `0x10000000`); not useful here.

### Second sweep: the most valuable items

**`linux/osfmach3/mach_init.c`** -- the source of the program LITES
wants, 91 lines. **Read only. It is not licensed for use here.** See
the licence note below before going near it.

It matches the strings in the PA-RISC binary exactly, and it is an
ordinary POSIX program: `open`, `dup`, `execve`, `fork`, `wait`,
`printf`, `_exit`. Nothing Mach-specific in it at all.

```c
if ((open("/dev/tty1", O_RDWR, 0) < 0) &&
    (open("/dev/ttyS0", O_RDWR, 0) < 0))
        printf("Unable to open an initial console.\n");
(void) dup(0); (void) dup(0);
execve("/etc/init", argv_init, envp_init);
execve("/bin/init", argv_init, envp_init);
execve("/sbin/init", argv_init, envp_init);
if (!(pid = fork())) do_rc("/etc/rc");
...
while (1) { if (!(pid = fork())) do_shell("/bin/sh"); ... }
```

So the first program is: open a console, try each init path, then spawn
`/bin/sh` in a loop.

### Licence: the personality tree carries no grant

This was got wrong once and is corrected here. `mach_init.c` was
described as "OSF code on the same terms as the rest of our tree, so we
can use it". **That is false.** Its entire notice is:

```c
/*
 * Copyright (c) Open Software Foundation, Inc.
 *
 */
```

A bare copyright notice with **no permission grant at all**. Compare a
file from our own kernel:

```c
/*
 * Copyright 1991-1998 by Open Software Foundation, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * ...
 */
```

Under default copyright, no grant means all rights reserved. Seeing
"Open Software Foundation" and "pmk1.1" and assuming the familiar
permissive terms is the same failure as assuming two libraries are the
same because their names look alike.

**The split is systematic, and measured:**

| tree | files carrying the grant |
|---|---|
| `linux/osfmach3` | **0 of 20** |
| `linux/arch/osfmach3_i386` | **0 of 16** |
| `osfmk/src/mach_kernel/kern` | 20 of 20 |
| `pmk1.1/src/mach_services/lib/libmach` | 20 of 20 |

The **kernel** trees are permissively licensed. The **personality**
trees are not. Treat everything under `linux/osfmach3` and
`linux/arch/osfmach3_i386` as read-only reference, in the same category
as XNU and GNU Mach.

### What may still be taken from it

Under the rule in the Licensing section: facts and design, never
expression.

- That LITES execs its first program at `/mach_servers/mach_init` and
  that it must be static. Both are facts about **our** system, already
  established from `server_init.c` and `s_execve`.
- That such a program is built with `gcc -static`. A fact about
  compilation.
- That a Unix first program opens a console, execs an init, and spawns
  a shell. This is the design of Version 7 UNIX `init` and is in every
  operating systems textbook; it predates this file by twenty years.

What must **not** happen: writing ours with that file open, or
reproducing its `execve` sequence, its argv and envp tables, or its
message strings.

**`Change/DR3/`** -- thirteen files from a release *later* than the
DR2.1 our tree dates to, and they are precisely the ones this project
has been working in, including `osfmk/src/bootstrap/elf.c`.

That file confirms the read-only `PT_LOAD` segment is a known upstream
problem, and shows upstream never solved it:

```c
} else {
#ifndef ppc
	    /* mklinux/ppc has a read-only section which is ignored */
	BOOTSTRAP_IO_LOCK();
	printf("ELF: Unknown program header flags 0x%x\n", ...
#endif /* ppc */
}
```

DR3 only suppresses the **warning** on ppc; the segment is still never
mapped. Our fix, extending `text_size` to cover it, is a genuine
improvement over upstream rather than a local workaround.

DR3's `elf.c` also carries an alternative implementation behind
`#ifndef ykpark`, which uses `trunc_page()` on both vaddr and offset --
worth knowing if segment alignment ever becomes a problem.

Its `conf/AT386/files` and `config.devices` differ from ours by enabling
PCI and NCR SCSI drivers, which is not our path today but is where to
look if more devices are ever wanted.

**`osfmk_2/export/powermac/include/mach/default_pager_object.h`**
contains the `#include <mach/default_pager_types.h>` that our exported
copy lacked, independently confirming that the missing include -- which
broke the `default_pager` build until the generated header was copied
over it -- was a real defect and not a local build accident.

### Also present, not yet examined

`ode/` (the build system), `X11R6.3`, `fdsrc`, `osfmk_2` (exports only),
`Change/` (which contains a DR3 tree).

## macMach5-92src: mixed licence, one useful piece

Its README: "The MacMach system started as the Berkeley **Tahoe**
release... converting all of the sources to compile with the GNU C
compiler".

**That matters for licensing.** 4.3BSD-Tahoe (1988) predates Net/2 and
4.4BSD-Lite, so it still carries AT&T-derived code -- the subject of the
USL litigation. LITES is 4.4BSD-Lite based and clean; Tahoe is not.

Checked rather than assumed, and the split is sharp:

| subtree | licence |
|---|---|
| `src/mach_kernel` | **20 of 20 sampled carry the CMU grant** -- permissive |
| `src/mach_servers` | mixed: 4 CMU, 13 Berkeley-agreement |
| `src/bin`, `src/etc` | Berkeley agreement -- **not usable** |

The userland notices read:

```
 * Copyright (c) 1980,1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 * @(#)init.c 5.10 (Berkeley) 1/10/88
```

That is the **pre-Net/2** form, referring to an agreement that
historically required an AT&T source licence, and the agreement is not
in the tree. It is not the modern BSD licence text. So `src/bin`,
`src/etc` -- including its `init` and shell -- are out, for the same
reason `OSF1-SRC-V2.0` is.

### The useful piece: a second mach_init

`src/mach_servers/mach_init/` is **CMU-granted** in all four files
(`main.c`, `service.c`, `test_service.c`, `waitfor.c`).

Comparing it with `user-mach4`'s:

| file | macMach (1992) | user-mach4 |
|---|---|---|
| `main.c` | 294 lines | 332 |
| `service.c` | 568 lines | 575 |

Same lineage, 46 differing lines in `main.c`. **user-mach4's is the
later revision** -- consistent with Helander's 1994 changes on top of
this CMU base -- and both implement `service_waitfor`.

So user-mach4's remains the one to port, and macMach's is a useful
second copy for cross-checking a question about the original.

`src/mach_servers` also holds `ux.28`, the UX BSD single-server, but its
licensing is in the mixed group and it is superseded by LITES anyway.

## OSF1-SRC-V2.0: proprietary. Do not use.

**This tree is Digital Equipment Corporation proprietary source and must
not be used, copied, or read for design.** Every file checked in
`sbin/init` carries:

```
 * Copyright (c) Digital Equipment Corporation, 1991, 1994
 * All Rights Reserved.  Unpublished rights reserved under the
 * copyright laws of the United States.
 * The software contained on this media is proprietary to and embodies
 * the confidential technology of Digital Equipment Corporation.
 * Possession, use, duplication or dissemination of the software and
 * media is authorized only pursuant to a valid written license from
 * Digital Equipment Corporation.
```

That is not a licence with restrictions; it asserts that **possession
itself** requires a written licence. It is categorically different from
the CMU, OSF and GPL notices elsewhere in the collection, all of which
grant something.

### What it contains, recorded only so nobody looks again

OSF/1 V2.0 **source**, in OSF subset format (`tar Zxf` each `OSCB*`
file). `OSCBSBIN200` alone holds source for a full base userland --
including `init` (with `init.c`, `getcmd.c`, `signals.c`, `output.c`,
`init_sec.c`), `sh`, another `mach_init`, and `mount`, `ls`, `cat`,
`cp`, `ps`, `fsck`, `newfs`, `disklabel` and around seventy more.

It is exactly what step 4 needs, and **we cannot use any of it.**

The permissively licensed alternatives already identified stand:
`user-mach4/etc/mach_init` under the CMU grant for the first program,
and for `/sbin/init` and a shell, a BSD-licensed userland obtained
elsewhere.

**If this tree is kept in the collection, it should be clearly marked.**
Its presence next to permissively licensed material is a hazard,
because the subset filenames give no hint of what is inside them.

## Provenance clarified: our base is not plain MkLinux

The collection holds `osfmk/` and `osfmk_random/`, which are **identical
to each other** (zero files differ) and are the **original MkLinux
OSFMK release**. Comparing them against the tree this project is built
on settles the lineage:

| tree | QEMU floppy fix | multiboot support |
|---|---|---|
| collection's `osfmk` / `osfmk_random` | **absent** | **absent** |
| `slp/osfmk-mklinux` (our base) | present | present |

So the chain is **OSF -> MkLinux -> slp's QEMU-adapted fork -> us**.
Earlier notes here called our base "MkLinux", which is imprecise: it is
a modernised fork of MkLinux, and the QEMU and multiboot work that makes
`-kernel` booting possible was added there, not by OSF or MkLinux.

That also explains why DR3 lacked multiboot despite being a later
official release: the multiboot work never went upstream.

**Measuring differences in these trees needs care.** Comparing the
collection's `osfmk` with ours shows 280 differing files, but a majority
differ **only in RCS keyword expansion** -- `$Header: /MkLinux/osfmk/...`
against `$Header: /u1/osc/rcs/...` -- because they are different
checkouts of the same code. Filter `Header:`, `Revision:` and `Log:`
lines before counting, or the noise swamps the signal.

`osfmk_anotherrandom/` is a PowerPC export tree, the same shape as
`osfmk_2/` and `DR3_powermac/`.

## mach4-UK22, the MIG implementations, and ode

### mach4-UK22 (Utah Mach 4, Bryan Ford)

Its README describes an i386 release "based on Remy Card's version of
CMU's MK83, with **modifications to the server bootstrap code to load
servers from a Linux ext2fs**", booting "directly from LILO as a
Linux-like boot image". Both are things this project has independently
arrived at.

**Licensing is mixed and was mapped file by file:**

| subtree | licence |
|---|---|
| `kernel`, `libmach`, `libthreads`, `mig` | CMU grant -- permissive |
| `bootstrap` | mostly CMU, but **six GPL files**: `ffs_compat.c/.h`, `minix_ffs_compat.c/.h`, `minix_fs.h`, `minix_super.h` |
| `bootstrap/ext2_file_io.c` | CMU grant -- permissive |

A `COPYING` with GPL v2 sits in `bootstrap/`. **This check is what led to
finding the same GPL files built into our own tree** -- see ROADMAP.

**A different pager design.** `bootstrap/def_pager_setup.c` gives the
default pager a **paging file** at `<server_dir>/paging_file` via
`add_paging_file(master_device_port, file_name)`, rather than a raw
device. Ours uses a raw disk named in `bootstrap.conf`. Worth knowing
both routes exist.

### MIG: we have our own source and do not need theirs

**`gnu-osfmig` and `osfmig-0.90` are both GPL**, so neither is usable.
`osfmig-0.90` describes itself as a distribution of "the OSF Mach 3.0
interface generator MiG", the same lineage as ours.

We do not need them. **OSFMK ships MIG in source** at
`mach_services/lib/migcom/` -- 10,688 lines with `lexxer.l` and
`parser.y` -- and `mach_services/lib/Makefile` already lists it as
`SETUP_SUBDIRS = migcom`.

Today `build-lites.sh` uses the **prebuilt `migcom` binary** from
`tools/i386/i386_linux/hostbin/` (321 KB), which cannot be inspected or
rebuilt. Building it from the tree's own source would remove a binary
blob from the build. A first attempt failed on an ODE sandbox path
rather than on the code, so this is open rather than ruled out.

### ode

`ode/bin` holds the ODE tools as **PA-RISC binaries** -- not usable
here; we use a Linux port. The inventory is still useful as a record of
what the environment provides: `build`, `make`, `md`, `mksb`, `workon`,
`mklinks` (shadow source trees), `genpath`, `makepath`, `release`,
`resb`, `sbinfo`, and the `bci`/`bco`/`bcs` source-control commands.

## user-mach4: the LITES userland

`user-mach4/` is the single most valuable thing in the collection. Its
README states what it is:

> The user collection is a group of programs and libraries that work
> with Mach and **Lites**... taken from the USER collection, release 22
> (USER22) distributed by CMU and put into a "mach4" style configure
> framework and **some of them were modified to work with Lites** (most
> notably ps, top and w)... they include **mach_init, which is required
> to boot**.

University of Utah, April 1996, Stephen Clawson. This is the userland
that goes with the personality we are booting.

### BSD licence forms, and which apply here

Three different notices appear in BSD-derived code in and around this
project. They are not interchangeable, and the difference decides
whether something is usable.

### 1. The 4-clause BSD licence -- what LITES carries

**536 files in our LITES tree** carry the advertising clause:

```
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
```

**UC Berkeley retired that clause on 22 July 1999**, retroactively, for
code copyrighted by the Regents. So this is effectively **3-clause BSD**
and every "4-clause" reference to LITES in these notes should be read
that way. It is fully compatible with this project.

### 2. The post-settlement USL notice -- also clean

**66 LITES files** additionally carry:

```
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
```

"**reproduced herein with the permission of**" is the 4.4BSD-Lite,
post-USL-settlement form. This is the settled, clean lineage, which is
why LITES is a sound base and Tahoe-derived code is a different
question.

### 3. The pre-Net/2 pointer -- unclear, and treated as unusable

`macMach5-92src`'s userland, and two files in `user-mach4`, carry:

```
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
```

This is **not licence text with an advertising clause**; it is a pointer
to a separate agreement, and that agreement is not in the collection.
The 1999 retirement amends clause 3 of the licence text -- whether it
reaches a file that only references an unincluded agreement is a
question this project is not equipped to answer.

**So these are treated as unusable on grounds of "form unclear and the
referenced agreement absent", not "definitely encumbered".** If the
agreement is located and turns out to be permissive, that judgement can
be revisited. In the meantime the affected material -- MacMach's `init`
and shell, `user-mach4`'s `w` and `hostinfo` -- is not needed, because
permissive alternatives exist.

### Summary

| notice | status |
|---|---|
| 4-clause BSD | **usable** -- 3-clause since 1999 |
| 4.4BSD-Lite USL permission notice | **usable** -- post-settlement |
| CMU Mach grant | **usable** |
| OSF grant | **usable** |
| "Berkeley software License Agreement specifies..." | **not used** -- form unclear, agreement absent |
| GPL (GNU Mach, `minixfs`, mach4 `bootstrap`) | incompatible -- design only |
| DEC proprietary (`OSF1-SRC-V2.0`) | **do not read** |

## Licensing## Licensing: permissive, with two exceptions

**84 of 86 C files carry an explicit CMU grant** -- "Permission to use,
copy, modify and distribute this software and its documentation is
hereby granted". That is compatible with this tree.

The two exceptions are `bin/w/w.c` and `bin/hostinfo/hostinfo.c`, which
say only that "The CMU software License Agreement specifies the terms
and conditions", referring to a document **not present in the
collection**. Treat those two as unlicensed. Neither is needed to boot.

**`etc/mach_init/main.c` and `service.c` both carry the explicit
grant.**

### What is in it

Thirty-two programs, of which these matter to us:

| program | why |
|---|---|
| `mach_init` | **required to boot** -- `main.c` 332 lines, `service.c` 575 |
| `machid`, `snames` | required for gdb support under LITES |
| `ps`, `top`, `w` | modified specifically for LITES |
| `vminfo`, `vmstat`, `zprint`, `hostinfo`, `pinfo`, `stacks`, `thstate` | kernel inspection from userland |
| `swapon` | **does not work** -- the README says so plainly, for mach3 or mach4. Our `bootstrap.conf` route to giving the pager a device was the right one |

Seven libraries: `libcmucs` (6 C files plus per-architecture
directories **including i386**), `libxmm` (26 C files), `libmachid`,
and four that are MIG interface definitions only -- `libservice`,
`libnetname`, `libnetmemory`, `libenv`.

### Building it

```sh
../user/configure --prefix=/usr/mach4
gmake
```

`mach_init` itself needs `-lservice -lthreads -lmach -lcmucs` and
`-static`.

### Settled: which `service.defs` is authoritative

OSFMK's `libservice/Makefile` takes its definitions by VPATH from
`mach_services/include/servers/service.defs`. The two differ, and not
only in the copyright header:

| tree | routines |
|---|---|
| OSFMK | `service_checkin` |
| user-mach4 | `service_checkin`, **`service_waitfor`** |

**user-mach4's is authoritative for `mach_init`.** Three pieces of
evidence:

- `etc/mach_init/service.c` **implements** `do_service_waitfor` -- it is
  the service *server*, so it needs server stubs for both routines
- `bin/waitfor/waitfor.c` **calls** `service_waitfor` -- the client side
- `service.c`'s own history says "Added service_waitfor", so
  user-mach4's interface is the later one

OSFMK's copy is the earlier, reduced interface. Building `mach_init`
against it would generate a dispatch table without `service_waitfor`,
and the `waitfor` client would not work, though `mach_init` itself would
still run.

Licensing is fine either way: OSFMK's carries the OSF grant and
user-mach4's carries the CMU grant, both explicit.

## Re-surveyed: xMach's LITES does carry post-u3 fixes

An earlier note dismissed xMach wholesale because it targets Mach 4 and
its pager uses `memory_object_establish`. That was right about the pager
and **wrong as a reason to stop looking**. A diff against a pristine
1.1.u3 clone shows real post-u3 work, some directly relevant.

Of the first 120 differing files, **58 differ only in CVS log headers**
added in 2000 and **62 carry real changes**. The `ChangeLog` there is
the u3 release's own, dated March 1996, so it describes changes already
in our base; the post-u3 work is undocumented and must be found by
diffing.

Excluding architectures we do not build, the substantive changes are in
the emulator (`e_linux.c`, `e_linux_trampoline.c`, `e_linux_sysent.c`,
`emul_exec.c` -- mostly Linux binary support) and in four files this
project has been debugging directly: `liblites/exec_file.c`,
`server/kern/init_main.c`, `server/serv/device_misc.c` and
`server/kern/vfs_conf.c`.

### The finding that matters: ELF binary classification

Our boot log shows the emulator classifying our i386 ELF server as
`BT=20`, which `ATSYS_NAMES` spells `hpelf` -- an HP-UX ELF binary. The
cause is in pristine `liblites/exec_file.c`:

```c
if ((hdr->magic == 0x464c457f)                        /* ELF */
    && (unsigned) hdr->elf.ehdr.e_entry > 0x10000000) {
        return;
}
```

LITES classifies an ELF as its own **only if the entry is above
`0x10000000`**. Our server's entry is `0x8049320`, so the test fails and
classification falls through.

xMach adds the `else` branch, reading program headers for ELF binaries
below that threshold. That is the shape of the fix needed once a first
program is built as an i386 ELF, since it will be misclassified the
same way.

**Not fixed in xMach:** `exec_file.c:273` still reads
`switch ((tmp >> 16) && 0x3ff)`, `&&` where `&` was meant. Ours to fix.

**Licensing:** the xMach modifications are by other hands with their own
headers -- reference only: read the design, write our own.

## Superseded: surveyed and rejected

`github.com/neozeed/xMach` mirrors the SourceForge xMach project and
carries a LITES tree with changes dated around 2000. It was checked in
case those changes overlapped ours. **They do not, and the reason is
structural rather than incidental.**

xMach is **Mach 4 + LITES**, the Utah/CMU lineage. Ours is OSFMK 7.3,
the OSF lineage. Both start from `Lites.1.1.u3`, and 308 files differ,
but the divergence is adaptation to a different kernel.

The decisive evidence is the pager, the same dividing line identified
earlier in this survey:

| tree | how a memory object is made ready |
|---|---|
| xMach (Mach 4) | `memory_object_establish` **and** `memory_object_ready` |
| MkLinux / ours (OSFMK 7.3) | `memory_object_change_attributes` |

`xmm_interface.c:117` and `:166` still call both routines, and OSFMK 7.3
removed both -- `mach.defs:247` and `:864` keep their message ids as
`skip`. So xMach's pager could not work here, and confirms from a third
tree what MkLinux and OSFMK 6.1 already showed.

None of the modernisation work overlaps either. xMach leaves untouched
every 1990s construct we had to fix:

| construct | xMach |
|---|---|
| `gensym.awk` literal newline in a string | unfixed |
| `case SIG_IGN:` pointer constant as a case label | unfixed |
| `*((char *)to)++`, a cast used as an lvalue | unfixed |
| `default_root[] = "hd0a"` | unchanged |

That is expected: their README says to cross-compile with gcc 2.7.2.3
and binutils 2.12. They never met a modern toolchain, so they never had
these problems.

### Worth remembering from it

Two genuine additions, neither useful now but both interesting later:

- **`server/miscfs/devfs/`**, about 1100 lines -- a device filesystem,
  which LITES 1.1u3 does not have. Relevant if `/dev` ever becomes
  awkward to populate by hand.
- **`emulator/e_linux.c`**, about 1700 lines, plus
  `e_linux_getcwd.c` -- a Linux personality emulator. Interesting far
  down the roadmap, though written against Mach 4.

The conclusion for anyone tempted to revisit this: xMach is a sibling
port, not a newer one. Take design ideas from it if useful, but its
kernel interface assumptions are the wrong ones for this tree.

## Licensing

**Every line of code in this project is written here.** No source is
copied from any other tree, and none of the reference trees is used as
anything but reading material.

### The rule

- **Reading a reference implementation to understand a design** -- fine.
  Copyright protects expression, not method. Learning *that* a floppy
  controller must have its reset interrupt acknowledged before it
  accepts another command is a fact about hardware.
- **Copying its expression** -- not done. Not a function, not a
  structure layout transcribed from someone's header, not a block of
  logic reworded.
- **Invoking a compiler, kernel or library interface** -- not copying at
  all, and worth stating because it can look like it at a glance.

### Compiler intrinsics are not imported code

`include/i386/stdarg.h` now reads:

```c
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start((ap), (last))
#define va_arg(ap, type)   __builtin_va_arg((ap), type)
#define va_end(ap)         __builtin_va_end(ap)
```

`__builtin_va_list` and the `__builtin_va_*` operators are **language
constructs the compiler recognises**, in the same category as `sizeof`,
`__asm__` and `__attribute__`. They expand to nothing textual; the
compiler handles them internally, and on i386 they generate direct stack
arithmetic with no library call at all. Nothing from GCC's own
`stdarg.h` was read or copied -- these four lines were written here from
the documented interface.

This is the standard way any codebase supplying its own headers under
`-nostdinc` declares varargs, and it is what the permissively licensed
BSDs do in the same file.

For completeness on the licence question that does not arise here: GCC
carries the GCC Runtime Library Exception specifically so that compiling
with GCC imposes nothing on the output. That exception is about linking
GCC's runtime, and these builtins link nothing.

### The reference trees, and what each may be used for

| tree | licence | use |
|---|---|---|
| MkLinux `osfmk/` | OSF, same as ours | **not** arm's length -- it is the same code; diffing establishes provenance |
| MkLinux `mklinux/` | OSF | worked example against this exact kernel; read for design |
| OSFMK 6.1 | OSF | ancestor; read to see what changed and why |
| XNU / Darwin | APSL | **read only.** Incompatible. Consult for design, never copy |
| GNU Mach | GPL | **read only.** Incompatible. Consult for design, never copy |
| xMach | Mach 4 lineage | read only; and see the survey above -- its interfaces are the wrong ones |

**Practice:** check a file's header before taking anything from it,
record in the commit message when a reference tree was consulted, and
when implementing something after reading a reference, write it from the
interface documentation rather than with their source open.


Compatible, and cleaner than UX.

- **Core (UC Berkeley lineage):** 4-clause BSD text, but 4.4BSD-Lite
  derived -- the post-settlement clean branch, marked by the "with the
  permission of UNIX System Laboratories" note. UC retroactively
  withdrew the advertising clause in 1999, so for UC-copyrighted files
  it is effectively BSD-3-Clause today.
- **Helander's Mach glue:** a permissive HPND-style grant, same family
  as OSFMK 7.3's own notice, no advertising clause.

Neither is copyleft.

**On UX and the Caldera argument:** the claim that Caldera's 2002 grant
implicitly freed 4.3BSD is not safe to rely on. That grant names UNIX
V1-V7 and 32V rather than derivatives, 4.3BSD contains much more than
32V-derived material, the *USL v. BSDi* settlement is what actually
addressed 4.3BSD and produced 4.4BSD-Lite as the clean branch, and
Caldera's authority was contested afterwards in *SCO v. Novell*. LITES
avoids the question entirely.

## Tried: configure and liblites build against our tree

Not a thought experiment any more. The following was done and works.

### Constructing a MACH_RELEASE_DIR

LITES wants `$(MACH_RELEASE_DIR)/{include,include/mach,lib}` and
`mig`/`migcom`. Our ODE export tree provides all of it:

```sh
MR=/tmp/machrel
mkdir -p $MR/bin $MR/libexec
ln -sfn $MK_BUILD/export/at386/include $MR/include
ln -sfn $MK_BUILD/export/at386/lib     $MR/lib
HB=osfmk7.3/osfmk/tools/i386/i386_linux/hostbin
ln -sf $PWD/$HB/mig    $MR/bin/mig
ln -sf $PWD/$HB/migcom $MR/bin/migcom
ln -sf $PWD/$HB/migcom $MR/libexec/migcom
```

`export/at386/include/mach/` contains the `.defs` files, including
`bootstrap.defs`, so LITES generates its Mach stubs from **our**
definitions with **our** `mig`. That was the central claim of this
survey and it is now demonstrated rather than argued.

### Configure and build

```sh
sh /path/to/lites/configure \
    --with-release=$MR \
    --with-config="STD+WS+osfmach3" \
    --host=i386-unknown-mach3 --target=i386-unknown-mach3

GI=$(gcc -m32 -print-file-name=include)
make CXXX="-m32 -isystem $GI" CHXXX="-m32"
```

`--with-config="STD+WS+osfmach3"` is **essential and not the default**.
Without it `LITES_CONFIG` is `STD+WS`, `OSFMACH3` and `OSF_LEDGERS` stay
undefined, and every device call has the wrong arity:

```
block_io.c:141: error: incompatible type for argument 4 of 'device_open'
block_io.c:132: error: too few arguments to function 'device_open'
```

That is not an incompatibility. LITES already brackets the extra
arguments correctly:

```c
rc = device_open(device_server_port,
#if OSF_LEDGERS
                 MACH_PORT_NULL,     /* ledger */
#endif
                 mode,
#if OSFMACH3
                 security_id,        /* security token */
#endif
```

which matches our `device.defs` exactly -- OSFMK 7.3 replaced
`device_open` with a ledger-and-token form and left the old message id
as `skip; /* nmk15: device_open */`. There are 66 such call sites across
`device_open`, `device_read`, `device_write`, `device_get_status`,
`device_set_status` and `device_close`, and the single config option
fixes all of them.

`CXXX` and `CHXXX` are user hooks in `conf/Makerules` that append to
`TARGET_CFLAGS` and `HOST_CFLAGS`, so the toolchain flags go in without
patching LITES.

### Result

`liblites` **compiles**. The build reaches `server/` and then fails in a
generated file:

```
bsd_types_gen.symc:8:6: error: missing terminating " character
```

`gensym.awk` emits output a modern cpp rejects -- structurally the same
problem OSFMK's own `genassym` had, and the next thing to fix.

### Further: MIG interoperates, and the server tree starts building

With `tools/lites/gensym-newline.patch` applied and
`tools/lites/mig-shim.sh` in place of `$MACH_RELEASE_DIR/bin/mig`:

- `bsd_types_gen.symc` compiles and `bsd_types_gen.h` is generated
- **our `mig` runs LITES's `.defs` against our `mach_types.defs`** and
  produces `bsd_1_server.c` and `bsd_1_server.h`
- `-DOSF_LEDGERS=1 -DUNTYPED_IPC=1` appear on the compile lines, so the
  `osfmach3` arms are live

That is the interoperation this survey set out to test, working at the
tool level: LITES source, our MIG, our definitions, one output.

The build then stops on a LITES packaging inconsistency rather than
anything to do with OSFMK. `conf/files:303` lists
`serv/bsd_server.c`, while the MIG rule derives its output name from
`bsd_1.srv` and so produces `bsd_1_server.c`. The two disagree, and make
passes the unresolved bare name to gcc:

```
cc1: fatal error: bsd_server.c: No such file or directory
```

Untangling that is LITES build-system work and is where the next session
should start.

### Further still: 22 objects, and the first real API difference

Adding `tools/lites/lites-compat.h` via `-include` carried the build
through `device_reply_hdlr.c` and 14 more objects.

That header covers the one genuine API difference found so far.
OSFMK 7.3 uses untyped (NDR) IPC, where the MIG error reply is
`mig_reply_error_t` -- a `Head`, an `NDR_record_t` and a `RetCode`. LITES
uses the typed-IPC name `mig_reply_header_t` in 13 places, which had a
`mach_msg_type_t` where the NDR record now is. It touches the differing
member, `RetCodeType`, in only two places and both are inside its `#else`
arm for typed IPC, which `UNTYPED_IPC` compiles out -- so the two
structures are interchangeable for every use that remains and a plain
typedef suffices.

The build then reaches `server/net/` and stops on a LITES internal
inconsistency: `include/sys/malloc.h:272` defines
`bsd_malloc(size, type, flags)` as `malloc(size)`, because the LITES
server has a one-argument malloc rather than the BSD kernel's
three-argument one, but `net/radix.h`'s KERNEL arm was never converted
and still calls `malloc` and `free` with BSD arity directly.
`tools/lites/radix-bsd-malloc.patch` routes it through the wrapper.

### Further still: ~35 objects, and the shape is now clear

Continuing past `server/net/` turned up three more issues, all the same
kind, and each one unblocked a batch of files rather than a single file.

**BSD malloc arity, 53 sites in 46 files.** LITES's `sys/malloc.h`
supplies `MALLOC`, `FREE`, `bsd_malloc` and `bsd_free`, all resolving to
a one-argument allocator, but the BSD-derived trees under `server/net`,
`server/netccitt` and `server/isofs` were never converted and still call
`malloc(size, type, flags)` and `free(addr, type)` directly. Patching 53
sites would be a large change against LITES; two variadic macros in
`tools/lites/lites-compat.h` drop the extra arguments instead and the
existing calls compile unchanged.

One detail matters there. The macros must expand so that a later
*declaration* of `malloc` is still valid C:

```c
#define malloc(sz, ...)  (malloc)(sz)     /* right */
#define malloc(sz, ...)  (malloc)((unsigned long)(sz))   /* wrong */
```

With the cast, a header declaring `void *malloc(unsigned long);` expands
to `(malloc)((unsigned long)(unsigned long))` and fails. Without it the
declaration becomes `extern void *(malloc)(unsigned long);`, which is
legal. GCC reports such failures at the macro's *definition* site, which
is misleading -- the real error is at whichever header declares the
function.

**`-fno-builtin` is required.** BSD's kernel `log(level, fmt, ...)`
collides with GCC's builtin `log(double)`, giving "too many arguments to
function 'log'". OSFMK's own build uses `-fno-builtin` for the same
reason.

**Pointer constants as case labels.** `kern_sig.c` has `case SIG_DFL:`
where `SIG_DFL` is `(void(*)())0`. K&R C accepted it; modern C requires
an integer constant expression. This is the current stopping point and
needs a LITES patch rather than a shim.

The full flag set that gets this far:

```sh
GI=$(gcc -m32 -print-file-name=include)
make CXXX="-m32 -fno-builtin -isystem $GI \
          -include /path/to/tools/lites/lites-compat.h" \
     CHXXX="-m32"
```

### Reproducible: one script, 159 objects, 5 undefined symbols

```sh
MK_BUILD=~/.cache/mk7.3 ./tools/lites/build-lites.sh ~/lites-1.1.u3 ~/lites-build
```

Verified from pristine clones of both repositories. It applies
`tools/lites/lites-osfmk73.patch`, builds a `MACH_RELEASE_DIR` from the
OSFMK export tree, configures with `osfmach3` and builds.

The flag set, with the reason for each:

| flag | why |
|---|---|
| `-std=gnu89` | GCC 14 makes K&R definitions and implicit int hard errors. GCC 13 did not, so this is easy to miss. |
| `-fno-builtin` | BSD's kernel `log(level, fmt, ...)` vs GCC's builtin `log(double)` |
| `-fgnu89-inline` | `cthreads.h` uses `extern __inline__`, which C99 rules emit per translation unit |
| `-fcommon` | tentative definitions in headers; GCC 10+ defaults to `-fno-common` |
| `-fno-stack-protector` | no `__stack_chk_fail_local` in this environment |
| `-D__NO_UNDERSCORES__` | `i386/asm.h` decorates `ENTRY(htonl)` as `_htonl` unless this is set. Without it every `htonl`/`ntohl` reference is undefined -- 322 of them. |
| `AWK=nawk` | the generators need nawk extensions; configure picks mawk |
| `LIBS` repeated | `libsa_mach` and `libmach` reference each other, and ld reads archives once |

Two more things the script handles:

**`crt0.o` lives inside `libsa_mach.a`.** OSFMK does not ship it
standalone, so `$MACH_RELEASE_DIR/lib` must be a real directory with the
object extracted into it, not a symlink to the export tree.

**The first make pass fails on `bsd_server.c`** and the second succeeds.
Make resolves it through VPATH only once the MIG outputs exist. The
script runs two passes.

### What the patch fixes

`tools/lites/lites-osfmk73.patch`, 8 files. The largest single win was
`vnode_if.sh`: it calls `bail()`, which the script never defines, so
both mawk and nawk abort at parse time and emit a 97 line stub instead
of the full 727 line `vnode_if.c`. That alone accounted for about 700 of
the undefined symbols. Defining `bail` fixes it.

The rest: `gensym.awk` and `newvers.sh` emitting literal newlines inside
string literals, pointer constants as `case` labels in `kern_sig.c` and
`serv_syscalls.c`, a cast used as an lvalue in `user_copy.c`, and two
Mach structure members that moved on in `vn_pager_misc.c` and
`xmm_interface.c`.

### Researched: the gap is NORMA/XMM, and it is one function wide

Comparing against OSFMK 6.1, XNU Rhapsody DR5.3 and the 7.3 tree itself
identifies what LITES's OSFMACH3 pager arm was written for, and it is
not a generic "older OSF Mach".

**`memory_object_establish` is a NORMA routine.** In OSFMK 6.1 it lives
in `norma/xmm_user.c`, is renamed to `k_memory_object_establish` by
`norma/xmm_server_rename.h`, and its body is:

```c
panic("memory_object_establish is not implemented\n");
```

It was part of NORMA, Mach's multicomputer/distributed memory layer, and
was **already unimplemented in 6.1**. The `memory_object.defs` comments
describe the protocol it belonged to: a discard request is answered with
either `memory_object_establish` or a discard. That is also where
`seqnos_memory_object_discard_request` comes from.

**OSFMK 7.3 removed NORMA entirely.** There is no `norma/` directory;
the mentions in `conf/files` are historical log entries. `mach.defs:247`
keeps the message id reserved as
`skip; /* was memory_object_establish; old port_set_backlog */`.

**XNU Rhapsody does not have it either**, which is consistent: the
lineage that became XNU dropped NORMA at the same point.

So LITES's file name is the clue that was there all along --
`xmm_interface.c`. Its OSFMACH3 arm targets a NORMA-enabled OSF Mach,
and the name says so.

#### MkLinux confirms the fix, and supplies the idiom

`github.com/slp/osfmk-mklinux` settles it, and more strongly than a
comparison would: **our OSFMK 7.3 is a copy of MkLinux's**. Diffing
`osfmk/src/mach_kernel` between the two trees gives exactly eleven
differing files, and they are exactly our eleven fixes:

```
i386/pio.h              i386/locore.S           i386/i386_rpc.c
i386/hardclock.c        i386/AT386/model_dep.c  i386/AT386/lpr.c
i386/AT386/fd.c         intel/pmap.c            kern/bootstrap.c
kern/ipc_kobject.c      kern/startup.c
```

Nothing else differs. So MkLinux's pager is not an analogous
implementation on a similar kernel -- it is an implementation against
*this* kernel, and its `memory_object` interface is byte-for-byte the
one we export.

Its OSFMK is therefore the **same generation as ours**: no `norma/` directory, and `mach.defs:247` reads
the identical `skip; /* was memory_object_establish; old
port_set_backlog */`. So MkLinux ran a real personality on an OSFMK with
NORMA already removed, which is exactly our situation, and its pager is
the canonical example.

`mklinux/src/osfmach3/server/inode_pager.c:905`, `inode_object_init`,
ends with:

```c
/*
 * Tell the micro-kernel that the memory object is ready on our side.
 */
attributes.copy_strategy    = imo->imo_copy_strategy;
attributes.cluster_size     = PAGE_SIZE;     /* or 0 for the default */
attributes.may_cache_object = imo->imo_cacheable;
attributes.temporary        = FALSE;
kr = memory_object_change_attributes(mem_obj_control,
                                     MEMORY_OBJECT_ATTRIBUTE_INFO,
                                     (memory_object_info_t) &attributes,
                                     MEMORY_OBJECT_ATTR_INFO_COUNT,
                                     MACH_PORT_NULL);
```

Its own comment -- "tell the micro-kernel that the memory object is
ready on our side" -- is precisely what `object_ready = TRUE` meant in
the NORMA establish call. The semantic did not disappear; it moved into
`change_attributes`, and the field vanished because being ready is now
implied by making the call.

This also confirms the second half. MkLinux's
`inode_object_discard_request` at line 895 is a one-line `panic()`. A
stub is the correct implementation, because this generation of OSFMK
never initiates the discard protocol.

One difference worth noting: MkLinux uses the **plain**
`memory_object_server`, not the sequence-numbered one -- zero `seqnos_`
references in its whole server. LITES chose the seqnos variant, and our
`libmach` does provide `Smem_svr`, so that choice remains workable. But
if the seqnos path gives trouble later, the plain interface is the
better-trodden one for this kernel.

Cross-checked against OSFMK 6.1 (`github.com/nmartin0/osfmk6.1`), whose
`norma/xmm_user.c:410` shows the NORMA layer doing the same thing by
either `K_SET_READY(mobj, OBJECT_READY_TRUE, MAY_CACHE_FALSE, modwc,
MEMORY_OBJECT_COPY_SYMMETRIC, PAGE_SIZE, ...)` or a plain
`memory_object_init`. Same four attributes, same intent, three
different spellings across three kernel generations.

#### The practical consequence: one function

Mapping the conditionals in `xmm_interface.c` shows `#if OSFMACH3` wraps
only the **initialisation** path:

| handler | line | arm |
|---|---|---|
| `seqnos_memory_object_init` | 136 | `#else` of `#if OSFMACH3` |
| `data_request` | 236 | top level |
| `data_unlock` | 336 | top level |
| `lock_completed` | 499 | top level |
| `data_return` | 557 | top level |
| `change_completed` | 572 | top level |
| `terminate`, `copy` | 176, 224 | top level |

Everything except initialisation is shared. The OSFMACH3 arm calls
`memory_object_establish` where the other defines
`seqnos_memory_object_init`, and that single substitution is the whole
incompatibility.

So the fix is not "write a pager". It is:

1. Provide `seqnos_memory_object_init` for the OSFMACH3 arm, doing what
   the establish call was meant to do, against 7.3's interface --
   `memory_object_change_attributes` with a
   `memory_object_attr_info` is the closest equivalent, and
   `vn_pager_misc.c` already calls it.
2. Provide `seqnos_memory_object_discard_request`, which can be a stub
   returning failure: it is the NORMA discard protocol, which 7.3 never
   initiates. `Smem_svr` references it only because the `.defs` still
   reserves the message.

Both belong in the OSFMACH3 arm of `xmm_interface.c`, which keeps the
change inside LITES and inside the patch series already carried here.

### Superseded framing: the real incompatibility

The three non-libgcc symbols are one problem, and it is the first
substantive mismatch found in this whole effort -- not a toolchain
issue, an actual interface divergence.

`memory_object_establish` does not exist in OSFMK 7.3.
`mach/mach.defs:247` reads:

```
skip;	/* was memory_object_establish; old port_set_backlog */
```

It was removed. LITES's `xmm_interface.c` calls it from its
`#if OSFMACH3` arm, so that arm targets an OSF Mach from before the
removal.

The two `seqnos_` handlers are the same divergence seen from the other
side. `Smem_svr.o` inside our `libmach.a` is the MIG **server** for the
sequence-numbered memory object interface: it provides
`seqnos_memory_object_server` and expects the pager to implement seven
handlers. LITES implements five of them in its OSFMACH3 arm. Of the
other two, `seqnos_memory_object_init` **is** defined in
`xmm_interface.c`, but at line 136, inside the `#else /* OSFMACH3 */`
arm -- so enabling `osfmach3`, which is required for the device call
arity, compiles it out. `seqnos_memory_object_discard_request` is not
defined anywhere in LITES.

So LITES has two pager implementations, and neither matches 7.3: the
OSFMACH3 one calls a routine 7.3 deleted, and the other one is written
against the older typed interface.

This is unsurprising in hindsight. The external pager interface is the
part of Mach that changed most between versions, and it is exactly where
a personality built for one OSF Mach would diverge from another.

Resolving it means writing the missing handlers against 7.3's actual
`memory_object` interface, using the 21 `memory_object_*` routines
`libmach` does export -- among them
`memory_object_change_attributes`, which is the closest thing 7.3 has to
what `memory_object_establish` did. That is real porting work rather
than a shim, and it is the first task in this effort that is.

### Done: every OSFMK-side symbol resolves

`tools/lites/lites-osfmk73.patch` now carries the pager work, and the
link is down to `__divdi3` and `__moddi3` alone -- libgcc helpers that
are absent only where no 32-bit libgcc is installed. Every symbol that
was ours is resolved.

Three changes in `xmm_interface.c` did it.

**`seqnos_memory_object_init` for the OSFMACH3 arm**, following
MkLinux's `inode_object_init` exactly: fill a
`memory_object_attr_info_data_t` with `copy_strategy`, `cluster_size`,
`may_cache_object` and `temporary`, then call
`memory_object_change_attributes` with `MEMORY_OBJECT_ATTRIBUTE_INFO`.
The rest of the body -- vnode lookup, pager wiring, `ux_server_add_port`
-- is identical to the `#else` arm's version.

Worth recording why neither existing arm worked: the OSFMACH3 arm calls
`memory_object_establish`, removed as a NORMA routine, and the `#else`
arm calls `memory_object_ready`, which `mach.defs:864` shows was also
removed ("was skip; memory_object_ready"). **Both** of LITES's pager
initialisation paths target routines 7.3 deleted, and both were folded
into `change_attributes`. That is why MkLinux is the only usable
template rather than one of two options.

**`seqnos_memory_object_discard_request`** as a panic stub, matching
MkLinux's `inode_object_discard_request`.

**`seqnos_memory_object_notify`'s establish call** replaced by a panic.
That handler belongs to the NORMA notify protocol; `Smem_svr` does not
reference `seqnos_memory_object_notify` at all, so it is unreachable on
this kernel. The attribute setting it used to carry now happens in
`init`.

### Superseded: the 5 that remain

```
__divdi3, __moddi3                      libgcc helpers
memory_object_establish                 in mach.defs, not in any library
seqnos_memory_object_discard_request    handlers for the MIG pager server
seqnos_memory_object_init
```

The first two are absent only where no 32-bit libgcc is installed; on a
host with working multilib they resolve. The other three are OSFMK-side:
`memory_object_establish` is declared in our `mach/mach.defs` but is not
compiled into `libmach` or `libsa_mach`, and the two `seqnos_` handlers
are wanted by a generated MIG server inside our own libraries. Which
`.defs` are compiled into which library, and whether the export tree is
missing one, is the next question -- and the first in this whole effort
that is ours rather than LITES's.

## Surveyed and rejected: xMach's LITES

`github.com/neozeed/xMach` mirrors the SourceForge xMach project and
carries a LITES tree with changes dated around 2000. It was checked in
case those changes overlapped ours. **They do not, and the reason is
structural rather than incidental.**

xMach is **Mach 4 + LITES**, the Utah/CMU lineage. Ours is OSFMK 7.3,
the OSF lineage. Both start from `Lites.1.1.u3`, and 308 files differ,
but the divergence is adaptation to a different kernel.

The decisive evidence is the pager, the same dividing line identified
earlier in this survey:

| tree | how a memory object is made ready |
|---|---|
| xMach (Mach 4) | `memory_object_establish` **and** `memory_object_ready` |
| MkLinux / ours (OSFMK 7.3) | `memory_object_change_attributes` |

`xmm_interface.c:117` and `:166` still call both routines, and OSFMK 7.3
removed both -- `mach.defs:247` and `:864` keep their message ids as
`skip`. So xMach's pager could not work here, and confirms from a third
tree what MkLinux and OSFMK 6.1 already showed.

None of the modernisation work overlaps either. xMach leaves untouched
every 1990s construct we had to fix:

| construct | xMach |
|---|---|
| `gensym.awk` literal newline in a string | unfixed |
| `case SIG_IGN:` pointer constant as a case label | unfixed |
| `*((char *)to)++`, a cast used as an lvalue | unfixed |
| `default_root[] = "hd0a"` | unchanged |

That is expected: their README says to cross-compile with gcc 2.7.2.3
and binutils 2.12. They never met a modern toolchain, so they never had
these problems.

### Worth remembering from it

Two genuine additions, neither useful now but both interesting later:

- **`server/miscfs/devfs/`**, about 1100 lines -- a device filesystem,
  which LITES 1.1u3 does not have. Relevant if `/dev` ever becomes
  awkward to populate by hand.
- **`emulator/e_linux.c`**, about 1700 lines, plus
  `e_linux_getcwd.c` -- a Linux personality emulator. Interesting far
  down the roadmap, though written against Mach 4.

The conclusion for anyone tempted to revisit this: xMach is a sibling
port, not a newer one. Take design ideas from it if useful, but its
kernel interface assumptions are the wrong ones for this tree.

## Licensing

Compatible, and cleaner than UX.

- **Core (UC Berkeley lineage):** 4-clause BSD text, but 4.4BSD-Lite
  derived -- the post-settlement clean branch, marked by the "with the
  permission of UNIX System Laboratories" note. UC retroactively
  withdrew the advertising clause in 1999, so for UC-copyrighted files
  it is effectively BSD-3-Clause today.
- **Helander's Mach glue:** a permissive HPND-style grant, same family
  as OSFMK 7.3's own notice, no advertising clause.

Neither is copyleft.

**On UX and the Caldera argument:** the claim that Caldera's 2002 grant
implicitly freed 4.3BSD is not safe to rely on. That grant names UNIX
V1-V7 and 32V rather than derivatives, 4.3BSD contains much more than
32V-derived material, the *USL v. BSDi* settlement is what actually
addressed 4.3BSD and produced 4.4BSD-Lite as the clean branch, and
Caldera's authority was contested afterwards in *SCO v. Novell*. LITES
avoids the question entirely.

## Tried: configure and liblites build against our tree

Not a thought experiment any more. The following was done and works.

### Constructing a MACH_RELEASE_DIR

LITES wants `$(MACH_RELEASE_DIR)/{include,include/mach,lib}` and
`mig`/`migcom`. Our ODE export tree provides all of it:

```sh
MR=/tmp/machrel
mkdir -p $MR/bin $MR/libexec
ln -sfn $MK_BUILD/export/at386/include $MR/include
ln -sfn $MK_BUILD/export/at386/lib     $MR/lib
HB=osfmk7.3/osfmk/tools/i386/i386_linux/hostbin
ln -sf $PWD/$HB/mig    $MR/bin/mig
ln -sf $PWD/$HB/migcom $MR/bin/migcom
ln -sf $PWD/$HB/migcom $MR/libexec/migcom
```

`export/at386/include/mach/` contains the `.defs` files, including
`bootstrap.defs`, so LITES generates its Mach stubs from **our**
definitions with **our** `mig`. That was the central claim of this
survey and it is now demonstrated rather than argued.

### Configure and build

```sh
sh /path/to/lites/configure \
    --with-release=$MR \
    --with-config="STD+WS+osfmach3" \
    --host=i386-unknown-mach3 --target=i386-unknown-mach3

GI=$(gcc -m32 -print-file-name=include)
make CXXX="-m32 -isystem $GI" CHXXX="-m32"
```

`--with-config="STD+WS+osfmach3"` is **essential and not the default**.
Without it `LITES_CONFIG` is `STD+WS`, `OSFMACH3` and `OSF_LEDGERS` stay
undefined, and every device call has the wrong arity:

```
block_io.c:141: error: incompatible type for argument 4 of 'device_open'
block_io.c:132: error: too few arguments to function 'device_open'
```

That is not an incompatibility. LITES already brackets the extra
arguments correctly:

```c
rc = device_open(device_server_port,
#if OSF_LEDGERS
                 MACH_PORT_NULL,     /* ledger */
#endif
                 mode,
#if OSFMACH3
                 security_id,        /* security token */
#endif
```

which matches our `device.defs` exactly -- OSFMK 7.3 replaced
`device_open` with a ledger-and-token form and left the old message id
as `skip; /* nmk15: device_open */`. There are 66 such call sites across
`device_open`, `device_read`, `device_write`, `device_get_status`,
`device_set_status` and `device_close`, and the single config option
fixes all of them.

`CXXX` and `CHXXX` are user hooks in `conf/Makerules` that append to
`TARGET_CFLAGS` and `HOST_CFLAGS`, so the toolchain flags go in without
patching LITES.

### Result

`liblites` **compiles**. The build reaches `server/` and then fails in a
generated file:

```
bsd_types_gen.symc:8:6: error: missing terminating " character
```

`gensym.awk` emits output a modern cpp rejects -- structurally the same
problem OSFMK's own `genassym` had, and the next thing to fix.

### Further: MIG interoperates, and the server tree starts building

With `tools/lites/gensym-newline.patch` applied and
`tools/lites/mig-shim.sh` in place of `$MACH_RELEASE_DIR/bin/mig`:

- `bsd_types_gen.symc` compiles and `bsd_types_gen.h` is generated
- **our `mig` runs LITES's `.defs` against our `mach_types.defs`** and
  produces `bsd_1_server.c` and `bsd_1_server.h`
- `-DOSF_LEDGERS=1 -DUNTYPED_IPC=1` appear on the compile lines, so the
  `osfmach3` arms are live

That is the interoperation this survey set out to test, working at the
tool level: LITES source, our MIG, our definitions, one output.

The build then stops on a LITES packaging inconsistency rather than
anything to do with OSFMK. `conf/files:303` lists
`serv/bsd_server.c`, while the MIG rule derives its output name from
`bsd_1.srv` and so produces `bsd_1_server.c`. The two disagree, and make
passes the unresolved bare name to gcc:

```
cc1: fatal error: bsd_server.c: No such file or directory
```

Untangling that is LITES build-system work and is where the next session
should start.

### Further still: 22 objects, and the first real API difference

Adding `tools/lites/lites-compat.h` via `-include` carried the build
through `device_reply_hdlr.c` and 14 more objects.

That header covers the one genuine API difference found so far.
OSFMK 7.3 uses untyped (NDR) IPC, where the MIG error reply is
`mig_reply_error_t` -- a `Head`, an `NDR_record_t` and a `RetCode`. LITES
uses the typed-IPC name `mig_reply_header_t` in 13 places, which had a
`mach_msg_type_t` where the NDR record now is. It touches the differing
member, `RetCodeType`, in only two places and both are inside its `#else`
arm for typed IPC, which `UNTYPED_IPC` compiles out -- so the two
structures are interchangeable for every use that remains and a plain
typedef suffices.

The build then reaches `server/net/` and stops on a LITES internal
inconsistency: `include/sys/malloc.h:272` defines
`bsd_malloc(size, type, flags)` as `malloc(size)`, because the LITES
server has a one-argument malloc rather than the BSD kernel's
three-argument one, but `net/radix.h`'s KERNEL arm was never converted
and still calls `malloc` and `free` with BSD arity directly.
`tools/lites/radix-bsd-malloc.patch` routes it through the wrapper.

### Further still: ~35 objects, and the shape is now clear

Continuing past `server/net/` turned up three more issues, all the same
kind, and each one unblocked a batch of files rather than a single file.

**BSD malloc arity, 53 sites in 46 files.** LITES's `sys/malloc.h`
supplies `MALLOC`, `FREE`, `bsd_malloc` and `bsd_free`, all resolving to
a one-argument allocator, but the BSD-derived trees under `server/net`,
`server/netccitt` and `server/isofs` were never converted and still call
`malloc(size, type, flags)` and `free(addr, type)` directly. Patching 53
sites would be a large change against LITES; two variadic macros in
`tools/lites/lites-compat.h` drop the extra arguments instead and the
existing calls compile unchanged.

One detail matters there. The macros must expand so that a later
*declaration* of `malloc` is still valid C:

```c
#define malloc(sz, ...)  (malloc)(sz)     /* right */
#define malloc(sz, ...)  (malloc)((unsigned long)(sz))   /* wrong */
```

With the cast, a header declaring `void *malloc(unsigned long);` expands
to `(malloc)((unsigned long)(unsigned long))` and fails. Without it the
declaration becomes `extern void *(malloc)(unsigned long);`, which is
legal. GCC reports such failures at the macro's *definition* site, which
is misleading -- the real error is at whichever header declares the
function.

**`-fno-builtin` is required.** BSD's kernel `log(level, fmt, ...)`
collides with GCC's builtin `log(double)`, giving "too many arguments to
function 'log'". OSFMK's own build uses `-fno-builtin` for the same
reason.

**Pointer constants as case labels.** `kern_sig.c` has `case SIG_DFL:`
where `SIG_DFL` is `(void(*)())0`. K&R C accepted it; modern C requires
an integer constant expression. This is the current stopping point and
needs a LITES patch rather than a shim.

The full flag set that gets this far:

```sh
GI=$(gcc -m32 -print-file-name=include)
make CXXX="-m32 -fno-builtin -isystem $GI \
          -include /path/to/tools/lites/lites-compat.h" \
     CHXXX="-m32"
```

### The whole LITES server now compiles

Every object builds and the link is reached:

```
startup.Lites.1.1.u3.STD+WS+osfmach3.unstripped
```

`tools/lites/build-lites.sh` reproduces it end to end.

Getting from ~35 objects to the link needed six more fixes, all the same
kind:

| issue | fix |
|---|---|
| `case SIG_DFL:` etc -- pointer constants as case labels, 11 in 2 files | cast the labels: `case (integer_t)SIG_DFL:` |
| `*((char *)to)++` -- a cast is not an lvalue, 1 site | spell the post-increment out |
| `cthread_mach_msg` declared differently by us and LITES | see below |
| `memory_object_behave_info.write_completions` gone | set `silent_overwrite` and `advisory_pageout` instead |
| `memory_object_attr_info.may_cache` / `.object_ready` | renamed to `may_cache_object`; `object_ready` has no counterpart |
| assembly built 64-bit | `ASFLAGS=-m32`, a separate hook from `CXXX` |
| generated `vers.c` had literal newlines in string literals | see below |

**`cthread_mach_msg`.** LITES supplies its own in `server/serv/cprocs.c`
with the nine-argument signature old cthreads had; its own comment says
"These are missing from cthreads". OSFMK 7.3's libcthreads has one too,
but collapsed into a single struct whose members map one to one onto
those nine arguments. Nothing calls ours, so they collide only as
declarations. `lites-compat.h` pulls `cthreads.h` in early with our name
renamed away; the include guard makes every later include a no-op, so
LITES's declaration and definition stand unopposed.

**`vers.c`.** `conf/newvers.sh` emits `\\n` expecting a backslash-n to
reach the C source, but `/bin/sh` on a modern Debian is dash, whose
`echo` interprets backslash escapes, so a real newline landed inside a
string literal. Changing those `echo` calls to `printf '%s\n'` fixes it.
This is a second instance of the same 1990s assumption as `gensym.awk`,
by a different mechanism -- there the C source was wrong, here the shell
was.

### 150 objects, and the duplicate symbols are gone

Two more flags clear every multiple-definition error:

| flag | why |
|---|---|
| `-fgnu89-inline` | `cthreads.h` declares `cthread_sp`, `spin_unlock` and `spin_try_lock` `extern __inline__`. Under C99 rules that emits a symbol in every translation unit; gnu89 semantics are what the header was written for. |
| `-fcommon` | `bufqueues`, `invalhash`, `bufhashtbl` and friends are tentative definitions in headers. GCC 10 and later default to `-fno-common`, so each object gets its own. |

**Rebuild from clean when changing these.** Stale objects compiled
without the flag keep their duplicate symbols and the link still fails,
which looks exactly like the flag not working.

Library naming is handled without touching LITES by making
`$MACH_RELEASE_DIR/lib` a real directory of symlinks and adding the two
aliases LITES asks for:

```sh
ln -sf libcthreads.a libthreads.a
ln -sf libsa_mach.a  libmach_sa.a
```

### The link runs; 1055 undefined symbols remain

The link now executes over all 150 objects. Getting there needed:

| issue | fix |
|---|---|
| `CRT0` unset, resolving to the literal `crt0-not-found` | `ar x libsa_mach.a crt0.o` into `$MACH_RELEASE_DIR/lib`; OSFMK keeps crt0 inside the archive rather than standalone |
| `ld: unrecognised emulation mode: 32` | the link rule calls `ld` directly, not `gcc`, so it is `LDFLAGS="-m elf_i386"` and not `-m32` |
| `liblites.a` built 64-bit | rebuild it from clean after adding `-m32`; the archive predated the flag |
| `-lmach` missing from `LIBS` | LITES's non-OSF arm omits it. Overriding `LIBS` on the make line took undefined symbols from 2089 to 1055 |
| `__stack_chk_fail_local` | `-fno-stack-protector` |

What is left divides in two.

**A 32-bit libgcc this host does not have.** `__divdi3` and `__moddi3`
are libgcc helpers, and `gcc -m32 -print-libgcc-file-name` returns the
x86_64 path because no multilib libgcc is installed. A machine with
`gcc-multilib` properly set up should resolve these.

**Mach RPCs our libraries do not export**, such as `clock_sleep` and
`host_get_clock_service`. These are generated stubs, so the question is
which `.defs` are compiled into which OSFMK library and whether the
export tree is missing one. That is OSFMK-side work and the first task
in this effort that is.

### Superseded: the link step

```
ld: cannot find crt0-not-found
ld: cannot find -lthreads
ld: cannot find -lmach_sa
```

Both are the link step rather than compilation. `CXXX` feeds
`TARGET_CFLAGS`, which the link rule does not use, so the link runs
64-bit and silently passes over our 32-bit archives -- the aliases exist
and `-L$MACH_RELEASE_DIR/lib` is on the command line, so "cannot find"
here means "found nothing of the right architecture". The link needs its
own `-m32`, and `CRT0` is unset, resolving to the literal
`crt0-not-found`.

### Superseded: two link errors

```
ld: cannot find -lthreads
ld: cannot find -lmach_sa
multiple definition of `cthread_sp'
```

The first two are naming: ours are `libcthreads.a` and `libsa_mach.a`.
The third is that `cthreads.h` declares `cthread_sp` and `spin_try_lock`
`extern __inline__`, which under modern GCC's C99 inline rules emits a
symbol in every translation unit that includes it; `-fgnu89-inline` or a
`static` qualifier is the usual remedy. Both are small and neither
touches OSFMK.

### Shims kept in this tree

Both are ours, so nothing in LITES or OSFMK is modified:

| file | purpose |
|---|---|
| `tools/lites/mig-shim.sh` | LITES invokes `mig -cc <cmd>`; OSF's `mig` spells it `-cpp`, and silently treats `-cc` as a cpp flag so the command nam