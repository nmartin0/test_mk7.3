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

## Order of work

The reference survey changed the shape of step 4. The first program went
from unsolved to a port with a known-good source, and the step *after*
it turned out to be the real long pole.

**1. Move the server volume to ext2.** *(done -- see boot-ide.sh)*
Retired `tools/mkminix.py` and removed GPL code from the bootstrap task
binary: `file_systems/minixfs` contains three GPL files and
`minixfs/machdep.mk` builds one of them into `libsa_fs.a`. `ext2fs` is
GPL-free and the bootstrap task already tried it first.

**2. Source a NetBSD 1.0 userland.** This must come **before** porting
`mach_init`, which is the correction that reordering this list is for.

`mach_init` is a BSD process -- its `fork`, `execve`, `open`, `kill`,
`waitpid` and `sigblock` go through the emulator into LITES. It cannot
be built as a standalone Mach program: `libsa_mach` supplies headers but
only `printf`, `exit` and `sleep`, none of the POSIX calls. It needs a
libc.

**LITES's own documentation settles how that libc arrives.**
`doc/install.freebsd`, by Helander, December 1994:

```
Installing Lites on a FreeBSD machine -- jvh 941204
- Install FreeBSD 2.0 on the machine
- Install the Mach bootable kernel in the root directory (e.g. /mach.boot)
- Create a /mach_servers directory
- Populate with startup, emulator, mach_init
- Create a paging file. ...
  ln -s /dev/sd0g/PAGING_FILE /mach_servers/paging_file
```

and `doc/README.netbsd` documents the same against **NetBSD 1.0**, which
is the one chosen here.

**LITES takes over an existing BSD installation.** The BSD system
provides `/sbin/init`, `/bin/sh`, libc and the whole userland; Mach adds
three files to `/mach_servers`. There is no hand-rolled libc in the
design, so building one would be inventing something the project never
had.

Two details there correct assumptions this project has been running on:
`/mach_servers` holds exactly `startup`, `emulator` and `mach_init`, and
the pager is given a **paging file** rather than the raw `hd1c` device
we currently use -- which matches `mach4-UK22`'s `def_pager_setup.c`.

### The a.out toolchain problem, and a way round it

**Linking `mach_init` against NetBSD's libc needs a toolchain we do not
have.** `comp10` supplies `usr/lib/libc.a` (453 KB) and the full
`usr/include`, and the symbols are there -- `_open`, `_execve`, `_fork`,
`_printf`, `_sigblock` all present with a.out's leading underscore. But:

```
$ nm usr/lib/libc.a
nm: truncate.o: file format not recognized

$ ld --version && ld --help | grep 'supported targets'
GNU ld (GNU Binutils for Ubuntu) 2.42
ld: supported targets: elf64-x86-64 elf32-i386 ... pe-i386 ... binary ihex
```

`ar` reads the archive, but every member is a.out and **binutils 2.42
has no a.out target at all**. Alan Modra's "various i386-aout and
i386-coff target removal" deleted `bfd/i386netbsd.c` among others, so
anything recent cannot link these objects.

Building a pre-removal binutils (around 2.30) as
`--target=i386-netbsdaout` would work, and the GitHub mirror
`bminor/binutils-gdb` is reachable from the sandbox. That is a real but
bounded piece of work.

**But it may not be necessary.** `/sbin/init` is already a working
NetBSD binary, and LITES has a flag to run it directly:

```c
		    case 'i':
			/* Allow non-default init program file name: */
			strcpy(init_program_name, argv[1]);
```

It sits inside `#if SECOND_SERVER`, and our build has
`#define SECOND_SERVER 1` in the generated `second_server.h`, so **the
flag is compiled in**.

If LITES can be pointed straight at `/sbin/init` via `bootstrap.conf`,
then `mach_init` is not needed to reach a shell, the cross-toolchain is
not needed to build it, and the port already committed becomes
belt-and-braces rather than a dependency. Worth testing before building
any toolchain.

### Verified against the real NetBSD 1.0 sets

The sets are mirrored in the reference collection at
`nmartin0/mach_stuff` under `netbsd-1.0-i386/binary/`, so they can be
inspected directly rather than reasoned about. What they show:

**`/bin/sh` and `/sbin/init` are statically linked.** Their a.out
headers, with `a_midmag` read big-endian as NetBSD packs it:

| file | midmag | MID | flags |
|---|---|---|---|
| `bin/sh` | `0x0086010b` | 134 (`MID_I386`) | **0x0** |
| `sbin/init` | `0x0086010b` | 134 | **0x0** |
| `bin/ls` | `0x0086010b` | 134 | **0x0** |
| `usr/libexec/ld.so` | `0xc086010b` | 134 | 0x30 |

`EX_DYNAMIC` is `0x20` and `EX_PIC` is `0x10`, so only `ld.so` itself is
dynamic. NetBSD 1.0 kept the traditional rule that `/bin` and `/sbin`
are static because `/usr` may not be mounted at boot.

**That removes the largest risk in this plan.** There is no `ld.so` on
the path to a shell prompt, so the dynamic-linking question -- whether
`ld.so`'s own mmap and relocation work survives the emulator -- does not
arise until we want something from `/usr/bin`.

**The emulator's test matches exactly.** `bin/sh`'s first four bytes
read little-endian are `0x0b018600`, which is precisely the constant
`emul_exec.c` compares against. That code was written against this
release.

**The `NEED` list was a guess and is now fact.** Every binary in it
exists at the path assumed, and the list has been widened to the useful
contents of `bin` and `sbin`.

### Correction: the emulator knows NetBSD explicitly

An earlier note here said NetBSD binaries "fall through to
`BT_FREEBSD`". That is true of `liblites/exec_file.c`, but **not** of
the emulator, which is what actually runs user programs.
`emulator/emul_exec.c` tests for NetBSD first and by name:

```c
if ((exdata.magic & ~0xfc) == 0x0b018600) {
	/*
	 * NetBSD magic's are in inverted byte order
	 * 0xfc is mask for flags field.
	 */
	*binary_type = BT_NETBSD;
```

`0x86` is 134, `MID_I386`. So NetBSD/i386 a.out is recognised properly
and gets `BT_NETBSD`, not a fallback.

**And `~0xfc` masks out the flags field deliberately**, which is where
NetBSD's `EX_DYNAMIC` bit lives. Static and dynamic binaries therefore
both match this test, on purpose. That is a deliberate accommodation of
shared libraries rather than an accident.

NetBSD 1.0 is the release where i386 gained shared libraries, per
NetBSD's own release notes, so `/bin/sh` may well be dynamic. Whether
`ld.so` then runs correctly under the emulator is a separate question
and untested -- but the binary will at least be classified correctly,
and `e_trampoline.c` gives `BT_NETBSD` the same BSD syscall table as
`BT_386BSD` and `BT_FREEBSD`.

### What the loaders expect from a NetBSD binary

Settled before going looking for install media.

`doc/README.netbsd` warns that "the only thing you need to do is to make
your bootstrap grok binaries with NetBSD's a.out header", so there is
known work on the bootstrap side. On the **LITES** side there is none,
and the reason is worth writing down because it also explains the
`BT=20` puzzle.

`liblites/exec_file.c` classifies a.out by the machine id in
`(magic >> 16) & 0xff`:

| MID | classified as |
|---|---|
| 100 | `BT_LINUX` / `BT_LINUX_SHLIB` |
| 0 | `BT_CMU_43UX` (entry non-zero) or `BT_386BSD` |
| 0x45 | pc532 |
| anything else, QMAGIC | **`BT_FREEBSD`** |

NetBSD/i386's MID is not in that switch, so its QMAGIC binaries fall to
the default and are classified `BT_FREEBSD`. **That does not matter**,
because `emulator/i386/e_trampoline.c` gives them all the same syscall
table:

```c
      case BT_386BSD:
      case BT_NETBSD:
      case BT_FREEBSD:
      default:
	current_nsysent = e_bsd_nsysent;
	current_sysent = e_bsd_sysent;
```

So a stock NetBSD userland is emulated correctly whichever of those it
is called.

**And the entry-address tests are not bugs.** `BT_LITES_Q` requires
`a_entry >= 0x90000000` and `BT_LITES_ELF` requires
`e_entry > 0x10000000`, with a comment in the source explaining that the
QMAGIC threshold was *raised* from `0x10000000` because "linux ld.so in
QMAGIC form has an entry of 0x62f00020 but we really don't want it to be
recognized as a BT_LITES_Q".

Those tests are how LITES tells **its own** binaries -- linked high, as
our emulator is at `0xa0001020` -- from foreign ones. Our server is
reported as `BT=20` because it is an ELF at `0x8049320`, below the
threshold, so it is not recognised as LITES-native. Whether that matters
depends on what a LITES-native ELF is supposed to look like, which is
the question to settle when item 4 comes round -- it is a narrower
question than "the classifier is broken".

### The root filesystem: ext2, and what goes in it

**Use ext2, not FFS.** `README.netbsd`'s bootstrap patch exists because
4.4BSD split `d_reclen` into `d_type` and `d_namlen` in the **FFS**
directory entry -- the same change ext2's `filetype` feature makes, and
which this project already handled with `-O ^filetype`. That patch is
only needed if the userland lives on FFS. Both our readers already
handle ext2, and it is proven working for the root and the server
volume.

**ext2 can hold a complete Unix root.** `debugfs` does all four things
needed, without mounting and without privileges:

```sh
debugfs -w -R "write localfile /path"      root.img   # files
debugfs -w -R "mkdir /sbin"                root.img   # directories
debugfs -w -R "symlink /bin/sh /sbin/sh"   root.img   # symlinks
debugfs -w -R "mknod /console c 0 0"       root.img   # device nodes
```

The `mknod` was tested: it produces mode `20000`, a character device.

**The device numbers come from LITES's own `cdevsw`**, in
`server/i386/conf.c`. Character majors:

| major | name | note |
|---|---|---|
| 0 | `console` | what `mach_init` opens |
| 1 | tty | controlling terminal |
| 2 | kmem, null | |
| 3 | `hd` | ISA disk, block major 8 |
| 5, 6 | pts, ptc | pseudo-terminals |
| 7 | log | |
| 8 | `com` | serial |
| 9 | `fd` | floppy, block major 8 |
| 13 | `sd` | SCSI disk |
| 15 | `cd` | CD-ROM |

So `/dev/console` is `mknod c 0 0`, which is what `mach_init` needs to
open before it can report anything.

### Where NetBSD 1.0/i386 lives

```
https://archive.netbsd.org/pub/NetBSD-archive/NetBSD-1.0/i386/binary/
    base10/   28 pieces base10.aa .. base10.bb, 240640 bytes each
              (~6.7 MB; cat them together for a gzipped tar)
    etc10/    /etc, including the rc scripts and ttys init reads
    comp10/   compiler, headers and libc for building mach_init
```

Dated 19 October 1994, which is the release LITES's own
`doc/README.netbsd` was written against.

`tools/mkroot-netbsd.sh` fetches the base and etc sets, extracts them,
and builds an ext2 root with `/dev/console` and the other device nodes
from LITES's `cdevsw`. It installs an explicit list of binaries rather
than the whole set, to keep the image small and make the dependency set
visible rather than implied.

**3. Link `mach_init` against that libc.** The requirements were worked
out ahead of time and are small.

From **NetBSD's libc**, all standard 4.4BSD:

```
open close dup execve fork kill getpid getppid
sigblock sigmask sigpause sigsetmask alarm
printf fprintf fflush _exit
```

From **Mach**, only three symbols: `cthread_fork_prepare`,
`cthread_fork_parent` and `cthread_fork_child`, which are in
`libcthreads`. Everything else went with the service server -- `main.c`
now uses **no Mach types at all**; the only `task_t` and port references
left are inside its HISTORY and explanatory comments.

Those three stay rather than being dropped as a further simplification.
`cthread_fork_prepare()` calls `vm_inherit(mach_task_self(),
p->stack_base, p->stack_size, VM_INHERIT_COPY)` so the child gets the
cthread stack, and `main.c`'s own HISTORY records that the explicit
calls were added deliberately, so someone found them necessary.

So the link is `main.o` + NetBSD libc + `libcthreads` + `libmach`, with
`comp10` supplying the first. The Mach traps `libcthreads` makes work
under LITES because Mach is underneath it. The port itself is done and
committed at `mach_services/cmds/mach_init/`; it compiles and waits only
for a libc.

**4. Fix ELF binary classification.** `liblites/exec_file.c` recognises
an ELF as its own only when the entry is above `0x10000000`; ours are at
`0x8049320`, which is why the emulator reports `BT=20` (`hpelf`). This
bites the moment an i386 ELF first program is exec'd. xMach shows the
shape of the fix.

## Multi-user boot works: init, rc, getty, login, csh

A cold boot reaches a login prompt unaided, and logging in gives a
working shell on a writable root. Evidence at the head of
`docs/current-blocker.md`. That completes what this roadmap called
step 4, and then some: the original goal was "a shell prompt".

**What is left is no longer a chain of blockers but a list of
independent things, none of which stops the system running:**

1. **The emulator terminates on an unmappable error**
   (`emulator/error_codes.c:49`) rather than returning one. That
   turned a missing return statement into a dead process, far from
   its cause. The cheapest robustness win available.
2. **Look for the fourth `mach_error_t` returned as an errno.** Three
   were found in one session; the table in `docs/current-blocker.md`
   says what the shape looks like.
3. **`/dev/mem`**, or a decision not to have one, for `ps`.
4. **The paging file**, replacing raw `hd1c`.
5. **More userland.** The root carries a deliberately small subset of
   the NetBSD sets. `id` is already missing, and anything beyond the
   `NEED` list in `mkroot-netbsd.sh` will be too.
6. **What the console does with two claimants** -- see the correction
   in `docs/current-blocker.md`. Not on the critical path.

## Superseded: the login chain works: getty, login, csh

A complete BSD login runs under LITES. Evidence at the head of
`docs/current-blocker.md`. The blocker was `set_task_priority()`
having no return statement, so `donice()` returned an uninitialised
register as `setpriority(2)`'s error.

**Next, in order:**

1. **`/etc/ttys`: turn the console line on.** It is installed
   unmodified, with `console` off and `ttyv0` on, so multi-user init
   spawns nothing. This is the last piece before a login prompt
   appears without being asked for -- and the first real test of
   multi-user boot.
2. **The console drops characters** once getty reconfigures the line.
   A password prompt tolerates dropped input far less than a username
   does.
3. **The emulator terminates on an unmappable error** rather than
   returning one. That turned a missing return into a dead process.
4. **`/dev/mem`**, or a decision not to have one, for `ps`.
5. **The paging file**, replacing raw `hd1c`.

## Superseded: the login chain: getty prompts, login does not run

`/etc` is populated and the login chain installed by
`mkroot-netbsd.sh`. getty runs, sets the terminal and prompts; login
dies in `setpriority`. Dynamic linking works, which was the open
question -- these are the first non-static binaries this project has
run.

**Next, in order:**

1. **`donice()` returns a raw Mach error** from
   `set_task_priority()`, which the emulator cannot map, so it
   terminates login. Same class as the TTY_STATUS bug just fixed in
   `tty_param`. Decide whether to translate it or to treat a failed
   Mach policy set as non-fatal, and separately whether the emulator
   should terminate at all when an error will not map.
2. **The console drops characters** once getty reconfigures the line.
   A login that cannot print its prompt cannot read a password.
3. **`/etc/ttys` needs the console line turned on** for a multi-user
   boot; it is installed unmodified, with `console` off and `ttyv0`
   on. Nothing can use it until 1 and 2 are done.
4. **`/dev/mem`**, or a decision not to have one, for `ps`.
5. **The paging file**, replacing raw `hd1c`.

## Superseded: step 4 is done, and the root is read-write

`/bin/sh` runs commands, and after `/sbin/mount -u -w /` it can write:
files created under LITES reach the disk and the result passes
`e2fsck` clean. Transcript and evidence at the head of
`docs/current-blocker.md`.

Item 1 below -- the read-write root -- turned out not to be the large
piece of work it was recorded as. It was not an ext2 defect at all: the
read-only mount is what 4.4BSD does, FFS does it identically in this
same tree, and the remount path already existed. What was missing was
`/dev/hd0c` and `/etc/fstab` in the root image, both now built by
`mkroot-netbsd.sh`. The write path itself worked first time.

**So the remaining work is userland assembly, in this order:**

1. **`/etc` and the login chain.** Now the top item.
   `mkroot-netbsd.sh` installs only `bin/` and `sbin/` binaries, so
   `/etc` holds nothing but the `fstab` just added: no `rc`, no
   `ttys`, no `getty`, no `login`, no password database. An `/etc/rc`
   would also make the read-write remount automatic, as it is on a
   real BSD. Multi-user init needs `ttys` to spawn anything at all.
2. **`/dev/mem`, or a decision not to have one**, for `ps`.
3. **The paging file**, replacing raw `hd1c`.

## Superseded: step 4 is done: a shell runs commands

`/bin/sh` executes commands typed at the console. The transcript is at
the head of `docs/current-blocker.md`; `ls /` alone exercises fork,
exec, an ext2 directory read and tty output.

`tools/console.py` is what made it reachable -- the serial line is now
a socket that can be answered, rather than a file that can only be
read.

**What the shell immediately showed is the next work, in order:**

1. **A read-write root.** `ext2_vfsops.c:117` mounts `MNT_RDONLY`, and
   the shell confirms it: `cannot create /tmp/x: read-only file
   system`. Nothing can be written anywhere. This is the largest item,
   because it is the first thing to exercise ext2's write path, which
   has never run in this project -- expect that to be a piece of work
   in itself rather than a flag change.
2. **`/etc` and the login chain**, for a multi-user system.
   `mkroot-netbsd.sh` fetches the `etc10` set but its `NEED` list
   installs only `bin/` and `sbin/` binaries, so `/etc` in the built
   root is **empty**: no `ttys`, no `rc`, no `getty`, no `login`, no
   password database. Multi-user init would read no `ttys` and spawn
   nothing. Extending that list is cheap; making `login` work needs
   the password database and a writable `/var` for `utmp`.
3. **`/dev/mem`**, or a decision not to have one. `ps` fails with
   `Device not configured`. A 1994 BSD `ps` reads the proc table out of
   kernel memory, which under a microkernel is not where it lives, so
   this is a design question rather than a missing node.
4. **The paging file.** `default_pager` is given raw `hd1c`;
   `doc/install.freebsd` describes a paging file in `/mach_servers`.

## Superseded: where step 4 stood after session 6

Step 4 is further than the sections below assume, and the remaining gap
is different from the one they describe.

**A NetBSD 1.0 userland is sourced and running** (step 2 is done, not
pending). `tools/mkroot-netbsd.sh` builds the ext2 root from the real
sets; `boot-ide.sh` installs `emulator` and `init` into
`/mach_servers`. LITES execs NetBSD's own `/sbin/init`, which acquires
the console, forks, and runs `/bin/sh`.

**So "a shell prompt" is reached**, in the sense that init prints its
single-user prompt and waits for input. What is not yet done is
answering it: `boot-ide.sh` gives the guest a serial console written to
a file, which cannot take keystrokes. `tools/boot-debug.sh` is the one
with an interactive console, and driving `/bin/sh` by hand through it
is untried.

**What blocks an unattended boot is `kern_exit.c`'s hard-coded pid 2**,
not the absence of `mach_init` -- see the head of
`docs/current-blocker.md`. That reframes 4a below: porting `mach_init`
is *one* of three ways to clear it, and it is the faithful one, because
the hack is correct whenever pid 2 really is `mach_init`. The other two
are to condition the hack on the init program, or to drop it for a
directly booted BSD init. Both of those touch LITES source and would
need regenerating into `tools/lites/lites-osfmk73.patch` in the same
commit.

4b is done: `boot-ide.sh` grew the population step.

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
