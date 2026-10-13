# What is OSF's, what is Berkeley's, what is borrowed, and what is neither

Every change on this project falls into one of seven kinds. The kinds
are about provenance, not about size or difficulty: they say how far a
given patch stands from the code its three upstreams shipped, so that a
reader can judge each one's authenticity without re-deriving the
argument.

The second half of this file records what was NOT done, and what that
costs, because a fork is shaped as much by its refusals as by its
changes.

## The seven kinds

	A  Upstream's code, corrected to upstream's own intent
	B  Upstream's code, corrected following another tree's line
	C  Upstream's code, changed with no donor line: our judgement
	D  Code transplanted that the receiving tree never had
	E  Code written from a specification OSF published
	F  An upstream's own file, restored from a tree that kept it
	G  Files that are ours, outside any upstream's tree

A is the most authentic and G the least. D, E, F and G are the only
kinds that put new text into a vendor directory.

**E is this project's characteristic kind and does not exist in the
Lite2 tree.** OSF published the Server Writer's Guide, the Server
Library Interfaces, the Kernel Principles, the 21-part Kernel
Interfaces and the MK6.1 release notes under a grant that explicitly
covers derivative works, and the two USENIX papers describe the
mechanisms this project implements. Code written from those is neither
transplanted nor invented. It is implemented from a specification, and
the specification is named with its section.

## Which upstream a file belongs to

Three trees, three idioms, and the boundary matters more here than the
file's directory does.

	osfmk7.3/   OSF's.  OSFMK conventions throughout.
	lites/      CMU's and Helander's, over 4.4BSD-Lite.
	bsd/        Berkeley's, as 4.4BSD-Lite2 and its dev3 corrections.

The seam is `uxkern/`. Code there faces Mach and follows OSFMK's
conventions — `kern_return_t`, `KERN_SUCCESS`, OSF's header shape.
Code in the personality proper follows 4.4BSD's, because that is what
it is and what it must stay mergeable with.

A change that crosses the seam in one file is usually two changes.

## A note on XNU, where the ancestry is not a relationship but an identity

The Lite2 tree records that Berkeley took NetBSD's changes into their
own i386 port in June 1993, so "NetBSD 1.0 writes it this way" is not
independent confirmation in that directory: it may be the same text
arriving by another route.

**This project has a stronger version of the same thing.** XNU's
`osfmk/` is not a relative of `osfmk7.3/`. It *is* OSFMK 7.3, carried
forward. Apple licensed it and imported it; `wsanchez`'s 1998 import
commits are still in the history.

The history says so explicitly. XNU files carry HISTORY blocks naming
OSF's own development streams:

	mk6 CR1120 - Merge mk6pro_shared into cnmk_shared
	Merge up to NMK17.3
	nmklinux_1.0b3_shared into pmk1.1

So three things follow.

**XNU agreeing with `osfmk7.3/` is usually not corroboration.** It is
the same line, later. Where a claim rests on XNU alone, say so.

**XNU's HISTORY blocks are evidence about OSFMK itself.** A file whose
history shows `mk6` or `nmk15` revisions predates Apple, and the
revision dates establish when OSF wrote it. That is a use of XNU no
other tree supports.

**Oldest first, always.** `rel/xnu-124` is the earliest tag and the
closest to our kernel. Moving forward is moving away.

## A -- corrected to upstream's own intent

Evidence inside the tree or in the upstream's own history; no other
system needed.

| what | the upstream's own evidence |
|---|---|
| `i386/AT386/model_dep.c`, `parse_multiboot` bounded by `mods_count` and `MULTIBOOT_MODS` | `i386/multiboot.h:102` "Valid only if MULTIBOOT_MODS is set in flags word above" and `:146` "the physical address of the first of 'mods_count' multiboot_module structures" — the header three files away states both constraints the code broke |
| `kern/bootstrap.c`, OSF's `bootstrap_create()` and `user_bootstrap()` restored under their own names; the GNU Hurd graft's functions of those names behind `#if 0`; `conf/files` stops building `kern/boot_script.c`. *Superseded 13 October 2026:* the graft is deleted, not fenced -- `slp/osfmk-mklinux`'s `63279d3`, verdict in `verdicts.md`, "slp's eighteen commits"; the two names are DR3's own again | The tree kept OSF's two functions as `bootstrap_create_old()` and `user_bootstrap_old()`, and the first still called the second by its original name, `thread_start(..., user_bootstrap)` — the wiring OSF wrote, recoverable from inside the file. No line was taken from another tree |

**The first entry of this kind, and it was nearly filed as something
weaker.** `dev`'s version of the same fix cites the multiboot
specification, an outside document. The tree had the answer, which is
what `audit.md` exists to catch.

**Qualified, one commit later.** "Upstream's own intent" is doing
more work in that row than it should. The header is the authority on
how a multiboot module array may be read, and the fix honours it. But
reading exactly two modules was not OSF being careless: it is the GNU
Hurd bootstrap grafted onto this tree before publication, which
expects `ext2fs.static` and `exec.static` as `mods[0]` and `mods[1]`
and therefore always two. See `docs/deferred.md` §2. **The code
corrected is the grafter's, not OSF's.** The entry stays kind A,
because the evidence is still inside the tree and no other system was
needed, but "upstream" there means the tree as published, not OSF's
design. Where that distinction matters — and on this project it
usually does — the row is not evidence about what OSF intended.

**Three notes on the `bootstrap.c` row.** `set_user_regs()` and
`build_args_and_stack()` were left compiled and uncalled (deleted with
the rest of the graft, 13 October 2026): the
graft's `user_bootstrap()` was their only caller, and whether they are
OSF's or the graft's is not established. *Established 12 October 2026:
the graft's.* Neither is in `slp/osfmk-mklinux`'s first commit,
`0f6f7e6`; both arrive with the Hurd graft in `63279d3`, and
`build_args_and_stack(argv, envp)` follows GNU Mach's
`kern/bootstrap.c` line for line in its comments ("pointers to
environment variables", "Then the strings and string pointers for each
environment variable"), where OSFMK 6.1's takes a variable argument
list. OSFMK 6.1 was read while
checking it and not used: its `bootstrap.c` is a different design, and
its notice differs from 7.3's — 6.1 adds that OSF's name not "be used
in advertising or publicity pertaining to distribution of the software
without specific, written prior permission", a clause 7.3's notice
does not have, so 6.1 lines grafted into a 7.3 file would bring that
condition with them. And `kern/boot_script.c`, no longer built and
since deleted, is GNU Mach's ("Written by Shantanu Goel") and carries no
licence notice of its own; it stayed in the tree as imported until
the graft was deleted, which settles whether it may be carried.

## B -- corrected following another tree's line

The code stays its upstream's; the line taken exists in a tree that may
be copied, and the commit names it with its release or tag.

*(none yet)*

## C -- our judgement, no donor line

Each is an edit no tree makes. They should be small and every one says
so at the site.

| what | why no donor, and the nearest precedent |
|---|---|
| `file_systems/AT386/machdep.mk` and `fs_switch.c`: the bootstrap task's file systems cut to UFS | A build change: `minixfs` (partly GPL) and `ext2fs` (derived from Linux's header) are MkLinux's additions and are no longer compiled for AT386; their sources stay as imported. The nearest precedent is OSF's own `HP700/fs_switch.c`, which has no minix reader. `docs/provenance/precedent.md`, "What OSF booted from" |
| `kern/bootstrap.c`, `do_bootstrap_compat()`: read-only PT_LOAD segments mapped read-only; `regions[]` sized by `NREGIONS`, five, and the bound tested | No tree we can read has this loader with a read-only arm; `dev`'s `11ecb84` is the nearest, and its `p_vaddr > e_entry` test is replaced by the bound it was unknowingly avoiding (verdict 26). The arms beside it were taken to be OSF's; they are not -- the whole ELF `PT_LOAD` parser in `do_bootstrap_compat()` is `slp/osfmk-mklinux`'s `444954a` (2011), which replaced OSF's `loader_info` code (corrected 12 October 2026; `docs/provenance/precedent.md`, "Where the two imports came from") |
| `lites/Makefile` added, `lites/Makeconf` removed: Lites' configure build run by ODE | No tree builds Lites under OSFMK's ODE. Nearest: Lites' own `OSF_ENVIRONMENT` block in `conf/Makerules`, which pointed the same build at an OSF sandbox's tools, and OSF's step 8, `build ... osf1_server`. The file says so in its AI-ONLY NOTE. `docs/provenance/precedent.md`, "How Lites meets ODE" |
| `lites/conf/config.guess`, `lites/conf/move-if-change`, mode 644 to 755 | The import carries every Lites file as 644, and no copy of Lites 1.1.u3 with its modes has been found to restore them from (that would be kind F). These two are run by path by `configure`; both are GNU scripts whose upstreams ship them executable |

**Both are build-level, not source.** No line of Lites' code changes;
`git diff e31a69d:lites HEAD:osfmk/src/lites` shows exactly these four
entries.

## D -- transplanted, never in the receiving tree

*(none yet)*

Expected: nothing large. The point of the design is that the
personality is reused unchanged.

## E -- written from an OSF specification

Expected entries, with the section that specifies each:

| what | specification |
|---|---|
| `uxkern/syscall.c`, the exception-driven entry | Patience §3–4; Wells 1994 Fig. 2 for the five stages |
| `THREAD_STATE_SYSCALL` | Patience §4: "a small subset of registers adequate for handling most system calls for most personalities. For the i386, this register set is currently 5 registers (as opposed to 17 for the full set)" |
| collocation registration | MK6.1 release notes, `mach_subsystem_create` and `mach_port_allocate_subsystem` reference pages |
| the server's main loop conventions | OSF Mach Server Writer's Guide, ch. on Basic IPC-Based Servers |

The grant these rely on is printed in each document:

> Permission to reproduce this document without fee is hereby granted,
> provided that the copyright notice and this permission notice appear
> in all copies, derivative works or modified versions.

**A specification settles what to build, not whose line to write it
as.** After reading Patience §4, the spelling is still chosen by the
order in `precedent.md`.

## F -- an upstream's own file, restored

*(none yet)*

## G -- ours, outside any upstream's tree

*(none yet)*

Expected: the build driver stanza, and whatever tooling the milestones
turn out to need. `RULES.md` 3.3 forbids building it before then.

---

# What was not done

## The emulator is removed rather than fixed

The largest refusal, and it is OSF's own. The emulator could be kept
and hardened; OSF spent two years trying and documented why they
stopped. Signal semantics were never fully correct because the emulator
switches stacks and the server cannot find the real user stack to build
a frame. A program can rewrite its own emulator and then exec a
setuid-root binary that inherits the tampered copy. Loadable syscall
modules cannot work because the vector is fixed when `init` loads.

What it costs: fifteen Lites files need rework, and the system is
slower until collocation lands.

## The personality is not OSF/1

It is 4.4BSD-Lite2. OSF's was 4.3BSD-Reno with Encore's SMP work,
DEC's POSIX.4 and ten years of accretion, and it was never released.

What it costs: Reno covers 56% of OSF/1's function inventory and Lite2
43%. Where OSF/1 and 4.4BSD differ, we are 4.4BSD. The project is a
reconstruction of the architecture, not of the system.

## Alpha, ECOFF and the loader are out of scope

OSF/1 1.3 and later from OSF RI — MK6 and MK7 — use ELF, rtld and
dlopen. The ECOFF `/sbin/loader` path belongs to OSF/1 1.0–1.2 and to
DEC's Alpha line.

What it costs: `exec_with_loader` is not implemented, and OSF/1 Alpha
binaries will not run. Neither was ever a goal.

## AD's distributed machinery is not carried

`fsvr*` (12 files), `pfs*`, `mach_norma_user.c`, HIPPI. Those are the
AD branch — Intel's Paragon line — not the MK single server.

What it costs: nothing for a single-node system. It does mean the one
surviving OSF/1 server source we have read is a worse model than its
size suggests, because much of it is machinery we do not want.

## MkLinux's framework is read but not used

`osfmach3/server/` is OSF's own evolution of the same CMU skeleton
Lites forked, seven years later, and it already does exception-based
syscalls. It is GPL by position inside `mklinux/src/`, and its files
carry a bare OSF copyright with an empty comment block and no grant.

What it costs: the most directly useful 6,800 lines in existence are
reference only. We write from Patience and the Server Writer's Guide
instead, which is slower and is the correct trade.
