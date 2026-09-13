# The bootstrap fork: Hurd or OSF multiserver

**Status: open. Not yet decided, and not yet reachable.**

Recorded while the evidence was fresh. Nothing here needs acting on
until the kernel gets far enough to call either function — see
"Why this is not urgent" below.

---

## What the tree actually does today

`kern/startup.c:517` calls `bootstrap_create()`. That function, at
`kern/bootstrap.c:1145`, is hardcoded to start **two GNU Hurd servers**:

```c
boot_script_parse_line (boot_start, boot_size,
    "ext2fs.static --multiboot-command-line=root=/dev/hd2s2 "
    "--host-priv-port=${host-port} --device-master-port=${device-port} "
    "--exec-server-task=${exec-task} -T typed device:hd2s2 "
    "$(task-create) $(task-resume)");

boot_script_parse_line (exec_start, exec_size,
    "exec.static $(exec-task=task-create)");
```

So as published, this tree expects:

| multiboot module | expected contents |
|---|---|
| `mods[0]` | `ext2fs.static` — Hurd ext2 filesystem translator |
| `mods[1]` | `exec.static` — Hurd exec server |

`kern/boot_script.c`, which parses those lines, is GNU Mach's
boot-script machinery. Someone grafted Hurd's bootstrap protocol onto
OSFMK before this tree was published.

This also explains something that first looked like a bug:
`parse_multiboot()` reads `mb_module[0]` and `mb_module[1]`
unconditionally. That is not a defect in OSF's design — it is a
hardcoded two-server Hurd boot.

## The original OSF path is still present, but disabled

`bootstrap_create_old()` at `kern/bootstrap.c:1257` is the classic Mach
path: allocate a bootstrap port, create a task and thread, set
`TASK_BOOTSTRAP_PORT`, and start the thread at `user_bootstrap`, which
loads the boot image into the new task.

It is wrapped in `#if 0` (lines 1255–1303) and nothing calls it.

**It has not rotted.** Measured, by flipping the guard and building:

| check | result |
|---|---|
| Compiles under GCC 13/14 | yes, clean |
| Links — every symbol it calls still exists | yes |
| Warnings | one, `no previous prototype`, cosmetic |
| Guarded against a missing module | yes: `if (boot_size == 0) { printf("Not starting bootstrap task.\n"); return; }` |

`SYS_REBOOT_COMPAT` is **not** defined in this configuration, so its
call to `do_bootstrap_compat()` is compiled out.

We have also already built the binary it wants:
`src/bootstrap/` produces `obj/at386/bootstrap/bootstrap`, a 220,656
byte `ELF 32-bit LSB executable, Intel 80386`. Supplying it with
`-initrd` sets `boot_start` and `boot_size` correctly — verified,
`boot_size` reads back as exactly 220656.

## The options

**A — boot Hurd.** Obtain `ext2fs.static` and `exec.static` from a Hurd
build and pass both as modules. Matches the code as written; needs no
source change. But it makes this a Hurd kernel and pulls a GPL userland
into a project whose stated goal is a permissively-licensed system.

**B — revive `bootstrap_create_old()`.** Change `startup.c:517` to call
it, and drop the `#if 0`. Two lines. Uses OSF's own bootstrap task,
which we can already build, and keeps the whole system permissive. This
is the direction the project originally set out in: OSFMK plus a
permissive multiserver personality.

**C — neither yet.** Chosen. See below.

## Why this is not urgent

**Neither function is reached.** A `-d exec` trace shows the kernel
enters only 55 distinct functions and stops in the scheduler's first
context switch:

```
setup_main
  printf_init
  panic_init
  sched_init
  pset_sys_bootstrap
    pset_init
  Switch_context      <- and never returns
```

`bootstrap_create_old`, `user_bootstrap`, `task_create_local`,
`thread_start` and `thread_resume` never execute — confirmed by
checking the set of functions entered, not by inference.

So whichever path is eventually chosen, the blocker in front of both is
the same: the first context switch does not return. That has to be
fixed first, and fixing it may change what we know about either path.

## Notes for whoever decides

- The object format question is settled: **ELF throughout** for
  AT386-on-Linux. The kernel links `-e pstart` (the ELF arm of
  `conf/AT386/template.mk`); MACHO and A_OUT link `-e _pstart` with the
  a.out underscore. `kern/bootstrap.c` is ELF-only (`Elf32_Ehdr`). The
  multi-format loaders under `src/bootstrap/` — `a_out.c`, `coff.c`,
  `elf.c`, `rose.c`, `som.c` — belong to the *user-space* bootstrap
  task for loading servers, and do not contradict this.
- If B is chosen, note that `parse_multiboot()` reads two modules
  unconditionally and does not consult `mods_count`. With one module
  supplied, `exec_size` reads back as garbage (`-1094452224` observed).
  With zero modules it reads the kernel command line as a module table
  and produces `boot_start = 0x6863616d`, which is ASCII "mach". Under
  B only `mods[0]` is meaningful, so those reads want guarding — but
  only once something actually depends on them.
