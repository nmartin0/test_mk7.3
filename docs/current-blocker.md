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

Bisected with breakpoints, reading physical `0x100ef2` each time:

| point | bytes at 0x100ef2 | intact? |
|---|---|---|
| before any execution | `8b 4c 24 04 8b 44 24 08` | yes |
| at `multiboot_entry` (`0x100194`) | `8b 4c 24 04 8b 44 24 08` | yes |
| after the page-directory `rep stos` (`0x1001cc`) | `8b 4c 24 04 8b 44 24 08` | yes |
| at `setup_main` (`0x122870`) | all zero | **NO** |

So `.text[0x100000..0x101000]` is destroyed between `0x1001cc` and
`setup_main`. The rest of `.text` survives — `setup_main` at `0x122870`
reads correctly at the same moment.

## The prime suspect

The PTE-fill loop in `i386/start.S`, immediately after `0x1001cc`:

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

That the loop terminates on a relationship between a frame number and a
table pointer is worth understanding before changing anything. It is the
kind of construction that works only while the table sits at a
particular address, and the table's address here derives from
`0x500000`, a hardcoded constant. The kernel is ~1 MB and loads at
`0x100000`, so it fits inside the mapped region — but the loop also
writes PTEs into `0x501000+`, and if any of its bounds are off by a
page, it writes into memory it should not.

**This is a hypothesis, not a conclusion.** It has not been confirmed
that this loop is what zeroes `0x100000`. What is confirmed is the
window it sits in.

## How to confirm it

The cheap decisive test is a watchpoint on the destroyed page during
that window:

```
target remote :1234
break *0x1001cc              # physical; paging not yet on
continue
watch *(unsigned int *)0x100ef2
continue
```

If gdb's hardware watchpoints prove unreliable against this stub, the
fallback is to sample `x/4xb 0x100ef2` at successive breakpoints through
`0x1001cc`, `0x100203`, `0x100210`, `0x100226` (the jump to
`fix_desc_common`) and `0x100289` (`vstart`), which brackets it to a
single instruction.

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
