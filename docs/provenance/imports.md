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

The seam is Lites' `server/serv/`. Code there faces Mach and follows OSFMK's
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
`0f6f7e6`; both arrive with the Hurd graft in `63279d3`, where `build_args_and_stack(argv, envp)` takes an environment ("pointers to environment variables", "Then the strings and string pointers for each environment variable"), and where OSFMK 6.1's takes a variable argument
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

| what | the line, and its tree |
|---|---|
| `i386/i386_lock.s`, `i_bit_set` and `i_bit_clear`: `bts %dl` and `btr %dl` become `%edx` | `slp/osfmk-mklinux` `18c9d31`. Neither instruction has a byte-register form and no current GNU as accepts one; the bit number is all of `S_ARG0`, which each routine loads into `%edx` first. The rest of that commit is reverted (`verdicts.md`, "slp's eighteen commits"), so this is the one line kept from it outside `start.s`, and the site says so |
| `conf/AT386/config.devices`: `pci` 2 to 0; `ncr810`, `tu` and `fta` commented out | `slp/osfmk-mklinux` `0d8ebab`, narrowed: the six sources these name are not in DR3 or in the MkLinux CVS tree, so OSF's configuration cannot be built from what OSF published. Configuration, not code; `conf/AT386/files` stays DR3's. The site says so |
| `lites/conf/gensym.awk`: each `asm` string begins `"\n"` instead of a newline | `test_mk7.3` `dev`'s `tools/lites/lites-osfmk73.patch`. A string literal may not span lines since GCC 3.3; the assembler sees the same text |
| `lites/conf/newvers.sh`: `printf "%s\n"` for five `echo`s | the same patch. Debian's `/bin/sh` has a System V `echo`, which turns `\\n` into a newline inside `vers.c`'s strings |
| `lites/server/serv/user_copy.c`, `copyinstr()`: the byte copy spelled out | the same patch. GCC 4 dropped the cast-as-lvalue `(*((char *)to)++ = *cp++)`, and `to` is a register variable, so `*(char **)&to` is not open |
| `lites/server/kern/kern_sig.c`, `lites/server/serv/serv_syscalls.c`: `case (integer_t)SIG_DFL:` and its kind, cast to each switch's own type | 4.4BSD-Lite2's `kern/kern_sig.c`, `issignal()`, which casts its labels `(long)SIG_DFL`; 4.4BSD-Lite, Lites' base, did not. Function-pointer constants are not case labels to current GCC |
| `lites/include/sys/exec_file.h`, `ATSYS_NAMES`: `m "bad"` for `m ## "bad"`, and so for each | `test_mk7.3` `dev`'s `tools/lites/lites-osfmk73.patch`. `##` does not paste two string literals into one; adjacent literals concatenate without it |
| `lites/emulator/i386/ecrt0.c`, `lites/emulator/emul_init.c`, under `OSFMACH3`: `mach_init()` called through `_mach_init_routine` | OSFMK 7.3's own: its libmach keeps `mach_init` static and exports the pointer, and its crt0, `libsa_mach/crt0.c`, calls `(*_mach_init_routine)()`. CMU's libmach exported `mach_init` itself |
| `lites/server/serv/xmm_interface.c`, under `OSFMACH3`: `seqnos_memory_object_init` in place of `_notify`, answered with `memory_object_change_attributes`; `_terminate` without the name port; `_change_completed` with OSFMK's arguments; `_discard_request` added | OSFMK 7.3's own: `mach/memory_object.defs` gives each handler's arguments ("skip; was memory_object_notify", "skip; was memory_object_establish" in `mach.defs`), and OSF's `default_pager/dp_memory_object.c` answers `memory_object_init` with `memory_object_change_attributes` and has `discard_request` as a stub. The attributes Lites sets go in the structure and flavor 7.3 keeps for them, `old_memory_object_attr_info`, `OLD_MEMORY_OBJECT_ATTRIBUTE_INFO`. Without these, libmach's `Smem_svr.o` calls handlers with arguments they do not take, and two it does not find |
| `lites/server/kern/kern_exit.c`, `wait4()`: the "major hack for BSD init compatibility", which hides pid 2 from pid 1, not compiled under `OSFMACH3` | OSF's own design: the OSF Research Institute's "Optimizing Mach-based Systems by Server Co-location" (Condict, Mitchell, Reynolds, August 1993), section 5.1: under the bootstrap task "the OSF/1 server runs /sbin/init. There is no need for mach_init as the service server is already running in the bootstrap task" -- this tree's `bootstrap/service.c`, "Program: Service server". The hack hides USER's `mach_init`, which forked and stayed as pid 2 while pid 1 became `/sbin/init`; with no `mach_init`, pid 2 is init's first child, which init must wait for. MkLinux 1.0b2, OSF's other server on OSFMK, ships a `mach_init` that only execs `/sbin/init` in place. Utah's 1.1.u3 binary on Mach4 (the MachUK22 image) shows the same failure with the link: "wait for /bin/sh on /etc/rc failed: No child processes" |
| `lites/server/Makerules`, `lites/emulator/Makerules`, `lites/conf/Makerules`, under `OSF_ENVIRONMENT`: OSFMK 7.3's libraries, `-lcthreads -lmach -lsa_mach -lmach`, for 1995's `-lcthreadsZ -lmachZ -lsa_machZ`; no crt0 file, `-u __start_mach -e __start_mach` | OSF's own: `default_pager`'s link line in this tree, and `makedefs/osf.gcc.mk` line 41, `_ELF_SA_MACH_LDFLAGS_`, for every standalone Mach program. `OSF_ENVIRONMENT` is Lites' authors' switch for an OSF sandbox; this brings it from OSF Grenoble's `nmk18b9` to OSFMK 7.3. Its Grenoble tool paths are left as written and overridden from `lites/makefile`, as site settings |

## C -- our judgement, no donor line

Each is an edit no tree makes. They should be small and every one says
so at the site.

| what | why no donor, and the nearest precedent |
|---|---|
| `file_systems/AT386/machdep.mk` and `fs_switch.c`: the bootstrap task's file systems cut to UFS | A build change: `minixfs` (partly GPL) and `ext2fs` (derived from Linux's header) are MkLinux's additions and are no longer compiled for AT386; their sources stay as imported. The nearest precedent is OSF's own `HP700/fs_switch.c`, which has no minix reader. `docs/provenance/precedent.md`, "What OSF booted from" |
| `kern/bootstrap.c`, `do_bootstrap_compat()`: read-only PT_LOAD segments mapped read-only; `regions[]` sized by `NREGIONS`, five, and the bound tested | No tree we can read has this loader with a read-only arm; `dev`'s `11ecb84` is the nearest, and its `p_vaddr > e_entry` test is replaced by the bound it was unknowingly avoiding (verdict 26). The arms beside it were taken to be OSF's; they are not -- the whole ELF `PT_LOAD` parser in `do_bootstrap_compat()` is `slp/osfmk-mklinux`'s `444954a` (2011), which replaced OSF's `loader_info` code (corrected 12 October 2026; `docs/provenance/precedent.md`, "Where the two imports came from") |
| `lites/makefile` replaced, `lites/Makeconf` removed: Lites' configure build run by ODE | No tree builds Lites under OSFMK's ODE. Utah's `makefile`, which drove the same configure build with Utah's paths, gives the name and the role; CMU's ODE `Makefile`s stay as shipped. It turns on `OSF_ENVIRONMENT` with this sandbox's tools, and runs configure with Kernighan's awk, the one `server/kern/vnode_if.sh` was written for: mawk rejects the script for a function it calls but never defines. Nearest: Lites' own `OSF_ENVIRONMENT` block in `conf/Makerules`, which pointed the same build at an OSF sandbox's tools, and OSF's step 8, `build ... osf1_server`. The file says so in its AI-ONLY NOTE. `docs/provenance/precedent.md`, "How Lites meets ODE" |
| `lites/server/Makerules`: `bsd_server.o: bsd_1_server.c`, was `bsd_server.c: bsd_1_server.c` | No tree has it. `serv/bsd_server.c` is a wrapper that `#include`s MIG's `bsd_1_server.c`; written on the source, the rule makes GNU make remake a VPATH file and look for it in the object directory. The site says so |
| `lites/server/serv/device_reply_hdlr.c`, under `OSFMACH3`: `mig_reply_error_t` for `mig_reply_header_t` | No tree has it. CMU's MIG name for the reply with a return code; OSFMK's `mach/mig_errors.h` names the same thing `mig_reply_error_t` and has no other |
| `lites/server/serv/ux_server_loop.c`, under `OSFMACH3`: Lites' `cthread_mach_msg` stub renamed for the file | No tree has it. The stub's own comment says the call was "missing from cthreads"; OSFMK 7.3's cthreads has it, with a struct argument, and the two declarations collide. Lites keeps its stub; OSFMK's receive-thread accounting is in `docs/deferred.md` |
| `lites/server/kern/subr_prf.c`, under `OSFMACH3`: `vprintf`, `printf`'s body for a `va_list` | No tree has it. OSFMK 7.3's libmach writes `fprintf_stderr` through `vprintf` unless the program sets `vprintf_stderr_func`, and libcthreads calls it; with no `vprintf` of the server's own, libsa_mach's `printf.o` is linked for it, bringing a second `sprintf` and, through `usleep`, a second `sleep`. Lites already pairs `vsprintf` with `sprintf` |
| `lites/server/serv/vn_pager_misc.c`, `vnode_pager_uncache()`: `silent_overwrite` and `advisory_pageout` for `write_completions` | No tree has it. OSFMK 7.3's `memory_object_behave_info` has the two in the one's place; all three off |
| `mach_services/lib/migcom/AT386/machdep.mk` and `mach_services/cmds/config/AT386/machdep.mk`, new: the two host programs built for AT386 on a Linux host | Both `Makefile`s include `${TARGET_MACHINE}/machdep.mk` when it exists, and OSF wrote one only for `POWERMAC`, the target MkLinux built on Linux. Ours follow those: `migcom`'s adds `-I.`, as `POWERMAC`'s does, because `yacc` writes `parser.c` in the object directory and it includes `"lexxer.h"` from the source directory; `config`'s names the host's `yacc` as `POWERMAC`'s names its `lex` -- `byacc`, because `gram.y` writes actions in the old `= { }` form, which bison rejects -- and empties `LIBS`, because `-lln` was OSF/1's lex library and `parse.l` supplies its own `yywrap` |
| `stand/AT386/boot/lib`, `primary` and `secondary` `Makefile`s: `-mtune=i386` for `-m386` | No tree we can read builds OSF's AT386 boot with a later GCC. GCC's own manuals are the line: in 2.7 `-m386` and `-m486` choose whether code is optimized for a 386 or a 486, and code for either runs on both; from 2.8 they are synonyms for `-mcpu=i386` and `-mcpu=i486`; 3.4 deprecated `-mcpu` for `-mtune`. The flag tunes and never limited the instruction set, so `-march=i386` would ask for more than OSF did |
| `lites/server/serv/ux_exception.c`, `serv/serv_syscalls.c`, `i386/serv_machdep.c`, `kern/init_main.c`, `emulator/i386/e_signal.c`, `include/sys/ushared.h`, under `OSFMACH3`: the exception port registered `EXCEPTION_STATE_IDENTITY`; `catch_exception_raise_state_identity` written, a caught signal sending the faulting thread to the emulator's signal entry in the reply; the entry recorded in the shared page, `us_sigtramp` | No tree has it. OSFMK's `exception()` resumes a thread only by the reply, where Mach 3 and Mach4 resumed one that `do_signal_notify()` had aborted (S-7). The mechanism is OSFMK's own, `exception_raise_state_identity` in its `exc.defs`, and the frame is the emulator's `do_signal_notify()`'s; what joins them is ours. `catch_exception_raise` takes OSFMK's `(exception, code[], codeCnt)`, as `bootstrap/bootstrap.c`'s does (S-3) |
| `mach_services/lib/libcthreads/cthreads.c`, `cthread_idle()`: the idle loop reads the run queue's head through a `volatile` pointer | No tree has it. The loop's other operand, `undepress`, is declared `volatile` in `struct cthread`, and `spin_lock_t` is `volatile int` (`include/i386/cthreads.h`): OSF marked what busy loops read, and this field was missed; today's GCC hoists its load out of the loop, and the system stops (S-9) |
| `i386/interrupt.s`: a spurious IRQ15 sends the master its EOI before returning | NetBSD 1.0's `i386/isa/vector.s`, whose slave entries EOI IRQ2 at the master for every interrupt the slave raises. The slave's own ISR is still read as OSF's code did, and a spurious IRQ7 still gets no EOI (S-10) |
| `i386/AT386/autoconf.c`, the `ne` entry: `neintr` for `at3c501intr` | MkLinux's entry, marked "MITCHEM", was copied from the 3c501's above it and kept that card's handler; `nedriver`'s own is `neintr`, declared in `if_ne_entries.h` |
| `i386/AT386/if_ne.c`, `nefetch()` and `neput()`: the odd last byte indexed as a byte; `nefetch()`'s wait masks the value read | The same file: `neput()`'s wait is `(inb(nec+ds0_isr) & DSIS_RDC)`, the form `nefetch()` lost to a misplaced parenthesis; `up` is an `int *`, so `*(up+len-1)` addressed word `len-1`, not byte `len-1` |
| `lites/emulator/e_bsd.c`, `e_adjtime()`: `olddelta` received into a local and copied out only if not null | The same file: `e_setitimer()` treats its `ovalue` so. 4.4BSD-Lite2's `adjtime()` (`kern/kern_time.c`) writes `olddelta` only `if (uap->olddelta)` (S-5) |
| `lites/server/serv/ux_exception.c`, `ux_exception()`, under `OSFMACH3`: a protection failure in the first page is SIGSEGV | No tree has it. The signal rule is every BSD `trap()`'s; what differs is that OSFMK's `mach_init()` reserves page 0 where NetBSD 1.0's a.out layout (`N_TXTADDR` = `__LDPGSZ`) leaves it unmapped, so the page is treated as the absent page it is to the BSD process (S-4) |

**The two Lites rows above are build-level, not source.** Lites'
source changes are the `lites/` rows of kinds B and C, each with a note
at its site; `git diff 9d1c98a HEAD:osfmk/src/lites` shows them all.

## D -- transplanted, never in the receiving tree

*(none yet)*

Expected: nothing large. The point of the design is that the
personality is reused unchanged.

## E -- written from an OSF specification

Expected entries, with the section that specifies each:

| what | specification |
|---|---|
| The exception-driven system-call entry | Patience §3–4; Wells 1994 Fig. 2 for the five stages |
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

| what | from |
|---|---|
| `mach_services/lib/migcom/` and `mach_services/cmds/config/`, the sources of the two host programs the build runs, replacing `slp/osfmk-mklinux`'s `4df2e7d` (2011) | The MkLinux project's CVS repository, `:pserver:public@cvs.mklinux.org:/MkLinux`, module `osfmk`, as checked out on 24 December 1999 (the maintainer's copy; the server is gone): `migcom` at revision 1.1 of 16 May 1999, its `Makefile` 1.2 of the same day; `config` at 1.1 of 20 June 1999. Each file's notice is "Copyright 1991-1998 by Open Software Foundation, Inc.", with CMU's; `config`'s files add Berkeley's of 1980 and 1990, from BSD's config(8). `diff -r -x CVS` against the checkout shows only the two `AT386/machdep.mk` files of kind C |
| `lites/`, the whole tree, replacing xMach's CVS copy of 2000 | Utah's `lites-1.1.u3.tar.gz`, 30 March 1996, unpacked in place: 892 files, byte for byte, with the tarball's modes. SHA-256 `f81eab46e47786a6db7217aab6d3b29534ff24a8314a889e8e249568e7042c93`, 1,763,728 bytes, the size `ftp.lip6.fr` lists. Its tree is `9d1c98a`, the import the deviation record is read against (`pristine.md`). Two empty directories, `liblites/parisc` and `server/netiso/xebec/test`, are not carried: git has no empty directories. The mode change once made here on `conf/config.guess` and `conf/move-if-change` (kind C) is gone with it: the release has both executable |
| `lites/server/serv/xmm_interface.c`, Utah's post-release copy | `ftp.lip6.fr/pub/mach/mach4/mach/lites/xmm_interface.c`, a mirror of Utah's `flux/lites/`: 17,042 bytes, dated 5 May 1996, SHA-256 `3d6c46a36552f9368a83e08a1d896e6ab0d0bfdbad6879521e76bb632277b759`, fetched by the maintainer. Against the release's 16,946 bytes it changes two functions and nothing else: `seqnos_memory_object_data_write` becomes a stub that panics, and its body moves to `seqnos_memory_object_data_return`, which the release left as the stub. OSFMK 7.3's `memory_object.defs` has `skip; /* was memory_object_data_write */` and its pageout daemon calls `memory_object_data_return` (`vm/vm_pageout.c` line 1381), so with the release's copy every pageout to a Lites-backed file panics. Neither xMach's copy nor any Lites tarball has this version |

**What they replace was older, not adapted.** `4df2e7d`'s files carry
"OSF Research Institute MK6.1 (unencumbered) 1/31/1995" and 6.1's
notice, the one with the advertising clause (kind A, the
`bootstrap.c` notes). With them came their objects, their `yacc`
output, two i386 binaries built from them, and a second copy of the
binaries in `tools/i386/i386_linux/hostbin`. All of that is deleted.
DR3's tarball had neither source, only PowerPC Linux binaries in
`tools/ppc/ppc_linux/hostbin`. `docs/tools/mkhostbin.sh` builds both
programs by DR3's own procedure, the `README` beside those binaries.

**The difference reaches one generated file.** Built with the 1999
programs, every MIG stub in the build is what slp's binaries produced,
byte for byte apart from the comment giving the generation date.
`confdep.h` loses four lines (`TIMEZONE`, `MAXUSERS`, `DST`,
`MAXDSIZ`), and no source includes it. The kernel is the same size,
1,008,208 bytes, and differs in seven bytes: a source path compiled
into an assertion string, and the build time.

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

It is 4.4BSD-Lite2. OSF/1's own was never released.

What it costs: where OSF/1 and 4.4BSD differ, we are 4.4BSD unless
OSF's public documents say otherwise. The project is a reconstruction
of the architecture, not of the system.

## Alpha, ECOFF and the loader are out of scope

OSF/1 1.3 and later from OSF RI — MK6 and MK7 — use ELF, rtld and
dlopen. The ECOFF `/sbin/loader` path belongs to OSF/1 1.0–1.2 and to
DEC's Alpha line.

What it costs: `exec_with_loader` is not implemented, and OSF/1 Alpha
binaries will not run. Neither was ever a goal.

## OSF/1 AD's distributed machinery is not carried

The AD papers describe a distributed OSF/1, its processes and files
spread across nodes. That is the AD branch, not the MK single server.

What it costs: nothing for a single-node system.

## Only public documents are the specification

The OSF/1 server's source was never released, and no unreleased or
GPL source is read for this project. We write from Patience, the
Server Writer's Guide and OSF's other public documents instead.

What it costs: what those documents leave open is decided by the
maintainer, and recorded.
