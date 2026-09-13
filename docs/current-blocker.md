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
