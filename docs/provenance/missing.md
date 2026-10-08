# What these trees name and do not contain

The Lite2 tree's version of this file is mechanical: every file in
Berkeley's tree carries a redistribution marker, the Lite releases kept
`%sccs.include.redist.c%` and dropped
`%sccs.include.proprietary.c%`, so "why is this missing" is a lookup
rather than an argument.

**This project has no equivalent mechanism.** Our three trees are
missing things for four different reasons, none of them marked in the
source, and the reasons matter because three of the four mean the thing
exists somewhere and one means it never will.

## The four reasons

### 1. Never released

The thing exists, was written, and was never distributed. No tree has
it and none ever will.

| what | evidence |
|---|---|
| the BSD4.3 UX server, which `startup` replaces | OSFMK 7.3's bootstrap names three servers, `name_server`, `default_pager` and `unix startup -s`. `name_server` and `default_pager` build from the tree; `unix` does not. `test_mk7.3`'s `docs/lites-survey.md`: it "was always distributed separately because it was licence-encumbered, and is not present in either OSFMK 7.3 or 6.1" |
| OSF/1 MK 5/6/7 single server source | Wikipedia and the MkLinux paper: the OSF/1 server was "encumbered by proprietary Unix licensing" while the microkernel "remained freely available". OSF RI wrote MkLinux's Linux server *because* of this — "we decided to produce an unencumbered UNIX-like server on top of OSF MK" |
| OSF/1 1.3 Engineering Release specification | named in Apple's Kernel Programming Guide bibliography (RI, May 1993); no public copy found |
| AD 2 design papers (Bryant 1995; Patience & Rabii 1994) | cited in the MK7.3a release notes; OSF RI internal reports, not in any digital library |
| Paragon R1.4 source tape | the product existed (its User's Guide and release notes are on bitsavers); the tape was not captured. cd163 is R1.4 *System Software*, binary |

The first row is the direct ancestor of the slot this project fills.
The kernel has always expected a `startup` and has never shipped one.

**Consequence for the project:** these are the holes `docs/imports.md`
kind E exists to fill. Written from specification, not restored.

### 2. Released but encumbered

The thing exists and can be read, and its code may not be used.

| what | terms |
|---|---|
| DEC OSF/1 (`calmsacibis995/osf1-10-src`) | "All Rights Reserved. Unpublished rights reserved... proprietary to and embodies the confidential technology of Digital Equipment Corporation", on 2,051 of 2,052 files scanned |
| Paragon OSF/1 AD, cd155 (`nmartin0/undoc_osfmk`) | Intel confidential, and the release letter requires three source licences: Intel, Open Software Foundation, and Unix Systems Laboratory |
| MkLinux `osfmach3/` | GPLv2 by position inside `mklinux/src/`; files carry a bare OSF copyright with an empty comment block and no grant |
| AdvFS | GPLv2, deliberately, for Linux-kernel compatibility |

**Consequence:** structural facts from these are findings and may be
recorded. Their code is not a donor. See `precedent.md`.

Note the asymmetry worth not forgetting: the identical OSF vintage
carries the full MIT/X11 grant where it sits under `osfmk/` and no
grant at all where it sits under `mklinux/src/`. That suggests the
omission is about placement rather than intent. Intent is not a
licence.

### 3. Documented but not implemented

The interface is specified and the implementation is absent from every
tree we may use.

| what | where it is specified |
|---|---|
| `THREAD_STATE_SYSCALL` | Patience §4. OSFMK 7.3's `mach/i386/thread_status.h` has flavours 1, 2, 3, 5, 8 and no syscall flavour |
| a collocatable server | MK6.1 release notes document `mach_subsystem_create` and say plainly: "A server must conform to a small set of rules if it is to collocate itself in the kernel's address space. **This release does not include a collocatable server.**" |

**Consequence:** the mechanism is published, the user of it is not.
That sentence from the release notes is the whole problem of this
project in one line.

### 4. Undocumented by vendor choice

The thing exists in a shipped system and its interface was deliberately
not published.

| what | evidence |
|---|---|
| `getsysinfo` operations marked "for internal use only" | the Tru64 `getsysinfo(2)` reference page lists them by that phrase and gives no structure or semantics |
| `sysinfo` commands beyond 9 | Linux's own `arch/alpha/kernel/osf_sys.c` comments that Digital UNIX "has a few unpublished interfaces here" and returns `EINVAL` |

**Consequence:** out of scope. Neither is on the path to a running
server, and both are recorded so the absence is not mistaken for an
oversight.

## What is absent from Lites and present in OSF's server

Measured by diffing `lites/server/serv/` against the `uxkern/` of the
one OSF/1 server source we have read. 31 filenames are shared; these
are in OSF's and not in Lites':

| file | what it is | our disposition |
|---|---|---|
| `syscall.c` | the exception-driven entry point | **write it.** Lites has `ux_syscall.c`, emulator-RPC-driven. The split between `syscall.c` and `syscall_subr.c` is the tell |
| `credentials.c`, `cred.defs`, `credcache.defs` | credential handling over Mach | no document we have found covers this. Design question, not a transplant |
| `proc_to_port.c`, `port_hash.c` | process-to-port mapping | Lites has `proc_to_task.c` only |
| `const_region.h`, `vm_unix.c` | | unexamined |
| `fsvr*` (12 files), `pfs*`, `mach_norma_user.c`, `raw_hippi.c`, `hippi_io.c` | AD's distributed machinery | **not wanted.** AD branch, not MK |
| `emul_call.defs`, `emul_call_reply.defs`, `emul_user.c` | AD's emulator RPC | **not wanted.** Exactly what MK 5.0 removed |

The last two rows are why the cd155 server is a worse model than its
size suggests: a large fraction of it is machinery this project does
not want.

## Proving an absence

A found donor proves itself: the line is shown. An absence does not.

No proposal may state that no donor exists unless all six trees have
been searched — `osfmk7.3/`, `lites/`, `bsd/`, OSFMK 6.1, Rhapsody,
XNU from `rel/xnu-124` forward — and the commit message names them.

The Lite2 tree records a wrong absence claim made after searching three
trees of eleven. Six is a small enough number that there is no excuse.
