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

### Remaining: two link errors

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
