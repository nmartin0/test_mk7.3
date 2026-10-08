# Where the three trees contradict each other, and which spelling to follow

The Lite2 tree's version of this file records contradictions *within*
one codebase written over fifteen years. This project has that problem
and a second one on top of it: three codebases, written by three
groups, meeting in one repository.

They are not competing conventions to choose between. Each is right for
its own code, and the rule is to know which code you are in.

## The seam

	osfmk7.3/          OSF's conventions
	lites/server/serv  the seam: OSF's conventions, Mach-facing
	lites/server/*     Berkeley's conventions, personality
	bsd/               Berkeley's conventions

`uxkern/` — which is what `server/serv` becomes — is where the idiom
changes. Both OSF and Lites put their Mach glue there under that name,
and both declare MIG `subsystem bsd_1 101000`.

A change that crosses the seam in one file is usually two changes.

## Error returns

OSFMK returns `kern_return_t` and compares against `KERN_SUCCESS`.
4.4BSD returns `int` and compares against zero, with `errno` values on
failure.

The glue is where they meet, and it is the glue's job to translate.
A `kern_return_t` must not propagate into the personality and an
`errno` must not propagate into a Mach call. Where a file does both,
the translation is explicit and at the boundary, not scattered.

This is not a style preference. Patience's whole argument for removing
the emulator is that the server's internal environment should become
"indistinguishable from the integrated OSF/1 system", so that the
personality can be reused unchanged. Letting Mach return codes leak
inward is the failure mode that makes that untrue.

## Locking, and why Lites' spelling is historical rather than chosen

Lites writes `spl0()`, `splnet()`, `splbio()`, `spltty()`, `splhigh()`
and `splx()`, all resolving to its own `spl_n(level)` in
`include/sys/synch.h`, alongside `mutex_lock` and `condition_wait` from
C threads. 603, 106 and 78 occurrences respectively, across roughly 96
files.

That is not Lites' invention. It is CMU's, from the UX server, and
Golub et al. (1990) state it plainly:

> Internal synchronization and process-switching within the Unix Server
> (e.g., sleep, wakeup, spl) are implemented by using the C Threads
> package's mutex, condition_wait and condition_signal functions.

OSF/1 did it differently: Mach's `simple_lock` family alongside `spl`,
with `kern/parallel.h` included by BSD files, Mach files, and files in
vm, ufs and net alike. 148 `simple_lock` calls across 25 BSD files.

**So the two spellings are two designs, not two styles.** Lites'
emulates BSD's interrupt-priority discipline in user space; OSF/1's is
the kernel's own. Converting is Tier 1 item 3 in the roadmap, and until
it lands, Lites' spelling is correct in Lites' files and must not be
half-converted.

## Where OSFMK contradicts itself

OSFMK 7.3 is itself a merge of several lineages — CMU Mach 3.0, Utah's
Mach 4, NORMA, and OSF's own streams — and carries their spellings
unevenly. The revision histories name the streams: `mk6`, `nmk15`,
`nmk17.3`, `cnmk_shared`, `is_shared`, `colo_shared`.

Two consequences.

**A file's HISTORY block dates its idiom.** A file last touched on
`nmk15` is CMU-era NORMA Mach; one touched on `mk6` is OSF's own. They
will not read alike and neither is wrong.

**Underscore conventions are configured, not fixed.**
`i386/asm.h` carries both a.out and ELF conventions and selects on
`__NO_UNDERSCORES__`, which `osc/Buildconf` sets for the i386-on-Linux
case. `ALIGN` is defined inside `#ifdef ASSEMBLER`, and the `.S.o` rule
in `conf/AT386/template.mk` passes `-DASSEMBLER`.

Both of these looked like source bugs to the test_mk7.3 project and
were configuration. Before changing a vendor file, establish that the
build is not simply misconfigured.

## File headers

OSFMK files carry, in order: the `@OSF_COPYRIGHT@` marker or an
expanded OSF notice, then CMU's and any other upstream's, then a
HISTORY block.

4.4BSD files carry the Regents notice and an SCCS marker.

XNU files, which we may take from, stack more: Apple under the APSL,
then OSF, then CMU, then the Regents, and sometimes USL.

**Never remove or reorder an existing notice.** Taking a line from a
file means taking its notice block with it. SPDX identifiers are added
alongside, never in place of, and the expression names every licence
with `AND`.

## Prototypes

4.4BSD-Lite2 writes `__P((...))` in headers for K&R compatibility, and
unevenly in drivers — `pmax/dev/scc.c` has it, `sparc`'s
`rcons_kern.c` writes prototypes plainly, and none of the twelve
drivers in `i386/isa` uses it.

OSFMK writes plain ANSI prototypes.

So a correction to a BSD driver that introduced `__P` would be
importing a style that directory does not have; and a correction to an
OSFMK file that introduced it would be importing one the tree does not
have at all.

Follow the file.
