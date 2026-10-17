# Reference imports

Other projects' documents and tools, carried in **verbatim**. Nothing
here is edited: corrections and adaptations for this project belong in
our own files under `docs/`, not here.

A reference import mirrors its upstream as that upstream arranges it,
including its `build/` and `tools/` directories. That is what makes
"verbatim" checkable. Anything we write or modify goes in
`docs/tools/`, never here.

The convention is the Lite2 tree's. Its `docs/reference/test_mk7.3/`
holds that project's rules byte-identical to a named commit, with a
README stating what in them does not apply. This directory does the
same, and inherits from both projects above us.

## What to import, and from where

| from | commit | why |
|---|---|---|
| `github.com/nmartin0/test_mk7.3` | `b1190b9` (`pristine`) | its documents and its `build/` and `tools/` as it arranges them; so far the two this tree's documents cite, below |
| `github.com/nmartin0/4.4BSD-Lite2`, `dev3` | pin one | nothing yet — but see below |

Record the commit and `sha1sum` every file, as the Lite2 tree does, so
that "verbatim" is checkable rather than asserted.

**Imported** (17 October 2026), from `nmartin0/test_mk7.3` at
`b1190b9077609f0f607f959403c7aad6a921b342`, the head of its
`pristine` branch, where `dev` has the same two files:

	e0882b8df7b0c3a581a6258d86078be1be6ac6f8  test_mk7.3/RULES.md
	3ace56592c37b5d082e3c2adc24c157b59826409  test_mk7.3/docs/METHODOLOGY.md

`RULES.md`, cited through `docs/` by section ("`RULES.md` 3.3"), is
this file; `docs/method.md` names both.

## Why test_mk7.3's rules apply here more directly than they did there

They were written for **this kernel**: OSF Mach Kernel 7.3 from MkLinux
DR3, for i386, built with ODE on a Linux host and run under QEMU. The
Lite2 tree imported them for their general method and had to warn that
their Mach-specific statements described another system.

Here most of those warnings fall away. `DEBUGGING.md`'s ODE, MIG and
`Buildconf` material is about our `osfmk7.3/`. `build/mksandbox.sh`'s
modern-GCC flags were found against this kernel.

**But not all of them.** Two to watch:

- `tools/vmem.py`'s docstring says the kernel is relocated by
  segmentation with base `0xC0000000`. That is OSFMK's, and it is
  **not** established for the server, which is an ordinary user task
  until collocation lands.
- `tools/vgadump.py` probes `0xa0000` because OSFMK's `kd` console
  writes there. The personality's console path is a different question.

## What the Lite2 tree's own docs are for here

Not imported as rules — they govern that tree, not this one — but they
are the direct ancestor of every file under `docs/`, and three of them
answer questions this project will have:

- `docs/provenance/precedent.md` — the search-order discipline, which
  this project's own version follows closely and departs from on APSL.
- `docs/method.md` §2 — "what kept going wrong", every item of which is
  a failure mode this project can repeat.
- `docs/provenance/conventions.md` — how to handle a tree that holds
  several spellings of the same thing, which is our problem three times
  over.

Cite them as `nmartin0/4.4BSD-Lite2`, `dev3`, with the file. They are a
different project and their findings are about their tree.

## Not to be imported

State documents of either project — their `status.md`, `roadmap.md`,
`deferred.md`, handoffs, current-blocker notes. They record another
system's state and would read as claims about ours.

The rule the Lite2 tree states and this one keeps: **their numbers,
addresses, file paths and build commands describe their tree.** Ours
are measured here or they are not written.
