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
