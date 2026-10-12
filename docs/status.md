# Where the work stands

**The microkernel boots.** Three trees run independently; none of
the design work in `docs/roadmap.md` has started. This file is honest
about that and will be rewritten as soon as it is not true.

`RULES.md` 7.1: an honest account of what is open ships with the work.
At this point the account is almost entirely open.

## The build

From a clean clone of this branch, with `docs/tools/bootstrap-ode.sh`
and `docs/tools/mksandbox.sh` run first and `~/.sandboxrc` written from
`sandboxrc.template`:

	cd osfmk/src && sh ../../build_world

| | |
| --- | --- |
| `mach_kernel.PRODUCTION` | 1,008,260 bytes, ELF 32-bit LSB executable, Intel 80386, statically linked |
| `bootstrap` | 212,464 bytes, same format; two PT_LOAD segments, R+X and R+W |
| errors | 0 |
| vendor files modified | 12 in the kernel, all under `osfmk/src/mach_kernel`, with 302 insertions and 57 deletions against `f8fee21`; in Lites, at `osfmk/src/lites` against `e31a69d`, one file removed (`Makeconf`), one added (`Makefile`) and two execute bits restored -- no source line changed |
| kernel text | 809,498 bytes |

The kernel text was 818,058 after `5af8713` and 818,102 after
`c964add`, whose `mods_count` guards added 44 bytes; 809,290 once the
GNU Hurd bootstrap graft and `kern/boot_script.c` were no longer
compiled ("The boot" below); 809,310 with
`-fno-aggressive-loop-optimizations` in `CARGS` (verdict 25); and
809,498 linked with `-z noseparate-code` (verdict 26), which puts
`.rodata` in the text segment, so `size` counts it there.

### Lites

Built separately, after `build_world`, as OSF's step 8 built its
server:

	cd osfmk/src && build -here lites

`osfmk/src/lites/Makefile` runs Lites' own `configure` and GNU make in
`obj/at386/lites/STD+WS+osfmach3/`, with the sandbox's compiler, MIG
and export area; `docs/provenance/precedent.md`, "How Lites meets
ODE". The line is not yet in `build_world`, and goes in when it
succeeds.

| | |
| --- | --- |
| configure | succeeds; server configuration `STD+WS+osfmach3` selects 157 of `conf/files`' 289 sources |
| `include/`, `liblites/` | build; `liblites.a` and `liblites_p.a` |
| `server/` | stops; 145 objects compiled, with `GMAKE="gmake -k"` |
| `emulator/`, `bin/` | not reached |
| kernel and bootstrap | not affected by building Lites |

What stops it, every error, from a clean object directory:

| where | what | kind |
|---|---|---|
| `conf/gensym.awk` → `bsd_types_gen.symc` | 22 errors: a string literal written across lines | compiler era |
| ← that | `bsd_types_gen.h` missing in `bsd_types.defs`, `bsd_types.h`, `server_exec.c`, `ux_server_loop.c`, and `bsd_1_server.h` in `serv_syscalls.c` | follows from the first |
| `kern/kern_sig.c:1071-1219` | 5, pointers as case values | compiler era |
| `serv/user_copy.c:218` | a cast used as an lvalue | compiler era |
| `serv/device_reply_hdlr.c:91` | `mig_reply_header_t`; OSFMK has `mig_reply_error_t` | OSFMK interface |
| `serv/xmm_interface.c:113,115` | `memory_object_attr_info` has no `object_ready`, `may_cache` | OSFMK interface |
| `serv/vn_pager_misc.c:400` | `memory_object_behave_info` has no `write_completions` | OSFMK interface |

`dev`'s `tools/lites/lites-osfmk73.patch` touches `gensym.awk`,
`kern_sig.c`, `user_copy.c`, `xmm_interface.c` and `vn_pager_misc.c`,
so the verdict walk reaches a fix for all but `device_reply_hdlr.c`.
Those come as their own commits, as the walk reaches them, not here.
Two more are known to be ahead, both found by `dev` first. The link:
`server/Makerules` names `-lthreads -lmach_sa` (or `-lcthreadsZ
-lmachZ -lsa_machZ` under `OSF_ENVIRONMENT`), and this export area has
`libcthreads`, `libmach` and `libsa_mach`. And `-Ulinux`: `-std=gnu89`,
which `CARGS` passes, predefines `linux`, and Lites tests it
(`dev`'s `ec33ce7`).

Reproduced four times: during the walk, from a clean clone with the
changes copied in, from the patch applied to a clean clone, and from
the pushed branch.

The remaining `build_world` step is `makeboot`, which reports "not
found".  `setup.sh` does not build it and it is PowerMac-specific:
AT386 has `conf/AT386/config.makeboot` and a boot path under
`stand/AT386`, so that step will differ.

## The boot

	qemu-system-i386 -kernel mach_kernel.PRODUCTION \
	    -initrd bootstrap -m 64 -display none -no-reboot

Zero exceptions, zero CPU resets, and on the VGA framebuffer:

	Kernel virtual space from 0x0 to 0x40000000.
	Available physical space from 0x101000 to 0x100000
	Mach 3.0 VERSION(PMK1.1): root <>; mach_kernel/PRODUCTION (vm)

PMK1.1 is the OSF branch name that XNU's own revision histories record,
printed by the kernel we built.

**The console is VGA, not serial.**  `-r` sets `cons_is_com1`, but the
kernel does not read the multiboot command line yet, so `-append` has
no effect and every serial capture is empty.  Reading the command line
is `nmartin0/test_mk7.3` `dev` commit 419e109, still ahead of us.
Capture the screen with QEMU's monitor instead:

	-monitor unix:/tmp/mon.sock,server,nowait
	printf 'screendump /tmp/shot.ppm\nquit\n' | socat - UNIX-CONNECT:/tmp/mon.sock

The kernel now probes hardware and initialises the VM system:

	Available physical space from 0x100000 to 0x3fe0000
	vm_page_bootstrap: 14705 free pages
	adjusting delay count: 10 4 10 42 105 150 164 179 187 177 181
	fdc0, fd0, fd1, kd0, com0, vga0 probed
	realtime clock configured
	battery clock configured
	intnull(14)

**OSF's bootstrap task starts `default_pager`.** With a boot disk:

	cd osfmk/src && build -here default_pager
	sh docs/tools/mkbootdisk.sh disk.img \
	    <obj>/default_pager/default_pager
	qemu-system-i386 -icount shift=auto \
	    -kernel mach_kernel.PRODUCTION -initrd bootstrap \
	    -drive file=disk.img,format=raw,if=ide,index=0 \
	    -m 64 -display none -no-reboot

the screen ends, in under 15 seconds,

	(bootstrap): loading /dev/boot_device/mach_servers/default_pager
	(bootstrap): started
	(default_pager): started

The disk is an MBR image with one ext2 partition; the kernel's own
default boot device, `hd0s1`, finds it, with no boot argument and no
kernel change (verdicts 27–31). `-icount shift=auto` is required under
QEMU's TCG: without it the kernel takes each IDE interrupt seconds late
and the same boot takes many minutes — `docs/method.md` §4, "Slow is
not hung". Two driver messages appear and have not been looked at:
"hdintr: interrupt w/controller not done." and "HD: false interrupt".

`name_server` is not on the disk: it does not build yet (verdict 31).

**Without a disk, the task stops at its own prompt.** Since verdicts
22–24 the kernel starts OSF's bootstrap task, not the GNU Hurd servers
(`docs/deferred.md` §2, "The bootstrap fork"); since verdict 25 kernel
IPC works; since verdict 26 the task's read-only data is mapped. With
no disk attached the screen ends:

	Looking for program sections
	Theorical number of sections: 4
	Loop
	Found text region
	Loop
	Found data region
	Loop
	Found unknown section: 0x4
	Loop
	Found unknown section: 0x6474e551
	I've found: 2 sections

	ERROR: bootstrap task cannot find configuration file, please make sure that
	       your boot device and partition is correctly specified.

	Configuration file (or 'builtin'): /dev/boot_device/mach_servers/bootstrap.conf

40 page faults, no resets, the same in three boots out of three: the
task waiting for a configuration file it cannot find.

**How each step was measured.**

- *Verdicts 22–24.* Without kernel IPC or `.rodata`, the task ran crt0,
  `mach_init`, `host_page_size` and `mach_init_ports` and stopped in
  `bootstrap_arguments` on its first read of `.rodata`, `0x08069ca0`:
  13 page faults, the same in three boots.
- *Verdict 25.* GCC had collapsed `mig_init`'s loop, so the kernel's MIG
  dispatch table, `mig_buckets`, held one routine per subsystem and
  every other kernel RPC answered `MIG_BAD_ID`. Read from the running
  kernel with gdb, 12 seconds into the boot: 8 of its 1,024 entries
  populated before, 196 after. The screen did not change, because the
  task read `.rodata` before it could show anything.
- *Verdict 26.* The boot module had two read-only PT_LOAD segments, the
  ELF headers and `.rodata`, reported as "unknown flags" and never
  mapped. Linked with `-z noseparate-code` it has two segments, R+X and
  R+W, and the screen above follows. The loader also maps read-only
  segments now: a bootstrap linked the default way, with four PT_LOAD
  segments, reaches the same prompt, its scan reading "Found read-only
  region" twice and "I've found: 4 sections".

What follows is the boot as it was until then, on the graft's path.
It loaded the boot module, parsed its ELF, created the task, resumed
it, and started the second one:

	Looking for program sections
	Found text region
	Found data region
	I've found: 2 sections
	task loaded:check1 / check2 / check3
	argv[0]: exec.static
	task loaded:check1 / check2 / check3
	task_resume entry
	after task resume
	start ext2fs.static:

Over a fixed 15 second run, measured with `qemu -d int`:

	18 page faults, 28 interrupt records in total

which is what demand paging for a running user task looks like.

Two defects had to go before it got this far, and they were
independent of each other: the `splx` panic, and an unbounded read of
the multiboot module array.  Both are below.

## The splx panic: diagnosis, and the fix

**Fixed.**  The defect was in neither `splx` nor `spl.S`.  It was GCC
rewriting the timer interrupt's stack frame.

`hardclock` is declared with four parameters but is reached through the
generic interrupt dispatcher, which pushes only one of them:

	i386/AT386/pic_isa.c   take_irq(pic, 0, SPLHI, (intr_t)hardclock)
	i386/interrupt.S:290   pushl %eax             <- the saved IPL
	                       pushl iunit(,%ecx,4)   <- the one argument
	                       call  *ivect(,%ecx,4)

`old_ipl` is therefore read from the slot the dispatcher pushed,
`ret_addr` from the call's own return address, and `regs` from the
interrupt frame beneath.  The aliasing is deliberate and reading
through it is correct.  Writing through it is not: GCC turns
`hardclock`'s trailing call to `hertz_tick` into a sibling call and
rebuilds the outgoing arguments in the incoming argument area, which
the C calling convention entitles it to assume it owns.  One of those
slots is the dispatcher's saved IPL.  `return_from_interrupt` pops the
wreckage and hands it to `set_spl_noi`, which stores it into
`curr_ipl` unchecked.

GCC 2.7.2.1, the compiler `Buildconf` names, had no sibling-call
optimization, so the contract held for thirty years.  **This is a
change in compiler behaviour, not a defect in OSF's code.**

Caught with a hardware watchpoint on the slot itself.  Four writes in
the whole boot:

	slot <- 0x154d79   eip=0x159e78   call set_spl pushing its return
	slot <- 0x0        eip=0x154d7a   push %eax, the saved IPL, valid
	slot <- 0x121641   eip=0x154111   hardclock, the poisoning write
	slot <- 0x154d9a   eip=0x159ecc   call set_spl_noi, after the pop

Fixed with the per-target `_CFLAGS` hook in
`mach_kernel/conf/template.mk`, which is OSF's own mechanism in OSF's
own file, so `i386/hardclock.c` stays byte-identical to the import.
See `docs/provenance/pristine.md` for the three alternatives that were
measured and declined.

### Two things this investigation ruled out

Both had been suspected here and both are innocent.  They are recorded
so they are not re-suspected.

**`splsched()` is correct.**  It is not a separate routine: with
`MACH_KPROF` off, `splclock`, `splvm`, `splsched`, `splhigh` and
`splhi` are five labels on one address, `0x159e38`, and the body is

	cli
	mov  curr_ipl,%eax      return the full 32-bit prior value
	movl $0x8,curr_ipl
	ret

It returned exactly what `curr_ipl` held.  `curr_ipl` was already
poisoned.

**`spl_t` being `unsigned char` is a red herring.**  `i386/spl.h:35`
types it one byte wide while the assembly moves 32-bit words, so
`install_special_handler`'s `movzbl %al,%ebx` truncates.  That is real,
but it is not a defect: `curr_ipl` holds 0..8, which fits in a byte.
It mattered only during diagnosis, where it hid the high bytes and made
every bad value look byte-sized.  No change is warranted.

**`splx`'s panic labels are still reversed**, and that is worth
keeping in mind when reading older logs.  cdecl pushes right to left,
so in `"splx(old %x, new %x)"` the printed **old** is the argument
passed to `splx` and the printed **new** is the current IPL.

### The completeness check

The same pattern is legal and common in C -- 118 functions in the
linked image write at or above argument two and end in a sibling call.
It is a defect only where the caller is hand-written assembly that
still owns those slots.  Every C function reachable from assembly was
enumerated from the `.S` files and from `mach_trap_table`, and
intersected with that set.

**`hardclock` is the only one.**  Everything else either discards its
arguments (`addl $N,%esp` -- `i386_astintr` at all four call sites,
and every other `ivect[]` handler) or abandons the frame wholesale
(`skip_syscall` recomputes `%esp` from the kernel stack base, which
covers `mach_msg_overwrite_trap`).

## The multiboot module array, read unbounded

**Fixed.**  `parse_multiboot()` read `mb_module[0]` and `mb_module[1]`
without consulting either `mb_info.flags` or `mb_info.mods_count`.
This tree's own `i386/multiboot.h` is the authority against it, twice:

	:102  "List of boot modules loaded with the kernel.  Valid only
	       if MULTIBOOT_MODS is set in flags word above."
	:146  "The mods_addr field above contains the physical address
	       of the first of 'mods_count' multiboot_module structures."

With one module supplied, which is how this kernel is booted,
`mb_module[1]` is past the end of the array.  Measured at the end of
`parse_multiboot`:

	                before              after
	mods_count      1                   1
	boot_start      0x200000            0x200000
	boot_size       220656              220656
	exec_start      0x706d742f          0
	exec_size       -1091240192         0

`0x706d742f` is ASCII `/tmp`: the QEMU command line read as a module
descriptor.  Those two feed `kern/bootstrap.c:1239` directly --
`boot_script_parse_line(exec_start, exec_size, "exec.static ...")` --
and the result was a page-fault storm:

	                page faults / 15s   interrupt records
	before          1,619,177           1,620,661
	after           18                  28

`boot_start` and `boot_size` were always correct, so the first boot
script line worked and the console looked healthy up to
`argv[0]: exec.static`.  The storm began after that and produced no
further output, which is why it was briefly mistaken here for the
kernel simply stopping.

**A measurement that does not carry over.**  `nmartin0/test_mk7.3`
`dev` commit `a77f81b` reports 0 faults for the one-module case both
before and after its fix.  That is correct for `dev`'s tree at that
point and wrong for ours.  `dev` still had the `splx` panic then, so
its kernel halted before reaching `bootstrap.c:1239` and the garbage
was never consumed.  This branch took the `hardclock` fix first, so
the kernel reaches that line and the storm appears.  The order the two
were adopted in changed what the instrument shows -- which is the
argument for re-measuring rather than inheriting a number.

## What runs

| | state | measured |
|---|---|---|
| OSFMK 7.3 under QEMU, i386 | boots | by the maintainer, outside this repository |
| Lites on OSFMK 7.3 | runs | by the maintainer, outside this repository |
| the adaptation that makes it run | `tools/lites/lites-osfmk73.patch`, 1,414 lines across 29 files | `nmartin0/test_mk7.3`, `dev` |
| 4.4BSD-Lite2 (`nmartin0/4.4BSD-Lite2`, `dev3`) | standalone kernel, execs init, enters user mode | that tree's `docs/status.md` |

The third is the one with a published account. Its kernel mounts an FFS
root from a labelled disk, loads `/sbin/init`, enters user mode at
`cs=0x1f`, and takes a correctly handled page fault on init's first bss
write — after which the machine produces no further exceptions. That is
that project's open question, not this one's, but it bounds what the
personality can be expected to do when it arrives here.

## What has been established without building anything

All of it by `grep -rl` against the trees, in the session that recorded
it. Commands are in `docs/method.md` §7.

**Patience's mechanisms are already in OSFMK 7.3.** This is the single
most consequential finding so far, because it removes the kernel half
of the syscall work:

| mechanism | files in `osfmk7.3/osfmk` |
|---|---|
| `EXC_SYSCALL` = 7, `EXC_MACH_SYSCALL` = 8 | `mach/exception.h` |
| `EXCEPTION_DEFAULT` 1, `EXCEPTION_STATE` 2, `EXCEPTION_STATE_IDENTITY` 3 | same |
| `catch_exception_raise_state` | 9 |
| `thread_set_exception_ports` | 19 |
| `thread_swap_exception_ports` | 7 |
| `mach_msg_overwrite` | 20 |
| `vm_read_overwrite` | 14 |
| `vm_remap` | 20 |
| `mach_subsystem_create` | 9 |
| `mach_port_allocate_subsystem` | 16 |
| `rpc_subsystem` | 16 |
| `routine_descriptor` | 17 |
| `mig_stub_routine` | 5 |
| `thread_activation_create` | 10 |

**`THREAD_STATE_SYSCALL` is not.** `mach/i386/thread_status.h` defines
flavours 1 (`i386_THREAD_STATE`), 2 (`i386_FLOAT_STATE`), 3
(`i386_ISA_PORT_MAP_STATE`), 5 (`i386_REGS_SEGS_STATE`) and 8
(`i386_SAVED_STATE`). No syscall-specific small flavour. It is the one
kernel addition the design requires.

**The i386 collocated fast path is written.** `i386/i386_rpc.c`, 608
lines, and `i386/machine_rpc.h`, 225. `call_exc_serv()` transfers the
exception arguments to a new stack and performs a side-call to the
collocated server by `jmp`, returning through
`exception_return_wrapper()`.

**OSFMK 7.3 has no `mach_init` server.** `bootstrap.template` installs
as `/mach_servers/bootstrap.conf` and its default content is three
lines: `name_server`, `default_pager`, `startup`. The two literal
`/mach_servers/mach_init` strings in the tree are the same comment in
two copies of `servers/service.defs`. `mach_init.c` in `libmach` is the
per-task library initialiser.

**`bootstrap.template` names our target.** It refers to "the
`osf1_server` (conventionally called `startup`)" and documents its
arguments: `-s` single-user, `-a` prompt for root device, and a root
filesystem name.

**Six of those 29 files are files Tier 1 rewrites**: `server/serv/`'s
`user_copy.c`, `serv_syscalls.c`, `server_init.c`, `server_exec.c`,
`xmm_interface.c` and `vn_pager_misc.c`. The other 23 are the
adaptation proper and are Tier 0 work already done.

**Lites declares MIG `subsystem bsd_1 101000`**, with 68 routines.

**Lites is 79% unchanged from stock 4.4BSD-Lite2** across 88 shared
files, comments stripped. Per directory: netinet 97.4%, ufs/ffs 86.2%,
ufs/ufs 80.1%, kern 73.6%, nfs 63.5%.

**Those similarity figures were measured against stock Lite2, not
against `bsd/`.** The maintainer reports `dev3` is more functional than
stock and fixes many bugs. The delta has not been measured and the
figures above should not be quoted about this repository until it has.

## Defects found and left, deliberately

Each blocks nothing and sits in a path that cannot be exercised yet.
Recorded so Tier 1 inherits them rather than rediscovers them.

| where | what |
| --- | --- |
| `i386/i386_rpc.c:607` | `"movl %0, %%edx; movl %%edx, %1"` writes `%1`, declared `"g"` input. Identical to the defect that blocked the build; escapes only because `*new_argv` is not a constant |
| `i386/i386_rpc.c` 195, 411, 464, 521, 567 | each does `addl $4, %N` on an operand declared `"r"` input. GCC may assume inputs unmodified and reuse the register; these want `"+r"` |
| `mach_services/lib/libmach/sbrk.c:38` | returns `-1` from a function declared `void *`. An unimplemented stub whose body is one `fprintf` |
| `i386/start.S:354` | suffix/width mismatch inside `#if NCPUS > 1`; PRODUCTION builds `NCPUS 1` so it never reaches the assembler. Left as OSF wrote it |

The whole kernel was scanned for int/pointer confusion by rebuilding
with `-Wint-conversion`, `-Wpointer-to-int-cast`, `-Wint-to-pointer-cast`
and `-Wpointer-compare` enabled.  After the eleven sites fixed in
`kern, vm, intel: use the typed null constants`, exactly one remains:
`sbrk.c` above.

## What has not been checked and should be, first

Both are greps, both take minutes, and both change the shape of Tier 1
if they fire.

1. **Does Lites allocate per-thread state at a constant offset from a
   fixed-size C-threads stack?** The OSF/1 server did this with
   `uthread`, which forced Utah's kernel to hand the server its own
   service stack rather than reuse the kernel stack. Lites shares the
   C-threads heritage.
2. **Do Lites' generated stubs copy arguments the BSD service routines
   already copy?** Utah found every argument moving twice, because the
   personality came from a monolithic kernel and does its own
   `copyin`/`copyout`.

Neither has been measured here. Both are recorded in Utah's paper as
costing them real time.

## The two Utah traps, measured

Both were listed in `docs/roadmap.md` Tier 0 as unchecked, and both
change the shape of Tier 1 if they fire.  Measured against Lites, at
`osfmk/src/lites/`.

### Trap 1 — per-thread state at a fixed offset from a C-threads stack

**It fires.**  `libcthreads/cthread_internals.h:241`:

	#define _cthread_ptr(sp) \
	    (*(cthread_t *) \
	      (((vm_offset_t)(sp) | cthread_status.stack_mask) \
	       + 1 - sizeof(cthread_t *)))

	#define _cthread_self()  ((cthread_t) _cthread_ptr(cthread_sp()))

A thread's identity is its **stack pointer masked to a fixed size**,
reading a self-pointer planted at the top of that stack by
`stack.c:251`.  Lites depends on it: `server/serv/ux_server_loop.c:155`
registers its per-thread state with
`cthread_set_data(cthread_self(), pk)`, where `pk` is the
`proc_invocation_t` that is Lites' equivalent of OSF/1's `uthread`.
Every `get_proc_invocation()` in the server resolves through that
mask.

**What it costs Tier 1.**  A syscall exception delivered on a stack
that C-threads did not allocate — the kernel's stack, or any stack of
the wrong size or alignment — makes `cthread_self()` return garbage,
and with it `pk`.  So the kernel must hand the server a C-threads
service stack, exactly as Utah found for the OSF/1 server.  This is a
constraint on the syscall path, Tier 1 item 2, not a detail of it.

Already anticipated in the library: `stack.c:430` has

	if (in_kernel && cthread_stack_size == 0)
		cthread_stack_size = IN_KERNEL_STACK_SIZE;

with `IN_KERNEL_STACK_SIZE` 32 KB at `cthread_internals.h:437`.  There
is a collocated path here already, and Tier 1 item 4 should start from
it rather than invent one.

### Trap 2 — arguments copied twice

**It does not fire on the generic path.  The MIG path is not settled.**

`server/serv/bsd_msg.h:99` is the whole request:

	struct bsd_request {
	    mach_msg_header_t  hdr;
	    mach_msg_type_t    int_type;
	    integer_t          rval2;
	    integer_t          syscode;
	    integer_t          arg[10];
	};

Ten inline scalars and no out-of-line buffer.
`server/serv/ux_syscall.c:227` hands `req->arg` straight to the BSD
handler, and pointer arguments are then fetched by Lites' own
`copyin` in `server/serv/user_copy.c`, which reads the user task
directly.  So a pointer argument's data moves once, not twice.

Not settled: `server/serv/bsd_1.defs` declares 70 MIG routines, and
whether any of them is reached in the configured build — and if so
whether its generated stub copies what the service routine copies
again — was not traced.  No out-of-line or array arguments were found
in it, which is weak evidence against the trap and not proof.  Stated
as open rather than closed.

### A third thing found on the way

`server/serv/cprocs.c` and `serv/synch_prim.c` are `fileif data_synch`
in `conf/files`, and `data_synch` is listed under "Experimental
features" in `conf/MASTER:64` and selected by no i386 configuration.
They do not compile.  That is why `cproc_self()` at
`include/mach/cthread_internals.h:184` expands to `ur_cthread_self()`,
which is **defined nowhere in the tree**, without anything
complaining.  Worth knowing before reading `cprocs.c` as though it
were live code.

## What is open and has no answer anywhere

- `getsysinfo` operations the vendor marked "for internal use only".
- The OSF/1 MK 6/7 single server source: confirmed never publicly
  released. OSF RI said why — the latest OSF/1 versions "are encumbered
  by commercial licenses", which is why they wrote MkLinux's server
  instead.
- `ftp.gwdg.de/pub/misc/opengroup/ri/`, the OSF RI publications mirror,
  never enumerated. One file from it is the best architecture document
  this project has.
