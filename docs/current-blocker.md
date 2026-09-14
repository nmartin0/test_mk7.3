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

## The live blocker: the task's read RPC

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
