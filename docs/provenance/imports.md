# What is OSF's, what is Berkeley's, what is borrowed, and what is neither

Every change on this project falls into one of seven kinds. The kinds
are about provenance, not about size or difficulty: they say how far a
given patch stands from the code its three upstreams shipped, so that a
reader can judge each one's authenticity without re-deriving the
argument.

The second half of this file records what was NOT done, and what that
costs, because a fork is shaped as much by its refusals as by its
changes.

## The seven kinds

	A  Upstream's code, corrected to upstream's own intent
	B  Upstream's code, corrected following another tree's line
	C  Upstream's code, changed with no donor line: our judgement
	D  Code transplanted that the receiving tree never had
	E  Code written from a specification OSF published
	F  An upstream's own file, restored from a tree that kept it
	G  Files that are ours, outside any upstream's tree

A is the most authentic and G the least. D, E, F and G are the only
kinds that put new text into a vendor directory.

**E is this project's characteristic kind and does not exist in the
Lite2 tree.** OSF published the Server Writer's Guide, the Server
Library Interfaces, the Kernel Principles, the 21-part Kernel
Interfaces and the MK6.1 release notes under a grant that explicitly
covers derivative works, and the two USENIX papers describe the
mechanisms this project implements. Code written from those is neither
transplanted nor invented. It is implemented from a specification, and
the specification is named with its section.

## Which upstream a file belongs to

Three trees, three idioms, and the boundary matters more here than the
file's directory does.

	osfmk7.3/   OSF's.  OSFMK conventions throughout.
	lites/      CMU's and Helander's, over 4.4BSD-Lite.
	bsd/        Berkeley's, as 4.4BSD-Lite2 and its dev3 corrections.

The seam is Lites' `server/serv/`. Code there faces Mach and follows OSFMK's
conventions — `kern_return_t`, `KERN_SUCCESS`, OSF's header shape.
Code in the personality proper follows 4.4BSD's, because that is what
it is and what it must stay mergeable with.

A change that crosses the seam in one file is usually two changes.

## A note on XNU, where the ancestry is not a relationship but an identity

The Lite2 tree records that Berkeley took NetBSD's changes into their
own i386 port in June 1993, so "NetBSD 1.0 writes it this way" is not
independent confirmation in that directory: it may be the same text
arriving by another route.

**This project has a stronger version of the same thing.** XNU's
`osfmk/` is not a relative of `osfmk7.3/`. It *is* OSFMK 7.3, carried
forward. Apple licensed it and imported it; `wsanchez`'s 1998 import
commits are still in the history.

The history says so explicitly. XNU files carry HISTORY blocks naming
OSF's own development streams:

	mk6 CR1120 - Merge mk6pro_shared into cnmk_shared
	Merge up to NMK17.3
	nmklinux_1.0b3_shared into pmk1.1

So three things follow.

**XNU agreeing with `osfmk7.3/` is usually not corroboration.** It is
the same line, later. Where a claim rests on XNU alone, say so.

**XNU's HISTORY blocks are evidence about OSFMK itself.** A file whose
history shows `mk6` or `nmk15` revisions predates Apple, and the
revision dates establish when OSF wrote it. That is a use of XNU no
other tree supports.

**Oldest first, always.** `rel/xnu-124` is the earliest tag and the
closest to our kernel. Moving forward is moving away.

## A -- corrected to upstream's own intent

Evidence inside the tree or in the upstream's own history; no other
system needed.

| what | the upstream's own evidence |
|---|---|
| `i386/AT386/model_dep.c`, `parse_multiboot` bounded by `mods_count` and `MULTIBOOT_MODS` | `i386/multiboot.h:102` "Valid only if MULTIBOOT_MODS is set in flags word above" and `:146` "the physical address of the first of 'mods_count' multiboot_module structures" — the header three files away states both constraints the code broke |
| `kern/bootstrap.c`, OSF's `bootstrap_create()` and `user_bootstrap()` restored under their own names; the GNU Hurd graft's functions of those names behind `#if 0`; `conf/files` stops building `kern/boot_script.c`. *Superseded 13 October 2026:* the graft is deleted, not fenced -- `slp/osfmk-mklinux`'s `63279d3`, verdict in `verdicts.md`, "slp's eighteen commits"; the two names are DR3's own again | The tree kept OSF's two functions as `bootstrap_create_old()` and `user_bootstrap_old()`, and the first still called the second by its original name, `thread_start(..., user_bootstrap)` — the wiring OSF wrote, recoverable from inside the file. No line was taken from another tree |
| `kern/profile.c`, `kern/profile.h`, `vm/vm_map.c`, `intel/pmap.h`, `kern/bootstrap.c`: a pointer compared or set against `MACH_PORT_NULL` or `(vm_offset_t)NULL` given the tree's typed null for it, `IP_NULL`, `TASK_NULL`, `PMAP_NULL` | The files' own spelling a few lines away; `b88f799`, whose message has the before-and-after object comparison (byte-identical). An early commit: the sites carry no note, and this row is their record |
| `i386/i386_rpc.c`, `machine_rpc_simple()` and `klkernel_call`: the asm's written operand declared an output, and `call *%2` | The asm's own text, which writes the operand it declared an input; `3b4584c`, note at the site |
| `i386/pio.h`, `inw` and `outw`: the 16-bit instructions for DR3's `.byte 0x66; inl` and `outl` | The same bytes (`66 ed`, `66 ef`), which the prefix spelled by hand; `743dffa`, note at the site |
| `i386/AT386/model_dep.c`, `parse_arguments()`: `case 's'` sets `startup_single_user` | OSF's MK7.3 Release Notes, Boot Options: "-s -- single-user mode (passed to server)". In the tree, `machine_boot_info()` a page below already hands the flag on as " -s" through `host_get_boot_info`, and HP700's `parse_args()` (`hp_pa/model_dep.c`) sets `RB_SINGLE` from `s`; on i386 only the old boothowto convention (`asm_startup.h`, `SYS_REBOOT_COMPAT`) set it, so `-s` after the kernel's name at OSF's `boot:` prompt was dropped. `docs/deferred.md`, O-7; note at the site |
| `i386/AT386/bbclock.c`: `yeartoday()` back to CMU's "(year % 4) ? 365 : 366"; `bbc_settime()` writes the year trimmed into 0-99 with `chips/mc_clock.c`'s two lines; that file's CMU notice added after OSF's | CMU's own rule, which a 1992 branch revision ("lance@mtxinu.com", 2.6.1.2, in the file's history) replaced with a century test on 1900-based years: Mach 3.0 MK83's `i386at/rtc.c`, Utah's Mach4 UK22 kernel (disassembled) and this tree's `sqt/clock.h`. The setter's line is this tree's, `mc_write()` for the same MC146818 chip, "Trim years into 0-99 range". `docs/deferred.md`, S-16; 0113, notes at the sites |
| `lites/server/serv/serv_synch.c`, `tsleep_main()`: a `PCATCH` sleep ends when the process exits, with `EINTR`, not only `sigpause`'s | Lites' own rule in this tree's newer sleeps, `serv/synch_prim.c`, `tsleep_abs()`, where `PKW_EXIT` ends every `PCATCH` sleep; the exception it replaces kept a terminal drain that `proc_died()` does as `proc0` (`serv/proc_to_task.c`, `proc_deref()`, "We don't want to sleep on the died proc"). `docs/deferred.md`, S-18; 0115, note at the site |
| `lites/server/ufs/ffs/ffs_vfsops.c`: `"fifo.h"` included; `ufs/ffs/ffs_extern.h`: `#ifdef FIFO` made `#if FIFO`. `lites/server/nfs/nfs_vnops.c`, the three FIFO wrappers: `get_time()` for `time` | Lites' own idiom: options as headers defining 0 or 1, which `ffs_vnops.c` includes and tests with `#if`; the mapped clock read with `get_time()`, as Lites writes Lite2's `ifp->if_lastchange = time` in `net/if_ethersubr.c`. Both compiled only with `fifo`. `docs/deferred.md`, S-20; 0119, notes at the sites |
| `i386/mp_desc.c`: `intstack[]` and `eintstack[]` declared `extern`; `ipc/ipc_port.c`, a MACH_KDB site: `IP_NULL` for `MACH_PORT_NULL` | The symbols are defined in `i386/start.s`, and `i386/db_trace.c` declares `intstack` the same way; `IP_NULL` is the tree's typed null, as row 92's eleven sites. Both compile only in FAST, DEBUG or MP kernels. 0095, notes at the sites; rows added 20 October 2026, when the audit against Apple's DR3 found them missing |
| `libcthreads/i386/csw.s`, `cthread_filter()`'s `USER_INVOKE` case: the result taken from `12(%esp)`, not `8(%esp)` | The file's own layout: `cthread_filter(context, type, ...)`, whose other cases take the first argument after the type from `12(%esp)` (`INTERNAL_BUILD`'s function, `INVOKE_SELF`'s `ARG2`); its comment, "result - The result code to return"; and OSF's `POWERMAC/csw.s`, `cthread_user_invoke(context, result)`, which returns it. DR3 returned the type, 3. Measured: a thread joined after returning 42, and one after `cthread_exit(7)`, gave 3 and 3, and give 42 and 7. 0127, note at the site |

**The first entry of this kind, and it was nearly filed as something
weaker.** `dev`'s version of the same fix cites the multiboot
specification, an outside document. The tree had the answer, which is
what `audit.md` exists to catch.

**Qualified, one commit later.** "Upstream's own intent" is doing
more work in that row than it should. The header is the authority on
how a multiboot module array may be read, and the fix honours it. But
reading exactly two modules was not OSF being careless: it is the GNU
Hurd bootstrap grafted onto this tree before publication, which
expects `ext2fs.static` and `exec.static` as `mods[0]` and `mods[1]`
and therefore always two. See `docs/deferred.md` §2. **The code
corrected is the grafter's, not OSF's.** The entry stays kind A,
because the evidence is still inside the tree and no other system was
needed, but "upstream" there means the tree as published, not OSF's
design. Where that distinction matters — and on this project it
usually does — the row is not evidence about what OSF intended.

**Three notes on the `bootstrap.c` row.** `set_user_regs()` and
`build_args_and_stack()` were left compiled and uncalled (deleted with
the rest of the graft, 13 October 2026): the
graft's `user_bootstrap()` was their only caller, and whether they are
OSF's or the graft's is not established. *Established 12 October 2026:
the graft's.* Neither is in `slp/osfmk-mklinux`'s first commit,
`0f6f7e6`; both arrive with the Hurd graft in `63279d3`, where `build_args_and_stack(argv, envp)` takes an environment ("pointers to environment variables", "Then the strings and string pointers for each environment variable"), and where OSFMK 6.1's takes a variable argument
list. OSFMK 6.1 was read while
checking it and not used: its `bootstrap.c` is a different design, and
its notice differs from 7.3's — 6.1 adds that OSF's name not "be used
in advertising or publicity pertaining to distribution of the software
without specific, written prior permission", a clause 7.3's notice
does not have, so 6.1 lines grafted into a 7.3 file would bring that
condition with them. And `kern/boot_script.c`, no longer built and
since deleted, is GNU Mach's ("Written by Shantanu Goel") and carries no
licence notice of its own; it stayed in the tree as imported until
the graft was deleted, which settles whether it may be carried.

## B -- corrected following another tree's line

The code stays its upstream's; the line taken exists in a tree that may
be copied, and the commit names it with its release or tag.

| what | the line, and its tree |
|---|---|
| `stand/AT386/boot`: `primary/sys.c` includes `ufs/defs.h` for `dirent.h` and `sys/reboot.h`; `primary/boot.c`, `secondary/mach_boot.c` and `secondary/lilo_env.c` define the `B_*` values they use, "from OSF/1's sys/reboot.h"; `secondary/boot.c` drops `sys/reboot.h`; `lib/elf.c` includes `lib/elf.h`, OSF/1's `sys/elf.h` copied into the boot; the three `Makefile`s add `-I../../../../file_systems`; `-m386` as DR3 wrote it (0052's `-mtune=i386` reverted) | OSF's Self-Contained Mach Distribution, barbou, 28 December 1995, in MkLinux 1.0b2's `osfmk` (`stand/AT386/boot`, whose RCS logs name each change): the same boot built from the MK tree without the OSF/1 server. `lib/elf.h` is 1.0b2's file. `lilo_env.c` is newer than 1.0b2 and takes the same three values as `boot.c`. The boot builds on the running system, where gcc 2.7.2.1 takes `-m386`; a Linux host's GNU as rejects `boot2.s` (`docs/deferred.md`, "Booting with OSF's own boot loader") |
| `i386/i386_lock.s`, `i_bit_set` and `i_bit_clear`: `bts %dl` and `btr %dl` become `%edx` | `slp/osfmk-mklinux` `18c9d31`. Neither instruction has a byte-register form and no current GNU as accepts one; the bit number is all of `S_ARG0`, which each routine loads into `%edx` first. The rest of that commit is reverted (`verdicts.md`, "slp's eighteen commits"), so this is the one line kept from it outside `start.s`, and the site says so |
| `conf/AT386/config.devices`: `pci` 2 to 0; `ncr810`, `tu` and `fta` commented out | `slp/osfmk-mklinux` `0d8ebab`, narrowed: the six sources these name are not in DR3 or in the MkLinux CVS tree, so OSF's configuration cannot be built from DR3 alone (*corrected 20 October 2026:* MkLinux 1.0b2's OSF tree has them, `docs/provenance/sources.md`). Configuration, not code; `conf/AT386/files` stays DR3's. The site says so |
| `lites/conf/gensym.awk`: each `asm` string begins `"\n"` instead of a newline | `test_mk7.3` `dev`'s `tools/lites/lites-osfmk73.patch`. A string literal may not span lines since GCC 3.3; the assembler sees the same text |
| `lites/conf/newvers.sh`: `printf "%s\n"` for five `echo`s | the same patch. Debian's `/bin/sh` has a System V `echo`, which turns `\\n` into a newline inside `vers.c`'s strings |
| `lites/server/serv/user_copy.c`, `copyinstr()`: the byte copy spelled out | the same patch. GCC 4 dropped the cast-as-lvalue `(*((char *)to)++ = *cp++)`, and `to` is a register variable, so `*(char **)&to` is not open |
| `lites/server/kern/kern_sig.c`, `lites/server/serv/serv_syscalls.c`: `case (integer_t)SIG_DFL:` and its kind, cast to each switch's own type | 4.4BSD-Lite2's `kern/kern_sig.c`, `issignal()`, which casts its labels `(long)SIG_DFL`; 4.4BSD-Lite, Lites' base, did not. Function-pointer constants are not case labels to current GCC |
| `lites/include/sys/exec_file.h`, `ATSYS_NAMES`: `m "bad"` for `m ## "bad"`, and so for each | `test_mk7.3` `dev`'s `tools/lites/lites-osfmk73.patch`. `##` does not paste two string literals into one; adjacent literals concatenate without it |
| `lites/emulator/i386/ecrt0.c`, `lites/emulator/emul_init.c`, under `OSFMACH3`: `mach_init()` called through `_mach_init_routine` | OSFMK 7.3's own: its libmach keeps `mach_init` static and exports the pointer, and its crt0, `libsa_mach/crt0.c`, calls `(*_mach_init_routine)()`. CMU's libmach exported `mach_init` itself |
| `lites/server/serv/xmm_interface.c`, under `OSFMACH3`: `seqnos_memory_object_init` in place of `_notify`, answered with `memory_object_change_attributes`; `_terminate` without the name port; `_change_completed` with OSFMK's arguments; `_discard_request` added | OSFMK 7.3's own: `mach/memory_object.defs` gives each handler's arguments ("skip; was memory_object_notify", "skip; was memory_object_establish" in `mach.defs`), and OSF's `default_pager/dp_memory_object.c` answers `memory_object_init` with `memory_object_change_attributes` and has `discard_request` as a stub. The attributes Lites sets go in the structure and flavor 7.3 keeps for them, `old_memory_object_attr_info`, `OLD_MEMORY_OBJECT_ATTRIBUTE_INFO`. Without these, libmach's `Smem_svr.o` calls handlers with arguments they do not take, and two it does not find |
| `lites/server/kern/kern_exit.c`, `wait4()`: the "major hack for BSD init compatibility", which hides pid 2 from pid 1, not compiled under `OSFMACH3` | OSF's own design: the OSF Research Institute's "Optimizing Mach-based Systems by Server Co-location" (Condict, Mitchell, Reynolds, August 1993), section 5.1: under the bootstrap task "the OSF/1 server runs /sbin/init. There is no need for mach_init as the service server is already running in the bootstrap task" -- this tree's `bootstrap/service.c`, "Program: Service server". The hack hides USER's `mach_init`, which forked and stayed as pid 2 while pid 1 became `/sbin/init`; with no `mach_init`, pid 2 is init's first child, which init must wait for. MkLinux 1.0b2, OSF's other server on OSFMK, ships a `mach_init` that only execs `/sbin/init` in place. Utah's 1.1.u3 binary on Mach4 (the MachUK22 image) shows the same failure with the link: "wait for /bin/sh on /etc/rc failed: No child processes" |
| `lites/server/Makerules`, `lites/emulator/Makerules`, `lites/conf/Makerules`, under `OSF_ENVIRONMENT`: OSFMK 7.3's libraries, `-lcthreads -lmach -lsa_mach -lmach`, for 1995's `-lcthreadsZ -lmachZ -lsa_machZ`; no crt0 file, `-u __start_mach -e __start_mach` | OSF's own: `default_pager`'s link line in this tree, and `makedefs/osf.gcc.mk` line 41, `_ELF_SA_MACH_LDFLAGS_`, for every standalone Mach program. `OSF_ENVIRONMENT` is Lites' authors' switch for an OSF sandbox; this brings it from OSF Grenoble's `nmk18b9` to OSFMK 7.3. Its Grenoble tool paths are left as written and overridden from `lites/makefile`, as site settings |
| `i386/locore.s`: about ten operands of the wrong width for their suffix -- `movl $USER_DS,%ecx`, `cmpl`, `movb %fs:(%esi),%al`, `addb $0x10,%cl` and the like -- which GNU as 2.42 rejects | `test_mk7.3` `dev`'s `3036fdb` (verdict 9). The sites carry no note; the file's one note is `trap_set_segs`', where three of `dev`'s spellings are bytes since 0076 |
| `i386/AT386/hd.c`, `hdprobe()`, `hd_read_id()`, `set_controller()`: nIEN set around the three polled commands | `test_mk7.3` `dev`'s `052a105` (verdict), with the probe's RESTORE added; ATA's device control register, which the driver already writes for SRST. 0098, note at `hdprobe()` |
| `lites/server/serv/ux_server_loop.c`, under `OSFMACH3`: `cthread_stack_size` set to 64K | This tree's `default_pager.c`, `vm_size_t cthread_stack_size = 16 *1024;`, an OSF server setting the size cthreads would otherwise probe; 64K is the size Lites' first server already had from the bootstrap's 64K stack (measured). `docs/deferred.md`, S-19; 0116, note at the site |
| `vm/vm_kern.c`, `kmem_alloc_contig()`: `vm_map_find_space()`'s result checked before the entry it returns is written through | the MkLinux project's CVS tree (`osfmk-src-12.24.99`), `vm_kern.c` of 10 April 1999: the same check, moved up. On failure there is no entry. 0125, note at the site |
| `device/net_device.c`, `IOBUS_LOCK_INIT`: `net_driver_intr` as the interrupt routine, where DR3 gave `net_driver_start` twice | the same tree, `net_device.c` of 17 January 1999; `iobus.h`'s macro takes the start routine, then the interrupt routine. Not built by our configurations. 0125, note at the site |
| `scsi/rz_disk.c`, the disk's size from READ CAPACITY: `+ 1` outside the `\|` of the four address bytes | the same tree, `rz_disk.c` of 6 February 1999. In DR3's line `+` binds first, and a last block address ending in 0xff loses its carry: a last address of 0x1ff gives 0x100 blocks, not 0x200. 0125, note at the site |
| `libmach/mach_msg_server.c`, `mach_msg_server_once()` and `mach_msg_server()`: both buffers freed on every way out, through a `cleanup:` label and two flags; in `mach_msg_server_once()` the flags' swap ends `allocatedReply = allocatedTemp` | the same tree, `mach_msg_server.c` of 15 February 1999, but for that one line: its `mach_msg_server_once()` assigns from `allocatedRequest`, already swapped, where its own `mach_msg_server()` uses `allocatedTemp`. DR3 returned early and leaked both buffers. 0125, note at the top of the function and at the line |
| `libsa_mach/printf.c`, `_doprnt()`: `%h` accepted; a precision pads numbers with zeros; zero padding goes after the sign and prefix, and only when not left-adjusted | the same tree, `printf.c` of 1999, but for one line: left-adjusted output is padded on the right with spaces, as DR3 has it and C (7.19.6.1, the `-` flag) requires, where the 1999 file pads with `padc`. Checked on the host against the C library for `%5d %-5d %05d %.5d %8.5d %-8.5d %hd %#x %-05d`: the 1999 code agrees in every case, DR3 in four of nine. 0125, note at the site |
| `bootstrap/bootstrap.c`, `parse_config_file()`: `bootstrap.conf`'s line ends made uniform first (a lone CR a newline, a CR before or after a newline a space), and the scans then look for `\\n` alone; the line loop's `*ptr != '\\r'` becomes `*pptr` | the same tree, `bootstrap.c` of 1999, without its two `static`s and a whitespace line. DR3 checked CR in some scans and not others, and tested the line's first character for CR where it meant the current one. Booted on QEMU: with CR-LF line ends DR3's bootstrap and this one both start Lites; with CR alone DR3's reads the file as one line, starts `default_pager` twice and panics ("there is another default pager"), and this one starts Lites. 0125, note at the site |

## C -- our judgement, no donor line

Each is an edit no tree makes. They should be small and every one says
so at the site.

| what | why no donor, and the nearest precedent |
|---|---|
| `file_systems/AT386/machdep.mk` and `fs_switch.c`: the bootstrap task's file systems cut to UFS | A build change: `minixfs` (partly GPL) and `ext2fs` (derived from Linux's header) are MkLinux's additions and are no longer compiled for AT386; their sources stay as imported. The nearest precedent is OSF's own `HP700/fs_switch.c`, which has no minix reader. `docs/provenance/precedent.md`, "What OSF booted from" |
| `kern/bootstrap.c`, `do_bootstrap_compat()`: read-only PT_LOAD segments mapped read-only; `regions[]` sized by `NREGIONS`, five, and the bound tested | No tree we can read has this loader with a read-only arm; `dev`'s `11ecb84` is the nearest, and its `p_vaddr > e_entry` test is replaced by the bound it was unknowingly avoiding (verdict 26). The arms beside it were taken to be OSF's; they are not -- the whole ELF `PT_LOAD` parser in `do_bootstrap_compat()` is `slp/osfmk-mklinux`'s `444954a` (2011), which replaced OSF's `loader_info` code (corrected 12 October 2026; `docs/provenance/precedent.md`, "Where the two imports came from") |
| `lites/server/serv/server_init.c`, `get_config_info()` under `OSFMACH3`: with arguments from `bootstrap.conf`, the flag words of the kernel's `host_get_boot_info` reply go ahead of them | No tree has it. Lites already fetches the reply, but only with no arguments, which OSF's bootstrap task never gives, and then prints "Boot info IGNORED"; its own `parse_arguments()` marks `case 'q'` "this is what kernel (and bootstrap) passes". The Release Notes say the kernel's `-s` is "passed to server", and `host_get_boot_info` is how OSF's kernel passes it. The reply's root device is not used (O-4), and a line with no arguments is left as Utah had it. Written fresh, from Lites' and OSF's own code. `docs/deferred.md`, O-7; note at the site |
| *Superseded 18 October 2026 by 0107 (E):* `lites/server/serv/server_init.c`, `parse_arguments()` and `get_config_info()` under `OSFMACH3`: with no root given, the kernel's boot device (the first word of the `host_get_boot_info` reply, `boot_device_name`) before Utah's `default_root`; with no server directory, the directory of the server's own path when OSF's bootstrap gives it as `/dev/...`, its `boot_device` alias named as the boot device; a line with no arguments parsed through; the flag loop bounded by `argc` | No tree has it. Release Notes bug 2271 says the default root should be the boot partition. Lites' own derivation of the directory from its path is kept for a CMU path; the bound keeps flags alone from reading past the end of the arguments. `docs/deferred.md`, O-1 and O-4; notes at the sites |
| `lites/conf/MASTER`: the option `noemulator` (`NO_EMULATOR`, `no_emulator.h`) | Configuration, not code: Lites' own option mechanism, one line beside `osfmach3`; no Lites names a no-emulator server. Read by nothing yet; the configuration `STD+WS+osfmach3+noemulator` is built by `build_world` (0109; `docs/roadmap.md`, Tier 1 item 1); note at the line |
| `kern/bootstrap.c`, `do_bootstrap_compat()`: each region sized from `trunc_page(p_vaddr)` to `round_page(p_vaddr + size)` | The tree's own: `bootstrap/elf.c` measures the servers' segments from their page, `p_vaddr + p_filesz - trunc_page(p_vaddr)`. The loader is `444954a`'s, and binutils 2.7's `ld` lays a data segment off a page (`docs/deferred.md`) |
| `i386/locore.s`, `trap_set_segs`: DR3's three segment moves written as `.byte`s | The same file: `set_cr4` writes `movl %eax,%cr4` as `.byte 0x0f,0x22,0xe0` for an assembler that lacked it. GNU as 2.42 and binutils 2.7 each reject one of the two spellings, and `movw` changes the encoding (`docs/deferred.md`) |
| `i386/start.s`, `i386/AT386/model_dep.c`, `kern/bootstrap.c`: two ways in -- `%eax` kept at entry, multiboot's path taken only for `0x2BADB002`, DR3's `asm_startup.h` otherwise; `parse_multiboot()` only after a multiboot entry; the ELF reader skipped when OSF's loader gave the regions | DR3's own text for OSF's way in, restored from under `#if 0`; the dispatch is ours. The multiboot specification's `%eax` value is the line between the two. DR3's `do_bootstrap_compat()` returned the same way when it had no load info (verdict `444954a`) |
| `i386/AT386/asm_startup.h`: `testb $x,%dl` for `%edx`, three times | The register the `b` suffix tests. binutils 2.7, which builds the kernel on the running system, encodes both `f6 c2 ib` (measured); GNU as 2.42 rejects `%edx` |
| `i386/AT386/hd.c`, `hd_read_id()`: `restore_request` 0 at probe | No tree has it; b2, DR3 and the CVS tree all set 1 there and issue no restore. The flag's other use, `hderror()`'s recalibration, sets it with `CMD_RESTORE` and is DR3's. slp's `7e81018` turned both off (verdict `7e81018`) |
| `lites/makefile` replaced, `lites/Makeconf` removed: Lites' configure build run by ODE | No tree builds Lites under OSFMK's ODE. Utah's `makefile`, which drove the same configure build with Utah's paths, gives the name and the role; CMU's ODE `Makefile`s stay as shipped. It turns on `OSF_ENVIRONMENT` with this sandbox's tools, and runs configure with Kernighan's awk, the one `server/kern/vnode_if.sh` was written for: mawk rejects the script for a function it calls but never defines. Nearest: Lites' own `OSF_ENVIRONMENT` block in `conf/Makerules`, which pointed the same build at an OSF sandbox's tools, and OSF's step 8, `build ... osf1_server`. The file says so in its AI-ONLY NOTE. `docs/provenance/precedent.md`, "How Lites meets ODE" |
| `lites/server/Makerules`: `bsd_server.o: bsd_1_server.c`, was `bsd_server.c: bsd_1_server.c` | No tree has it. `serv/bsd_server.c` is a wrapper that `#include`s MIG's `bsd_1_server.c`; written on the source, the rule makes GNU make remake a VPATH file and look for it in the object directory. The site says so |
| `lites/server/serv/device_reply_hdlr.c`, under `OSFMACH3`: `mig_reply_error_t` for `mig_reply_header_t` | No tree has it. CMU's MIG name for the reply with a return code; OSFMK's `mach/mig_errors.h` names the same thing `mig_reply_error_t` and has no other |
| `lites/server/serv/ux_server_loop.c`, under `OSFMACH3`: Lites' `cthread_mach_msg` stub renamed for the file | No tree has it. The stub's own comment says the call was "missing from cthreads"; OSFMK 7.3's `cthreads.h` declares it, with a struct argument, and the two declarations collide. Lites keeps its stub. *Corrected 14 October 2026:* OSFMK's `libcthreads` defines none of the three; `docs/deferred.md`, "Lites' receive threads" |
| `lites/server/kern/subr_prf.c`, under `OSFMACH3`: `vprintf`, `printf`'s body for a `va_list` | No tree has it. OSFMK 7.3's libmach writes `fprintf_stderr` through `vprintf` unless the program sets `vprintf_stderr_func`, and libcthreads calls it; with no `vprintf` of the server's own, libsa_mach's `printf.o` is linked for it, bringing a second `sprintf` and, through `usleep`, a second `sleep`. Lites already pairs `vsprintf` with `sprintf` |
| `lites/server/serv/vn_pager_misc.c`, `vnode_pager_uncache()`: `silent_overwrite` and `advisory_pageout` for `write_completions` | No tree has it. OSFMK 7.3's `memory_object_behave_info` has the two in the one's place; all three off |
| `mach_services/lib/migcom/AT386/machdep.mk` and `mach_services/cmds/config/AT386/machdep.mk`, new: the two host programs built for AT386 on a Linux host | Both `Makefile`s include `${TARGET_MACHINE}/machdep.mk` when it exists, and OSF wrote one only for `POWERMAC`, the target MkLinux built on Linux. Ours follow those: `migcom`'s adds `-I.`, as `POWERMAC`'s does, because `yacc` writes `parser.c` in the object directory and it includes `"lexxer.h"` from the source directory; `config`'s names the host's `yacc` as `POWERMAC`'s names its `lex` -- `byacc`, because `gram.y` writes actions in the old `= { }` form, which bison rejects -- and empties `LIBS`, because `-lln` was OSF/1's lex library and `parse.l` supplies its own `yywrap` |
| `stand/AT386/boot/include/boot.h` and `lib/io.c`: `size_t strlen()` for `int` | No tree has it. `secondary/mach_boot.c` includes `mach/fs/file_system.h`, which in DR3 includes `<string.h>` -- libsa_mach's, ANSI's `size_t strlen()` -- where 1.0b2's included a `strings.h`; DR3's boot was not built with DR3's `file_systems` (its top `Makefile` has no `stand`). The boot's own definition changes with the declaration |
| `stand/AT386/boot/{lib,primary,secondary}/Makefile`: `-DASSEMBLER -D__LANGUAGE_ASSEMBLY` for the assembler files | 1.0b2's rules pass `-D__LANGUAGE_ASSEMBLY`, which its `machine/asm.h` tested for `ALIGN`; DR3's `asm.h` tests `ASSEMBLER`, which the kernel's `template.mk` and libcthreads' `AT386/machdep.mk` pass, and `mach_perf`'s `AT386/machdep.mk` passes both. `include/boot.h` still hides its C under `__LANGUAGE_ASSEMBLY` |
| `lites/server/serv/ux_exception.c`, `serv/serv_syscalls.c`, `i386/serv_machdep.c`, `kern/init_main.c`, `emulator/i386/e_signal.c`, `include/sys/ushared.h`, under `OSFMACH3`: the exception port registered `EXCEPTION_STATE_IDENTITY`; `catch_exception_raise_state_identity` written, a caught signal sending the faulting thread to the emulator's signal entry in the reply; the entry recorded in the shared page, `us_sigtramp` | No tree has it. OSFMK's `exception()` resumes a thread only by the reply, where Mach 3 and Mach4 resumed one that `do_signal_notify()` had aborted (S-7). The mechanism is OSFMK's own, `exception_raise_state_identity` in its `exc.defs`, and the frame is the emulator's `do_signal_notify()`'s; what joins them is ours. `catch_exception_raise` takes OSFMK's `(exception, code[], codeCnt)`, as `bootstrap/bootstrap.c`'s does (S-3) |
| `mach_services/lib/libcthreads/cthreads.c`, `cthread_idle()`: the idle loop reads the run queue's head through a `volatile` pointer | No tree has it. The loop's other operand, `undepress`, is declared `volatile` in `struct cthread`, and `spin_lock_t` is `volatile int` (`include/i386/cthreads.h`): OSF marked what busy loops read, and this field was missed; today's GCC hoists its load out of the loop, and the system stops (S-9) |
| `i386/interrupt.s`: a spurious IRQ15 sends the master its EOI before returning | NetBSD 1.0's `i386/isa/vector.s`, whose slave entries EOI IRQ2 at the master for every interrupt the slave raises. The slave's own ISR is still read as OSF's code did, and a spurious IRQ7 still gets no EOI (S-10) |
| `i386/AT386/autoconf.c`, the `ne` entry: `neintr` for `at3c501intr` | MkLinux's entry, marked "MITCHEM", was copied from the 3c501's above it and kept that card's handler; `nedriver`'s own is `neintr`, declared in `if_ne_entries.h` |
| `i386/AT386/if_ne.c`, `nefetch()` and `neput()`: the odd last byte indexed as a byte; `nefetch()`'s wait masks the value read | The same file: `neput()`'s wait is `(inb(nec+ds0_isr) & DSIS_RDC)`, the form `nefetch()` lost to a misplaced parenthesis; `up` is an `int *`, so `*(up+len-1)` addressed word `len-1`, not byte `len-1` |
| `lites/emulator/e_bsd.c`, `e_adjtime()`: `olddelta` received into a local and copied out only if not null | The same file: `e_setitimer()` treats its `ovalue` so. 4.4BSD-Lite2's `adjtime()` (`kern/kern_time.c`) writes `olddelta` only `if (uap->olddelta)` (S-5) |
| `lites/server/serv/ux_exception.c`, `ux_exception()`, under `OSFMACH3`: a protection failure in the first page is SIGSEGV | No tree has it. The signal rule is every BSD `trap()`'s; what differs is that OSFMK's `mach_init()` reserves page 0 where NetBSD 1.0's a.out layout (`N_TXTADDR` = `__LDPGSZ`) leaves it unmapped, so the page is treated as the absent page it is to the BSD process (S-4) |
| `lites/server/serv/serv_syscalls.c`, `thread_signal()`, under `OSFMACH3`: when the redirect's frame cannot be written, SIGILL with its default action | 4.4BSD-Lite2's i386 `sendsig()` (`i386/i386/machdep.c`), "Process has trashed its stack; give it an illegal instruction to halt it in its tracks"; NetBSD 1.0's, `sigexit(p, SIGILL)` |
| `lites/server/serv/ux_exception.c`, `ux_page0_mapped()`: S-4's rule only when page 0 is the reservation | The `vm_region()` call is `serv/device_misc.c`'s; a write to text at 0 keeps NetBSD 1.0's own answer, SIGBUS |
| `lites/emulator/i386/e_machinedep.c`, `take_a_signal()`: `%eax` no longer set to 1 after a handler returns | `take_signals_and_do_sigreturn()` in the same file sets it for its own setjmp test, the only path that wants it |
| `mach_services/include/i386/cthreads.h`, `spin_try_lock()` and `spin_unlock()`: a `"memory"` clobber | `libcthreads/i386/lock.s` has the same two as functions, which a compiler cannot move memory across; the inline forms now match them |
| `lites/server/serv/ux_server_loop.c`, under `OSFMACH3`: `ux_server_max_kernel_threads` 0 | Lites' own value where cthreads cannot hold its limit (`USEACTIVATIONS`); the limit's own comment, "at least 5 (wired) + 1 (running) + receive_max", needs a `cthread_mach_msg` that bounds receivers, which OSFMK's `libcthreads` does not define (S-13) |
| `lites/server/serv/xmm_interface.c`, `seqnos_memory_object_data_return()`: `wlen` cut short only for the page that crosses the file's end | No tree has it; every copy sets it for every page. The line moved into the `if` whose message prints it, so `wlen` keeps its starting value, one page, otherwise -- what 4.4BSD's `vnode_pager_io()`, which this pager replaced, does in Lite and Lite2 (S-14) |
| `lites/include/sys/vnode.h`: a prototype for `vn_pageinout()` | No tree has it. Beside `vn_rdwr()`'s, which it copies but for the cache-state hook; without it the pageout's 32-bit offset shifted the `off_t` and every argument after it (S-14) |
| `conf/template.mk`: `hardclock.o_CFLAGS` gives `-fno-optimize-sibling-calls` where `GCC_LATEST` names a compiler newer than 2.x | No tree has it; the file's own per-object hook, as its `MIG_CFLAGS` lines use it (`5af8713`; guard since 0085). Note at the site; `docs/provenance/pristine.md` has the alternatives measured |
| `i386/start.s`, `i386/AT386/asm_startup.h`: the multiboot magic kept in `%edx` and stored by the first CPU only; `movb $1,%al` and `movb %dl,wncpu` in the `NCPUS > 1` path | No tree has it; every CPU enters at `pstart` (`mp/mp_v1_1.c`). 0080, notes at the sites |
| `i386/start.s`, `i386/genassym.c`: after a multiboot loader, the first free address is the highest end of the kernel, the information block, the command line, the module list and each module, the offsets from `multiboot.h` through `genassym` | Mach4's `init_alloc_aligned()` keeps clear of the same regions; slp's constant `0x500000` before. 0081, note at the site |
| `build_world`: the `default_pager`, `lites`, `libservice`, `libnetname` and `netname` steps | MkLinux's own lines, uncommented, and OSF's step 8 for the server (`2eceaae`, `b39da9e`, `2bcf090`, `f251a43`); notes at the lines |
| `build_world`: a second `lites` step, `LITES_CONFIG=STD+WS+osfmach3+noemulator` | The server without the emulator, built beside the first from the same tree (0109; `docs/roadmap.md`, Tier 1 item 1); note at the line |
| `lites/conf/MASTER.h`: `fifo` added to `STD` | No Lites set has it. OSF/1's manual gives FIFOs as standard, not as an option (OSF/1 1.0, `mkfifo(3)`, `mknod(2)`), and Berkeley's GENERIC configurations all name `FIFO`; added to `STD`'s definition, so that the configuration keeps its name. The maintainer's choice. `docs/deferred.md`, S-20; 0119, note at the site |
| `lites/server/kern/kern_sig.c`, `postsig()`, under `NO_EMULATOR`: a caught signal ends the process, with a console line, until handlers are run | Interim, no donor: Lites panics there, and the emulator ran handlers. 4.4BSD's `sendsig()`, `docs/roadmap.md` Tier 1 item 2 step 5, replaces it. 0131 |
| `lites/conf/files`, `conf/i386/files`, `conf/MASTER`'s note: `noemul_sysent.c`, `kern_exec.c` and `i386/syscall.c` built under `noemulator` | Lites' own `fileif` mechanism. 0131 |

**The two Lites rows above are build-level, not source.** Lites'
source changes are the `lites/` rows of kinds B and C, each with a note
at its site; `git diff 9d1c98a HEAD:osfmk/src/lites` shows them all.

## D -- transplanted, never in the receiving tree

| what | from |
|---|---|
| `lites/server/kern/noemul_sysent.c`, new, built only under `NO_EMULATOR`: the system call table of a process the server runs without the emulator, 4.4BSD's numbers and argument words, each row given the server's own function or `nosys` | 4.4BSD-Lite2's `sys/kern/syscalls.master` (8.6, Berkeley, 3/30/95), as it stands in `nmartin0/4.4BSD-Lite2` `dev3` at `8954f549`, the distribution's file unchanged; under the Regents' BSD licence. Generated by `docs/tools/mknoemulsysent.py`, which reads the server's compiled sources for the functions; the file says so. NetBSD 1.0's `<sys/syscall.h>` was compared number by number: the same calls but its own 0, 165, 169--171 and 198. 0131 |

Expected: nothing large. The point of the design is that the
personality is reused unchanged.

## E -- written from an OSF specification

Entries, with the section that specifies each: the first four expected, the rest made.

| what | specification |
|---|---|
| The exception-driven system-call entry. *Made 19 October 2026 (0131)* as `lites/server/i386/syscall.c`, below | Patience §3–4; Wells 1994 Fig. 2 for the five stages |
| `THREAD_STATE_SYSCALL` | Patience §4: "a small subset of registers adequate for handling most system calls for most personalities. For the i386, this register set is currently 5 registers (as opposed to 17 for the full set)" |
| collocation registration | MK6.1 release notes, `mach_subsystem_create` and `mach_port_allocate_subsystem` reference pages |
| the server's main loop conventions | OSF Mach Server Writer's Guide, ch. on Basic IPC-Based Servers |
| `lites/server/serv/server_init.c` under `OSFMACH3`: the OSF/1 server's arguments -- flags and a root -- parsed from the kernel's boot information first and `bootstrap.conf`'s after, so that a root on the line wins; a further word ignored, with a message; no server directory; the root the boot device, else Utah's `hd0a`; `-a` asking after both, in OSF's words; `/sbin/init` the first program, Utah's `-e` and `-i` for others. Utah's `parse_arguments()` and globals kept unchanged for the other kernels (0107; supersedes 0106) | `bootstrap/bootstrap.template` (the server takes "-s (single-user), -a (prompt for root device), and a root file system name"); MK7.3 Release Notes §2.4.2.1 (`root device: [fd0b]`), bug 2271 (the default root is the boot partition), Boot Options (`-s` "passed to server"); OSF RI co-location paper §5.1 ("The OSF/1 server runs /sbin/init"). `docs/deferred.md`, O-1, O-2, O-4, O-5; notes at the sites |
| `lites/server/serv/serv_syscalls.c` under `OSFMACH3`: the bootstrap service on each task's bootstrap port, its process port, through OSF libmach's `bootstrap_server()`; `ux_server_loop.c` asks it before `bad_request_server()`; `syscall_subr.c` names op 1020 for tracing (0108) | OSF RI co-location paper §5.1 (the OSF/1 server "implements the bootstrap service for its child tasks"; the answer "its own policy decision"); OSF Server Library Interfaces, `mach_privileged_host_port` and `mach_device_server_port` ("will succeed only for privileged tasks", the same ports `task_by_pid(-1)` and `(-2)` give); `mach/bootstrap.defs` (999999 skipped as old); for empty arguments and environment, `mach_perf/lib/server.c`'s own bootstrap server and `libsa_mach` built `OLD_COMPAT` on i386. `docs/deferred.md`, O-6; notes at the sites |
| `lites/server/i386/syscall.c`, new, under `NO_EMULATOR`: each process's thread sends `EXC_SYSCALL` to the process's request port, `EXCEPTION_STATE`, `i386_THREAD_STATE`; `bsd_syscall_server()` takes the message's sequence number in turn and `catch_exception_raise_state()` makes the call -- arguments from the user stack, `noemul_sysent`, carry flag and errno, `ERESTART` back over the 7-byte `lcall`, `EJUSTRETURN` with the registers the call set, pending signals taken on the way out; `ux_server_loop.c` asks it before `exc_server()`, and `ux_exception.c`'s stub is not built | Patience §3 (a port of the server's, not the debugger's), §4 (`EXCEPTION_STATE`; the port names the thread), §7 (the integrated kernel's `syscall()` with a preamble and postamble). The register conventions are 4.4BSD-Lite2's i386 `syscall()`, read in `nmartin0/4.4BSD-Lite2` `dev3`, and the emulator's (`emulator/i386/e_trampoline.c`); errno conversion the emulator's (`emulator/error_codes.c`). 0131 |
| `lites/server/serv/user_copy.c` (`copyout`, `copyin`), `kern/kern_subr.c` (`uiomove`, `ureadc`), under `NO_EMULATOR`: user data written with `vm_write` and read with `vm_read_overwrite`, EFAULT for a bad address; `user_reply_msg.c`'s staging panics if reached | Patience §5.1; OSFMK 7.3's trap -30 and `vm_map_copy_overwrite_unaligned`. 4.4BSD's `uiomove` and `ureadc` use `copyout` and `subyte` |
| `lites/server/serv/server_exec.c` and `kern/kern_exec.c`, under `NO_EMULATOR`: `execve()` copies the strings in and calls `s_execve()`, which loads a NetBSD a.out without the emulator, builds the emulator's stack, zeroes the BSS fragment, sets the registers and the syscall port; the calling thread is not aborted out of its exception RPC | Patience §7. Lite's `execve` body was deleted; the stack is Lites' own (`emulator/emul_init.c`, `emul_setup_argv()`), the load the emulator's (`emul_exec.c`); `dev3`'s written `execve` for the argument conventions (`E2BIG`, `EFAULT`). 0131 |
| `lites/server/kern/kern_sysctl.c`, `__sysctl()`, under `NO_EMULATOR`, and its row in `noemul_sysent.c`: the name and `*oldlenp` copied in from the process, the old and new values through buffers in the server, `s___sysctl()` called, the value and `*oldlenp` copied out | Patience §7. 4.4BSD-Lite2's `__sysctl()` (`kern/kern_sysctl.c`, `nmartin0/4.4BSD-Lite2` `dev3`) for what is copied in and out and when; Lites' own `bsd_sysctl()` (`serv/bsd_server_side.c`) for the buffers `s___sysctl()` takes. Measured by `docs/tools/noemuprobe/`: `hw.pagesize` 4096, `kern.ostype` "Lites", a size-only query, `ENOMEM` for a short buffer, `EFAULT` for a bad name. 0132, note at the site. A length that wraps `round_page()` is `EFAULT` before the call, as Lite2's `useracc()` refuses it; a bad old buffer is found only when the value is copied out, after a new value is set, where Lite2 checks first (0133) |
| `lites/server/serv/vm_syscalls.c`, `obreak()`, and `serv/server_exec.c`'s `vm_daddr` and `vm_dsize`, under `NO_EMULATOR`, and row 17 of `noemul_sysent.c`: `break` grows or shrinks the data with `vm_allocate` and `vm_deallocate` on the process's task, within `RLIMIT_DATA`; the loader records the data's start and its size to the end of the last section | Patience §7. 4.4BSD-Lite2's `obreak()` (`vm/vm_unix.c` 8.2, `nmartin0/4.4BSD-Lite2` `dev3`), under the same Utah and Regents notice as `vm_syscalls.c`, with `p->p_task` for its `vm_map` and a cast for Mach's `round_page()`; the break where the emulator puts it (`emulator/emul_exec.c`, `emul_exec_load()`, the end of the last section; `e_obreak()`). Measured by `docs/tools/noemuprobe/`, now more than a page of text: four pages grown, written and copied into, shrunk away (`EFAULT`), and `ENOMEM` past the data limit. 0134, notes at the sites. Not Lite2's arithmetic: its signed `(int)(new - old)` lets a break near 3 GB pass `RLIMIT_DATA` and shrink where it should grow, and a `round_page()` that wraps frees the process's pages from 0; addresses compare unsigned and a break of 0 is `ENOMEM`, as `e_obreak()` has them and NetBSD's `uvm_unix.c` `sys_obreak()` (1.51) fixed Lite's |
| `lites/server/kern/vfs_vnops.c`, `vn_stat()`, under `NO_EMULATOR`: the `stat` structure zeroed before it is filled | Found by an independent review: `st_lspare` and `st_qspare` are not set, and without the emulator the server copies the structure out (`stat`, `lstat`, `fstat`), so 20 bytes of a server thread's stack, which serves every process, went to the process. Zeroed first, as NetBSD's `vn_stat()` does (`kern/vfs_vnops.c`, `memset(sb, 0, sizeof(*sb))`); 4.4BSD-Lite2 has the gap. With the emulator the structure was built in the process. Measured by `docs/tools/noemuprobe/`: the spares come back 0 over a buffer filled with 0x77. 0135, note at the site |
| `lites/server/kern/kern_descrip.c`, `fstat()`, and `kern/sys_socket.c`, `soo_stat()`, under `NO_EMULATOR`, and row 189 of `noemul_sysent.c`: a file's status by `vn_stat()`, a socket's by a zeroed `stat` marked `S_IFSOCK` with the protocol's block size, device and inode, copied out | Patience §7. 4.4BSD-Lite2's `fstat()` (`kern/kern_descrip.c` 8.8) and `soo_stat()` (`kern/sys_socket.c` 8.3), under the same notices as Lites' copies of those files; `soo_stat()` asks Lites' `soo_getattr()`, as Lites' protocols fill a `vattr` in `PRU_SENSE` where Lite2's fill a `stat`, and zeroes where the emulator's `getattr()` leaves `VNOVAL` (its socket mode, 0xffff, is not `S_ISSOCK`). Not Lite2's `panic()` for another file type: `open()` enters its file untyped and can sleep (S-23), so `EBADF`. Measured by `docs/tools/noemuprobe/`: a device, a file against `stat`, the spares zeroed, an unaligned buffer, `EFAULT`, `EBADF`, a socket. 0136, notes at the sites |

The grant these rely on is printed in each document:

> Permission to reproduce this document without fee is hereby granted,
> provided that the copyright notice and this permission notice appear
> in all copies, derivative works or modified versions.

**A specification settles what to build, not whose line to write it
as.** After reading Patience §4, the spelling is still chosen by the
order in `precedent.md`.

## F -- an upstream's own file, restored

| what | from |
|---|---|
| `mach_services/lib/migcom/` and `mach_services/cmds/config/`, the sources of the two host programs the build runs, replacing `slp/osfmk-mklinux`'s `4df2e7d` (2011) | The MkLinux project's CVS repository, `:pserver:public@cvs.mklinux.org:/MkLinux`, module `osfmk`, as checked out on 24 December 1999 (the maintainer's copy; the server is gone): `migcom` at revision 1.1 of 16 May 1999, its `Makefile` 1.2 of the same day; `config` at 1.1 of 20 June 1999. Each file's notice is "Copyright 1991-1998 by Open Software Foundation, Inc.", with CMU's; `config`'s files add Berkeley's of 1980 and 1990, from BSD's config(8). `diff -r -x CVS` against the checkout shows only the two `AT386/machdep.mk` files of kind C |
| `lites/`, the whole tree, replacing xMach's CVS copy of 2000 | Utah's `lites-1.1.u3.tar.gz`, 30 March 1996, unpacked in place: 892 files, byte for byte, with the tarball's modes. SHA-256 `f81eab46e47786a6db7217aab6d3b29534ff24a8314a889e8e249568e7042c93`, 1,763,728 bytes, the size `ftp.lip6.fr` lists. Its tree is `9d1c98a`, the import the deviation record is read against (`pristine.md`). Two empty directories, `liblites/parisc` and `server/netiso/xebec/test`, are not carried: git has no empty directories. The mode change once made here on `conf/config.guess` and `conf/move-if-change` (kind C) is gone with it: the release has both executable |
| `lites/server/serv/xmm_interface.c`, Utah's post-release copy | `ftp.lip6.fr/pub/mach/mach4/mach/lites/xmm_interface.c`, a mirror of Utah's `flux/lites/`: 17,042 bytes, dated 5 May 1996, SHA-256 `3d6c46a36552f9368a83e08a1d896e6ab0d0bfdbad6879521e76bb632277b759`, fetched by the maintainer. Against the release's 16,946 bytes it changes two functions and nothing else: `seqnos_memory_object_data_write` becomes a stub that panics, and its body moves to `seqnos_memory_object_data_return`, which the release left as the stub. OSFMK 7.3's `memory_object.defs` has `skip; /* was memory_object_data_write */` and its pageout daemon calls `memory_object_data_return` (`vm/vm_pageout.c` line 1381), so with the release's copy every pageout to a Lites-backed file panics. Neither xMach's copy nor any Lites tarball has this version |
| Files returned to DR3's text over slp's 2011--2012 changes: the kernel's sixteen assembler files under their `.s` names and the six of `libsa_mach` (`18c9d31`, `090a2dc`), `conf/AT386/template.mk` and `conf/AT386/files` (`0d8ebab`), the typeless declarations of ten files (`2913827`), `i386/proc_reg.h` and `intel/pmap.[ch]`'s `INTEL_PTE_GLOBAL` (`d4d387a`), `kern/lock.c` (`44e8969`), `mach/i386/vm_param.h` (`9335f94`), `ext2fs/ext2_fs.h` (`ebdad63`), `conf/files` (`63279d3`), `link/tools` (`f4fa636`), and `kern/boot_script.[ch]` deleted | DR3 as `slp/osfmk-mklinux`'s first commit, `0f6f7e6`, keeps it; each reversion and its reason is a row of `docs/provenance/verdicts.md`, "slp's eighteen commits". Restored text carries no note: the vendor's own is the note |
| `mach_services/cmds/` (compose, ptbldump, flipc_tests, flipc_examples, ipc_test, msg_test, mprof, etap and seventeen small Mach commands) and `mach_services/servers/` (s_xkern, netmsgserver, netmem, rpc, snames, and the files of machid and netmemoryserver DR3 lacked): 257 files DR3's `osfmk` tarball did not carry | OSF's companion package to DR3, `cmk1.1-980417.src.tgz` ("cmk1.1", OSF's 1991--1998 notice), shipped beside `DR3.osfmk.src.tgz` in `misc_source/` of the MkLinux DR3 CD (Prime Time Freeware, July 1998, on the Internet Archive). SHA-256 `2cf1b9a5621886d1256620e58aafcf77728382f9f44228bec4a95a73acac7883`. Only files the tree lacked, byte for byte; where both have a file, ours stays. Left out: `etap/skiplist.c` and `skiplist.h` ("THIS CODE IS UNDER THE GNU COPYLEFT"), and `rc_files/` and `setuplinks`, cmk's own sandbox. Not built yet, but for `compose` and `ptbldump` (`docs/tools/mkhostbin.sh`); `cmds/Makefile` stays DR3's |

**What they replace was older, not adapted.** `4df2e7d`'s files carry
"OSF Research Institute MK6.1 (unencumbered) 1/31/1995" and 6.1's
notice, the one with the advertising clause (kind A, the
`bootstrap.c` notes). With them came their objects, their `yacc`
output, two i386 binaries built from them, and a second copy of the
binaries in `tools/i386/i386_linux/hostbin`. All of that is deleted.
DR3's tarball had neither source, only PowerPC Linux binaries in
`tools/ppc/ppc_linux/hostbin`. `docs/tools/mkhostbin.sh` builds both
programs by DR3's own procedure, the `README` beside those binaries.

**The difference reaches one generated file.** Built with the 1999
programs, every MIG stub in the build is what slp's binaries produced,
byte for byte apart from the comment giving the generation date.
`confdep.h` loses four lines (`TIMEZONE`, `MAXUSERS`, `DST`,
`MAXDSIZ`), and no source includes it. The kernel is the same size,
1,008,208 bytes, and differs in seven bytes: a source path compiled
into an assertion string, and the build time.

## G -- ours, outside any upstream's tree

None inside a vendor directory. Outside them, ours: `docs/`, its
tools in `docs/tools/` (the sandbox, the host tools, the disk, the
boot under QEMU, the NetBSD root, the self-hosting root, NetBSD 1.0's
shell with its fix -- `netbsd10-sh.diff` carries four lines that are
NetBSD's, BSD-licensed, into NetBSD's own `jobs.c`), and
`.gitignore`; and, never committed, what `mksandbox.sh` generates --
`Buildconf.local` and the kernel's link script (0083).

---

# What was not done

## The emulator is removed rather than fixed

The largest refusal, and it is OSF's own. The emulator could be kept
and hardened; OSF spent two years trying and documented why they
stopped. Signal semantics were never fully correct because the emulator
switches stacks and the server cannot find the real user stack to build
a frame. A program can rewrite its own emulator and then exec a
setuid-root binary that inherits the tampered copy. Loadable syscall
modules cannot work because the vector is fixed when `init` loads.

What it costs: fifteen Lites files need rework, and the system is
slower until collocation lands.

## The personality is not OSF/1

It is 4.4BSD-Lite2. OSF/1's own was never released.

What it costs: where OSF/1 and 4.4BSD differ, we are 4.4BSD unless
OSF's public documents say otherwise. The project is a reconstruction
of the architecture, not of the system.

## Alpha, ECOFF and the loader are out of scope

OSF/1 1.3 and later from OSF RI — MK6 and MK7 — use ELF, rtld and
dlopen. The ECOFF `/sbin/loader` path belongs to OSF/1 1.0–1.2 and to
DEC's Alpha line.

What it costs: `exec_with_loader` is not implemented, and OSF/1 Alpha
binaries will not run. Neither was ever a goal.

## OSF/1 AD's distributed machinery is not carried

The AD papers describe a distributed OSF/1, its processes and files
spread across nodes. That is the AD branch, not the MK single server.

What it costs: nothing for a single-node system.

## Only public documents are the specification

The OSF/1 server's source was never released, and no unreleased or
GPL source is read for this project. We write from Patience, the
Server Writer's Guide and OSF's other public documents instead.

What it costs: what those documents leave open is decided by the
maintainer, and recorded.
