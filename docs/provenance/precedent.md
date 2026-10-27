# Precedent, attribution, and what in this tree is ours

How donors are chosen for the server, what has been written here rather
than taken, and corrections to statements already committed. Facts, not
legal advice.

This project has three vendor imports, each with its own lineage, and
the search order differs by which of them a change touches. That is the
first thing to establish before looking anywhere.

## The three trees

	name        where it is          import    what
	osfmk7.3/   osfmk/               f8fee21   OSF Mach Kernel 7.3, from MkLinux DR3, as slp/osfmk-mklinux had it in 2012.
	lites/      osfmk/src/lites/     9d1c98a   Lites 1.1.u3, the CMU/HUT 4.4BSD-Lite single server, as Utah released it on 30 March 1996.
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

**Neither import is its upstream's release, corrected 12 October 2026.**
Each is verbatim from the copy it was taken from, and each of those
copies had been changed by an intermediate project. "Where the two
imports came from", next, has the evidence; `docs/roadmap.md`,
"Intermediate goal", milestone 0, restores the releases.

## Where the two imports came from

**`osfmk7.3/` is `slp/osfmk-mklinux` at its last commit, `d4d387a`
(3 November 2012)**, Sergio López's "OSF Mach and MkLinux conservation
project" on GitHub. Our `osfmk/` at `4b29812`, `f8fee21` and `918a874`
has git tree `10ca4d13231745f33d052c271071142f7b0687b1`, the same tree
as `d4d387a:osfmk`; nmartin0/osfmk7.3 is a one-commit copy of it with
the Linux server, `mklinux/`, left out. Verdict 1 called slp's
repository "an independent repository" that confirmed the import. It
is the import's source, so it confirms nothing about DR3.

*Settled 20 October 2026:* Apple's own `DR3.osfmk.src.tgz`, from the
MkLinux DR3 CD, is in hand (`docs/provenance/sources.md`), and
`0f6f7e6` with `start.s~` as `start.s` is it, byte for byte, but for
empty directories and the separately shipped `export`, `tools` and
`link`. The CD's tarballs are the kernel's reference release.

slp's repository has nineteen commits. The first, `0f6f7e6`
(4 January 2011, "Initial commit"), was the nearest to MkLinux DR3
that could be read: no "multiboot", "GRUB" or "boot_script" anywhere in its
`osfmk/`. It is not untouched either. Its `i386/start.s` puts the
`#include <i386/AT386/asm_startup.h>` block -- the code that takes
`mach_boot`'s arguments off the stack and moves the bootstrap image
-- inside `#if 0`, and the editor's backup beside it, `start.s~`,
differs from it in exactly those two lines. So the original survives in
the commit itself. The other eighteen commits, in order:

	18c9d31  .s renamed .S; start.S gains a multiboot header and entry
	81429d0, 0d8ebab  PCI disabled for i386
	2913827  "register" -> "register int" in eight drivers
	f4fa636  minor fixes: rtclock.c, at386_scsi.c, template.mk
	4df2e7d  config and migcom sources and prebuilt binaries
	364472d  assym.s -> assym.S
	444954a  multiboot (GRUB): model_dep.c, and do_bootstrap_compat()'s
	         loader_info code replaced by an ELF PT_LOAD parser
	7e81018  hd.c: "Workaround to be able to use ATA devices under QEMU"
	9335f94  more kernel virtual memory (mach/i386/vm_param.h)
	090a2dc  assembler comments in libsa_mach
	af5babc  i386/multiboot.h (Utah) and mach/elf.h
	ebdad63  ext2_dir_entry updated
	8105dd7  parse_arguments() changed
	63279d3  the GNU Hurd graft: kern/boot_script.[ch], exec_load(),
	         exec_map(), set_user_regs(), build_args_and_stack(), the
	         new bootstrap_create(); OSF's renamed *_old behind #if 0
	55c7c1c  the Linux server's Makefiles (mklinux/, not in our import)
	44e8969  usimple lock check commented out in kern/lock.c
	d4d387a  Page Global Extensions for i386

`stand/`, `i386/AT386/asm_startup.h`, `conf.c`, `autoconf.c` and
`mach/boot_info.h` are as `0f6f7e6` has them.

**Restored 13 October 2026: `lites/` is Utah's release.** The commit
"Restore Lites 1.1.u3 as Utah released it" replaced the import below
with `lites-1.1.u3.tar.gz` unpacked in place, 892 files and their
modes; `9d1c98a` above is that tree's object id, the same in every
clone, where a commit id would not be. What it replaced, for the
record:

**`lites/` was xMach's SourceForge CVS module (`/cvsroot/xmach/lites`)
as of late 2000**, not Utah's release. 78 files carry
`$Header: /cvsroot/xmach/lites/...`; the CVS logs show the import by
`xmach` on 6 May 2000 and changes by `welchd` in October and November
2000. Utah's last release, `lites-1.1.u3.tar.gz` (30 March 1996), has
892 files; ours has 761. 445 are byte-identical, 308 differ, and of
those 264 differ only in CVS keyword and log lines. xMach removed 139
files -- the alpha, mips, ns532 and parisc ports, Lites' old CMU
Makefiles, `conf/configure`, Utah's `makefile` -- and changed 44:
dynamically linked Linux ELF binaries, a devfs, `ostype` "xMach", Linux
2.4 bitops in ext2fs, `configure` regenerated with autoconf 2.13, and
generated `conf/Makeconf` and `conf/Makevar` committed by mistake.
None of the 28 files with `OSFMACH3` code differs in that code.

The tarball used for comparison is `github.com/ryanwoodsmall/lites`'s,
SHA-256 `f81eab46e47786a6db7217aab6d3b29534ff24a8314a889e8e249568e7042c93`.
It is 1,763,728 bytes, the size `ftp.lip6.fr/pub/mach/mach4/mach/lites/`
lists for it, 29 March 1996, and xMach's import of 2000 agrees with its
content file for file. Its seven scripts are mode 0775, and with the
release restored our mode change on `conf/config.guess` and
`conf/move-if-change` is gone.

## What OSF's running system was

What the intermediate goal in `docs/roadmap.md` reconstructs. OSF's own
words first; where OSF said nothing, Lites' authors'.

**The parts, and where they were installed.** MK7.3 Release Notes
§2.5.2 lists them: primary bootstrap `/stand/boot`, secondary
bootstrap `/mach_boot`, bootstrap task `/mach_servers/bootstrap`,
microkernel `/mach_kernel`, default pager
`/mach_servers/default_pager`, name server `/mach_servers/name_server`,
"MK Server" `/mach_servers/startup`. The boot loaders' own code
disagrees with the notes on one path: `stand/AT386/boot/primary/boot.c`
loads `/stand/mach_boot`, and `primary/README` says "The default is
/stand/mach_boot".

**How it started.** The primary boot, in the first 8 KB of the disk,
loads `mach_boot`; `mach_boot` takes `boot: [<kernel>] [flags] [--]
[<bootstrap>] [flags]`, with `hd(unit,partition)` device names, and
loads `/mach_kernel` and then `/mach_servers/bootstrap`
(`stand/AT386/boot/secondary/README`; Release Notes §2.5.3). The
bootstrap task reads `/dev/boot_device/mach_servers/bootstrap.conf`.
OSF shipped `bootstrap/bootstrap.template` as that file:

	name_server name_server
	default_pager default_pager
	startup startup

with the comment that the default pager takes "a paging device name"
and the server -- "osf1_server (conventionally called "startup")" --
takes "-s (single-user), -a (prompt for root device), and a root file
system name (ex: hd0a)". `bootstrap.c`'s built-in fallback is the same
three with `unix startup -s`. §2.5.3's example for an in-kernel server:

	name_server /mach_servers/name_server
	def_pager /mach_servers/default_pager sd0b
	-k startup /mach_servers/osf1_server.INK sd0a

**Paging is a partition.** `default_pager` takes `-v`, `cl=N` and Mach
device names (`default_pager.c`); §2.7.3 gives the partition on its
`bootstrap.conf` line or as a `sw` entry in `/etc/fstab`, and §2.7.4
makes b the paging partition. MK6.1's paging file is gone.

**No mach_init.** *Optimizing Mach-based Systems by Server
Co-location*: "The OSF/1 server runs /sbin/init. There is no need for
mach_init as the service server is already running in the bootstrap
task and the netname service is already running as well." Lites'
`doc/README` says the same from its side: "mach_init may be replaced
by a symlink to /sbin/init if a Mach name server is not used" -- and
here `name_server` is that server.

**What OSF did not say: how to run Lites on it.** Lites has had OSF MK
code since 1.1 (March 1995; `OSFMACH3` in 28 files, and `conf/Makerules`
pointing at an OSF Grenoble sandbox, `nmk18b9`), but no document in any
Lites release says how to boot it on OSF's kernel. With no arguments,
its `server_init.c` uses `-s hd0a /dev/hd0a/mach_servers`. The
userland its documents name is NetBSD 1.0's or FreeBSD 2.0's:
`doc/README` ("NetBSD, FreeBSD, UX, Linux, and 386BSD binaries are
supported"),
`doc/README.lites-1.1.u1` (a NetBSD 1.0 `/etc/ttys`) and
`doc/install.freebsd`. Helander's thesis (1994) reports Lites
"self-hosting on the i386", compiling with the host BSD's own gcc.

**Network.** §2.2.7 supports SMC WD8003/8013 and DEC DE435/DE450
boards, configured with `net_start <address> wd0|tu0 <netmask>`
(§2.4.2.3). Neither board is emulated by QEMU, and `tu`'s driver is not
in this tree (only `if_tu_entries.h`). The PRODUCTION configuration
also builds an NE2000 driver, `ne` (`conf/AT386/config.devices`, under
"MITCHEM"), which matches QEMU's `ne2k_isa` -- with two defects: its
`autoconf.c` entry installs `at3c501intr` as its interrupt handler, and
`if_ne.c`'s odd-length copies index an `int *` by bytes. Lites runs
4.4BSD's TCP/IP stack in the server and opens an interface by its Mach
device name (`server/serv/ether_io.c`, `iface_find()`).

**How OSF built it.** Natively, on "a DEC PC/AT class machine running
MK7.2 or later" (§2.5.2), with binaries of "the FSF gcc and the ODE
minimal tools set" on the tape's sixth volume; the host supplies `cc`,
`lex`, `yacc`, `libc.a`, `libl.a` and `libln.a`. §1.2.3: gcc 2.7.1 "is
the version of 'gcc' that has been used with MK7.2 and MK7.3".
`osc/Buildconf` says `GCC_LATEST 2_7_1`, and `2_7_2_1` on a Linux host.
FSF GCC's `config/i386/osfelf.h`, "Intel 386 (OSF/1 with ELF)",
predefines `__NO_UNDERSCORES__`, which `Buildconf` adds by hand for
Linux. GCC 3.1 declared the OSF/1 i386 targets obsolete; no later GCC
has them.

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
make — the case `set_ode_path.sh` exists for.

### Two corrections, in order

**`d67b353` was half right.** It put Lites inside `osfmk/src` so that
OSF's tooling could build it with `build -here` — which is what OSF
did. It was wrong to bury it in `mach_services/servers/` among the
small daemons, and to name the source directory after the runtime slot,
`startup`.

**`5a36519` was wrong, and so was the section it rested on.** It moved
Lites out to the top level, beside `osfmk/`, on the strength of CMU's
`src/{mk,ux}` and DR3's `osfmk/` + `mklinux/`. The CMU layout was
mapped one level too high — `osfmk/src/` corresponds to CMU's `src/`,
not to its `mk` collection — and DR3's sibling was generalised from an
exception. OSF's release notes, read after that commit landed, settle
it. `92a2cd2` moved Lites to `osfmk/src/lites/`.

## How Lites meets ODE

`build -here lites` builds Lites. `osfmk/src/lites/makefile`, in place
of Utah's driver of that name, is an ODE makefile that runs **Lites' own
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
  *Corrected 13 October 2026:* that was xMach's copy, which had removed
  them. The release has CMU's ODE Makefiles at the top and in
  `include/`, `include/i386/`, `include/sys/`, `liblites/`,
  `emulator/`, `emulator/libsyscall/`, `server/` and `server/conf/`,
  1992-95; only `bin/` has none. The server's configuration is still
  the part that predates `osfmach3`.
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
one removed -- since the release was restored, one replaced (Utah's
`makefile`) and one removed.

**C. A driver in `docs/tools/`, outside `build_world`.** Contrary to
OSF's release notes, which build the server with the same driver as
everything else. Listed for completeness.

**B was chosen**, by the maintainer on 11 October 2026: it keeps OSF's
build entry, changes least inside Lites, and is the shortest route to
a running system. **A is not refused.** It is the more faithful copy
of OSF's own server layout and may be revisited; `docs/deferred.md`
§2, "Lites' ODE build", holds it with what it would take.

### What B needed, and why each piece

The comment at the top of `osfmk/src/lites/makefile` carries all of
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
  as the kernel has `PRODUCTION/`. While the driver was named
  `Makefile`, configure's generated `Makefile` in the object directory
  itself was read by ODE's make in its place, and the second build
  failed.
- **No execute bits to restore.** xMach's import carried every file as
  mode 644, and `conf/config.guess` and `conf/move-if-change`, which
  configure runs by path, were set to 755 here. The release has them,
  and five more, as 0775.
- **Utah's `makefile` replaced, CMU's `Makefile` kept.** Lites 1.1.u3
  has both at the top: CMU's ODE `Makefile` and Utah's BSD-make
  `makefile`, a driver that runs `conf/configure` and gmake in an
  object directory with Utah's paths. ODE's make reads `makefile`
  before `Makefile` (`ode/bin/make/main.c`), so this site's driver
  takes the name and the role of Utah's, and every CMU Makefile stays
  as shipped.
- **`OSF_ENVIRONMENT`, brought to OSFMK 7.3** (13 October 2026,
  chosen by the maintainer over keying it on `osfmach3` or faking a CMU
  release). Lites has two switches for OSF's kernel: the `osfmach3`
  configuration option, for the kernel's interfaces in C, and
  `OSF_ENVIRONMENT`, for what an OSF sandbox provides -- its tools in
  `conf/Makerules`, its libraries in `server/` and `emulator/Makerules`.
  The link is the second's. Its libraries become 7.3's, and the server
  takes its start code from `libsa_mach` at `__start_mach`, as
  `osf.gcc.mk` links every standalone Mach program. The block's Grenoble
  tool paths stay as written; the makefile gives this sandbox's on the
  gmake line, as it gives MIG.
- **Kernighan's awk.** configure prefers mawk, and mawk refuses
  `server/kern/vnode_if.sh`'s awk programs whole, because they call a
  `bail()` they never define; the awk 4.4BSD shipped, Kernighan's,
  reports that only if the call is reached. With mawk the server links
  with every `vop_*_desc` undefined. configure takes `AWK` from the
  environment, and the makefile sets it, `LITES_AWK`, default
  `original-awk`.

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

**How the disk is made.** OSF made its disks on a running system.
MK7.3 Release Notes §2.4.2.2: `disk_setup`, run "on the target
machine", partitions with the `vtoc` utility and creates "new
filesystems with newfs"; `mk_install` then extracts the release onto
them. The boot floppies were made with `mkbootfloppies` on "MK7.X"
(§2.4), and `stand/AT386/boot/secondary/README` gives the same steps by
hand: `newfs fd0`, `mount /dev/fd0 /mnt`, `cp mach_boot /mnt`. Lites'
`doc/install.freebsd` does the same on FreeBSD 2.0. None of those
commands is in this tree, and the MK7.3 source distribution itself
left out "those commands and libraries that are unchanged from the
OSF/1 1.3 release" (§2.5.1).

No BSD tool from before 4.4BSD fills a file system without mounting it.
Checked in the Unix history repository's tags:

- `newfs` in Net/2 and in 386BSD 0.0 and 0.1 is one file (`mkfs.c`
  6.18, 7/3/91, UCB licence). It writes the 4.3BSD-Reno format, whose
  `struct fs` OSF's matches offset for offset (OSF/1 makes Reno's
  `fs_state` a 56th `fs_sparecon`), but makes only an empty file
  system on a device. 4.4BSD-Lite2's `newfs -O` writes the old
  directories and inodes in 4.4BSD's superblock, which differs from
  Reno's at its end: Reno's 55 spares hold `fs_contigsumsize`,
  `fs_maxsymlinklen`, `fs_inodefmt` (−1) and `fs_maxfilesize`, and
  `fs_state` moves from before `fs_qbmask` to after `fs_qfmask`, so
  the two masks sit 4 bytes earlier than in Reno. 4.4BSD's kernel and
  `fsck` recompute both masks for an old-format file system, marked
  "XXX" (`ffs_vfsops.c`, `fsck/setup.c`).
- `mkproto`, "appeared in 4.2BSD", filled a new file system from a
  prototype file. Its source in 4.4BSD says "This module is believed to
  contain source code proprietary to AT&T"; it is in 4.3BSD-Reno and
  4.4BSD and absent from Net/2, 386BSD and 4.4BSD-Lite1 and Lite2,
  whose `usr.sbin/Makefile` still lists it.
- `fsdb` (4.3BSD-Reno, 4.4BSD) is an interactive debugger. FreeBSD 2.0
  has `newfs` alone. `makefs` "appeared in NetBSD 1.6" (its manual,
  HISTORY), in 2002.

`makefs` writes the old format since NetBSD 11.0: "allow `version=0`
4.3BSD FFS file systems to be created for compatible boot ROMs" (the
11.0 announcement, 30 July 2026; commit `6952d6617dd`, tsutsui,
10 January 2026, "for systems with ancient boot ROM requirements like
LUNA"). FreeBSD's `makefs` takes version 1 or 2 only; Debian's and
Ubuntu's `makefs` package is a 2019 snapshot. So
`docs/tools/mknbmakefs.sh` builds NetBSD 11.0's, as NetBSD builds its
own host tools on another system, and `docs/tools/mkbootdisk.sh` runs
it with `-o version=0`. What it writes, read back: directory entries
with a 16-bit `d_namlen` and no `d_type`; `fs_maxsymlinklen` 0;
`fs_inodefmt` −1. Its superblock is 4.4BSD's, as Lite2's `newfs -O`
writes it: OSF's `struct fs` would find `fs_qbmask` and `fs_qfmask` 4
bytes off. OSF's reader uses neither — `blkoff()` and `fragroundup()`
use `fs_bmask` and `fs_fmask` — and Lites recomputes them. It does not
set the 16-bit owner fields at inode offsets 4 and 6, so every file
reads as owned by root; OSF's reader ignores ownership. Measured: a
4.4BSD-format image gives "cannot find configuration file"; the
`version=0` image boots to "(default_pager): started".

Until this commit a converter, `docs/tools/ufs-oldfmt.py`, rewrote a
4.4BSD-format `makefs` image instead. Compared on the same files, the
two images differ in 112 bytes: inode numbering order, the random
`fs_id`, and two things the converter left from NetBSD's later UFS1
(`FS_FLAGS_UPDATED` and the 64-bit size copies in the superblock),
which a 4.3BSD file system does not have and `version=0` leaves zero.

Making disks as OSF did — `newfs`, mount and copy, on the running
system — is possible since 13 October 2026 and not yet the procedure:
`docs/deferred.md` §2.

**Not OSF's until 0079:** the boot loader. OSF's is in the tree,
`stand/AT386/boot/` — a primary boot in block 0 and a secondary that
reads the file system and loads `/mach_kernel` and
`/mach_servers/bootstrap` — and this project booted with QEMU's
multiboot `-kernel` instead. Since 0079 OSF's boot loader, built on
the running system, boots it, and multiboot is the second way in
(`docs/deferred.md`, "Booting with OSF's own boot loader").

## What runs first: /sbin/init, not mach_init

Under CMU's bootstrap the server's first program was USER's
`mach_init`, the "service" server: it forked, pid 1 became
`/sbin/init`, and the child stayed as pid 2. Lites was written for
that, and its `wait4()` (`server/kern/kern_exit.c`, "major hack for
BSD init compatibility") hides pid 2 from pid 1, in every release
from 1.0 to 1.1.u3 and in xMach.

OSFMK's bootstrap does that job itself. The OSF Research Institute's
"Optimizing Mach-based Systems by Server Co-location" (Condict,
Mitchell, Reynolds, August 1993; uploaded as
`server_colocation_design.ps`), section 5.1, sets the two side by
side: in the older procedure "The OSF/1 server runs the mach_init
program, which implements the 'service' service, and it will fork,
running /sbin/init in the parent"; in the bootstrap-task procedure,
which is MK7.3's, "The OSF/1 server runs /sbin/init. There is no need
for mach_init as the service server is already running in the
bootstrap task and the netname service is already running as well."
The service server is in this tree as `bootstrap/service.c`
("Program: Service server", CMU's), and OSF's `bootstrap.template`
starts `name_server` first.

Lites' own notes offer a `mach_init` that is init itself, as a link:
"ln -s /sbin/init /mach_servers/mach_init".

With the link and the hack together, init cannot wait for its first
child. That is not this port's doing: Utah's own 1.1.u3 server
(`startup.Lites.1.1.u3.LARGE+mach4`, built at Utah in March 1996,
with the hack -- `cmpl $0x1` and `cmpl $0x2` on `p_pid` in its
`wait4`) on Mach4 UK22, from the MachUK22 disk image, boots NetBSD's
init to "wait for /bin/sh on /etc/rc failed: No child processes;
going to single user mode" and no login prompt, as ours did. So the
hack is left out under `OSFMACH3`, Lites' switch for OSFMK, and kept
for the CMU environment it was written for. Lites' own comment names
the cleaner fix -- "mach_init does an init_process system call. It
should cause this effect" -- which needs no change here, as there is
no mach_init.

## How Lites gets its arguments from OSF's bootstrap

Lites' `parse_arguments()` (`server/serv/server_init.c`) reads the
CMU Mach 3.0 protocol: flags, root name, server directory -- "Arg 2
should be root name, Arg 3 should be server_dir_name (3.0 style)".
CMU's and Mach4's bootstrap built that vector itself: xMach's
`kernel/generic/bootstrap/bootstrap.c` loads the server with "startup
-boot_flags root_name server_dir_name", the last being
`/dev/<root>/mach_servers`.

OSF's bootstrap task does not. Its `argv` is the server's path in
the bootstrap's own name space, then whatever `bootstrap.conf` gives:
`parse_path()` in `bootstrap/bootstrap.c` makes a relative name
`/dev/boot_device/mach_servers/<name>`, and `do_bootstrap_arguments()`
hands that buffer to the server, where libsa_mach's crt0 splits it
into `argv` (`__get_arguments`, `__setup_ptrs`). So `argc` is at
least 1 and `argv[0]` is `/dev/boot_device/mach_servers/startup` --
seen on the screen, where Lites, given no directory, derives
`/dev/hd0a/dev/boot_device/mach_servers` from it. The template says
what the rest is for: 'The "server-arguments" are any arguments to
be passed to the server'; its osf1_server line (-s, -a, a root name)
describes OSF's server, not every server.

The `OSFMACH3` code in Lites, added between 1.0 and 1.1-950808 and
unchanged to 1.1.u3, shows what its authors passed on OSF's
microkernel. Its `get_config_info()` hard-codes, as a fallback, the
vector

	"/dev/hd0f/mach_servers/startup", "-s", "hd0a", "/dev/hd0a/mach_servers"

-- an OSF-style path for `argv[0]`, then flags, root and server
directory. That is the shape of this project's line,

	startup startup -s hd0a /dev/hd0a/mach_servers

and the server directory is not this project's invention: it is
Lites' own argument, carried by OSF's own mechanism. No code change
is needed for it.

That fallback runs only when `argc` is 0. Under OSF's bootstrap task
it never is. It is reachable when the kernel starts the server itself,
as its first task: the kernel serves `bootstrap_arguments` too
(`kern/bootstrap.c`, `do_bootstrap_arguments()`, "CR9917", 1993), with
the boot loader's options for the first task -- DR3's
`i386/AT386/asm_startup.h` stores them as `boot_args_start` and
`boot_args_size`, 0 when nothing follows the task's name on the boot
line. That is not MK7.3's documented boot -- the
Release Notes, section 2.5.3, start servers from `bootstrap.conf` --
so the branch, which fetches `host_get_boot_info` and prints "Boot
info IGNORED", is left as Utah shipped it.

Since 18 October 2026 the kernel's boot information is read on the
other branch too, for its flags only: `-s` at OSF's `boot:` prompt,
which the Release Notes say is "passed to server", reaches Lites that
way (`docs/deferred.md`, O-7). The root and server directory still
come from `bootstrap.conf` when it gives them.

**Since 0107, the OSF/1 server's arguments, not CMU's.** The section
above explains the 3.0-style line; the project is moving toward the
OSF/1 server, which takes flags and a root and nothing more
(`bootstrap.template`), runs `/sbin/init` (co-location paper §5.1), and
so has no server directory. Under `OSFMACH3` Lites now does the same:
the kernel's boot information is parsed first as a command line and
`bootstrap.conf`'s arguments after it; the root is the last one given,
else the boot device the kernel names (Release Notes bug 2271), else
Utah's `hd0a`; a word after the root is ignored, with a message,
since nothing documents one. OSF's template line, `startup startup`, is the
whole configuration. 0106 had kept the server directory, deriving it
from the server's own path; 0107 removes it, and Utah's code for it
stays only in the build for CMU's kernels (`docs/deferred.md`, O-1,
O-2, O-4, O-5).

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

5. **Third-party artifacts of OSF/1 that are not GPL**, added 26
   October 2026 at the maintainer's word: how other projects'
   toolchains, headers and build files treated OSF/1 and Mach (a
   compiler target for OSF/1, say, or a port's notes). They show what
   OSF/1 did, and are read for that. The behaviour is taken, and the
   code is written here. A GPL artifact is not read under this step.
   The maintainer let one be read once, on 26 October 2026, "just to
   collect information on how OSF/1 was compiled ... not reading the
   code line for line and copying it, just the publicly documented
   behavior"; that was for the question asked, and the next GPL file
   asks again. What was found is recorded as the behaviour and where
   it was seen (GCC 2.7.2.1's `config/i386/osfrose.h`: no
   `gcc2_compiled.` label for OSF/1 targets, the reason given there
   that GDB does not need it and kdb dislikes it).

6. **Our own code, last**, and only where the five above have nothing.

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

**GPL is still excluded.** GNU Mach, the Hurd, Linux and MkLinux's
`mklinux/` personality are not read, and nothing here may be derived
from them.

**Nor is any source that was not released for others to use.** The
OSF/1 server's own source never was. The design comes from OSF's
public documents and from the trees above; where they are silent, the
maintainer decides. Nothing unreleased is read, not even as a
specification.

### GPL files already in the tree, as imported

The rule above is about what may come in. Some GPL files came in with
the imports themselves, and stay, as imported, by the maintainer's
decision of 18 October 2026: kept, recorded here, not built into
anything. Found by `git grep -il "General Public\|GPL\b\|copyleft"`
over the whole tree, each hit read; the ones that are not GPL
(`gpl`, a floppy controller's gap length, in `chips/fdc_82077.h`,
`i386/AT386/fd.c` and `fdreg.h`; our own notes in
`file_systems/AT386/machdep.mk`) are left out.

| files | from | notice | built? |
|---|---|---|---|
| `file_systems/minixfs/minix_ffs_compat.c`, `minix_ffs_compat.h`, `minix_fs.h` | the OSFMK import, `4b29812`; MkLinux's | below OSF's notice, the GNU GPL, version 2 or later: "Copyright (C) 1994 Csizmazia Balazs" on the two `minix_ffs_compat` files, and on `minix_fs.h` "stolen … from the Linux distribution Copyright (C) 1994 Linus Torvalds" | no: not for AT386 since `file_systems/AT386/machdep.mk` and `fs_switch.c` were cut to UFS ("What OSF booted from") |
| `file_systems/POWERMAC/COPYLEFT/hfs/`, 19 of its 22 files: `block`, `btree`, `data`, `file`, `hfs`, `low`, `node`, `record`, `volume` (`.c` and `.h`) and `internal.h` | the OSFMK import, `4b29812`; MkLinux's, for the PowerMac | "hfsutils … Copyright (C) 1996, 1997 Robert Leslie", GNU GPL version 2 or later | no: only `POWERMAC/machdep.mk` includes it, and nothing here builds for the PowerMac |
| the same directory's `mainhfs.c` | as above | OSF's and CMU's notices, then "modified from original hfslib package by Brad Midgley … GPL'd" | no, as above |
| `lites/conf/config.guess` | Utah's Lites 1.1.u3, whose tree is `9d1c98a` (`imports.md`) | "Copyright (C) 1992, 1993, 1994 Free Software Foundation", GNU GPL version 2 or later; Utah's own `lites/doc/FILES` lists it "fsf gpl … not compiled, not necessary" | not compiled; it is run, as a program, by Lites' `configure` when no host is given, which `lites/makefile` does not give, and prints the host's name |

The rest of `POWERMAC/COPYLEFT/hfs/`, `mainhfs.h` (OSF's and CMU's
notices) and `machdep.mk` (a build file), carry no GPL text. Lites'
two `configure` scripts are the Free Software Foundation's, but with
"unlimited permission to copy, distribute and modify it": not GPL.
`file_systems/ext2fs/ext2_fs.h`, with no GPL text but derived from
Linux's header, is recorded in "What OSF booted from".

None of these is a donor either: nothing here is taken from them,
and a change that would build one of them, or link it into a
binary, comes back to the maintainer first.

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

The seam is Lites' `server/serv/`, where its Mach-facing glue is, and
that is where the conventions change.

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

4. **Behaviour with no precedent is stated, and confirmed on its own.**
   Set by the maintainer on 24 October 2026 as a hard rule. Whenever a
   change creates behaviour that follows no precedent in the order
   above, the proposal says so: it names the behaviour, gives the
   alternatives that do have a precedent, and asks the maintainer to
   verify it. That verification is separate from the one the code
   gets: a patch approved is not its unprecedented behaviour
   approved. The search behind "no precedent" is the one "Proving an
   absence" asks for, and what the maintainer decides is recorded
   with the change (step 5's, in `docs/deferred.md`, "Step 5's
   choices").

## Proving an absence

A found donor proves itself: the line is shown. An absence does not.

No proposal may state that no donor exists unless every tree above has
been searched, and the commit message names them. For this project that
is: `osfmk7.3/`, `lites/`, `bsd/`, OSFMK 6.1, Rhapsody, and XNU from
`rel/xnu-124` forward.

Six trees, and the third-party sweep of step 5. The sweep is cheap and the claim is worthless without it.

## What in this tree is ours

Nothing yet. This section fills as work lands, in the Lite2 tree's
form: a table of every file and change that is ours, with its size and
what it is, so that the answer to "what did this project write" is a
lookup rather than a diff.

The expected entries, from `docs/roadmap.md`:

| file or change | expected kind |
|---|---|
| `syscall.c`, the exception-driven system-call entry, in the server's glue | ours, written from Patience §3–4 against OSFMK's exception interface |
| `THREAD_STATE_SYSCALL` in `osfmk7.3/` | ours; a new i386 thread-state flavour, no donor has it |
| building the server | a `build -here lites` stanza after `mach_services`, OSF's step 8, plus whatever bridges Lites' own configuration to ODE. `build_world` is a vendor file whose header invites exactly this kind of edit |

Our own files carry an SPDX-FileCopyrightText line. Whether they carry
a licence identifier, and which, is the maintainer's decision and is
not asserted here.
