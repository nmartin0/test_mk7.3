# Sources found on 20 October 2026, and what to take from them

A search for the pieces DR3's DIPC lacks, and for anything else
useful, through every OSF, MkLinux and Mach source we could reach.
This page records what each source is, what may be copied from it,
what it settled, and what to inherit. Facts, not legal advice; the
rules are `docs/provenance/conventions.md`'s and `precedent.md`'s.

## The sources

| source | what it is | may we copy |
|---|---|---|
| MkLinux DR3 CD, Prime Time Freeware, July 1998 (Internet Archive, `mklinuxforpowermacintoshdr3primetimefreewarejuly1998`; ISO SHA-256 `6c53a72aa2795af5d102ea62ac65b3d51d281e56048acf3df7d90eed807e5cb7`) | Apple's DR3 release. `mklinux_source/` holds `DR3.osfmk.src.tgz` (11 June 1998, 2,990 entries, owner `gilbert`), `DR3.osfmk.export.tgz`, `DR3.tools.tgz`, `DR3.mklinux.src.tgz` (the Linux server, GPL), `ode-bin.tgz`; `misc_source/` holds `cmk1.1-980417.src.tgz` (OSF's Mach commands and servers, 17 April 1998), `gdb-4.16-mk.tgz`, `install.tgz` | the OSF parts, under OSF's 1991--1998 notice; not `DR3.mklinux.src.tgz` or gdb (GPL) |
| "All material on the Mach Microkernel I can find" (Internet Archive, `mach-microkernel-files`), `OSF/osf-6.1/` | OSF RI's "mk6.1_free" (nmk18b5, README of 1 February 1995): the whole kernel, `src/` (bootstrap, default_pager, mach_services, stand, tools) and the documentation, papers and release notes | yes, OSF's unencumbered release |
| the same item, `linux/osfmk-src-12.24.99-2.tar.gz`, and `OSF/unknown/osfmk.tar.gz` (the same tree) | the MkLinux project's CVS tree (`cvs.mklinux.org:/MkLinux`) on 24 December 1999: DR3 imported 25 October 1998, then fourteen months of community commits, nearly all PowerPC | file by file: OSF's notice yes; its GNU Mach device server, `COPYLEFT/` (GPL BootX) and NetBSD USB no |
| the same item, `hurdppc/gnu-osfmach.tar.gz` | GNU's PowerPC port of OSF Mach | not read (GNU, excluded) |
| unix.superglobalmegacorp.com's CVS archive (Internet Archive, `web_and_cvs.repos_superglobalmegacorp`, `cvsd.repos.sqsh`, 622,411,776 bytes) | the site's CVS repositories, among them `osfmk` (three imports: `osf61`, MK6.1; `mkLinux1999`, the tree above; `gnu`/`gnuosfmk`, a 2004 GNU/Hurd PowerPC port, not read), `lites` (Lites 1.0, 1.1, 1.1.u1, 1.1.u3), `xmach` (Lites and Mach 4, 2000), `mach3` (CMU MK42--MK84 kernels, libmach, threads, bsdss), `darwin0`, `xnu` | per repository |
| `rohsaini/mkunity` on GitHub | another copy of slp's DR3 | nothing new |
| `xnu-123.5` (Apple) | DIPC removed | nothing for this |

The live cvsweb at unix.superglobalmegacorp.com answers 404 for every
repository; the archive is the way in.

## What they settled

**DR3's DIPC was never finished in anything released.**
`KMSG_CONNECTING` and `kmsg_connect_reply_wait` (`ipc/ipc_mqueue.c`)
are referenced in DR3, in the 1999 MkLinux tree and in the
`mkLinux1999` import, and defined in none of them, nor in MK6.1 (which
has no DIPC; it has CMU-style NORMA IPC), MkLinux 1.0b2 or XNU. The
other three spots that stop a NORMA kernel building (`ipc_mig.c`'s
memory-object traps under `NORMA_VM`, advisory pageout, the
`string.h` in `kern/misc_protos.h`) are the same everywhere. MkLinux
1.0b2's OSF tree (NMK b6) has each spot as it was before OSF's change;
`dipc/` itself differs from DR3's by 49 lines of code, `flipc/` by
none.

**Our import is DR3.** slp's first commit, `0f6f7e6`, with its
`start.s~` put back as `start.s`, is byte for byte Apple's
`DR3.osfmk.src.tgz`, but for empty directories git cannot hold
(`cbus`, `mbus`, `norma`, `ppc/POWERMAC/floppy`, `obj`) and for what
DR3 shipped in separate tarballs (`export`, `tools`, `link`). Every
verdict written against `0f6f7e6` stands. Our tree against the CD: 38
changed files, all recorded (notes at the sites, `imports.md`), and
the extra files all accounted for, but for these:

- `i386/start.s~`, slp's editor backup: not DR3's, not recorded.
- Six symbolic links DR3 ships are plain files here. `export.tgz` has
  five under `export/powermac` (`include/machine`,
  `include/mach/machine`, `include/sa_mach/machine` and
  `mach_kernel/machine` to `ppc`; `include/profile/profile-md.h` to
  `ppc/profile-md.h`), and `tools.tgz` has `link/tools` to
  `../tools`. Ours are small files reading "link ...", as slp's
  archive flattened them.
- `imports.md` lacks rows for `i386/mp_desc.c` and `ipc/ipc_port.c`
  (0095), which have notes at their sites. Added with this page.

**The MkLinux 1999 tree is newer, but not OSF's.** Its x86 changes
worth taking are bug fixes: `vm/vm_kern.c` (`kmem_alloc_contig()` uses
the map entry before checking `vm_map_find_space()`);
`device/net_device.c` (`IOBUS_LOCK_INIT` given
the start routine for the interrupt routine); `scsi/rz_disk.c` (a
capacity of `... | lba4 + 1`, `+` binding first); `libmach/
mach_msg_server.c` (buffers leaked on early returns; its own fix
swaps two flags wrongly in one of its two functions);
`libsa_mach/printf.c` (`%h`, precision);
`bootstrap/bootstrap.c` (CR and CRLF in `bootstrap.conf`, `*ptr` for
`*pptr`). Its device layer is GNU Mach's (Elgin Lee's 1998 port),
excluded. Its `kern/lock.c` has the `return; // dg` slp's `44e8969`
copied, already reverted here.

**OSF's DR3 shipped in two packages.** `cmk1.1` carries what DR3's
tree lacks: `compose` and `ptbldump`, the x-kernel tools the
`NORMA+net` kernels run; `makeboot`'s and `mig`'s sources (DR3 had
only their PowerPC binaries); FLIPC's tests and examples; `ipc_test`,
`msg_test`, `mprof`, `etap` and small Mach commands; and the
user-space x-kernel server, `netmsgserver`, `netmem`, `rpc`,
`snames`. Imported in 0121 (`imports.md`, kind F), but for `etap`'s
`skiplist.[ch]`, "UNDER THE GNU COPYLEFT".

**OSF's networking for DIPC went from user space into the kernel.**
Arizona's x-kernel on Mach (1993--94) ran in user space, reaching any
Mach network device by name (`s_xkern`'s `ethdrv`). MK6.1 had none.
MkLinux 1.0b2 moved it into the kernel for DIPC ("xkern/dipc merge",
July 1994), with glue per driver (`xkwd` in `if_ns8390.c`) and a
shared receive side, `device/eth_common.h` ("meant to make the driver
adaptation simpler"). DR3 adds `device/net_device.c`, a generic driver
framework whose x-kernel object, `xknet/<unit>`, needs no glue per
driver -- under `NET_XKMACHKERNEL`, which nothing defines, with an
"XKERNEL TODO list"; its only user is the HP700 Myrinet driver. OSF's
NORMA IPC Version Two paper (MK6.1's `mk_papers/norma2.ps`, October
1994) has DIPC speak only to KKT, which "may select various
lower-level software transports and/or physical media". For QEMU's
NE2000, either glue in `if_ne.c` after `xkwd` (an 8390 like the WD
cards, but data through an I/O port), or `if_ne.c` moved onto
`net_device`, OSF's newer way; both new code.

CMU's kernels date OSFMK's branch from CMU: OSF MK6.1's `kern/ipc_tt.c`
history ends at CMU's revision 2.12, MK67's; `kern/thread.c` has
MK67's 2.22 with CMU's NORMA branch to 1992, not MK74's 2.26.

**Notices in DR3 itself.** Files OSF published in DR3 under its own
notice also carry older restrictive ones: IBM's CDLI (`device/cdli.[ch]`,
`eth_demux.[ch]`, `nd_lan.[ch]`, `ndd.h`: "Licensed Materials -
Property of IBM"), Intel's (`stand/AT386/misc.c`, "INTEL CORPORATION
PROPRIETARY INFORMATION"; `device/tape_status.h`, "INTEL
CONFIDENTIAL"), AT&T's (`mach_perf/standalone/HP700/machine.c`), and a
Prime Computer mention in `i386/pic.h`'s history (its notice is
CMU's). The CDLI files are not built (`CDLI` 0). **For the
maintainer**: whether OSF's own publication covers them, as the
project has assumed, or whether the unbuilt ones should leave the
tree.

## What to inherit

Each item would be its own patch, with notes and the maintainer's
OK; none is taken by this page.

1. **Done, 0121--0122**: `cmk1.1`'s commands and servers, and
   `compose` and `ptbldump` built by `mkhostbin.sh`.
2. **Done, 0125**: the six bug fixes from the 1999 tree above, as
   kind B rows, each with the file's own line.
3. **`makeboot` and `mig` built from source** (`cmk1.1`), replacing
   the PowerPC binaries' scripts `mkhostbin.sh` copies.
4. **DR3's six symbolic links restored**, and `start.s~` removed or
   recorded.
5. **PCI and drivers from MkLinux 1.0b2's OSF tree**: `device/
   dma_support.c`, `dma_direct_map.c`, `busses/pci/pci.h`, `if_tu.c`
   (DEC 21x4x; QEMU has a `tulip`), `if_fta.c`, `scsi_53C810_hdw.c`
   (QEMU has an `lsi53c810`), and `cbus/`, `mbus/` -- all named by
   DR3's own `conf/files`. Older than DR3; each would need checking
   against DR3's interfaces.
6. **MK6.1 as the second place to look**, now whole, in
   `precedent.md`'s order.
7. **For the DIPC work, when it resumes**: the four spots from
   MkLinux 1.0b2's text, the `cmk1.1` FLIPC tests, and a choice
   between `xkne` glue and `net_device` for `if_ne.c`.
8. **For Lites**: the `lites` CVS history (1.0 to 1.1.u3) and
   `xmach`'s Lites, as donors after `nmartin0/4.4BSD-Lite2`'s `dev3`.

## Checked again, 20 October 2026

**DIPC's gap, searched for a second time.** Every tree and archive
above, every revision in the superglobalmegacorp `osfmk` CVS file
`ipc_mqueue.c,v`, `darwinstep` and the web. `KMSG_CONNECTING` and `kmsg_connect_reply_wait` appear
only in copies of `ipc/ipc_mqueue.c`, and are defined nowhere.
`c_imr_connect_reply_race`, counted among the missing before, is
defined in that same file (line 948). So the gap is two names in one
file; the rest is the three mismatched spots above. `net_device.c`'s
`xknet` code is switched off (`NET_XKMACHKERNEL`), not incomplete:
it names nothing undefined, though it does not compile as it stands
(an undeclared variable, two names declared only elsewhere, a
function with no prototype, one comparison of unlike pointers).

**The 1999 tree's USB stack is not worth taking.**
`mach_kernel/busses/usb`, about 12,400 lines, is NetBSD's 1998 USB code
inside the kernel (not Lites), under the NetBSD Foundation's
four-clause licence. Only the PowerMac configuration names it, as
`pseudo-device usb 0`; its `bsdcomp.h` is empty, it includes
`sys/device.h` and `sys/bus.h`, which the tree lacks, and it has no
PCI attachment. It was never running. A later, complete NetBSD stack
would be the donor if USB is ever wanted.

**`net_device` and the x-kernel.** The x-kernel (University of
Arizona) builds network stacks from protocol modules joined by a
configuration file; OSF used it for kernel-to-kernel messages, which
DIPC rides on. `net_device` is DR3's shared network-driver framework:
a driver supplies two short tables of routines, one for the card and
one for how it is wired to the machine (ports or shared memory), and
the framework does registration, interrupts, queues and the x-kernel
attachment. Only one DR3 driver uses it (HP700 Myrinet). It does not
let a BSD driver drop in; the driver still has to be cut into those
routines. For `if_ne.c` the shipped, tested way is the WD driver's
`xkwd` glue in `if_ns8390.c` (some 400 to 600 lines); moving it onto
`net_device` means first making the framework's x-kernel half build.

**`stand/AT386/misc.c`, `console.c`, `rmcoffhdr.c`** (Intel's notice)
are built by nothing: `stand/AT386`'s makefile builds `boot/` and
`rmelfhdr`, and the boot has its own A20 code. They shipped on the
DR3 CD, in MkLinux 1.0b2 and in the 1999 tree, and stay as shipped.

**More to consider**, each its own patch if wanted:

9. MK6.1's `cthread_mach_msg()` (`libcthreads/cthreads.c`, about 150
   lines with its helpers), which our `cthreads.h` already declares
   and nothing defines; it bounds how many threads wait on one port,
   the deferred item "Lites' receive threads". Needs `cthread_block`
   and the `CTHREAD_FILTER` hooks checked first.
10. MK6.1's documentation (the Kernel Interfaces, the Server Writer's
   Guide, Kernel Principles, release notes, the OSF RI papers), about
   5 MB of PostScript, into `/docs`.
11. The 1999 tree's `ppc/POWERMAC/if_tulip.c` (NetBSD `if_de.c` 1.81,
   BSD licence, on OSFMK's PCI layer), a second network driver QEMU
   emulates on x86, once PCI is up.
12. MK6.1's tests and libraries (`ipc_test`, `xptest`, `mach_perf`,
   `librthreads`, `libxmm`), each checked against 7.3's interfaces.
13. CMU's `lites/user` programs from the Lites CVS (`mach_init`, `ps`,
   `top` and others), CMU's licence.

`gdb-4.16-mk` on the DR3 CD is GPL: a tool to run, not to merge.

## Checked by trying, 20 October 2026

Each candidate tried in a copy of the tree; nothing taken.

**DIPC's two names guard nothing in DR3.** `mach/message.h` defines
`MACH_MSGH_BITS_CONNECTING` and `MACH_MSGH_BITS_CONNECT_WAIT` (lines
237-238), in the space 1.0b2 left spare
(`MACH_MSGH_BITS_DIPC_EXPANSION`), and nothing in DR3, 1.0b2 or the
1999 tree sets either; only `ipc_kmsg.c`'s debug printer names them.
So `KMSG_CONNECTING()` could only be false, and the block in
`ipc_mqueue_finish_receive()` that uses the two names does nothing:
without it, DR3 receives as 1.0b2 does, whose function has no such
block. On one processor, without `MACH_RT`'s preemption, the race the
block's comment describes cannot happen; on several it is open, as in
1.0b2. A guard would be our own code, in `dipc_receive.c` as well.
Searched for again, online: OSF's MK7.3a Release Notes (28 October
1998) confirm the `+NORMA+net` configurations shipped; no source for
MK7.x outside DR3 and its descendants was found.

**Item 5, reversed: don't take 1.0b2's PCI header or `if_tu.c`.**
1.0b2's `busses/pci/pci.h` is marked `@DEC_COPYRIGHT@`, "Originated
from DEC alpha release", and its `if_tu.c` "Taken from DEC sources":
DEC's code, likely why DR3 lacks them. DR3's `busses/pci/pci.c` and
five other files include `pci.h`, so DR3's PCI does not build without
one; a clean header would come from NetBSD's or FreeBSD's PCI
definitions.

9. **`cthread_mach_msg`: not taken.** `docs/deferred.md`, "Lites'
   receive threads": 7.3's library no longer has what 6.1's code
   uses. Trying it found `csw.s`'s `USER_INVOKE`
   defect (`docs/deferred.md`).
10. **MK6.1's documentation fits**: 7.1 MB, the 1993 specifications,
   Kernel Principles, the Server Writer's Guide and Server Library,
   release notes, eight RI papers; OSF's "permission to reproduce"
   notice on the specifications, the guides, the release notes and
   the rthreads paper, none on seven papers and two notes. It
   describes 6.1. Better taken first: OSF's MK7.3 documents, the
   Release Notes (30 October 1997, the same notice), the install
   notes, and the locking, untyped-IPC and collocation design notes.
11. **The 1999 `if_tulip.c` waits on PCI**: licence-clean (OSF's
   notice and Matt Thomas's BSD licence), on the old device interface,
   with PowerPC-only pieces (`eieio`, `bcopy_nc`, `flush_dcache`) to
   replace; and DR3's PCI needs a clean `pci.h` first.
12. **MK6.1's tests and libraries: nothing to take.** `ipc_test` and
   `xptest` came with cmk1.1 (0121); `mach_perf`, `librthreads` and
   `libxmm` are DR3's. Of these only `mach_perf` builds today.
13. **CMU's USER programs, in part.** `ps`, `top`, `stacks`,
   `hostinfo` and a dozen more are new to us; `ps` and `top` need
   about seven Mach 3.0 to OSFMK renames each, and a way to link
   OSFMK's libmach into NetBSD programs. The Mach-only tools are here
   already in OSF's versions (`mach_services/cmds`). Not `mach_init`
   (`imports.md`, kind B, the `wait4` row). Exclude the tree's GNU
   readline and `gnu/`.

**Where CMU's USER tools were used.** CMU's companion to its Mach 3
kernels and UX server, hence `table()` and `task_by_pid`, CMU's calls;
Lites reused the tree, and OSF took the Mach-only tools into its
kernel tree.

**`net_device` is for network cards only.** Every driver uses Mach's
`dev_ops`; `net_device` (new in DR3; 1.0b2 has none) adds queues,
filters, statistics and the x-kernel's attachment for network cards,
which supply two tables of routines. Its only drivers are HP700
Myrinet's. Its x-kernel half fails to compile in four places, one of
them a design gap (`net_driver_xkalloc` has no Ethernet header to
read), and has a `case` without a `break` that can leak a buffer.
Left as it is until a driver moves onto it. For `if_ne.c`, the WD
driver's `xkwd` glue (`if_ns8390.c`) is the pattern: about 400 to 450
lines, some 85 per cent copied with names changed, 12 adapted (one
staging buffer for port I/O, the receive path through
`eth_dispatch`), the rest configuration and one `static` in
`eth_common.h`, which its own comment asks for.
