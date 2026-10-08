# The verdict record: walking dev's 131 commits

`dev3` starts from `f8fee21`, the verbatim vendor import. Everything
`test_mk7.3`'s `dev` branch built on top of it — 131 commits — is
reviewed here, one at a time, and adopted only on its merits.

**This file records every verdict, including the ones that take
nothing.** A list of what was declined, and why, is worth as much as
the code that was kept: without it the next person cannot tell whether
something is absent because it was rejected or because nobody looked.

The commit messages carry the same reasoning in the other direction.
`docs/method.md` §6: a commit says what was wrong, what was measured,
**what was declined and why — the alternative donor, the larger change,
the options this one was chosen from** — and what it does NOT do.

So the two records answer different questions. This file answers "what
happened to dev's commit N". The history answers "why is this line the
way it is".

## The four verdicts

**adopt** — canonical, idiomatic, minimal, and the reasoning holds.
Cherry-picked as it stands, or with the message extended to name the
options it was chosen from where the original did not.

**adopt reworked** — the change is right and the shape is not.
Typically: a source change where configuration would serve
(`docs/provenance/pristine.md` has the order), an invention where a
donor exists, or a citation naming the wrong tree. The commit message
says what the original did and why this differs.

**defer** — correct, but premature for our ordering. Named here with
the tier it belongs to in `docs/roadmap.md`, so it is picked up rather
than lost.

**decline** — not taken. The reason is recorded in full. A decline is
not a judgement on the original work: `dev` was reaching a booting
kernel, and this project's constraints are different.

## The tests a commit must pass to be adopted

From `RULES.md`, and each one has cost somebody time:

- **5.5** one logical change. Two unrelated fixes are two commits.
- **5.6** builds and passes on its own, so bisect works and it can be
  reverted alone.
- **5.9** the measurements are in the message.
- **3.8** it says what it does NOT do.
- **3.1** any deviation from the vendor import is justified by the
  message.
- **1.2** every number is a measurement from a run, not an estimate.

And two from `docs/provenance/precedent.md`:

- every citation names a tree **and** a release, tag or commit.
- nothing outside the six trees on disk is precedent. If an idea came
  from reasoning rather than a donor, it is labelled as this project's
  own invention.

## Method

`docs/method.md` §2: **no commit gets a verdict unopened.** Resemblance
between two commits is not evidence about either. The Lite2 tree marked
five batches complete with commits unopened, grouped by resemblance,
and every time it was caught the skipped commits held something — a
false citation, an unadopted convention, a second `NOPIC`.

So, per commit:

1. Open it. Read the diff and the message in full.
2. Name what it changes and which of the four levels in
   `pristine.md` it sits at.
3. Check the message's claims against the tree — counts re-run, cited
   files opened.
4. Search our own trees for the construct before accepting an outward
   citation (`docs/provenance/audit.md`).
5. Record the verdict here, with the reasoning, before moving on.

Batches are agreed with the maintainer before each run, and the
verdicts are reported before anything is cherry-picked.

## Verdicts

Nothing has been walked yet. The table fills in `dev` order, oldest
first.

| # | commit | subject | verdict | reasoning |
|---|---|---|---|---|

## Standing notes for the walk

Recorded in advance, so the first batch does not rediscover them.

**`dev` and `dev3` have different goals.** `dev` was reaching a booting
microkernel with Lites on top, and took the shortest honest path.
This project is building a single server and keeps the imports
pristine. A commit that was right for `dev` can be wrong here without
either being a mistake.

**`dev` carries the kernel twice.** `osfmk/` from the root commit
`4b29812` and `osfmk7.3/osfmk` from `f8fee21`, byte-identical. `dev3`
keeps one. A commit touching the redundant copy needs its paths
rewritten, and that rewriting is "adopt reworked", not "adopt".

**`dev` has no `lites/` tree.** Its Lites work is tooling —
`tools/lites/build-lites.sh`, `lites-compat.h`, `lites-osfmk73.patch`
(1,414 lines across 29 files) and `mig-shim.sh` — applied against an
external checkout. We carry Lites as a vendor import instead, so those
commits become changes to `lites/` or to `docs/tools/`, never a patch
file applied at build time.

**The 29 files that patch touches overlap the ones Tier 1 rewrites.**
`server/serv/user_copy.c`, `serv_syscalls.c`, `server_init.c`,
`server_exec.c`, `xmm_interface.c` and `vn_pager_misc.c` are all in
both sets. Adopting the patch's changes to those files and then
rewriting them is work done twice; the walk should notice when it
reaches them and say so rather than discovering it later.

**`dev`'s single vendor-import change is a build change.** `fb0c4d5`,
`-fno-strict-aliasing` and `-fno-pic`. That is level 2 of four in
`pristine.md` and the shape we want. Whether it is still needed with a
pristine `lites/` is a question the walk must ask rather than assume.
