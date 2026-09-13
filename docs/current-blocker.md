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
