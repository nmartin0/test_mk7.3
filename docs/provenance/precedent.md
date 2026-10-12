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
`bootstrap/`, `default_pager/` and `mach_services/`, configured by its
own `conf/<MACHINE>/` the way `mach_kernel` is, built by the same ODE
`build` after `mach_services`, and installed in `/mach_servers/` with
`startup` pointing at it. For this project that directory is
`osfmk/src/lites/`.

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
it. The commit that follows this one moves Lites to `osfmk/src/lites/`.

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

**GPL is still excluded.** GNU Mach, the Hurd, Linux and MkLinux's
`mklinux/` personality are not read, and nothing here may be derived
from them.

**Nor is any source that was not released for others to use.** The
OSF/1 server's own source never was. The design comes from OSF's
public documents and from the trees above; where they are silent, the
maintainer decides. Nothing unreleased is read, not even as a
specification.

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
| `syscall.c`, the exception-driven system-call entry, in the server's glue | ours, written from Patience §3–4 against OSFMK's exception interface |
| `THREAD_STATE_SYSCALL` in `osfmk7.3/` | ours; a new i386 thread-state flavour, no donor has it |
| building the server | a `build -here lites` stanza after `mach_services`, OSF's step 8, plus whatever bridges Lites' own configuration to ODE. `build_world` is a vendor file whose header invites exactly this kind of edit |

Our own files carry an SPDX-FileCopyrightText line. Whether they carry
a licence identifier, and which, is the maintainer's decision and is
not asserted here.
