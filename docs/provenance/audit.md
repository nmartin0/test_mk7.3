# Where a donor was cited and this tree had the answer

An audit of every commit that cites a donor, asking one question of
each: **did one of our own three trees already have the thing, and was
it looked at?**

`docs/provenance/precedent.md` puts this tree at the head of the search
order. The Lite2 tree records three occasions where that order was
skipped, each caught only after the work was committed, and the sweep
that followed found five more. This file is the equivalent for this
project.

**Nothing to audit yet.** The method is recorded now so that the first
sweep is run against a rule that already existed, rather than invented
to excuse what it finds.

This file and `docs/provenance/verdicts.md` are two halves. That one
walks `dev`'s 131 commits and decides what this project takes. This one
audits the citations in whatever lands, ours included, and is run again
after each batch.

## What counts as a finding

**The code being wrong is not the test.** In the Lite2 tree's sweep the
code was right in every case, because the donors and the tree agreed.
What was wrong was the citation: a note sending the next reader to
NetBSD for something `hp300` already does. That is a defect in the
record, and the record is most of what this project produces.

Four kinds, the first three inherited and the fourth specific to this
project:

1. **Cited outward, available inward.** A donor named as the source for
   a construct one of `osfmk7.3/`, `lites/` or `bsd/` already has.
2. **Taken wholesale, should have been spliced.** Donor code imported
   where adding to an existing file would have served.
3. **Guard imported without checking the condition.** A donor's extra
   test taken on the assumption that the case occurs here.
4. **XNU cited as corroboration when it is the same text.** See below.

## The fourth kind, which this project has and the Lite2 tree does not

XNU's `osfmk/` is not a relative of `osfmk7.3/`. It is OSFMK 7.3
carried forward — Apple licensed and imported it, and `wsanchez`'s 1998
import commits are still in the history.

So **"XNU agrees" is usually not evidence.** It is the same line,
later. The test is the file's HISTORY block: a revision naming an OSF
stream (`mk6`, `nmk15`, `nmk17.3`, `cnmk_shared`, `is_shared`,
`colo_shared`) predates Apple and is OSFMK's own text, which means our
own tree is the better authority and XNU is the route rather than the
witness.

This is the same shape as the trap the Lite2 tree records for
`sys/i386`, where Berkeley took NetBSD's changes into their own port in
June 1993 and "NetBSD 1.0 writes it this way" stopped being independent
confirmation. There it cost a wrong deletion, because five trees were
counted as agreeing when what they shared was 386BSD's lineage.

**Before citing XNU for anything under `osfmk7.3/`, read the file's
HISTORY block.**

## Method

For each commit that cites a donor:

1. Open the commit. No verdict unopened; resemblance between two
   commits is not evidence about either.
2. Name the construct it takes.
3. Search our own three trees for it, including related structures —
   `mach_services/`, `bootstrap/`, Lites' `server/serv/`, the other
   ports under `bsd/sys`.
4. Search OSFMK 6.1, then Rhapsody, then XNU oldest-first.
5. Where the citation names XNU, check the HISTORY block.
6. Record: cited tree, what this tree had, whether the code is affected.

A sweep tool that searches all six in order and prints a line per tree
whether or not it finds anything is kept outside the tree with the
maintainer's working copies. It is not part of building this system.

## Where the pull toward the wrong donor will be strongest

Predicted from the shape of the project, so that the first sweep knows
where to look hardest. These are hypotheses, not findings.

**The syscall path.** XNU has the most readable descendant of this
code and the strongest pull. But `osfmk7.3/.../i386/i386_rpc.c` is 608
lines of the collocated path already written, and `lites/server/serv/`
has the dispatch. Both are inward.

**Anything about exceptions.** `catch_exception_raise_state` and the
`thread_set_exception_ports` family are in our own tree, in 9 and 19
files. A citation pointing outward for either is a finding.

**The server main loop.** `mach_services/servers/netname/netname.c` is
a complete worked OSF server, 671 lines, sitting in our own tree. The
Server Writer's Guide's single-threaded example is the specification
for the same thing.

**Device access.** The 12-routine interface is `device/device.defs` in
our own tree, and both Lites and MkLinux use it. There is nothing to go
outward for.

**The buffer cache and the vnode pager.** This is where the pull is
toward *Lites* rather than outward, and where it is most likely to be
right — `vfs_bio.c` is 13% similar to stock Lite2, which is to say
Lites rewrote it. Taking Lites' version here is kind D, not a shortcut,
and the citation should say which file and what it replaced.

## Corrections to committed messages

*(none yet)*

When a claim in a commit message turns out to be wrong, it is corrected
here and in the document where it lives, in the same change that
disproves it — `RULES.md` 2.5 and 7.2. The commit itself is not
rewritten; the correction names it.
