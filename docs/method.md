# Working on this tree

`docs/reference/test_mk7.3/RULES.md` is the general rule set and
`docs/reference/test_mk7.3/docs/METHODOLOGY.md` the general method.
This file is what is specific to building a single server, and it is
mostly a record of what is known to go wrong — some of it inherited
from the two projects this one stands on, some of it specific to the
seam between a Mach kernel and a BSD personality.

The short version. **Run it, then check the lineage agrees.** The code
decides; donors corroborate. That rule comes from the Lite2 tree and
every serious error recorded there came from doing those in the other
order.

---

## 1. The three questions, in order

Every change answers these before it is written:

1. **Is it a hack?** Code whose correctness depends on something not
   guaranteed by the language, the ABI or the hardware specification.
   Old-fashioned, verbose or slow is not a hack. A documented hardware
   workaround is not a hack. The test is "is this guaranteed to work",
   not "is this pretty".

2. **Would splicing have been more canonical?** Could the result be
   reached by adding to a file one of our three trees already has, or
   by combining what two donors agree on and writing it in the idiom of
   the directory it lands in? A wholesale import is the last resort.

3. **Is the existing code sound?** This governs both. Carrying a hack
   forward because it is ours is the wrong trade.

A fourth question is specific to this project and sits beside the
second:

4. **Which side of the seam is this?** Mach-facing code follows OSFMK's
   conventions; personality code follows 4.4BSD's. A change that
   crosses the seam in one file is usually two changes.

---

## 2. What is known to go wrong

These are inherited findings. They cost the two upstream projects real
time and none of them is discoverable from the code.

### Verdicts written before reading

Five batches of the Lite2 tree's audit were marked complete with
commits unopened, grouped by resemblance. Every time this was caught,
the skipped commits held something.

**No commit gets a verdict unopened. Resemblance between two commits is
not evidence about either.**

### Counts written without running them

Fourteen figures in the Lite2 tree's commit messages and documents did
not reproduce. One came from a grep that *was* run and returned numbers
whose arithmetic was visibly impossible.

> **A figure in a note is a measurement. Run it against the tree the
> note will ship in, say what command produced it, and check that the
> arithmetic closes.**

### Citations without a date

Thirteen citations in the Lite2 tree named a tree with no release, or
named a modern tree in a context that reads as a contemporary.

This project has a sharper version of the same trap, because **XNU is
28 branches of one codebase**. "XNU does it this way" is not a
citation. `rel/xnu-124` and `main` are a quarter-century apart and the
code in between was rewritten more than once.

### Instrument errors reported as findings

Three times in the Lite2 tree a broken check was reported as a defect
in the thing under test: `lorder` "returned 0" when it was not on
`PATH`; `delay(10000)` "crashed the guest" when QEMU had hit its
timeout; "the GDT register reads the wrong base" when the grep had
matched SeaBIOS's SMM dump.

**When a check reports something surprising, suspect the check first.**

### The sandbox can kill its own tool call

`pkill -f qemu-system-i386` matches the shell running the command.
Match on the process name, remembering Linux truncates it to fifteen
characters: `pkill -x qemu-system-i38`. `RULES.md` §8 has the rest.

---

## 3. The precedent method

`docs/provenance/precedent.md` has the rule. In practice, for this
tree:

1. **Our own three trees, and related structures inside them.** OSFMK's
   `mach_services/servers/netname.c` is a complete worked OSF server.
   Its `bootstrap/bootstrap.template` already names `startup` and
   documents its arguments. Lites' `server/serv/` and OSF's `uxkern/`
   share 31 filenames and the same MIG subsystem number, 101000. Look
   in all of it first.
2. **OSFMK 6.1**, kernel and the 1,254 pages in `doc/`.
3. **Rhapsody.**
4. **XNU, oldest tag first.**
5. **Ours.**

### The papers are specifications, not donors

Patience's *Redirecting System Calls in Mach 3.0* (USENIX Mach III,
1993, pp. 57–74) and Lepreau et al.'s *In-Kernel Servers on Mach 3.0*
(same proceedings) describe the two mechanisms this project
implements. Implementing from them is legitimate and intended.

But **a paper settles what to build, not whose line to write.** After
reading Patience §4, the question of how to spell the dispatch in this
tree's idiom is still open and still answered by the search order.

### Read the donor before designing anything

The rule the Lite2 tree records as "broken more often than any other
rule in this file". Its worked case: three approaches to moving
`KERNBASE` were designed and argued before anyone read what NetBSD
1.0's `locore.s` actually does, which was smaller than all three.

**Before designing an approach, read the donor's version of the same
file end to end.** Not grep for the symbol — read the file.

This project's standing instance: before designing anything about the
syscall path, read `lites/server/serv/ux_syscall.c` and
`osfmk7.3/.../i386/i386_rpc.c` in full. The second is 608 lines and
already contains the collocated call path.

### Source from this tree first, and check the guard is needed

Two rules that failed together in the Lite2 tree often enough to belong
together.

**First**, the order above was skipped three times running and each
time one of that tree's own ports already had the answer.

**Second**, a donor's version often carries a guard the tree's own
version lacks. Before taking it, **grep this tree for the condition
before importing the guard against it.** A donor's extra line is
evidence that the condition occurs in *their* tree.

### The 1993 merge has an analogue here

In `sys/i386`, Berkeley took NetBSD's changes into their own port in
June 1993, so "NetBSD 1.0 writes it this way" is not independent
confirmation in that directory.

**This project has the same problem, in both directions.** XNU's
`osfmk/` *is* OSFMK 7.3 carried forward — the same text, not a
relative. And its revision history says so: HISTORY blocks name the OSF
streams (`mk6`, `nmk15`, `cnmk_shared`, `is_shared`, `colo_shared`).

So XNU agreeing with `osfmk7.3/` is usually not corroboration; it is
the same line, later. Where a claim rests on XNU alone, say so, and
check whether the file's HISTORY shows it predates Apple.

### The donors' apparently redundant details are usually load-bearing

Three times in one Lite2 session a detail that looked like belt and
braces turned out to be the thing that works. **Ask why they would have
written that.** It has paid better than reasoning about what ought to
work.

The standing example here is Patience's own: he tried plain
`catch_exception_raise` first and rejected it for three reasons that
are not obvious until stated. Anyone designing this path from first
principles would have built the version he discarded.

---

## 4. The checks that actually find things

### Did the code reach the binary?

A false `#if` deletes code with no error and no warning. `nm` is how
you tell — `T` is defined here, `U` is referenced and needs linking.

OSFMK's own trap is worse than a plain `#if`: `osfmk7.3/.../i386/asm.h`
carries both a.out and ELF conventions and selects on
`__NO_UNDERSCORES__`, and `ALIGN` is defined inside `#ifdef ASSEMBLER`.
Both looked like source bugs and were configuration.

### Does the arithmetic close?

Before publishing any count, check the numbers are consistent with each
other.

### Did the server actually get the message?

The standing instrument for this project. A syscall that does not
arrive looks identical to one that arrives and is mishandled. Before
concluding the exception path is wrong, confirm the port is registered
and an adjacent exception does arrive.

`RULES.md` 1.5: **trust positives, distrust negatives.** "The server
received nothing" is weak evidence until the instrument is proved.

### Does the kernel get further?

	grep -m1 'v=' /tmp/q.log       # the first exception
	grep -A14 'v=0d' /tmp/q.log    # full register state

`v=0d e=0010` is a general protection fault against selector 0x10;
`v=0e` with `CR2` is a page fault and `CR2` is the address.

### Is the hardware doing what you think?

For anything timing- or hardware-shaped, a stub under QEMU settles it
in minutes where reasoning does not.

---

## 5. Delivery

**Check the patch's commit count before handing it over.**

	git format-patch <base>..HEAD --stdout > p.patch
	grep -c '^From ' p.patch      # must equal:
	git rev-list --count <base>..HEAD

The maintainer applies with `git am`, which re-commits with new SHAs.
Local originals then look unmerged and land in the next patch as
duplicates. This happened on six consecutive deliveries of the Lite2
tree.

**Dry-run on a fresh clone, and build in it.** A patch that applies and
does not build is worse than no patch.

---

## 6. Commit messages

The ones that have held up say four things:

- **what was wrong**, concretely, with file and line
- **what was measured**, with the command or the numbers
- **what was declined and why** — the alternative donor, the larger
  change, and the options this one was chosen from. A reader should be
  able to see the choice, not just the result
- **what this does NOT do**, which is usually the most useful part

The last matters most here because almost nothing has run. "It compiles
and the symbols resolve" is not "it works", and the difference belongs
in the message rather than being discovered later.

When a commit corrects an earlier one, say which and say what the
earlier one got right.

The documents and the history divide the work between them.
`docs/provenance/verdicts.md` records what happened to each of `dev`'s
131 commits, including the ones that take nothing. The history records
why each line that landed is the way it is. Neither substitutes for the
other.

---

## 6a. What the first build taught

Three instrument errors, all mine, all in one session.  They are listed
because the pattern is the same each time and it is the pattern this
file exists to catch.

**`grep -m1` on a conditional config file.**  Checking `Buildconf` for
`ELF_CC_EXEC_PREFIX` returned the general line at 118 and appeared to
contradict the claim.  The i386-on-Linux override is at 152.  A config
file written as a cascade cannot be checked with a first-match grep.

**Reading `rc=0` as success.**  A test of whether `object_base` could
be redirected returned 0 and produced a two-line log saying "No such
directory: ../obj/at386".  The build never started.  The headers
counted afterwards were left from an earlier run.

**Counting a diagnostic by its quoted source text.**  `grep -c
'IN_KERNEL definition'` reported 8 after the fix, because
`-Wtraditional` warns *about* the `#error` line and quotes it.  The
directive was dead.  Grep for the diagnostic class, not for text that
appears in the source.

And one destructive error: a scratch `rm -rf "$REPO/osfmk/export"`
deleted 240 vendor files.  `docs/provenance/pristine.md` records it.

## 7. Standing facts worth not rediscovering

- **Patience's mechanisms are already in OSFMK 7.3.** `EXC_SYSCALL`
  = 7 and `EXC_MACH_SYSCALL` = 8 in `mach/exception.h`;
  `EXCEPTION_DEFAULT` 1, `EXCEPTION_STATE` 2, `EXCEPTION_STATE_IDENTITY`
  3; `catch_exception_raise_state` in 9 files;
  `thread_set_exception_ports` in 19; `mach_msg_overwrite` in 20;
  `vm_read_overwrite` in 14; `vm_remap` in 20. Measured by `grep -rl`
  against `osfmk7.3/osfmk`.
- **`THREAD_STATE_SYSCALL` is not.** i386 has flavours 1, 2, 3, 5 and
  8 only (`mach/i386/thread_status.h`). It is the one kernel addition
  the design needs and no donor has it.
- **The collocation API is in 7.3**: `mach_subsystem_create`,
  `mach_port_allocate_subsystem`, `rpc_subsystem`, `routine_descriptor`,
  `mig_stub_routine`, `thread_activation_create`, 5–17 files each. The
  i386 kernel side is `i386/i386_rpc.c` (608 lines) and
  `i386/machine_rpc.h` (225), where `call_exc_serv()` does the
  side-call to a collocated server.
- **OSFMK 7.3 has no `mach_init` server.** Bootstrapping is
  `bootstrap.conf`, read by the bootstrap task, three lines by default:
  `name_server`, `default_pager`, `startup`. `mach_init.c` in `libmach`
  is the per-task library initialiser, a different thing with the same
  name.
- **`bootstrap.template` names our target.** It calls the OSF/1 server
  "conventionally called `startup`" and documents its arguments: `-s`
  single-user, `-a` prompt for root device, and a root filesystem name.
- **Path resolution in `bootstrap.conf`**: a relative path is taken
  from the directory holding the file; a path starting with `/` but not
  `/dev/` from `/dev/boot_device/`; `/dev/` names a Mach device.
- **The device interface is 12 routines**, subsystem 2800, in
  `device/device.defs`. The async request/reply pairs are separate
  (`device_request.defs`, `device_reply.defs`), and there is a trap
  fast path at −94 to −99 with `io_done_queue_wait`. OSF's own server
  used the synchronous path.
- **Lites and OSF's `uxkern/` share 31 filenames** and both declare
  `subsystem bsd_1 101000` — OSF with 75 routines, Lites with 68.
- **Lites is 79% unchanged from 4.4BSD-Lite2** across 88 shared files.
  The serverisation is concentrated in `vfs_bio.c` (13% similar),
  `tty_subr.c` (14%), `kern_xxx.c` (30%), `init_main.c` (35%),
  `uipc_syscalls.c` (40%), `ufs_ihash.c` (39%), `nfs_serv.c` (42%).
- **Two traps Utah recorded and we have not yet checked for.** The
  OSF/1 server put its `uthread` on the service thread's stack at a
  constant offset, assuming a fixed-size C-threads stack; and BSD
  service routines do their own `copyin`/`copyout`, so generated stubs
  that also copy move every argument twice. Both apply to Lites by
  inheritance and neither has been measured here.
