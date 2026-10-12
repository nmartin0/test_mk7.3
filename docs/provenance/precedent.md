# Precedent, attribution, and what in this tree is ours

How donors are chosen for the server, what has been written here rather
than taken, and corrections to statements already committed. Facts, not
legal advice.

This project has three vendor imports, each with its own lineage, and
the search order differs by which of them a change touches. That is the
first thing to establish before looking anywhere.

## The three trees

	osfmk7.3/   OSF Mach Kernel 7.3, from MkLinux DR3.  Verbatim.
	lites/      Lites, the CMU/HUT 4.4BSD-Lite single server.  Verbatim.
	bsd/        the 4.4BSD-Lite2 personality, from nmartin0/4.4BSD-Lite2.

All three run. That is the premise of the project and the reason the
imports are verbatim: `git diff <import>..HEAD -- <dir>/` is the
complete deviation record for each, and each must stay small enough to
read.

## Where the server lives, and why

The server is a peer of the microkernel, not a part of it. It sits at
the top of the repository beside `osfmk/`, is built by its own
configuration against the kernel's exported headers and libraries, and
installs as `/mach_servers/startup`. Every lineage this project can read
arranges it that way, and CMU wrote the rule down.

| lineage | kernel | server | how the server reaches the kernel | read in |
|---|---|---|---|---|
| CMU Mach 3.0, 1993 | `src/mk` | `src/ux`, a peer collection | shared `export/<context>`, `release/<context>` | Thompson, *Building Mach 3.0* |
| Lites at CMU, 1994 | `src/mk` | `src/lites`, a peer | `--with-release=…/export/…` | Lites `doc/building` |
| OSF/1 MK, OSF | the MK product | OSF/1's own tree, a separate product | the MK release | Wells 1994; Condict et al.; MK6.1 release notes |
| Paragon OSF/1 AD 1.0.4, Intel | `kernels/src/mk` | `servers/svr/src`, a peer | `MKREL=…/release/i386_mach` | cd155 `servers/svr/src/latest/Makeconf` |
| MkLinux DR3, OSF RI, OSFMK 7.3 | `osfmk/` | `mklinux/`, a peer | committed links into `osfmk/export/` | `OSFMK_BUILD.README`; `mklinux/src/export-osfmach3/` |

**CMU, in its own words.** Mary R. Thompson, *Building Mach 3.0*, CMU,
18 June 1993 — `mach3_build.doc` in `github.com/Prajna/mach`:

	src/{buildtools,mk,ux,user,bsdss}/...
	obj/<context>/{buildtools,mk,ux,user,bsdss}/...
	release/<context>/{bin,lib,include,etc,man,special}/...
	export/<context>/{bin,include,lib,special}/...

"Each collection contains one Makeconf now, at top-level." The UX
server is built from `ux/` after `mk` has exported its headers and
libraries, and "both the mach kernel and Unix server use a modified
version of the BSD config program" — `MASTER`, `files`, `template.mk`,
`CONFIG` set in each collection's own `Makeconf`. All of it under OSF's
ODE, whose `odemake` "came from the OSF DCE v 2.1 tape". Distribution
followed the same seam: "There is a different collection for each
source subtree", each with its own licence.

Lites came out of that environment. Its `doc/building` (Helander,
November 1994) builds it at CMU from `/p/mach/mach3/work/src/lites` —
a peer of `mk` and `ux` — with `--with-release` pointing at
`/p/mach/mach3/work/export/…`, and says "in theory you could pick up a
standard MK83a release and build with that."

**The name is CMU's too.** `mach3_setup.doc`: "In Mach 3.0 the names
mach and mach.boot are used for the Mach micro-kernel and vmunix and
startup are used for the Unix-server", and the kernel looks in
`/mach_servers` for "startup which is the program it will start the
user task in". OSF kept it: `osfmk/src/bootstrap/bootstrap.template`
names "the osf1_server (conventionally called "startup")", and the
bootstrap task's built-in default is `unix startup -s`
(`osfmk/src/bootstrap/bootstrap.c:81`).

**OSF, in its own words.** OSF's server was the OS's own system
services, carried in OSF/1's tree rather than the microkernel's:

- "The server code is taken directly from the integrated kernel code
  base with over 95% code reuse." — Wells, OSF RI, 1994 IEEE
  Dual-Use Technologies conference.
- "Over 90% of the OSF/1 server code in the MK version of the system
  is unchanged from the OSF/1 IK, and the code that differs consists
  almost entirely of glue code." — Condict, Bolinger, Mitchell and
  McManus, OSF RI, the collocation paper in OSFMK 6.1's `doc/`.
- "This is a kernel-only release; we are not releasing an unencumbered
  UNIX server at this time." — MK6.1 release notes.
- *Design of the OSF/1 Operating System, Release 1.2* (OSF, Prentice
  Hall, 1993), §1.3 and §1.4: the kernel "consists of two logical
  elements: the core kernel and the system services", and "most of
  what is now contained in the system services will be moved into its
  own, separate address space." The core kernel became the MK product.

**For OSFMK 7.3 itself**, OSF RI's `OSFMK_BUILD.README`: the kernel
tree builds "header files and libraries required for mach tasks (such
as the Linux single server)", then "You can stop now, if you're just
going to rebuild the Linux Server … simply move on to building the
Linux server."

**Runtime and source are different questions.** At runtime the server
is one of the `/mach_servers` programs like `name_server` and
`default_pager`: started by the bootstrap task, asking it for
privileged ports through `bootstrap.defs`. In source it is not one of
them. Those two build with `build -here` inside `osfmk/src`; no
personality server in any lineage does. The Paragon AD server is a
reminder that the two questions separate: its source arrangement
agrees with the table, but its runtime does not transfer, because it
asks for its ports with CMU's raw `msgh_id 999999` handshake, which
OSFMK 7.3's `bootstrap.defs` skips.

**Not yet read.** *MK7.3a Real-Time Microkernel for the OSF/1 OS —
Release Notes*, §2.5 "Source Installation" and §2.5.2 "Building the
Release", would show OSF's own OSF/1 MK layout. Only a preview has
been reachable. Every precedent above agrees; if it does not, this
section changes.

### A correction to `696c173`

That commit moved Lites to `osfmk/src/mach_services/servers/startup/`,
reasoning that the server "would have been built by a "build -here"
line alongside" `name_server` and `default_pager`, and that "a
personality sitting outside that directory cannot be built by
build_world at all."

Both halves are wrong. No lineage builds its personality server inside
the microkernel's source tree, and OSF's own build notes for this
kernel stop `build_world` before the server and build it separately.
The runtime slot was right — `startup` in `/mach_servers` — and the
source location was not. The commit that follows this one moves Lites
back to `lites/`, where `e31a69d` imported it.

The mistake cost something already: `docs/provenance/pristine.md` had
to narrow the deviation-record command to `osfmk/src/mach_kernel`,
because the server "would otherwise dominate the diff". The move back
restores one plain command per tree.

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
| `servers/startup/uxkern/syscall.c` | ours, written from Patience §3–4 against OSFMK's exception interface |
| `THREAD_STATE_SYSCALL` in `osfmk7.3/` | ours; a new i386 thread-state flavour, no donor has it |
| the server's build driver, in `docs/tools/` | ours, following Lites' `doc/building` and CMU's release-tree convention. Not a `build_world` stanza: DR3's `build_world` builds no server, and OSF's notes build it separately |

Our own files carry an SPDX-FileCopyrightText line. Whether they carry
a licence identifier, and which, is the maintainer's decision and is
not asserted here.
