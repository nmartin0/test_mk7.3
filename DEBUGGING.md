# Debugging methodology

How to find out why this kernel does not do what you expect.

This is written for an AI agent picking the project up cold. It is not
general advice — it is what worked and what failed on **this** kernel,
under **this** emulator, with the actual commands. Every "do not" below
is something that already cost real time here.

The single theme: **measure, do not infer.** Most of the mistakes
recorded here were correct-sounding inferences from an instrument that
was lying, or that was pointed at the wrong thing.

---

## 0. Is the failure deterministic? Check before anything else

**Run the same boot three times and compare the failure.** If the values
differ, every technique below changes meaning, because you can no longer
compare a measurement from one run against a measurement from another.

The `splx` panic on this branch reports a different value every boot:
`0x91`, `0x57`, `0x3f`, `0x18`, `0x4b`, `0x17`. That was not noticed for
several rounds, and it produced the most expensive wrong turn in the
project:

- `%ebx` was measured as `8` at the call site — correct, in that run.
- `%eax` was measured as `0x91` at the panic — correct, in a *different*
  run.
- Comparing the two produced a conclusion that a stack slot was being
  corrupted between two adjacent instructions, which is impossible and
  wasted three rounds, including an interrupt hypothesis that the
  evidence later contradicted outright.

**Rule: on a non-deterministic failure, every value in a chain of
reasoning must come from a single stopped guest.** Anchor on a
breakpoint, then read everything you need before continuing. Never
assemble an argument from values captured in separate runs.

If you must compare across runs, compare *distributions*, not values,
and say that is what you are doing.

## 1. Look at the right output device

OSFMK's AT386 console is `kd` — the VGA text screen and the AT keyboard.
`cninit()` lives in `i386/AT386/kd.c`. **There is no serial console
option.** Do not pass `-serial stdio` and conclude from silence that the
kernel is hung. It printed its banner to VGA for an entire session while
that conclusion was being drawn.

Read the VGA text buffer, 80x25, two bytes per cell (character,
attribute) -- but **check both framebuffer addresses**:

```
0xb8000   colour text mode, the usual one
0xa0000   the graphics window
```

`kd_xga_init` probes the adapter during `cninit()`, and this kernel ends
up writing to **0xa0000**. A reader hardcoded to `0xb8000` reports a
blank screen for a kernel that is printing perfectly well. That happened
here and looked exactly like a regression caused by the previous commit;
a wider `pmemsave` and a search for the banner text found the output
sitting at `0xa03c0`. `tools/vgadump.py` now tries both and reports
which one it used.

**Read it through the QEMU monitor, not gdb.** `pmemsave` takes a
**guest physical** address. gdb's `dump binary memory` takes a **guest
virtual** address, and once the kernel enables paging the two differ:
`0xB8000` becomes unreadable and `0xC00B8000` returns zeros. The monitor
sidesteps paging entirely.

```sh
qemu-system-i386 -kernel mach_kernel.PRODUCTION -display none \
    -no-reboot -m 64 -monitor unix:/tmp/mon,server,nowait &
python3 tools/vgadump.py /tmp/mon /tmp/vga.bin 8
```

If both addresses come back empty, do not conclude the kernel is silent
until you have searched memory for the text:

```sh
# in the monitor: pmemsave 0 0x400000 "/tmp/mem.bin"
python3 -c "d=open('/tmp/mem.bin','rb').read(); print(hex(d.find(b'Mach 3.0')))"
```

The console characters are interleaved with attribute bytes, so search
for the plain string first (it will find the *format string* in .data)
and then for the interleaved form to find the framebuffer itself.

**Always validate the reader against a known-good kernel before trusting
a blank result.** A build that is known to print its banner is the
control. A blank screen from an unvalidated reader means nothing.

---

## 2. Trace first, single-step last

`-d exec` logs every translated block with the guest EIP. One run
answers "where does it end up" with no guessing:

```sh
qemu-system-i386 -kernel mach_kernel.PRODUCTION -display none \
    -no-reboot -m 64 -d exec -D /tmp/exec.log
```

The guest EIP is the second bracketed field:

```
Trace 0: 0x7f36... [c0000000/00000000c0101005/000000f0/ff020000]
                                    ^^^^^^^^ guest EIP
```

Extract with `re.findall(rb'/00000000([0-9a-f]{8})/', data)` and map to
symbols with `nm -n`. Subtract `0xC0000000` first: the kernel links at
`0x100000` but runs from its high-half mapping.

Two things this gives you that breakpoints do not:

- **The set of functions ever entered.** Deduplicate the EIP stream by
  first occurrence and print the tail. That is literally "how far did
  startup get", and it found the real stopping point in one run after
  breakpoints had been misleading for an hour.
- **The last distinct EIPs**, which distinguishes a tight loop from a
  halt.

`-d int` logs exceptions and CPU resets. Zero `v=` lines means the
kernel is **not faulting** — it is looping or halting deliberately.
Check this before hunting for a fault that isn't there.

### Do not conclude "stuck" from `stepi`

**`stepi` on a `rep` instruction executes one iteration and leaves `pc`
unchanged.** A loop that breaks when the pc repeats will fire on a
perfectly healthy `rep movsb`. This produced three separate false "it is
stuck here" diagnoses:

- `rep movsb` copying the multiboot info — working
- `rep stos` zeroing a page table with `ecx = 0x400`, against a
  detector threshold of 800 iterations — working
- and the conclusion that a hang existed at all

If you must detect a spin by stepping, watch `%ecx` or the full
(pc, regs) tuple, not `pc` alone. Better: use `-d exec`.

### Do not breakpoint a symbol without checking it is on the path

`vstart` was breakpointed and never hit, which read as a hang. It was
never going to be hit: `multiboot_entry` jumps to `fix_desc_common` at
`0x1002d0`, **past** `vstart` at `0x100289`. Disassemble the path before
deciding a missed breakpoint means anything.

---

## 2a. The kernel is relocated by SEGMENTATION, not just paging

This wasted more time than any other single misunderstanding, so it gets
its own section.

`-d exec` shows the segment base explicitly:

```
Trace 0: 0x7f36... [c0000000/00000000c0101005/000000f0/ff020000]
                    ^^^^^^^^ cs_base    ^^^^^^^^ linear pc
```

`cs_base = 0xC0000000`. The CPU's `EIP` is the **low** value
(`0x101005`); the linear address is `0xC0000000 + EIP`. `start.S`
arranges this at `0x100252` by copying PDE[768] into PDE[0], so the low
4 MB is identity-mapped and code can keep running at low `EIP` until the
`lgdt`/`ljmp` installs high-based segments.

Consequences:

- **Breakpoints in kernel C code need the LINEAR address.**
  `break *0xc01706f0` for `machine_startup`, not `break *0x1706f0`.
  A breakpoint set at the low address simply never fires, which reads
  exactly like "the kernel never gets there" and is not.
- **Memory reads work at either address**, since both map to the same
  physical page. So a read succeeding tells you nothing about which
  addressing you are using.
- `nm` prints link addresses (low). Add `0xC0000000` before setting a
  breakpoint; do not add it when reading memory *through gdb*.
- **`ESP` and other pointers in registers are segment offsets too.** The
  data segments have the same `0xC0000000` base as `CS`. An `ESP` of
  `0x08b78f1c` is linear `0xC8B78F1C`. Reading the raw value through the
  QEMU monitor returns "Cannot access memory" and looks like an
  unmapped stack; adding the segment base makes it readable. This cost
  several rounds, during which the stack was believed to be corrupt.

## 3. gdb, when you do need it

Attach with `-s -S`, then **`target remote` first and `symbol-file`
after**. Loading the file before connecting puts gdb in a state where
`continue` reports "Selected thread is running" and batch scripts fail.

```
target remote :1234
symbol-file mach_kernel.PRODUCTION
break *0xc0101005
continue
```

Breakpoint addresses must be **virtual** (`0xc01704b0`), not the
physical address `nm` prints (`0x1704b0`). Reading globals also works
better by address than by name: gdb frequently reports
`'cnvmem' has unknown type; cast it to its declared type`, and
`x/1dw &sym` sidesteps it.

Asynchronous `interrupt` after `continue &` does not work reliably in
batch mode here. Use a breakpoint you know will be hit instead.

**Hardware watchpoints do work**, unlike conditions, and they catch
writes through computed addresses that grepping the disassembly cannot:

```
(gdb) watch *(unsigned int*)0x1e0a08
```

They report EIP *after* the storing instruction. This found `bzero`
writing a variable via `rep stos`, which no search for direct stores to
that address would have shown. But they have only proved reliable for
the first few hits: a loop of 25 `continue`s produced nothing at all,
twice. Use them to answer "what writes this, early", validate against a
known write first, and do not yet trust them to scan.

**`-d exec` counts are not execution counts.** Measured against
breakpoints in the same build:

| function | trace said | breakpoints show |
|---|---|---|
| `set_spl` | 7 | thousands |
| `splx` | 1574 | >4000 |
| `install_special_handler` | 1 | at least 22 |

Two causes. A **fallthrough** from one function into the next does not
start a new translated block, so only `call`-entries are counted; `splx`
falls through into `set_spl`. And QEMU **chains** translated blocks, so
a block re-executed through a chain is not re-logged.

Use `-d exec` for "was this reached" and for the *order* of first entry.
Never use it for "how many times", to size a brute-force search, or to
conclude something runs only once. That last error was made here and
three separate conclusions were built on it.

**Breakpoint conditions do not work at all.** `break *ADDR if $eax != 8`
stops with `$eax == 8`; the condition is ignored and the breakpoint
behaves as unconditional. This silently produces wrong answers rather
than an error, so never filter with a condition -- count and filter with
a `-d exec` trace instead.

**gdb often cannot read kernel data at a breakpoint** even when reading
registers works. Both the link address and the linear address return
"Cannot access memory". The technique that does work is to combine the
two interfaces: run QEMU with `-s -S` *and* a monitor socket, break in
gdb, then `shell` out to a script that issues `pmemsave` while the guest
is stopped. `pmemsave` takes a guest **physical** address, so paging and
segmentation do not enter into it.

```
(gdb) shell python3 tools/pmem.py  /tmp/mon 0x1e0a08  4   # PHYSICAL
(gdb) shell python3 tools/vmem.py  /tmp/mon 0xc8b78f1c 8  # LINEAR
```

Two readers, because the monitor has two addressing modes and you need
both:

| tool | monitor cmd | address space | use for |
|---|---|---|---|
| `tools/pmem.py` | `pmemsave` | guest **physical** | globals whose link address you know, framebuffers |
| `tools/vmem.py` | `x/Nxw` | guest **linear** | stacks and anything reached through a register |

For a register-derived address, add the segment base first: linear =
register + `0xC0000000`.

The monitor echoes input with readline escape sequences before the
reply, so a naive reader captures only the echo. Drain the socket for a
few seconds and strip `\x1b[...` before parsing; `tools/vmem.py` does
this.

---

## 4. Measure in a clean tree

**Build into a fresh `MK_BUILD` before believing any number.** A reused
build directory reports progress a clean checkout cannot reproduce.

Worse, check the *source* tree too. An early working copy had 160
generated files under `src/mach_kernel/PRODUCTION` committed into what
was supposed to be a pristine vendor import, because the tree had been
built in before it was committed. A stale
`PRODUCTION/mach/memory_object.h` there shadowed the real source header
and produced a failure that does not exist in a clean checkout — which
led to a wrong object count being published in a commit message and then
"corrected" to another wrong number.

```sh
git ls-files 'osfmk7.3/**/PRODUCTION/*' | wc -l     # must be 0
```

When a measurement matters, take it in a fresh clone of the pushed
repository. It is public; there is no reason to measure anywhere else.

---

## 5. Distinguish "the toolchain refuses" from "the code is wrong"

Two different failure classes need two different responses.

**Build failures are usually configuration.** This tree was written to
be portable across a.out and ELF, K&R and ANSI, and several assemblers.
It usually has a conditional for whatever you are hitting; the flag is
just not being passed. Before editing any 1995 source, find the switch.
Examples that cost time before the switch was found:

- Six `.S` files failed with *"bad or irreducible absolute expression"*.
  `ALIGN` is defined inside `#ifdef ASSEMBLER` in `i386/asm.h`, and the
  `.S.o` rule passes `-DASSEMBLER`. Not a source bug.
- Symbols came out with a leading underscore. `i386/asm.h` carries both
  a.out and ELF conventions selected on `__NO_UNDERSCORES__`, which
  `osc/Buildconf` sets for exactly the i386-on-Linux case. Not a source
  bug.

**Read `osfmk7.3/osfmk/src/osc/Buildconf` before touching build
settings.** It is OSF's own ODE configuration and already supports an
i386 target on a Linux host.

**When a source change is genuinely required, prove equivalence rather
than arguing it.** Assemble both forms and compare bytes:

```
.byte 0x66; inl  %dx,%eax  ->  66 ed   in  (%dx),%ax
inw  %dx,%ax               ->  66 ed   in  (%dx),%ax
```

That evidence belongs in the commit message.

**And check whether a configuration escape exists first.** For the
assembler suffix/register mismatches, every `-m` option gas has was
tested; none accepts the 1995 form, and `-mold-gcc` no longer exists.
Only then was the source edited.

---

## 6. Enumerate before proposing

Three fixes were proposed here on partial information and had to be
withdrawn:

- A global `-fno-zero-initialized-in-bss` to rescue one variable. It
  relocated ~20 KB of objects and silenced the console.
- Then a section attribute on `mb_info` alone — still wrong, because
  `parse_multiboot()` writes **nine** variables, not one, and the
  attribute would have rescued only the one that had been looked at.
- Only after listing all nine did the actual defect appear: the BSS
  clear ran three calls *after* the function whose output it was
  erasing. An ordering bug, fixed by moving one statement.

The rule: **before proposing, enumerate everything in the same class.**
List every variable the function writes. List every suffix/width
mismatch in every assembly file, not the two the assembler happened to
report. Read the call order before theorising about placement.

A scan is cheap; a withdrawn patch is not.

---

## 7. Check whether the "fix" is even reachable

`bootstrap_create_old()` was analysed as a candidate path. It compiles
clean, links clean, and every symbol it calls exists. It also **never
runs**, because the kernel stops in `Switch_context` long before
`startup.c:517`.

Before investing in a function, confirm it executes. `-d exec` plus the
first-occurrence set from §2 answers this in one run:

```
did the bootstrap path run?
  bootstrap_create_old   no
  user_bootstrap         no
  task_create_local      no
```

---

## 7a. Tail calls hide the caller

GCC turns `... ; splx(s); }` into a tail jump:

```asm
10cbde:  mov  %ebx,0x10(%esp)
10cbe2:  add  $0x4,%esp
10cbe5:  pop  %ebx
10cbe6:  pop  %esi
10cbe7:  jmp  159df2 <splx>
```

Two consequences that both caused wrong conclusions here:

- **The return address on the stack belongs to the caller's caller.**
  At `splx`, `[esp]` was `0x10cc39`, which is inside `thread_hold` —
  the return address from `thread_hold`'s `call install_special_handler`.
  It is tempting to read that as "`thread_hold` called `splx`". It did
  not; `install_special_handler` tail-jumped there and left the frame
  in place.
- **`-d exec` block adjacency is not a call relationship.** Blocks
  logged next to each other may be a tail jump, a fallthrough into the
  next function, or a branch taken inside one block. On this branch
  `splx+0` appearing immediately before `splxpanic+0` was read as "this
  invocation panicked", which was right, but the same adjacency was
  *also* read as proof about which caller was responsible, which was
  wrong.

To identify a caller reliably: read `[esp]` at the callee's entry, map
it with `nm`, and then **disassemble that address** to see whether it is
a return site from a `call` — and if so, a call to *what*. Do not assume
the instruction at a return address is the call itself; it is the
instruction after it.

## 8. Negative and positive controls

Never claim a change was necessary without removing it and watching the
failure return. Each of the three OSFMK source changes was individually
reverted and rebuilt:

| reverted      | objects | kernel produced |
|---------------|---------|-----------------|
| `pio.h`       | 131     | no              |
| `locore.S`    | 204     | no              |
| `i386_rpc.c`  | 122     | no              |
| none          | 206     | yes             |

The last row is the positive control and matters as much as the others.

---

## 9. Instrument hygiene

- **`pkill -f qemu` matches your own shell's command line** and kills
  the session mid-command. Use `pkill -x qemu-system-i386`.
- Verify a compiler shim actually takes effect before trusting a
  negative result. A PATH shim intended to force gcc-14 silently never
  applied, which produced a confident and wrong "not reproducible here".
  Repointing `/usr/bin/cc` and `/usr/bin/gcc` reproduced the failure
  immediately.
- `ln -sfn target existing_dir/` creates the link *inside* the
  directory, not in place of it. This silently produced 3 KB MIG stubs
  instead of 150 KB and looked like success.
- A generated MIG header can shadow a real source header of the same
  name. `mach/memory_object.h` exists in the source tree; a generated
  one earlier on the `-I` path broke ~150 objects at once and presented
  as a type error.

---

## 10. Patch hygiene

Generate patches against **the commit the maintainer last confirmed
pushed**, not against your own `HEAD~1`. The two diverge the moment a
patch is questioned instead of applied, and `git am` fails on a single
line of mismatched context. Four patches in a row had to be regenerated
before this was taken seriously.

The repository is public. Clone it, apply there, build there, and only
then send:

```sh
git clone https://github.com/nmartin0/test_mk7.3.git /tmp/verify
cd /tmp/verify && git am /path/to/the.patch && <build>
```

A failed `git am` leaves `.git/rebase-apply` behind and **every
subsequent `git am` fails until `git am --abort`**, with an error that
does not mention the real cause.

---

## 11. What good evidence looks like

A claim is ready to act on when it has a measurement attached:

- not "mostly compiles" but "206 of 206 objects, kernel 1,021,568 bytes"
- not "the encodings are equivalent" but `66 ed` / `66 ed`
- not "it hangs here" but a `-d exec` trace showing the last distinct
  EIPs and the set of functions entered
- not "this flag is needed" but the build output with and without it

If a number cannot be produced, say that plainly instead of reaching for
an adjective.
