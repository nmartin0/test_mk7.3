# The restored system, a checkpoint

The `restored` branch holds this tree at the point where the system
OSF and Utah assembled runs end to end and builds itself (the
roadmap's intermediate goal, milestones 0 to 5), with the fixes that
followed it: the multiprocessor kernel on two CPUs, and the IDE
driver's and disk tool's last two boot messages (S-6, S-15). It is
cut once, from `dev3` at the commit that last changes this file, 0102,
and is not committed to again: work goes on in `dev3`, and anything built later can be
compared with, or rebuilt from, this point.

## What it is

- **OSFMK 7.3**, MkLinux DR3's kernel for i386, PRODUCTION, loaded by
  OSF's own primary boot and `mach_boot` from a vtoc-labelled IDE disk,
  or by multiboot as a second way in; FAST, with the debugger, and
  FAST+MP, which runs on two CPUs.
- **OSF's bootstrap task**, reading `bootstrap.conf` from UFS, starting
  **`name_server`**, **`default_pager`** on partition b and
  **Lites 1.1.u3**, Utah's release, as `startup`.
- **NetBSD 1.0's userland** under Lites, multi-user, on the network
  through the kernel's `ne` driver: telnet and ftp both ways, the
  resolver.
- **Self-hosting**: on the running system, ODE, binutils 2.7 and gcc
  2.7.2.1 for OSF/1 i386 ELF build every component, the boot included,
  and the system so built builds itself again identically but for its
  build stamps.

Every change to a vendor file is recorded in
`docs/provenance/imports.md`, with its kind and reason, and most with
an AI-ONLY NOTE at the site; the decisions on others' commits are in
`docs/provenance/verdicts.md`; what is open is in `docs/deferred.md`.

## Measured at the checkpoint, 17 and 18 October 2026

- **From a clean clone, on the Linux host** (Ubuntu 24.04, GCC 13.3,
  binutils 2.42): `bootstrap-ode.sh`, `mksandbox.sh`, `mkbinutils.sh`,
  `mkhostbin.sh` and `build_world`, 0 errors, in about three minutes:
  `mach_kernel.PRODUCTION` 907,224 bytes (text 687,143), `bootstrap`
  183,316, `name_server` 116,276, `default_pager` 178,932, Lites'
  `startup` 746,164 and emulator 598,352. That set, on a disk with
  NetBSD 1.0's root, boots multi-user and passes the
  compile-and-install loop by multiboot, and by OSF's loader with the
  boot the running system built (`docs/status.md`, "The boot").
- **The multiprocessor kernel** (0095--0097, 0101): FAST+MP, built on
  the host and on the running system, runs on two CPUs under QEMU
  (`-smp 2,sockets=2,cores=1,threads=1`; `docs/tools/boot.sh`), from
  OSF's primary and by multiboot, both CPUs running with ticks
  counting, and passes the compile-and-install loop.
- **The boot's last messages** (0098, 0100): "HD: false interrupt" and
  "dev 0: alternate sectors corrupt" are gone; the loop passes by both
  loaders.
- **On the running system**: generation 1, built there and installed
  where OSF's loader looks, booted from OSF's primary and built the
  same sources again; 589 of 592 object files identical, the other
  three differing only in their build stamps (`docs/status.md`,
  "Rebuilding itself").

## Rebuilding it

On the host, from a clone of the branch:

	ODE4LINUX=<ode4linux clone> sh docs/tools/bootstrap-ode.sh
	ODE4LINUX=<ode4linux clone> sh docs/tools/mksandbox.sh
	cp ~/.cache/osfmk/sandboxrc ~/.sandboxrc
	. ./set_ode_path.sh
	export PATH=~/.cache/osfmk/ode-sandbox/tools/at386_linux/bin:$PATH
	sh docs/tools/mkbinutils.sh
	cd osfmk/src
	sh ../../docs/tools/mkhostbin.sh
	sh ../../build_world

`MK_BUILD`, default `~/.cache/osfmk`, holds everything built; nothing
is written into the tree but the five paths `.gitignore` lists. The
root and the disk:

	sh docs/tools/mknbmakefs.sh				# once
	sh docs/tools/mklitesroot.sh root <NetBSD 1.0 sets>	# as root
	ROOT_TREE=root ROOT_MB=100 PAGING_MB=64 BOOTSTRAP_CONF=bootstrap.conf \
	    sh docs/tools/mkbootdisk.sh disk.img name_server default_pager startup emulator
	sh docs/tools/boot.sh disk.img

with `bootstrap.conf` as `docs/status.md` gives it. For OSF's loader,
`BOOT=`, `MACH_BOOT=`, `KERNEL=` and `BOOTSTRAP=` to `mkbootdisk.sh`
and `BIOS=1` to `boot.sh`; the boot is built on the running system
only (`build -here stand`), whose binutils read its 16-bit code. The
running system's own disk, with its toolchain, is made by
`docs/tools/mkselfhost.sh`; the GNU toolchain's sources go on that
disk, never into the tree.

## What it does not settle

Carried open, each where it is recorded:

- `docs/deferred.md`: S-8, a shell that sometimes stops after a pipeline
  (watched); S-11, for the maintainer; S-12's `sendmail`; O-1, O-2 and
  O-4 to O-7, Lites against the OSF/1 server's documented behaviour.
- `docs/provenance/verdicts.md`: the walk of `dev`'s commits stopped at
  row 32, its goal reached by this tree's own route.
- `docs/provenance/imports.md`: two early sets of vendor changes,
  `b88f799`'s typed nulls and `3036fdb`'s `locore.s` operand widths,
  recorded there without notes at their sites.
- A GCC later than 2.7.2.1 on the running system is the roadmap's
  next step for milestone 5.

## Falling back

	git fetch origin restored
	git switch -c <name> origin/restored

The branch is the reference: `git diff origin/restored` from `dev3`
shows everything done since.
