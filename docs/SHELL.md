# The shell as an instrument

A guide to Unix tools and shell usage for systems work, written as
learning material. The principles are general; the examples come from
this project.

A framing that will make the rest of this cohere: **the shell is not a
programming language you happen to write in, it is a machine for
composing measurements.** Most of what follows is about building
pipelines that answer a question precisely, and about the surprising
number of ways such a pipeline can silently answer a *different*
question instead.

---

## Contents

- [Chapter 0: What the shell is for](#chapter-0-what-the-shell-is-for)
- [Chapter 1: The core tools](#chapter-1-the-core-tools)
- [Chapter 2: Composition](#chapter-2-composition)
- [Chapter 3: Quoting and expansion](#chapter-3-quoting-and-expansion)
- [Chapter 4: Running things](#chapter-4-running-things)
- [Chapter 5: Text processing, and when to stop](#chapter-5-text-processing-and-when-to-stop)
- [Chapter 6: Files, binaries, and disks](#chapter-6-files-binaries-and-disks)
- [Chapter 7: Ways pipelines lie](#chapter-7-ways-pipelines-lie)
- [Chapter 8: Worked examples](#chapter-8-worked-examples)
- [Chapter 9: The short version](#chapter-9-the-short-version)

---

# Chapter 0: What the shell is for

Three jobs, and it is worth knowing which one you are doing:

**Asking a question.** "How many interrupts fired?" "Does this symbol
exist?" "Did my change reach the binary?" These are one-liners, they are
throwaway, and their correctness matters enormously because you will
believe the answer.

**Running a procedure.** Build, boot, capture, check. These get repeated
dozens of times, so they belong in a file, not in your history.

**Transforming data.** Extracting addresses from a log, converting one
format to another. The shell is good at this up to a point, discussed in
Chapter 5.

The first is where most of the value is, and most of the danger. A
measurement pipeline that quietly answers the wrong question is worse
than no measurement, because you will act on it.

---

# Chapter 1: The core tools

You do not need many. These do almost everything.

## 1.1 grep: does this exist, and where

```sh
grep pattern file              # lines matching
grep -n pattern file           # ... with line numbers        <- use this
grep -c pattern file           # count only
grep -v pattern file           # lines NOT matching
grep -r pattern dir/           # recursive
grep -l pattern *.c            # just the filenames
grep -w word file              # whole word only
grep -E 'a|b' file             # extended regex (alternation, +, ?)
grep -o 'pat' file             # print only the matching part
grep -A3 -B3 pattern file      # context after/before
```

**Habits worth forming:**

*Use `-n` by default.* Line numbers turn "this appears" into "this
appears *here*", and ordering is often the evidence you actually need.

*Use `-w` for identifiers.* `grep -w panic` will not match `panicstr`,
`warning_panic` or `panicking`. Without it you get noise that looks like
signal.

*`-c` for measurement.* When you want to know whether something changed,
a count is cleaner than eyeballing output. `grep -c 'false interrupt'`
going from 2 to 1 is a result; scrolling through a log is not.

## 1.2 sed: extract a range, replace a string

```sh
sed -n '100,140p' file         # print lines 100-140, nothing else
sed -n '/start/,/end/p' file   # print between two patterns
sed 's/old/new/' file          # replace first per line
sed 's/old/new/g' file         # replace all
sed -i 's/old/new/g' file      # edit in place  <- careful
sed 's/^/  /' file             # indent every line
```

`sed -n 'A,Bp'` is the single most useful form for reading source:
precise, no filtering, no surprises. **Prefer it to `grep` for reading
code** — see §7.1 for why filtering source is dangerous.

`sed -i` edits in place with no backup. For anything that matters, write
to a new file and compare, or use version control so you can undo.

## 1.3 awk: when you need fields or arithmetic

```sh
awk '{print $3}' file                    # third whitespace-separated field
awk -F: '{print $1}' /etc/passwd         # custom separator
awk '$2 == "T" {print $1, $3}' syms      # condition, then action
awk '{sum += $1} END {print sum}' nums   # accumulate
awk 'NR >= 10 && NR <= 20' file          # line-number range
awk '/start/,/end/' file                 # pattern range
```

Reach for `awk` when you need **columns** or **arithmetic**. For
anything with real structure, go to Chapter 5.

**A portability trap that cost time here:** `awk` is several different
programs. `mawk` (common default on Debian), `gawk`, and `nawk` differ
in what they accept. A generator script written for one may abort or,
worse, silently produce truncated output under another. If a
script-generated file looks wrong, try a different `awk` before
debugging the script.

## 1.4 find and xargs: acting on many files

```sh
find . -name '*.c'                       # by name
find . -name '*.c' -newer reference      # modified more recently than
find . -type f -size +10M                # big files
find . -name '*.o' -delete               # careful

find . -name '*.c' | xargs grep -l pattern
find . -name '*.c' -exec grep -l pattern {} +
```

`xargs` splits its input on whitespace by default, which breaks on
filenames containing spaces. `find -print0 | xargs -0` is the safe form,
or use `-exec ... +`.

## 1.5 Pipeline plumbing

```sh
cmd | head -20                 # first 20 lines
cmd | tail -20                 # last 20
tail -f file                   # follow a growing file
cmd | sort | uniq -c | sort -rn  # frequency count, most common first
cmd | wc -l                    # count lines
cmd | wc -c                    # count bytes
cut -c1-72 file                # first 72 columns
tr -d '/' < file               # delete characters
```

`sort | uniq -c | sort -rn` is worth memorising. It answers "what is in
here and how much of each", which is the shape of a great many
questions.

## 1.6 Seeing what is really there

```sh
cat -A file       # show tabs as ^I, line ends as $, control chars
od -c file | head # octal dump with character interpretation
od -Ax -tx1 file  # hex dump with hex offsets
file something    # what kind of thing is this
```

`cat -A` is invaluable when whitespace matters — makefiles, patch files,
anything where a tab and eight spaces differ. It made a multi-line
string literal's raw newlines visible in this project when the source
looked fine.

---

# Chapter 2: Composition

## 2.1 The pipeline as an argument

Each stage narrows. Build them left to right, checking as you go:

```sh
grep -oE 'v=[0-9a-f]+' int.log                      # 1. extract
grep -oE 'v=[0-9a-f]+' int.log | sort               # 2. order
grep -oE 'v=[0-9a-f]+' int.log | sort | uniq -c     # 3. count
grep -oE 'v=[0-9a-f]+' int.log | sort | uniq -c | sort -rn   # 4. rank
```

**Run the intermediate stages.** A pipeline that produces nothing tells
you something is wrong but not where. Running it in pieces tells you
exactly which stage emptied.

## 2.2 Redirection, precisely

```sh
cmd > file          # stdout to file (truncates)
cmd >> file         # stdout appended
cmd 2> file         # stderr to file
cmd > file 2>&1     # both to file          <- order matters
cmd 2>&1 | grep x   # both into a pipe
cmd > /dev/null     # discard stdout
cmd 2>/dev/null     # discard stderr only
```

`> file 2>&1` and `2>&1 > file` do different things. The first sends
both to the file; the second sends stderr to the *old* stdout (your
terminal) and only then redirects stdout. Read it as a sequence of
assignments, left to right.

**Compiler and build errors go to stderr**, so `make | grep error` finds
nothing. It must be `make 2>&1 | grep error`.

## 2.3 Command substitution and process substitution

```sh
files=$(ls *.c)                  # capture output into a variable
echo "count: $(wc -l < f)"       # inline

diff <(cmd1) <(cmd2)             # process substitution (bash, not sh)
```

`diff <(sort a) <(sort b)` compares two *outputs* without temporary
files. This needs bash; it is not POSIX `sh`.

## 2.4 Here-documents

For multi-line input to a command:

```sh
cat > file <<'EOF'
literal text, no $variable expansion
EOF

cat > file <<EOF
expanded: $HOME
EOF

python3 - <<'PY'
print("a whole script inline")
PY
```

**Quote the delimiter (`<<'EOF'`) unless you want expansion.** Unquoted,
`$`, backticks and backslashes are interpreted, which mangles code.

**A practical warning.** Heredocs pasted into a terminal are fragile: if
your terminal wraps a long line, the delimiter can end up mid-line and
the heredoc never closes, or closes somewhere unintended. Symptoms are
bizarre — a command running with a fragment of the next one glued on.
When a pasted heredoc misbehaves, put it in a file and run the file. In
this project a paste mangled `EOF` into the middle of a line and
produced a completely mystifying error.

---

# Chapter 3: Quoting and expansion

This is where most shell bugs live. The rules are short.

## 3.1 The three quoting states

```sh
echo $var        # unquoted: expanded, then split on whitespace, then globbed
echo "$var"      # double: expanded, NOT split, NOT globbed
echo '$var'      # single: literal, no expansion at all
```

**Double-quote every variable expansion unless you have a specific
reason not to.** The failure mode is silent: a path with a space becomes
two arguments, and a command gets the wrong thing without complaint.

```sh
K=/path/with space/kernel
qemu -kernel $K      # two arguments: "/path/with" and "space/kernel"
qemu -kernel "$K"    # one argument, correct
```

## 3.2 Why variables help with long commands

Beyond quoting, there is a practical reason to hoist long paths into
variables:

```sh
K=$BUILD/obj/at386/mach_kernel/PRODUCTION/mach_kernel.PRODUCTION
qemu-system-i386 -kernel "$K" ...
```

A long path pasted into a terminal can be wrapped or truncated. If it is
in a variable on its own line, a mangled paste fails visibly instead of
silently passing a truncated path. This happened in this project: a
kernel path was cut to `.../m` and QEMU reported a missing file, which
was at least obvious — a subtler truncation might not have been.

## 3.3 Line continuations

```sh
cmd -a \
    -b \
    -c
```

**The backslash must be the last character on the line.** A single
trailing space after it breaks the continuation, and the shell runs a
truncated command followed by garbage. It is invisible in most editors.

In this project a trailing space after `\` caused QEMU to silently omit
a `-device` argument, and the resulting behaviour was misread as the
device option not working.

To check:

```sh
grep -nE '\\ +$' script.sh     # backslash followed by spaces at line end
cat -A script.sh | grep '\\ '  # see it directly
```

## 3.4 Globs are the shell's, not the command's

```sh
ls *.c           # the SHELL expands *.c, then runs ls with the results
grep pat *.c     # same
```

If nothing matches, most shells pass the pattern through literally,
which produces confusing errors. And a glob that matches thousands of
files can exceed the argument limit — use `find | xargs` for large sets.

---

# Chapter 4: Running things

## 4.1 Foreground, background, and why your prompt vanished

```sh
cmd             # foreground: your shell waits
cmd &           # background: you get your prompt back
jobs            # what is running
kill %1         # kill job 1
wait            # wait for all background jobs
```

A long-running command in the foreground looks identical to a hung
terminal. **Background anything that runs for minutes**, especially
emulators and servers:

```sh
qemu-system-i386 ... &
sleep 180
tail -20 /tmp/console.log
```

Note what this composite does: QEMU runs in the background, `sleep`
blocks in the foreground, and you get your prompt back after the sleep
while QEMU *keeps running*. That surprises people. Check with `jobs` or:

```sh
ps aux | grep -c '[q]emu-system'
```

The `[q]` trick makes the pattern not match the `grep` process itself.

## 4.2 Bounding runtime

```sh
timeout 300 cmd                 # kill after 300 seconds
timeout 300 cmd || echo "timed out or failed"
```

`timeout` is essential for anything that might hang. Without it, an
emulator that never exits holds file locks and blocks the next run — a
recurring nuisance in this project, where a leftover QEMU held a write
lock on a disk image and the next invocation failed with an unhelpful
message.

## 4.3 Is it hung, or just slow?

The most important measurement in a slow environment:

```sh
wc -c /tmp/log; sleep 60; wc -c /tmp/log
```

If the byte count moves, it is working. This one check prevents a whole
class of false diagnosis. "Nothing has happened yet" and "nothing will
ever happen" look identical and need different responses.

## 4.4 Exit status

```sh
cmd; echo $?              # 0 = success, non-zero = failure
cmd && echo "worked"      # run if success
cmd || echo "failed"      # run if failure
cmd1 && cmd2 || cmd3      # NOT if/then/else -- see below
```

**`&&`/`||` chains are not if/then/else.** If `cmd2` fails, `cmd3` runs
too. For real branching use `if`.

**And a subtle one that bit me in this project:**

```sh
cmd | grep error | sed 's/x/y/' || echo "OK"
```

The `||` tests the exit status of the *last* command in the pipeline,
which is `sed` — and `sed` almost always succeeds. So the `|| echo "OK"`
never fires, no matter what `grep` found. I wrote exactly this and
misread the resulting silence as "no errors".

If you need a pipeline's earlier status, either restructure or use
`set -o pipefail` in bash.

## 4.5 Scripts

The moment a sequence is run twice, put it in a file:

```sh
#!/bin/sh
set -e          # exit on any command failing
set -u          # error on undefined variables
```

`set -e` and `set -u` catch a great many mistakes. Note that `set -e`
does not trigger on failures inside `&&`/`||` chains or in conditions.

**Know which shell you are writing for.** `#!/bin/sh` on many systems is
`dash`, not `bash`, and dash lacks arrays, `[[ ]]`, process substitution
and more. Use `#!/bin/bash` if you want bash features — see §7.4 for the
`echo` trap that follows from this.

---

# Chapter 5: Text processing, and when to stop

## 5.1 The escalation ladder

Match the tool to the structure:

| Data shape | Tool |
|---|---|
| Lines, matched by pattern | `grep` |
| Lines, simple substitution | `sed` |
| Whitespace-separated columns | `awk` |
| Anything with real structure | a real language |

The mistake is staying in the shell too long. If your pipeline has more
than about three stages of `sed`/`awk` with escaped regexes, you are
writing a program badly. Stop and write it properly:

```sh
python3 - <<'PY'
import re, bisect
# now you have data structures, error messages, and the ability to test
PY
```

Throughout this project, anything involving parsing a binary format,
mapping addresses to symbols, or editing source structurally was done in
Python invoked from the shell. That is the right division: the shell
runs things and moves data between them; the language does the thinking.

## 5.2 Structured edits belong in a language

Editing source with `sed` is tempting and dangerous. A pattern that
matches slightly more than you intended damages the file silently. The
safer pattern:

```sh
python3 - <<'PY'
p = 'file.c'
s = open(p).read()
old = '''exact text to replace'''
new = '''replacement'''
assert s.count(old) == 1        # fail loudly if the assumption is wrong
open(p,'w').write(s.replace(old, new))
PY
```

**The `assert` is the important line.** It converts "silently edited the
wrong thing" into "stopped and told you". Every structural edit in this
project used this form.

## 5.3 Regular expressions, briefly

```
.        any character            ^     start of line
*        zero or more             $     end of line
+        one or more (with -E)    []    character class
?        optional (with -E)       [^]   negated class
|        alternation (with -E)    \<\>  word boundaries (GNU)
```

Use `grep -E` (or `egrep`) for `+`, `?` and `|`. Basic `grep` requires
backslashes for these, which is a needless source of confusion.

**Test a regex before trusting it.** Especially a `-v` (inverting) one:

```sh
echo 'test line' | grep -E 'yourpattern'
```

---

# Chapter 6: Files, binaries, and disks

## 6.1 Inspecting binaries

```sh
file binary                  # architecture, type, stripped or not
nm binary                    # symbols (needs an unstripped binary)
nm binary | grep -w ' T main'
readelf -h binary            # header, including entry point
readelf -S binary            # sections
readelf -l binary            # segments (what actually gets loaded)
objdump -d binary            # disassemble
strings binary               # printable sequences
strings -t x binary          # ... with hex file offsets
```

**`strings -t x` is underrated.** The offset tells you *where* a string
lives, which tells you which section, which often answers the real
question.

**And a caution from this project.** `strings` finds printable byte
sequences, not strings. The four bytes `55 57 56 53` — the standard
i386 function prologue `push ebp; push edi; push esi; push ebx` — render
as `UWVS`. A binary full of `UWVS` at thousands of offsets is not full
of a mysterious string; it is full of functions. Knowing this turned an
apparently corrupt panic message into a precise diagnosis: the pointer
was aimed at code, not at text.

## 6.2 Comparing

```sh
diff a b                     # line differences
diff -q a b                  # just "differ or not"
diff -r dir1 dir2            # recursive
diff -rq dir1 dir2           # which files differ, recursively
cmp a b                      # first differing byte (works on binaries)
md5sum a b                   # same or not
```

`diff -rq` between your tree and a reference tree is one of the highest
value commands in systems archaeology. In this project it established
that our kernel was a copy of a public tree plus exactly eleven changed
files — which made that tree's userland a worked example rather than an
analogy.

## 6.3 Disks and raw data

```sh
dd if=/dev/zero of=img bs=1M count=20      # create a blank image
dd if=img bs=512 count=1 | od -Ax -tx1     # read the first sector
truncate -s 20M img                        # faster for sparse files
```

`dd`'s `bs` and `count` are in units of `bs`. `bs=1M count=20` is 20 MB.

To check a filesystem's magic number without mounting anything:

```sh
python3 -c "
import struct
d = open('img','rb').read()
print(hex(struct.unpack_from('<H', d, 1024+56)[0]))   # ext2 magic at sb+56
"
```

Reading a structure directly beats trusting that a formatter did what
you asked.

## 6.4 Where things are

```sh
which prog                   # first match in PATH
command -v prog              # POSIX equivalent
type prog                    # shell's view: alias, function, builtin, file
ls /sbin/prog                # some tools are not in a normal user's PATH
```

**System administration tools often live in `/sbin` or `/usr/sbin`,
which are not in a regular user's `PATH`.** `mke2fs`, `fdisk`, `ip` and
many others. `which mke2fs` returning nothing does not mean it is not
installed — this cost a round in this project.

## 6.5 Space

```sh
df -h /tmp                   # free space on a filesystem
du -sh dir/                  # size of a directory
```

**`/tmp` is frequently a RAM filesystem (`tmpfs`).** Writing a large log
there consumes memory and can fill in seconds. In this project an 8 GB
execution trace filled a 7.9 GB `/tmp`, truncating the log and
destroying the measurement. Check with `df -h /tmp` before writing
anything large, and write big files to real disk.

---

# Chapter 7: Ways pipelines lie

The failure modes that matter. Each of these produced a wrong conclusion
in this project.

## 7.1 A filter that removes the answer

This pattern is meant to strip C comment continuation lines:

```sh
grep -vE '^\s*\*' file.c
```

`^\s*\*` matches ` * comment`. It **also** matches a pointer
dereference assignment:

```c
	*hostp = some_value;         /* silently deleted from the output */
```

So a function whose body is out-parameter assignments appears to be an
empty stub. This produced a confident and completely wrong diagnosis
**twice, on the same function**.

**The lesson generalises: do not filter what you are trying to
understand.** Filters are for finding, not for reading. Once you have
found the region, read it with `sed -n 'A,Bp'` and look at every line.

## 7.2 An empty result that means something else

```sh
grep -c 'pattern' file
```

Zero means "not found". It does *not* mean "not there". It might mean:

- the file is not what you think (wrong path, stale copy)
- the pattern is wrong (case, word boundaries, regex dialect)
- the text is split across lines
- the output went to stderr and never reached the pipe
- the process had not produced it yet

**Before believing an absence, prove your pipeline can find a
positive.** Search for something you know is there. If that fails, your
pipeline is broken, not the world.

## 7.3 Reading output before it exists

```sh
cmd > log &
tail -20 log        # may show a partial file, or nothing
```

A log being written concurrently is a moving target. A `tail` taken mid
write can show a truncated final line, or stop just before the
interesting part. In this project a `tail` taken at the wrong moment
omitted a line, and its absence was read as evidence — leading to a
retraction.

**Wait for a specific marker rather than a duration:**

```sh
while ! grep -q 'expected text' log 2>/dev/null; do sleep 5; done
```

## 7.4 The shell is not the shell you think

`/bin/sh` is `dash` on many modern systems, and `dash`'s `echo`
interprets backslash escapes while `bash`'s does not:

```sh
echo "a\nb"     # bash: literal a\nb      dash: a, newline, b
```

A 1990s build script using `echo` to emit `\n` into a generated C file
produces a real newline under `dash`, and the resulting C is malformed.
This happened in this project and took a while to see, because the
script was obviously correct.

**Use `printf` instead of `echo` for anything containing a backslash:**

```sh
printf '%s\n' "$var"
```

## 7.5 Stale artefacts

You rebuild, you test, you get the old behaviour.

```sh
ls -la output_file          # is the timestamp recent?
ls -la output_file input.c  # is the output newer than the input?
```

Two real instances here. First, a link step did not rerun, so a boot was
testing a binary from forty minutes earlier. Second — more insidious —
a configuration change altered the *output filename*, so the old file
was still present and still being copied, while the new one was built
alongside it under a different name.

**Check the timestamp, and check you are looking at the right file.**

## 7.6 Counting the wrong thing

```sh
make 2>&1 | grep -c 'error'
```

This counts lines containing "error", which includes `-Werror` in a
command line, the word "error" in a filename, and notes about errors. Be
specific:

```sh
make 2>&1 | grep -cE 'error:'      # the compiler's actual error format
```

---

# Chapter 8: Worked examples

## 8.1 Streaming a log too big to store

**The problem.** An emulator trace produces ~300 MB per 20 seconds, and
a full run would be ~13 GB. `/tmp` is a 7.9 GB tmpfs. Writing the log
filled it, truncated the output, and destroyed the measurement.

**The solution.** Never store it. Filter it as it is produced, using a
named pipe:

```sh
mkfifo /tmp/fifo

grep --line-buffered -oE '/00000000080[0-9a-f]{5}/' /tmp/fifo \
    | tr -d '/' > /tmp/addresses.txt &
FILTER=$!

qemu-system-i386 ... -d exec -D /tmp/fifo

wait $FILTER
wc -l /tmp/addresses.txt
```

**Why each part:**

- `mkfifo` creates a named pipe — the writer thinks it is a file
- `--line-buffered` makes `grep` emit each line immediately rather than
  buffering, which matters because the writer may run for minutes
- `-oE` prints only the matching part, discarding everything else
- `$!` captures the background job's PID so `wait` can join it
- the filter starts *first*, so nothing is lost

Result: a few MB on disk instead of 13 GB, and the measurement survived.

**The general lesson:** when data is too big to keep, reduce it at the
source. A named pipe lets you put a filter between a producer and its
"file" without either knowing.

## 8.2 Checking a format before writing a parser

**The mistake.** I wrote a filter for an emulator trace based on what I
assumed the format was:

```sh
grep -oE '0x08[0-9a-f]{6}'      # found nothing
```

**The reality**, discovered after wasting a 15-minute run:

```
Trace 0: 0x7fc410000240 [000f0000/00000000000fe05b/00000040/ff020000]
```

The guest address is the second bracketed field, as sixteen hex digits —
so it appears as `/0000000008049330/`, which the pattern above never
matches.

**The habit that prevents it:**

```sh
timeout 20 producer -d exec -D /tmp/sample.log
head -3 /tmp/sample.log
rm -f /tmp/sample.log
```

Twenty seconds and three lines, before committing to anything. **Look at
real data before writing a pattern for it** — always, even when you are
sure you know the format.

## 8.3 A safe structural edit

**The problem.** Change a specific function in a source file, as part of
a patch series that must apply cleanly to a pristine upstream clone.

**The wrong way:** `sed -i 's/old/new/'` — no confirmation that it
matched once, or at all, or in the right place.

**The way used throughout this project:**

```sh
python3 - <<'PY'
p = 'server/kern/subr_prf.c'
s = open(p).read()

old = '''	va_start(ap, fmt);
	printf("panic: %r\\n", fmt, ap);
	va_end(ap);'''

new = '''	printf("panic: %s\\n", fmt);

	va_start(ap, fmt);
	printf("panic args: %r\\n", fmt, ap);
	va_end(ap);'''

assert s.count(old) == 1          # <- the load-bearing line
s = s.replace(old, new)
open(p,'w').write(s)
print('  patched')
PY
```

Then verify the result independently:

```sh
git diff > /tmp/series.patch
cd /tmp && rm -rf verify
git clone -q <upstream> verify
cd verify && patch -p1 --dry-run -s < /tmp/series.patch && echo VERIFIED
```

**Why this shape:**

- exact text, not a regex, so it cannot match something unintended
- `assert count == 1` fails loudly if the file is not what you expect
- `--dry-run` against a *fresh clone* proves the patch applies to
  upstream, not merely to your working copy

## 8.4 Turning a log into a diagnosis

**The question.** Which functions did a program execute before it died?

**The pipeline**, in stages, each checked before adding the next:

```sh
# 1. symbols, sorted by address
nm --defined-only binary.unstripped | sort > /tmp/syms.txt
wc -l /tmp/syms.txt                       # sanity: is this plausible?

# 2. addresses from the trace
grep -oE '/00000000080[0-9a-f]{5}/' trace.log | tr -d '/' > /tmp/eips.txt
wc -l /tmp/eips.txt                       # sanity: non-zero?

# 3. map addresses to names -- now in a real language
python3 - <<'PY'
import bisect
syms = []
for line in open('/tmp/syms.txt'):
    parts = line.split()
    if len(parts) == 3 and parts[1] in 'TtWw':
        syms.append((int(parts[0], 16), parts[2]))
syms.sort()
addrs = [a for a, _ in syms]

seen = []
for line in open('/tmp/eips.txt'):
    v = int(line.strip(), 16)
    i = bisect.bisect_right(addrs, v) - 1
    name = syms[i][1] if i >= 0 else '?'
    if not seen or seen[-1] != name:
        seen.append(name)

print(f'{len(seen)} distinct functions')
for n in seen[-25:]:
    print('  ', n)
PY
```

**Note the division of labour.** The shell extracts and counts; Python
does the address arithmetic. Trying to do a binary search in `awk` would
be possible and miserable.

**And note the sanity checks.** `wc -l` after each extraction step. If
step 2 produces zero lines, step 3 will confidently print nothing and
you will believe the program executed nothing — which is exactly the
wrong conclusion, and exactly what §7.2 warns about.

## 8.5 Debugging a pipeline that produces nothing

A worked routine, because this is the commonest shell problem:

```sh
# the pipeline that produces nothing
make 2>&1 | grep -E 'error:' | head -20
```

Work backwards:

```sh
make 2>&1 | wc -l                     # is there any output at all?
make 2>&1 | tail -20                  # what does the end look like?
make 2>&1 | grep -ci error            # case-insensitive, any mention?
make 2>&1 | grep -E 'rror'            # partial, in case of odd formatting
```

Most often one of:

- output went to stderr and you forgot `2>&1`
- the build did nothing because it was already up to date
- the pattern is subtly wrong
- the failure is a warning promoted to an error and says "Error 1"
  rather than "error:"

**The principle: prove the pipeline can find something before believing
it found nothing.**

---

# Chapter 9: The short version

**Reading and finding**

1. `grep -n` by default; `-w` for identifiers; `-c` to measure.
2. Read source with `sed -n 'A,Bp'`, unfiltered. Filters eat answers.
3. `cat -A` when whitespace matters.

**Composing**

4. Build pipelines in stages and run the intermediates.
5. `2>&1` or you will not see compiler errors.
6. `sort | uniq -c | sort -rn` answers "what is in here".

**Quoting**

7. Double-quote every variable expansion.
8. Hoist long paths into variables — a mangled paste then fails visibly.
9. A trailing space after `\` silently truncates a command.

**Running**

10. Background anything slow; `timeout` anything that might hang.
11. `wc -c file; sleep 60; wc -c file` distinguishes hung from slow.
12. `||` after a pipeline tests the *last* command, not the interesting
    one.

**Escalating**

13. More than three `sed`/`awk` stages means write it in a real
    language.
14. Structural edits: exact strings plus `assert count == 1`.

**Distrusting**

15. Prove your pipeline can find a positive before believing a negative.
16. Look at real data before writing a pattern for it.
17. Check timestamps — and check you are looking at the right file.
18. `/bin/sh` may be `dash`; use `printf`, not `echo`.
19. `/tmp` may be RAM; check `df -h` before writing anything large.
20. `strings` finds printable bytes, not strings. `UWVS` is a function
    prologue.

---

*The thread running through all of this: a shell pipeline is an
instrument, and instruments can be wrong. The difference between a good
and a bad one is rarely cleverness — it is whether you checked that it
measures what you think it measures. Most of the mistakes catalogued
here cost an hour or a day in this project, and every one of them would
have been caught by running the pipeline against a known-positive case
first.*
