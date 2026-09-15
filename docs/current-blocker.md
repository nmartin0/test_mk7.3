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

# WORKING: the bootstrap task reads its config off a minix floppy

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
