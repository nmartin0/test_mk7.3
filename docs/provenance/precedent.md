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
| `build_world` stanza for the server | derived from MkLinux's, which is OSF's own |

Our own files carry an SPDX-FileCopyrightText line. Whether they carry
a licence identifier, and which, is the maintainer's decision and is
not asserted here.
