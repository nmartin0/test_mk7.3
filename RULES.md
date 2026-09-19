# RULES.md

The generic rules this project works under, in one place.

Everything here is distilled from `PRINCIPLES.md`, `AGENTS.md`,
`WORKFLOW.md`, `DEBUGGING.md`, `docs/GIT-HYGIENE.md` and
`docs/METHODOLOGY.md`. Those files keep the worked examples and the
project-specific detail; this file keeps the rules alone, so the
method can be copied to another project without carrying Mach with it.

Nothing here is a matter of taste. Every rule exists because breaking
it cost this project time that is recorded somewhere in those files.

---

## 1. Evidence

**1.1 Verify directly; never assume.** If you have not run it, you do
not know it. This applies to things that seem certain, and especially
to things that seem certain.

**1.2 A number is a measurement from a run in this session, not an
estimate.** Say "178 of 203 objects compile". If a figure is an
estimate, say that it is.

**1.3 A grep is not proof.** Check the claim where it would actually
live, and check the negative case too. Filename-level surveys give
wrong answers that line-level surveys correct.

**1.4 Prove a semantic claim; do not argue it.** If you claim two
encodings are equivalent, assemble both and compare bytes. Put the
evidence in the commit message.

**1.5 Trust positives; distrust negatives.** "X appeared" is strong.
"X never appeared" is weak: the instrument may be off, the path may not
have been reached, the output may be buffered. Before concluding that
something never ran, prove the instrument works — confirm the symbol
exists, confirm an adjacent probe fires.

**1.6 An empty result is not a zero.** Measuring an empty directory, a
stripped binary, or a build that silently skipped gives you nothing,
not a negative. Confirm you measured the thing you meant to.

**1.7 Ordering is evidence, but it is print order, not causal order.**
Interleaved output from several processes tells you what reached the
log first, not what happened first. Use ordering to generate
hypotheses, never to close them.

**1.8 Prefer a new fact to a new interpretation.** When you catch
yourself re-reading the same output for the fifth time, stop and go get
a measurement that does not exist yet. The tell is re-reading files you
have already read.

**1.9 Count things.** "Some objects fail" is not a finding. "206 of 206
build; the kernel is 1,025,836 bytes" is.

**1.10 Distinguish "hung" from "not there yet".** Check whether the
output is growing and whether the process is consuming CPU before
calling anything stuck.

**1.11 Print the identity of what you are measuring, not just its
value.** A plausible number from the wrong source is the most expensive
kind of wrong. Print the pid, the pointer, the device, the path.

**1.12 One value per debug print.** A multi-argument formatter can
desynchronise on an unexpectedly-sized argument and shift every later
value. This produced two confident, wrong root causes here.

**1.13 Suspect your instrument before the code.** Here the measurement
was wrong before the code was, four times: a 64-bit build of 32-bit
division routines, a divisor masked to zero after being checked,
output piped into `head` so the writer died of SIGPIPE partway, and a
duplicate reader stealing input.

---

## 2. Reaching a conclusion

**2.1 Make the model explain everything.** A hypothesis that explains
four of five observations is wrong. Account for the fifth or keep
looking.

**2.2 Bisect layers, not lines.** Ask which layer the fault is in
before asking which line.

**2.3 Self-audit before you believe yourself.** Write down, in
sentences: what did I actually measure? What would I see if I were
wrong, and does my evidence rule that out? Does this explain all the
observations? What have I not checked that could invalidate this? The
act of forming sentences exposes gaps that thinking does not.

**2.4 Write down the wrong answers.** Keep disproved hypotheses with
the evidence that killed them. A list of what is *not* the problem is a
genuine asset, and it stops the next person — often you — repeating the
cycle.

**2.5 State corrections prominently and early.** If a previous claim
was wrong, say so plainly, including in the history where it already
lives. Do not quietly replace it.

---

## 3. Changing code

**3.1 Deviate from upstream as little as possible, and make the
deviation legible.** The diff against the vendor import is the
deviation record. Every hunk in it must be justified by a commit
message.

**3.2 Stay true to form.** Match the surrounding idiom, era and
conventions. A change should be hard to distinguish from the code it
sits in.

**3.3 No speculative code.** Do not build for a requirement nobody has
stated. Do not add a configuration switch for a case that does not
exist.

**3.4 Do not add a conditional to preserve superseded code.**
Superseded code belongs in history, not in a live branch nothing can
select.

**3.5 Fix the cause, not the call site.** A fix applied where the
symptom appeared will leave every other caller broken. Two bugs here
recurred for exactly this reason.

**3.6 Prefer a build-level change to a source change**, and a
configuration change to either, when they are equally correct.

**3.7 Research real precedent before inventing a pattern.** What do
comparable systems do? Quote the source. Where this project needed
compiler flags, the answer came from FreeBSD's `kern.pre.mk` and
Linux's kernel Makefile, not from reasoning.

**3.8 Say what a change does NOT do** whenever a reader could
over-infer. This is as important as saying what it does.

**3.9 Source changes carry their reasoning in the source**, as a
comment at the site, when the reason is not obvious from the diff.

---

## 4. Proving a change is right

**4.1 Never claim a change is required without removing it and watching
the failure return.** For build fixes: revert, rebuild, confirm the
failure. For runtime fixes: revert, run, confirm the regression.

**4.2 Always include the positive control** — the configuration with
everything applied. A table of failures with no success row proves
nothing.

**4.3 Reproduce the original failure before claiming a fix works.**

**4.4 When a new bug is found that could have caused earlier symptoms,
re-test the earlier fixes against it**, each reverted individually with
the new bug held out of the way.

**4.5 Verify the artefact, not the exit status.** A build that returns
zero may have skipped the step you care about. Check timestamps,
symbols, file contents — something that could only be true if the work
actually happened.

**4.6 Test at the boundary you will ship across.** If the deliverable
is a patch, apply it to a fresh clone of the real remote and build
there. If it is a script, run it exactly as written, in one paste, with
no manual steps in between.

---

## 5. Commits

**5.1** Separate subject from body with a blank line.

**5.2** Subject: 50 characters soft, 72 hard. Capitalised, imperative
mood, no trailing period.

**5.3** Wrap the body at 72 characters.

**5.4** Explain **what and why**, not how. The diff shows how.

**5.5 One logical change per commit.** Two unrelated bugs are two
commits, even if found in the same sitting.

**5.6 Each commit builds and passes its tests on its own**, so bisect
works and any commit can be reverted alone.

**5.7 Documentation and tests go in the same commit as the code they
describe.**

**5.8 A commit claiming a fix must contain the fix.** If the code
ships through a generated artefact — a patch file, a vendored blob —
regenerate that artefact in the same commit.

**5.9 Put the measurements in the message.** The numbers you used to
convince yourself are what convince the next reader.

**5.10 Before pushing, read `git log --oneline -10` and ask:** can I
tell what each commit does from its subject alone? Would I be
comfortable reverting any single one? Do the messages tell a coherent
story?

---

## 6. Delivering work

**6.1 Audit before you deliver.** Re-read what you produced as though
someone else wrote it, check every factual claim in it against a
command you have run, and only then hand it over. This is a step, not
an attitude.

**6.2 Verify every factual claim in a message before sending it.**
If you cannot support a sentence with something you ran, cut it.

**6.3 Give the exact commands to apply the work**, every time, and say
what to look at afterwards.

**6.4 Lead with the check that the work has not already landed.** An
already-applied patch fails with a plain conflict that looks like
corruption.

**6.5 Name what you did not verify.** An untested path stated as
untested is useful; the same path implied to be tested is a trap.

**6.6 Get agreement on shape before large or irreversible work.**
Rewriting history, reordering a build's include path, or anything
touching many commits at once.

**6.7 Keep the record honest.** A history that hides a wrong turn is
worth less than one that shows it, because the next person cannot tell
which parts to trust. If a clean presentation is wanted, build it
alongside the record and label it as a reconstruction.

---

## 7. Documents

**7.1 An honest account of what is open ships with the work.** Never
imply completeness you have not earned.

**7.2 Correct a disproved claim in the document where it lives**, in
the same change that disproves it. A document asserting three things
now known false is worse than one that is merely incomplete.

**7.3 Prefer the specific to the general.** "`e2fsck` reports no
errors after a halt" beats "the filesystem is reliable".

**7.4 Record the traps, with the symptom as it appeared.** The next
reader will meet the symptom first, not the cause.

---

## 8. Working as an agent in a sandbox

These are properties of the environment rather than the project, and
none is discoverable from the code.

**8.1 Do not kill processes by command-line match.** `pkill -f
<pattern>` matches any process whose command line contains the pattern,
including the shell running your own command. Match on the process
name, remembering that Linux truncates it to 15 characters.

**8.2 Long jobs run in the foreground.** A backgrounded build can be
killed at a call boundary, leaving a log that stops mid-file with no
error in it — indistinguishable from a build failure.

**8.3 Do not wrap a long-running guest in a timeout.** A timeout firing
mid-run leaves a truncated log that reads exactly like a hang.

**8.4 Wait on what the system says, not on a guessed number of
seconds.** Block until the expected output appears, with a deadline.

**8.5 Run exactly one reader per input channel.** Two consumers of one
FIFO or socket split the input between them and lose half, silently.

**8.6 Do not modify a guest's disk image from the host while the guest
holds it, or after killing the guest rather than shutting it down.**
Unwritten metadata will be lost and the host tool will reallocate
structures the filesystem still uses.

---

## 9. The two that matter most

If everything above is reduced to two rules:

**Measure, then say.** Every claim traceable to something you ran.

**Say what you got wrong, where the wrong thing lives.** The record is
the asset. It is what lets the next person — including you — trust the
parts that are right.
