# Precedent, attribution, and what in this tree is ours

How donors are chosen for the server, what has been written here rather
than taken, and corrections to statements already committed. Facts, not
legal advice.

This project has three vendor imports, each with its own lineage, and
the search order differs by which of them a change touches. That is the
first thing to establish before looking anywhere.

## The three trees

	name        where it is          import    what
	osfmk7.3/   osfmk/               f8fee21   OSF Mach Kernel 7.3, from MkLinux DR3.  Verbatim.
	lites/      osfmk/src/lites/     e31a69d   Lites, the CMU/HUT 4.4BSD-Lite single server.  Verbatim.
	bsd/        not yet imported               the 4.4BSD-Lite2 personality, from nmartin0/4.4BSD-Lite2.

**Throughout `docs/`, `osfmk7.3/`, `lites/` and `bsd/` are the trees'
names, not paths.** A path written inside a tree — `lites/server/serv/`,
say — is relative to where that tree lives, given above. Lites lives
inside the kernel's sandbox because that is where OSF built its own
server; "Where the server lives, and why" below has the sources.

All three run. That is the premise of the project and the reason the
imports are verbatim: each has a one-command deviation record, given in
`docs/provenance/pristine.md`, and each must stay small enough to
read.

## Where the server lives, and why

The server is a peer of the microkernel **inside the same source tree**:
a component directory of `osfmk/src/`, beside `mach_kernel/`,
`bootstrap/`, `default_pager/` and `mach_services/`, built by the same
ODE `build` after `mach_services`, and installed in `/mach_servers/`
with `startup` pointing at it. For this project that directory is
`osfmk/src/lites/`. OSF configured its server with its own
`conf/<MACHINE>/`, the way `mach_kernel` is; Lites is configured by its
own `configure`, run by ODE — "How Lites meets ODE" below has why.

That is OSF's own arrangement for OSF/1 on MK, stated twice in OSF's
release notes, and it is CMU's arrangement one level up.

### OSF's release notes say so directly

*MK7.3 Release Notes*, chapter 2 (`Install-MK7.ps`, OSF RI archive,
`ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk7.3/`):

- "The distribution tape only includes the source code needed to build
  the MK7.3 microkernel and OSF/1 server, together with supporting
  MK-related software." (§2.1)
- One extracted tree, `mk7.3/{doc,export,obj,src,tools,logs,boot}`, and
  "The ./src tree contains all the source files needed to build the
  microkernel, server and related MK utilities." (§2.5.1–2.5.2)
- One driver for everything, run from `src/`, in this order: headers,
  `mach_kernel`, libraries, `bootstrap`, `default_pager`,
  `mach_services`, then step 8:

		$ cd <build_path>/mk7.3/src
		$ sh -f build.sh AT386 osf1 \
		    osf1_server build_all 2>&1 | tee server.log

- `SERVER_CONFIG` selects PRODUCTION, FAST or DEBUG, as
  `MACH_KERNEL_CONFIG` does for the kernel. "By default the server is
  built in two flavors: both collocated, as osf1_server.<config>.INK
  … and as osf1_server.<config> built to run in user space." (§2.5.2)
- Installed in `/mach_servers/` and linked as `startup.INK`; the sample
  `bootstrap.conf` line is `-k startup /mach_servers/osf1_server.INK
  sd0a`. (§2.5.3)

*OSF-RI MK6.1 Release Notes* (`os/osfmk/mk6.1/relnotes.ps`) say the
same for MK6, and name the server's own configuration file:
`<build_path>/mk6.1/src/osf1_server/conf/AT386/template.mk` — the same
`conf/<MACHINE>/template.mk` scheme as `mach_kernel/conf/`. They add
that "some files in this build are derived from a snapshot of OSF 1.3",
which is why the tree is not to be overlaid on an OSF/1 tree.

At boot, the same chapter's walkthrough shows the bootstrap task
prompting for `/dev/boot_device/mach_servers/bootstrap.conf`, then
`default_pager`, then `startup -s -a fd0b`.

### CMU, mapped at the right level

Mary R. Thompson, *Building Mach 3.0*, CMU, 18 June 1993
(`mach3_build.doc`, `github.com/Prajna/mach`):

	src/{buildtools,mk,ux,user,bsdss}/...
	obj/<context>/{buildtools,mk,ux,user,bsdss}/...
	release/<context>/{bin,lib,include,etc,man,special}/...
	export/<context>/{bin,include,lib,special}/...

"Both the mach kernel and Unix server use a modified version of the
BSD config program", all of it under OSF's ODE. CMU's `src/` and OSF's
`src/` are the same level:

| CMU `src/` | OSF MK `src/` | this project, `osfmk/src/` |
|---|---|---|
| `mk` | `mach_kernel`, `bootstrap`, `default_pager` | the same |
| `ux` | `osf1_server` | `lites` |
| `user` | `mach_services` | `mach_services` |

The name `startup` is CMU's (`mach3_setup.doc`: "vmunix and startup are
used for the Unix-server"), kept by OSF in `bootstrap.template` and the
bootstrap task's built-in `unix startup -s`. It names the runtime slot
in `/mach_servers`, never a source directory.

Lites came out of CMU's environment — `doc/building` shows Helander
building it from `/p/mach/mach3/work/src/lites`, a peer of `mk` and
`ux` in CMU's `src/`, against `--with-release=…/export/…`.

### Supporting, and the exception

OSF's papers describe what went in that directory: the OSF/1 system
services, "taken directly from the integrated kernel code base with
over 95% code reuse" (Wells, 1994); "over 90% … unchanged from the
OSF/1 IK, and the code that differs consists almost entirely of glue
code" (Condict et al., the MK6 collocation paper). *Design of the
OSF/1 Operating System, Release 1.2*, §1.3–1.4, gives the split the
directory embodies: the core kernel became the microkernel and the
system services "its own, separate address space".

MkLinux DR3 is the exception, not the rule: its Linux server sits
outside `osfmk/` as `mklinux/`, reaching the kernel through committed
links into `osfmk/export/`, because Linux was GPL and built with GNU
make — the case `set_ode_path.sh` exists for. Paragon OSF/1 AD's
`kernels/` and `servers/` split is Intel's product packaging around a
NORMA kernel, and its runtime does not transfer: it asks for its ports
with CMU's raw `msgh_id 999999` handshake, which OSFMK 7.3's
`bootstrap.defs` skips.

### Two corrections, in order

**`696c173` was half right.** It put Lites inside `osfmk/src` so that
OSF's tooling could build it with `build -here` — which is what OSF
did. It was wrong to bury it in `mach_services/servers/` among the
small daemons, and to name the source directory after the runtime slot,
`startup`.

**`bc3630b` was wrong, and so was the section it rested on.** It moved
Lites out to the top level, beside `osfmk/`, on the strength of CMU's
`src/{mk,ux}` and DR3's `osfmk/` + `mklinux/`. The CMU layout was
mapped one level too high — `osfmk/src/` corresponds to CMU's `src/`,
not to its `mk` collection — and DR3's sibling was generalised from an
exception. OSF's release notes, read after that commit landed, settle
it. `97b5ea4` moved Lites to `osfmk/src/lites/`.

## How Lites meets ODE

`build -here lites` builds Lites. `osfmk/src/lites/Makefile`, the one
file added for it, is an ODE Makefile that runs **Lites' own
`configure` and GNU make** in the object directory, against the
sandbox's compiler, MIG and export area. Lites' older ODE build was not
revived. Both were tested before either was written.

### What Lites carries

Two build descriptions, of different ages:

- **CMU ODE**, `Makeconf` at the top and `server/conf/` (`MASTER`,
  `files` in OSF's dialect, `template.mk` by Mary Thompson and Johannes
  Helander, 1992 and 1994, and the `Makefile.config` that composes a
  configuration). Only the server has one; `include/`, `liblites/`,
  `emulator/` and `bin/` have no ODE Makefile at all.
- **autoconf and GNU make**, `configure` from `conf/configure.in`, with
  `conf/MASTER`, `conf/files` (the `fileif` dialect) and a `Makefile.in`
  and `Makerules` per directory. Helander's `doc/building` documents
  it: "run configure; gmake; gmake install".

The second is the one that knows about this kernel. `osfmach3` —
`options osfmach3 OSFMACH3 1`, with `-DOSF_LEDGERS=1 -DUNTYPED_IPC=1`,
selecting the `bootstrap_ports()` handshake in `server_init.c` and
OSFMK code paths in about twenty server files — is in `conf/MASTER`
and nowhere in `server/conf/`. The configure build also has 22 sources
the ODE one lacks, among them ext2fs, msdosfs and minixfs. The ODE
configuration was abandoned before Lites ran on OSFMK.

And the configure build was used against an OSF sandbox by the people
who wrote it. `conf/Makerules`, under `###### OSF STUFF #####`:

	ifdef OSF_ENVIRONMENT
	LD = /afs/gr.osf.org/users/guest/sbox/nmk18b9/link/tools/at386/elf/ld -T ...
	CC = /afs/gr.osf.org/users/guest/sbox/nmk18b9/link/tools/at386/elf/gcc -B...

— an ODE sandbox at OSF Grenoble, `nmk18b9`, its `link/tools/` the
same shape as this tree's `osfmk/link/tools/`. That is the arrangement
this file reproduces: Lites' build, the sandbox's tools.

### What was measured

- **Lites' `Makeconf` makes it a separate ODE collection, and that
  fails here.** With it present, `build -here lites` stops at once:
  `No such directory: ../obj/at386/lites` — ODE's make takes
  `osfmk/src/lites/` as the top of a tree and resolves the object
  directory from there. With it absent, ODE treats Lites as a component
  like `mach_kernel`: objects under `obj/at386/lites`, `RULES_MK`,
  `EXPORTBASE` and `TARGET_MACHINE=AT386` all set. No other component
  under `osfmk/src` has a `Makeconf`. It is CMU's convention — "Each
  collection contains one Makeconf now, at top-level" — for a tree
  where `lites` would be a collection rather than a component.
- **OSFMK's `config` accepts `server/conf`** once its header is
  translated (`conftype "i386"` to `machine` and `cpu`, which is what
  OSFMK's `config` requires), and writes a 605-line ODE Makefile with
  150 objects. So reviving it is possible.
- **The configure build, against the export area, compiles 144 of the
  server's objects before stopping** by hand, and 145 through ODE,
  where MIG also runs. Of the first attempt's 146 errors, 108 were
  Lites' own two-argument `free()` and three-argument `malloc()`
  meeting a compiler with built-ins, and vanished with `-fno-builtin`,
  which every compile in this sandbox already carries (`osf.gcc.mk`
  line 90's `BASE_CC`, and `ANSI_CC` in `docs/tools/mksandbox.sh`).

### The options, and the choice

**A. Revive the ODE build.** Forward-port `osfmach3`, the filesystem
options and the 22 sources into `server/conf`; write ODE Makefiles for
`include/`, `liblites/`, `emulator/`, `bin/` and the top; express
`gensym.awk` and the MIG rules as ODE rules. This is the closer
likeness to OSF's own `src/osf1_server/conf/AT386/template.mk`. It is
also the most new code inside Lites, re-creating a build its authors
had left, and every later Lites change would have to be made twice or
the two would drift.

**B. One ODE Makefile that runs Lites' configure build.** OSF's step 8
keeps its shape — `build -here` inside the sandbox — and inside Lites
the build is the one its authors used on OSF's kernel. One file added,
one removed.

**C. A driver in `docs/tools/`, outside `build_world`.** Contrary to
OSF's release notes, which build the server with the same driver as
everything else. Listed for completeness.

**B was chosen**, by the maintainer on 11 October 2026: it keeps OSF's
build entry, changes least inside Lites, and is the shortest route to
a running system. **A is not refused.** It is the more faithful copy
of OSF's own server layout and may be revisited; `docs/deferred.md`
§2, "Lites' ODE build", holds it with what it would take.

### What B needed, and why each piece

The comment at the top of `osfmk/src/lites/Makefile` carries all of
this; in short:

- **`Makeconf` removed**, for the reason measured above. Its defaults
  were `LITES_CONFIG=STD+WS+DEBUG` and `VERSION=LITES`; `VERSION` is
  read only by `server/conf/template.mk`, and the configuration default
  moves into the Makefile as `STD+WS+osfmach3` — configure's own
  default plus this kernel's option.
- **The cross tools by their target names.** configure runs the target
  preprocessor itself, for the server configuration, before any
  Makefile exists, so tools cannot be supplied later on the make line.
  Lites expects them installed as `i386-mach-gcc` and so on
  (`doc/README.lites-1.1.u1`, "the i386-mach cross-build tools");
  `OSF_ENVIRONMENT` pointed at the sandbox's own. The Makefile writes
  one-line wrappers into the object directory, each an `exec` of what
  the sandbox uses — `${_CC_} ${CARGS}`, `${_LD_} ${LDOPTS}`,
  `${_AR_}`, `${_RANLIB_}`.
- **`<stdarg.h>` from `sa_mach`**, added after all of Lites' include
  directories with `-idirafter`. `osc/Buildconf` line 134 gives every
  Mach task `-I…/include -I…/include/sa_mach`; the exported
  `mach_init.h` includes `<stdarg.h>`. `dev`'s `0b9c704` reached the
  same answer independently ("the way upstream does it").
- **MIG on the gmake line.** Lites' Makerules builds
  `MIG := mig -cc "${CPP}"` for the CMU and Utah driver; OSF's driver
  spells that `-cpp` and passes unknown options to the preprocessor.
  `MIG="${_MIG_} -cpp i386-mach-gcc"` overrides the one line's result,
  and `MIGCOM` goes with it because Makerules reassigns it empty.
  `dev` translated the option in a wrapper, `mig-shim.sh`; a
  command-line variable does the same without a script.
- **`--prefix` inside the object directory.** Lites puts
  `${prefix}/include` on every compile; the default, `/usr/local`,
  put the host's headers on a `-nostdinc` build.
- **`INCDIRS`, `LIBDIRS` and `MAKEFLAGS` cleared** for gmake. Buildconf
  exports the first two as ready-made `-I`/`-L` strings, and Lites'
  Makerules uses the same names for bare lists it prefixes itself,
  producing `-I-I/…`.
- **A subdirectory per configuration**, `obj/at386/lites/<config>/`,
  as the kernel has `PRODUCTION/`. Run in the object directory itself,
  configure's generated `Makefile` is read by ODE's make in preference
  to the source one, and the second build fails.
- **Two execute bits restored**, on `conf/config.guess` and
  `conf/move-if-change`. The import carries all 761 Lites files as
  mode 644; these are the two the configure build runs by path rather
  than through `sh`, and without them configure cannot guess the host
  and writes no option headers. The other eight scripts with `#!` are
  run through `sh` and are left as imported.

## What OSF booted from

**UFS, on a disk labelled with OSF's vtoc.** The question arose because
the bootstrap task, as imported, also reads ext2 and minix, and the
first boot disk used ext2.

The evidence, all of it OSF's:

- *MK7.3 Release Notes*, §2.4.2.2: `disk_setup` partitions the disk
  "using the vtoc utility" and creates "new filesystems with newfs".
  §2.7.4, "Sample Disk Partition Table": a root, b paging, c entire
  disk, d usr, and a `vtoc` listing with "alt ptr 15360 alt len 2048".
  §2.5.3: the boot loader takes `disk(unit, partition)/kernel`, "partition
  is the partition containing the root file system (… default is a)",
  and loads `/mach_servers/bootstrap` from it.
- *Optimizing Mach-based Systems by Server Co-location* (Condict,
  Mitchell, Reynolds, OSF RI, August 1993), p. 17, describing boot
  before the bootstrap task existed: the default pager, then the
  bootstrap service, "uses a Mach version of the ufs code to lookup
  /dev/<rootname>/mach_servers/paging_file" and loads
  ".../mach_servers/startup". In the newer arrangement the same paper
  moves "filesystem dependencies" to "the initial boot and the
  bootstrap task".
- *OSF-RI MK6.1 Release Notes*: the OSF/1 server's file systems are
  "ufs, nfs, s5fs".
- In this tree, `file_systems/ufs/` is CMU's and OSF's code — its
  history is rpd's and af's, 1992 — and reads OSF/1's own UFS: a 16-bit
  `d_namlen` (`ufs/defs.h`) and `FS_SEC_MAGIC`, OSF/1's secure variant.
  The switch tables tell the rest. `HP700/fs_switch.c` and
  `AT386/fs_switch.c` put `ufs_ops` first; `POWERMAC/fs_switch.c`, the
  MkLinux machine, has no UFS at all, only minix, ext2, bext2 and HFS.
  ext2fs and minixfs are MkLinux's additions for Linux disks.
- `i386/AT386/bios_label.c` reads both a plain DOS partition table and,
  under `VTOC_COMPAT` — on in this configuration — "old vtoc layed out
  disks": an active DOS partition of type 99 with OSF's `evtoc` at
  sector 29 within it.

**Licences, which make this more than fidelity.** `minixfs/`'s
`minix_ffs_compat.c`, `minix_ffs_compat.h` and `minix_fs.h` are under
the GPL ("Copyright (C) 1994 Csizmazia Balazs … GNU General Public
License"), and `minix_ffs_compat.o` was in `libsa_fs.a` and so in the
bootstrap task; `dev` found the same at `19b0048`. `ext2fs/` carries
OSF's and CMU's notices and no GPL text, but `ext2fs/ext2_fs.h` says it
is "linux/include/linux/ext2_fs.h … from linux/include/linux/minix_fs.h,
Copyright (C) 1991, 1992 Linus Torvalds" — `dev`'s "entirely GPL-free"
is too strong. Neither is built for AT386 now; both stay in the tree as
imported.

**How the disk is made.** No tool writes OSF/1's UFS. `makefs` from
NetBSD writes 4.4BSD's, whose directory entries split `d_namlen` into
`d_type` and an 8-bit length; OSF's reader then finds no file. 4.4BSD
kept the old format as an option, and `docs/tools/ufs-oldfmt.py`
converts to it: directory entries, the 16-bit owner fields in each
inode, and the two superblock fields that mark the format. Measured:
the 4.4BSD image gives "cannot find configuration file"; converted, the
same image boots to "(default_pager): started". `docs/tools/mkbootdisk.sh`
writes the vtoc itself, from `struct evtoc` in `i386/AT386/disk.h`.

**Still not OSF's:** the boot loader. OSF's is in the tree,
`stand/AT386/boot/` — a primary boot in block 0 and a secondary that
reads the file system and loads `/mach_kernel` and
`/mach_servers/bootstrap` — and this project boots with QEMU's
multiboot `-kernel` instead. `docs/deferred.md` §2 holds it.

## Where to look, in order

1. **This tree itself** — any of the three subdirectories, and
   **related structures within them**. A spelling one of our own trees
   already uses beats one borrowed from outside, and the three are
   closer kin than they look: Lites and OSFMK descend from the same CMU
   Mach 3.0, and Lites' personality and ours descend from the same
   4.4BSD-Lite.

   "Related structures" is meant broadly. OSFMK's `mach_services/`
   holds a worked server (`netname.c`) and the libraries a server links
   against. Its `bootstrap/` already names `startup` and `emulator`.
   Lites' `server/serv/` shares 31 filenames with OSF's own `uxkern/`.
   Look in all of it before looking out.

2. **OSFMK 6.1** — `github.com/traviolia/osfmk6.1`. The kernel is
   earlier than ours and the repository carries OSF's own
   documentation: Server Writer's Guide, Server Library Interfaces,
   Kernel Principles, the 21-part Kernel Interfaces, the collocation
   paper and the MK6.1 release notes, 1,254 pages in `doc/`. Those
   documents carry an explicit grant covering derivative works, so
   implementing from them is not a workaround — it is what they were
   published for.

3. **Rhapsody** — `github.com/calmsacibis995/xnu-rhapsody-53`, the
   Darwin 0.1 kernel. Mach 2.5-era with 4.3BSD/NeXT personality
   collocated in the kernel. Closer in era to Lites than XNU is.

4. **XNU, oldest first.** `github.com/apple-oss-distributions/xnu`,
   `rel/xnu-124` through `main`. Start at the earliest tag that could
   have the thing and move forward only as needed. `osfmk/` there is
   descended from this very kernel, and its revision history records
   the OSF streams by name (`mk6`, `nmk15`, `cnmk_shared`,
   `is_shared`, `colo_shared`) — so an XNU file's HISTORY block is
   often a direct statement about OSFMK's own development.

5. **Our own code, last**, and only where the four above have nothing.

## APSL code may be used, and that is the difference from the Lite2 tree

`4.4BSD-Lite2`'s `docs/provenance/precedent.md` makes XNU and Rhapsody
read-only, because that tree must stay BSD-licensed and APSL is not
compatible with it. **That constraint does not apply here.**

This project may contain APSL code. XNU and Rhapsody are therefore
sources, not merely witnesses, and sit in the search order above as
ordinary donors.

Three things follow, and they are not relaxations:

**The order still holds.** APSL being permitted does not make XNU the
first place to look. It is fourth, behind our own trees and OSFMK 6.1,
for the same reason every other donor is ranked: the closer the kin,
the better the fit, and a line that already exists in our idiom needs
no adaptation.

**Mixed files are the normal case and must stay legible.** An XNU file
typically stacks several notices — Apple under the APSL, then OSF, CMU,
Berkeley, and sometimes USL. Taking from such a file carries every one
of them. The notice block comes with the code, unaltered, and the SPDX
expression names all of them with `AND`.

**GPL is still excluded.** GNU Mach, Linux and MkLinux's `mklinux/`
personality are read-only, and nothing here may be derived from
reading them. MkLinux's `osfmach3/` is the sharpest case: the files
carry a bare `Copyright (c) 1991-1998 Open Software Foundation, Inc.`
with an empty comment block and no grant, and they sit inside
`mklinux/src/`, which `COPYING` covers. Read it to understand what OSF
did. Copy nothing.

The same applies to the two encumbered trees that answer design
questions better than anything else available: the DEC OSF/1 tree
(`calmsacibis995/osf1-10-src`, "proprietary to and embodies the
confidential technology of Digital Equipment Corporation", 2,051 of
2,052 files) and the Paragon OSF/1 AD server on cd155
(`nmartin0/undoc_osfmk`, Intel confidential, requiring three source
licences — Intel, OSF and USL). Structural facts from them are
findings and may be recorded. Their code is not a donor.

## The tree stays pristine, and config beats code

`docs/provenance/pristine.md` has the rule and the four-level order of
preference: vendor configuration, then a build-level change, then a
symlink out of the tree, then a source change with an `AI-ONLY NOTE`.

It belongs beside this file because the two together decide every
change: this one says *whose line*, that one says *at which level*.

## The canonical style is OSFMK 7.3's

`4.4BSD-Lite2` preserves Berkeley's idiom. This tree preserves OSF's.
Where the two meet — and they meet in every file of the server — the
rule is:

- Code under `osfmk7.3/` follows OSFMK's conventions, full stop.
- Code in the server that talks to Mach follows OSFMK's conventions:
  `kern_return_t`, `KERN_SUCCESS`, OSF's brace and comment style, the
  `@OSF_COPYRIGHT@`-era file header shape.
- Code in the server that is the personality follows 4.4BSD's, because
  that is what it is and what it must stay mergeable with.

The seam is `uxkern/`. Lites and OSF both put the Mach-facing glue
there, under those names, and that is where the conventions change.

## Every claim names a tree and a release, and is read before it is made

Taken unchanged from the Lite2 tree's rule, because it is right and
because this project has more trees to confuse, not fewer.

1. A citation names the tree **and** the release, tag or commit. Not
   "XNU does it this way" but "`rel/xnu-124`, `osfmk/kern/syscall_sw.c`".
   Not "OSFMK has it" but which of 6.1 and 7.3.

2. Nothing outside the trees on disk is precedent. If an idea comes
   from a paper, from a model's training, or from its own reasoning, it
   is this project's own invention: it is labelled as such, the
   alternatives are given, and the maintainer decides.

   **The papers are the exception that proves this, and they are
   narrow.** Patience's *Redirecting System Calls in Mach 3.0* and
   Lepreau et al.'s *In-Kernel Servers on Mach 3.0* describe mechanisms
   this project implements. They are specifications and may be
   implemented from. They are not code and citing one is not citing a
   donor: "Patience §4 says the state travels in the message" settles
   *what to build*, and the question of *whose line to write it as* is
   still open and still answered by the order above.

3. A claim is written only after the file it describes has been read in
   the session that writes it — not from memory of what Mach "usually
   does".

## Proving an absence

A found donor proves itself: the line is shown. An absence does not.

No proposal may state that no donor exists unless every tree above has
been searched, and the commit message names them. For this project that
is: `osfmk7.3/`, `lites/`, `bsd/`, OSFMK 6.1, Rhapsody, and XNU from
`rel/xnu-124` forward.

Six trees. The sweep is cheap and the claim is worthless without it.

## What in this tree is ours

Nothing yet. This section fills as work lands, in the Lite2 tree's
form: a table of every file and change that is ours, with its size and
what it is, so that the answer to "what did this project write" is a
lookup rather than a diff.

The expected entries, from `docs/roadmap.md`:

| file or change | expected kind |
|---|---|
| `syscall.c` in the server's glue, `lites/server/serv/` today, after OSF's `uxkern/syscall.c` | ours, written from Patience §3–4 against OSFMK's exception interface |
| `THREAD_STATE_SYSCALL` in `osfmk7.3/` | ours; a new i386 thread-state flavour, no donor has it |
| building the server | a `build -here lites` stanza after `mach_services`, OSF's step 8, plus whatever bridges Lites' own configuration to ODE. `build_world` is a vendor file whose header invites exactly this kind of edit |

Our own files carry an SPDX-FileCopyrightText line. Whether they carry
a licence identifier, and which, is the maintainer's decision and is
not asserted here.
