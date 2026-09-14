# ARCHIVE: the splx panic investigation

**This problem is SOLVED.** See the commit "i386/hardclock.c: stop GCC
rewriting the interrupt frame via a sibling call". The current state of
the project is in `docs/current-blocker.md`.

This file is kept because it records, in order, every hypothesis that
was eliminated and every instrument trap that was found. Most of those
traps are still live and will catch the next person. The narrative also
shows several confident conclusions being retracted, which is the
honest shape of this kind of work.

**Read the sections on instrument failures before debugging anything
here.** The most expensive were: `pkill` never matching the process,
only one breakpoint servicing at a time, breakpoint conditions silently
doing nothing, and `-d exec` counts not being execution counts.

---

# Current blocker: the first page of .text is zeroed during early boot

**Status: open, narrowed to a ~200 byte window. Not yet fixed.**

Written for whoever picks this up next. Everything below was measured,
not inferred; the commands are in `DEBUGGING.md`.

---

## The symptom

The kernel boots, sizes memory correctly, prints its banner, and stops:

```
Kernel virtual space from 0x0 to 0x40000000.
Available physical space from 0x101000 to 0x3fe0000
Mach 3.0 VERSION(PMK1.1): ... mach_kernel/PRODUCTION (vm)
```

Then nothing. It does **not** fault — `-d int` reports zero exceptions.
It executes about 4.8 million basic blocks and cycles forever between
three addresses.

## What is actually happening

A `-d exec` trace shows the kernel enters exactly **55 distinct
functions** and stops here:

```
setup_main
  printf_init
  panic_init
  sched_init
    pset_sys_bootstrap
      pset_init
        setbit          <- last sane block
Switch_context+56       <- control lands mid-function
testdev_generate_replies.cold+3
return_xfer_stack+19
syscall_native+47
set_tr+7
```

Those last symbols are not a call chain. They are unrelated functions at
ascending addresses, which is the signature of the CPU running *through*
memory rather than calling anything.

`pset_init` calls `setbit(31, &pset->...)`:

```asm
11c74d:  push %esi
11c74e:  push $0x1f
11c750:  call 100ef2 <setbit>
11c755:  mov  %ebx,%eax        <- where the ret should land
```

`setbit` as linked is four instructions and a clean `ret`:

```asm
100ef2:  8b 4c 24 04   mov 0x4(%esp),%ecx
100ef6:  8b 44 24 08   mov 0x8(%esp),%eax
100efa:  0f ab 08      bts %ecx,(%eax)
100efd:  c3            ret
```

**But that code is not in memory when it runs.** Read from the guest at
`setup_main` entry, `0xc0100ef2` is all zero bytes. Stepping through
`setbit` confirms it: gdb advances two bytes at a time, not four, and
`%ecx` is 0 where the stack argument says `0x1f`.

So the CPU executes a sled of `00 00` (`add %al,(%eax)`), which both
corrupts memory as it goes and eventually lands in `Switch_context`'s
middle with a garbage stack. The registers captured there make this
unambiguous:

```
esp = 0x000fc103   edi = 0x000fe103   esi = 0x000ff103   eip = 0x00100103
```

Each 4-byte slot 0x1000 higher than the last, low bits `0x103`. Those
are not saved registers — they are **page table entries** (`P|RW|G`,
frames 0xfc000, 0xfe000, 0xff000, 0x100000) being read as thread state.

## When the page dies

Bisected with breakpoints, reading `0x100ef2` at each point. The window
has been closed to a single function:

| point | bytes at 0x100ef2 | intact? |
|---|---|---|
| before any execution | `8b 4c 24 04` | yes |
| `multiboot_entry` (`0x100194`) | `8b 4c 24 04` | yes |
| after page-directory `rep stos` (`0x1001cc`) | `8b 4c 24 04` | yes |
| after the PTE-fill loop (`0x100203`) | `8b 4c 24 04` | yes |
| after the zero loop (`0x100210`) | `8b 4c 24 04` | yes |
| before `fix_desc_common` (`0x100226`) | `8b 4c 24 04` | yes |
| right after paging is enabled (`0x100274`) | `8b 4c 24 04` | yes |
| `machine_startup` entry (`0xc01706f0`) | `8b 4c 24 04` | yes |
| after the BSS clear (`0xc0170708`) | `8b 4c 24 04` | yes |
| **after `i386_init()` (`0xc0170717`)** | **`00 00 00 00`** | **NO** |

**`i386_init()` destroys it.** The rest of `.text` survives — `setup_main`
at `0x122870` still reads correctly at the same moment, so this is
specifically the page at `0x100000`.

## The prime suspect: pmap_bootstrap allocating over the kernel

`i386_init()` calls `pmap_bootstrap()`, which builds the kernel's real
page tables and zeroes each page-table page it allocates. It is the only
thing in that function that writes whole pages.

The evidence from the crash site fits it exactly. The registers
recovered from the wild jump were **page table entries** — four
consecutive slots 0x1000 apart with low bits `0x103` (`P|RW|G`), naming
frames `0xfc000`, `0xfe000`, `0xff000` and `0x100000`. That is a page
allocator walking upward and reaching `0x100000`, which is precisely
where the kernel's text is loaded.

The banner supports the same reading:

```
Available physical space from 0x101000 to 0x3fe0000
```

`avail_start = 0x101000` is **one page above the kernel's load
address**, not above the kernel's *end* (`end = 0x1fe084`). If
`pmap_bootstrap` hands out pages from anywhere below `0x1fe084` it is
allocating on top of the kernel image, and the first page it reaches
going upward through `0xfc000`, `0xfd000`, `0xfe000`, `0xff000` is
`0x100000` — the text page that dies.

**Next step:** read `pmap_bootstrap()` in `intel/pmap.c` and check the
arithmetic that produces its first free physical page against
`avail_start`, `first_addr` and `end`. `model_dep.c` sets
`avail_start = first_addr` after `first_addr = round_page(first_addr)`;
find what `first_addr` was initialised from and whether it accounts for
the kernel image at all.

## Eliminated: the PTE-fill loop in start.S

This was the previous prime suspect and is **disproved** — the page is
still intact at `0x100203`, `0x100210` and `0x100226`, all after the
loop has run. Kept here so it is not re-tried. The loop is:

```asm
1001cc:  add  $0xc00,%ebx      ; ebx -> PDE slot for 0xC0000000
1001d2:  mov  %edi,%esi
1001d4:  mov  $0x3,%eax        ; first PTE: phys 0 | P|RW
1001d9:  cmp  %esi,%edi
1001db:  jb   1001f5
1001dd:  mov  %edi,%edx
1001df:  and  $0xfffff000,%edx
1001e5:  or   $0x3,%edx
1001e8:  mov  %edx,(%ebx)      ; write PDE
1001ea:  add  $0x4,%ebx
1001ed:  mov  %edi,%esi
1001ef:  add  $0x1000,%esi
1001f5:  mov  %eax,(%edi)      ; write PTE
1001f7:  add  $0x4,%edi
1001fa:  add  $0x1000,%eax     ; next physical frame
1001ff:  cmp  %edi,%eax
100201:  jb   1001d9           ; loop while eax < edi
```

Entering it, `%edi = 0x501000` (the page-directory `rep stos` advanced
it a page past the directory at `0x500000`).

The termination condition compares `%eax`, a **physical frame address**,
against `%edi`, a **pointer into the PTE array**. Solving
`3 + 0x1000n >= 0x501000 + 4n` gives n = 1282, so it writes 1282 PTEs at
`0x501000..0x502008` and maps physical `0` through roughly `0x502000`.

It terminates when `%eax`, a physical frame address, exceeds `%edi`, a
pointer into the PTE array — an odd construction, but not the bug. It
writes 1282 PTEs at `0x501000..0x502008`, mapping physical `0` through
about `0x502000`, and leaves `0x100000` alone.

## The addressing correction that cost the most time

**The kernel is relocated by segmentation, not only by paging.** The
`-d exec` trace makes this explicit:

```
Trace 0: 0x7f36... [c0000000/00000000c0101005/000000f0/ff020000]
                    ^^^^^^^^ cs_base    ^^^^^^^^ linear pc
```

`cs_base = 0xC0000000`, so the CPU's `EIP` is the **low** value
(`0x101005`) while the linear address is `0xC0000000 + EIP`.
`start.S` does this deliberately at `0x100252`:

```asm
mov %cr3,%eax
mov 0xc00(%eax),%ecx    ; read PDE[768], the 0xC0000000 entry
mov %ecx,(%eax)         ; copy it to PDE[0] -- identity-map low 4MB
```

so both the low and high ranges are valid, and code runs at low `EIP`
until the `lgdt`/`ljmp` installs the high-based segments.

Practical consequence: **gdb breakpoints in kernel C code need the
linear form** — `break *0xc01706f0` for `machine_startup`, not
`break *0x1706f0`. Several earlier "breakpoint never hit" results were
this mistake and said nothing about the kernel. Memory reads work at
either address, since both map to the same physical page.

## Hypotheses already eliminated

Do not spend time on these again.

- **`genassym` offsets are wrong.** Disproved. `TH_KERNEL_STACK`,
  `TH_TOP_ACT` and `TH_CONTINUATION` in the generated `assym.S` are 32,
  316 and 72; recompiling the same `offsetof` expressions with the
  kernel's own flags gives 32, 316 and 72. `genassym.c`'s `offsetof`
  macro takes a **pointer** type (`((TYPE)0)->MEMBER`) and `thread_t` is
  `struct thread_shuttle *`, so the usage is correct too.
- **The BSS clear is responsible.** Disproved. `edata = 0x1d6cac`,
  `end = 0x1fe084`; the clear covers only that range and cannot reach
  `0x100000`.
- **The PTE-fill loop in `start.S`.** Disproved by sampling at
  `0x100203`, `0x100210` and `0x100226` — all after it, all intact.
- **The BSS clear moved to `machine_startup`.** Disproved; the page is
  intact at `0xc0170708`, immediately after it returns.
- **`setbit` has a bug.** No. Its four instructions are correct as
  linked. The problem is that they are not in memory when called.
- **`Switch_context` has a bug.** No. It faithfully loads registers from
  where it is told; it is told to look at page tables.
- **The kernel is hung or faulting.** No. Zero exceptions, and the
  "loop" is a sled through zeroed memory, not a loop.

## Traps hit while finding this

All recorded in `DEBUGGING.md`, listed here because they cost the most
time on this particular bug:

- Reading `0xb8000` through gdb after paging is enabled returns nothing
  useful. Use the monitor's `pmemsave`; it takes a **physical** address.
- `objdump -d --start-address=X` will disassemble from X even if X is
  mid-instruction, producing convincing nonsense. Always start from a
  symbol address.
- A `pmemsave` taken several seconds in shows memory already corrupted
  **by** the runaway. Sample early, at a breakpoint, or the cause and
  the consequence are indistinguishable.

## State of the tree

The kernel builds clean and boots to the point described. Deviation from
the vendor import is four files, all documented in place:

```
i386/AT386/model_dep.c   clear BSS before consuming boot data
i386/i386_rpc.c          asm operands written must be outputs
i386/locore.S            register widths matched to suffixes
i386/pio.h               inw/outw instead of the 0x66 prefix hack
```

`docs/bootstrap-fork.md` records a separate open decision (Hurd vs OSF
bootstrap) that is **not** reachable until this is fixed — neither
bootstrap function is among the 55 functions the kernel enters.

---

# Next blocker: splsched() returns a bogus IPL

**Status: open, narrowed to one routine. Supersedes the section above,
which is resolved.**

The `.text`-zeroing bug is fixed (see the commit "Keep the two variables
start.S writes out of BSS"). The kernel now reaches far further:

```
distinct functions entered   55  ->  320
```

It completes VM init, IPC bootstrap, task and thread creation, services
timer interrupts, and runs device autoconfiguration to completion:

```
Available physical space from 0x100000 to 0x3fe0000
vm_page_bootstrap: 14705 free pages
adjusting delay count: 10 9 35 175 313 261 319 333 327
Unrecognized processor (type = 0x0, family = 0x6, model = 0x6)
fdc0: port = 3f2, spl = 5, pic = 6.
fdc0: at atbus0
 fd0: at fdc0 slave 0, port = 3f2, spl = 5, pic = 6.
 fd1: at fdc0 slave 1, port = 3f2, spl = 5, pic = 6.
kd0: at atbus1, port = 60, spl = 6, pic = 1.
com0: 82550 or 16550 chip.
com0: at atbus2, port = 3f8, spl = 6, pic = 4. (DOS COM1)
S3 chip ID probe returned 0x0
vga: standard
vga0: at atbus3
realtime clock configured
battery clock configured
intnull(14)
panic: splx(old 91, new 8): logic error in locore.s
In tight loop: hit ctl-alt-del to reboot
```

## Read the panic message backwards

**The labels in that message are swapped.** `i386/spl.S` pushes:

```asm
splxpanic:
        pushl   EXT(curr_ipl)   /* pushed first  -> prints as the 2nd %x */
        pushl   %eax            /* pushed second -> prints as the 1st %x */
        pushl   $splxpanic2
        call    EXT(panic)
```

Under cdecl the last thing pushed before the format string is argument
one, so `%eax` prints where the message says "old". The real reading is:

- **requested level = 0x91** (the argument to `splx`)
- **curr_ipl = 8**, which is healthy: `SPLHI` is 8 for AT386
  (`ipl.h` defines `IPLHI` as 7 only for `iPSC386`)

`0x91 = 145 > 8`, so `ja splxpanic` fires correctly. The check is doing
its job; the caller is wrong.

Confirmed against the compiled code rather than the headers:

```asm
159dfe:  cmp $0x0,%eax
159e01:  jl  splxpanic
159e03:  cmp $0x8,%eax        <- SPLHI is 8, as expected
159e06:  ja  splxpanic
```

## The caller

From a `-d exec` trace, the blocks immediately before `splxpanic`:

```
thread_create_in
  thread_hold
    splvm
    install_special_handler
      install_special_handler_locked
    splx            <- panics
```

`install_special_handler()` at `kern/thread_act.c:1549` is an ordinary,
correct pairing:

```c
spl_t   spl;
...
spl = splsched();
...
splx(spl);
```

So **`splsched()` is returning 0x91** and `install_special_handler` is
faithfully handing it back. `0x91` is not a plausible IPL; it looks like
whatever happened to be in `%eax`, i.e. a path that returns without
setting a value.

That lead was followed and the answer is below. `splsched` does not
fall through without setting `%eax`; the problem is a width disagreement
between the assembly and the C declaration.

## Root cause: curr_ipl is 32 bits in assembly, 8 bits in C

`i386/spl.h:35`:

```c
typedef unsigned char           spl_t;
```

But every spl routine is assembly that returns the full 32-bit value:

```asm
00159dc8 <splsched/splhi/splhigh/splclock>:
        cli
        mov   0x1e0a08,%eax      ; return the FULL 32-bit curr_ipl
        movl  $0x8,0x1e0a08      ; and store a 32-bit 8
        ret
```

and `curr_ipl` is written with `movl` throughout — the encoding at
`0x159dce` is `c7 05 08 0a 1e 00 08 00 00 00`, a four-byte store.

GCC, seeing prototypes that return `spl_t`, keeps only the low byte:

```asm
0010cba9:  call   159dc8 <splclock>
0010cbb2:  movzbl %al,%ebx        ; discard the upper 24 bits
```

That truncation is **correct for the declared type** and is not itself
the defect. It is what makes the defect visible: `curr_ipl` genuinely
holds a value whose low byte is `0x91`, and the C side can only ever see
that byte.

So the assembly and the C disagree about the width of a shared object.
This is the same family as the `.bss` bugs already fixed -- assembly and
C disagreeing about a shared variable's representation -- except the
disagreement is width rather than section.

**What is NOT yet established**, and must not be guessed: which side is
wrong. Either `curr_ipl` should be a 32-bit `int` and `spl.h`'s
`unsigned char` is the error, or the assembly should be storing and
loading a byte. Deciding needs the C declaration of `curr_ipl` itself
read against every assembly site that touches it, in `spl.S` and
`interrupt.S`. Note the C declarations found so far disagree with each
other too:

```
i386/spl.h:35              typedef unsigned char  spl_t;
hp_pa/spl.h:205            typedef unsigned       spl_t;
i386/kgdb_interface.c:82   typedef int            spl_t;   /* "XXX" */
i386/AT386/lpr.c:221       extern spl_t curr_ipl[];
i386/AT386/mp/mp_v1_1.c:69 extern int   curr_ipl[NCPUS];   <- int, not spl_t
```

`mp_v1_1.c` declaring it `int` while `lpr.c` declares it `spl_t`
(= `unsigned char`) is a direct contradiction inside the same tree.

**Also still unexplained:** where the value `0x91` comes from at all.
Only four instructions in the entire linked kernel write `curr_ipl`, and
none of them can produce 145:

```
158388:  movl $0x8,0x1e0a08
159dce:  movl $0x8,0x1e0a08
159e13:  mov  %eax,0x1e0a08     (set_spl, after the bounds check)
159e5c:  mov  %eax,0x1e0a08
```

The indexed write at `interrupt.S:430` is inside
`#if NCPUS > 1 && AT386 && !MP_V1_1` and is compiled out, as is the
whole `MP_V1_1` interrupt path. So either something writes `curr_ipl`
that is not a direct store to `0x1e0a08` -- a stray pointer, or a
neighbouring object overrunning into it -- or `0x159e5c` is reached with
an unvalidated `%eax`. `0x159e5c` sits just past `splxpanic`
(`0x159e4a`) and has not been identified; identify it first.

## Not the trigger: intnull(14)

`intnull(14)` prints immediately before the panic and looks related. It
is not. Booting with `-nodefaults` to remove the IDE controller, the
usual source of IRQ 14, leaves both the message and the panic exactly
as they were. Where the interrupt comes from is a separate question and
is not blocking.

## Eliminated

- **`splsched` returning without setting `%eax`.** It does set it. With
  `MACH_KPROF` off, `Entry(splsched)` deliberately falls through into
  `Entry(splhigh)`/`Entry(splhi)`, which loads `%eax` from `curr_ipl`
  before overwriting it. `splsched`, `splhi`, `splhigh` and `splclock`
  are all the same address (`0x159dc8`), as are `splimp`, `splnet` and
  `spltty` (`0x159dc0`) -- a breakpoint on one catches all of them.
- **GCC's `movzbl %al,%ebx` truncation.** Correct for
  `spl_t = unsigned char`. It reveals the bug rather than causing it.
- **`spl.S:335-338` clobbering `%edx`.** It looks as though `%edx`, the
  CPU index, is overwritten by the old IPL between reading and writing
  `curr_ipl`. It is not: line 337 is `CPU_NUMBER(%edx)`, which reloads
  it. Read the intervening line before reporting this.
- **`SPLHI` being 7 while something passes 8.** `ipl.h` does define
  `IPLHI` twice, but the 7 arm is `#if iPSC386`. The compiled constant
  is 8.


---

# Narrowed: the splx argument is corrupted on the stack

**Status: open. Mechanism located to a three-instruction window.
Supersedes the width-mismatch section above, which is real but is not
what produces 0x91.**

## The call is correct when it is made

`install_special_handler` is entered **once** before the panic (counted
from a `-d exec` trace, not from gdb). Measured at breakpoints on that
single call:

```
at 0x10cbb2, after "call splclock":   %eax = 8
at 0x10cbde, where the arg is stored: %ebx = 8
```

So the value handed to `splx` is correct at the moment it is written.

## The tail call is also correct

```asm
10cbde:  mov  %ebx,0x10(%esp)   ; place arg where splx will read it
10cbe2:  add  $0x4,%esp
10cbe5:  pop  %ebx
10cbe6:  pop  %esi
10cbe7:  jmp  159df2 <splx>     ; tail jump
```

`%esp` rises 12 across the `add` and two `pop`s, so the slot written at
`0x10(%esp)` is at `0x4(%esp)` when `splx` executes
`mov 0x4(%esp),%eax`. The arithmetic checks out.

## Therefore the stack slot is overwritten in between

`splx` reads `0x91` from a slot that held `8` three instructions
earlier, on the only call to this function. Nothing in those three
instructions writes memory. The only thing that can run in that window
is an **interrupt**, and the interrupt path pushes onto this same stack.

`intnull(14)` prints immediately before the panic, which places an
unhandled interrupt at exactly the right moment.

**Next step:** confirm an interrupt is taken in that window. Either use
`-d int` correlated with the block index of `0x10cbde` from the exec
trace, or capture `%esp` at `0x10cbde` and compare it against the frame
the interrupt path builds. If confirmed, the question becomes why the
interrupt frame lands on top of a live stack slot rather than below
`%esp`.

## A real hazard found on the way, not the cause

`interrupt.S` calls `set_spl` **directly at its entry**, which in the
linked image is `0x159e08`:

```asm
movzbl EXT(intpri)(%ecx), %eax   # eax = intpri[int#]
call   EXT(set_spl)              # sets curr_ipl = eax
```

`splx` at `0x159df2` performs the bounds check and then *falls through*
into `set_spl` at `0x159e08`. Entering `set_spl` by `call` therefore
**bypasses the check entirely**, and `set_spl` writes `curr_ipl`
unvalidated at `0x159e13`. Any bad value in `intpri[]` would reach
`curr_ipl` with nothing to catch it.

It is not the cause here -- `intpri` was read out of the running kernel
and is correctly populated, matching the boot log exactly:

```
intpri[0..15] = 08 06 00 00 06 00 05 00 00 00 00 00 00 01 00 00
                ^clock ^kd      ^com  ^fdc
intpri[1]=6  matches "kd0: spl = 6"
intpri[4]=6  matches "com0: spl = 6"
intpri[6]=5  matches "fdc0: spl = 5"
intpri[14]=0 valid (SPL0)
```

Worth recording anyway; it is a latent trap for the next person who
changes `intpri` or adds a driver.

## Eliminated this round

- **`intpri[14]` holding garbage.** It holds 0, which is valid.
- **`set_spl_noi` writing the bad value.** It is the only unvalidated
  writer and is reached only from `return_from_interrupt`, but the
  argument passed to `splx` is already wrong before any of that matters.
- **`install_special_handler_locked` clobbering `%ebx`.** It pushes
  `%ebx` at entry and restores it; `%ebx` reads 8 after it returns.
- **The tail-call stack arithmetic.** Verified instruction by
  instruction.

## Instrument failures -- read this before trusting a measurement

**gdb breakpoint conditions do not work against this QEMU stub.**

```
break *0xc0159df6 if $eax != 8
```

stops with `$eax == 8`. Conditions appear to be ignored entirely, so the
breakpoint behaves as unconditional. Do not use them. Count and filter
with `-d exec` traces instead.

Two earlier results in this file were produced with unreliable methods
and are corrected:

| claim | method | truth |
|---|---|---|
| `set_spl_noi` only runs during the panic | gdb loop | runs **8** times before it |
| `splx`: 800 calls, all `0x8` | gdb loop capped at 800 | `splx` runs **1574** times before the panic; the loop simply stopped early |

The working technique for reading kernel data at a breakpoint, after
several failures, is to **combine gdb and the monitor**: break in gdb,
then `shell` out to a script that issues `pmemsave` over the monitor
socket while the guest is stopped. `pmemsave` takes a guest *physical*
address and works regardless of paging or segmentation.

```
(gdb) shell python3 tools/pmem.py /tmp/mon 0x1e0a08 4 /tmp/out.bin
```

gdb alone cannot read many kernel addresses at a breakpoint -- both
`0x1e0a08` and `0xc01e0a08` return "Cannot access memory" at moments
when reading registers works fine.


---

# CORRECTION and current state of the splx panic

**Everything above about a "corrupted stack slot" is wrong.** It is left
in place because the reasoning is instructive, but do not act on it.

## What was wrong, and why

The failure is **non-deterministic**. The reported value differs every
boot: `0x91`, `0x57`, `0x3f`, `0x18`, `0x4b`, `0x17` have all been seen
from the same binary.

That was not noticed, and two measurements taken in *different runs*
were compared as though they came from one:

- `%ebx = 8` at `install_special_handler+62` — correct, in that run.
- `%eax = 0x91` at `splxpanic` — correct, in another run.

Comparing them produced "the argument is correct when written and wrong
three instructions later", which is impossible, and then an interrupt
hypothesis to explain the impossibility. A later measurement showed
**no interrupt is taken in that function at all** — only four interrupts
occur before the panic, and the 1,453 at `0x1704f4` are after it while
halted.

Specifically retracted:

- "the stack slot is overwritten in between" — no.
- "the only thing that can run in that window is an interrupt" — nothing
  runs there.
- "`install_special_handler` is not the culprit" — it is; see below.

## What is actually established

`install_special_handler` **tail-jumps** into `splx`:

```asm
10cbde:  mov  %ebx,0x10(%esp)
10cbe2:  add  $0x4,%esp
10cbe5:  pop  %ebx
10cbe6:  pop  %esi
10cbe7:  jmp  159df2 <splx>
```

so at `splx` the stack still holds `thread_hold`'s frame. Read at the
panic, in one stopped guest:

```
eax = 0x91            the bad IPL
esp = 0x08b78f1c      linear 0xC8B78F1C  (segment base!)
[esp]   = 0x0010cc39  return addr from thread_hold's
                      "call install_special_handler" at 0x10cc34
[esp+4] = 0x00000091  the argument
```

`0x10cc39` is **not** a call to `splx` — it is the instruction after
`call install_special_handler`. Reading it as "`thread_hold` called
`splx`" is the tail-call trap described in `DEBUGGING.md` §7a.

So the panicking call is `install_special_handler`'s, and its argument
comes from:

```asm
10cba9:  call   159dc8 <splclock>     ; == splhi == splsched == splvm
10cbb2:  movzbl %al,%ebx              ; keep the low byte
```

and `splclock` is only:

```asm
mov  0x1e0a08,%eax      ; return the OLD curr_ipl
movl $0x8,0x1e0a08
```

**Therefore `curr_ipl` itself is intermittently garbage.** Nothing is
wrong in `splx`, `install_special_handler` or `thread_hold`; they
faithfully pass along a bad value that was already in `curr_ipl`.

## The remaining suspect

Only four instructions in the linked kernel write `curr_ipl`:

```
158388:  movl $0x8,0x1e0a08     picinit, constant
159dce:  movl $0x8,0x1e0a08     splhi, constant
159e13:  mov  %eax,0x1e0a08     set_spl
159e5c:  mov  %eax,0x1e0a08     set_spl_noi   <- UNVALIDATED
```

`set_spl_noi` is called from exactly one place, `return_from_interrupt`,
with the IPL popped off the interrupt frame, and it performs no bounds
check. `set_spl` is also reachable unchecked: `interrupt.S` calls its
entry at `0x159e08` directly, which is *past* the bounds check that
`splx` performs at `0x159dfe`–`0x159e06` before falling through into it.

And a nested interrupt has been observed: `v=0x4f` (IRQ 15) taken at
`EIP=0x158410` with `ECX=0x0e`, i.e. while IRQ 14 was being handled, on
a different stack from the IRQ 14 frame. `intnull(14)` prints
immediately before the panic in every run.

**Hypothesis, not yet proven:** the nested interrupt's return path
restores a bad saved IPL through `set_spl_noi`, leaving `curr_ipl`
out of range for the next `splsched`.

**Next experiment.** In one stopped guest: break at `0x159e5c`, read
`%eax` each time, and find the call that writes a value outside `0..8`.
gdb breakpoint conditions do **not** work against this stub (see
`DEBUGGING.md` §3), so count and filter with `-d exec` or step manually.
Because the failure is non-deterministic, take every value in the chain
from the same run.


---

## Round 2 eliminations: the curr_ipl writers are clean

All values below come from **single runs**, per section 0 of
`DEBUGGING.md`.

**`set_spl_noi` is eliminated.** It was the leading suspect, being the
only unvalidated writer. Breaking on its first call:

```
first set_spl_noi call: eax = 0x5   (valid)
console at that moment: "panic: splx(old 91, new 8)" ALREADY PRINTED
```

It never runs before the failure. Earlier readings of `0x1704f4`
(`halt_all_cpus+36`) from it are all post-panic, from timer interrupts
arriving while the kernel sits in its halt loop.

**`set_spl` is eliminated.** 4,000 consecutive writes logged in one run,
every one in range `0..8`.

**The `-d exec` counts were wrong, in a way worth remembering.** `splx`
*falls through* into `set_spl` at `0x159e08`, and a fallthrough does not
start a new translated block, so the trace only counts entries reached
by `call`. It reported 7 `set_spl` entries where a breakpoint sees
thousands, and 1,574 `splx` entries where the panic had still not been
reached after 4,000 `set_spl` writes. **Never size a brute-force search
from a `-d exec` count where fallthrough is possible.**

## Hardware watchpoints DO work, and found a writer grep missed

Contrary to breakpoint *conditions*, which are silently ignored
(`DEBUGGING.md` §3), hardware watchpoints function:

```
(gdb) watch *(unsigned int*)0x1e0a08
Hardware watchpoint 1
```

Validated against known writes; it reports EIP **after** the storing
instruction. First three hits of a boot:

```
eip=0x153539   bzero+17      rep stos    <- the BSS clear
eip=0x158392   picinit+236   movl $0x8
eip=0x159e18   set_spl+16    mov %eax,...
```

`bzero` is the important one: it writes `curr_ipl` through a computed
address (`rep stos`), so **grepping the disassembly for stores to
`0x1e0a08` does not find all writers**. The earlier claim in this file
that "only four instructions write `curr_ipl`" is therefore wrong as
stated — it was four *direct* stores. Any stray pointer write would
likewise be invisible to that method.

**Caveat on the instrument:** the watchpoint fires reliably for the
first few hits, but a loop of 25 `continue`s produced no output at all,
twice. It is usable for "what writes this, early" and not yet trusted
for "scan until a condition holds". Validate before relying on it.

## What is still unexplained

`curr_ipl` holds `0x91` when `splclock` reads it, yet no writer has been
caught writing an out-of-range value. Both cannot be true. The most
likely wrong assumption is still the completeness of the writer set,
which `bzero` has already shown to be incomplete once.

Unresolved ambiguity worth stating plainly: at the panic, `[esp]` is
`0x0010cc39` and `[esp+4]` is the bad value. `0x10cc39` is the
instruction after `thread_hold`'s `call install_special_handler`. That is
consistent with **either** reading -- `install_special_handler` tail-
jumping into `splx` and leaving `thread_hold`'s frame in place, **or**
something `call`ing `splx` with `0x10cc39` as a genuine return address.
The tail-call reading was asserted earlier in this file with more
confidence than the evidence supports.

## Suggested next steps

1. **Do not brute-force `set_spl`.** It is clean and the search space is
   far larger than the trace suggests.
2. Get a reliable long-running watchpoint, or find another way to catch
   a write of a value `> 8` to `0x1e0a08`. Validate whatever is chosen
   against a known write first.
3. Settle the tail-call ambiguity by reading `[esp]` at `splx` *entry*
   on a run that panics, and comparing `esp` there against `esp` at
   `install_special_handler+62` in the **same** run.
4. Consider whether `0x91`-class values could come from somewhere other
   than `curr_ipl` at all -- the read is
   `mov 0x1e0a08,%eax` then `movzbl %al,%ebx`, so only the low byte
   survives, and every observed bad value fits in a byte.


---

## Round 3: the panic is on a different stack, and the trace counts are unreliable

All values below come from **single runs**.

### The whole chain, one run

```
A  at install_special_handler's "call splclock":
       curr_ipl = 0x8        esp = 0x001c9f30      <- boot stack
C  at its store of the argument:
       ebx      = 0x8        esp = 0x001c9f30
E  at splxpanic:
       eax      = 0x91       esp = 0x08b78f1c      <- THREAD stack
```

The stacks are unrelated. `0x1c9f30` is the boot stack set up by
`vstart` (`lea 0x1ca000,%esp`); `0x08b78f1c` is in kernel VM, allocated
for a thread.

### install_special_handler is healthy, and is called many times

Breaking on `0xc010cba9` and logging every call in one run: **22
consecutive calls, every one on the boot stack with `curr_ipl = 8` and
`ebx = 8`.** None of them is the failing call.

So the panicking `splx` comes from a later invocation running on a
thread stack, and stepping to it one breakpoint at a time does not
converge -- 22 iterations took about 150 seconds.

### The `-d exec` counts have been wrong every time

This is the finding with the widest consequences. Comparing trace counts
against breakpoint counts, in the same build:

| function | `-d exec` said | breakpoints show |
|---|---|---|
| `set_spl` | 7 | thousands |
| `splx` | 1574 | still not panicking after 4000 |
| `install_special_handler` | 1 | at least 22 |

Two distinct causes:

- **Fallthrough.** `splx` falls through into `set_spl`; a fallthrough
  does not start a new translated block, so only `call`-entries count.
- **Block chaining.** QEMU chains translated blocks and does not
  re-log an entry every time a chained block is re-executed, so a
  function called repeatedly from the same site can be logged once.

**Consequence: `-d exec` is reliable for "was this code ever reached"
and for the *order* of first entry. It is not reliable for "how many
times", and must not be used to size a search or to conclude that
something runs only once.** Several earlier inferences in this file were
built on exactly that, including the claim that
`install_special_handler` is "entered exactly once before the panic".

### What is now known about the failing call

`0x10cc39` on the panic stack **cannot** be a return address from a call
to `splx`: `thread_hold+41` is `mov 0x184(%ebx),%eax`, not a call
instruction. It is the return address from
`call install_special_handler` at `thread_hold+36`. So either

- `install_special_handler` tail-jumped into `splx` from an invocation
  running on a thread stack -- consistent with everything, and the
  simplest reading -- or
- it is stale data on that thread's stack and the real caller is
  elsewhere.

The first is more likely, because the value at `[esp+4]` is exactly the
bad IPL, which is where `install_special_handler`'s tail call puts its
argument.

### Next experiment

Catch `install_special_handler` on a **thread** stack rather than the
boot stack. `esp` is the discriminator: boot stack is `0x001c9xxx`,
thread stacks are in kernel VM around `0x08bxxxxx`.

gdb breakpoint conditions do not work here, so this needs either a
scripted loop that continues until `$esp >> 20 != 0x1c9`, accepting the
runtime, or a different instrument. Before investing in the loop, note
that the same script reached only 22 calls in 150 seconds; the panic is
much further out.

An alternative worth trying first: make the failure happen sooner or
more often. `thread_hold` is called per thread creation, so a
configuration that creates fewer threads, or booting without modules so
the boot script does not run, may reach the failing call earlier. Booting
with no modules still panics with the same class of value, and is a
shorter path.


---

## Round 4: instrument limits mapped, and a latent width bug next door

### Booting without modules does not shorten the path

Logged every `install_special_handler` call with no modules supplied:
30 calls, all on the boot stack (`esp=0x001c9f30`), all with
`curr_ipl = 8`. The suggestion at the end of round 3 -- that the
no-module boot might reach the failing call sooner -- is **wrong**, and
should not be retried.

### splx has 503 call sites

```
408  call 159df2 <splx>
 95  jmp  159df2 <splx>     (tail calls)
```

Focusing on `install_special_handler` was far too narrow. The stack
layout at the panic still points there, but it is one of 503.

### Every gdb feature that evaluates and resumes is broken

Already known: breakpoint **conditions** are silently ignored. Now also
measured: **ignore counts** do not work either.

```
(gdb) ignore 1 50          -> prints nothing (should confirm)
(gdb) continue             -> stops; registers then unreadable
(gdb) info breakpoints     -> "ignore next 50 hits"   (never decremented)
```

The coherent picture: this stub supports setting breakpoints and
stopping at them. **Anything requiring gdb to evaluate at a stop and
resume automatically -- conditions, ignore counts -- silently does
nothing useful.** Only manual stop-and-read loops work, at roughly six
stops per second, which is what makes a search of this size impractical.

### curr_ipl's neighbourhood, and a real latent bug

`curr_ipl` sits inside a block of 2-byte PIC register variables:

```
0x1e0a06 .. 0x1e0a08  size=2   master_ocw
0x1e0a08 .. 0x1e0a0c  size=4   curr_ipl
0x1e0a0c .. 0x1e0a0e  size=2   PICM_ICW3
```

Every access to `master_ocw` in the linked image:

```
15833c:  66 89 0d 06 0a 1e 00   mov %cx,0x1e0a06     WRITE, 16-bit
159e37:  8b 15 06 0a 1e 00      mov 0x1e0a06,%edx    READ,  32-bit
159e78:  8b 15 06 0a 1e 00      mov 0x1e0a06,%edx    READ,  32-bit
```

The reads are 32-bit against a 2-byte object, so they pull `curr_ipl`'s
low half into the top of `%edx`. The cause is an assembly/C width
disagreement:

```c
i386/pic.c:171      i386_ioport_t master_icw, master_ocw, slaves_icw, slaves_ocw;
```
```asm
i386/spl.S:170      movl EXT(master_ocw),%edx
i386/interrupt.S:233,263   movl EXT(master_icw),%edx
```

**This is not the panic cause.** It is a read, not a write, so it cannot
corrupt `curr_ipl`; and only `%dx` reaches `outb %al,(%dx)`, so the port
number is right. Recorded because it is real, because it is the fourth
instance of assembly and C disagreeing about a shared object's
representation, and because anyone who later changes the layout of these
variables or starts using the full `%edx` will be bitten by it.

### Where that leaves the search

Still unexplained: `curr_ipl` reads `0x91` at the failing `splclock`,
and no writer has been caught writing out of range. Ruled out so far:
`set_spl`, `set_spl_noi`, `intpri[]`, every observed
`install_special_handler` call, and now the neighbouring-variable
overrun theory.

The remaining candidates, in the order worth trying:

1. **A stray pointer write** from anywhere in the kernel. A hardware
   watchpoint is the only instrument that can see this; it works for the
   first few hits but has not been made to scan (see above). Getting a
   long-running watchpoint working is probably the highest-value
   instrument work available.
2. **The failing `install_special_handler` invocation on a thread
   stack**, which needs either a working conditional stop or a way to
   make the failure occur sooner. Both currently unavailable.
3. That `curr_ipl` is not the source at all -- the read is
   `mov 0x1e0a08,%eax` then `movzbl %al,%ebx`, so only the low byte
   survives, and every bad value observed fits in a byte.


---

# FOUND: set_spl_noi writes a code address into curr_ipl

**This supersedes every "eliminated" verdict above that concerns
`set_spl_noi`. Read this section first.**

## The measurement

A hardware watchpoint on `curr_ipl`, logging the first 20 writes of one
run:

```
 0 t=0.01s eip=0x00159e18 curr_ipl=0x8        esp=0x001c9fec
 1 t=0.01s eip=0x00159e61 curr_ipl=0x1704f4   esp=0x001c9ff4
 2 t=0.02s eip=0x00159e18 curr_ipl=0x8        esp=0x001c9fec
 3 t=0.03s eip=0x00159e61 curr_ipl=0x1704f4   esp=0x001c9ff4
 ...  alternating, every ~0.005s, indefinitely
```

`0x159e18` is `set_spl+16`, writing a correct `0x8`.
`0x159e61` is `set_spl_noi+5`, writing **`0x1704f4`**, which is
`halt_all_cpus+36` -- a code address, not an IPL.

## Two earlier verdicts in this file are wrong

- **"`set_spl_noi` is eliminated."** It was eliminated on the evidence
  that its *first* call carries `eax = 0x5`. That is true and
  irrelevant: the first call is fine and every subsequent one is not.
  Checking only the first instance of a repeating call is not an
  elimination.
- **"The `0x1704f4` readings are all post-panic."** They are not.
  `esp = 0x001c9ff4` is the **boot stack**, and the writes begin at
  t=0.01s, long before the panic. That dismissal was based on the value
  looking like halt-loop noise rather than on when it occurred.

## Why the value is what it is

`set_spl_noi` has exactly one caller, `return_from_interrupt`, which
recovers the saved IPL like this:

```asm
                pushl   %eax                    # save old IPL
                pushl   EXT(iunit)(,%ecx,4)     # unit# as handler arg
                call    *EXT(ivect)(,%ecx,4)    # the handler
return_from_interrupt:
                addl    $4,%esp                 # drop the handler arg
                cli
                popl    %eax                    # the saved IPL
```

Recovering `halt_all_cpus+36` from that `popl` means the stack is **off
by one slot** at that point: it pops a return address where the saved
IPL should be. `set_spl_noi` then writes it to `curr_ipl` with no bounds
check, which is why nothing catches it.

This also explains the downstream behaviour that has been chased for
several rounds. Once `curr_ipl` holds a code address, the next
`splclock`/`splsched` returns it, `movzbl %al,%ebx` keeps the low byte,
and `splx` is handed a value like `0xf4` -- a byte-sized garbage value
that differs per run because the code address differs. Every observed
bad value fits in a byte, which matches.

## Next step

Work out why the interrupt return stack is off by one slot. Candidates,
untested:

1. A handler reached through `ivect[]` that does not conform to the
   convention `return_from_interrupt` assumes. `intnull` is the stub for
   unregistered vectors and `intnull(14)` prints immediately before the
   panic in every run.
2. A path that reaches `return_from_interrupt` without having pushed
   both the saved IPL and the handler argument -- i.e. a `jmp` into the
   middle of the sequence rather than a fall-through from the `call`.
3. `ETAP_INTERRUPT_PROBE` or `MP_*` macros expanding to something that
   disturbs the stack in this configuration.

Check 2 first: search for every branch to `return_from_interrupt` and
confirm each arrives with the same stack shape.


---

## Instrument failure: stale QEMU processes invalidated measurements

`pkill -x qemu-system-i386` **never matched anything**. Linux truncates
`comm` to 15 characters, so the process is `qemu-system-i38`. Every
"fresh" QEMU started during this investigation raced a stale one for
port 1234, and gdb frequently attached to the old, already-failed guest.

Two runs of the identical watchpoint script, back to back:

```
run 1   initial curr_ipl = <unreadable, guest at reset>
        bzero(0) -> picinit(8) -> set_spl(5) -> set_spl(6) -> set_spl_noi(5)
        all healthy

run 2   initial curr_ipl = 0x1704F4        <- already corrupt AT ATTACH
        then the alternating 0x8 / 0x1704f4 pattern
```

Run 2 is an artefact. The guest was started with `-S` and cannot have
executed, so gdb was talking to the previous run's process.

Confirmed directly: after `pkill -x qemu-system-i386`,
`ps -eo pid,etimes,comm` still shows `qemu-system-i38` alive, and a
freshly attached gdb reports `eip = 0x1704f4` and
`curr_ipl = 0x1704f4` before any `continue`.

### What this invalidates

Any measurement in this file where the guest appears to be **past the
failure at attach time** must be treated as suspect. Specifically:

- **The alternating `set_spl` / `set_spl_noi` pattern** reported in the
  previous section was captured in a stale-process run. The conclusion
  that `set_spl_noi` writes a code address into `curr_ipl` is therefore
  **not established**. It may still be true -- but it was observed on a
  guest that had already failed, where the value is expected to be
  garbage and the alternation is just the halt loop taking timer
  interrupts.
- Earlier readings of `0x1704f4` that were dismissed as "post-panic
  noise" were probably correct after all, for this reason.
- The inconsistent breakpoint behaviour seen throughout -- breakpoints
  "not firing", registers unreadable, hit sequences differing between
  identical runs -- is explained by this and need not be attributed to
  the gdb stub.

### What survives

Run 1, taken against a guest genuinely at reset, is clean:

```
bzero writes 0, picinit writes 8, set_spl writes 5, 6, then
set_spl_noi writes 5
```

Every early write is a valid IPL. So on a correctly-started guest, the
first writes to `curr_ipl` are healthy and the corruption happens later.

The gdb limitations recorded earlier -- conditions and ignore counts
silently doing nothing -- were each observed more than once and are
probably real, but should be re-confirmed on a guest verified to be at
reset before being relied on again.

### Required procedure from now on

```sh
pkill -x qemu-system-i38
ps -eo pid,etimes,comm | grep qemu     # must be empty
```

and, at attach, assert the guest is at reset before measuring:

```
(gdb) info registers eip        # expect 0x0000fff0, the reset vector
```

Any run where `eip` is not the reset vector at attach is invalid and
must be discarded.


---

# RE-ESTABLISHED, on a verified-clean guest: set_spl_noi writes a return address

The previous section retracted this finding because it had been measured
on a stale guest. Re-run under the corrected procedure, **it holds.**

## The measurement

Procedure followed exactly: `pkill -x qemu-system-i38`, `ps` confirmed
empty, guest started with `-S`, and `eip` asserted at attach.

```
ATTACH eip=0xfff0  OK reset vector

  #1  val=0x0       eip=0x153539   bzero
  #2  val=0x8       eip=0x158392   picinit
  #3  val=0x5       eip=0x159e18   set_spl
  #4  val=0x6       eip=0x159e18   set_spl
  #5  val=0x5       eip=0x159e61   set_spl_noi      <- valid
  #6  val=0x8       eip=0x159e18   set_spl
  ...
*** FIRST OUT-OF-RANGE at write #51, t=0.7s:
    curr_ipl = 0x121591  from eip=0x159e61 (set_spl_noi)
    esp = 0x1c9ff4 (boot stack)
```

45 healthy writes precede it, and it happens at t=0.7s -- long before
the panic, on the boot stack, on a guest proven to have started at the
reset vector. This is not halt-loop noise.

## Where 0x121591 comes from

It is not a stale value or a coincidence. It is exactly a return site:

```asm
00121560 <thread_continue>:
  ...
  12158c:  call 159db0 <spllo>
  121591:  add  $0x4,%esp        <- the value found in curr_ipl
```

and `spllo` is a tail-jump stub:

```asm
00159db0 <spllo>:
  159db0:  mov $0x0,%eax
  159db5:  jmp 159e08 <set_spl>
```

So `thread_continue`'s `call spllo` pushes `0x121591`, `spllo` jumps
rather than calls, and that return address remains at `[esp]` for the
whole of `set_spl`. `return_from_interrupt` later executes

```asm
154d48:  add  $0x4,%esp
154d4b:  cli
154d4c:  pop  %eax          <- retrieves 0x121591, not the saved IPL
154d55:  call 159e5c <set_spl_noi>
```

and `set_spl_noi` writes it to `curr_ipl` with no bounds check.

Downstream this is exactly what has been chased: the next `splclock`
returns the code address, `movzbl %al,%ebx` keeps its low byte, and
`splx` is handed a byte-sized garbage value -- `0x91`, that differs per
run because the code address differs.

## What is still not established

**Why** `return_from_interrupt`'s `pop` reaches `thread_continue`'s
frame. The call site's own push/pop accounting is correct, verified
instruction by instruction, and `intnull` balances exactly (28 consumed,
28 restored). Both were checked. So the stack is already off by one slot
*before* `return_from_interrupt` runs.

The tail-jump structure of the spl family is the obvious place to look:
`spllo`, `splbio`, `spltty`, `splnet`, `splimp` all `jmp` into `set_spl`
rather than calling it, so every one of them leaves its caller's return
address at `[esp]` while `set_spl` executes. If an interrupt is taken in
that window and its return path assumes a different stack shape, this is
precisely the value that would surface.

Next: determine whether the interrupt is taken inside the
`spllo`/`set_spl` window. `set_spl` does `cli` at `0x159e0b`, three
instructions after entry, so there is a real window in which interrupts
are still enabled.


---

# The trigger: com0 and fdc0 interrupt with no handler registered

Refines the previous section. `0x121591` is **not** `thread_continue`'s
return address left on the stack by `spllo`'s tail jump. It is the
**interrupted EIP**, pushed by the CPU as part of the hardware interrupt
frame.

## The evidence

Every interrupt taken during a clean boot, by EIP:

```
1477  IP=0008:001704f4   halt_all_cpus   (post-panic, the halt loop)
   2  IP=0008:0016e557
   2  IP=0008:00121591   thread_continue+49   <- the bad value
   1  IP=0008:0017307d
   1  IP=0008:00158410   intnull
```

and the records themselves:

```
Servicing hardware INT=0x44
  v=44  IP=0008:00121591  SP=0010:08b78fd0  EAX=00000008  EFL=00000202
Servicing hardware INT=0x46
  v=46  IP=0008:00121591  SP=0010:08b78fd0  EAX=00000008
```

`INT_VEC_START` is `0x40`, so these are **IRQ 4 and IRQ 6**. `EFL` has
`IF` set, so interrupts were legitimately enabled -- `thread_continue`
had just called `spllo`, which sets `SPL0`.

`EAX = 8` at the moment of interrupt, so `curr_ipl` was healthy going
in. The corruption is entirely on the way out.

## Why those two IRQs

The boot log configures both devices:

```
fdc0: port = 3f2, spl = 5, pic = 6.
com0: at atbus2, port = 3f8, spl = 6, pic = 4. (DOS COM1)
```

but the dispatch table does not have handlers for them:

```
ivect[ 0] = hardclock      ivect[ 1] = kdintr
ivect[ 4] = intnull   <-- com0 configured on pic 4
ivect[ 6] = intnull   <-- fdc0 configured on pic 6
ivect[13] = fpintr        ivect[14] = intnull
```

So the devices are probed, configured and **enabled at the PIC**, but
their interrupts dispatch to the null stub. That is the trigger: a real
device raises a real interrupt that nothing claims.

`intnull` itself is correct -- it balances exactly, 28 bytes consumed
and restored -- so the fault is not in the stub. The fault is that
`return_from_interrupt`'s `pop %eax` retrieves the CPU-pushed EIP from
the hardware frame rather than the IPL the kernel pushed, which means
the entry and exit paths disagree about the stack shape by exactly the
hardware frame.

## Two questions for the next session

1. **Why does the interrupt exit path reach the hardware frame?** The
   call site's own accounting is correct and `intnull` balances, both
   verified instruction by instruction. So the discrepancy is in how the
   interrupt is *entered* -- what pushes happen before the code at
   `0x154d34` runs, and whether every vector arrives through the same
   prologue.
2. **Why are com0 and fdc0 configured without handlers?** This may be a
   second, independent defect. If the drivers are meant to register via
   `ivect[]` during autoconfiguration and are not doing so, that is
   worth understanding on its own -- and fixing it would also remove the
   trigger, though not the underlying stack bug.

Question 2 is the cheaper one and may be the real fix: a kernel whose
configured devices register their handlers would not exercise this path
at all.


---

# RETRACTION: com0 and fdc0 DO have handlers registered

The previous section claimed `ivect[4]` and `ivect[6]` were `intnull`,
i.e. that com0 and fdc0 configured without registering handlers. **That
is wrong.** The reading was taken through gdb against a stale guest,
before the `pkill` defect was found.

Re-read on a guest verified at the reset vector, stopped at the panic:

```
  ivect[ 0] = 0x001540a0  hardclock
  ivect[ 1] = 0x0016d220  kdintr
  ivect[ 4] = 0x0015e740  comintr      <- real handler
  ivect[ 6] = 0x00161240  fdintr       <- real handler
  ivect[13] = 0x00153f30  fpintr
  ivect[14] = 0x00158410  intnull      <- the only null stub
```

The device table was right all along: `autoconf.c:600` carries
`(intr_t)comintr` and `:406` carries `(intr_t)fdintr`, `take_dev_irq`
passes `dev->intr` through to `take_irq`, and `take_irq` installs it.
Autoconfiguration works correctly. There is no missing-handler defect.

So the "cheaper question" recommended at the end of the previous section
does not exist. Only question 1 remains: **why the interrupt exit path
reaches the hardware frame.**

## What still stands from that section

The interrupt evidence itself came from standalone `-d int` runs, which
write to their own log file and do not use gdb or port 1234, so they are
not affected by the stale-process defect. Still valid:

```
Servicing hardware INT=0x44
  v=44  IP=0008:00121591  SP=0010:08b78fd0  EAX=00000008  EFL=00000202
Servicing hardware INT=0x46
  v=46  IP=0008:00121591  SP=0010:08b78fd0  EAX=00000008
```

Two interrupts are taken at `thread_continue+49`, with `IF` set and
`curr_ipl` healthy at 8. `0x121591` is the interrupted EIP from the
hardware frame, and it is the value that later appears in `curr_ipl`.

But the reading changes. These are **IRQ 4 and IRQ 6 being serviced
normally by `comintr` and `fdintr`** -- ordinary device interrupts on a
working kernel, not unclaimed interrupts hitting a null stub. The path
that corrupts `curr_ipl` is therefore the *normal* interrupt path, not
an error path, which makes it a more serious defect than described and
removes the possibility of side-stepping it.

## Lesson

Every gdb-derived reading taken before the `pkill` defect was found must
be re-verified before being relied on. The `-d int` and `-d exec` runs
are not affected. Readings already re-confirmed on clean guests:

- the `set_spl_noi` out-of-range write at t=0.7s -- **holds**
- `ivect` contents -- **retracted, was wrong**

Not yet re-verified, and currently unsafe to rely on: `intpri` contents,
the gdb conditions and ignore-count limitations, and the
`install_special_handler` boot-stack readings.


---

# The interrupt entry switches stacks; that is where to look next

`return_from_interrupt`'s `pop %eax` does **not** run on the stack the
interrupt arrived on. `all_intrs`, the IDT stub at `0x1005da`, switches
to a dedicated interrupt stack first:

```asm
1005da:  push %ecx
1005db:  push %edx
1005dc:  cld
1005dd:  cmp  %ss:0x1ca000,%esp     ; already on the interrupt stack?
1005e4:  jb   10063e <int_from_intstack>   ; yes -> do not switch
1005e6:  push %ds
1005e7:  push %es
1005e8:  mov  %ss,%dx
1005eb:  mov  %edx,%ds
1005ed:  mov  %edx,%es
1005ef:  mov  $0x48,%dx
1005f3:  mov  %edx,%gs
1005f5:  mov  0x1ca004,%ecx         ; int_stack_top
1005fb:  xchg %ecx,%esp             ; SWITCH to the interrupt stack
1005fd:  push %ecx                  ; save the old esp
1005fe:  mov  $0x8,%edx
100603:  incl %gs:(%edx)            ; cpu_data interrupt nesting count
100606:  call 154ce4 <interrupt>    ; dispatch; contains the IPL push/pop
10060b:  mov  $0x8,%edx
100610:  decl %gs:(%edx)
100613:  pop  %esp                  ; switch back
```

This ties the observations together:

- The interrupt was taken with `SP = 0x08b78fd0`, a thread stack.
- The corrupted `set_spl_noi` write had `esp = 0x1c9ff4`, which is just
  below `0x1ca000` -- the **interrupt stack**, exactly where the push
  and pop of the saved IPL happen after the switch.

So the `push %eax` at `0x154d39` and the `pop %eax` at `0x154d4c` both
execute on the interrupt stack, inside the `call interrupt` at
`0x100606`. Their accounting was verified correct in isolation, and
`intnull` balances, so if the popped value is wrong the discrepancy must
come from the surrounding structure rather than from those instructions.

## What to examine

1. **The switch guard.** `cmp %ss:0x1ca000,%esp` then `jb`. A thread
   stack at `0x08b78fd0` is far above `0x1ca000`, so the branch is not
   taken and the switch happens -- correct. A nested interrupt already
   on the interrupt stack would be below `0x1ca000` and would take
   `int_from_intstack` -- also correct on the face of it. Both arms need
   checking against what `interrupt` and `return_from_interrupt` assume.
2. **`int_from_intstack` at `0x10063e`.** This is the no-switch arm. If
   it reaches `return_from_interrupt` with a different stack shape than
   the switching arm, that is the defect. It was never examined.
3. **The interrupt stack itself.** `0x1ca000` is also where `vstart` put
   the boot stack (`lea 0x1ca000,%esp`). If the boot stack and the
   interrupt stack are the same memory, an interrupt arriving while the
   kernel is still on the boot stack would switch onto a region it is
   already using. Worth confirming; `int_stack_top` is read from
   `0x1ca004` and the guard compares against `0x1ca000`.

Point 3 is the most suspicious and the cheapest to check.

## Reminder on trust

Everything above is disassembly of the linked image, which is not
affected by the stale-guest defect. The runtime readings quoted --
`SP = 0x08b78fd0` and `esp = 0x1c9ff4` -- come from a `-d int` log and
from a watchpoint run on a guest verified at the reset vector
respectively, so both are sound.


---

# Stack layout confirmed: boot and interrupt stacks are one 4 KB region

Point 3 of the previous section is confirmed, and it is **by design**,
not a defect.

```
intstack        = 0x1c9000
eintstack       = 0x1ca000        interrupt stack is exactly 4 KB
int_stack_high  @ 0x1ca000, value 0x1ca000
int_stack_top   @ 0x1ca004, value 0x1ca000
vstart          : lea 0x1ca000,%esp
```

`start.S` declares the region as "Interrupt and bootup stack for initial
processor" -- OSF deliberately shares it. `vstart` sets `%esp` to
`eintstack`, so the boot stack **is** the interrupt stack.

The switch guard is therefore correct in both directions:

- on the boot stack, `esp` is just under `0x1ca000`, so
  `cmp int_stack_high,%esp; jb` takes the branch to
  `int_from_intstack` and does **not** switch -- right, because it is
  already the interrupt stack;
- on a thread stack such as `0x08b78fd0`, far above `0x1ca000`, the
  branch is not taken and the switch happens. **This is the failing
  case.**

Every runtime value observed in this investigation lies in that 4 KB
region: `install_special_handler` at `esp = 0x1c9f30`, 208 bytes from
the top, and the corrupted `set_spl_noi` write at `esp = 0x1c9ff4`, just
12 bytes from the top.

## A typo in start.S, harmless

`start.S:249` defines the label with the colon inside the macro
argument:

```asm
        .globl  EXT(eintstack)
EXT(eintstack:)                 <- should be EXT(eintstack):
```

Line 246 gets it right for `intstack`. It assembles and resolves
correctly -- `nm` shows `eintstack` at `0x1ca000` as intended -- because
`EXT(x)` expands to `x` here and `EXT(eintstack:)` therefore yields
`eintstack:`, a valid label. It works under the underscore convention
too. **Not a bug, and not to be "fixed";** recorded only so the next
reader does not spend time on it, as this one did.

## Where the search now stands

The structure is understood and is correct as written:

- entry stub, switch guard and both arms -- examined, consistent
- `interrupt`'s push/pop accounting -- verified instruction by
  instruction
- `intnull` -- balances exactly, 28 bytes
- stack layout and sharing -- confirmed, deliberate
- `ivect` registration -- correct, handlers installed

And yet `pop %eax` at `0x154d4c` retrieves the interrupted EIP. Every
individual piece checks out while the whole does not, which means the
wrong assumption is still somewhere unexamined rather than in any of the
pieces above.

The most likely remaining place is the **transition between the two
arms**: what happens when an interrupt arrives on a thread stack,
switches to the shared 4 KB region, and the code already using that
region -- the boot stack context -- is still live. The shared-stack
design is only safe if the boot stack is abandoned before threads run.
`install_special_handler` was observed running at `esp = 0x1c9f30`, on
the boot stack, *while* threads existed. That combination is worth
checking directly: if boot-stack code is still executing when a
thread-stack interrupt switches onto the same region, the two will
overwrite each other.


---

# Instrument failure: only one breakpoint services at a time

Setting two breakpoints and continuing services only one of them,
silently. Three runs against the same build:

```
bps at 0x154d39 + 0x154d4c  ->  only 0x154d39 fired, 23 times
bps at 0x154d41 + 0x154d48  ->  only 0x154d41 fired, 16 times
bp  at 0x154d48 alone       ->  fires normally, 8 for 8
```

The natural reading of the first two is "the code between A and B is
never reached". That produced a confident and completely wrong
conclusion here: that `kdintr` never returns, and therefore that the
interrupt path never completes and never restores the saved IPL. Tested
in isolation, the handler returns every time.

A second error rode along with it. With those breakpoints set, `%ecx`
was read as the interrupt vector and reported as `vec=1`, the keyboard.
At `0x154d48`, `%ecx` has been reused and reads `97`. The vector
attribution was wrong too.

## Measurements this invalidates

Any run in this file that used **two or more simultaneous breakpoints**
must be re-taken. Known cases:

- "`install_special_handler` entered 22 times, all on the boot stack" --
  used a breakpoint plus `splxpanic`. **Suspect.**
- "4,000 `set_spl` writes, all in range" -- used two breakpoints.
  **Suspect.**

Measurements that remain sound:

- The A/C/E chain capture, which used `break`, measure, `delete`,
  `break` sequentially -- **valid**.
- All watchpoint scans. Two *watchpoints* behave differently from two
  breakpoints and are in fact required; see `DEBUGGING.md`.
- Everything from `-d int` and `-d exec`, which do not involve gdb.
- All disassembly.

## Required technique

**One breakpoint at a time.** Set it, measure, `delete`, set the next.
Never infer "the code between A and B was not reached" from two
breakpoints; verify the second in isolation first.

## Where the search stands

No progress on the defect itself this round. The circularity is
unchanged: `curr_ipl` first goes bad at watchpoint write #51 with 45
good writes before it, every writer checks out, every stack accounting
checks out, and `set_spl` returns the old `curr_ipl` -- which says it
was already bad.

Because two of the measurements that shaped the current picture are now
suspect, the honest next step is to re-take them with single
breakpoints before drawing any further conclusions from them.


---

# Re-verification after the fix: curr_ipl is clean

Two measurements in this file were marked suspect because they used two
simultaneous breakpoints, before that limitation was discovered. Both
concerned whether anything wrote an out-of-range value to `curr_ipl`.

Rather than re-take them individually against the broken kernel, the
question they were asking has been answered directly against the
**fixed** kernel, using a watchpoint, which is unaffected by the
breakpoint limitation:

```
ATTACH eip=0xfff0  OK reset vector
166103 curr_ipl writes observed, 0 out of range
distinct values: ['0x0', '0x5', '0x6', '0x8']
```

Every value is a legitimate IPL -- SPL0, SPL5, SPL6 and SPLHI. Over
166,103 writes, `curr_ipl` never leaves the range `0..8`.

This supersedes both suspect entries:

- "`install_special_handler` entered 22 times, all on the boot stack"
- "4,000 `set_spl` writes, all in range"

Neither needs re-taking. The property they were probing -- that no
writer corrupts `curr_ipl` -- now holds absolutely, and the one writer
that did corrupt it is fixed at source.
