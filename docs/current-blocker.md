# Current state: the bootstrap task runs and prints

The kernel boots, runs user code at ring 3, and the bootstrap task now
initialises its console and produces its own output. The long
investigations that got here are archived under `docs/archive/`.

---

## Where it gets to

```
Kernel virtual space from 0x0 to 0x40000000.
Available physical space from 0x100000 to 0x3fe0000
vm_page_bootstrap: 14705 free pages
fdc0, fd0, fd1, kd0, com0, vga0 configured
realtime clock configured / battery clock configured
entry: 0x8063e80
Found read-only region / Found text region
Found read-only region / Found data region
I've found: 3 sections
ERROR: bootstrap task cannot find configuration file, please make sure
       that your boot device and partition is correctly specified.
Configuration file (or 'builtin'): /dev/boot_device/mach_servers/bootstrap.conf
```

That last block is the **bootstrap task's own output**, which proves the
whole path works: task creation, cthread initialisation, Mach IPC,
`printf_init`, `device_open` on the `console` device, and
`device_write_inband` to `kd`.

Run it with:

```sh
qemu-system-i386 -kernel mach_kernel.PRODUCTION -append "-o" \
    -initrd bootstrap -display none -no-reboot -m 64 \
    -monitor unix:/tmp/mon,server,nowait
python3 tools/vgadump.py /tmp/mon /tmp/vga.bin 10
```

Without `-o` the GNU Hurd path runs instead; both are fixed by the same
changes and both reach user code.

## The floppy works. Invocation:

```sh
qemu-system-i386 -kernel mach_kernel.PRODUCTION \
    -append "BOOTDEV=fd BOOTPART=1 -o" \
    -initrd bootstrap -fda boot.img \
    -display none -no-reboot -m 64 \
    -monitor unix:/tmp/mon,server,nowait
```

`BOOTPART=1` is required and is **not** a partition number here. The
boot device minor is `unit + BOOTPART`, and `MEDIATYPE(dev)` is
`dev & 0x03`, indexing `m765f[]`:

```c
80, 18, 1440,  9   /* [0] 3.50" 720  Kb  */
80, 36, 2880, 18   /* [1] 3.50" 1.44 Meg */
40, 18,  720,  9   /* [2] 5.25" 360  Kb  */
80, 30, 2400, 15   /* [3] 5.25" 1.20 Meg */
```

Without it the minor is 0, the driver uses 720 Kb geometry with 9
sectors per track, and every seek past that fails against the 18 the
image really has. Measured: `c_intr` sat at `SKFLAG|SKEFLAG`, seek error
recovery. With `BOOTPART=1` the error counters are zero.

This was found using the environment mechanism restored in
"i386/AT386/model_dep.c: populate the environment from the command
line", and is the first concrete payoff from that work -- a pure
configuration fix with no source change.

### Driver state: working

The floppy read completes end to end:

```
rbrate YES   fdseek YES   geteblk YES  setqueue YES  m765io YES
rwintr YES   quechk YES   iowait YES   io_completed YES
```

Two driver bugs were fixed to get here. The reset interrupt drain in
`rstout()` is committed. The geometry is configuration.

# MILESTONE: LITES runs on OSFMK 7.3

```
Lites VERSION(Lites.1.1.u3): Tue Sep 15 04:11:54 PM EDT 2026; STD+WS+osfmach3

Copyright (c) 1982, 1986, 1989, 1991, 1993
        The Regents of the University of California.
Copyright (c) 1992 Carnegie Mellon University.
Copyright (c) 1994, 1995 Johannes Helander (Helsinki University of Technology).
All rights reserved.
```

A 4.4BSD-Lite UNIX personality, loaded off a minix floppy by the OSF
bootstrap task, printing its banner on this kernel.

## The fix: the ELF entry point

LITES links with `-e __start`. This crt0 defines `__start_mach`. The
symbol does not exist, so `ld` says so and carries on:

```
ld: warning: cannot find entry symbol __start; defaulting to 08049000
```

`0x08049000` is the first byte of `.text`, which `nm` identifies as
`ip_setmoptions.cold` -- a cold-path fragment of a networking function.
Every previous boot jumped there and died instantly, before `crt0`,
before `main`, before any console. That is why there was never any
output and why nothing else we tried made any difference.

The fix is one linker flag:

```
LDFLAGS="... --defsym __start=__start_mach"
```

after which `readelf -h` reports entry `0x8049330`, which is
`__start_mach`. Checking that number is the cheapest possible
confirmation and should be done before any boot attempt.

The precedent was in our own build all along. OSFMK's `default_pager`
links with `-e __start_mach -u __start_mach`; that is how a server built
against this crt0 is meant to be linked.

## How it was found

A `-d exec` trace, with user-mode EIPs symbolised against `nm` on the
`startup` binary. The decisive number: **zero** instructions executed
above `0x08066228`, the bootstrap task's `etext`, and the highest
address reached in the whole run was `0x08065cd2`. Since LITES's text
runs to `0x080f709a`, it had plainly never executed its own code.

Four earlier hypotheses were each audited and disproved before this:
that `do_bootstrap_ports` was a stub, that a NULL `argv` was
dereferenced, that crt0's `else` branch skipped thread initialisation,
and that the loader failed to pass arguments. All wrong. The measurement
settled in one run what reading had not settled in four attempts.

## The blocker is the root filesystem, not paging

`init_main.c:349` onward, immediately after the banner:

```c
printf("%s\n", version);
printf(copyright);          /* <- last thing seen on the console */

/* Mount the root file system. */
kr = (*mountroot)();
#if EXT2FS
/* XXX if FFS fails, fall back to EXT2FS */
if (kr == EINVAL)
        kr = ext2_mountroot();
#endif
if (kr != KERN_SUCCESS)
        panic("cannot mount root x%x %s", kr, mach_error_string(kr));
```

The panic is the next statement after the copyright text, and the
copyright text is the last output. There is no other panic between them.
**LITES panics because it has no root filesystem.**

`panic: UWVS+` is that message mangled. `panic` does
`printf("panic: %r\n", fmt, ap)`, and `%r` -- a BSD extension that
re-expands a format string against a `va_list`, implemented at
`subr_prf.c:473` -- destroys the text while the panic itself is real and
correctly placed. Cosmetic, but it cost a detour and is worth fixing.

**The paging messages are a consequence, not the cause.** In the console
log the panic is line 48 and the pager complaints begin at line 49.
`panic()` calls `boot(TRUE, RB_AUTOBOOT|RB_DUMP)`, and `RB_DUMP` asks
for space to write a crash dump, which is what the pager cannot supply.
Reading `swapon suggested` as the blocker sent this investigation in the
wrong direction for a round; the line ordering said otherwise.

`-m 256` changes nothing -- same panic, same position -- which correctly
rules out memory pressure.

## FIXED: LITES's varargs were pre-ANSI; every printf argument was garbage

```
panic args: cannot mount root x9c6 unknown error code
```

A readable panic message with its arguments expanded, for the first
time. The cause was `include/i386/stdarg.h`:

```c
typedef char *va_list;
#define va_start(ap, last) (ap = ((char *)&(last) + __va_promote(last)))
```

Computing the argument pointer by taking the address of the last named
parameter and stepping past it assumes a stack layout the compiler is
not obliged to provide. Modern GCC at `-O2` does not provide it, so
**named parameters read correctly while every variadic argument was
garbage**. It now uses `__builtin_va_list`, `__builtin_va_start`,
`__builtin_va_arg` and `__builtin_va_end`.

### Why this took so long to find

The symptom pointed away from the cause at every step. `fmt` was
verifiably correct -- a hardware breakpoint at `panic` showed
`0x080ed7e6`, which `readelf` confirms is the right string in
`.rodata` -- and yet `printf("%s", fmt)` printed the bytes of an
unrelated function. That combination looks impossible, and six
hypotheses were built trying to explain it: `%r` mangling, unmapped
rodata, partially mapped text, paging pressure, absent backing store,
and a wild `fmt` pointer.

The measurement that broke it open was printing a literal with **no**
arguments from inside `panic`. It printed perfectly, which proved
`printf` worked and narrowed the fault to argument passing rather than
output or pointers.

**A correct value and a corrupt one can coexist** when the corruption is
in the mechanism that transports the value rather than in the value
itself. When evidence looks contradictory, suspect the transport.

### What it unblocks

The panic text is now readable, so every future failure names itself.
The remaining blocker is the one the message states: `x9c6` is
`D_NO_SUCH_DEVICE` on the root mount. "unknown error code" is
`mach_error_string` not knowing the device subsystem, which is cosmetic.

## Measured: the development sandbox boots this in 2 seconds

Not a claim, a measurement. Kernel, bootstrap task and `default_pager`
built in the sandbox and booted there under **pure TCG on one CPU**,
with no hardware acceleration:

```
(bootstrap): loading /dev/boot_device/mach_servers/default_pager
(bootstrap): started
(default_pager): started
```

reached in **2 seconds**.

That is faster than the KVM floppy boot by two orders of magnitude, and
it settles the question the other way round from how it was framed all
session. Emulation speed was never the constraint. The floppy was. A
boot that cost 500 seconds with hardware acceleration costs 2 without
it, once the same data comes off an IDE disk.

Two beliefs shaped this session and both were wrong:

- that 32-bit objects could not be linked in the sandbox
  (`gcc-multilib` installed in one command)
- that emulation without KVM was too slow to iterate on
  (it is 2 seconds)

Neither was ever tested. Both were inferred from a single early failure
and then treated as fixed properties of the world.

### Build notes for the sandbox

The full sequence, after `gcc-multilib`:

```sh
export MK_BUILD=/tmp/hj ODE4LINUX=~/ode4linux
sh build/ode.sh MAKEFILE_PASS=FIRST
sh build/ode.sh -here mach_kernel MACH_KERNEL_CONFIG=PRODUCTION
sh build/ode.sh -here mach_services/lib/libsa_mach
sh build/ode.sh -here mach_services/lib/libcthreads
sh build/ode.sh -here mach_services/lib/libmach
sh build/ode.sh -here mach_services/lib/libmach_maxonstack
sh build/ode.sh -here file_systems          # libsa_fs, needed by bootstrap
sh build/ode.sh -here bootstrap
sh build/ode.sh -here default_pager
```

One trap: the exported `mach/default_pager_object.h` can be installed
without the `import <mach/default_pager_types.h>` line that MIG emits,
and then `default_pager` fails with
`DEFAULT_PAGER_BACKING_STORE_MAXPRI undeclared`. The generated copy
under `obj/at386/default_pager/mach/` has it; copying that over the
exported one fixes the build. `-I.` does not help, because it precedes
`-I-` and so serves only `""` includes, not `<>` ones.

## The development sandbox can build and run this after all

`gcc-multilib` was installable the whole time. The belief that 32-bit
objects could not be linked in the sandbox was formed early from a
single failure, never rechecked, and shaped the entire session: every
LITES build and every boot was handed back and forth instead of being
run where the analysis was happening.

```sh
apt-get install -y --no-install-recommends gcc-multilib
gcc -m32 -print-libgcc-file-name    # .../13/32/libgcc.a
```

Verified by compiling, linking and running a 32-bit binary that uses the
64-bit division helpers LITES needs.

Combined with the 20-second IDE boot, the whole cycle -- build LITES,
build the server volume, boot, read the console -- can now run in one
place.

## The boot cycle is now 20 seconds, down from ~500

Booting `/mach_servers` from IDE works end to end: pager loaded, LITES
loaded, banner printed, `added device hd1c`, all in **20 seconds**
against roughly 500 from floppy. Every future measurement is 25x
cheaper, which changes what is worth attempting -- an experiment that
costs twenty seconds can be run on a hunch, where one costing ten
minutes cannot.

Run it with:

```sh
export MK_BUILD=~/.cache/mk7.3
sh tools/boot-ide.sh
```

## Booting the servers from IDE

`tools/boot-ide.sh` boots `/mach_servers` from a minix volume on a third
IDE disk instead of the floppy, which takes the cycle from about 500
seconds to a few. The floppy read was I/O bound, so hardware
acceleration never helped it.

```
hd0  ext2   LITES root
hd1  raw    paging, given to default_pager as hd1c
hd2  minix  /mach_servers, booted from
```

`-append "-r BOOTDEV=hd BOOTUNIT=2 BOOTPART=2 -o"`. `model_dep.c` joins
`BOOTDEV` and `BOOTUNIT`, looks the name up, and adds `BOOTPART` to the
unit; `dev_name_lookup` computes `unit * d_subdev + partition`, and
`d_subdev` is 16 for `hd`, so this is minor 34 -- unit 2, partition `c`,
the whole-disk fallback.

The minix reader needed no change: every access in
`file_systems/minixfs/minixfs.c` goes through
`device_read(fp->f_dev.dev_port, ...)`, so it is device-agnostic.

### Two problems this surfaced

**Names longer than 14 bytes.** A minix v1 directory entry holds 14, and
LITES's binary name is 43. The manual flow had always copied it to
`/tmp/startup` first. `mkminix.py` now takes `path:name=args` and
renames on the way in, which keeps the directory entry and the generated
`bootstrap.conf` in step by construction.

**The filesystem was always 1.44 MB.** `nzones` was hardcoded to 1440
and `bytearray(nzones * BS)` replaced whatever the image had been, so a
16 MB disk image came back out as a 1.44 MB file. On a floppy that was
invisible. On a disk it is not: the `hd` driver takes its geometry from
IDENTIFY, so the kernel believes the disk is its full size, and reads
past the end of a shorter backing file fail. The bootstrap task reported

```
(bootstrap): unloadable file format (result = 0x9c6)
```

-- `D_NO_SUCH_DEVICE` again -- while loading the 995 KB server, having
loaded the 211 KB pager without trouble. `mkminix.py` now sizes the
filesystem from the image file, capped at 65535 zones since minix v1
zone numbers are 16-bit.

## Superseded: LITES asks for hd0a

`kr = 0x9c6` is **2502 = `D_NO_SUCH_DEVICE`**. A hardware breakpoint on
`device_open`, printing the name at each call, showed what LITES
actually asks the kernel for:

```
"console"  "time"  "console0"  "hd0a"
```

**`hd0a`, not `hd0c`.** Partition `a` does not exist on an unpartitioned
disk -- `getvtoc` falls back to making partition `c` the whole disk --
so `hdopen` refuses and the root mount fails.

The `default_root[] = "hd0c"` patch was real but irrelevant on this
path. `server_init.c:711` holds a **second** compiled-in configuration:

```c
char argv_space[10][40] = {"/dev/hd0f/mach_servers/startup",
                           "-s",
                           "hd0a",                      /* the root device */
                           "/dev/hd0a/mach_servers",
                           (char *)0,};
...
parse_arguments(4, foo_argv);   /* XXX */
```

`get_config_info()` hands these to `parse_arguments` as **argc 4**, so
these four strings are the configuration and `default_root` is never
consulted. That also explains two older puzzles: `parse_arguments`'
`if (argc == 0) return` guard never fires, and `-s` is already applied
from `argv_space[1]` rather than from the `bootstrap.conf` attempt.

The patch series now sets `hd0c` in both entries. `hd0f` in entry 0 is
left alone: it only derives a path when none is given, and entry 3
supplies one.

### How this was found, and why it is worth noting

Four steps, each a measurement rather than a hypothesis:

1. `hbreak` at `panic`, read the stack -> `fmt` is valid, `kr = 0x9c6`
2. decode `0x9c6` -> `D_NO_SUCH_DEVICE`
3. `hbreak` at `device_open`, print the name at each call -> `hd0a`
4. `grep` for `hd0a` -> a second hardcoded table

**Each step was a measurement rather than a hypothesis, which is why it
took four steps instead of the six failed rounds before it.** Those six
-- `%r` mangling, unmapped rodata, partial text mapping, paging
pressure, absent backing store, and a wild `fmt` pointer -- were each
plausible, each argued from evidence already in hand, and each wrong.
The difference was not cleverness. It was that steps 1 to 4 each
produced a new fact, and the six rounds before them each produced a new
interpretation of the same facts.

## Superseded: the panic is "cannot mount root"

A hardware breakpoint at `panic` caught the call with LITES current:

```
eip 0x80aa350   esp 0x4fe78
0x4fe78:  0x08069a1a  0x080ef7e6  0x000009c6  0x080f16ad
          return       fmt         arg1        arg2
0x80ef7e6: "cannot mount root x%x %s"
```

**`fmt` is perfectly valid.** It points at the right string, in
`.rodata`, fully readable, with the arguments behind it. The panic is
the one in `init_main.c`, and `kr` is `0x9c6`.

**So `printf` is the broken thing, not the pointer.**
`printf("panic: %s\n", fmt)` printed `UWVS1+` while `fmt` pointed at
correct text. Six hypotheses were built on reading that garbage as
evidence about the pointer; the pointer was never wrong. The `-z
muldefs` collision between LITES's `printf` in `server/kern/subr_prf.c`
and `libsa_mach`'s is the only remaining explanation, and it is now
confirmed by elimination rather than assumed.

That also retires the `UWVS` analysis. `fmt` never pointed at code; the
`UWVS` text was produced by a `printf` reading from somewhere other than
its argument.

### The real blocker: error 0x9c6 from the root mount

`0x9c6` is 2502 decimal. It is a **Mach** error code, not an errno, so
the earlier reasoning about `EIO` versus `EINVAL` was about the wrong
kind of value entirely -- `(*mountroot)()` is returning a Mach error
from the device layer, not a BSD errno from a filesystem check.

Decode it before anything else. `mach_error_string` was already called
on it, and its result is on the stack at `0x080f16ad`, so the text is
available in the failing image.

### Method notes

**`hbreak`, not `break`, for a task that is not current.** A software
breakpoint must write `int3` into the target page, so it can only be set
while that page is mapped in the current context -- for a user task,
only while that task is scheduled. A hardware breakpoint uses the CPU's
debug registers and needs no memory access at all. In a microkernel,
where the task of interest is one of several and rarely current when you
attach, **`hbreak` is the default choice and `break` is the special
case.** Several attempts were lost to this.

**A broken instrument corrupted six rounds of reasoning.** The garbage
string was treated as data about the program. It was data about
`printf`. Fixing the instrument first -- which was attempted, but with
`%s` through the same broken `printf` -- would have needed an
independent output path to be conclusive.

## Superseded: the pager has a backing store

```
(default_pager): added device hd1c
```

The argument chain works end to end for the first time:
`bootstrap.conf` -> the bootstrap task -> crt0's `bootstrap_arguments()`
RPC -> `main(argc, argv)` -> `bs_add_device()`. The pager now has 32 MB
of backing store on a second IDE disk.

**`ps_allocate_cluster` is gone.** Every previous boot ended with four of
those messages; this one has none.

**The LITES panic is unchanged** -- still `UWVS1+`, in the same place.
That is now a clean result rather than a disappointing one: paging was
never implicated, and the last confounding symptom has been removed.

### What remains

`fmt` is a **code address passed where a format string was expected**.
`UWVS` is `55 57 56 53`, the i386 prologue
`push ebp; push edi; push esi; push ebx`, which `strings` renders as
text; it appears at thousands of offsets in the binary because every
function starts with it. The trailing bytes differ between builds
because the following instructions shift.

So some caller reaches `panic` with a pointer to code in the format
argument. Candidates, none yet tested:

- a call through a function pointer where the callee's signature
  differs from the caller's expectation
- an argument list misaligned by one slot, so a code pointer lands where
  `fmt` should be
- a `panic` reached from library code -- `libsa_mach` and `libmach` both
  provide one, and `-z muldefs` resolves the clash by link order, so a
  library caller may be reaching LITES's `panic` with different
  conventions

The measurement that settles it is the return address on the stack at
`panic` (`0x080aa350`), which `nm` turns into the calling function.
Getting it needs gdb attached **after** LITES starts and **before** it
panics; attaching earlier finds kernel space, and attaching later finds
the task gone.

## Superseded: how a server gets arguments

The chain is now mapped end to end, and every link was read in source:

```
bootstrap.conf              lines are  [-flags] symtab_name path [args...]
  -> bootstrap task         parse_config_file() stores per-server argv
  -> crt0 __get_arguments() calls bootstrap_arguments() over IPC
  -> main(argc, argv)
  -> default_pager          bs_add_device(*argv, master_device_port)
```

`default_pager`'s `main()` loops `while (--argc > 0)` calling
`bs_add_device()` on each non-flag argument. With `argc == 0` the loop
never runs, **so it starts with no paging segment at all** and every
`ps_allocate_cluster()` fails. That is the message we have been seeing
since the first LITES boot.

The zero-filled stack servers start on is deliberate (a "dummy 0
argument count"); real arguments arrive later, by RPC, from
`bootstrap.conf`. So giving the pager a disk needs **no code change** --
only a config line.

**It also explains why `-s` never reached LITES.** `parse_boot_args()`
consumes leading `-X` flags as the *bootstrap task's* own options
(`-k`, `-S`, `-w` ...) and does not pass them on. A server flag written
at the start of a config line is silently eaten.

`tools/mkminix.py` now accepts `path=args`:

```sh
python3 tools/mkminix.py /tmp/minix.img \
    $MK_BUILD/obj/at386/default_pager/default_pager=hd1c \
    /tmp/startup
```

writing `default_pager default_pager hd1c`.

**`hd1c`, not `hd0c`**: `hd0c` is the whole first disk and will hold the
root filesystem, so paging there would destroy it. The paging device
must be a second disk, attached as `-drive ...,if=ide,index=1`.

## Superseded: four hypotheses dead

The LITES panic after the banner has now survived four explanations,
each killed by measurement. Recording them so nobody retries them:

| hypothesis | how it died |
|---|---|
| `%r` mangles the message | `printf("panic: %s", fmt)` prints the same garbage -- `%r` is not involved |
| `.rodata` is not mapped | segment arithmetic checks out; `text_size` covers rodata exactly, and the contiguous path is taken |
| LITES's text is partly mapped | the unreadable addresses are simply not yet demand-paged; readable ones are those already executed |
| paging pressure | 128 MB behaves identically to 64 MB -- same panic, same messages, log byte-identical in length |

### What the garbage actually is

`strings` on the binary shows `UWVS` at thousands of offsets. It is not
a string: `55 57 56 53` is the i386 function prologue
`push ebp; push edi; push esi; push ebx`. So `fmt` points **into code**,
at or near a function entry. The varying suffix between builds
(`UWVS+`, `UWVS\002k`, `UWVS1+`) is just the following instruction bytes
shifting as the binary changes.

`fmt` is therefore a **code address passed where a format string was
expected** -- not corruption, but a wrong pointer.

### Two facts established along the way

**The kernel does not boot with 512 MB.** It prints
`cnvmem: 639 KB, extmem: 523136 KB, mem_size 523772 KB` and stops --
no `Kernel virtual space` line follows. 64 MB and 128 MB both work, so
the limit is between 128 MB and 512 MB. Likely `vm_page_bootstrap` or
`pmap` not scaling. Worth a separate investigation; for now, stay at or
below 128 MB.

**Paging pressure is not the constraint.** Doubling memory changed
nothing, so `ps_allocate_cluster: no space in available paging segments`
is not about a *small* backing store. It is about there being **none**.
Nothing in this configuration ever calls
`default_pager_add_segment` or `default_pager_backing_store_create` with
a real device, so the pager starts with zero paging segments and fails
the first time anything needs one.

### Next

Give `default_pager` a backing store. This is required work regardless
of whether it is the current blocker -- a Mach system with no swap
cannot page anonymous memory at all, and the IDE disk is now working
well enough to serve as one.

## Superseded: the panic message is garbage

The whole LITES tree builds -- server, ext2, emulator. The boot still
panics after the banner, and `ext2_mountroot` is now present in the
binary (`nm` confirms it), yet no ext2 diagnostic appears.

**The panic text changed between builds:**

```
UWVS+        (earlier build)
UWVS\002k     (this build)
```

Identical boots, different text. So it is not a message at all --
`%r` is printing whatever memory it lands on. Every earlier inference
from `UWVS+` being "stable across runs" was wrong: it was stable because
the binary was unchanged, not because the text was real.

That matters because **several panics are reachable at this point in
boot** -- `"cannot mount root"` in `init_main.c`,
`"ffs_mountroot: can't setup bdevvp's"` in `ffs_vfsops.c`, and others --
and without readable text there is no way to tell which fired. The
unreadable message has been obscuring the diagnosis for several rounds.

### Why `%r` misbehaves

`panic` does `printf("panic: %r\n", fmt, ap)`, where `%r` re-expands a
format string against a `va_list`. Two `printf` implementations are
linked into this server -- LITES's own in `server/kern/subr_prf.c` and
`libsa_mach`'s -- and `-z muldefs` resolves the clash by link order, so
which one handles `%r` is not obvious and may not be the one that
implements it.

### The fix

Rather than untangle that, `panic` now prints the format string with
`%s` first and then attempts `%r` separately:

```c
printf("panic: %s\n", fmt);
va_start(ap, fmt);
printf("panic args: %r\n", fmt, ap);
va_end(ap);
```

The first line always identifies which panic fired. The second still
shows the arguments when `%r` works, and is harmless when it does not.

An unformatted message that names the panic is worth more than a
formatted one that cannot be read.

## Superseded: the server builds, emulator string literals

Every error is now in `emulator/`, which has never been built in this
project. **`server/` is complete, including ext2.**

`include/sys/exec_file.h` builds a table of binary-type names:

```c
#define ATSYS_NAMES(m) \
    m ## "bad", m ## "lites", ...
```

called as `ATSYS_NAMES("i386_")`. The `##` operator pastes
*preprocessing tokens*, and two string literals cannot be pasted into
one:

```
error: pasting ""i386_"" and ""bad"" does not give a valid
       preprocessing token
```

Older preprocessors tolerated it. None is needed: **adjacent string
literals are concatenated by the compiler**, so removing `##` gives
`"i386_" "bad"`, which is `"i386_bad"` -- exactly the intent. Verified
by inspecting the preprocessor output.

## Superseded: `#if linux`

With the asm fixed, `ext2_linux_ialloc.c` failed on Linux kernel idioms:

```
error: 'struct inode' has no member named 'i_sb'
error: 'struct inode' has no member named 'u'
error: too few arguments to function 'bread'
error: too many arguments to function 'mark_buffer_dirty'
```

Every error was inside one `static` function, `inc_inode_version`,
which is **defined once and called from nowhere** -- a fragment of
Linux's ext2 that came across with the file and was never wired up.

And it is already guarded:

```c
#if linux
...
static void inc_inode_version (struct inode * inode, ...)
...
#endif /* linux */
```

In 1995 an undefined identifier in `#if` evaluates to 0, so the block
was excluded and nobody ever noticed it would not compile.

**GCC predefines `linux = 1` in its GNU dialects.** Measured:

| flag | `linux` defined |
|---|---|
| `-std=gnu89` | yes |
| `-std=c89` | no |

So `-std=gnu89` -- **the flag added earlier in this port to make GCC 14
accept K&R function definitions** -- turned the guard on and started
compiling Linux-only code into a BSD server. One era-gap fix created
another.

The remedy is `-Ulinux`, not editing the guard: the source is correct
and the predefine is an accident of the host. `-std=c89` would also work
but loses GNU extensions this code needs elsewhere.

## Superseded: ext2's inline asm

After the string-literal fix below, the same header failed differently:

```
i386-bitops.h:81:9: error: 'asm' operand has impossible constraints
                    or there are not enough registers
```

three times -- once per call site where the `extern inline` was
instantiated.

**The measurement that identified it.** Two candidate causes, tested one
flag at a time:

| build | impossible-constraint errors |
|---|---|
| `-O2 -fomit-frame-pointer` | 3 |
| `-O0` | 0 |

Freeing EBP made no difference, so it is not frame-pointer pressure.
Failing only with the optimiser on is the signature of a **constraint
bug**, not of genuinely insufficient registers.

**The bug.** `find_first_zero_bit` declared ECX and EDI as inputs *and*
as clobbers:

```c
:"=d" (res)
:"c" (...), "D" (addr), "b" (addr)      /* ECX, EDI, EBX in      */
:"ax", "cx", "di");                     /* EAX, ECX, EDI clobbered */
```

A register cannot be both: the compiler has to set an input up and have
the value survive until the asm reads it, which a clobber declaration
contradicts. The block really does modify both -- `repe` decrements ECX
and `scasl` advances EDI -- so they are **read-write** operands, which
is spelled as early-clobber outputs tied to matching inputs. EAX is
written by the `movl`, so it is an output too rather than a clobber.

Older GCC tolerated the original. Modern GCC rejects it at `-O2` and
accepts it at `-O0`, which is exactly the pattern observed.

Verified in isolation, with three callers of increasing register
pressure:

| constraints | `-O0` | `-O2` | `-O3` |
|---|---|---|---|
| original | pass | **fail** | -- |
| rewritten | pass | pass | pass |

Only the first of the file's three asm blocks was wrong; the second uses
plain `"=r"`/`"r"` and the third already uses matching `"0"`/`"1"`
operands.

## Superseded: ext2 now compiles, multi-line asm string literals

With `ext2fs` enabled the ext2 sources build for the first time in this
project, and `server/ufs/ext2fs/i386-bitops.h` failed immediately:

```
i386-bitops.h:81:17: error: missing terminating " character
i386-bitops.h:86:20: error: invalid suffix "f" on integer constant
```

Three inline assembly blocks are written with **raw newlines inside the
string literal**:

```c
	__asm__("
		cld
		movl $-1,%%eax
		...
		addl %%edi,%%edx"
		:"=d" (res) ...);
```

K&R compilers accepted that. Modern C requires each line to end `\n\`,
which keeps the literal legal while still giving the assembler the
newlines it needs.

**This is the third instance of the same construct in this project**,
after `conf/gensym.awk` and `conf/newvers.sh`. It is worth recognising
on sight: `missing terminating " character` together with
`invalid suffix "f" on integer constant` -- the latter because a local
assembler label like `1f` ends up parsed as C once the string breaks.

The patch series converts all three blocks. Verified: the header passes
`gcc -m32 -std=gnu89 -fsyntax-only` standalone with zero errors.

## Superseded: EXT2FS was never compiled in

Before the EIO/EINVAL question below matters at all, the ext2 reader has
to exist in the binary, and it did not.

`ext2fs` is an option in LITES's `conf/MASTER`:

```
options		ext2fs	EXT2FS	1	ext2fs.h
```

and it is **not** part of the `STD+WS` set. Our configure line was
`--with-config="STD+WS+osfmach3"`, which produced

```
config lites+mtime+muarea+file_ports+vnpager+old_synch+ether+inet+ffs
      +pty+second_server+syscalltrace+compat_43+compat_oldsock+kernfs
      +nfs+atsys+i386+iopl+com+osfmach3
```

`ffs` is present; `ext2fs` is absent. The generated
`<builddir>/obj/server/ext2fs.h` therefore contains

```c
#define EXT2FS 0
```

so no `ext2_*.o` objects are built and the entire
`#if EXT2FS ... ext2_mountroot() ... #endif` block in `init_main.c` is
compiled out. **The ext2 reader was never in the binary.**

That is the real reason no `"Wrong magic number"` diagnostic ever
appeared, and it means the `EIO` versus `EINVAL` fix below, while
correct, was inert.

`tools/lites/build-lites.sh` now configures with
`--with-config="STD+WS+osfmach3+ext2fs"`. Check it took:

```sh
cat <builddir>/obj/server/ext2fs.h     # want: #define EXT2FS 1
```

### Method note

This should have been the first check, not the third. The question
"does the code I am debugging exist in the binary at all" is cheaper
than any reasoning about its behaviour, and two rounds were spent
analysing a code path that was not compiled. `docs/METHODOLOGY.md`
§6.2 says to check this; it was not applied here.

## Then: the ext2 fallback needs EIO as well as EINVAL

LITES's root mount tries FFS first and falls back to ext2:

```c
kr = (*mountroot)();            /* ffs_mountroot */
#if EXT2FS
/* XXX if FFS fails, fall back to EXT2FS */
if (kr == EINVAL)
        kr = ext2_mountroot();
#endif
if (kr != KERN_SUCCESS)
        panic("cannot mount root x%x %s", kr, mach_error_string(kr));
```

But `ffs_mountfs` returns **`EIO`**, not `EINVAL`, when the superblock
magic is wrong:

```c
if (fs->fs_magic != FS_MAGIC || ...) {
        brelse(bp);
        return (EIO);           /* XXX needs translation */
}
```

`EIO != EINVAL`, so on a disk holding an ext2 filesystem the fallback
never fires and `ext2_mountroot` never runs. **The ext2 filesystem was
never looked at.**

That explains an observation that had resisted explanation: LITES's
console works, and `ext2_vfsops.c` has a `"Wrong magic number"`
diagnostic, yet no such message ever appeared. It could not -- the code
was never reached.

The same file is inconsistent about which errno means "not my
filesystem": `ffs_vfsops.c:214` sets `EINVAL` with the same
"needs translation" comment, and `:261` returns `EINVAL` outright. The
`XXX` on the `EIO` return is the author flagging exactly this.

The patch series now widens the caller's test to accept both, which
keeps the policy in the caller and leaves FFS's behaviour untouched for
anything else depending on it.

### What this does and does not tell us

It does **not** mean the ext2 image is good. It means we have not yet
found out. The first real test of the filesystem comes after this
change, and `ext2_vfsops.c` will say what it thinks:

- `"Wrong magic number: %x (expected %x for ext2 fs"` -- the read
  worked and the filesystem is wrong, or the read returned garbage
- no magic complaint but a later failure -- the superblock is fine and
  something deeper is wrong
- a successful mount -- done

Note that a magic complaint would also settle a separate open question:
whether the IDE read returns **valid** data. FFS rejecting a superblock
is consistent both with a good read of a non-FFS disk and with a garbled
read. The reported magic value distinguishes them -- `0xef53` means the
read is correct.

## Superseded: the IDE path is clean, the filesystem is not accepted

The nIEN fix works. `HD: false interrupt` went from two occurrences to
one, and the one that remains is at line 23, **before** the `entry:`
line -- probe time. The second, which previously appeared between
LITES's copyright banner and the panic, is gone.

That was the harmful one: the `CMD_SETPARAMETERS` interrupt latched by
the PIC and delivered after `controller_busy` was set, which `hdintr`
consumed as the read's completion. With `nIEN` set around the polled
command it is never raised, and the read is no longer corrupted by it.

The remaining probe-time message is `CMD_IDENTIFY`'s, and harmless --
the controller is idle, `hdintr` discards it. `nIEN` is set around that
command too, so its survival suggests the drive asserts INTRQ once
before the control register write takes effect, or that QEMU latches it
regardless. Not worth chasing: it is discarded and nothing depends on
it.

**The panic is unchanged**, which is now informative rather than
discouraging. The interrupt path is clean, the geometry is right, the
partition opens, and the read is no longer being corrupted -- so the
failure has moved to the filesystem itself. LITES's 1995 ext2 reader
does not accept what modern `mke2fs` produces.

That is the same family as the minix `0x137F` magic: a reader written
against a 1995 on-disk format meeting a modern formatter's defaults. The
image was made with

```sh
/sbin/mke2fs -q -F -b 1024 \
    -O ^resize_inode,^dir_index,^ext_attr,^sparse_super -I 128 root.img
```

which is already conservative, but has not been checked against what
`server/ufs/ext2fs` actually reads.

## Superseded: the IDE disk opens, the read does not complete

The geometry work is done and validated. `hd0` now reports

```
hd0: 20 Meg, C:40 H:16 S:63 - QEMU HARDDISK
```

where it previously said `0 Meg, C:0 H:0 S:0`. Two commits did it:
sizing the whole-disk partition from the label rather than the empty
BIOS table, and falling back to IDENTIFY's default geometry words when
the "current" words read zero.

LITES now reaches the disk. The console ordering is the evidence:

```
Copyright (c) 1994, 1995 Johannes Helander ...

HD: false interrupt          <- new; was not here before
panic: UWVS+
```

Previously the panic followed the banner immediately. The interrupt
between them means `hdopen` got far enough to begin a transaction, so
the failure has moved from "device unopenable" to "device opened, read
does not complete".

### The signal: exactly two false interrupts

```
line 23:  HD: false interrupt     (after "battery clock configured" -- probe)
line 53:  HD: false interrupt     (after LITES's copyright -- first real I/O)
```

and **nothing else**: no `no bp buffer`, no
`hdintr: interrupt w/controller not done`. So the handshake is not
broadly broken; two specific interrupts arrive unexpectedly.

`hdintr` prints this when `controller_busy` is false (`hd.c:955`), then
dumps registers and discards the interrupt without processing it.

An interrupt arriving **after** IDENTIFY completes is the signature of a
polled command: the driver polls `PORT_STATUS` until not-busy, reads the
data and returns without ever setting `controller_busy`, and the
controller then raises IRQ 14 at a driver that has stopped listening.
That leaves an interrupt pending on the controller, and per ATA the next
command's interrupt can be lost or misattributed -- which would explain
why LITES's first real read never completes.

This is the **same defect class as the floppy's reset interrupt**, fixed
earlier in `rstout`: OSFMK's drivers assume controllers are forgiving
about when interrupts arrive relative to status polling, and QEMU
asserts them strictly per spec.

### Where to start next

1. Does the IDENTIFY path in `hd_ssend` set `controller_busy`? If not,
   the first false interrupt is explained outright.
2. Does anything clear the pending interrupt after a polled command?
   The ATA way is to read the status register, which
   `hd_dump_registers` may already do incidentally on the
   false-interrupt path -- or may not.

The precedent for the fix is `rstout`, which drains the 82077's reset
interrupt with four `sis()` calls because the controller will not accept
further commands while one is pending. The IDE equivalent is clearing
the pending interrupt after a polled command rather than leaving it for
the next one to trip over.

Not yet established: whether the filesystem itself is acceptable to
LITES's 1995 ext2 reader. That question cannot be reached until a read
completes, and it is a separate problem of the same family as the minix
`0x137F` magic.

### The root device: hd0c, and why

`server_init.c:301` had `char default_root[] = "hd0a"`, used because
`argc == 0`. The patch series now makes it `hd0c`, and the reason is in
the kernel's `hd` driver.

`hdopen` refuses unless `getvtoc(dev)` succeeds **and** the partition
has non-zero size. `getvtoc` builds the partition table by calling
`read_bios_partitions(dev, 0, ...)` -- reading sector 0 as a DOS/BIOS
partition table -- and when that fails it does this:

```c
/* make partition 'c' the whole disk in case of failure */
label->d_partitions[PART_DISK].p_offset = 0;
label->d_partitions[PART_DISK].p_size =
        ncyl * nheads * nsec;
```

`PART_DISK` is 2 (`disk.h:149`), and `dev_name_lookup` maps partition
letters `a`-`h` onto indices 0-7, so index 2 is `c`.

So **an unpartitioned disk image gives `hd0c` = the whole disk**, with
no MBR, no BSD disklabel and no partition arithmetic to get right.
`hd0a` would have required a real DOS partition table.

Note also that this driver is **CHS, not LBA**: `hd_ssend` computes
sector, head and cylinder from `label->d_nsectors` and `d_ntracks`, so
the geometry QEMU presents has to be consistent with what the driver
reads from CMOS.

### What is needed

A filesystem LITES can mount as root. The code above takes **either**:

- BSD FFS, via `mountroot`
- **ext2**, via `ext2_mountroot`, tried when FFS returns `EINVAL`

ext2 is far easier to produce on a modern Linux host, and unlike the
kernel's minix reader there is no exotic magic requirement to satisfy --
this is LITES's own ext2 implementation, not the bootstrap task's.

The root device comes from `rootname`/`rootdev_name` in
`server_init.c`, with a compiled-in default in `argv_space` beginning
`/dev/hd0f/mach_servers/startup`, so selecting the device is a separate
question from creating the filesystem.

## Superseded: the new blocker, no paging store

```
panic: UWVS+
(default pager): ps_allocate_cluster: no space in available paging
                 segments; swapon suggested
```

`default_pager` starts but has no backing store, so the first demand for
anonymous memory has nowhere to go. The pager says what it needs:
a swap device, via `default_pager_add_segment` or
`default_pager_backing_store_create`, neither of which anything
currently calls with a real device.

Worth trying first, as a one-word change: more memory, `-m 256` rather
than `-m 64`, which may push the first paging event past
initialisation.

`panic: UWVS+` is not yet decoded. LITES's `panic` does
`printf("panic: %r\n", fmt, ap)`, where `%r` is a BSD kernel extension
for recursive format expansion; the string appears nowhere in the source
as a literal, so this may be `%r` being mishandled rather than a real
message. Check before reading anything into it.

## Superseded: RETRACTED, the argument frame is correct by design

The note below claimed `i386/set_regs.c` fails to write an argument
block and called it a bug. That is wrong, and this retraction is kept
because the claim was committed and pushed.

`load.c:459` says what the block is for:

```c
/*
 * Allocate space for:
 *    dummy 0 argument count
 *    dummy 0 pointer to arguments
 *    dummy 0 pointer to environment variables
 *    and align to integer boundary
 */
arg_len = sizeof(int) + 2 * sizeof(char *);
```

The zeros are **deliberate**. `vm_allocate` zero fills, and
`uesp = stack_end - 0x10` leaves sixteen zero bytes where twelve are
needed, so the task receives exactly the intended
`argc = 0, argv = NULL, envp = NULL` frame. The `/* XXX */` marks the
hardcoded sixteen rather than using `arg_len`; it does not mark missing
data. HP700 computing `stack_start + arg_size + 32` is the same idea
spelled differently, not evidence of an unfinished i386 port.

Servers here are **designed** to start with no arguments. LITES is built
for that: `init_second_server_flag` returns on `argc <= 0`,
`parse_arguments` returns on `argc == 0`, and `get_config_info` falls
through to `host_get_boot_info`, with a hardcoded default in
`argv_space[10][40]` beginning "/dev/hd0f/mach_servers/startup".

So adding `-s` to `bootstrap.conf` was never going to reach LITES, but
not because of a defect -- the mechanism simply is not arguments. How
LITES is meant to be configured is through the kernel boot info and its
compiled-in defaults, and that is the thread to pull next.

## Superseded claim follows

LITES loads, is resumed, and terminates before any output. The argument
path is broken, and that is proven; whether it is the whole cause of the
termination is not yet proven.

## What is proven

`src/bootstrap/load.c:466` computes the size of an argument block:

```c
arg_len = sizeof(int) + 2 * sizeof(char *);   /* argc + argv[0] + NULL */
arg_len = (arg_len + (sizeof(int) - 1)) & ~(sizeof(int)-1);
...
set_regs(master_host_port, user_task, user_thread, &ofmt.info,
         mapend, arg_len);
```

Nothing writes that block. There is no `vm_write`, no copy, nowhere in
`load.c` or `bootstrap.c` that puts `argc`, `argv` or `envp` into the
new task's memory.

`i386/set_regs.c` then discards the size as well:

```c
(void)vm_allocate(user_task, &stack_start, ..., FALSE);  /* zero filled */
regs.eip  = lp->entry_1;
regs.uesp = stack_end - 0x10;      /* XXX */
```

`arg_size` appears only in the parameter list, never in the body.

Three things corroborate that this is unfinished rather than intended:

- **HP700 honours it**: `HP700/set_regs.c:82` reads
  `regs.sp = ((stack_start + arg_size + 32) & ~(sizeof(int)-1));`
- the i386 offset carries the author's own `/* XXX */`
- the parameter exists at all

So every server the bootstrap task loads starts on a freshly
`vm_allocate`d, zero-filled stack with no arguments.

**This immediately explains one observation.** Adding `-s` to
`bootstrap.conf` changed nothing, because the flag never reaches the
server: `init_second_server_flag(argc, argv)` and LITES's `parse_args`
both see `argc == 0`.

This is the same defect, one level up, as the one already fixed in
`kern/bootstrap.c`, where `user_bootstrap_old` did not build argc and
argv for the bootstrap task and crt0 gates on `kargv[0]`. The kernel
loads the bootstrap task; the bootstrap task loads the servers; both
loaders had it.

`i386/set_regs.c` is byte identical to MkLinux's, so this is not
something introduced here.

## What is NOT yet proven

That this is why LITES terminates. With a zero-filled stack crt0 will
read `argc == 0` and an `argv` pointing at zeros, so `argv[0]` is NULL;
whether LITES dereferences it before its console exists has not been
measured. The next step is a `-d exec` trace symbolised against `nm` on
the `startup` binary, to find the last user-mode symbol reached.

## Superseded: MILESTONE, the OSF multiserver userland runs

```
(bootstrap): loading /dev/boot_device/mach_servers/name_server
ELF: Unknown program header flags 0x4
ELF: Unknown program header flags 0x4
(bootstrap): loading /dev/boot_device/mach_servers/default_pager
ELF: Unknown program header flags 0x4
ELF: Unknown program header flags 0x4
(bootstrap): started
(name_server): started
(default_pager): started
```

Both servers -- `name_server` at 140,396 bytes and `default_pager` at
211,100 bytes -- are read off a minix floppy, ELF parsed, loaded and
started. The whole chain works:

```
kernel boots -> VM, IPC, devices -> user task at ring 3
-> cthreads init -> Mach IPC -> console device
-> floppy driver -> minix filesystem
-> /mach_servers/bootstrap.conf read and parsed
-> name_server  loaded and STARTED
-> default_pager loaded and STARTED
-> (bootstrap): started
```

That takes about 34 minutes of wall time in this environment. See the
timing note below before concluding anything is wrong.

## The invocation

```sh
qemu-system-i386 -kernel mach_kernel.PRODUCTION \
    -append "-r BOOTDEV=fd BOOTPART=1 -o" \
    -initrd bootstrap -fda minix.img \
    -serial file:/tmp/console.log -display none -no-reboot -m 64
tail -f /tmp/console.log
```

`-r` selects the serial console, which gives full scrollback. Do not use
`tools/vgadump.py` for this -- it reads a 25 line framebuffer that
scrolls.

## Building the image

```sh
python3 tools/mkminix.py minix.img \
    $MK_BUILD/obj/at386/mach_services/servers/netname/name_server \
    $MK_BUILD/obj/at386/default_pager/default_pager
```

`tools/mkminix.py` both **formats and populates**, so `mkfs.minix` is
not needed -- which matters, because Debian 13 dropped it from
util-linux and it is no longer packaged there at all.

Formatting in the tool also removes a trap. The reader,
`file_systems/minixfs/minixfs.c:560`, accepts only `MINIX_SUPER_MAGIC`
`0x137F`, the original 14-character-name variant, while `mkfs.minix -1`
defaults to 30-character names and magic `0x138F`, which is silently
rejected.

Two things are easy to get wrong. `minixfs.c:560` accepts only
`MINIX_SUPER_MAGIC` `0x137F`, and `mkfs.minix -1` defaults to 30
character names giving `0x138F`, so **`-n 14` is required**. And
`tools/mkminix.py` writes directories and files by hand, including
single indirect blocks for files over 7 KB, because a loop mount needs
privileges the build environment does not have.

The servers must be built first:

| server | needs |
|---|---|
| `name_server` | `mach_services/lib/libservice` |
| `default_pager` | libcthreads, libsa_mach, libmach, libmach_maxonstack |

## Still open

**`BOOTPART=1` is required and should not be.** The boot device minor is
`unit + BOOTPART`, and `MEDIATYPE(dev)` is `dev & 0x03`, indexing
`m765f[]`; without it the driver picks entry [0], 720 Kb with 9 sectors
per track, and every seek past that fails. Selecting 1.44 Meg through a
partition number is a workaround, not a fix.

**Floppy throughput.** Measured both ways: with KVM the full boot to
three servers takes **a few minutes**; without it, on TCG with a single
CPU, it takes about **35 minutes**, because the guest advances at
roughly a sixth of wall clock. So most of the apparent slowness in the
development logs was the environment, not the driver -- but the floppy
is still slow in absolute terms even with KVM, so there may be a real
driver inefficiency underneath. It does not block anything.

Note for anyone reading the development history: a long series of
"it hangs in X" conclusions in `docs/archive/` were all this. Nothing
was hung; the samples were taken too early.

**Unrecognised CPU.** The console prints
`Unrecognized processor (type = 0x0, family = 0x6, model = 0x6)` on a
modern host. The identification table predates the processor. Harmless
-- the machine configures and boots -- and cheap to extend if it ever
matters.

## Superseded: the bootstrap task reads its config

```
(bootstrap): loading /dev/boot_device/mach_servers/name_server
```

The whole chain works end to end: kernel boots, user task at ring 3,
cthreads init, Mach IPC, console device, floppy driver, minix
filesystem, `/mach_servers/bootstrap.conf` read and parsed, first server
being loaded.

## The invocation

```sh
qemu-system-i386 -kernel mach_kernel.PRODUCTION \
    -append "BOOTDEV=fd BOOTPART=1 -o" \
    -initrd bootstrap -fda minix.img \
    -display none -no-reboot -m 64 \
    -monitor unix:/tmp/mon,server,nowait
python3 tools/vgadump.py /tmp/mon /tmp/vga.bin 255     # NOTE: 255 seconds
```

## Building the image

Minix v1, **14 character names**. `AT386/fs_switch.c` registers `ufs`,
`ext2fs` and `minixfs`, and `minixfs.c:560` accepts only
`MINIX_SUPER_MAGIC` `0x137F`. `mkfs.minix -1` defaults to 30 character
names (`0x138F`), which is rejected, so `-n 14` is required:

```sh
dd if=/dev/zero of=minix.img bs=1024 count=1440
mkfs.minix -1 -n 14 minix.img
python3 tools/mkminix.py            # writes /mach_servers/bootstrap.conf
```

`tools/mkminix.py` writes the directory and file by hand, because a loop
mount needs privileges the build environment does not have.

## THE CRITICAL FACT: ~26 seconds per read

Nothing in this investigation was ever hung. The floppy driver completes
every read successfully -- `syscall_device_read` returns `KERN_SUCCESS`
every time -- but takes about **26 seconds per read**:

```
read #0-#3   t =  54.1s   (a burst)
read #4      t =  80.0s
read #5      t = 107.1s
```

A directory walk plus inode reads is therefore minutes of work. Every
console check made at 20-40 seconds looked frozen and was simply too
early. The config file appears at roughly 250 seconds.

**Always give the floppy path at least 255 seconds before concluding
anything.**

This also retracts the conclusion that `ext2fs_open_file` hangs on an
ext2 image. It does not; it was reading at 26 seconds per operation and
was never waited out. The ext2 path may well work too.

## The live problem: the 26s-per-read driver bug

The reads succeed, so this is performance rather than correctness, and
the system functions meanwhile. ~26 seconds is the signature of a
missing completion interrupt with each transfer falling back on a
timeout. `fd.c` has `timeout((timeout_fcn_t)m765intrsub, uip, SEEKWAIT)`
in the `SKFLAG`/`RBFLAG` arm of `fdintr`, which is the obvious place to
look.

Worth fixing, but it does not block progress.

## Next: put the servers on the image

The task is loading `name_server` and will fail because only
`bootstrap.conf` is on the image. Both servers already build from this
tree and fit on 1.44 MB:

| server | size |
|---|---|
| `name_server` | 140,396 bytes (`mach_services/servers/netname`, needs `libservice`) |
| `default_pager` | 211,100 bytes (needs libcthreads, libsa_mach, libmach, libmach_maxonstack) |

## Superseded: ds_device_open calls fdopen eight times

Every value below is a direct measurement.

```
task:   ONE device_open MIG request, then blocked in mach_msg_overwrite_trap
kernel: ds_device_open
          call *0x4(%eax) at 0x14a25e  ->  fdopen   x8, always dev=0x1,
                                           always returning to 0x14a261
            each pass: geteblk, m765sweep (inlined), setqueue,
                       iowait on the same ior 0x4156f40,
                       512-byte read of recnum 0, error=0, resid=0, brelse
        reply never sent, task never wakes
```

The task's own call sequence, symbolised against
`src/bootstrap/bootstrap`, ends:

```
main -> open_file -> malloc -> cthread_malloc -> vm_allocate
-> syscall_vm_allocate -> open_file -> strcpy -> open_file
-> device_open -> mig_strncpy -> device_open -> mig_get_reply_port
-> device_open -> mach_msg_overwrite_trap
```

So `device_open` **is** a MIG RPC (unlike `device_read` and
`vm_allocate`, which are traps), it is sent **once**, and the task blocks
awaiting a reply that never comes. The repetition is entirely kernel
side.

The call site and what follows it:

```asm
14a25e:  call *0x4(%eax)        ; dev_ops->d_open -> fdopen
14a261:  add  $0x10,%esp        ; <- the return address seen 8 times
14a264:  cmp  $0xffffffff,%eax  ; result == D_IO_QUEUED ?
14a267:  je   14a286
14a269:  sub  $0xc,%esp
14a26c:  mov  %eax,0x3c(%ebx)   ; ior->io_error = result
```

**Next measurement:** `%eax` at `0x14a264` on each pass. That is one
register at one address, at a site proven to execute eight times per
boot, and it says whether `fdopen` returns `D_IO_QUEUED`, an error, or
success, and which branch drives the repetition.

## Eight retracted theories

All were measured and all were wrong. Recorded so none is retried:

| theory | why it died |
|---|---|
| the I/O never completes | `biodone`/`iodone` are macros; `io_completed` runs |
| `fs_switch` is malformed | read from the binary, it is correct |
| the task's `vm_allocate` RPC is undispatched | it is Mach trap 65, and works |
| the heap is corrupt | mapped; chain at `0x1e80` holds `0x1e00` |
| `ds_read_done` never firing is the bug | correct for the sync path by design |
| the read fails and is retried | `error=0`, `resid=0` on every pass |
| drive B is empty so disk-change sticks | same behaviour with media in B |
| `OKTYPE` never persists | measured at `iowait` entry, before the line that sets it |

Two instrument traps produced most of these, and both are now in
`DEBUGGING.md`: a name that is not a symbol always traces as "no"
(`biodone`, `iodone`), and a name that **is** a symbol can still be off
the path (`ds_device_read`, `_Xvm_allocate`) or inlined (`m765sweep`,
which shows "no" while its effect, `dr_type = 0x08`, is plainly
visible).

## Superseded: device_read is a trap, not an RPC

The framing below -- that no read is ever issued -- is **wrong**, and
several rounds were built on it.

`device_read` is a Mach trap, exactly like `vm_allocate`:

```
MACH_TRAP(syscall_device_read, 6),   /* 77 */   kern/syscall_sw.c:367
```

so `ds_device_read` and `_Xdevice_read`, the MIG message-path symbols,
are never on the code path and their absence from traces means nothing.

The last user-mode blocks the task executes, from a `-d exec` trace
symbolised against `src/bootstrap/bootstrap`:

```
cthread_malloc+228     the mallocs succeed
ufs_open_file+64
memset+0               succeeds
ufs_open_file+82
strcpy+0               succeeds
ufs_open_file+106
device_read+0          the read IS issued
syscall_device_read+0  via the trap
```

And the whole kernel-side path runs:

```
syscall_device_read YES   port_name_to_device YES
ds_device_read_common YES device_read_alloc YES
fdread YES                fdstrategy YES            io_completed YES
```

So the read is issued, the driver performs it, and the I/O completes.
The task blocks **after** that -- inside the `IO_SYNC` wait in
`ds_device_read_common`, or on the `copyout` of the result.

`syscall_device_read` itself is fully implemented: it resolves the port,
calls `ds_device_read_common` with `IO_READ|IO_SYNC`, panics on
`MIG_NO_REPLY`, then copies out `data` and `data_count`. No panic
occurs, so the sync operation is not returning `MIG_NO_REPLY`.

**Watch `ds_device_read_common`, not `ds_device_read`.** That is the
third time in this investigation that a conclusion came from tracing a
name that is not on the path -- after `biodone`/`iodone` and
`_Xvm_allocate`.

## Superseded framing: the bootstrap task is blocked, cause unknown

The task loads, runs, initialises its console, prints, opens the floppy
and gets its record size. Then it stops. It is **blocked** -- not
faulting, not spinning -- with a healthy heap and a working device
beneath it.

### What is proven working

```
floppy driver      rbrate, fdseek, geteblk, setqueue, m765io,
                   rwintr, quechk, iowait, io_completed   all run
device layer       device_open, device_get_status         both served
task heap          mapped; free-list nodes contain valid forward
                   pointers into their own arena
vm_allocate        a Mach trap (#65), not an RPC; it works
fs_switch table    [ufs_ops, ext2fs_ops, minixfs_ops, 0], each
                   ops[0] pointing at the right *_open_file
```

Execution reaches `ufs_open_file` through a well-formed indirect call.
`ds_device_read` never runs, so no reader ever issues its first read.

### Four hypotheses, all measured and all wrong

Recorded so they are not retried:

- **"the I/O never completes"** -- traced `biodone`, then `iodone`;
  neither is a symbol. `device/buf.h` defines `biodone` as `iodone` and
  `device/ds_routines.h` defines `iodone(ior)` as
  `io_completed(ior, FALSE)`. `io_completed` runs fine.
- **"`fs_switch` is malformed"** -- read from the binary, it is perfect.
- **"the task's `vm_allocate` RPC is not dispatched"** -- it is not an
  RPC. The stub is `mov $0xffffffbf,%eax; lcall $0x7,$0x0`, Mach trap
  65, matching `MACH_TRAP(syscall_vm_allocate, 4)` in `syscall_sw.c`.
  No GP fault occurs, and it works.
- **"the heap is corrupt"** -- `0x1e80` holds `0x1e00`, a valid chain
  into the same arena, and `0x1000` is mapped.

### A measurement trap to avoid

`eip` read at attach is **the idle loop**, not the task. It differs every
time -- `0x122455`, `0x15a589`, `0x154bfb` -- because the task is
blocked and the kernel is idling. Those values say nothing about where
the task stopped. Several rounds were wasted on them.

### What to measure next

The blocked **thread's** saved context, not the idle loop's registers.
The task is waiting on something; find the wait. Candidates in order:

1. Walk the bootstrap task's thread list and read the blocked thread's
   saved `eip`/`esp` from its PCB, which gives the real stop point.
2. Check what `ufs_open_file` does between entry and `mount_fs` --
   two `malloc` calls and a `memset` -- and whether any of them is the
   stop.
3. Check whether `open_file`'s `device_open` on the *filesystem* path
   differs from the one that succeeded. `open_file` opens the device
   with `D_READ|D_WRITE`; a read-only medium would fail that.

### Instrument that works

Attach **without** `-S` to the hung guest and read globals by address.
Kernel breakpoints have been unreliable all session; this method has
not failed. Task globals are addressable via `nm` on
`src/bootstrap/bootstrap`.

## Superseded: the old read-RPC framing

`ds_device_read` never runs, so the bootstrap task is not issuing its
read even though the device beneath it now works. `open_file` reaches
the device layer for `device_open` but not for `device_read`.

That is where to look next.

## Superseded: the old device blocker

The task builds server paths as
`/dev/boot_device/mach_servers/<name>` (`src/bootstrap/bootstrap.c:723`)
and reads them with `open_file(bootstrap_master_device_port, ...)`. It
cannot find its config file because:

```c
model_dep.c:606   char bootdev_name[10] = "hd0s1";   /* hardcoded IDE partition */
model_dep.c:645   if (p = getenv("BOOTDEV"))         /* always NULL */
bootstrap.c:389   #if 0  env_start = (vm_offset_t) env_buf;   /* env block DISABLED */
```

`getenv` reads `env_start`/`env_size`, which stay 0 because the code
populating them in `do_bootstrap_compat` is behind `#if 0`. So OSF's
documented `BOOTDEV` override can never be set, and `boot_device` is
aliased to an IDE partition for which this configuration has no driver.

**This is configuration, not a missing driver.** `conf.c:154` defines
`hdname "hd"` and `:160` defines `fdname "fd"`, each with full open,
close and read entries, and `fd0` is configured at boot with a working
`fdintr`. QEMU emulates a floppy controller.

Two routes:

1. Change `bootdev_name` to `"fd"`. One line, immediately testable.
2. Re-enable the env block and feed it from the multiboot command line,
   restoring OSF's own mechanism. More principled, and it is another
   instance of the `#if 0` disconnection pattern.

The diagnostic either way is the kernel's own
`Warning: unable to set boot_device`, printed between the `vga0` line
and `realtime clock configured` if `dev_name_lookup` fails.

**Unknown, and it should be settled before building any image:** what
filesystem `open_file` understands. That decides what to put on the
floppy. Note that answering `builtin` at the prompt does **not**
sidestep this -- it supplies the *config*, but the servers it names
still resolve through `/dev/boot_device/mach_servers/` and the same
`open_file` path.

## Servers available

| server | status |
|---|---|
| `name_server` | **builds**, 140,396 bytes. It is `mach_services/servers/netname`; needs `mach_services/lib/libservice` first. |
| `default_pager` | **builds**, 211,100 bytes. Needs libcthreads, libsa_mach, libmach, libmach_maxonstack in the same MK_BUILD. |
| `unix` | absent. The encumbered UX lineage. **LITES** is the replacement -- see `docs/lites-survey.md`. |

## Known latent defects, none blocking

| where | defect |
|---|---|
| `i386/spl.h` | `spl_t` is `unsigned char` while the spl assembly returns 32 bits. Harmless while IPLs stay in 0..8. |
| `i386/pic.c` + `spl.S`, `interrupt.S` | `master_icw`/`master_ocw` are 2-byte but read with `movl`. Harmless -- only `%dx` reaches the `outb`. |
| `i386/start.S:249` | `EXT(eintstack:)` -- colon inside the macro argument. Resolves correctly by luck. **Do not "fix" it.** |
| interrupt dispatch | `set_spl` is reachable by `call`, bypassing the bounds check `splx` performs before falling through into it. |
| `kern/bootstrap.c` | `regions[4]` is exactly full at three mapped segments plus headroom; a binary with more loadable segments would overflow it. |

## Instrument warnings

Read `DEBUGGING.md` before measuring anything. The short version, all
learned expensively:

- `pkill -x qemu-system-i386` **never matches**; `comm` truncates to 15
  characters. Use `qemu-system-i38` and verify with `ps`.
- Assert `eip == 0xfff0` at attach, or you are reading a stale guest.
- **Only one breakpoint services at a time.**
- Breakpoint **conditions** and **ignore counts** silently do nothing.
- **A negative from a breakpoint is not evidence.** Four "never
  reached" results this session were all false; the `-d exec` trace
  contradicted every one. Positive readings are trustworthy, absences
  are not.
- Hardware watchpoints work, but watch **both** the link and linear
  addresses.
- `-d exec` counts are not execution counts, and a 20s trace exceeds
  600MB. Keep traces to 6-8 seconds.
