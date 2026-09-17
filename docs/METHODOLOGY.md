# Debugging and developing an operating system

A guide to the craft, written as learning material. The principles apply
to any kernel; the worked examples come from this project, because real
examples beat invented ones.

---

## Contents

- [Chapter 0: Why this is different](#chapter-0-why-this-is-different)
- [Chapter 1: The instruments](#chapter-1-the-instruments)
- [Chapter 2: Making the machine talk](#chapter-2-making-the-machine-talk)
- [Chapter 3: The method](#chapter-3-the-method)
- [Chapter 4: Reading code you did not write](#chapter-4-reading-code-you-did-not-write)
- [Chapter 5: Research](#chapter-5-research)
- [Chapter 6: Getting unstuck](#chapter-6-getting-unstuck)
- [Chapter 7: Worked examples](#chapter-7-worked-examples)
- [Chapter 8: The short version](#chapter-8-the-short-version)

---

# Chapter 0: Why this is different

Debugging an application is comfortable. You attach a debugger, set a
breakpoint, inspect variables, print things. The operating system
underneath you is a reliable witness: when your program stops, something
still works well enough to tell you so.

In kernel work that witness is what you are building. This changes
things in four ways, and every technique in this guide exists because of
one of them.

**1. There is nothing underneath you.** `printf` is a service the kernel
provides. Before it works, you cannot print. A failure early in boot is
invisible by default, and your first job on any new kernel is usually to
get *some* channel out — a serial port, a memory buffer you can dump
later, a specific pattern written to video memory. Until you have that,
you are working blind.

**2. Failure is silent and total.** An application crashes and you get a
stack trace. A kernel triple-faults and the machine resets, or wedges
with no output at all, or — worst — keeps running with quietly corrupt
state and fails somewhere unrelated ten seconds later. "Nothing
happened" is the most common symptom and the least informative.

**3. The bug may not be in the code you are reading.** It may be in the
compiler's interpretation of it, in the assembler, in the linker, in the
hardware's response to a register write, or in the emulator's model of
that hardware. In application work you can usually assume the layers
below are correct. Here you cannot, and a surprising fraction of real
kernel bugs turn out to live in those layers.

**4. The feedback loop is slow.** Rebuild, reboot, watch. Seconds if you
are lucky, minutes if you are not. This changes the economics
completely: it is worth spending ten minutes designing a measurement
that answers the question definitively, rather than five minutes on one
that half-answers it.

> **The consequence.** Application debugging rewards inspection — look
> at things until you understand. Kernel debugging rewards
> **experiment design** — decide what would distinguish two
> possibilities, then measure exactly that.

---

# Chapter 1: The instruments

Each instrument answers a different class of question. Knowing which to
reach for is most of the skill; the commands are easy.

| I want to know | Instrument |
|---|---|
| What did the kernel say? | Serial console |
| Did this code run at all? | Execution trace |
| What is in this variable right now? | Debugger, attached to a hung machine |
| Is the hardware doing anything? | Interrupt trace |
| Is this binary shaped the way I think? | `nm`, `readelf`, `objdump` |
| Did my source change reach the binary? | `strings`, `nm`, rebuild timestamps |

## 1.1 The serial console: your lifeline

**The question it answers:** what did the kernel manage to say before it
stopped?

Every kernel worth the name can write to a serial port very early in
boot, because a UART needs almost no setup — no interrupts, no memory
management, no drivers. That makes it the most reliable output channel
in existence, which is why it has been the standard kernel debugging
channel for fifty years.

The general pattern, whatever the kernel:

1. Tell the kernel to use the serial port for console output. Usually a
   boot flag or command-line parameter.
2. Tell the emulator to capture that port to a file.
3. Read the file, and **grep it** rather than eyeballing it.

```sh
# QEMU: capture COM1 to a file
qemu-system-i386 ... -serial file:/tmp/console.log

# watch it live from another terminal
tail -f /tmp/console.log
```

**Why not the screen?** A video console is a scrolling 25-line buffer.
Once output passes the top it is gone. If your boot produces sixty lines
you can only see the last twenty-five, and if you sample it at the wrong
moment you see a snapshot that looks like a hang.

> **In this project:** the kernel's `-r` boot flag routes output to
> COM1. Before we used it we were dumping video memory with a Python
> script, and several wrong conclusions came from sampling that
> scrolling buffer at the wrong moment and reading absence as evidence.
> Switching to serial eliminated an entire class of mistake.

**How to read it.** Always with `grep -n`, never by scrolling:

```sh
grep -nE 'panic|error|warning|fault' /tmp/console.log
```

The `-n` matters more than you would think. Line numbers give you
**ordering**, and ordering is evidence — see §3.5.

## 1.2 The execution trace: did this code run?

**The question it answers:** which instructions actually executed?

Emulators can log every basic block the CPU runs. This is the instrument
of last resort and the one that ends arguments, because it observes
execution directly rather than inferring it.

```sh
qemu-system-i386 ... -d exec -D /tmp/exec.log
```

**Three things to know before you use it.**

*It is enormous.* Hundreds of megabytes per minute. It will fill a
`/tmp` that lives in RAM. Either write to real disk, or stream it
through a filter and keep only what you care about:

```sh
mkfifo /tmp/fifo
grep --line-buffered -oE '<your pattern>' /tmp/fifo > /tmp/filtered.txt &
qemu-system-i386 ... -d exec -D /tmp/fifo
```

*It usually needs software emulation.* Hardware acceleration (KVM) runs
the guest natively, so there is nothing to trace. Tracing runs are
therefore slow — plan for it.

*Check the line format before writing a filter.* It is emulator- and
version-specific and rarely what you would guess:

```
Trace 0: 0x7fc410000240 [000f0000/00000000000fe05b/00000040/ff020000]
                          ^cs_base  ^^^^^^^^^^^^^^^^ guest PC
```

The guest program counter is the *second* bracketed field, sixteen hex
digits. A filter looking for `0x08...` matches nothing, because the
address appears as `/0000000008049330/`.

> **Do this first:** run the emulator for twenty seconds, `head -3` the
> log, and look at a real line. Guessing the format costs a full run.

**Turning addresses into names.** A trace of raw addresses is useless
until you map it to symbols:

```sh
nm --defined-only binary | sort > symbols.txt
```

then for each address find the largest symbol address not greater than
it. In Python, `bisect.bisect_right` over a sorted list. The last
distinct symbol before execution stops is usually your answer.

## 1.3 The debugger: what is in memory right now?

**The question it answers:** what are the actual values, at the actual
moment of failure?

Most emulators expose a GDB stub:

```sh
qemu-system-i386 ... -s          # stub on localhost:1234
qemu-system-i386 ... -s -S       # ... and freeze until gdb connects
```

**The technique that works best on a hang**, and it is not the obvious
one: do *not* use `-S` and breakpoints. Start the machine normally, let
it reach the failure, **then attach**. The stub halts the guest when
gdb connects, so you can read whatever you like with no breakpoints at
all:

```sh
gdb -q -batch \
    -ex 'target remote :1234' \
    -ex 'x/4xw 0xc01d83ec' \
    -ex 'info registers eip esp'
```

This is reliable in a way breakpoints often are not, and it is perfect
for the commonest kernel question: *the machine is stuck; what does its
state look like?*

**Traps that will bite you:**

*Address translation.* Kernels often run at a virtual address different
from where they are linked. If your kernel is linked at `0x00100000` but
runs at `0xC0100000`, every address you give the debugger needs the
offset. Check your kernel's linker script or its segment base.

*Breakpoint limitations.* In several emulator/gdb combinations only one
breakpoint services reliably at a time, and breakpoint **conditions**
and **ignore counts** may silently do nothing. Set one, measure,
delete, set the next.

*Use a hardware breakpoint when the target may not be the current
task.* A software breakpoint is implemented by writing a trap
instruction into the target's memory, so it can only be set while that
memory is mapped in the current context. On a microkernel, where the
task you care about is one of several and is rarely running at the
moment you attach, this fails with "Cannot insert breakpoint / Cannot
access memory" even though the address is perfectly valid. A hardware
breakpoint uses the CPU's debug registers, needs no memory access to
set, and can be placed before the target task even exists. Make it the
default for user-space code in a multi-task system.

*Do not blame hardware acceleration without evidence.* It is easy to
attribute flaky debugging to KVM and disable it, which can turn a
one-minute boot into a forty-minute one. In this project that was done
on an invented belief: the breakpoint failures on record happened in an
environment that had no KVM, so they were software-emulation failures
being blamed on hardware acceleration. What is actually true is
narrower -- an execution trace needs software emulation, because with
KVM there is nothing to log. Breakpoints themselves work under
acceleration. Check which of your problems is which before paying for
the slow path.

*A breakpoint that does not fire proves nothing.* See §3.4 — this is
important enough to have its own section.

## 1.4 The interrupt trace: is the hardware alive?

**The question it answers:** is the device doing anything, and how much?

Far smaller than an execution trace and often sufficient:

```sh
qemu-system-i386 ... -d int -D /tmp/int.log
grep -oE 'v=[0-9a-f]+' /tmp/int.log | sort | uniq -c | sort -rn
```

On x86 the vector tells you the source. Vectors `0x00`–`0x1f` are CPU
exceptions (`v=00` divide error, `v=0e` page fault, `v=0d` general
protection). Above that they are whatever your kernel programmed the
interrupt controller to use, so check your own IDT setup — in this
project the PIC base is `0x40`, making timer `v=40`, keyboard `v=41`,
floppy `v=46`.

**Counting is often the whole measurement.** "This device produced two
interrupts and then stopped" is a diagnosis. So is "it produced ten
after my change, where it produced two before".

> **In this project:** a floppy driver hang was confirmed fixed by
> exactly this — interrupt count went from 2 to 10, and every
> previously-unreached function started running.

## 1.5 Static inspection: is the binary what I think?

Before blaming runtime behaviour, check that the artefact is shaped
correctly. These are instant and catch entire categories of problem:

```sh
file binary                 # architecture, 32/64-bit, static/dynamic
readelf -h binary           # ENTRY POINT -- see below
readelf -l binary           # segments: where things load
nm binary | sort            # symbols and addresses
objdump -d --start-address=0x1234 --stop-address=0x1300 binary
strings binary | grep -x 'somestring'   # did my change get compiled in?
```

**`readelf -h` deserves a habit.** The entry point is where execution
begins, and if it is wrong nothing else matters. Check it against your
symbol table:

```sh
readelf -h binary | grep -i entry
nm binary | grep -w '_start\|__start\|main'
```

If the entry address does not correspond to a plausible startup symbol,
**stop and fix that first**.

> **In this project:** a server loaded, was scheduled, and died with no
> output. Four hypotheses were investigated and disproved over several
> hours. The entry point was `0x8049000`; `nm` said that address was
> `ip_setmoptions.cold`, a fragment of an unrelated networking function.
> Two commands would have found it immediately. Full story in §7.3.

## 1.6 Emulator monitors and other odds

Most emulators have a monitor interface for inspecting machine state
without a debugger — memory dumps, device state, registers:

```sh
qemu-system-i386 ... -monitor unix:/tmp/mon,server,nowait
```

Useful when you want device-level truth (is this disk attached? what
geometry does the emulator think it has?) rather than guest-level.

---

# Chapter 2: Making the machine talk

You cannot debug what you cannot observe. On a system that does not yet
have working output, getting *any* channel is the first task, and it is
worth doing properly rather than improvising.

**In rough order of how early they work:**

1. **A single byte to a port.** `outb` to the serial data register with
   no setup at all. Ugly, but works before anything else does.
2. **Writing a known pattern to video memory.** On x86, poking
   characters at `0xB8000` needs no driver. You will see them even if
   the machine then dies.
3. **Serial console.** Needs a few register writes; works before
   interrupts, memory management, or any device framework.
4. **A memory ring buffer you dump afterwards.** Write messages into a
   fixed address, then read it with a debugger after the machine hangs.
   Invaluable when output itself is what is broken.
5. **The kernel's own `printf`.** Comfortable, but depends on a working
   console device, which depends on device configuration, which is often
   exactly what you are debugging.

**When output is unavailable**, the instruments from Chapter 1 substitute
for it: an execution trace tells you where you got to, and attaching a
debugger to the hung machine tells you what state you got there with.
Between them you can diagnose a kernel that has never printed a
character.

> **A note on ordering.** Get the console working before anything else,
> even if it feels like a detour. Every hour spent on it is repaid many
> times over, because every subsequent bug becomes visible instead of
> silent.

---

# Chapter 3: The method

Tools are the easy part. This chapter is the difficult part.

## 3.1 The loop

```
observe  →  form ONE hypothesis  →  design the cheapest measurement
that could DISPROVE it  →  run it  →  believe the result
```

The word doing the work is **disprove**. A measurement that can only
confirm what you already believe teaches you nothing. Before running it,
ask: *"if my hypothesis is wrong, what will this show?"* If the answer
is "the same thing", design a different measurement.

**One hypothesis at a time.** Two changes at once, and a change in
behaviour tells you nothing about which caused it. This is tedious and
it is the difference between converging and wandering.

## 3.2 Bisect layers, not lines

An operating system is a stack. A failure at the top can originate
anywhere below. Do not read code looking for the bug — establish which
**layer** it is in, then recurse into that layer.

A generic storage stack, each link separately verifiable:

```
does the kernel boot?              → any console output at all
is the device detected?            → probe message
does the device open?              → open call returns success
does I/O complete?                 → data arrives, correct length
is the data correct?               → contents match what was written
does the filesystem mount?         → mount returns success
can you read a file?               → contents of a known file
```

Each question is cheap, and each answer eliminates everything below it.
This is the single most effective technique in this guide, because it
turns "something is broken" into "the fault is between step 3 and step
4", which is a tractable problem.

> **In this project**, the storage path was fixed in four separate
> stages — device naming, partition sizing, geometry source, interrupt
> attribution — and each blocker only became visible once the one before
> it was cleared. Trying to reason about all four at once would have
> been hopeless.

## 3.3 Make the model explain everything

A hypothesis that explains *one* symptom while hand-waving another is
usually wrong. Insist that your model account for **all** the evidence,
including the parts that are inconvenient.

If you see two error messages, your explanation must produce exactly
two, in those places. If a change made no difference, your model must
say why. The moment you catch yourself thinking "that other message is
probably unrelated", treat it as a signal to keep working.

> **In this project**, three explanations for an IDE failure each
> accounted for one of two interrupt messages. The fourth accounted for
> both, and for the failed read, and predicted which message would
> disappear after the fix. That one was right.

## 3.4 Trust positives, distrust negatives

This is the least intuitive rule here, and the most valuable. Evidence
is **not symmetric**.

**A positive observation is strong.** A breakpoint fired: that code ran.
A symbol appears in a trace: it executed. A register holds `0x10`: it
holds `0x10`.

**A negative observation is weak.** "It never ran" is a claim your
instrument is usually not entitled to make. A function can be absent
from a trace while running perfectly, for at least four reasons:

1. **It is a macro, not a function.** No symbol exists, so nothing can
   ever match. In this project `biodone` is `iodone` is
   `io_completed(...)` — tracing the first two found nothing while the
   code worked fine, and produced three separate wrong conclusions.
2. **It was inlined.** The out-of-line copy exists in the symbol table
   but is never called.
3. **It is reached by a different mechanism** than you assume. A system
   call implemented as a trap will never show the message-passing
   symbols you are watching for.
4. **Your instrument is lying.** Breakpoints that do not fire, filters
   that do not match, samples taken at the wrong moment.

> **The rule:** before concluding "X never ran", confirm with `nm` that
> `X` exists as a symbol *and* that it is on the code path you think it
> is. If you cannot confirm both, you have not measured anything.

## 3.5 Ordering is evidence

Console output is a partial execution trace. Use it as one.

```
line 48:  panic: cannot mount root
line 49:  (pager): no space in paging segments; swapon suggested
```

The pager's complaint comes **after** the panic, so it is a consequence,
not a cause — the panic handler tried to write a crash dump, and *that*
is what needed paging space. Reading those two lines in the wrong order
sent this project down a wrong path for a full cycle.

Equally: if a message appears *between* two lines that previously
followed each other directly, **new code ran between them**. That is
often the clearest evidence you will get that a change had an effect.

**But ordering tells you about print order, not causal order.** A fault
that is taken, handled and retried can happen long before the message
describing it appears, and an error printed after a panic may still
describe a condition that existed before it. In this project a pager
error printed after a panic was filed as a consequence on exactly this
reasoning; it later turned out to describe a resource that had been
missing since boot. Use ordering to generate hypotheses, not to close
them.

## 3.5a Check that your evidence is evidence

A string that looks like a message may not be one. In this project a
panic printed `UWVS+`, and its stability across repeated boots was taken
as proof it was real text. It was stable because the binary had not
changed; a rebuild turned it into `UWVS\002k`. The formatting directive
producing it was reading uninitialised memory, and several rounds of
diagnosis had been resting on text that meant nothing.

**Vary something irrelevant and see whether your evidence moves.** If a
value changes when it should not, it is not measuring what you think.
And when an instrument's output is unreadable, fixing the instrument
usually beats working around it -- an unformatted message that names the
failure is worth more than a formatted one that cannot be read.

## 3.5b Prefer a new fact to a new interpretation

When an investigation stalls, notice which kind of step you are taking.
A step that produces a **new fact** -- a value read, a name printed, a
count taken -- moves you forward whatever it shows. A step that produces
a **new interpretation** of facts you already have moves you sideways,
and can do so indefinitely.

In this project one failure was chased through six rounds of the second
kind: six explanations, each plausible, each argued from evidence
already in hand, each wrong. It was then settled in four steps of the
first kind -- read the stack at the fault, decode the error code, print
the argument at the call, grep for the string that appeared. Roughly a
day against half an hour.

The tell is that you are re-reading files you have already read. When
you catch yourself doing it, stop and ask what measurement would
produce a fact you do not yet have.

## 3.6 Count things

Numbers that move under a change are worth more than any amount of
reasoning. When you are unsure whether something is working, find a
countable proxy:

- interrupts per boot
- objects compiled, symbols undefined at link
- entries in a table
- bytes of console output
- how far a trace gets before stopping

> **In this project:** "8 of 1024 dispatch table entries populated"
> located a compiler bug in one reading. "1054 undefined symbols became
> 11" confirmed a fix before anything was run. "Interrupts went 2 → 10"
> proved a driver fix worked.

## 3.7 Distinguish "hung" from "not there yet"

These look identical and are completely different. Before concluding
anything is hung:

```sh
wc -c /tmp/console.log; sleep 60; wc -c /tmp/console.log   # is it moving?
```

Know your system's real speed. Emulation without hardware acceleration
can be five to ten times slower than real time, and an operation that
takes two seconds on hardware can take a minute. If you sample before
the machine could plausibly have got there, you will diagnose a hang
that does not exist — repeatedly, and each time pinned to whatever
function happened to be executing.

## 3.8 Self-audit before you believe yourself

When you think you have the answer, stop. Write down:

1. **What did I actually measure?** Measured, not inferred.
2. **What would I see if I were wrong?** Does my evidence rule that out,
   or merely fit alongside it?
3. **Does this explain all the observations?**
4. **What have I not checked that could invalidate it?**

Writing it out catches things that thinking about it does not. The act
of forming sentences exposes gaps that stay hidden in your head.

## 3.9 Write down the wrong answers

Keep a record of hypotheses you disproved, with the evidence that killed
them. In commit messages, in a working notes file, anywhere durable.

This feels like documenting failure. It is the opposite: it stops you —
or the next person, often you in three weeks — from spending another
cycle on a dead end. A list of things that are *not* the problem is a
genuine asset.

---

# Chapter 4: Reading code you did not write

Old kernels are archaeology. Some habits make it much less painful.

## 4.1 Read the whole text, unfiltered

Resist the urge to filter source through `grep` to make it shorter.
You will filter out the answer.

A real example. This pattern is meant to drop comment continuation
lines:

```sh
grep -vE '^\s*\*'
```

`^\s*\*` matches a comment line ` * foo`. It **also** matches a C
pointer dereference:

```c
	*hostp = bootstrap_master_host_port;      /* silently deleted */
	*devicep = bootstrap_master_device_port;  /* silently deleted */
```

So a function whose body is out-parameter assignments reads as an empty
stub returning success. In this project that produced a confident and
entirely wrong diagnosis — **twice, on the same function.**

Use `sed -n '100,140p' file`, or open it in an editor, and read what is
there.

## 4.2 The comments are load-bearing

In old systems code, comments carry information that exists nowhere
else: why a value is what it is, what a workaround is for, what was
intentionally left undone.

> **In this project:** an "obviously missing" argument block was
> declared a bug with four pieces of supporting evidence. Ten lines
> above it:
>
> ```c
> /*
>  * Allocate space for:
>  *    dummy 0 argument count
>  *    dummy 0 pointer to arguments
>  *    dummy 0 pointer to environment variables
>  */
> ```
>
> The zeros were deliberate. All four evidence items were real and all
> pointed the wrong way, because none of them was the comment stating
> the intent. **I read around the answer rather than through it.**

`XXX`, `TODO`, `HACK` and `/* for now */` are especially worth reading —
they mark places the original authors knew were fragile.

## 4.3 Read the build output

Warnings you have scrolled past for weeks may be the answer.

```
ld: warning: cannot find entry symbol __start; defaulting to 08049000
```

That line explains a program that loads, is scheduled, and dies without
output. It sat unread while four other theories were investigated.

> **A warning telling you your program will start somewhere other than
> where you intended is not a warning. It is an error with bad
> manners.**

Turn warnings up. Read them once properly, decide which are noise, and
then actually look at the rest.

## 4.4 Learn the idioms of the era

Code from the 1980s and 1990s uses conventions that have since
disappeared. Recognising them saves enormous time:

- **K&R function definitions** — parameter types after the parameter
  list
- **Flexible array idiom before C99** — `struct { ... int x[1]; }` where
  the real length is dynamic
- **`extern inline`** with pre-C99 semantics
- **Tentative definitions** relied on being merged by the linker
- **Pointer constants as `case` labels**, casts used as lvalues
- **Assembly written for a specific assembler's** quirks

Each has a modern counterpart that behaves differently — see §6.1.

---

# Chapter 5: Research

## 5.1 Find the closest relative

Almost no system is truly alone. Ancestors, descendants and siblings all
exist, and someone has hit your problem in one of them.

**Rank them by closeness, and be precise about how they relate.** For
each candidate tree, ask:

- Is it the *same code* with different changes? → gold: diff it
- A direct ancestor or descendant? → shows what changed and why
- A sibling from a common ancestor? → informative, but its interfaces
  may have diverged
- An independent reimplementation? → design ideas only

> **In this project**, the tree we work on turned out to be a *copy* of
> another public tree plus our own fixes. `diff -rq` between them showed
> exactly eleven differing files, which were exactly our changes. That
> one command established provenance, and made that tree's userland a
> worked example written against our exact kernel rather than an
> analogy.
>
> By contrast, a superficially similar project turned out to target a
> different Mach lineage whose interfaces had diverged. Its code would
> not have worked, and checking saved us from adopting it.

**How to tell quickly.** Pick an interface you know is version-sensitive
and see which spelling each tree uses. In this project, one function's
presence or absence cleanly separated two Mach lineages across four
different repositories.

## 5.2 Licence discipline

Reference trees frequently have licences incompatible with yours. The
rule is simple and worth following strictly:

**Read to understand the design. Never copy the expression.**

Copyright protects expression, not method. Learning *that* a driver must
acknowledge an interrupt before issuing the next command is a fact about
hardware. Copying twenty lines of someone's implementation is not.

Check the file header before using anything, and record in your own
notes which trees are read-only and why. Practically:

- Note the licence of every reference tree in your project docs
- If you consult a file, say so in your commit message
- When you implement something after reading a reference, write it from
  the interface documentation, not with their source open

## 5.3 Read the hardware documentation

For driver bugs, the specification is often faster than the source. A
1995 driver works on 1995 hardware; when it fails on an emulator, the
question is usually "what does the spec require that the real chip
tolerated?"

Emulators implement specifications. Real hardware implemented
specifications *loosely*. That asymmetry is the source of an enormous
fraction of retro-computing driver bugs, and the spec tells you which
way to look.

> **In this project**, a floppy driver hung forever. The answer was in
> the controller datasheet: after a reset, the chip requires its
> interrupt to be acknowledged before it will accept another command.
> The driver never did it. Real hardware apparently did not care; the
> emulator does. No amount of reading the driver would have revealed
> this — the driver looked correct, because it *was* correct for the
> hardware it was written for.

---

# Chapter 6: Getting unstuck

## 6.1 Know the catalogue of era-gap failures

When old code meets modern tools, the failures are not random. They come
from a small catalogue, and recognising one on sight converts an hour
into a minute.

**Compiler and language:**

| Symptom | Cause | Remedy |
|---|---|---|
| Loop runs once when it should iterate | Compiler took a declared array bound literally and proved the loop dead | Access through a pointer, or fix the declaration |
| Code behaves differently at `-O2` than `-O0` | Modern optimiser exploits undefined behaviour the original never triggered | Find the UB; disabling optimisation is a diagnosis, not a fix |
| Interrupt or trap handler corrupts its own frame | Tail-call/sibling-call optimisation reusing the stack frame | Disable that optimisation for the function |
| `asm operand has impossible constraints` at `-O2` but not `-O0` | A constraint bug, not register pressure: usually a register named as both an input and a clobber | Make it a read-write operand -- an early-clobber output tied to a matching input |
| K&R definitions rejected as errors | Recent compilers made implicit-int an error | `-std=gnu89` |
| `extern __inline__` gives duplicate symbols | C99 inline semantics differ from GNU89 | `-fgnu89-inline` |
| "multiple definition" for variables in headers | Compilers default to `-fno-common` now | `-fcommon` |
| Pointer constant used as a `case` label | Was legal, now requires an integer constant | Cast it |
| A cast used as an assignment target | Was legal in some compilers, never standard | Rewrite the expression |
| `pasting "x" and "y" does not give a valid preprocessing token` | `##` applied to two string literals. Never valid, but tolerated by old preprocessors | Delete the `##`; adjacent string literals concatenate on their own |

**Assembler and linker:**

| Symptom | Cause |
|---|---|
| A valid-looking instruction is rejected | Modern assemblers are stricter about operand/suffix width |
| Program starts executing garbage | Entry symbol not found; linker silently defaulted |
| Symbols undefined that obviously exist | Name decoration differs — leading underscore conventions, a.out versus ELF |
| Wrong architecture at link | Toolchain defaulting to 64-bit; the flag differs between compiler and linker |

**Shell and build:**

| Symptom | Cause |
|---|---|
| Generated file has a real newline where `\n` was intended | `/bin/sh` is `dash`, whose `echo` interprets escapes; use `printf` |
| `missing terminating " character`, often with `invalid suffix "f" on integer constant` | A multi-line string literal written with raw newlines -- common in old inline assembly. End each line `\n\`. The odd second error is an assembler label like `1f` being parsed as C once the string breaks |
| Generated file's C is malformed | Generator written for a pre-ANSI compiler's tolerances |
| An `awk` script aborts | Modern `awk` variants reject things the original tolerated |
| Configure or build tool fails oddly | 1990s autoconf against a modern shell |

**Hardware and emulation:**

| Symptom | Cause |
|---|---|
| A device responds once then hangs | A pending interrupt or status condition the driver never acknowledged |
| A BIOS-provided value reads as zero | You booted the kernel directly and skipped the BIOS that would have set it |
| Spurious interrupts | Polled commands in an interrupt-driven driver; the interrupt controller latched one anyway |
| Works on hardware, fails on emulator | The emulator implements the spec; the hardware was forgiving |

| Code inside `#if somename` compiles that never used to | The compiler predefines `somename`. GCC defines `linux`, `unix`, `i386` and others in its GNU dialects; in K&R C an undefined identifier in `#if` is 0, so such blocks were silently excluded | `-Uname`, or a stricter `-std=` |

> **The unifying idea:** most bugs in old code are not logic errors. They
> are **the world having moved**. Ask "what changed underneath this?"
> before "what is wrong with this?"

> **A warning about your own fixes.** Each flag you add to accommodate
> old code changes the compilation environment, and can switch on code
> paths that have been dormant for decades. In this project `-std=gnu89`
> was added so GCC would accept K&R function definitions; it also
> predefines `linux=1`, which enabled a block guarded by `#if linux` and
> produced a page of errors in Linux-only code that had been correctly
> excluded since 1995. When a new failure appears immediately after you
> change flags, suspect the flags first.

## 6.2 When you are genuinely stuck

Signs: three hypotheses disproved in a row; re-reading the same file;
each cycle costing more than the last; catching yourself saying "it must
be...".

Things that work, roughly in order of how often:

**Check whether the code you are debugging runs at all.** Extremely
cheap, and it collapses whole investigations. If four hypotheses all
assume a function is executing and failing, and it never executes, all
four die at once.

**Go one layer out.** If the server is broken, is it loaded correctly?
If the read fails, does the device open? The bug is often not in the
layer you are staring at, and layers are cheap to test individually.

**Look for the same pattern elsewhere in the tree.** Bugs of a kind
cluster, because they came from one cause. Once you have found one
instance of "the original authors disconnected this", look for others.
In this project that pattern appeared nine times.

**Ask what the code was written for.** Not what it does — what world it
assumed. Then ask what has changed about that world.

**Count something you have not counted yet.** A number that moves is
worth more than a page of reasoning.

**Reread the failure from the beginning.** Not the part you have been
staring at — all of it, including the boring startup lines. The answer
is often in output you stopped reading days ago.

**Explain it to someone.** Or to a file. Forming sentences exposes gaps
that survive indefinitely as vague thoughts.

## 6.3 Making hunches, honestly

Experienced kernel people appear to guess well. What they are actually
doing is pattern-matching against a catalogue like §6.1, plus three
questions:

1. **What is the last thing that definitely worked?** The bug is between
   there and the symptom.
2. **What changed?** In your code, your toolchain, your environment, or
   the world since the code was written.
3. **What does this failure resemble?** Not in this system — in any
   system. "Device responds once then stops" has a small number of
   causes anywhere it occurs.

A hunch is a hypothesis with no evidence yet. That is fine — it is a
starting point, not a conclusion. The discipline is what you do next:
**design the measurement that would prove it wrong**, and run that
before you get attached to it.

---

# Chapter 7: Worked examples

Five real investigations, with the wrong turns kept in. The wrong turns
are the point.

## 7.1 When the compiler is right and the code is wrong

**Symptom.** Every kernel service call failed with "bad message ID".
Nothing in userland worked at all.

**Hypothesis 1.** The dispatch table is never built — the initialisation
function is not being called.

**Measurement.** Count populated entries in the table. Result: **8 of
1024**. So the table *was* being built, just almost empty. The
hypothesis died in one reading, and the number pointed straight at the
loop that fills it.

**The code:**

```c
for (j = 0; j < subsystem->end - subsystem->start; j++)
    if (subsystem->routine[j].stub_routine) { ... }
```

**The cause.** `routine` is declared `routine[1]` — the pre-C99 flexible
array idiom, where the real length is allocated dynamically. A modern
compiler takes that bound literally, proves `j < 1`, and collapses the
loop to a single iteration. The 1995 compiler did not do this analysis.

**The fix.** Read through a pointer, which defeats the bound analysis:

```c
rd = subsystem->routine;
if (rd[j].stub_routine) { ... }
```

**Confirmation.** 8 entries became 196.

**What to take from it.** When a loop misbehaves, ask what the compiler
can *prove* about it. Modern optimisers reason far more aggressively
than old ones, and pre-C99 idioms are a rich source of things they can
now prove that the author did not intend. And note that counting found
this in one step, where reading the loop would not have.

## 7.2 When the answer is in the datasheet

**Symptom.** Opening the floppy device never returned. Exactly two
interrupts from the controller per boot, then silence forever.

**Three wrong hypotheses, each killed by one memory read:**

| Hypothesis | Measurement | Result |
|---|---|---|
| The wakeup targets the wrong address | Read both pointers | They were equal |
| The `switch` on a flags field does not match | Read the flags variable | Exactly the expected value |
| Interrupts are masked | Read interrupt level, CPU flags, controller mask | All enabled |

Attaching to the hung machine and reading globals made each of these a
two-minute question.

**The measurement that mattered.** At the hang, the command's flag field
still held its "waiting" bit. The interrupt handler's *first action* in
that branch is to clear that bit. Therefore **the handler had never
taken that branch** — meaning no interrupt had arrived after the command
was issued.

That reframed the question from "why isn't the wakeup working?" to "why
isn't the device interrupting?" — a hardware question, not a software
one.

**The answer, from the controller's datasheet.** After a reset, this
chip asserts an interrupt and will not accept another command until that
interrupt is acknowledged with a specific status command — up to four
times. The driver never did it, because its interrupt handler switches
on a state variable that is zero at reset time and matches no case.

**The fix.** Issue the acknowledgement four times at the end of the
reset routine.

**Confirmation.** Interrupts went from 2 to 10; every function that had
never been reached started running.

**What to take from it.** The driver *looked* correct, because it *was*
correct for hardware that tolerated the omission. When software looks
right and hardware is unresponsive, read what the hardware requires.

## 7.3 When four hypotheses share one wrong assumption

**Symptom.** A 953 KB server was loaded from disk, started, and
terminated immediately with no output whatsoever.

**Four hypotheses, all investigated, all wrong:**

1. A service the server calls at startup is an unimplemented stub →
   **wrong**, and wrong because of the `grep -vE '^\s*\*'` filter from
   §4.1, which deleted the function's body from my view
2. It dereferences a null argument vector → wrong; every use is guarded
3. Its startup code skips thread initialisation on one path → wrong;
   both paths initialise
4. The loader fails to pass arguments → wrong; the zeros are deliberate
   (§4.2)

Each cost a cycle. Each was plausible. Each was wrong.

**The measurement that ended it.** An execution trace, user-mode
addresses symbolised against `nm`:

> **Zero** instructions executed above the *previous* program's end
> address, and the highest address reached in the entire run was below
> it — while this server's code extended far past it.

The server had never executed a single instruction of its own code.
That one number invalidated all four hypotheses simultaneously, because
every one of them assumed it was running and failing somewhere.

**The cause, in two commands:**

```sh
readelf -h server | grep -i entry     # 0x8049000
nm server | grep ' 08049000'          # ip_setmoptions.cold
```

The entry point was the first byte of `.text` — a cold-path fragment of
an unrelated networking function. The server is linked expecting an
entry symbol that its startup library spells differently, so the linker
had defaulted, and had said so:

```
ld: warning: cannot find entry symbol __start; defaulting to 08049000
```

**The fix.** One linker flag aliasing the expected name to the real one.

**What to take from it.** Three things. Check the entry point before
booting (§1.5). Read the build warnings (§4.3). And when several
hypotheses are all disproved, stop generating more — find the
**assumption they share** and test that instead. Here it was "the
program is running at all".

## 7.4 When the model must explain everything

**Symptom.** A disk was detected with correct geometry, but mounting the
filesystem failed. Exactly two "false interrupt" messages appeared: one
during device probe, one immediately before the failure.

**How the evidence constrained the answer.** Two messages, at two
specific moments, plus a read that never delivered data. Any correct
explanation had to produce *exactly two*, in *those places*, and account
for the failed read. That ruled out several ideas immediately.

**A wrong turn worth keeping.** The first explanation was "these polled
commands never acknowledge their interrupts". Then I read the polling
routine:

```c
while (--n && inb(STATUS_PORT(addr)) & STATUS_BUSY);
```

Reading the ATA status register *does* clear the device's interrupt. So
the acknowledgement happens — **at the drive**. But the interrupt
controller has already latched the request, so the interrupt is still
delivered to the CPU. Getting that distinction right mattered: it ruled
out "read the status register afterwards" as a fix.

**The model that fit everything:**

```
probe:  IDENTIFY issued and polled → interrupt latched by the PIC
        → delivered to an idle driver → "false interrupt" #1, harmless

read:   the start routine calls set-parameters, polled → IRQ latched
        ... then the busy flag is set, and the real read is issued ...
        → the latched IRQ is delivered, and the handler attributes it
          to the READ: runs the data path with no data ready, completes
          the buffer, clears the busy flag
        → the read's own interrupt then arrives at an idle driver
        → "false interrupt" #2, and no data was ever transferred
```

Two messages, both positions, and a failed read. The structural detail
that made it work — the set-parameters command runs *before* the busy
flag is set — was visible in ten lines of the start routine.

**The fix.** Disable device interrupts around both polled commands,
using the control register bit provided for exactly this purpose. This
is the standard way to issue a polled command from an interrupt-driven
driver.

**Confirmation.** False interrupts went from 2 to 1, and the survivor
was the harmless probe-time one. The one that mattered was gone, and the
failure moved on to the next layer.

**What to take from it.** The earlier explanations each accounted for
one message and waved at the other. That should have been the signal to
keep going. Insist on a model that explains *all* of it — and notice
that this model also made a *prediction* (which message would survive),
which the fix then confirmed.

## 7.5 When your instrument lies about time

**Symptom.** A long series of "it hangs in X" conclusions, for many
different values of X, none of which held up.

**The cause.** The development environment had no hardware
virtualisation. Under pure software emulation the guest ran at roughly
**one sixth of wall-clock speed** — measured directly, by counting timer
interrupts against elapsed real time. A boot that took a few minutes on
an accelerated host took thirty-five minutes there.

Every console check made at twenty or forty seconds looked frozen and
was simply **too early**. Each produced a confident diagnosis pinned to
whatever function happened to be executing when the sample was taken.

**The fix was not technical.** It was noticing that "hung" and "not
there yet" are indistinguishable without an explicit check:

```sh
wc -c /tmp/console.log; sleep 60; wc -c /tmp/console.log
```

**What to take from it.** Measure your environment's real speed once, in
a unit you can reason about (guest seconds per wall-clock second), and
remember it. Then design your waits around it. And treat "nothing has
happened" as a claim requiring evidence, exactly like any other negative
observation (§3.4).

---

# Chapter 8: The short version

Pin this somewhere visible.

**Before you start**

1. Get a reliable output channel first. Serial console, not video.
2. Know how fast your environment actually runs.

**When something fails**

3. Bisect layers, not lines. Which layer, then recurse.
4. Form one hypothesis. Design the measurement that would **disprove**
   it.
5. Count something. Numbers that move beat reasoning.
6. Check the code you are debugging runs at all.

**When reading evidence**

7. Trust positives. Distrust negatives — confirm the symbol exists and
   is on the path.
8. Ordering is evidence. A message after a panic is a consequence.
9. Your model must explain **everything**, not just the symptom you
   started with.

**When reading code**

10. Read it unfiltered. Filters eat the answer.
11. The comments are load-bearing. So are the build warnings.
12. Check the binary is shaped right: `readelf -h`, `nm`, `strings`.

**When stuck**

13. Most old-code bugs are the world having moved, not logic errors.
14. Find the closest relative tree and diff against it.
15. For driver bugs, read the datasheet, not just the driver.

**Always**

16. Self-audit in writing before you believe yourself.
17. Write down the wrong answers. They are worth as much as the right
    ones.

---

*The honest summary of this project: roughly fifteen confident diagnoses
turned out to be wrong, and every one was corrected by a measurement
rather than by more thinking. The method above is mostly machinery for
finding out you are wrong quickly and cheaply. Someone who is wrong ten
times an hour and finds out within three minutes each time will make
faster progress than someone who is wrong twice and spends a day on
each. That is the skill, and it is learnable in a way that being right
is not.*
