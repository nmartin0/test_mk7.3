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

## Build issues found so far

All are 1990s-toolchain modernisation, none are interface problems:

| issue | status |
|---|---|
| `conf/files:347` `# Linux file systems` rejected as an invalid cpp directive | open; BSD `config(8)` files use `#` comments but run through cpp |
| `-nostdinc` without GCC's own include path, so `stdarg.h` is missing | solved with `-isystem $(gcc -m32 -print-file-name=include)` |
| builds 64-bit by default, so `movl %%esp, %0` fails to assemble | solved with `-m32` via `CXXX`/`CHXXX` |
| device call arity | solved by `--with-config=...+osfmach3` |
| `gensym.awk` output rejected by modern cpp | **open, current blocker** |
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
