# LITES 1.1.u3 as the UNIX personality

Survey only. Nothing has been built or ported yet.

Source: `github.com/nmartin0/lites-1.1.u3`, 12 MB.

## Why it is the right candidate

The bootstrap task's built-in configuration names three servers:

```c
name_server name_server
default_pager default_pager
unix startup -s
```

`name_server` and `default_pager` both build from this tree. `unix` is
the BSD4.3 UX server, which was always distributed separately because it
was licence-encumbered, and is not present in either OSFMK 7.3 or 6.1.

LITES is the free 4.4BSD-Lite-based replacement.

## It has first-class OSF Mach support

`conf/MASTER`:

```
options     osfmach3  OSFMACH3  1  osfmach3.h
makeoptions osfmach3  TARGET_CFLAGS+=-D_ANSI_C_SOURCE -DOSF_LEDGERS=1 \\
                                     -DUNTYPED_IPC=1 -D__STDC__=1
```

88 `#if OSFMACH3` sites across `server/`, `include/`, `liblites/` and
`emulator/`. Both defines are OSF-specific:

- **`OSF_LEDGERS`** -- ledgers are an OSF addition GNU Mach does not
  have. They are the `root_wired_ledger`/`root_paged_ledger` ports our
  `do_bootstrap_ports` returns.
- **`UNTYPED_IPC`** -- the NDR message format OSFMK 7.x uses, as
  against the older typed IPC.

## The message format matches 7.3 exactly

This is the sharpest compatibility test and the two sides agree.

LITES, `server/serv/ux_syscall.c:81`:

```c
#if UNTYPED_IPC
	mach_msg_format_0_trailer_t *trailer;
#else
	static mach_msg_type_t bsd_rep_int_type = { ... };   /* old typed IPC */
```

OSFMK 7.3: `mach/ndr.h` defines `NDR_record_t`, the generated stubs
carry 232 `NDR_record` references, and `ipc_kobject_server` declares
`mach_msg_format_0_trailer_t *trailer` -- the identical type.

So LITES's OSF arm targets untyped/NDR IPC with format-0 trailers, which
is what 7.3 speaks, not the typed IPC of MK6.x and CMU Mach 3.

## Interface compatibility is structural, not lucky

LITES ships **5** `.defs` files and they are all its own interfaces:
`bsd_1`, `bsd_types`, `Nbsd_1`, `signal`, `emul_mach`. It ships **no**
Mach `.defs` -- no `device.defs`, no `mach.defs`.

`conf/Makerules:96`:

```make
MIG := $(wildcard ${INSTALL_BINDIR}/mig ${MACH_RELEASE_DIR}/bin/mig)
MIG := $(firstword ${MIG} mig)
```

It locates `mig` in the **target Mach's** release directory and
generates every Mach RPC stub from the target kernel's own `.defs`.
Whatever message ids, struct layouts and trailer formats OSFMK 7.3 uses,
LITES's stubs are produced to match, because they are produced from it.

This is the inverse of the Hurd situation:

| | LITES | Hurd servers |
|---|---|---|
| form | source, built against the target kernel | prebuilt binaries |
| Mach stubs | generated from *our* `.defs` by *our* `mig` | compiled against GNU Mach |
| dialect risk | structurally eliminated | open, untested |

We have the toolchain: `osfmk7.3/osfmk/tools/i386/i386_linux/hostbin/`
holds `mig` and `migcom`.

## Surveyed: nmartin0/mach_stuff

A 454 MB collection of extracted tarballs. What is in it, and what it is
worth.

### Directly useful

**`linux/arch/osfmach3_i386/`** -- a complete Linux personality running
on OSFMK, **on i386**. Three copies are present (`linux/`,
`mklinux-2.0.38-pre9/src/`, `Change/DR3/mklinux/src/`). This solves the
same problem LITES does, against the same kernel, on our architecture,
and it shipped and worked. It is the closest published analogue to this
project and the first place to look for any question about how a
personality talks to this kernel.

It already settled one: `arch/osfmach3_i386/Makefile:69` reads

```make
LDFLAGS = -e __start_mach -static -nostdlib
```

confirming that `__start_mach`, from `libsa_mach`'s crt0, is the correct
entry for a personality server here -- which had been reasoned out
independently and is now corroborated.

**`new_release_kernel/mach_servers/bootstrap.conf`** -- a real
bootstrap.conf from a working system:

```
# bootstrap.conf
-w default_pager default_pager 
-k startup vmlinux 
```

confirming the `[-flags] symtab_name path` format, and that the
bootstrap task's own flags come first.

**`pmk1.1/`** -- a third Mach 3.0 PMK tree, same version as ours,
differing from MkLinux in files we have patched (`hd.c`, `fd.c`,
`ipc_kobject.c`, `model_dep.c`). Useful as a cross-check, though it
carries the same bugs: its `getvtoc` still sizes the whole-disk
partition from `cmos_parm`, and its floppy code matches MkLinux's.

### Dates our tree

`DR2.1u6-wip971126.src.patch` (170k lines) and `u5-u6.patch` are MkLinux
DR2.1 update patches. Their one generic kernel change is to
`device/dev_name.c`, adding `lenunit = cp - name;` -- **which our tree
already has**. So our OSFMK is at or past DR2.1u6d. The rest of their
kernel changes are PPC and HP700 specific.

### What is not there

**No i386 `mach_init` program**, source or binary. The only one is
`new_release_kernel/mach_servers/mach_init`, which is PA-RISC, and the
`usr/` tree is a PA-RISC Linux userland. So the current step 4 blocker
is not solved here.

**No `libmach_sa`.** Neither `osfmk/` nor `pmk1.1/` has it; both ship
only the profiled, broken `libmach_sa_p`. This confirms that adding it
was necessary rather than a local workaround, and that the gap is
upstream.

### Also present, not yet examined

`ode/` (the build system), `X11R6.3`, `fdsrc`, `osfmk_2` (exports only),
`Change/` (which contains a DR3 tree).

## Surveyed and rejected: xMach's LITES

`github.com/neozeed/xMach` mirrors the SourceForge xMach project and
carries a LITES tree with changes dated around 2000. It was checked in
case those changes overlapped ours. **They do not, and the reason is
structural rather than incidental.**

xMach is **Mach 4 + LITES**, the Utah/CMU lineage. Ours is OSFMK 7.3,
the OSF lineage. Both start from `Lites.1.1.u3`, and 308 files differ,
but the divergence is adaptation to a different kernel.

The decisive evidence is the pager, the same dividing line identified
earlier in this survey:

| tree | how a memory object is made ready |
|---|---|
| xMach (Mach 4) | `memory_object_establish` **and** `memory_object_ready` |
| MkLinux / ours (OSFMK 7.3) | `memory_object_change_attributes` |

`xmm_interface.c:117` and `:166` still call both routines, and OSFMK 7.3
removed both -- `mach.defs:247` and `:864` keep their message ids as
`skip`. So xMach's pager could not work here, and confirms from a third
tree what MkLinux and OSFMK 6.1 already showed.

None of the modernisation work overlaps either. xMach leaves untouched
every 1990s construct we had to fix:

| construct | xMach |
|---|---|
| `gensym.awk` literal newline in a string | unfixed |
| `case SIG_IGN:` pointer constant as a case label | unfixed |
| `*((char *)to)++`, a cast used as an lvalue | unfixed |
| `default_root[] = "hd0a"` | unchanged |

That is expected: their README says to cross-compile with gcc 2.7.2.3
and binutils 2.12. They never met a modern toolchain, so they never had
these problems.

### Worth remembering from it

Two genuine additions, neither useful now but both interesting later:

- **`server/miscfs/devfs/`**, about 1100 lines -- a device filesystem,
  which LITES 1.1u3 does not have. Relevant if `/dev` ever becomes
  awkward to populate by hand.
- **`emulator/e_linux.c`**, about 1700 lines, plus
  `e_linux_getcwd.c` -- a Linux personality emulator. Interesting far
  down the roadmap, though written against Mach 4.

The conclusion for anyone tempted to revisit this: xMach is a sibling
port, not a newer one. Take design ideas from it if useful, but its
kernel interface assumptions are the wrong ones for this tree.

## Licensing

**Every line of code in this project is written here.** No source is
copied from any other tree, and none of the reference trees is used as
anything but reading material.

### The rule

- **Reading a reference implementation to understand a design** -- fine.
  Copyright protects expression, not method. Learning *that* a floppy
  controller must have its reset interrupt acknowledged before it
  accepts another command is a fact about hardware.
- **Copying its expression** -- not done. Not a function, not a
  structure layout transcribed from someone's header, not a block of
  logic reworded.
- **Invoking a compiler, kernel or library interface** -- not copying at
  all, and worth stating because it can look like it at a glance.

### Compiler intrinsics are not imported code

`include/i386/stdarg.h` now reads:

```c
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start((ap), (last))
#define va_arg(ap, type)   __builtin_va_arg((ap), type)
#define va_end(ap)         __builtin_va_end(ap)
```

`__builtin_va_list` and the `__builtin_va_*` operators are **language
constructs the compiler recognises**, in the same category as `sizeof`,
`__asm__` and `__attribute__`. They expand to nothing textual; the
compiler handles them internally, and on i386 they generate direct stack
arithmetic with no library call at all. Nothing from GCC's own
`stdarg.h` was read or copied -- these four lines were written here from
the documented interface.

This is the standard way any codebase supplying its own headers under
`-nostdinc` declares varargs, and it is what the permissively licensed
BSDs do in the same file.

For completeness on the licence question that does not arise here: GCC
carries the GCC Runtime Library Exception specifically so that compiling
with GCC imposes nothing on the output. That exception is about linking
GCC's runtime, and these builtins link nothing.

### The reference trees, and what each may be used for

| tree | licence | use |
|---|---|---|
| MkLinux `osfmk/` | OSF, same as ours | **not** arm's length -- it is the same code; diffing establishes provenance |
| MkLinux `mklinux/` | OSF | worked example against this exact kernel; read for design |
| OSFMK 6.1 | OSF | ancestor; read to see what changed and why |
| XNU / Darwin | APSL | **read only.** Incompatible. Consult for design, never copy |
| GNU Mach | GPL | **read only.** Incompatible. Consult for design, never copy |
| xMach | Mach 4 lineage | read only; and see the survey above -- its interfaces are the wrong ones |

**Practice:** check a file's header before taking anything from it,
record in the commit message when a reference tree was consulted, and
when implementing something after reading a reference, write it from the
interface documentation rather than with their source open.


Compatible, and cleaner than UX.

- **Core (UC Berkeley lineage):** 4-clause BSD text, but 4.4BSD-Lite
  derived -- the post-settlement clean branch, marked by the "with the
  permission of UNIX System Laboratories" note. UC retroactively
  withdrew the advertising clause in 1999, so for UC-copyrighted files
  it is effectively BSD-3-Clause today.
- **Helander's Mach glue:** a permissive HPND-style grant, same family
  as OSFMK 7.3's own notice, no advertising clause.

Neither is copyleft.

**On UX and the Caldera argument:** the claim that Caldera's 2002 grant
implicitly freed 4.3BSD is not safe to rely on. That grant names UNIX
V1-V7 and 32V rather than derivatives, 4.3BSD contains much more than
32V-derived material, the *USL v. BSDi* settlement is what actually
addressed 4.3BSD and produced 4.4BSD-Lite as the clean branch, and
Caldera's authority was contested afterwards in *SCO v. Novell*. LITES
avoids the question entirely.

## Tried: configure and liblites build against our tree

Not a thought experiment any more. The following was done and works.

### Constructing a MACH_RELEASE_DIR

LITES wants `$(MACH_RELEASE_DIR)/{include,include/mach,lib}` and
`mig`/`migcom`. Our ODE export tree provides all of it:

```sh
MR=/tmp/machrel
mkdir -p $MR/bin $MR/libexec
ln -sfn $MK_BUILD/export/at386/include $MR/include
ln -sfn $MK_BUILD/export/at386/lib     $MR/lib
HB=osfmk7.3/osfmk/tools/i386/i386_linux/hostbin
ln -sf $PWD/$HB/mig    $MR/bin/mig
ln -sf $PWD/$HB/migcom $MR/bin/migcom
ln -sf $PWD/$HB/migcom $MR/libexec/migcom
```

`export/at386/include/mach/` contains the `.defs` files, including
`bootstrap.defs`, so LITES generates its Mach stubs from **our**
definitions with **our** `mig`. That was the central claim of this
survey and it is now demonstrated rather than argued.

### Configure and build

```sh
sh /path/to/lites/configure \
    --with-release=$MR \
    --with-config="STD+WS+osfmach3" \
    --host=i386-unknown-mach3 --target=i386-unknown-mach3

GI=$(gcc -m32 -print-file-name=include)
make CXXX="-m32 -isystem $GI" CHXXX="-m32"
```

`--with-config="STD+WS+osfmach3"` is **essential and not the default**.
Without it `LITES_CONFIG` is `STD+WS`, `OSFMACH3` and `OSF_LEDGERS` stay
undefined, and every device call has the wrong arity:

```
block_io.c:141: error: incompatible type for argument 4 of 'device_open'
block_io.c:132: error: too few arguments to function 'device_open'
```

That is not an incompatibility. LITES already brackets the extra
arguments correctly:

```c
rc = device_open(device_server_port,
#if OSF_LEDGERS
                 MACH_PORT_NULL,     /* ledger */
#endif
                 mode,
#if OSFMACH3
                 security_id,        /* security token */
#endif
```

which matches our `device.defs` exactly -- OSFMK 7.3 replaced
`device_open` with a ledger-and-token form and left the old message id
as `skip; /* nmk15: device_open */`. There are 66 such call sites across
`device_open`, `device_read`, `device_write`, `device_get_status`,
`device_set_status` and `device_close`, and the single config option
fixes all of them.

`CXXX` and `CHXXX` are user hooks in `conf/Makerules` that append to
`TARGET_CFLAGS` and `HOST_CFLAGS`, so the toolchain flags go in without
patching LITES.

### Result

`liblites` **compiles**. The build reaches `server/` and then fails in a
generated file:

```
bsd_types_gen.symc:8:6: error: missing terminating " character
```

`gensym.awk` emits output a modern cpp rejects -- structurally the same
problem OSFMK's own `genassym` had, and the next thing to fix.

### Further: MIG interoperates, and the server tree starts building

With `tools/lites/gensym-newline.patch` applied and
`tools/lites/mig-shim.sh` in place of `$MACH_RELEASE_DIR/bin/mig`:

- `bsd_types_gen.symc` compiles and `bsd_types_gen.h` is generated
- **our `mig` runs LITES's `.defs` against our `mach_types.defs`** and
  produces `bsd_1_server.c` and `bsd_1_server.h`
- `-DOSF_LEDGERS=1 -DUNTYPED_IPC=1` appear on the compile lines, so the
  `osfmach3` arms are live

That is the interoperation this survey set out to test, working at the
tool level: LITES source, our MIG, our definitions, one output.

The build then stops on a LITES packaging inconsistency rather than
anything to do with OSFMK. `conf/files:303` lists
`serv/bsd_server.c`, while the MIG rule derives its output name from
`bsd_1.srv` and so produces `bsd_1_server.c`. The two disagree, and make
passes the unresolved bare name to gcc:

```
cc1: fatal error: bsd_server.c: No such file or directory
```

Untangling that is LITES build-system work and is where the next session
should start.

### Further still: 22 objects, and the first real API difference

Adding `tools/lites/lites-compat.h` via `-include` carried the build
through `device_reply_hdlr.c` and 14 more objects.

That header covers the one genuine API difference found so far.
OSFMK 7.3 uses untyped (NDR) IPC, where the MIG error reply is
`mig_reply_error_t` -- a `Head`, an `NDR_record_t` and a `RetCode`. LITES
uses the typed-IPC name `mig_reply_header_t` in 13 places, which had a
`mach_msg_type_t` where the NDR record now is. It touches the differing
member, `RetCodeType`, in only two places and both are inside its `#else`
arm for typed IPC, which `UNTYPED_IPC` compiles out -- so the two
structures are interchangeable for every use that remains and a plain
typedef suffices.

The build then reaches `server/net/` and stops on a LITES internal
inconsistency: `include/sys/malloc.h:272` defines
`bsd_malloc(size, type, flags)` as `malloc(size)`, because the LITES
server has a one-argument malloc rather than the BSD kernel's
three-argument one, but `net/radix.h`'s KERNEL arm was never converted
and still calls `malloc` and `free` with BSD arity directly.
`tools/lites/radix-bsd-malloc.patch` routes it through the wrapper.

### Further still: ~35 objects, and the shape is now clear

Continuing past `server/net/` turned up three more issues, all the same
kind, and each one unblocked a batch of files rather than a single file.

**BSD malloc arity, 53 sites in 46 files.** LITES's `sys/malloc.h`
supplies `MALLOC`, `FREE`, `bsd_malloc` and `bsd_free`, all resolving to
a one-argument allocator, but the BSD-derived trees under `server/net`,
`server/netccitt` and `server/isofs` were never converted and still call
`malloc(size, type, flags)` and `free(addr, type)` directly. Patching 53
sites would be a large change against LITES; two variadic macros in
`tools/lites/lites-compat.h` drop the extra arguments instead and the
existing calls compile unchanged.

One detail matters there. The macros must expand so that a later
*declaration* of `malloc` is still valid C:

```c
#define malloc(sz, ...)  (malloc)(sz)     /* right */
#define malloc(sz, ...)  (malloc)((unsigned long)(sz))   /* wrong */
```

With the cast, a header declaring `void *malloc(unsigned long);` expands
to `(malloc)((unsigned long)(unsigned long))` and fails. Without it the
declaration becomes `extern void *(malloc)(unsigned long);`, which is
legal. GCC reports such failures at the macro's *definition* site, which
is misleading -- the real error is at whichever header declares the
function.

**`-fno-builtin` is required.** BSD's kernel `log(level, fmt, ...)`
collides with GCC's builtin `log(double)`, giving "too many arguments to
function 'log'". OSFMK's own build uses `-fno-builtin` for the same
reason.

**Pointer constants as case labels.** `kern_sig.c` has `case SIG_DFL:`
where `SIG_DFL` is `(void(*)())0`. K&R C accepted it; modern C requires
an integer constant expression. This is the current stopping point and
needs a LITES patch rather than a shim.

The full flag set that gets this far:

```sh
GI=$(gcc -m32 -print-file-name=include)
make CXXX="-m32 -fno-builtin -isystem $GI \
          -include /path/to/tools/lites/lites-compat.h" \
     CHXXX="-m32"
```

### Reproducible: one script, 159 objects, 5 undefined symbols

```sh
MK_BUILD=~/.cache/mk7.3 ./tools/lites/build-lites.sh ~/lites-1.1.u3 ~/lites-build
```

Verified from pristine clones of both repositories. It applies
`tools/lites/lites-osfmk73.patch`, builds a `MACH_RELEASE_DIR` from the
OSFMK export tree, configures with `osfmach3` and builds.

The flag set, with the reason for each:

| flag | why |
|---|---|
| `-std=gnu89` | GCC 14 makes K&R definitions and implicit int hard errors. GCC 13 did not, so this is easy to miss. |
| `-fno-builtin` | BSD's kernel `log(level, fmt, ...)` vs GCC's builtin `log(double)` |
| `-fgnu89-inline` | `cthreads.h` uses `extern __inline__`, which C99 rules emit per translation unit |
| `-fcommon` | tentative definitions in headers; GCC 10+ defaults to `-fno-common` |
| `-fno-stack-protector` | no `__stack_chk_fail_local` in this environment |
| `-D__NO_UNDERSCORES__` | `i386/asm.h` decorates `ENTRY(htonl)` as `_htonl` unless this is set. Without it every `htonl`/`ntohl` reference is undefined -- 322 of them. |
| `AWK=nawk` | the generators need nawk extensions; configure picks mawk |
| `LIBS` repeated | `libsa_mach` and `libmach` reference each other, and ld reads archives once |

Two more things the script handles:

**`crt0.o` lives inside `libsa_mach.a`.** OSFMK does not ship it
standalone, so `$MACH_RELEASE_DIR/lib` must be a real directory with the
object extracted into it, not a symlink to the export tree.

**The first make pass fails on `bsd_server.c`** and the second succeeds.
Make resolves it through VPATH only once the MIG outputs exist. The
script runs two passes.

### What the patch fixes

`tools/lites/lites-osfmk73.patch`, 8 files. The largest single win was
`vnode_if.sh`: it calls `bail()`, which the script never defines, so
both mawk and nawk abort at parse time and emit a 97 line stub instead
of the full 727 line `vnode_if.c`. That alone accounted for about 700 of
the undefined symbols. Defining `bail` fixes it.

The rest: `gensym.awk` and `newvers.sh` emitting literal newlines inside
string literals, pointer constants as `case` labels in `kern_sig.c` and
`serv_syscalls.c`, a cast used as an lvalue in `user_copy.c`, and two
Mach structure members that moved on in `vn_pager_misc.c` and
`xmm_interface.c`.

### Researched: the gap is NORMA/XMM, and it is one function wide

Comparing against OSFMK 6.1, XNU Rhapsody DR5.3 and the 7.3 tree itself
identifies what LITES's OSFMACH3 pager arm was written for, and it is
not a generic "older OSF Mach".

**`memory_object_establish` is a NORMA routine.** In OSFMK 6.1 it lives
in `norma/xmm_user.c`, is renamed to `k_memory_object_establish` by
`norma/xmm_server_rename.h`, and its body is:

```c
panic("memory_object_establish is not implemented\n");
```

It was part of NORMA, Mach's multicomputer/distributed memory layer, and
was **already unimplemented in 6.1**. The `memory_object.defs` comments
describe the protocol it belonged to: a discard request is answered with
either `memory_object_establish` or a discard. That is also where
`seqnos_memory_object_discard_request` comes from.

**OSFMK 7.3 removed NORMA entirely.** There is no `norma/` directory;
the mentions in `conf/files` are historical log entries. `mach.defs:247`
keeps the message id reserved as
`skip; /* was memory_object_establish; old port_set_backlog */`.

**XNU Rhapsody does not have it either**, which is consistent: the
lineage that became XNU dropped NORMA at the same point.

So LITES's file name is the clue that was there all along --
`xmm_interface.c`. Its OSFMACH3 arm targets a NORMA-enabled OSF Mach,
and the name says so.

#### MkLinux confirms the fix, and supplies the idiom

`github.com/slp/osfmk-mklinux` settles it, and more strongly than a
comparison would: **our OSFMK 7.3 is a copy of MkLinux's**. Diffing
`osfmk/src/mach_kernel` between the two trees gives exactly eleven
differing files, and they are exactly our eleven fixes:

```
i386/pio.h              i386/locore.S           i386/i386_rpc.c
i386/hardclock.c        i386/AT386/model_dep.c  i386/AT386/lpr.c
i386/AT386/fd.c         intel/pmap.c            kern/bootstrap.c
kern/ipc_kobject.c      kern/startup.c
```

Nothing else differs. So MkLinux's pager is not an analogous
implementation on a similar kernel -- it is an implementation against
*this* kernel, and its `memory_object` interface is byte-for-byte the
one we export.

Its OSFMK is therefore the **same generation as ours**: no `norma/` directory, and `mach.defs:247` reads
the identical `skip; /* was memory_object_establish; old
port_set_backlog */`. So MkLinux ran a real personality on an OSFMK with
NORMA already removed, which is exactly our situation, and its pager is
the canonical example.

`mklinux/src/osfmach3/server/inode_pager.c:905`, `inode_object_init`,
ends with:

```c
/*
 * Tell the micro-kernel that the memory object is ready on our side.
 */
attributes.copy_strategy    = imo->imo_copy_strategy;
attributes.cluster_size     = PAGE_SIZE;     /* or 0 for the default */
attributes.may_cache_object = imo->imo_cacheable;
attributes.temporary        = FALSE;
kr = memory_object_change_attributes(mem_obj_control,
                                     MEMORY_OBJECT_ATTRIBUTE_INFO,
                                     (memory_object_info_t) &attributes,
                                     MEMORY_OBJECT_ATTR_INFO_COUNT,
                                     MACH_PORT_NULL);
```

Its own comment -- "tell the micro-kernel that the memory object is
ready on our side" -- is precisely what `object_ready = TRUE` meant in
the NORMA establish call. The semantic did not disappear; it moved into
`change_attributes`, and the field vanished because being ready is now
implied by making the call.

This also confirms the second half. MkLinux's
`inode_object_discard_request` at line 895 is a one-line `panic()`. A
stub is the correct implementation, because this generation of OSFMK
never initiates the discard protocol.

One difference worth noting: MkLinux uses the **plain**
`memory_object_server`, not the sequence-numbered one -- zero `seqnos_`
references in its whole server. LITES chose the seqnos variant, and our
`libmach` does provide `Smem_svr`, so that choice remains workable. But
if the seqnos path gives trouble later, the plain interface is the
better-trodden one for this kernel.

Cross-checked against OSFMK 6.1 (`github.com/nmartin0/osfmk6.1`), whose
`norma/xmm_user.c:410` shows the NORMA layer doing the same thing by
either `K_SET_READY(mobj, OBJECT_READY_TRUE, MAY_CACHE_FALSE, modwc,
MEMORY_OBJECT_COPY_SYMMETRIC, PAGE_SIZE, ...)` or a plain
`memory_object_init`. Same four attributes, same intent, three
different spellings across three kernel generations.

#### The practical consequence: one function

Mapping the conditionals in `xmm_interface.c` shows `#if OSFMACH3` wraps
only the **initialisation** path:

| handler | line | arm |
|---|---|---|
| `seqnos_memory_object_init` | 136 | `#else` of `#if OSFMACH3` |
| `data_request` | 236 | top level |
| `data_unlock` | 336 | top level |
| `lock_completed` | 499 | top level |
| `data_return` | 557 | top level |
| `change_completed` | 572 | top level |
| `terminate`, `copy` | 176, 224 | top level |

Everything except initialisation is shared. The OSFMACH3 arm calls
`memory_object_establish` where the other defines
`seqnos_memory_object_init`, and that single substitution is the whole
incompatibility.

So the fix is not "write a pager". It is:

1. Provide `seqnos_memory_object_init` for the OSFMACH3 arm, doing what
   the establish call was meant to do, against 7.3's interface --
   `memory_object_change_attributes` with a
   `memory_object_attr_info` is the closest equivalent, and
   `vn_pager_misc.c` already calls it.
2. Provide `seqnos_memory_object_discard_request`, which can be a stub
   returning failure: it is the NORMA discard protocol, which 7.3 never
   initiates. `Smem_svr` references it only because the `.defs` still
   reserves the message.

Both belong in the OSFMACH3 arm of `xmm_interface.c`, which keeps the
change inside LITES and inside the patch series already carried here.

### Superseded framing: the real incompatibility

The three non-libgcc symbols are one problem, and it is the first
substantive mismatch found in this whole effort -- not a toolchain
issue, an actual interface divergence.

`memory_object_establish` does not exist in OSFMK 7.3.
`mach/mach.defs:247` reads:

```
skip;	/* was memory_object_establish; old port_set_backlog */
```

It was removed. LITES's `xmm_interface.c` calls it from its
`#if OSFMACH3` arm, so that arm targets an OSF Mach from before the
removal.

The two `seqnos_` handlers are the same divergence seen from the other
side. `Smem_svr.o` inside our `libmach.a` is the MIG **server** for the
sequence-numbered memory object interface: it provides
`seqnos_memory_object_server` and expects the pager to implement seven
handlers. LITES implements five of them in its OSFMACH3 arm. Of the
other two, `seqnos_memory_object_init` **is** defined in
`xmm_interface.c`, but at line 136, inside the `#else /* OSFMACH3 */`
arm -- so enabling `osfmach3`, which is required for the device call
arity, compiles it out. `seqnos_memory_object_discard_request` is not
defined anywhere in LITES.

So LITES has two pager implementations, and neither matches 7.3: the
OSFMACH3 one calls a routine 7.3 deleted, and the other one is written
against the older typed interface.

This is unsurprising in hindsight. The external pager interface is the
part of Mach that changed most between versions, and it is exactly where
a personality built for one OSF Mach would diverge from another.

Resolving it means writing the missing handlers against 7.3's actual
`memory_object` interface, using the 21 `memory_object_*` routines
`libmach` does export -- among them
`memory_object_change_attributes`, which is the closest thing 7.3 has to
what `memory_object_establish` did. That is real porting work rather
than a shim, and it is the first task in this effort that is.

### Done: every OSFMK-side symbol resolves

`tools/lites/lites-osfmk73.patch` now carries the pager work, and the
link is down to `__divdi3` and `__moddi3` alone -- libgcc helpers that
are absent only where no 32-bit libgcc is installed. Every symbol that
was ours is resolved.

Three changes in `xmm_interface.c` did it.

**`seqnos_memory_object_init` for the OSFMACH3 arm**, following
MkLinux's `inode_object_init` exactly: fill a
`memory_object_attr_info_data_t` with `copy_strategy`, `cluster_size`,
`may_cache_object` and `temporary`, then call
`memory_object_change_attributes` with `MEMORY_OBJECT_ATTRIBUTE_INFO`.
The rest of the body -- vnode lookup, pager wiring, `ux_server_add_port`
-- is identical to the `#else` arm's version.

Worth recording why neither existing arm worked: the OSFMACH3 arm calls
`memory_object_establish`, removed as a NORMA routine, and the `#else`
arm calls `memory_object_ready`, which `mach.defs:864` shows was also
removed ("was skip; memory_object_ready"). **Both** of LITES's pager
initialisation paths target routines 7.3 deleted, and both were folded
into `change_attributes`. That is why MkLinux is the only usable
template rather than one of two options.

**`seqnos_memory_object_discard_request`** as a panic stub, matching
MkLinux's `inode_object_discard_request`.

**`seqnos_memory_object_notify`'s establish call** replaced by a panic.
That handler belongs to the NORMA notify protocol; `Smem_svr` does not
reference `seqnos_memory_object_notify` at all, so it is unreachable on
this kernel. The attribute setting it used to carry now happens in
`init`.

### Superseded: the 5 that remain

```
__divdi3, __moddi3                      libgcc helpers
memory_object_establish                 in mach.defs, not in any library
seqnos_memory_object_discard_request    handlers for the MIG pager server
seqnos_memory_object_init
```

The first two are absent only where no 32-bit libgcc is installed; on a
host with working multilib they resolve. The other three are OSFMK-side:
`memory_object_establish` is declared in our `mach/mach.defs` but is not
compiled into `libmach` or `libsa_mach`, and the two `seqnos_` handlers
are wanted by a generated MIG server inside our own libraries. Which
`.defs` are compiled into which library, and whether the export tree is
missing one, is the next question -- and the first in this whole effort
that is ours rather than LITES's.

## Surveyed and rejected: xMach's LITES

`github.com/neozeed/xMach` mirrors the SourceForge xMach project and
carries a LITES tree with changes dated around 2000. It was checked in
case those changes overlapped ours. **They do not, and the reason is
structural rather than incidental.**

xMach is **Mach 4 + LITES**, the Utah/CMU lineage. Ours is OSFMK 7.3,
the OSF lineage. Both start from `Lites.1.1.u3`, and 308 files differ,
but the divergence is adaptation to a different kernel.

The decisive evidence is the pager, the same dividing line identified
earlier in this survey:

| tree | how a memory object is made ready |
|---|---|
| xMach (Mach 4) | `memory_object_establish` **and** `memory_object_ready` |
| MkLinux / ours (OSFMK 7.3) | `memory_object_change_attributes` |

`xmm_interface.c:117` and `:166` still call both routines, and OSFMK 7.3
removed both -- `mach.defs:247` and `:864` keep their message ids as
`skip`. So xMach's pager could not work here, and confirms from a third
tree what MkLinux and OSFMK 6.1 already showed.

None of the modernisation work overlaps either. xMach leaves untouched
every 1990s construct we had to fix:

| construct | xMach |
|---|---|
| `gensym.awk` literal newline in a string | unfixed |
| `case SIG_IGN:` pointer constant as a case label | unfixed |
| `*((char *)to)++`, a cast used as an lvalue | unfixed |
| `default_root[] = "hd0a"` | unchanged |

That is expected: their README says to cross-compile with gcc 2.7.2.3
and binutils 2.12. They never met a modern toolchain, so they never had
these problems.

### Worth remembering from it

Two genuine additions, neither useful now but both interesting later:

- **`server/miscfs/devfs/`**, about 1100 lines -- a device filesystem,
  which LITES 1.1u3 does not have. Relevant if `/dev` ever becomes
  awkward to populate by hand.
- **`emulator/e_linux.c`**, about 1700 lines, plus
  `e_linux_getcwd.c` -- a Linux personality emulator. Interesting far
  down the roadmap, though written against Mach 4.

The conclusion for anyone tempted to revisit this: xMach is a sibling
port, not a newer one. Take design ideas from it if useful, but its
kernel interface assumptions are the wrong ones for this tree.

## Licensing

Compatible, and cleaner than UX.

- **Core (UC Berkeley lineage):** 4-clause BSD text, but 4.4BSD-Lite
  derived -- the post-settlement clean branch, marked by the "with the
  permission of UNIX System Laboratories" note. UC retroactively
  withdrew the advertising clause in 1999, so for UC-copyrighted files
  it is effectively BSD-3-Clause today.
- **Helander's Mach glue:** a permissive HPND-style grant, same family
  as OSFMK 7.3's own notice, no advertising clause.

Neither is copyleft.

**On UX and the Caldera argument:** the claim that Caldera's 2002 grant
implicitly freed 4.3BSD is not safe to rely on. That grant names UNIX
V1-V7 and 32V rather than derivatives, 4.3BSD contains much more than
32V-derived material, the *USL v. BSDi* settlement is what actually
addressed 4.3BSD and produced 4.4BSD-Lite as the clean branch, and
Caldera's authority was contested afterwards in *SCO v. Novell*. LITES
avoids the question entirely.

## Tried: configure and liblites build against our tree

Not a thought experiment any more. The following was done and works.

### Constructing a MACH_RELEASE_DIR

LITES wants `$(MACH_RELEASE_DIR)/{include,include/mach,lib}` and
`mig`/`migcom`. Our ODE export tree provides all of it:

```sh
MR=/tmp/machrel
mkdir -p $MR/bin $MR/libexec
ln -sfn $MK_BUILD/export/at386/include $MR/include
ln -sfn $MK_BUILD/export/at386/lib     $MR/lib
HB=osfmk7.3/osfmk/tools/i386/i386_linux/hostbin
ln -sf $PWD/$HB/mig    $MR/bin/mig
ln -sf $PWD/$HB/migcom $MR/bin/migcom
ln -sf $PWD/$HB/migcom $MR/libexec/migcom
```

`export/at386/include/mach/` contains the `.defs` files, including
`bootstrap.defs`, so LITES generates its Mach stubs from **our**
definitions with **our** `mig`. That was the central claim of this
survey and it is now demonstrated rather than argued.

### Configure and build

```sh
sh /path/to/lites/configure \
    --with-release=$MR \
    --with-config="STD+WS+osfmach3" \
    --host=i386-unknown-mach3 --target=i386-unknown-mach3

GI=$(gcc -m32 -print-file-name=include)
make CXXX="-m32 -isystem $GI" CHXXX="-m32"
```

`--with-config="STD+WS+osfmach3"` is **essential and not the default**.
Without it `LITES_CONFIG` is `STD+WS`, `OSFMACH3` and `OSF_LEDGERS` stay
undefined, and every device call has the wrong arity:

```
block_io.c:141: error: incompatible type for argument 4 of 'device_open'
block_io.c:132: error: too few arguments to function 'device_open'
```

That is not an incompatibility. LITES already brackets the extra
arguments correctly:

```c
rc = device_open(device_server_port,
#if OSF_LEDGERS
                 MACH_PORT_NULL,     /* ledger */
#endif
                 mode,
#if OSFMACH3
                 security_id,        /* security token */
#endif
```

which matches our `device.defs` exactly -- OSFMK 7.3 replaced
`device_open` with a ledger-and-token form and left the old message id
as `skip; /* nmk15: device_open */`. There are 66 such call sites across
`device_open`, `device_read`, `device_write`, `device_get_status`,
`device_set_status` and `device_close`, and the single config option
fixes all of them.

`CXXX` and `CHXXX` are user hooks in `conf/Makerules` that append to
`TARGET_CFLAGS` and `HOST_CFLAGS`, so the toolchain flags go in without
patching LITES.

### Result

`liblites` **compiles**. The build reaches `server/` and then fails in a
generated file:

```
bsd_types_gen.symc:8:6: error: missing terminating " character
```

`gensym.awk` emits output a modern cpp rejects -- structurally the same
problem OSFMK's own `genassym` had, and the next thing to fix.

### Further: MIG interoperates, and the server tree starts building

With `tools/lites/gensym-newline.patch` applied and
`tools/lites/mig-shim.sh` in place of `$MACH_RELEASE_DIR/bin/mig`:

- `bsd_types_gen.symc` compiles and `bsd_types_gen.h` is generated
- **our `mig` runs LITES's `.defs` against our `mach_types.defs`** and
  produces `bsd_1_server.c` and `bsd_1_server.h`
- `-DOSF_LEDGERS=1 -DUNTYPED_IPC=1` appear on the compile lines, so the
  `osfmach3` arms are live

That is the interoperation this survey set out to test, working at the
tool level: LITES source, our MIG, our definitions, one output.

The build then stops on a LITES packaging inconsistency rather than
anything to do with OSFMK. `conf/files:303` lists
`serv/bsd_server.c`, while the MIG rule derives its output name from
`bsd_1.srv` and so produces `bsd_1_server.c`. The two disagree, and make
passes the unresolved bare name to gcc:

```
cc1: fatal error: bsd_server.c: No such file or directory
```

Untangling that is LITES build-system work and is where the next session
should start.

### Further still: 22 objects, and the first real API difference

Adding `tools/lites/lites-compat.h` via `-include` carried the build
through `device_reply_hdlr.c` and 14 more objects.

That header covers the one genuine API difference found so far.
OSFMK 7.3 uses untyped (NDR) IPC, where the MIG error reply is
`mig_reply_error_t` -- a `Head`, an `NDR_record_t` and a `RetCode`. LITES
uses the typed-IPC name `mig_reply_header_t` in 13 places, which had a
`mach_msg_type_t` where the NDR record now is. It touches the differing
member, `RetCodeType`, in only two places and both are inside its `#else`
arm for typed IPC, which `UNTYPED_IPC` compiles out -- so the two
structures are interchangeable for every use that remains and a plain
typedef suffices.

The build then reaches `server/net/` and stops on a LITES internal
inconsistency: `include/sys/malloc.h:272` defines
`bsd_malloc(size, type, flags)` as `malloc(size)`, because the LITES
server has a one-argument malloc rather than the BSD kernel's
three-argument one, but `net/radix.h`'s KERNEL arm was never converted
and still calls `malloc` and `free` with BSD arity directly.
`tools/lites/radix-bsd-malloc.patch` routes it through the wrapper.

### Further still: ~35 objects, and the shape is now clear

Continuing past `server/net/` turned up three more issues, all the same
kind, and each one unblocked a batch of files rather than a single file.

**BSD malloc arity, 53 sites in 46 files.** LITES's `sys/malloc.h`
supplies `MALLOC`, `FREE`, `bsd_malloc` and `bsd_free`, all resolving to
a one-argument allocator, but the BSD-derived trees under `server/net`,
`server/netccitt` and `server/isofs` were never converted and still call
`malloc(size, type, flags)` and `free(addr, type)` directly. Patching 53
sites would be a large change against LITES; two variadic macros in
`tools/lites/lites-compat.h` drop the extra arguments instead and the
existing calls compile unchanged.

One detail matters there. The macros must expand so that a later
*declaration* of `malloc` is still valid C:

```c
#define malloc(sz, ...)  (malloc)(sz)     /* right */
#define malloc(sz, ...)  (malloc)((unsigned long)(sz))   /* wrong */
```

With the cast, a header declaring `void *malloc(unsigned long);` expands
to `(malloc)((unsigned long)(unsigned long))` and fails. Without it the
declaration becomes `extern void *(malloc)(unsigned long);`, which is
legal. GCC reports such failures at the macro's *definition* site, which
is misleading -- the real error is at whichever header declares the
function.

**`-fno-builtin` is required.** BSD's kernel `log(level, fmt, ...)`
collides with GCC's builtin `log(double)`, giving "too many arguments to
function 'log'". OSFMK's own build uses `-fno-builtin` for the same
reason.

**Pointer constants as case labels.** `kern_sig.c` has `case SIG_DFL:`
where `SIG_DFL` is `(void(*)())0`. K&R C accepted it; modern C requires
an integer constant expression. This is the current stopping point and
needs a LITES patch rather than a shim.

The full flag set that gets this far:

```sh
GI=$(gcc -m32 -print-file-name=include)
make CXXX="-m32 -fno-builtin -isystem $GI \
          -include /path/to/tools/lites/lites-compat.h" \
     CHXXX="-m32"
```

### The whole LITES server now compiles

Every object builds and the link is reached:

```
startup.Lites.1.1.u3.STD+WS+osfmach3.unstripped
```

`tools/lites/build-lites.sh` reproduces it end to end.

Getting from ~35 objects to the link needed six more fixes, all the same
kind:

| issue | fix |
|---|---|
| `case SIG_DFL:` etc -- pointer constants as case labels, 11 in 2 files | cast the labels: `case (integer_t)SIG_DFL:` |
| `*((char *)to)++` -- a cast is not an lvalue, 1 site | spell the post-increment out |
| `cthread_mach_msg` declared differently by us and LITES | see below |
| `memory_object_behave_info.write_completions` gone | set `silent_overwrite` and `advisory_pageout` instead |
| `memory_object_attr_info.may_cache` / `.object_ready` | renamed to `may_cache_object`; `object_ready` has no counterpart |
| assembly built 64-bit | `ASFLAGS=-m32`, a separate hook from `CXXX` |
| generated `vers.c` had literal newlines in string literals | see below |

**`cthread_mach_msg`.** LITES supplies its own in `server/serv/cprocs.c`
with the nine-argument signature old cthreads had; its own comment says
"These are missing from cthreads". OSFMK 7.3's libcthreads has one too,
but collapsed into a single struct whose members map one to one onto
those nine arguments. Nothing calls ours, so they collide only as
declarations. `lites-compat.h` pulls `cthreads.h` in early with our name
renamed away; the include guard makes every later include a no-op, so
LITES's declaration and definition stand unopposed.

**`vers.c`.** `conf/newvers.sh` emits `\\n` expecting a backslash-n to
reach the C source, but `/bin/sh` on a modern Debian is dash, whose
`echo` interprets backslash escapes, so a real newline landed inside a
string literal. Changing those `echo` calls to `printf '%s\n'` fixes it.
This is a second instance of the same 1990s assumption as `gensym.awk`,
by a different mechanism -- there the C source was wrong, here the shell
was.

### 150 objects, and the duplicate symbols are gone

Two more flags clear every multiple-definition error:

| flag | why |
|---|---|
| `-fgnu89-inline` | `cthreads.h` declares `cthread_sp`, `spin_unlock` and `spin_try_lock` `extern __inline__`. Under C99 rules that emits a symbol in every translation unit; gnu89 semantics are what the header was written for. |
| `-fcommon` | `bufqueues`, `invalhash`, `bufhashtbl` and friends are tentative definitions in headers. GCC 10 and later default to `-fno-common`, so each object gets its own. |

**Rebuild from clean when changing these.** Stale objects compiled
without the flag keep their duplicate symbols and the link still fails,
which looks exactly like the flag not working.

Library naming is handled without touching LITES by making
`$MACH_RELEASE_DIR/lib` a real directory of symlinks and adding the two
aliases LITES asks for:

```sh
ln -sf libcthreads.a libthreads.a
ln -sf libsa_mach.a  libmach_sa.a
```

### The link runs; 1055 undefined symbols remain

The link now executes over all 150 objects. Getting there needed:

| issue | fix |
|---|---|
| `CRT0` unset, resolving to the literal `crt0-not-found` | `ar x libsa_mach.a crt0.o` into `$MACH_RELEASE_DIR/lib`; OSFMK keeps crt0 inside the archive rather than standalone |
| `ld: unrecognised emulation mode: 32` | the link rule calls `ld` directly, not `gcc`, so it is `LDFLAGS="-m elf_i386"` and not `-m32` |
| `liblites.a` built 64-bit | rebuild it from clean after adding `-m32`; the archive predated the flag |
| `-lmach` missing from `LIBS` | LITES's non-OSF arm omits it. Overriding `LIBS` on the make line took undefined symbols from 2089 to 1055 |
| `__stack_chk_fail_local` | `-fno-stack-protector` |

What is left divides in two.

**A 32-bit libgcc this host does not have.** `__divdi3` and `__moddi3`
are libgcc helpers, and `gcc -m32 -print-libgcc-file-name` returns the
x86_64 path because no multilib libgcc is installed. A machine with
`gcc-multilib` properly set up should resolve these.

**Mach RPCs our libraries do not export**, such as `clock_sleep` and
`host_get_clock_service`. These are generated stubs, so the question is
which `.defs` are compiled into which OSFMK library and whether the
export tree is missing one. That is OSFMK-side work and the first task
in this effort that is.

### Superseded: the link step

```
ld: cannot find crt0-not-found
ld: cannot find -lthreads
ld: cannot find -lmach_sa
```

Both are the link step rather than compilation. `CXXX` feeds
`TARGET_CFLAGS`, which the link rule does not use, so the link runs
64-bit and silently passes over our 32-bit archives -- the aliases exist
and `-L$MACH_RELEASE_DIR/lib` is on the command line, so "cannot find"
here means "found nothing of the right architecture". The link needs its
own `-m32`, and `CRT0` is unset, resolving to the literal
`crt0-not-found`.

### Superseded: two link errors

```
ld: cannot find -lthreads
ld: cannot find -lmach_sa
multiple definition of `cthread_sp'
```

The first two are naming: ours are `libcthreads.a` and `libsa_mach.a`.
The third is that `cthreads.h` declares `cthread_sp` and `spin_try_lock`
`extern __inline__`, which under modern GCC's C99 inline rules emits a
symbol in every translation unit that includes it; `-fgnu89-inline` or a
`static` qualifier is the usual remedy. Both are small and neither
touches OSFMK.

### Shims kept in this tree

Both are ours, so nothing in LITES or OSFMK is modified:

| file | purpose |
|---|---|
| `tools/lites/mig-shim.sh` | LITES invokes `mig -cc <cmd>`; OSF's `mig` spells it `-cpp`, and silently treats `-cc` as a cpp flag so the command name becomes a filename. The shim translates and passes everything else through. |
| `tools/lites/gensym-newline.patch` | a one-line change to LITES's `conf/gensym.awk`, carried as a patch rather than a fork |
| `tools/lites/lites-compat.h` | injected with `-include`; typedefs `mig_reply_header_t` to `mig_reply_error_t` for untyped IPC |
| `tools/lites/radix-bsd-malloc.patch` | routes `net/radix.h` through LITES's own `bsd_malloc` wrapper |

## Build issues found so far

All are 1990s-toolchain modernisation, none are interface problems:

| issue | status |
|---|---|
| `conf/files:347` `# Linux file systems` rejected as an invalid cpp directive | open; BSD `config(8)` files use `#` comments but run through cpp |
| `-nostdinc` without GCC's own include path, so `stdarg.h` is missing | solved with `-isystem $(gcc -m32 -print-file-name=include)` |
| builds 64-bit by default, so `movl %%esp, %0` fails to assemble | solved with `-m32` via `CXXX`/`CHXXX` |
| device call arity | solved by `--with-config=...+osfmach3` |
| `gensym.awk` output rejected by modern cpp | solved by `tools/lites/gensym-newline.patch` |
| `mig -cc` vs OSF's `-cpp` | solved by `tools/lites/mig-shim.sh` |
| `conf/files` names `bsd_server.c` vs MIG's `bsd_1_server.c` | transient; VPATH resolves it once the MIG outputs exist |
| `mig_reply_header_t` absent under untyped IPC | solved by `tools/lites/lites-compat.h` |
| `net/radix.h` calls `malloc` with BSD arity | superseded by the variadic macros below |
| 53 raw BSD-arity `malloc`/`free` calls in 46 files | solved by variadic macros in `tools/lites/lites-compat.h` |
| BSD kernel `log()` vs GCC's builtin | solved by `-fno-builtin` |
| `case SIG_DFL:` -- pointer constant as a case label | solved by casting the labels |
| library names and `extern __inline__` duplicate symbols | **open**; two link errors, see above |
| `-I-` deprecated, `#endif KERNEL` extra tokens | warnings only |

## Known work before it can be tried

- **autoconf 2.3** (1994). Will need the same treatment ODE did: modern
  `gcc` rejecting K&R constructs, `install` detection, a `config.guess`
  predating x86-64. `conf/config.guess` already emits
  `i386-unknown-mach3`, so the target triple exists.
- **`osfmach3.h` does not ship** -- it is generated by BSD `config(8)`
  from the `options` line in `conf/MASTER`. Not a problem, just not
  obvious.
- **`MACH_RELEASE_DIR` layout.** LITES expects an installed Mach release
  tree with `bin/mig`, headers and libs. Ours is an ODE export tree with
  a different shape. Plumbing, not a blocker.
- **Semantics, not interfaces.** LITES may call RPCs this kernel
  implements as stubs. The 88 `#if OSFMACH3` sites show it ran on *an*
  OSF Mach, not necessarily 7.3.

## Dependency

LITES uses Mach device RPCs in `server/serv/cons.c`, `tty_io.c` and
`tape_io.c`, so it needs the same device path the bootstrap task uses.
That path now works -- the bootstrap task opens `console` and prints
successfully -- but there is still no readable block device. See
`docs/current-blocker.md`.
