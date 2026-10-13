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

	sh docs/tools/mkbinutils.sh		# once per sandbox
	cd osfmk/src
	sh ../../docs/tools/mkhostbin.sh	# once per sandbox
	sh ../../build_world

`mkbinutils.sh` builds GNU as 2.42 for i386-elf, the target's
assembler, which reads OSF's System V `/` comments; `mkhostbin.sh`
builds `config` and `migcom` from OSF's sources. Both install into the
sandbox's `tools_base`, outside the tree. The host needs `flex`, its
32-bit library and `byacc`, and, for Lites, `original-awk`.

| | |
| --- | --- |
| `mach_kernel.PRODUCTION` | 976,084 bytes, ELF 32-bit LSB executable, Intel 80386, statically linked |
| `bootstrap` | 203,732 bytes, same format; two PT_LOAD segments, R+X and R+W; reads UFS only |
| errors | 0 |
| vendor files modified | 107 in `osfmk/` against `f8fee21`, 3,393 insertions and 8,048 deletions. 63 of them, 1,867 insertions and 5,214 deletions, are `config` and `migcom` -- OSF's 1999 sources restored from the MkLinux CVS repository, slp's 1995 ones, built objects and binaries deleted, two `AT386/machdep.mk` added -- and `link/tools`, spelt as CVS spells it (`docs/provenance/imports.md`, kinds C and F). 24, 1,273 insertions and 1,261 deletions, put DR3's assembler files back -- the kernel's sixteen under their `.s` names and with OSF's comments, and `libsa_mach`'s six -- with `template.mk`'s lines that name them and all of `conf/AT386/files`; `config.devices` keeps PCI out, because its sources are not in DR3. Against DR3 they differ by a 60-line diff in three files, `locore.s`, `i386_lock.s` and `start.s`: operand fixes no current GNU as lets pass, and the multiboot entry (`docs/provenance/verdicts.md`, `18c9d31`). Seven of the rest return files to DR3 exactly (`kern/boot_script.[ch]` deleted, `conf/files`, `ext2fs/ext2_fs.h`, `kern/lock.c`, `i386/proc_reg.h`, `mach/i386/vm_param.h`), because `f8fee21` is `slp/osfmk-mklinux`'s 2012 tree, not DR3 (`docs/provenance/verdicts.md`, "slp's eighteen commits"); in Lites, at `osfmk/src/lites` against `9d1c98a`, Utah's 1.1.u3 release, one file removed (`Makeconf`), one replaced (`makefile`), and the source fixes for today's compiler and for OSFMK 7.3 that `docs/provenance/imports.md` lists under kinds B and C, each with a note at its site |
| kernel text | 777,345 bytes |

The kernel text was 818,058 after `2508bd3` and 818,102 after
`752cba4`, whose `mods_count` guards added 44 bytes; 809,290 once the
GNU Hurd bootstrap graft and `kern/boot_script.c` were no longer
compiled ("The boot" below); 809,310 with
`-fno-aggressive-loop-optimizations` in `CARGS` (verdict 25); and
809,498 linked with `-z noseparate-code` (verdict 26), which puts
`.rodata` in the text segment, so `size` counts it there; 809,426 once
the graft's last compiled pieces were deleted and slp's Page Global
Extensions removed; 777,345 with GCC's automatic inlining off in
`CARGS`, as GCC 2.7's -O2 had it, for the era's variadic functions
(`docs/tools/mksandbox.sh`).

### Lites

Built separately, after `build_world`, as OSF's step 8 built its
server:

	cd osfmk/src && build -here lites

`osfmk/src/lites/makefile` runs Lites' own `configure` and GNU make in
`obj/at386/lites/STD+WS+osfmach3/`, with the sandbox's compiler, MIG
and export area; `docs/provenance/precedent.md`, "How Lites meets
ODE". The line is not yet in `build_world`, and goes in when it
succeeds.

| | |
| --- | --- |
| configure | succeeds; server configuration `STD+WS+osfmach3` selects 150 of `conf/files`' 287 sources. xMach's copy selected 157: its `conf/i386/MASTER` added `extfs` and with it the seven `cd9660` sources |
| `include/`, `liblites/` | build; `liblites.a` and `liblites_p.a` |
| `server/` | builds: `startup.Lites.1.1.u3.STD+WS+osfmach3`, 866,924 bytes stripped, ELF 32-bit, entered at `__start_mach`, linked with OSFMK 7.3's `libcthreads`, `libmach` and `libsa_mach` |
| `emulator/` | builds: `emulator.Lites.1.1.u3`, 217,359 bytes of text, linked at `0xa0001000` by `emulator_base` |
| `bin/` | nothing to build in this configuration |
| kernel and bootstrap | not affected by building Lites |

`build -here lites` completes. What it reports, from a clean object
directory:

| where | what | kind |
|---|---|---|
| `conf/files:347` | `# Linux file systems`, a `#` comment in a file configure runs through the C preprocessor, reported as an invalid directive; the configuration is written all the same. xMach's copy has it as `/* */` | compiler era, harmless |

`serv/xmm_interface.c` is Utah's post-release copy of 5 May 1996,
which implements `memory_object_data_return`, the call OSFMK's
pageout makes; the release's panics there (`docs/provenance/imports.md`,
kind F). It compiled once OSFMK 7.3's memory object interface was given to it
(`docs/provenance/imports.md`, kind B).

The compiler-era errors are fixed -- `gensym.awk`, `newvers.sh`,
`kern_sig.c`, `serv_syscalls.c`, `user_copy.c`, and a GNU make
dependency in `server/Makerules` -- and so are OSFMK 7.3's changed
names and messages in `xmm_interface.c`, `vn_pager_misc.c`,
`device_reply_hdlr.c` and `ux_server_loop.c`. The server links:
Lites' `OSF_ENVIRONMENT` names OSFMK 7.3's libraries and start code,
the server supplies the `vprintf` OSFMK's libmach writes through, and
configure runs Kernighan's awk (`docs/provenance/precedent.md`, "What
B needed"). The emulator builds once `ATSYS_NAMES` stops pasting
string literals and `mach_init` is called as OSFMK 7.3 exports it.
`dev`'s `-Ulinux` (`ec33ce7`) is not needed here: `-std=gnu89`, which
`CARGS` passes, predefines `linux`, but in Utah's release the one place
the server tests it is `ufs/ext2fs/ext2_linux_ialloc.c`, which this
configuration does not compile. `emulator/programs/emulator_base.c`
tests it too, as a host program, to pick the linker's options; on this
Linux host it takes the branch its authors wrote for one, whose
`-qmagic` GNU ld ignores ("Ignored for Linux compatibility").

Measured from a clean clone at each commit, with `GMAKE="gmake -k"`.

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

	sh docs/tools/mknbmakefs.sh		# once
	(cd osfmk/src && build -here default_pager)
	sh docs/tools/mkbootdisk.sh disk.img \
	    $MK_BUILD/obj/at386/default_pager/default_pager
	sh docs/tools/boot.sh disk.img

the screen ends, in under 20 seconds under TCG,

	(bootstrap): loading /dev/boot_device/mach_servers/default_pager
	(bootstrap): started
	(default_pager): started

The disk is laid out as OSF's MK7.3 laid out an i386 disk: a DOS
partition of type 99 holding OSF's vtoc, partition a a UFS file system
with `/mach_servers/` on it, b for paging. The bootstrap task reads only
UFS: ext2fs and minixfs, MkLinux's additions, are no longer built.
`docs/provenance/precedent.md`, "What OSF booted from". `mkbootdisk.sh`
needs `python3` and NetBSD 11.0's `makefs`, which
`sh docs/tools/mknbmakefs.sh` builds once into `$MK_BUILD/netbsd/`
(git and a C compiler; a 235 MB sparse checkout). The kernel's own
default boot device resolves to unit 0, partition a, so no boot
argument is needed.

`boot.sh` uses KVM where `/dev/kvm` is usable and otherwise TCG with
`-icount shift=auto`, which TCG requires: without it the kernel takes
each IDE interrupt seconds late and the same boot takes many minutes —
`docs/method.md` §4, "Slow is not hung". KVM has not been tried here;
there is none in this environment. Two driver messages appear,
"hdintr: interrupt w/controller not done." and "HD: false interrupt";
`dev`'s `052a105` explains the second, and is the next commit.

`name_server` is not on the disk: it does not build yet (verdict 31).

**Lites starts.** With the server and emulator `build -here lites`
makes, as `/mach_servers/startup` and `/mach_servers/emulator`, and

	default_pager default_pager
	startup startup -s hd0a

as `bootstrap.conf` (`BOOTSTRAP_CONF` for `mkbootdisk.sh`), the screen
continues past `(default_pager): started`:

	(lites): WARNING! no server directory specified.
	(lites): path(/dev/hd0a/dev/boot_device/mach_servers) derived from root
	(lites): server_dir(/dev/boot_device/mach_servers) on root.
	(lites): emulator_path(/dev/boot_device/mach_servers/emulator)
	(lites): init_program(/dev/boot_device/mach_servers/mach_init)
	Lites VERSION(Lites.1.1.u3): ... STD+WS+osfmach3 (vm)
	Copyright (c) 1982, 1986, 1989, 1991, 1993 ...
	Base is Mon Oct 12 20:14:56 2026
	Time is set to Sun Oct 11 20:15:0 2026
	timer: 4 timeouts adjusted by 0.720000 seconds
	panic: first program (/dev/boot_device/mach_servers/mach_init) exec failed: xc002 (os/unix) file or directory does not exist
	panic: init died

It has mounted partition a, an FFS root, read its time, and then
panics, as it must with no userland: there is no `mach_init` to run
as the first program, and the reap of the dead init panics again.
Until automatic inlining was turned off the screen showed "panic:"
and three bytes of garbage: GCC had inlined `printf` into `panic`, and
the era's `va_start`, address arithmetic on the last argument, read
arguments that were never stored (`docs/tools/mksandbox.sh`, `CARGS`).
Both panics were first caught with gdb on QEMU's stub, at `panic`'s
address, which is how the arguments were known to be right.
The next thing is a userland, NetBSD 1.0's, and `mach_init` linked
to its `/sbin/init`.

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
	i386/interrupt.s:290   pushl %eax             <- the saved IPL
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

**Lites and OSF's `uxkern/` share 31 filenames** and both declare MIG
`subsystem bsd_1 101000`, OSF with 75 routines and Lites with 68.

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
| `i386/start.s:354` | suffix/width mismatch inside `#if NCPUS > 1`; PRODUCTION builds `NCPUS 1` so it never reaches the assembler. Left as OSF wrote it |

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
- `sysinfo` commands beyond 9, which Linux's own source flags as
  unpublished.
- The OSF/1 MK 6/7 single server source: confirmed never publicly
  released. OSF RI said why — the latest OSF/1 versions "are encumbered
  by commercial licenses", which is why they wrote MkLinux's server
  instead.
- `ftp.gwdg.de/pub/misc/opengroup/ri/`, the OSF RI publications mirror,
  never enumerated. One file from it is the best architecture document
  this project has.
