# Git hygiene, and repairing a history that lacks it

This repository's history was rewritten from 194 commits to 81 after an
audit found that a large fraction of them carried no code, three claimed
fixes they did not contain, and the reasoning behind each change was
scattered across commits that no longer sat next to it.

This file records what went wrong, the rules that would have prevented
it, and the exact technique used to repair it without losing anything.
It is written for whoever inherits this tree, and for anyone about to
make the same mistakes.


## Part 1: the failure modes

These are real, and each one was caught only because someone asked a
sceptical question. None was caught by a passing check.

### 1. Documenting a fix without shipping it

The worst of the four. Each fix was made in a scratch tree, tested, and
then written up in a commit that touched only `docs/`. The actual code
lived in a patch series file, `tools/lites/lites-osfmk73.patch`, which
was not updated.

The result was commits reading

    FIXED: MAX_PHDRS; getpid now works; new fault storm after it
     docs/current-blocker.md | 64 ++++++

A reader sees "FIXED" and finds no fix. A build from that commit still
has the bug. Six fixes were in this state simultaneously.

**Rule: a commit that claims a fix must contain the fix.** If the code
lives in a generated artefact such as a patch series, regenerate that
artefact as part of the same commit. Never "test locally now, fold in
later" -- later does not arrive.

### 2. Squashing with `fixup` discards reasoning

The first repair folded documentation commits into code commits using
`fixup` semantics. That preserves the *file changes* and discards the
*commit messages*.

The file changes were the point, so this looked correct. It was not: 113
commit messages containing the investigation -- the probes run, the
values measured, the hypotheses retracted -- were destroyed. They were
recoverable only because an unpushed clone still existed.

**Rule: when collapsing history, decide explicitly what happens to each
message.** `fixup` throws them away. `squash` concatenates them. Neither
default is automatically right.

### 3. Folding chronologically instead of topically

The second repair preserved the messages but attached each investigation
to the *next commit in time* that contained code. Because this project's
fixes were committed late -- long after the work that produced them --
that put 28 investigations from five unrelated bugs into a single
commit about a sixth.

The MAX_PHDRS investigation ended up inside the console fix. The tree
hash still matched. Every mechanical check still passed.

**Rule: a work-up belongs with the change it produced, not the change
that happened to come next.** Verify by reading, not by hashing.

### 4. Letting a commit imply more than it did

The `p_pptr` change was real and correct, and its message explained that
`getpid` dereferences the field. A reader would reasonably conclude the
change fixed the wrong-pid bug. It did not; the cause was found three
commits later.

This is the same reason Linux commits say "no functional change" on
refactors: the author knows what the diff does and does not do, and the
reader would have to work it out. State it.

**Rule: say what a change does *not* do when a reader could over-infer.**


## Part 2: the rules worth following

From Chris Beams' seven rules, the Git project's own `SubmittingPatches`,
and the atomic-commit convention followed by the Linux kernel and
Angular.

### Message form

1. Separate subject from body with a blank line.
2. Limit the subject to 50 characters (soft) and 72 (hard).
3. Capitalise the subject; no trailing period.
4. Use the imperative mood: "raise MAX_PHDRS", not "raised" or "raising".
5. Wrap the body at 72 characters.
6. Use the body to explain **what and why**, not how -- the diff shows
   how.

The 50-character soft limit is routinely exceeded in kernel work where a
subsystem prefix is useful (`i386/hardclock.c: ...`). 72 is the limit
that actually matters, because `git log` and most forges wrap there.

### Commit scope

7. One logical change per commit. Two unrelated bugs are two commits,
   even if found in the same sitting.
8. Each commit should build and pass tests on its own, so `git bisect`
   works and any commit can be reverted alone.
9. Include the related documentation and tests **in the same commit** as
   the code they describe.

Rule 9 is the one this repository broke, and it is the one that caused
every downstream problem.

### A useful test

Before pushing, run `git log --oneline -10` and ask:

- Can I tell what each commit does from its subject alone?
- Would I be comfortable reverting any single one of them?
- Do the messages tell a coherent story?


## Part 3: repairing a bad history

The repair had to satisfy one hard constraint: **the final tree must be
byte-identical to what was there before.** A history rewrite is allowed
to change how the work is presented; it is not allowed to change the
work.

### The technique

`git rebase -i` is unusable at this scale -- 194 lines of todo, and
`fixup`/`squash` do the wrong thing with messages. Building the new
history directly with plumbing gives complete control:

```python
# for each group of (documentation... , code) commits:
tree = run(['git', 'rev-parse', code + '^{tree}'])     # the tested tree
msg  = compose(code, folded_documentation_commits)      # message we choose
args = ['git', 'commit-tree', tree] + (['-p', parent] if parent else [])
parent = run(args, input=msg)                           # chain them
```

`git commit-tree` takes a tree, a parent and a message, and returns a
commit. Chaining it builds an arbitrary history. Because the tree comes
straight from a commit that was previously built and booted, the new
commit inherits that state exactly.

Preserve authorship explicitly, or every commit gets today's date and
the rewriting identity:

```python
env.update(GIT_AUTHOR_NAME=an,  GIT_AUTHOR_EMAIL=ae,  GIT_AUTHOR_DATE=ad,
           GIT_COMMITTER_NAME=an, GIT_COMMITTER_EMAIL=ae, GIT_COMMITTER_DATE=ad)
```

### The invariant that makes it safe

Every rewritten commit's tree is identical to some original commit's
tree. That gives two things for free:

- **Nothing is lost.** If the final trees match, no content changed.
- **Buildability is inherited.** Each tree was built and booted when it
  was originally made; folding documentation into it cannot break it,
  because those documentation changes were already present in that tree.

Check it mechanically:

```bash
# every new tree must appear among the original code-commit trees
python3 - <<'EOF'
codetrees = {tree_of(h) for h, kind in rows if kind == 'C'}
missing   = [t for t in map(tree_of, new_commits) if t not in codetrees]
print('trees never previously tested:', len(missing))   # must be 0
EOF
```

### Verifying before pushing

Do not verify in the repository you rewrote. Bundle it and check from an
empty one:

```bash
git branch -f clean-history <new-head>
git bundle create /tmp/clean.bundle clean-history

mkdir /tmp/verify && cd /tmp/verify && git init -q .
git fetch /tmp/clean.bundle clean-history
git rev-parse FETCH_HEAD^{tree}     # must equal the original tree
git rev-list --count FETCH_HEAD
```

And on the receiving side, the check that actually proves it:

```bash
git branch backup-old-history          # before anything
git reset --hard FETCH_HEAD
git diff backup-old-history --stat     # MUST print nothing
git push --force
```

`git diff` against the pre-rewrite branch printing nothing means the
working tree is unchanged by a rewrite that removed 113 commits. Keep
the backup branch until the result has been read on the forge.

### The audit script

Mechanical checks catch form. Run them on the rewritten history:

```python
for h in commits:
    body = message(h).split('\n')
    subject = body[0]
    assert len(subject) <= 72                    # hard limit
    assert not subject.rstrip().endswith('.')
    assert len(body) == 1 or not body[1].strip() # blank line after subject
    assert files_changed(h)                      # no empty commits
    assert code_files_changed(h)                 # no doc-only "fix" commits
assert len(subjects) == len(set(subjects))       # no duplicates from a bad fold
```

Long lines in *quoted* material (code excerpts, register dumps, log
output) are an acceptable exception. Re-wrapping them corrupts them.


## Part 4: what mechanical checks cannot tell you

This is the lesson that cost the most time.

Every wrong version of this rewrite passed every mechanical check. The
tree hash matched. The commit count was right. No commit was empty. The
seven rules were satisfied.

And the reasoning was attached to the wrong changes.

**A tree-hash check verifies mechanics. It says nothing about meaning.**
The only way to catch a misattributed work-up is to read the commits and
ask whether the story each one tells is true.

So after the mechanical audit passes, do this:

```bash
# for each fix, list the investigations folded into it and read them
git log --format='%B' -n1 <fix> | sed -n '/--- work-up/,$p' | grep '^\* '
```

Then ask of each entry: *did this investigation lead to this fix?* If a
commit about the ELF loader appears under a console fix, the fold was
chronological rather than topical, and the whole rewrite needs redoing.


## Part 5: the shape that was settled on

Each commit carries the change, an explanation of what and why, and --
where the change was the end of an investigation -- the trail that led
to it:

```
lites: raise MAX_PHDRS and stop parse_exec_file reading past the array

include/sys/elf.h overlays the file with a struct of one Elf32_Ehdr
followed by MAX_PHDRS program headers, and MAX_PHDRS was 4.
...

--- work-up -------------------------------------------------------

The investigation that led to this change, in the order it happened,
quoted verbatim from the commits made along the way. Wrong turns and
retractions are kept: they record what was ruled out and why.

* Narrow the e_getpid bug: not the cache, not the server's implementation
  ...
* COMPLETE ROOT CAUSE: MAX_PHDRS is 4 and the emulator has 6
  ...
```

The retractions are deliberately kept. In this tree the investigation
that led to MAX_PHDRS included a reading of `prot=0` that turned out to
be a bad `printf` format, and a suspicion of `ext2_specop_p` that turned
out to be correctly wired. Recording that those were eliminated, and
how, is what stops the next person spending an afternoon on them.


## Summary

- A commit claiming a fix must contain the fix.
- Documentation, tests and code for one change belong in one commit.
- When collapsing history, decide deliberately what happens to messages.
- Fold investigations onto the change they produced, not the next one.
- Say what a change does not do, when a reader could over-infer.
- Build rewrites with `git commit-tree`; verify by tree hash from an
  empty clone; keep a backup branch until the result has been read.
- Mechanical checks verify form. Only reading verifies meaning.
