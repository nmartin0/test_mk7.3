# MILESTONE: a shell, and commands that run

NetBSD 1.0's `/bin/sh` is running under LITES on OSFMK 7.3 and
executing commands typed at the console. Step 4 of `ROADMAP.md` is
done.

The transcript, unedited apart from stripping carriage returns:

```
Enter pathname of shell or RETURN for sh:
# echo hello from lites
hello from lites
# pwd
/
# ls /
bin             etc             mach_servers    tmp
dev             lost+found      sbin            usr
# ls /mach_servers
emulator        init
# ps
ps: /dev/mem: Device not configured
# date
Fri Sep 18 00:35:04  2026
# echo test > /tmp/x
cannot create /tmp/x: read-only file system
```

`ls` alone is fork, exec, a directory read through ext2 and formatted
output back through the tty, in one line.

## What this settles

**COM input reaches LITES.** Every byte before this went outward, and
whether the kernel's `com` driver delivered input through the device
port to the server's tty layer was unknown -- it is the one thing that
could not be established by reading. It works, first try, with no
change to any driver.

**The console is the serial line, not `kd`.** `-r` in the kernel
command line selects it and `boot-ide.sh` has always passed it, which
is why output reached a file at all. Answering the prompt confirms the
same line carries input.

**The line discipline is doing real work**: `#` prompts, commands echo,
and a carriage return terminates a line. No `stty` was needed.

## What the transcript also establishes, as open work

- **`ps` fails: `/dev/mem: Device not configured`.** The node exists
  (`c 2 0`) and LITES has no device behind it. `ps` on a 1994 BSD reads
  kernel memory directly, which on a microkernel is not where the proc
  table lives, so this wants thought rather than a device node.
- **The root is read-only**, confirmed from userland rather than from
  `ext2_vfsops.c:117` alone. Nothing can be written anywhere, including
  `/tmp`, which rules out most real use and is the next substantial
  piece of work -- and the first thing to exercise ext2's write path,
  which has never run.
- `date` is right, and still prints the `e_mapped_timeofday` fallback
  because `/dev/time` does not exist.

## How to get here

```sh
CONSOLE=socket STARTUP_ARGS='-s -i /init' sh tools/boot-ide.sh
python3 tools/console.py --attach &
python3 tools/console.py --wait-for 'RETURN for sh:' --send ''
python3 tools/console.py --wait-for '# ' --send 'echo hi'
tail /tmp/console.log
```

`--wait-for` is not decoration. Under TCG the prompt arrives about five
minutes after the boot starts, so a bare `--send` on the next line
types into a guest that is still loading the kernel, and the console
then looks like it never answered. Waiting for the text the guest
prints is also better than sleeping a guessed number of seconds, which
is the habit this project keeps having to correct.

---

# RESOLVED: the pid-2 hack is now conditioned on mach_init

The blocker below is fixed, by option 2 of the three listed there.
`wait4()`'s hard-coded `p->p_pid == 2` test now also asks whether
mach_init is the program LITES actually started, via the basename of
`init_program_path`. Shipped in `tools/lites/lites-osfmk73.patch`.

Boot after the change, single user, otherwise identical:

| marker                    | before | after |
|---------------------------|--------|-------|
| `No child processes`      | repeating | **0** |
| `can't get /dev/console`  | repeating | **0** |
| `Enter pathname of shell` | repeating | **1** |

The log then stops and stays stopped while QEMU is alive and running:
init blocked reading the console at its single-user prompt.

**The mach_init path is untested and cannot be tested yet**, because no
mach_init exists for i386 (ROADMAP 4a). The evidence for it is that the
predicate was checked on the host against seven inputs, including the
near misses `mach_init2`, `my_mach_init` and `/mach_init/init`, and
that the code path is unchanged when the predicate is true. That is
weaker than a boot, and is recorded as weaker.

This is an interim, not a verdict. Porting mach_init makes the hack
correct on its own terms and remains the faithful fix; option 3,
deleting the hack, stays wrong for the same reason it always was.

**What is now open is different**: init prints its prompt and waits for
input, and nothing has ever answered it. `boot-ide.sh` writes the
serial console to a file, which cannot take keystrokes, so `/bin/sh`
has never been driven and no command has ever run to completion under
LITES. `tools/boot-debug.sh` is the one with an interactive console and
is untried for this. That is the next thing to establish, and it is
what would let step 4 be called finished.

---

# ROOT CAUSE: init's ECHILD is LITES's own mach_init pid-2 hack

Measured, then controlled. Found by reading `server/kern/kern_exit.c`,
not by booting.

`wait4()` scans the caller's child list and counts matches in `nfound`.
At the end of the loop:

```c
#if defined(LITES)
	/*
	 * XXX major hack for BSD init compatibility.
	 *
	 * Under lites, mach_init (pid 2) is a child of BSD init (1).
	 * ... For now we just pretend mach_init isn't there ...
	 */
	if (q->p_pid == 1 && p->p_pid == 2) {
		nfound--;
		continue;
	}
#endif
	}
	if (nfound == 0)
		return (ECHILD);
```

LITES hard-codes that **pid 2 is mach_init** and hides it from init's
`wait()`, so that init does not block for 30 seconds on a child that
never exits.

We boot with `-i /init`, which skips mach_init entirely and makes
NetBSD's init the first program, pid 1. The first shell it forks
therefore takes **pid 2** -- the reserved pid -- and the hack fires
against a legitimate child. `nfound` drops to zero and `wait()` returns
ECHILD.

## This explains every symptom, in order

1. init (pid 1) forks the single-user shell, which is pid 2. It
   acquires the console normally -- the probe recorded
   `sctty: pid 2 ... granted`.
2. init calls `wait()`. The hack hides pid 2, `nfound` is 0, ECHILD:
   `wait for single-user shell failed: No child processes; restarting`.
3. init forks again. That child is pid 3. Pid 2 is still alive and
   still owns the console, so pid 3's `TIOCSCTTY` is refused --
   correctly -- and init reports `can't get /dev/console for
   controlling terminal`.
4. Later children (pid 4, pid 5) are granted the console, because by
   then the earlier ones are gone. The probe recorded exactly that.

So the console error was two steps downstream of the cause, and the
`TIOCSCTTY` code was never at fault.

## The control

`kern_exit.c` patched to announce the hack and skip it, one value per
`printf`. One boot, otherwise identical:

```
wait4: mach_init hack would hide pid 2
wait4: hack DISABLED for this run
init: /etc/spwd.db: No such file or directory
Enter pathname of shell or RETURN for sh:
```

The hack fires on pid 2, as predicted. With it skipped:

| marker in console log        | hack enabled | hack skipped |
|------------------------------|--------------|--------------|
| `No child processes`         | present      | **0**        |
| `can't get /dev/console`     | present      | **0**        |
| `Enter pathname of shell`    | repeating    | **1**        |

The log then stops growing and stays stopped for over ten minutes,
while QEMU remains alive and running (`ps` state R, 8 minutes of CPU).
That is init blocked reading the console at its single-user prompt,
which is the correct behaviour -- not a hang. The respawn loop is
gone.

## What this does NOT establish

The probe is not a fix and is not committed. Deleting the hack is one
of the options below, not a decision; the tree is unchanged.

The `-serial file:` console cannot take input, so the prompt has not
been answered and `/bin/sh` has not been driven interactively.
`tools/boot-debug.sh` is the one with an interactive console.

## The options, to be decided rather than assumed

1. **Run mach_init as the first program**, which is what LITES expects
   and what makes pid 2 genuinely mach_init. This is ROADMAP step 4a;
   the port exists at `mach_services/cmds/mach_init/` and waits only
   on a libc to link against. The hack then becomes correct again and
   needs no change. Most faithful, most work.
2. **Condition the hack on the init program actually being
   mach_init**, rather than on the bare number 2. A LITES source
   change, so it would have to be regenerated into
   `tools/lites/lites-osfmk73.patch` in the same commit.
3. **Drop the hack** when booting a BSD init directly. Smallest
   change, but it silently breaks the configuration LITES was written
   for, which is the one option 1 restores.

Option 1 is the design; options 2 and 3 are ways to run a
configuration LITES did not anticipate. The choice belongs to the
maintainer.

## Correction to commit d157d38

That commit's message says a launching tool call "may report failure
while qemu is in fact running". That is a misdiagnosis. The calls were
dying because `pkill -f qemu-system-i386` matches the caller's own
shell, exactly as DEBUGGING.md section 9 describes. Fixed in the
commit that precedes this one.

---

# CORRECTION: TIOCSCTTY is not the blocker, and the refusal is correct

Measured, not reasoned. A probe in the `TIOCSCTTY` case of
`server/kern/tty.c` printing each clause's inputs, one value per
`printf`:

```
init: wait for single-user shell failed: No child processes; restarting
sctty: pid 2   leader_is_self 1  s_ttyvp 0  p_session 49cea4
sctty: t_session 0                                  -> granted
init: /etc/spwd.db: No such file or directory
Enter pathname of shell or RETURN for sh:
sctty: pid 3   leader_is_self 1  s_ttyvp 0  p_session 49ce64
sctty: t_session 49cea4                             -> REFUSED clause 2
init: can't get /dev/console for controlling terminal: Operation not
      permitted
sctty: pid 4   t_session 0                          -> granted
/etc/rc: Can't open /etc/rc
sctty: pid 5   t_session 0                          -> granted
```

(The columns are joined here for width; each value was printed alone.)

## What this establishes

**The EPERM is correct BSD behaviour.** pid 3 is refused because
`tp->t_session` is `0x49cea4`, which is pid 2's session -- another
process already owns the console. That is exactly what the clause is
for. There is no permission bug in `tty.c` to find.

**`setsid()` works.** Every child is its own session leader:
`leader_is_self 1`, `pgid == pid`, a distinct `p_session` each time.
The session-leader clause never fires.

**The shell runs.** `/etc/rc: Can't open /etc/rc` is `/bin/sh`
executing and failing to open a file the minimal root does not have.
pid 4 and pid 5 were both granted the console. NetBSD's userland is
running under LITES.

## What the real defect is

The first message is the primary one:

```
init: wait for single-user shell failed: No child processes; restarting
```

That is `wait()` returning **ECHILD** to init for a child it has just
forked. init concludes the shell died, and forks another -- while the
first is still alive and still holds the console. The second child's
`TIOCSCTTY` is then refused, correctly, and init reports "can't get
/dev/console". The loop repeats.

So the causal order is the reverse of what was assumed: the console
message is a downstream symptom of a `wait()`/child-bookkeeping fault,
not a tty permission problem. Chasing `TIOCSCTTY` further would have
been chasing a correct refusal.

## What I got wrong earlier in this same session

The note at the head of this file claimed the ordering of init's
messages proved the *first* forked child was being refused, since
NetBSD's `single_user()` prompts before calling `setctty()`. The
reasoning about NetBSD's source order was right and the conclusion was
wrong: the refused process is pid 3, the second child, and pid 2 was
granted the console before any of those messages appeared. The
ordering of init's own output does not order the children, because
init's messages and the probe's interleave from different processes.

This is the same error the file already records twice -- a plausible
inference from the wrong source -- and the fix was the same: print the
identity of what is being measured, not just its value. `pid` and
`p_session` are what made it unambiguous.

## Next

Find why `wait()` returns ECHILD. Start in `server/kern/kern_exit.c`
and the proc bookkeeping `kern_fork()` sets up, and note that
`init_main.c` builds `initproc` by calling `newproc()` directly rather
than through `kern_fork()` -- a difference session 5 already had to
patch once, for the shared region and the parent pointer.

Worth printing, in init's own wait path: the child pid returned by
fork, `p_pptr` of the child, and the contents of the parent's child
list at the moment `wait()` decides there is nothing to wait for.

The probe that produced this is not committed. It is a dozen `printf`
calls around the clause in `server/kern/tty.c:865`; re-add it there if
the tty path needs measuring again.

---

# Reproduced from a clean tree, session 6

The blocker below is reproduced, on a sandbox rebuilt from nothing. The
console now reads:

```
Sep 17 22:44:11 init: wait for single-user shell failed: No child
                      processes; restarting
Sep 17 22:44:13 init: /etc/spwd.db: No such file or directory
Enter pathname of shell or RETURN for sh:
Sep 17 22:44:14 init: can't get /dev/console for controlling terminal:
                      Operation not permitted
```

## What the ordering settles

**This section was wrong and is kept as written, with the correction
here, because the reasoning is a trap worth seeing.** See the ROOT
CAUSE section at the head of this file: the refused process is pid 3,
the *second* child, and the hypothesis dismissed below -- that an
earlier child holds `tp->t_session` -- is exactly what is happening.

The argument was: the three messages arrive in the order spwd.db,
prompt, then the TIOCSCTTY failure, and NetBSD 1.0's `single_user()`
runs the SECURE password check and the DEBUGSHELL prompt *before* it
calls `setctty()`, so the failing ioctl must be in the first child init
forks.

Every step of that is true about NetBSD's source and the conclusion is
still false, because init's messages do not order init's *children*.
Two children were interleaving their output, and a third process -- the
probe -- was printing between them. `METHODOLOGY.md` section 3.5 says
this directly: ordering tells you about print order, not causal order;
use it to generate hypotheses, not to close them.

What settled it was printing `p_pid` and `p_session` inside the clause,
which is section 3.5b: a new fact rather than a new reading of the
facts already in hand.

The three clauses that can return EPERM are in `server/kern/tty.c:865`,
not `server/serv/tty_io.c` as the session 5 handoff says:

```c
if (!SESS_LEADER(p) ||
    (p->p_session->s_ttyvp || tp->t_session) &&
    (tp->t_session != p->p_session))
	return (EPERM);
```

`login_tty()` calls `setsid()` and ignores its return value, so a
`setsid()` that failed in the child would leave it a non-session-leader
and produce exactly this EPERM with nothing logged. That is the first
thing to measure, and it is measurable: print which clause fired,
together with `p_pid`, `p_pgid`, `s_leader`'s pid, `s_ttyvp` and
`tp->t_session`, rather than the values alone.

## Getting back to this state

Two defects had to be fixed before the tree would reach it at all; both
are committed, with their reasoning, and neither is related to the tty.

- `libmach_sa` could not link. The committed tree did not build LITES.
- `mkroot-netbsd.sh` built roots whose `/dev` was empty.

A third was a boot configuration error rather than a bug, and it is
what the commit carrying this note fixes: with no server directory
named in `bootstrap.conf`, LITES derives one by concatenating the root
name with the directory part of `argv[0]`, and `argv[0]` is the full
path the bootstrap task loaded the server from, not what
`bootstrap.conf` says. The result is

```
(lites): path(/dev/hd0c/dev/boot_device/mach_servers) derived from root
(lites): init_program(/dev/boot_device/mach_servers/init)
panic: first program (...) exec failed: xc002 file or directory does
       not exist
panic: init died
```

so LITES looks for a literal `/dev/boot_device/mach_servers` directory
on the root filesystem. Naming the directory explicitly, which
`server_init.c` calls the 3.0 style, strips the paths back to
`/mach_servers/init` and `/mach_servers/emulator`.

Those two files are **not** installed by any script. `mkroot-netbsd.sh`
creates `/mach_servers` and leaves it empty; the emulator and the init
program have to be written into it by hand:

```sh
debugfs -w -R "write ~/lites-build/obj/emulator/emulator.Lites.1.1.u3 \
	/mach_servers/emulator" /tmp/root.img
debugfs -w -R "dump /sbin/init /tmp/nbinit" /tmp/root.img
debugfs -w -R "write /tmp/nbinit /mach_servers/init" /tmp/root.img
debugfs -w -R "sif /mach_servers/init mode 0100755" /tmp/root.img
```

`debugfs`'s `write` does resolve a path, unlike its `mknod`. The `sif`
is needed because `write` leaves the mode at 0644 and exec wants a set
execute bit even for root.

Then, for the single-user boot this needs:

```sh
STARTUP_ARGS='-s -i /init' sh tools/boot-ide.sh
```

`-i /init` is relative to the server directory, so it names
`/mach_servers/init`, which is why NetBSD's `/sbin/init` is copied
there rather than pointed at in place.

---

# SOLVED: NetBSD's init talks to the console

```
Sep 17 20:11:55 init: wait for single-user shell failed: No child processes; restarting
Sep 17 20:11:57 init: /etc/spwd.db: No such file or directory
Enter pathname of shell or RETURN for sh:
Sep 17 20:11:57 init: can't get /dev/console for controlling terminal: Operation not permitted
```

**`Enter pathname of shell or RETURN for sh:`** -- NetBSD 1.0's
single-user prompt, printed by an unmodified 1994 binary running under
LITES on OSFMK 7.3. Zero panics.

## The bug

`tty_param()` ends:

```c
error = device_set_status(tp->t_device_port, TTY_STATUS,
			  (int *)&ttstat, ttstat_count);
return (error);
```

OSFMK's i386 console is the `kd` driver, which **does not implement
`TTY_STATUS`**, so that returns `D_INVALID_OPERATION` -- 2505, `0x9c9`.
`tty_open()` then did:

```c
rc = tty_param(tp, &tp->t_termios);
if (rc != D_SUCCESS)
	return(rc);		/* raw Mach error */
```

and that propagated **untranslated** through `cons_open()` and
`spec_open()` to `open()`, where `e_mach_error_to_errno()` mapped it to
`ENOTTY`. So every `open("/dev/console")` failed with "Inappropriate
ioctl for device", and init could never acquire a console.

## The fix

Treat a device that refuses `TTY_STATUS` as one that simply does not
have it:

```c
if (rc == D_INVALID_OPERATION)
	rc = D_SUCCESS;
```

The surrounding code already assumes this interface may be absent:
`tty_param()` discards the matching `device_get_status()` with a
`(void)` cast, and the next line sets `TS_CARR_ON` with the comment
`/* should get from TTY_STATUS */`. Only the `set` path treated absence
as fatal.

## How it was found, since the trail was long

Seven wrong turns, each eliminated by measurement rather than argument:

1. `prot = 0` on the mapped sections -- **my own format string**, `%x`
   against a 64-bit `off_t`, desynchronising the varargs.
2. `e_getuid` returning a garbage euid -- the trampoline deliberately
   preserves the caller's `edx`.
3. `initproc->p_pptr` unset -- true, and fixed, but not this bug.
4. The `VBLK` vnode in `spec_open` -- a different device; the console is
   `dev 0`, that was `dev 2`.
5. `ext2_inode_cnv.c` never setting `i_rdev` -- `di_rdev` is a macro for
   `di_db[0]`, which the block loop does copy.
6. `ext2_specop_p` being incomplete -- `{ &vop_open_desc, spec_open }`
   is correctly wired.
7. `MNT_NODEV` on the ext2 root -- the mount sets `MNT_RDONLY`, not
   `MNT_NODEV`.

What finally located it was probing `cons_open()` and reading the value
`tty_open()` returned: `0x9c9`, which is not an errno at all.

## Remaining, and all of it ordinary

- `can't get /dev/console for controlling terminal: Operation not
  permitted` -- `TIOCSCTTY` is refused. Next thing to look at.
- `/etc/spwd.db` and `/etc/ttys` are absent from our minimal root.
- `e_mapped_timeofday init failed 2` -- `/dev/time` does not exist; the
  fallback to `e_gettimeofday()` is deliberate.

# Console open: what is established, precisely

## 25 really is ENOTTY

Confirmed in LITES's own table:

```c
include/sys/errno.h:105:  #define ___ENOTTY  25  /* Inappropriate ioctl for device */
```

And the translation preserves it. `e_open()` does

```c
err = kr ? e_mach_error_to_errno(kr) : 0;
```

and `e_mach_error_to_errno()` passes through
`e_kernel_error_to_lites_error()`, which unwraps a LITES errno rather
than inventing one. So **LITES genuinely returns ENOTTY from
`open("/dev/console", O_RDWR)`** -- this is not the mislabelled-number
trap that caught the `code`/`subcode` reading earlier.

## Where ENOTTY comes from in the server

Four sites return it. Three are in `tty_ioctl` and `device_misc.c`,
reached only from `ioctl()`. The fourth is the interesting one:

```c
/* server/kern/subr_xxx.c */
/*
 * Unsupported ioctl function.
 */
enoioctl()
{
	return (ENOTTY);
}
```

`enoioctl` is the stub a `cdevsw` entry uses for an operation it does
not implement. ENOTTY arriving from `open()` therefore suggests a
**device operation vector reaching a stub**, rather than an open check
failing.

## The other established facts

- `/dev/console` never reaches `spec_open()`. The probe, narrowed to
  major 0, only ever sees a **block** device with `dev = 2`, while the
  console node is `c 0 0`, `dev = 0`.
- `revoke("/dev/console")` on the same path **succeeds**, returning
  `(x0 x0)`. So `namei()` resolves it and the vnode exists.
- The ext2 root is mounted **`MNT_RDONLY`** -- `ext2_mountroot()` sets
  `mp->mnt_flag = MNT_RDONLY` then adds `MNT_ROOTFS`. Opening a device
  node `O_RDWR` on a read-only filesystem is legal in BSD, since the
  restriction applies to the filesystem rather than the device, but it
  is worth eliminating.

## Next

Two probes, one boot:

1. **`vn_open()`**, in `server/kern/vfs_syscalls.c`: print its return
   and the `v_type` it sees for the console path. `revoke()` proves the
   lookup succeeds, so the rejection is in `vn_open()`'s own checks or
   in `VOP_OPEN`.
2. **`ext2_specop_p`**: `ext2_vfsops.c:935` passes this vector to
   `ufs_vinit()` for special files. If it is incomplete -- an entry left
   as `enoioctl` or an equivalent stub where `spec_open` should be --
   that would produce exactly ENOTTY from `open()` on any device node in
   an ext2 filesystem, while leaving `revoke()` unaffected.

The second is the stronger hypothesis. It also predicts that **no**
device node on ext2 can be opened, which is testable with `/dev/null`
and would be a general defect rather than a console-specific one.

# CORRECTION: the VBLK node is not the console

The previous entry reported `spec_open()` receiving the console as
`VBLK`. **That was the wrong device.** Narrowing the probe to major 0
and printing the full `dev`:

```
so: CONSOLE v_type   3      VBLK
so: CONSOLE dev      2      minor 2
```

`/dev/console` was created as `c 0 0`, so its `dev` is **0**, not 2.
Every hit on major 0 has minor 2 and is a **block** device, so these are
some other node going through `bdevsw[0]` -- not the console.

**So `/dev/console` never reaches `spec_open()` at all**, and the
original reading was right. The `VBLK` finding was real but about a
different device, and drawing the console conclusion from it was the
same mistake as reading `getuid`'s success as proof the proc was
correct: a plausible number from the wrong source.

## What is actually established

- `open("/dev/console", O_RDWR)` returns 25.
- It does **not** reach `spec_open()`, so it fails in `namei()` or
  `vn_open()` before device dispatch.
- `revoke("/dev/console")` on the **same path** succeeds, returning
  `(x0 x0)`. So the path resolves and the vnode is found -- the failure
  is specific to opening it, not to naming it.

That last point is the useful one and is new: `revoke()` and `open()`
take the same `namei()` route, and one works. Whatever rejects the open
is between the successful lookup and `VOP_OPEN`.

## Next

Probe `vn_open()` in `server/kern/vfs_syscalls.c` -- its return value,
and the `v_type` it sees -- for the console path specifically. Since
`revoke()` proves the lookup succeeds, the fault is in `vn_open()`'s own
checks: it tests `v_type` against the requested mode, rejects `VBLK` and
`VCHR` in some configurations, and checks the mount's `MNT_NODEV` flag.

**`MNT_NODEV` is worth checking first.** `spec_open()` has

```c
if (vp->v_mount && (vp->v_mount->mnt_flag & MNT_NODEV))
	return (ENXIO);
```

and `vn_open()` may have an equivalent. If the ext2 root is mounted with
`MNT_NODEV` set -- by default, or because the flag word is
uninitialised -- every device node on it would be unusable while
ordinary files and `revoke()` continued to work. That fits every
observation.

# REAL NEWS: the device node arrives as VBLK, not VCHR

`spec_open()` **is** entered -- the earlier conclusion that it was not
reached was wrong, and came from probing `tty_open()` in a build that
did not also probe `spec_open()`. With the probe in the right place:

```
so: entered
so: v_type   3
so: dev      2
so: maj      0
```

In BSD's vnode types -- `VNON=0, VREG=1, VDIR=2, VBLK=3, VCHR=4` --
**`v_type` is 3, `VBLK`.** A console must be `VCHR`.

`spec_open()` switches on `vp->v_type`:

```c
switch (vp->v_type) {
case VCHR:
	if ((u_int)maj >= nchrdev)
		return (ENXIO);
	...
	error = (*cdevsw[maj].d_open)(dev, ap->a_mode, S_IFCHR, ap->a_p);
```

The `VBLK` arm does entirely different checks and never reaches
`cdevsw[maj].d_open`. **That is exactly why `cons_open()` and
`tty_open()` never fired**, and it explains the failure without any of
the tty-layer theories.

## Where VBLK comes from

The node was created as a **character** device:

```sh
debugfs -w -R "mknod /dev/console c 0 0" root.img
debugfs -w -R "ln <33> /dev/console"     root.img
```

and `debugfs` reported mode `20000`, which is `S_IFCHR`. So the on-disk
inode is right, and something between the ext2 inode and `vp->v_type`
turns a character device into a block device.

That conversion is in the ext2 reader, and it is where the earlier
`i_rdev` question really belongs -- not the device number, which is
arriving (`dev` and `maj` are consistent with a node in that range), but
the **type**.

## Next

Find where ext2 sets `v_type` from the inode mode. UFS does this in
`ufs_vnops.c`/`ufs_inode.c` via `IFTOVT()`; the ext2 reader has its own
path. Printing the mode it reads alongside the `v_type` it derives will
show whether the mode is wrong on arrival or the mapping is.

**Also note `dev = 2` with `maj = 0`**, so minor 2 -- while
`/dev/console` was created as `c 0 0`, giving `dev` 0. So this
particular `spec_open` call may be for a different node entirely, and
the probe should print the vnode or path to be sure which device is
being opened before drawing conclusions about the console specifically.

# The console open never reaches tty_open

Probes placed in `tty_open()` -- on `cdev_name_string()`'s result and on
`device_open()`'s -- **never fire**, while the emulator still reports:

```
[2] e_open(/dev/console x2 x2)   -> 25
```

So `/dev/console` fails **before** `cons_open()` and `tty_open()` are
called at all. The failure is in the VFS layer, not the tty layer, and
the earlier suspicion of `cdev_name_string()` or `device_open()` is
ruled out.

That also disposes of the question of whether `25` is a Mach code or an
errno: it is produced somewhere else entirely, so neither of the two
`return (rc)` paths that motivated the question is involved.

## What was checked on the ext2 side, and a correction

The natural suspect is the device node itself. In ext2, a device's
major and minor live in `i_block[0]` of the inode, and the reader has to
carry that into the in-core inode as `i_rdev`.

`grep` shows `rdev` appearing exactly once in
`server/ufs/ext2fs/ext2_inode_cnv.c`, and only as `#undef i_rdev`, while
the UFS reader references `ip->i_rdev` throughout. **That looked
conclusive and is not.** In the BSD `dinode`, `di_rdev` is a macro for
`di_db[0]`, and the conversion does:

```c
for (i = 0; i < NDADDR; i++)
	di->di_db[i] = ei->i_block[i];
```

which copies `i_block[0]` -- exactly where ext2 keeps the device number
-- into the slot `di_rdev` names. So the device number may well arrive
correctly, and the absence of the identifier `i_rdev` proves nothing.

Recorded because the first reading was wrong and would have sent the
next attempt down a dead end.

## Next

Find what actually returns 25 for this open. The path is
`e_open` -> LITES's `open()` -> `namei()` -> `vn_open()` -> `VOP_OPEN`,
and the device dispatch happens in `spec_open()`. Probing `vn_open()`'s
return, and `spec_open()`'s entry and return, will locate it in one
boot. `spec_open()` is also where an `i_rdev` of zero would be rejected,
so the ext2 question can be settled at the same time by printing the
device number it actually sees.

Note `/dev/console` was created as `c 0 0`, giving `rdev` 0, and LITES's
console **is** character major 0. If anything on that path treats a zero
device number as "no device", the node would need a different minor --
which is cheap to test once the failing function is known.

# Located: open("/dev/console", O_RDWR) fails in tty_open

`e_open` already traces its path at `syscall_debug > 1`, so with tracing
forced on the sequence is unambiguous:

```
[1] e_open(/dev/time x0 x0)                -> ENOENT
[1] e_open(/etc/localtime x0 x0)           -> ENOENT
[1] e_open(/usr/share/zoneinfo/GMT ...)    -> ENOENT
[2] e_open(/dev/console x2 x2)             -> 25          <-- O_RDWR
```

The three ENOENTs are cosmetic -- `/dev/time` is the mapped-time device
(hence `e_mapped_timeofday init failed 2`), and the timezone files are
simply not in our minimal root. **The one that matters is
`/dev/console` with `O_RDWR` returning 25.**

## Why 25 is probably not ENOTTY

`cons_open()` (`server/serv/cons.c`) finds the console major by name in
`cdevsw`, builds `makedev(major, 0)` -- so our node's minor number is
irrelevant -- and calls `tty_open()`. That does:

```c
rc = cdev_name_string(dev, name);
if (rc != 0)
	return (rc);		/* bad name */
mode = D_READ|D_WRITE;
rc = device_open(device_server_port, ..., name, ...);
```

Both failure paths **`return (rc)` directly** -- a Mach `kern_return_t`
handed back as though it were a BSD errno. So the `25` the emulator
reports is very likely an untranslated Mach code, not `ENOTTY`, and
reading it as ENOTTY would send the next person the wrong way.

The two candidates are `cdev_name_string()` failing to build the device
name, or `device_open()` refusing it. Both are reachable and both would
surface as this same number.

## Next

Trace `rc` separately at each of those two returns, one value per
`printf`. That distinguishes a naming failure from an open failure, and
also reveals whether the value is a Mach error (large, structured) or a
small errno -- which settles the translation question at the same time.

This is the same pattern as the `code`/`subcode` confusion earlier in
this file: a number that looks like a familiar errno but is not one.

## Still cosmetic, not chased

`/dev/time` is missing, so `e_mapped_timeofday` falls back to
`e_gettimeofday()` on every process. Harmless, and the fallback is
deliberate.

# REAL NEWS: init runs properly. The device nodes were never created.

With `syscall_debug` forced on, NetBSD's `init` is doing exactly the
right thing:

```
e_getuid  e_getpid  e_setsid  e_setlogin
e_sigaction x16     e_bsd_sigprocmask
e_close e_close e_close
e_sysctl
e_machine_fork
```

Root check, pid check (now passing), session, login name, sixteen signal
handlers, closing inherited descriptors, then forking for the
single-user shell. That is a correct NetBSD init startup.

## The blocker was my own tooling

The child was calling `revoke(_PATH_CONSOLE)` and getting **ENOENT**,
because `/dev` contained nothing but `boot_device`:

```
$ debugfs -R "ls -l /dev" nbroot.img
  15 . | 2 .. | 29 boot_device
```

**`debugfs`'s `mknod` allocates an inode but does not link it into the
directory.** It reports "Allocated inode: 33" and exits successfully, so
every `mknod` in the root-building recipe silently did nothing. The node
has to be linked afterwards:

```sh
debugfs -w -R "mknod /dev/console c 0 0" root.img   # allocates inode 33
debugfs -w -R "ln <33> /dev/console"       root.img # links it -- required
```

That affects `tools/mkroot-netbsd.sh`, which uses `mknod` alone and
therefore produces a root with no device nodes at all. **It needs
fixing.**

## Result

With the nodes linked:

```
[2] return[2]  e_machine_fork = (x0 x1)
[2] return[56] e_revoke       = (x0 x0)      <-- succeeds now
[2] return[48] e_bsd_sigprocmask
[2] return[46] e_sigaction
[2] return[83] e_setitimer
[2] err_return[111] e_sigsuspend -> 4        EINTR
[2] return_SIG14                             SIGALRM
```

The child made **63 syscalls**, against a handful before.

# Next: opening the console returns ENOTTY

```
err_return[5] e_open -> 25
```

25 is `ENOTTY`. The node exists and `revoke` works, so the path resolves
and the device is found -- it is the open itself that fails, in LITES's
tty layer.

`server/i386/conf.c` entry 0 is
`{ "console", 0, console_ops }` with `cons_open`, and the emulator
resolves `/dev/console` to it. Whether `cons_open` needs a controlling
terminal established first, or the minor number matters, or the console
port is not attached, is the thing to read next.

`e_setsid` succeeded earlier, so the child does have its own session,
which is normally the precondition for acquiring a controlling tty.

# MILESTONE: multiple processes running. fork works.

```
emulator [1] e_mapped_timeofday init failed 2
emulator [2] e_mapped_timeofday init failed 2
emulator [3] e_mapped_timeofday init failed 2
emulator [4] e_mapped_timeofday init failed 2
```

**Zero exceptions**, down from 364,450. No panic. NetBSD's `/sbin/init`
is alive and forking children, and the emulator is running four
processes.

# The bug: emul_save_state saved the wrong stack pointer

`emulator/i386/emul_misc_asm.s`, a hand-written `setjmp`:

```asm
movl	0(%esp),%ecx
movl	%ecx,48(%edx)	/* pc to state[12] -- the return address */
...
movl	%esp,%ecx
movl	%ecx,60(%edx)	/* sp to state[15] -- esp INSIDE this function */
```

The saved `pc` is the return address, so a restore resumes at the
instruction after the call -- correct. But the saved `esp` still has that
return address on top of it. A real `ret` pops it and adds 4; a restore
does not. **So the child resumed four bytes low, and every `N(%esp)`
offset in the caller addressed the wrong slot.**

That is exactly why `*ischild = TRUE` succeeded and `*pid = x` faulted
four bytes away: neighbouring slots, one of which happened to hold
something writable.

**The fix** is one instruction -- save the `esp` the caller will have
*after* the return:

```asm
leal	4(%esp),%ecx
movl	%ecx,60(%edx)
```

## Confirmed by measurement

Before the fix, `state.uesp` was consistently below `e_fork_call`'s own
`esp`, and the child's probes never ran at all. After it:

```
ffk: child esp    bfffde08     the child branch executes
ffk: v_pid        bfffdf10     the same value the parent had
```

The child now sees the caller's frame as the caller left it.

## Why 1995 did not hit this

The saved `pc` and `esp` disagree by exactly the 4 bytes a `call`
pushes, so whether it matters depends entirely on what the compiler puts
where in the caller's frame. With the code generation of the day the
wrong slots happened to be harmless. It is the same shape as the other
faults in this chain: an assumption that held for one compiler and does
not hold for another.

## Where it stands now

Booting with `-s` in `bootstrap.conf`:

```
startup /mach_servers/startup -s -i /init hd0c
```

`server_init.c:359` turns that into `boothowto |= RB_SINGLE`, and
`init_main.c` passes `-s` through to the first program, so NetBSD's
`init` should take its single-user path and run `/bin/sh` on the
console.

It still forks repeatedly -- fewer children than before, but the pattern
is unchanged: process 1 forks, the child does not survive, and process 1
forks again. That is NetBSD `init`'s respawn loop.

**So init is reaching its child path and the child is failing to become
a shell.** The next thing to find is where: the child either fails to
open the console, fails to `exec /bin/sh`, or execs it and the shell
exits immediately.

Worth checking first, in order:

1. **Is `/bin/sh` reachable at the path init uses?** It is at `/bin/sh`
   in the ext2 root, but init may look elsewhere in single-user, and the
   emulator resolves paths through LITES's VFS.
2. **Does opening `/dev/console` succeed?** It was created with
   `mknod /dev/console c 0 0`, which matches LITES's `cdevsw` entry 0,
   but nothing has confirmed an `open()` on it works.
3. **Does the shell exec and then exit?** A statically linked NetBSD
   `sh` with no terminal, no `/etc/profile` and closed descriptors may
   simply exit.

**Syscall tracing is no longer on.** `syscall_debug > 2` gates it, and
`syscall_debug` is a BSS global that was **non-zero by accident** before
the BSS fix -- the tracing that guided this whole investigation was
enabled by the very corruption being investigated. It now correctly
reads zero, so it has to be turned on deliberately to see what the child
is doing.

## Remaining

`e_mapped_timeofday init failed 2` on every process. Error 2 is
`ENOENT`. Not fatal -- the processes run regardless -- but it is the
next thing to look at, along with what init is doing with its four
children and why it has not reached a shell.

# The setjmp fix did not work, and that is informative

Copying `pid`, `ischild` and `isvfork` into `volatile` locals before
`emul_save_state()` and using those afterwards **did not fix the
fault**. It moved it:

```
before:  eip a0012d21    mov %eax,(%edx)     *pid = x
after:   eip a0012d42    mov %edx,(%eax)     *v_pid = x
```

Same statement, compiled differently. The compiler now reads both
pointers from stack slots:

```asm
a0012d30:  mov  0xc(%esp),%eax
a0012d34:  movl $0x1,(%eax)       ; *v_ischild = TRUE   -- succeeds
a0012d3a:  mov  0x8(%esp),%eax
a0012d3e:  mov  0x4(%esp),%edx
a0012d42:  mov  %edx,(%eax)       ; *v_pid = x          -- faults
a0012d44:  call a0001c80 <child_init>
```

**This rules out register caching.** Both values come from memory, so
the problem is not that the compiler kept something in a register the
restore did not reload. The child's stack slot at `0x8(%esp)` holds
garbage while the one at `0xc(%esp)` four bytes away is fine.

## What that means

The child's stack is **partially** wrong. Adjacent slots in the same
frame disagree, which is not what a missing or unmapped stack looks
like, and not what a register-allocation hazard looks like either.

Candidates, in the order they seem worth testing:

1. **`emul_save_state()` restores a slightly different `esp` in the
   child than the parent had**, so the same offsets address different
   slots. The fault `esp` is `bfffde24`; comparing it against the
   parent's `esp` at the save would settle this immediately.
2. **The saved state is stale** -- taken before the frame was fully
   built, so the child resumes with an `esp` that was correct at save
   time but does not match the frame the code then expects.
3. **The copy-on-write of the stack page races the child's first
   write**, leaving part of the page unfaulted.

## Progress worth keeping

The volatile change is retained. It is correct regardless -- a
`setjmp`-style function must not read non-`volatile` locals after the
second return -- and leaving the code relying on three inline-asm
clobbers would be storing up the same class of failure for the next
compiler. But it is not this bug, and the commit says so.

**Also new:** `e_mapped_timeofday init failed 2` now appears, which it
did not before. Error 2 is `ENOENT`. Worth noting but not chased.

# The fault storm is in e_fork_call, on the child path

Limiting the exception trace to three and capturing the thread state
localises it exactly:

```
exception: exc     1          EXC_BAD_ACCESS
exception: code    5276a0     the faulting address -- varies each time
exception: signal  a          SIGBUS
exception: eip     a0012d21   constant
exception: esp     bfffde34   a valid stack address
```

`eip` is **inside the emulator's text** (`a0001000`-`a0024423`), not in
init. The same instruction faults every time; only the address it
touches varies.

```
a0012c90 T e_fork_call

a0012d14:  mov  0x64(%esp),%edx    ; a pointer argument, from the stack
a0012d18:  movl $0x1,(%eax)
a0012d1e:  mov  (%esp),%eax
a0012d21:  mov  %eax,(%edx)        ; <-- faults, writes through edx
a0012d23:  call a0001c80 <child_init>
```

So **init called `fork()`** -- it got past `getuid`, `getpid` and
whatever else -- and in the **child** path the emulator writes through a
pointer that holds garbage. The varying fault address is that pointer
differing between runs, which is the signature of reading uninitialised
or stale memory.

## Why it loops rather than dying

The fault is taken in the emulator, which is also what delivers signals.
Signalling the process re-enters the faulting path, faults again, and
repeats -- 364,450 times before the run was cut short. The loop is a
consequence of where the fault is, not a separate bug.

## The faulting line, exactly

`emulator/i386/e_machinedep.c:43`, `e_fork_call(boolean_t isvfork,
pid_t *pid, boolean_t *ischild)`:

```c
	x = emul_save_state(&state);
	...
	if (x != 0) {			/* the child */
		*ischild = TRUE;	/*  movl $0x1,(%eax)              */
		*pid = x;		/*  mov (%esp),%eax; mov %eax,(%edx)  <-- faults */
		child_init();		/*  call a0001c80 <child_init>    */
		...
```

The disassembly matches line for line. **`*pid = x` faults** because
`pid`, a parameter reloaded from `0x64(%esp)`, holds garbage in the
child.

`*ischild = TRUE` did **not** fault, so one parameter pointer survived
and the other did not. Both were on the parent's stack, so whatever went
wrong is partial rather than the whole frame being absent.

## Stack inheritance is probably not it

`server_exec.c` allocates the user stack with

```c
kr = vm_allocate(p->p_task, &stack_start, stack_size, FALSE);
```

and `vm_allocate` gives `VM_INHERIT_DEFAULT`, which is
`VM_INHERIT_COPY`. The emulator itself never calls `vm_inherit()` or
`cthread_fork_prepare()`, and it runs on the user stack rather than a
cthread stack, so there is nothing else to set.

More decisively: **`*ischild = TRUE` succeeded on the same stack frame
that `*pid = x` faulted on.** If the stack were missing or unreadable,
both would fail. The frame is there; one pointer value in it is wrong.

## The likely cause: a setjmp hazard

```c
e_fork_call(boolean_t isvfork, pid_t *pid, boolean_t *ischild)
{
	struct i386_thread_state	state;
	int rv[2];
	errno_t error;
	volatile int x;			/* only x is volatile */

	asm volatile("nop" : : : "eax", "edx", "ecx", "cc");
	x = emul_save_state(&state);	/* returns twice, like setjmp */
	asm volatile("nop" : : : "eax", "edx", "ecx", "cc");

	if (x != 0) {			/* the child */
		*ischild = TRUE;
		*pid = x;		/* pid reloaded from 0x64(%esp) */
```

`emul_save_state()` returns twice. **Any local or parameter not declared
`volatile` has an indeterminate value after the second return**, and
only `x` is marked. The two `asm volatile` statements are an attempt to
force reloads, but they clobber only `eax`, `edx` and `ecx` -- they say
nothing about what the compiler may have cached in a callee-saved
register or about which stack slots it considers live.

That fits the evidence exactly: one parameter pointer is usable and the
other is not, the bad value varies between runs, and the faulting
instruction reloads from a fixed stack offset.

**It also explains why this was never seen in 1995.** The clobber lists
happened to be sufficient for the compiler of the day. A modern GCC
makes different decisions about what to keep where, and this function
has no contract that constrains it.

**The fix** is to declare `pid` and `ischild` (and anything else read
after the save) `volatile`, which is what the C standard requires of a
`setjmp`-style function, rather than relying on inline-asm clobbers.
That needs testing rather than assuming: the parameters cannot be
redeclared in place, so they must be copied to `volatile` locals before
the save and used from those afterwards.

## Superseded: stack inheritance across fork

The child resumes at the state saved by `emul_save_state()`, on whatever
stack the fork produced. If that stack is not inherited correctly, every
reloaded parameter is garbage -- which is precisely what
`VM_INHERIT_COPY` exists to arrange:

```c
/* libcthreads, cthread_fork_prepare() */
vm_inherit(mach_task_self(), p->stack_base, p->stack_size,
	   VM_INHERIT_COPY);
```

That is the same machinery noted earlier when porting `mach_init`, whose
own HISTORY records that the explicit `cthread_fork_{prepare,parent,
child}` calls were added deliberately because they were needed.

**So the question is whether the emulator's stack is marked
`VM_INHERIT_COPY` before `bsd_fork` is called.** If it is shared or not
inherited, the child writes through pointers into a stack that is not
its own, or not there at all.

## Where to look

`e_fork_call` is in `emulator/i386/`. The instruction sequence --
`*eax = 1` then `*edx = <stack word>` immediately before
`call child_init` -- looks like the two-value fork return being written
through caller-supplied pointers, the same `rval[0]`/`rval[1]` pair the
trampoline uses:

```c
/* emulator/i386/e_trampoline.c */
err = e_fork((pid_t *) rval);
if (*rval)
    rval[1] = 0;
else
    rval[1] = 1;
```

So the suspect is how `rval` reaches `e_fork_call` on the child side,
where the stack has just been replaced by the fork. Reading the C source
of `e_fork_call` against this disassembly is the next step.

## Worth noting

This is the fourth bug in this chain to come from **uninitialised or
stale memory being used as a pointer**, after the shared region, the
emulator's BSS, and `getpid_cache`. Three of those had the same root --
`MAX_PHDRS`. Whether this one does too is not yet known, but it is the
first question to ask.

# FIXED: MAX_PHDRS. getpid now works. New failure after it.

The fix, in three parts, all needed together:

- **`MAX_PHDRS` 4 -> 16** in `include/sys/elf.h`.
- **The loop in `parse_exec_file()` bounded by `MAX_PHDRS`** as well as
  `e_phnum`, so a binary with more headers loses segments rather than
  reading past the fixed-size array.
- **`NSECTIONS` -> `MAX_PHDRS + 1`** in both callers, and the guard
  corrected from `nsecs < e_phnum` to `nsecs <= e_phnum`. The loop writes
  `secs[phdr+1].how = EXEC_M_STOP` after the last segment, so the array
  needs one entry **more** than the header count. The old test let a
  six-header binary into a six-entry array and wrote one past the end --
  the same class of overrun, found while fixing the first.

## Measured before and after

| | before | after |
|---|---|---|
| `li->zero_start` | `8` | **`a003fc44`** |
| `li->zero_count` | `ff8` | **`13bc`** |
| `getpid_cache` at first call | `1f1a0` | **`0`** |

And getpid now goes to the server and resolves correctly:

```
gp: p                7004
gp: initproc         7004
gp: p->p_pid         1
gp: initproc->p_pid  1
```

Same proc, right pid. NetBSD `init`'s `if (getpid() != 1)` check passes
for the first time.

# NEW: a fault storm immediately after

init gets past `getpid` and then takes **364,450** protection faults, at
addresses that vary each time:

```
exception: exc=1
exception: code=5276a0
exception: subcode=2
exception: signal=10
...repeating...
```

Every one is `EXC_BAD_ACCESS` with `KERN_PROTECTION_FAILURE`, delivered
as SIGBUS. The process is signalled, the signal handling faults, and it
repeats without progress.

That it loops rather than dying suggests the fault is taken while
delivering the signal for the previous fault -- so the first fault's
cause and the loop's cause may be different, and the loop should be
stopped before diagnosing the fault. `thread_psignal()` in
`server/serv/ux_exception.c` is where the signal is delivered.

**Note the addresses are in the same range as the original SIGBUS**
(`0x776a0` appears again). With the emulator's BSS now correctly
cleared, whatever is mapped there is worth re-examining -- the earlier
conclusion that the region was corrupted by the BSS overlap may have
been only half the story.

# COMPLETE ROOT CAUSE: MAX_PHDRS is 4, the emulator has 6

`include/sys/elf.h`:

```c
/* XXX Assumes the program headers will immediately follow the file header,
   which, while usually OK, isn't right according to the ELF spec.

   Also places a ceiling on the number of program headers.  */

#define MAX_PHDRS 4
typedef struct {
  Elf32_Ehdr ehdr;
  Elf32_Phdr phdrs[MAX_PHDRS];
} elf_exec;
```

The emulator, linked by modern GNU ld:

```
Start of program headers:   52      <- the first assumption holds
Number of program headers:  6       <- the second does not

LOAD       0xa0000000 R       phdrs[0]
LOAD       0xa0001000 R E     phdrs[1]
LOAD       0xa0025000 R       phdrs[2]
LOAD       0xa003bfe4 RW      phdrs[3]   filesz 0x3c60  memsz 0x434c
GNU_STACK                     phdrs[4]   OUT OF BOUNDS
GNU_RELRO                     phdrs[5]   OUT OF BOUNDS
```

`parse_exec_file()` loops `for (phdr = 0; phdr < elf->ehdr.e_phnum;
phdr++)` -- to **6** -- over an array of **4**. `phdrs[4]` and
`phdrs[5]` read past the end of the struct.

**And the order is exactly wrong.** `phdrs[3]`, the real data/bss
segment, correctly sets

```
li->zero_start = 0xa003bfe4 + 0x3c60 = 0xa003fc44
```

Then the two out-of-bounds reads follow. Whatever lies past the struct
was read as a program header, and if it looks like a writable `PT_LOAD`
with a non-zero `p_memsz` it passes both guards and **overwrites**
`zero_start` -- which is how it becomes `8`.

`GNU_STACK` and `GNU_RELRO` are exactly the headers modern linkers add
and 1995 linkers did not. The ceiling of 4 was adequate for the ELF of
its day.

## The complete chain

1. `MAX_PHDRS` is 4; the emulator has 6 program headers.
2. `parse_exec_file()` reads two headers out of bounds.
3. Garbage overwrites `li->zero_start`, `0xa003fc44` becoming `8`.
4. `set_emulator_state()` passes `8` in `ebx` and `0xff8` in `edi`.
5. `ecrt0` zeroes bytes 8 to 0x1000 -- the first page -- and never
   touches the emulator's BSS.
6. `getpid_cache`, a BSS global declared `= 0`, holds `0x1f1a0`.
7. `e_getpid` returns the cache without messaging the server.
8. NetBSD `init` runs `if (getpid() != 1) errx(1, "already running")`.
9. `errx` writes to stderr, gets `EBADF`, exits 1.
10. `panic: init died`.

Every step measured.

## The fix

Bound the loop by `MAX_PHDRS` **and** raise the ceiling. Both are
needed: raising it alone leaves the same overrun for any binary with
more headers, and bounding alone would silently ignore real segments.

A third improvement is worth taking at the same time: honour `e_phoff`
rather than assuming the headers follow the file header, which the
comment in that file already admits is not what the ELF spec says.

And `ecrt0` should refuse to clear from an address outside the
emulator's own image. That check would have turned four hours of silent
corruption into an immediate error.

# ROOT CAUSE FOUND: zero_start is 8

Probed in `set_emulator_state()`, one argument per `printf`:

```
ses: zero_start    8
ses: zero_count    ff8
ses: pc            a0001020
```

The entry point is correct. **`zero_start` is `8`.** It should be
`0xa003fc44`, the end of the emulator's `.data`.

So `ecrt0` does:

```c
register char *zero_start asm("ebx");	/* = 8 */
register int   zero_count asm("edi");	/* = 0xff8 */
...
for ( ; zero_count > 0; zero_count--)
	*zero_start++ = 0;
```

It zeroes bytes `8` through `0x1000` -- the first page of the address
space -- and **never touches the emulator's BSS**.

## The whole chain, end to end

1. `zero_start`/`zero_count` are computed as `8`/`0xff8` instead of
   `a003fc44`/`0x13bc`.
2. `ecrt0` therefore clears the wrong page and leaves the emulator's BSS
   holding whatever was in those pages.
3. `getpid_cache`, a BSS global at `a003fca4` declared
   `pid_t getpid_cache = 0;`, holds `0x1f1a0`.
4. `e_getpid` tests `(!XXX_enable_getpid_cache || getpid_cache == 0)`,
   finds the cache non-zero, and returns it **without messaging the
   server** -- which is why a server-side probe on `syscode == 20` never
   fired.
5. NetBSD's `init` runs `if (getpid() != 1) errx(1, "already running")`,
   which fires.
6. `errx()` writes to stderr, fails with `EBADF` since no descriptors
   are open, and exits 1.
7. LITES panics with "init died".

Every step of that is now measured rather than inferred.

## Where the bad value comes from

`liblites/exec_file.c` has a correct ELF computation:

```c
li->zero_start = elf->phdrs[phdr].p_vaddr + elf->phdrs[phdr].p_filesz;
li->zero_count = secs[phdr].size - secs[phdr].amount;
```

which for our emulator gives `a003c000 + 0x3c44 = a003fc44`. The value
`8` cannot come from that, so either this branch does not run for the
emulator, or the program headers it reads are not the emulator's.

`server_exec_load()` -- the function whose comment says it loads
"only emulators or other native programs" -- computes the same fields
from **a.out** header fields (`exech->a_data`, `exech->a_bss`) and has
no `BT_LITES_ELF` case at all. Applying a.out arithmetic to an ELF
header is the obvious way to get a small nonsense number.

**Next:** print `binary_type` in `server_exec_load()` and confirm which
path the emulator takes, then give it a correct ELF case or route it
through `parse_exec_file()`.

## A second bug, free with the first

`ecrt0` writing to address `8` is scribbling over the first page. It has
not caused a visible failure yet, but any fix should also make that
impossible -- a sanity check on `zero_start` before the loop would have
turned this silent corruption into an immediate, obvious error.

# NAILED: the emulator's BSS is not zeroed

`e_getpid` never reaches the server. Probed at the decision point:

```
gpc: cache    1f1a0
gpc: enable   1
```

```c
if (!XXX_enable_getpid_cache || getpid_cache == 0) {
	... real syscall ...
} else {
	*pid = getpid_cache;		/* taken: no message is sent */
}
```

`getpid_cache` is declared `pid_t getpid_cache = 0;` yet holds
`0x1f1a0` on the **first** call, so the cache branch is taken and
returns garbage. That is why the server-side probe on `syscode == 20`
never fired: no getpid message is ever sent.

## Why the cache is garbage

```
a003c28c D XXX_enable_getpid_cache	 correctly 1
a003fca4 B getpid_cache			 garbage
```

`getpid_cache` is in **BSS**, at `0xa003fca4`. The emulator's crt0
clears only a fragment:

```c
/* emulator/i386/ecrt0.c */
/*
 * ebx points to the BSS dirty page (shared with data)
 *	that needs to be cleared.
 * edi is the clearing count for the BSS fragment.
 */
register char *zero_start asm("ebx");
register int   zero_count asm("edi");
...
/* Clear beginning of BSS (on the page shared with DATA) */
for ( ; zero_count > 0; zero_count--)
	*zero_start++ = 0;
```

The server passes those in `ebx`/`edi` through `set_emulator_state()`.
Only the page BSS shares with data is cleared; everything beyond is
expected to be zero-filled by its anonymous mapping. `getpid_cache` is
three pages past that fragment, so nothing zeroes it.

**Every zero-initialised global in the emulator beyond the first BSS
page is affected**, not just this one. `getpid_cache` is simply the
first that got read.

## An independent confirmation of the earlier fix

`0xa003fca4` falls inside the **old** shared region
(`0xa003c000`-`0xa0040000`). So the emulator's BSS really did reach into
those pages, which corroborates the overlap fix from measurement rather
than argument.

## Why: the native loader has no ELF case

The emulator is loaded by `server_exec_load()`, described in its own
comment as loading "only emulators or other native programs". Its
binary-type switch accepts exactly four types:

```
case BT_LITES_Z:
case BT_LITES_Q:
case BT_LITES_SOM:
case BT_LITES_MIPSEL:
default:
	printf("server_exec_load: unknown binary_type x%x", binary_type);
	return ENOEXEC;
```

**There is no `BT_LITES_ELF` case**, and everything after the switch is
a.out arithmetic:

```c
data_size          = round_page(exech->a_data);
bss_fragment_start = data_start + exech->a_data;
bss_fragment_size  = data_size - exech->a_data;
bss_residue_size   = exech->a_bss - bss_fragment_size;
```

`a_data` and `a_bss` are a.out header fields. Our emulator is ELF, whose
real layout is

```
.data   VMA a003c000  size 3c44   ends a003fc44
.bss    VMA a003fc60  size  6d0   ends a0040330
```

`liblites/exec_file.c` does have a correct ELF path, computing
`zero_start = p_vaddr + p_filesz` and a count covering the rest of the
segment -- which would cover `getpid_cache` at `a003fca4`. But that path
is not the one the emulator goes through.

## This also explains the `0x10000000` threshold

Earlier this file recorded puzzlement at `exec_file.c` classifying an
ELF as `BT_LITES_ELF` only when its entry is above `0x10000000`. The
reason is now clear: **the emulator is deliberately linked high**, at
`0xa0001020`, so that it classifies as LITES-native rather than as a
foreign binary. The threshold is how LITES tells its own components from
the programs it runs. It was never a bug.

## Next

Establish which of these is true, since they need different fixes:

1. `server_exec_load()` is genuinely being used for the emulator, in
   which case it reaches `default:` and returns `ENOEXEC` -- but the
   emulator plainly runs, so this cannot be the whole story.
2. Something else loads the emulator and computes `zero_start` and
   `zero_count` from a.out fields on an ELF header, producing a
   clearing range that misses `getpid_cache`.

Printing `li->zero_start` and `li->zero_count` in `set_emulator_state()`
and comparing them against `a003fc44` and `0x13bc` will settle it in one
boot.

## Superseded: find what maps the emulator's BSS The
emulator is loaded by the server, so the mapping is made there --
`emul_exec_map_section()` handles `EXEC_M_ZERO_ALLOCATE` with
`vm_map(..., MACH_PORT_NULL, ..., TRUE /* anywhere? */, ...)`, which
should give zero-filled anonymous memory. Either the emulator's BSS is
not going through that path, or its size is understated so only part of
it is mapped and the rest lands on whatever was already there.

# FIXED: the emulator image overlapped the shared region

`include/i386/param.h` reserved a 256 KB window for the emulator and put
the four shared pages at the **top of that same window**:

```
EMULATOR_BASE  0xa0000000
emulator image ends              0xa003d04b   (249931 bytes)
shared region starts (END-4*pg)  0xa003c000   <-- 4171 bytes inside the image
EMULATOR_END   0xa0040000
```

The emulator's own data and bss sat on the shared pages. `us_version`
read as garbage, the emulator disabled the region, and the structures
LITES keeps there -- `us_vmspace`, `us_limit` -- were used as pointers.

**Fix: widen the window to 512 KB.** `EMULATOR_END` becomes
`0xa0080000`, so the shared pages move to `0xa007c000`-`0xa0080000`,
clear of the image. `EMULATOR_BASE` is unchanged, so the emulator's link
address is unchanged. Nothing else on i386 depends on END's exact value:
`mapin_user()` and `emul_mapped.c` both follow it, and the three range
checks in `i386/e_machinedep.c` only ask whether the PC lies inside the
emulator.

**Proof it was the cause.** Before the fix the emulator read `0xb` at
`shared_base_ro`. With `us_version` deliberately stamped `0x1234` on the
server side, it then read `shared region mismatch 1234/1` -- the
marker, proving it had reached the real region for the first time. With
`USHARED_VERSION` restored, the mismatch message disappears entirely.

## Result: init runs

The SIGBUS is gone. `/sbin/init` no longer faults; it executes and exits
deliberately:

```
return[24] e_getuid = (x0 ...)        uid 0       correct
return[20] e_getpid = (x1f1a0 ...)    pid 127392  WRONG
err_return[4] e_write -> 9            EBADF
exit(1)
```

One syscall became four, and the last is a clean `exit`, not a fault.

# Next: e_getpid returns garbage

NetBSD 1.0's `init` does:

```c
if (getuid() != 0)  errx(1, "%s", strerror(EPERM));   /* passes */
if (getpid() != 1)  errx(1, "already running");       /* fires */
```

`e_getpid` returns `0x1f1a0`, 127392, where it must return 1. `errx()`
then writes to stderr, which fails with `EBADF` because no descriptors
are open yet, and exits 1.

## What has been checked so far

- **The cache is not to blame.** `e_bsd.c:66` initialises
  `getpid_cache = 0` and `XXX_enable_getpid_cache = TRUE`, so the
  condition `(!XXX_enable_getpid_cache || getpid_cache == 0)` is true on
  the first call and the real syscall path runs.
- **The server's implementation is correct.**
  `server/kern/kern_prot.c:62` does `*retval = p->p_pid`, and the exec
  probe printed `p_pid` as 1 for this process.
- **The value looks like an address, not a pid.** `0x1f1a0` falls inside
  init's own bss (`0x1e000`-`0x1ff50`), so something is returning a
  pointer where a pid belongs.

**Eliminated: `p_pptr`.** `COMPAT_43` is indeed 1 in this build, and
`p2->p_pptr = p1` is set at `serv_fork.c:479`, inside `kern_fork()`
after `newproc()` returns -- so `initproc->p_pptr` really was never set,
exactly like its shared region. Setting it in `init_main.c` next to the
shared-region fix **did not change the returned value**, which stays
`0x1f1a0`. The fix is kept because it is correct on its own terms, but
it is not this bug.

**The plumbing is clean end to end.** Traced both directions:

```c
/* emulator: emul_generic.c */
bsd_msg.req.rval2 = rvalp[1];          /* send */
rvalp[0] = bsd_msg.rep.rval[0];        /* receive */

/* server: ux_syscall.c */
retval[0] = 0;
retval[1] = req->rval2;
error = (*callp->sy_call)(p, req->arg, retval);
rep->rval[0] = retval[0];

/* server: kern_prot.c */
*retval = p->p_pid;
```

Nothing drops or reorders the value. **So `p->p_pid` really is
`0x1f1a0` for whichever proc the server resolved** -- which is not the
one the exec probe saw as pid 1.

### The proc is resolved from the message port

```c
p = proc_receive_lookup(req->hdr.msgh_local_port, seqno);
```

If that returns the wrong proc, or a stale one, every field read through
`p` is wrong. That fits better than any plumbing fault, and it explains
something otherwise odd: **`e_getuid` returning 0 is not evidence that
the proc is right.** A garbage or wrong proc whose `cr_uid` happens to
read zero looks exactly like success. So the first "correct" syscall may
never have been correct -- only lucky.

**Next:** print `p` and `p->p_pid` from inside `ux_generic_server()`,
one argument per call, and compare against `initproc`. If they differ,
the bug is in `proc_receive_lookup()` or in what port the emulator sends
on; if they match, then `p_pid` is being corrupted after `newproc()`,
which is the same family of defect as the shared region and the parent
pointer.

### Superseded lead: the uninitialised word

```c
/* emulator/emul_generic.c */
bsd_msg.req.rval2 = rvalp[1];
```

`rvalp` is the caller's return array, and the callers look like this:

```c
errno_t e_getpid(pid_t *pid)
{
	integer_t rv[2];		/* uninitialised */
	...
	kr = emul_generic(process_self(), SYS_getpid, &a, &rv);
```

So `rv[1]` is stack garbage and is **sent to the server as an input**.
`e_getuid` and the other short syscalls share the pattern. Whether the
server uses `rval2` on the way in, and what it sends back, is the next
thing to read -- `bsd_msg.req` is a MIG request structure, so the reply
handling in the same file will say.

Note the returned `0x1f1a0` is inside init's bss, and the second value
`0x1dbbc` is identical to the one `e_getuid` returned, which is the
preserved `edx`. So the two calls agree about `rval[1]` and disagree
about `rval[0]`, which is consistent with the pid being overwritten
rather than never set.

### Superseded suspect: the parent pointer

```c
	*retval = p->p_pid;
#if COMPAT_43 || defined(COMPAT_SUNOS)
	retval[1] = p->p_pptr->p_pid;
#endif
```

`retval[1]` dereferences `p_pptr`, the parent proc. `initproc` is made
by calling `newproc()` directly from `init_main.c` rather than through
`kern_fork()`, which is exactly the path that was already found to skip
initialisation -- so whether its parent linkage is set is worth
checking, and whether `COMPAT_43` is even on in this build.

Two things to fix, in order:

1. **`e_getpid`** -- find why it returns that value. The process is pid
   1 on the server side (`proc_died()` panics on `p_pid == 1`, and the
   exec probe printed `p_pid` as 1), so the value is being lost or
   mistranslated between the server and the emulator.
2. **Descriptors** -- `/dev/console` needs opening as fd 0, 1 and 2
   before init runs, or by init itself. With `getpid` fixed, init gets
   further and will need them.

# ROOT CAUSE: the emulator image overlaps the shared region

Measured, not inferred.

| | address |
|---|---|
| `EMULATOR_BASE` | `0xa0000000` |
| emulator image ends (text 233359 + data 15456 + bss 1744 = 250559) | **`0xa003d2bf`** |
| shared **RW** region starts (`EMULATOR_END - 4*vm_page_size`) | **`0xa003c000`** |
| shared **RO** page (`EMULATOR_END - vm_page_size`) | `0xa003f000` |
| `EMULATOR_END` | `0xa0040000` |

**The emulator's image overruns into the shared region by 4,799
bytes.** `include/i386/param.h` reserves only 256 KB between
`EMULATOR_BASE` and `EMULATOR_END`, and places the four shared pages at
the top of that same window. Our emulator is 245 KB of image, which
leaves less than the four pages the shared region needs.

## How this was established

Each step measured with one argument per `printf`, after the earlier
lesson about multi-argument traces:

1. The section protections are correct (`prot` 5, 3, 3) -- ruled out.
2. The stamp lands: writing `0x1234` to `initproc->p_shared_ro` reads
   back as `0x1234` on the server side.
3. The exec path sees the same proc and the same memory: `p_pid` 1,
   `p_shared_off` `0x14000`, `us_version` `0x1234`, immediately before
   `mapin_user()`.
4. The emulator reads the **right address**: `vm_page_size` is `0x1000`
   and `shared_base_ro` is `0xa003f000`, exactly `EMULATOR_END - page`.
5. And it reads `0xb` there, not `0x1234`.

Right proc, right offset, right address, wrong contents. The only
remaining explanation is that something else occupies those pages -- and
the emulator's own image does.

## Why this fits every symptom

- **"shared region mismatch b/1"**: `us_version` reads whatever the
  emulator's data segment happens to hold at that offset.
- **The moving fault address** (`0x776a0`, `0x8bad0`, `0x51bad0`): the
  shared region contains `us_vmspace` and `us_limit`, which LITES uses
  as real structures. Garbage there is used as pointers, and where it
  points varies with whatever the emulator last wrote.
- **Death after one syscall**: `getuid` returns cleanly through the
  syscall path, then libc start-up touches something derived from the
  corrupted region.

## The fix, and why it needs care

Three candidates, in order of preference:

1. **Shrink the emulator.** It is built with debugging probes and
   `syscall_debug` support. A production build may fit in 256 KB, which
   would make this a configuration problem rather than a layout one.
2. **Move `EMULATOR_END` up** in `include/i386/param.h`. Changes a
   published address boundary, so anything else assuming that layout
   must be checked first -- `emul_mapped.c`, `mapin_user()`, and the
   emulator's own link address all reference it.
3. **Map the shared region somewhere else entirely**, away from the
   emulator's window.

Option 1 should be tried first because it is reversible and tells us
whether the original layout was ever adequate or whether we have simply
grown past it.

# The stamp lands; the emulator reads different memory

Measured, with one argument per `printf`:

| probe, immediately after stamping `initproc` | value |
|---|---|
| `initproc->p_shared_ro` | `0x4d0000` -- a valid mapping |
| `initproc->p_shared_off` | `0x14000` |
| `us_version` read back | **1** |

**The server-side write succeeds.** `us_version` reads back as
`USHARED_VERSION`. And the emulator still reports:

```
emulator [1] shared region mismatch b/1
```

So the server and the emulator are looking at **different memory**,
which is the whole problem stated precisely.

## Why that is surprising

The backing store should be common. `alloc_mapped_uarea()` in
`server_init.c` maps four pages from `shared_memory_port` at
`shared_offset` into the *server*, and sets

```c
p->p_shared_off = shared_offset;
p->p_shared_rw  = shared_address + 2*vm_page_size;
p->p_shared_ro  = shared_address + 3*vm_page_size;
```

`mapin_user()` in `serv_fork.c` maps the *same port* into the user task:

```c
vm_map(p->p_task, &user_addr /* EMULATOR_END - vm_page_size */, ...,
       shared_memory_port, p->p_shared_off + 3*vm_page_size,
       ..., VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
```

With `p_shared_off = 0x14000`, both sides should reference offset
`0x17000` of `shared_memory_port`. The emulator reads
`EMULATOR_END - vm_page_size`, which is exactly what that maps to.

`server_exec.c` calls `mapin_user(p)` at lines 355 and 429, so the exec
path does re-map after building the new task.

## The remaining question, narrowly

**Is the process that execs actually `initproc`?** Everything above
holds only if the `struct proc` whose region was stamped is the one
whose task the emulator runs in. The emulator labels its output
`emulator [1]`, and `proc_died()` panics on `p_pid == 1`, so the failing
process is pid 1 -- but that has not been checked against `initproc`.

The cheap test: stamp `us_version` with a recognisable value such as
`0x1234`, and print `p_shared_off` from inside `server_exec.c` just
before `mapin_user()`. If the offsets differ, the exec'd proc is not
the one that was stamped. If they match and the emulator still sees the
old value, the mapping itself is wrong.

## Still unexplained

The moving fault address -- `0x776a0`, `0x8bad0`, `0x51bad0` -- remains
the strongest hint that something uninitialised is being used as a
pointer. Whether that is the shared region is still not established, and
should not be assumed until the mismatch above is understood.

# The shared region: three creation paths, one of them incomplete

Tracked down properly. There are three ways a proc comes into being in
LITES, and they do not all set up the shared region.

| path | shared region |
|---|---|
| `proc0`, in `init_main.c` | uses it (`p_vmspace`, `p_limit` point into it) but never stamps it |
| `kern_fork()`, `serv_fork.c:404+` | **complete**: `mapin_user()`, then `us_version`, `us_proc_pointer`, both share locks |
| `newproc()`, `serv_fork.c:280` | **allocates the proc only** -- no region setup at all |

`kern_fork()` calls `newproc()` and then does the region work itself.
But `init_main.c:440` calls `newproc()` **directly**:

```c
initproc = newproc(p, TRUE, FALSE);
```

so none of it runs for the first program. `us_version` holds whatever
the page contained -- `0xb` -- and `emul_mapped.c` refuses the region,
leaving `shared_enabled` at 0 for the one process with no parent to
inherit a good region from.

## What was tried, and why it was not enough

Stamping the fields on `initproc` after `newproc()` returns -- mirroring
exactly what `kern_fork()` does -- **did not clear the mismatch.** The
emulator still reports `b/1`.

The reason is the step before the stamping. `kern_fork()` does:

```c
if ((result = mapin_user(p2)) != KERN_SUCCESS) { ... panic("kern_fork"); }
bcopy(p1->p_shared_ro, p2->p_shared_ro, sizeof(struct ushared_ro));
...
p2->p_shared_ro->us_version = USHARED_VERSION;
```

**`mapin_user()` is what maps the region into the user task**, at the
address the emulator looks for (`EMULATOR_END - vm_page_size`). Without
it, writing `us_version` through `initproc->p_shared_ro` on the server
side changes a page the emulator is not reading. Something else is
mapped at that address in the user task, which is why the emulator gets
`0xb` rather than faulting.

So the fix needs `mapin_user(initproc)` as well, in the right order, and
possibly the `bcopy` from `proc0` that `kern_fork` does first. That is
the next thing to try, and it should be tried as one change with the
stamping, not separately.

## Still not established

Whether any of this causes the SIGBUS. The users of the shared region
test `shared_enabled` before touching it, so a disabled region should
degrade to syscalls rather than fault. The fault address still moves
between runs (`0x776a0`, `0x8bad0`, `0x51bad0`), which suggests
something uninitialised being used as a pointer rather than a fixed
wrong mapping -- consistent with, but not proof of, an uninitialised
shared region.

# NAILED: the trace bug was mine, and it exposed a real one

## The instrument was fine; my format string was wrong

`e_emulator_error()` is not broken. My probe was:

```c
e_emulator_error("... off=%x prot=%x max=%x",
                 ..., section->offset, section->prot, section->maxprot);
```

and `struct exec_section`'s `offset` is declared `off_t`, which in this
tree is

```c
include/sys/types.h:74:  typedef quad_t off_t;   /* file offset */
```

**eight bytes.** The `%x` conversion does `va_arg(adx, unsigned int)`
and consumes four, so the va_list desynchronises and every argument
after it shifts by one. That is precisely the observed `off=0`,
`prot=0`, `max=5`: `prot` read `offset`'s high half and `max` read
`prot`.

So the earlier "prot=0" finding, the "one-field shift", and the
suspicion that the emulator's varargs are broken were all one mistake --
mine -- and the section protections were correct from the start.

**The rule stands, for a better reason than I gave it.** One argument
per call is not a workaround for a broken printf; it is how to avoid
writing a format string that silently misreads a 64-bit argument. This
printf has no `default:` case in its conversion switch either, so an
unrecognised conversion consumes nothing and shifts everything after it
the same way.

## The real bug it exposed: the first program's shared region is never versioned

Because `e_emulator_error` is sound, this message is trustworthy and was
wrongly doubted:

```
emulator [1] shared region mismatch b/1
```

It is a two-argument call with two `int`s, so it reports exactly what it
says: `us_version` is `0xb` where `USHARED_VERSION` is 1.

`us_version` is assigned in exactly one place in the whole tree:

```
server/serv/serv_fork.c:415:  p2->p_shared_ro->us_version = USHARED_VERSION;
```

**Only on fork.** The first program is `exec`'d, never forked, so its
shared region is never initialised and `us_version` holds whatever the
page contained. `emul_mapped.c` then refuses it:

```c
if (shared_base_ro->us_version != USHARED_VERSION) {
	e_emulator_error("shared region mismatch %x/%x", ...);
	return;			/* shared_enabled stays 0 */
}
```

so `shared_enabled` remains 0 for the one process that has no parent to
inherit a good region from.

## Next

Whether that causes the SIGBUS is not yet established -- the users of
the shared region test `shared_enabled` before touching it, so a
disabled region should degrade to syscalls rather than fault. But it is
a genuine defect on the exec path, it affects exactly the process that
is failing, and it is the first thing in this investigation that is both
real and unexplained.

Find where the shared region is set up for `exec` as opposed to `fork`,
and whether anything reaches `shared_base_rw` without checking
`shared_enabled` first.

# CORRECTION: the protections are fine. The instrument was lying.

The previous entry concluded that `/sbin/init`'s sections are mapped
with `prot = 0` and that the struct is read shifted by one field. **Both
are wrong.** Re-probing with one argument per `e_emulator_error()` call:

| probe | value |
|---|---|
| `ap: s0.prot` (after `parse_exec_file`) | **5** |
| `ap: s0.maxprot` | **7** |
| `ms: va=1000, prot` (in `map_section`) | **5** |
| `ms: va=1d000, prot` | **3** |
| `ms: va=1e000, prot` | **3** |
| `sizeof(struct exec_section)` | `0x2c`, identical at both points |

`prot` is 5, 3, 3 and `maxprot` is 7 throughout, exactly as
`exec_file.c` sets them. **There is no shift, and the mapping is
correct.**

## The real finding: e_emulator_error corrupts multi-argument calls

The earlier `prot=0 max=5` reading came from a six-argument
`e_emulator_error()`. The same values printed one per call are right. So
that function mangles its arguments beyond the first.

**This has been corrupting diagnostics throughout.** Anything printed by
a multi-argument `e_emulator_error()` is suspect, including:

```
emulator [1] shared region mismatch b/1
```

which is a two-argument call, so the claim that `us_version` is `0xb`
against an expected `1` may itself be wrong.

It is not the obvious cause. `e_bsd.c:2875` uses proper ANSI varargs --
`va_list adx; va_start(adx, fmt); va_arg(adx, ...)` -- and includes
`<machine/stdarg.h>`, which resolves to the `include/i386/stdarg.h` this
project already repaired. So the fix is elsewhere: possibly the mix of
`putchar()` and buffered output in that function, or a difference in how
the emulator directory is compiled. A controlled test -- a call with
known constant arguments -- will settle it without guessing.

## What this means for the fault

**The cause of init's SIGBUS is still unknown.** The one solid
measurement that survives is the exception itself, which was traced with
single-argument prints and is therefore trustworthy:

```
exception: exc=1        EXC_BAD_ACCESS
exception: code=776a0
exception: subcode=2    KERN_PROTECTION_FAILURE
exception: signal=10    SIGBUS
```

A protection failure at an address past the end of the image, at a
location that moved between runs. With the section protections now known
to be correct, that address is not in the program's own image, so the
question becomes what else is mapped there and who touches it.

## Method note

This is the second time in this investigation that a multi-argument
trace produced a confident, wrong answer, and the second time that
re-printing one value per call settled it. **For anything printed by
LITES or its emulator, one argument per call is the only form to
trust** until the varargs defect is found and fixed.

# The fault: sections are mapped with prot=0

Traced to the mapping step. `emul_exec_map_section()` receives every
section of `/sbin/init` with **`prot = 0`**, i.e. `VM_PROT_NONE`:

```
map_section: how=1 va=1000  size=1c000 off=0     prot=0 max=5
map_section: how=1 va=1d000 size=1000  off=1c000 prot=0 max=3
map_section: how=3 va=1e000 size=2000  off=0     prot=0 max=3
```

`max` is right -- 5 is `READ|EXECUTE` for text, 3 is `READ|WRITE` for
data and bss -- but the current protection is none, so any access
faults. That matches the symptom exactly: `EXC_BAD_ACCESS` with
`KERN_PROTECTION_FAILURE` (the page exists, the permission is wrong),
and an address that moves between runs because it is wherever the
program happens to touch first.

## parse_exec_file fills it correctly

Probing immediately after `parse_exec_file()` returns, before anything
else touches the array:

```
after parse: bt=7 s0.prot=5 s0.max=7 s1.prot=3 s2.prot=3
```

So liblites sets `prot` to 5, 3, 3 and `maxprot` to 7, exactly as
`exec_file.c` lines 372-382 and the `case BT_NETBSD` branch intend.

**Between `parse_exec_file()` returning and `emul_exec_map_section()`
reading, `prot` becomes 0 and `maxprot` becomes what `prot` held.** The
values shift by exactly one field.

## What has been ruled out

- **Not a duplicate struct.** `struct exec_section` is defined only in
  `include/sys/exec_file.h`. `emul_exec.c` does not include it directly
  but gets it through `e_defs.h`, which does.
- **Nothing writes `prot` in between.** The only assignment is
  `secs[i].file = image_port`, the field immediately *before* `prot` --
  suggestive, but a correct `mach_port_t` store cannot overrun into the
  next field.
- **Probably not the instrument.** `e_emulator_error()` uses proper
  ANSI varargs (`va_list`, `va_start(adx, fmt)`) and compiles against
  the `include/i386/stdarg.h` this project already fixed. Worth
  confirming, given that a broken printf would explain an apparent shift
  with no mechanism, and this code base has been bitten by exactly that
  before.

## The decisive next test

Re-probe inside `emul_exec_map_section()` with **one argument per
call**, as was done for the exception trace, which is what turned the
`code`/`subcode` confusion from guesswork into fact. That separates "the
struct really is being read shifted" from "the trace is lying", and
those need different fixes.

If the shift is real, the next suspect is a compilation flag difference
between `liblites` and `emulator` -- the two are built as separate
directories with different flags, and a packing or enum-size difference
would do exactly this.

# MILESTONE: a real NetBSD binary runs under LITES

```
emulator [1] emul_exec_open success: "/dev/boot_device/mach_servers/init" p=803 fd=-1 BT=7
emulator [1] emul_exec_start: starting at x1020 k=xbfffdff0 (x1 xbfffe000 x0 x0)
emulator [1] emul_syscall[24] e_getuid(xbfffdff4, xbfffdff0, x0, ...)
emulator [1] return[24] e_getuid = (x0 x1dbbc)
panic: init died
```

**NetBSD 1.0's `/sbin/init`, unmodified, loaded and executed a system
call.** Everything in the chain worked:

- **`BT=7`** is index 7 in `ATSYS_NAMES`: **`netbsd`**. Classified
  correctly, as predicted from `emul_exec.c`'s explicit MID_I386 test
- **entry `0x1020`** matches the a.out header decoded from the binary
- **`e_getuid` returned 0**, so init's root check passed

## How it was reached, without mach_init

`mach_init` was **not** needed. LITES's `-i` flag names an alternative
first program, and it is compiled in because our build has
`#define SECOND_SERVER 1`:

```
startup /mach_servers/startup -i /init hd0c
```

`init_program_path` resolves to `/dev/boot_device/mach_servers/init` in
LITES's own VFS on the ext2 root, so the binary and the emulator both go
there. `/bin/sh` and the device nodes live at their normal paths.

**This takes the a.out cross-toolchain off the critical path.**
`mach_init` cannot be linked without one -- binutils 2.42 has no a.out
target at all -- but it does not have to be, to reach a shell.

## Where it stops, and what was ruled out

`init` dies after exactly one syscall.

**The obvious suspect was investigated and is innocent.** `e_getuid`
writes only `rv[0]` and leaves `rv[1]` untouched, which looked like it
would hand the program a garbage euid in `edx`. It does not: the
trampoline initialises the pair from the caller's own registers before
dispatch:

```c
rval[0] = 0;
rval[1] = regs->edx;		/* preserve the caller's edx */
```

So the `x1dbbc` in the trace is whatever `edx` held when init made the
call, faithfully preserved -- deliberate, for syscalls that do not set a
second value. Nothing to fix there.

### What the absence of further syscalls tells us

NetBSD 1.0's `init` begins roughly:

```c
if (getuid() != 0)   errx(1, ...);     /* traced, returned 0 */
if (getpid() != 1)   errx(1, ...);     /* NOT traced */
if (setsid() < 0)    warn(...);        /* NOT traced */
```

`getpid` and `setsid` are ordinary syscalls and would appear in the
trace. Neither does, and `errx()` would itself call `write` and `exit`,
also absent. So **init never reached its second syscall**: it faulted
between the two rather than exiting deliberately.

Two candidates from the trace:

```
emulator [1] shared region mismatch b/1
emul_exec_start: starting at x1020 k=xbfffdff0 (x1 xbfffe000 x0 x0)
```

- **`envp` is 0.** Those four values are argc=1, argv=0xbfffe000,
  envp=0, and 0. A null environment pointer faults anything that walks
  it, and libc start-up code routinely does.
- **"shared region mismatch"** is the emulator complaining about its own
  shared region before the program starts at all. Unexplained so far.

## FOUND: init faults in heap space, SIGBUS

Adding a trace to `catch_exception_raise()` in
`server/serv/ux_exception.c` -- the one place where the cause is still
known, since `proc_died()` learns of the death through a Mach dead-name
notification that carries no reason -- gives:

```
emul_syscall[24] e_getuid(...)
exception: task 44be14 exc 1 code 489120 subcode 2 -> signal 10
panic: init died
```

`exc 1` is **`EXC_BAD_ACCESS`**, signal 10 is **SIGBUS**, and the
address is `489120` = **`0x776a0`**.

### Where that address is

Decoding `/sbin/init`'s a.out header: text 114688, data 4096, bss 8016,
entry `0x1020`. For QMAGIC that lays out as

| region | range |
|---|---|
| text | `0x1000` - `0x1d000` |
| data | `0x1d000` - `0x1e000` |
| bss | `0x1e000` - `0x1ff50` |
| **fault** | **`0x776a0`** |
| stack | `0xbfffe000` |

The fault is **well past the end of the image** and far below the
stack. That is heap space -- `sbrk`/`malloc` territory. So libc obtained
memory and then touched something that is not actually mapped.

### CORRECTION: it is a protection failure, and the arguments are swapped

The reading above -- unmapped heap -- is **wrong**, and the same trace
shows why. LITES's own converter treats `code` as the kern_return:

```c
case EXC_BAD_ACCESS:
    if (code == KERN_INVALID_ADDRESS)  *ux_signal = SIGSEGV;
    else                               *ux_signal = SIGBUS;
```

We saw `code = 489120`, which is not a kern_return at all, so this fell
through to SIGBUS by default. Meanwhile `subcode = 2` **is** a valid
kern_return: `KERN_PROTECTION_FAILURE`.

So the two arrive the other way round from what LITES expects:

| argument | LITES expects | what arrived |
|---|---|---|
| `code` | kern_return | **address `0x776a0`** |
| `subcode` | address | **`2`, `KERN_PROTECTION_FAILURE`** |

Two consequences.

**The fault is a protection failure on a mapped page**, not an access to
unmapped memory. Something wrote to a page it was not allowed to write,
or read one it could not read. The heap theory is out; the address is
still `0x776a0`, still past the image, but the page exists.

**And LITES's exception decoding is wrong against this kernel.** It
would report `SIGSEGV` as `SIGBUS` for every genuine invalid-address
fault, because the value it tests is never a kern_return. Here the
signal happened to come out right, since a protection failure maps to
SIGBUS anyway, but that is luck.

### The kernel sends them in the order LITES expects

Checked, and it does. `i386/trap.c:334`:

```c
i386_exception(EXC_BAD_ACCESS, kr, regs->cr2);
```

and `i386_exception` packs them:

```c
codes[0] = code;	/* the kern_return */
codes[1] = subcode;	/* cr2, the faulting address */
exception(exc, codes, 2);
```

So `codes[0]` is the kern_return and `codes[1]` the address, which is
exactly what LITES's `catch_exception_raise(..., code, subcode)`
expects.

### But that is not what arrives

Re-traced with one argument per `printf`, to rule out the variadic
formatting being at fault:

```
exception: exc=1
exception: code=776a0
exception: subcode=2
exception: signal=10
```

`code` holds an address and `subcode` holds `2`. A `kern_return_t`
cannot be `0x776a0`, so the two really are swapped somewhere between
`i386_exception()` and LITES.

**The emulator is the likely place.** It holds its own exception port --
that is how it delivers signals to the program it runs -- so a fault in
the program goes to the emulator first, which may re-raise it to LITES
with its own argument order. `emulator/i386/e_signal.c` is where to
look.

### What is solid regardless

Whichever field is which by convention, the two values are **the
faulting address `0x776a0`** and **`2`, `KERN_PROTECTION_FAILURE`**. So
the page exists and was touched with the wrong permission, and the
address is past the end of init's image (`0x1ff50`) and far below the
stack (`0xbfffe000`).

### Superseded: what the heap theory pointed at

The initial break. If the a.out loader sets the break to the wrong
place, libc's allocator believes it owns memory the kernel never mapped,
and the first write into it faults exactly like this. The break should
be the end of bss, `0x1ff50` rounded up.

It also explains the missing syscalls: the fault happens inside libc's
start-up before init reaches `getpid()`, and possibly before any
allocation syscall is issued at all, if the break was simply wrong from
the start rather than being moved.

**Next step:** find where the break is set for a.out binaries -- in the
emulator's exec path or LITES's `s_execve` -- and compare it with
`0x1ff50`.

### Superseded: print the exit reason in `proc_died()`, which currently
panics on pid 1 without reporting a status and so throws away the one
piece of information that would say whether this is a signal and which
one. Raising `syscall_debug` at the same time would confirm no further
calls are simply going untraced.

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

# MILESTONE: LITES runs on OSFMK 7.3

```
Lites VERSION(Lites.1.1.u3): Tue Sep 15 04:11:54 PM EDT 2026; STD+WS+osfmach3

Copyright (c) 1982, 1986, 1989, 1991, 1993
        The Regents of the University of California.
Copyright (c) 1992 Carnegie Mellon University.
Copyright (c) 1994, 1995 Johannes Helander (Helsinki University of Technology).
All rights reserved.
```

A 4.4BSD-Lite UNIX personality, loaded off a minix floppy by the OSF
bootstrap task, printing its banner on this kernel.

## The fix: the ELF entry point

LITES links with `-e __start`. This crt0 defines `__start_mach`. The
symbol does not exist, so `ld` says so and carries on:

```
ld: warning: cannot find entry symbol __start; defaulting to 08049000
```

`0x08049000` is the first byte of `.text`, which `nm` identifies as
`ip_setmoptions.cold` -- a cold-path fragment of a networking function.
Every previous boot jumped there and died instantly, before `crt0`,
before `main`, before any console. That is why there was never any
output and why nothing else we tried made any difference.

The fix is one linker flag:

```
LDFLAGS="... --defsym __start=__start_mach"
```

after which `readelf -h` reports entry `0x8049330`, which is
`__start_mach`. Checking that number is the cheapest possible
confirmation and should be done before any boot attempt.

The precedent was in our own build all along. OSFMK's `default_pager`
links with `-e __start_mach -u __start_mach`; that is how a server built
against this crt0 is meant to be linked.

## How it was found

A `-d exec` trace, with user-mode EIPs symbolised against `nm` on the
`startup` binary. The decisive number: **zero** instructions executed
above `0x08066228`, the bootstrap task's `etext`, and the highest
address reached in the whole run was `0x08065cd2`. Since LITES's text
runs to `0x080f709a`, it had plainly never executed its own code.

Four earlier hypotheses were each audited and disproved before this:
that `do_bootstrap_ports` was a stub, that a NULL `argv` was
dereferenced, that crt0's `else` branch skipped thread initialisation,
and that the loader failed to pass arguments. All wrong. The measurement
settled in one run what reading had not settled in four attempts.

## The blocker is the root filesystem, not paging

`init_main.c:349` onward, immediately after the banner:

```c
printf("%s\n", version);
printf(copyright);          /* <- last thing seen on the console */

/* Mount the root file system. */
kr = (*mountroot)();
#if EXT2FS
/* XXX if FFS fails, fall back to EXT2FS */
if (kr == EINVAL)
        kr = ext2_mountroot();
#endif
if (kr != KERN_SUCCESS)
        panic("cannot mount root x%x %s", kr, mach_error_string(kr));
```

The panic is the next statement after the copyright text, and the
copyright text is the last output. There is no other panic between them.
**LITES panics because it has no root filesystem.**

`panic: UWVS+` is that message mangled. `panic` does
`printf("panic: %r\n", fmt, ap)`, and `%r` -- a BSD extension that
re-expands a format string against a `va_list`, implemented at
`subr_prf.c:473` -- destroys the text while the panic itself is real and
correctly placed. Cosmetic, but it cost a detour and is worth fixing.

**The paging messages are a consequence, not the cause.** In the console
log the panic is line 48 and the pager complaints begin at line 49.
`panic()` calls `boot(TRUE, RB_AUTOBOOT|RB_DUMP)`, and `RB_DUMP` asks
for space to write a crash dump, which is what the pager cannot supply.
Reading `swapon suggested` as the blocker sent this investigation in the
wrong direction for a round; the line ordering said otherwise.

`-m 256` changes nothing -- same panic, same position -- which correctly
rules out memory pressure.

## ddb, the in-kernel debugger, works and is one flag away

OSFMK ships its own kernel debugger in `src/mach_kernel/ddb/`, and it is
compiled **out** of the PRODUCTION config:

```c
/* obj/at386/mach_kernel/PRODUCTION/mach_kdb.h */
#define MACH_KDB 0
```

`conf/AT386/config.debug` turns it on, and there is a ready-made config
that includes it:

```
options  MACH_KDB           /* the debugger */
options  MACH_TR            /* kernel tracing */
options  BOOTSTRAP_SYMBOLS  /* symbols for bootstrap-loaded servers */
```

```sh
sh build/ode.sh -here mach_kernel MACH_KERNEL_CONFIG=DEBUG
```

Builds clean, 1,586,388 bytes against PRODUCTION's 1,025,836, and gives
a prompt on the serial console:

```
inline call to debugger(machine_startup)
Stopped	at  0x1bc8bd:	int	$3
db8$>
```

### Why it is worth using

It is a better instrument than the gdb stub for this project, for three
reasons.

**It understands Mach's own types.** `db_task_thread.c` and
`db_print.c` give it tasks, threads, ports and VM maps as first-class
objects. The gdb stub sees only memory, which is why reading kernel
structures through it has meant hand-computing addresses all session.

**There is no attach window to miss.** It runs inside the guest, so the
timing problem that cost several attempts -- attach too early and the
task's pages are not mapped, too late and the task is gone -- does not
arise.

**`BOOTSTRAP_SYMBOLS` covers loaded servers**, so it can symbolise
LITES, not just the kernel.

### Which kernel to use, in plain terms

**PRODUCTION** is the kernel used so far. No debugger. It boots straight
through and everything goes to the log file, which is what you want when
you just need to see what happens. `tools/boot-ide.sh` runs this.

**DEBUG** is the same kernel with the debugger compiled in. It stops
early and shows a `db8$>` prompt on the console, where you type commands
to inspect the running machine. `tools/boot-debug.sh` runs this.

You cannot script the second, because it waits for you to type. You
cannot inspect the first, because there is nothing to type at. So keep
both and pick per question.

At the prompt: `c` continues booting, `trace` gives a backtrace,
`show all threads` lists every thread, `show all ports` lists Mach ports
-- which the gdb stub cannot do at all -- `examine <addr>` dumps memory,
`break <addr>` sets a breakpoint, `help` lists the rest. Ctrl-A then B
drops back into the debugger later; Ctrl-A then X quits QEMU.

### The catch, and the setup needed

It needs an **interactive** console. The current scripts use
`-serial file:` to capture output, which gives the debugger nowhere to
read from, so the DEBUG kernel stops at `machine_startup` and waits
forever. Using it means `-serial stdio`, `-serial mon:stdio`, or a pty,
and driving it by hand rather than from a script.

That is a different working style from the automated boots, so the
sensible arrangement is to keep PRODUCTION for scripted runs and switch
to DEBUG when a question needs poking at live kernel state.

# Two improvements found by comparing against DR3

Comparing our tree against the DR3 release subtree by subtree showed
almost everything byte-identical -- `stand`, `xkern`, `usr`, `tgdb`,
`osc`, `makedefs` and `default_pager` differ in **zero** files. Exactly
two files differ, and both are informative.

## 1. The bootstrap task can read ext2. mkminix.py may be unnecessary.

`file_systems/AT386/machdep.mk`:

```make
AT386_OFILES = ${UFS_OFILES} ${EXT2FS_OFILES} ${MINIXFS_OFILES} fs_switch.o
```

and `AT386/fs_switch.c` registers all three, tried in order:

```c
&ufs_ops,
&ext2fs_ops,
&minixfs_ops,
```

So `/mach_servers` does **not** have to be a minix volume. The i386
bootstrap task builds a UFS reader, an **ext2** reader and a minix
reader, and tries each in turn.

`tools/mkminix.py` exists because `mkfs.minix` was dropped from Debian
13 and the kernel's minix reader is picky about the magic number. If the
server volume were ext2 instead, it could be built with stock `mke2fs`
and populated with `debugfs` -- the same tools already used for the root
filesystem -- and all three disks would be one filesystem type.

Worth trying. `mkminix.py` works and is not urgent to replace, but this
removes a hand-written tool from the critical path and is one fewer
thing to be wrong.

## 2. Our kernel's ext2 reader handles `filetype`; LITES's does not

The one differing file in `file_systems` is `ext2fs/ext2_fs.h`:

```c
/* DR3 */                          /* ours (MkLinux) */
unsigned short name_len;           unsigned char  name_len;
                                   unsigned char  file_type;
```

MkLinux updated the **kernel-side** reader for the `filetype` feature.
**LITES's own reader was not updated** -- `server/ufs/ext2fs/ext2_fs.h`
still declares `__u16 name_len`, which is precisely why the root
filesystem has to be made with `-O ^filetype`.

So the two ext2 readers in this system disagree about the on-disk
format. The kernel's is modern; LITES's is not.

That means `-O ^filetype` is a workaround for a fixable defect, and the
fix is already written in our own tree: apply the same two-field split
to LITES's `ext2_fs.h` and teach `ext2_lookup` and `ext2_readdir` to
mask `name_len` to eight bits. Both readers are then consistent, and the
root filesystem can be made with stock `mke2fs` defaults.

Not urgent -- `^filetype` works -- but it is the correct fix rather than
an avoidance, and the reference for it is in-tree and permissively
licensed.

# Step 4: what the first program actually is

## The emulator works

```
emulator [1] emul_exec_open success: "/dev/boot_device/mach_servers/mach_init" p=803 fd=-1 BT=20
emulator [1] emul_exec_start: starting at x80615b0 k=xbfffdff0
```

With `libmach_sa` built, the emulator links, loads and **starts** a
program. The exec path works end to end. (The program here is
`default_pager` standing in as a placeholder, so the warnings that
follow are it making calls LITES does not expect -- not a fault.)

## mach_init is just the personality's first user program

MkLinux's `mach_init` binary is in the reference collection at
`new_release_kernel/mach_servers/mach_init`. It is PA-RISC, so not
directly usable, but it is **not stripped**, and its strings settle what
the program is:

```
/etc/init  /bin/init  /sbin/init  /etc/rc  /bin/sh  -/bin/sh
HOME=/     HOME=/usr/root
/dev/tty1  /dev/ttyS0
"Unable to open an initial console."
"Fork failed in mach_init"
```

That is **Linux's init sequence**, verbatim: open a console, try
`/etc/init`, `/bin/init`, `/sbin/init`, fall back to `/bin/sh`. Its
symbol table is 219 entries of which only `main` is its own; everything
else is statically linked glibc.

So `mach_init` is not a Mach-specific bootstrap program. It is **the
personality's first user program**, an ordinary statically linked C
program run under the emulator. The name is historical.

**This means we can write one.** It needs to be a static i386 binary
that the emulator recognises and that makes the personality's system
calls. LITES ships none -- its `bin/Makefile.in` has an empty `all:`
target.

## One thing to check first: binary type detection

`BT=20` in the trace above is the binary type index into `ATSYS_NAMES`
in `include/sys/exec_file.h`. Counting -- bad, lites x5, bnr, netbsd,
freebsd, ux, script, isc4, linux x3, ultrix, riscos, hpbsd, hpux,
hpkludge, hpelf, osf1 -- index 20 is **`hpelf`**, an HP-UX ELF binary.

The emulator classified an i386 ELF executable as HP-UX. That may be
harmless, or it may mean `guess_binary_type_from_header()` in
`liblites/exec_file.c` does not recognise this ELF properly -- worth
settling before building a first program that has to be classified
correctly. Note that file also contains

```c
switch ((tmp >> 16) && 0x3ff) {     /* && where & was meant */
```

which the compiler warns about as a boolean condition and which is
almost certainly a typo for `&`.

# Step 4: the emulator is the missing piece

## Correction: ext2 lookup works

The previous note concluded the fault was in `ext2_lookup`. **That was
wrong.** Instrumenting the lookup shows it resolving every component:

```
EXT2LK <dev>
EXT2LK <boot_device>
EXT2LK <mach_servers>
EXT2LK <mach_init>
EXT2LK <dev> ... <emulator>
EXT2LK <dev> ... <emulator.old>
```

and the `ENOENT` return inside `ext2_lookup` **never fires** -- a probe
on that path prints nothing. So the directories created with `debugfs`
are found, `mach_init` is found, and ext2 is working correctly.

## Where the ENOENT actually comes from

`s_execve` calls

```c
kr = server_exec(p, fname, emul_name, cfname, cfarg, &li_data, &image_port);
```

and `emul_name` is the **emulator**. The lookup sequence shows it:
LITES resolves the program, then `emulator`, then `emulator.old`. The
emulator file does not exist, so the exec fails.

The emulator is not optional. It is the syscall trampoline mapped into
every process, and LITES loads it alongside the program being exec'd.

## Why the emulator does not build

It compiles but does not link:

```
ecrt0.c:67:    undefined reference to `mach_init'
emul_init.c:298: undefined reference to `mach_init'
```

**This is a different `mach_init` from the init program** -- it is
libmach's runtime initialisation function, and the two share a name.

In this libmach it is **static**:

```c
/* mach_services/lib/libmach/mach_init.c */
static int mach_init(void);              /* :74  */
static int mach_init(void) { ... }       /* :93  */
int (*_mach_init_routine)(void) = mach_init;   /* :181 */
```

`nm` confirms it: `00000000 t mach_init`, a local symbol. It is reached
only through the `_mach_init_routine` function pointer, which is how
crt0 calls it. LITES's emulator calls it **by name**, so it expects a
libmach where the symbol is global.

That is a genuine interface mismatch between this OSFMK libmach and the
one LITES was written against, and it is the next thing to resolve. The
obvious options are to make the symbol global, or to give the emulator a
small shim that calls through `_mach_init_routine` instead.

## Superseded: ext2 mounts, but lookups return ENOENT

Measured, with instrumentation at the mount site:

```
MOUNT ffs  kr=c016      EINVAL -- not an FFS filesystem, as expected
MOUNT ext2 kr=0         success
EXEC FAILED path=/mach_servers/mach_init kr=c002
panic args: ... exec failed: xc002 (os/unix) file or directory does not exist
```

**ext2 is genuinely what mounted**, the root vnode is established
(`VFS_ROOT` returns without the panic that follows it firing), and yet
`namei` returns ENOENT for a file that demonstrably exists. Verified
with `debugfs`:

```
$ debugfs -R "ls -l /mach_servers" root.img
   19  100755 (0)  0  0  211100  mach_init
```

Tried at three different paths, all present in the image, all ENOENT:
`/dev/boot_device/mach_servers/mach_init`, `/mach_init`, and
`/mach_servers/mach_init`.

So the fault is in **ext2 directory lookup** -- `ext2_lookup` in
`server/ufs/ext2fs/ext2_lookup.c` -- which is a different code path from
the directory *read* that the earlier `^filetype` fix repaired. That fix
made the root directory parseable; this is about finding a named entry
within it.

## Useful things learned getting here

**`server_dir` must start with `/dev/`.** LITES says so itself:

```
(lites): server_dir(/) ignored.  It does not start with /dev/
(lites): init_program(/mach_servers/mach_init)
```

so passing `/` as the second argument falls back to `/mach_servers`,
which is at least a short path to test against.

**The panic arguments now expand**, confirming the varargs fix end to
end:

```
panic args: first program (/mach_servers/mach_init) exec failed:
            xc002 (os/unix) file or directory does not exist
```

**Mach error encoding is `0xc000 + errno`**: `0xc002` ENOENT, `0xc016`
EINVAL.

**A correction.** An earlier note here said that placing a known-good
binary as `mach_init` showed the loader working, because a second
`default_pager` complained another existed. That was wrong: exec
returned ENOENT, so nothing was loaded, and those messages came from the
real pager. There is still no evidence either way about whether
`s_execve` can load a binary -- the lookup fails first.

## Superseded: the init program, and where it must live

LITES mounts the root and then fails to exec its first program:

```
EXEC FAILED path=/dev/boot_device/mach_servers/mach_init kr=c002
panic: first program (%s) exec failed: x%x %s
panic: init died
```

`0xc002` is `0xc000 + 2`, **ENOENT**. (The same encoding gives `0xc016`
for EINVAL, 22, seen earlier.)

## The path is wrong, not the loader

Putting a known-good Mach binary on the server volume as `mach_init`
showed the loader itself works: a second `default_pager` instance
started and complained that another already existed. So `s_execve`
loads fine. It simply cannot find the file.

`/dev/boot_device/mach_servers/mach_init` is a name the **bootstrap
task** understands -- an indirection the kernel sets up from
`BOOTDEV`/`BOOTUNIT`/`BOOTPART`. LITES resolves paths through **its own
VFS**, which has the ext2 root mounted and knows nothing called
`/dev/boot_device`.

So the init program must live on the **ext2 root filesystem**, at a path
LITES can resolve, and `init_program_path` must point there.

`server_init.c` builds that path from `server_dir` and
`init_program_name` (`"/mach_init"`), and `parse_arguments` will take an
alternative from `argv[1]`, so this is configurable without code changes
once there is something to point at.

## Populating an ext2 image without root: debugfs

`mkminix.py` exists because the minix volume had to be built by hand.
Nothing equivalent is needed for ext2: **`debugfs` writes into an image
without mounting it and without privileges.**

```sh
debugfs -w -R "write localfile pathname" root.img
debugfs -w -R "mkdir /sbin"             root.img
debugfs -w -R "symlink /bin/sh /sbin/sh" root.img
debugfs    -R "ls -l /"                 root.img
```

Verified: writing a file and listing the directory both work as an
ordinary user. It lives in `/sbin`, which is not on a normal user's
PATH.

## What is still needed

`mach_init` itself. It is **not** in this tree -- neither OSFMK's `src/`
nor LITES has it; only `server/serv/mach_init_ports.c`, which is
unrelated. It came from the Mach 3.0 userland distribution.

Options, in increasing order of work:

1. **Point `init_program_name` at something else** that LITES can exec.
   Cheapest, and the mechanism already exists via `argv[1]`.
2. **Write a minimal first program.** It has to satisfy whatever
   `s_execve` and `server_exec.c:437` ("Duplicate work of the emulator.
   For first program loading") expect, which is the next thing to read.
3. **Find a Mach 3.0 userland distribution** with `mach_init` in it.

The emulator matters here too: `emulator_path` points at
`/dev/boot_device/mach_servers/emulator`, which has the same
resolution problem, and LITES's `emulator/` directory does build.

# MILESTONE: the root filesystem mounts AND reads

```
(lites): server_dir(/dev/boot_device/mach_servers) on root.
(lites): init_program(/dev/boot_device/mach_servers/mach_init)
panic: first program (%s) exec failed: x%x %s
panic: init died
```

**Roadmap step 3 is complete.** The ext2 filesystem mounts, the root
directory is read successfully, and LITES fails only because there is no
init program on it to exec. That is step 4.

## The last piece: mke2fs's filetype feature

```
bad directory entry: reclen is too small for name_len
offset=0, inode=2, rec_len=12, name_len=513
/: bad dir ino 2 at offset 0: mangled entry
```

`513` is `0x0201`. In ext2 revision 0 a directory entry's `name_len` is
a 16-bit field; the **filetype** feature splits it into an 8-bit
`name_len` and an 8-bit `file_type`, and `mke2fs` enables it by default.
So the `"."` entry -- `name_len` 1, `file_type` 2 for a directory --
reads as `name_len` 0x0201 to a 1995 reader, and the directory is
rejected.

`-O ^filetype` fixes it. The full set now used:

```sh
mke2fs -q -F -b 1024 \
    -O ^resize_inode,^dir_index,^ext_attr,^sparse_super,^filetype \
    -I 128 root.img
```

**Do not also pass `-r 0`.** Forcing revision 0 changes inode-size
handling and the mount then fails with EINVAL (`0xc016`). Tested both
ways.

This is the third instance of the same shape in this project, after the
minix magic number and the CHS geometry: a 1995 reader meeting a modern
formatter's defaults. The lesson each time is to turn the modern
features off rather than to teach the old reader about them.

## Superseded: the root filesystem mounts

```
panic: bad dir
panic: first program (%s) exec failed: x%x %s
panic: init died
```

`cannot mount root` is gone. LITES mounted the ext2 filesystem on
`hd0c` and went looking for `/sbin/init`, which an empty filesystem does
not have. **Roadmap step 3 is complete**, and the failures above are
step 4.

## The fix was a config line, not code

LITES gets its root device as an **argument**, and that is the only way
it gets one.

`get_config_info()` has two paths:

```c
if (argc) { parse_arguments(argc, argv); return; }   /* path A */
...
parse_arguments(4, foo_argv);                        /* path C, uses argv_space */
```

A server here always has at least one argument -- its own name, because
`bootstrap.conf` names it -- so `argc` is 1, **path A is always taken,
and `argv_space` is dead code.** Then `parse_arguments` does:

```c
pname = argv[0]; argv++, argc--;    /* argc becomes 0 */
if (argc == 0) return;              /* returns at once */
```

leaving `rootdev` at its uninitialised zero: `major 0 minor 0`, which is
device `hd`, unit 0, partition `a`. LITES asks the kernel for `hd0a`,
which does not exist on an unpartitioned disk, and the open fails with
`D_NO_SUCH_DEVICE`.

Naming the device in `bootstrap.conf` makes `argc` 2, so
`parse_arguments` reaches the end and sets `rootdev`:

```
startup startup hd0c
```

Measured before and after:

```
DBG rootdev=0 reached=0 name=<>      gci argc=1 path=A
DBG rootdev=2 reached=1 name=<hd0c>  gci argc=2 path=A
```

**The earlier `argv_space` patch changed nothing**, because that table
is only read on a path that never executes. It is left in place as
correct-but-unused, and the comment there now says so.

### How this was found

Printing from `get_config_info` produced nothing, because it runs before
the console is up. Capturing the values into globals and printing them
after the banner worked. That trick -- **record early state, report it
once output exists** -- is worth remembering for anything that runs
before a console.

## FIXED: LITES's varargs were pre-ANSI; every printf argument was garbage

```
panic args: cannot mount root x9c6 unknown error code
```

A readable panic message with its arguments expanded, for the first
time. The cause was `include/i386/stdarg.h`:

```c
typedef char *va_list;
#define va_start(ap, last) (ap = ((char *)&(last) + __va_promote(last)))
```

Computing the argument pointer by taking the address of the last named
parameter and stepping past it assumes a stack layout the compiler is
not obliged to provide. Modern GCC at `-O2` does not provide it, so
**named parameters read correctly while every variadic argument was
garbage**. It now uses `__builtin_va_list`, `__builtin_va_start`,
`__builtin_va_arg` and `__builtin_va_end`.

### Why this took so long to find

The symptom pointed away from the cause at every step. `fmt` was
verifiably correct -- a hardware breakpoint at `panic` showed
`0x080ed7e6`, which `readelf` confirms is the right string in
`.rodata` -- and yet `printf("%s", fmt)` printed the bytes of an
unrelated function. That combination looks impossible, and six
hypotheses were built trying to explain it: `%r` mangling, unmapped
rodata, partially mapped text, paging pressure, absent backing store,
and a wild `fmt` pointer.

The measurement that broke it open was printing a literal with **no**
arguments from inside `panic`. It printed perfectly, which proved
`printf` worked and narrowed the fault to argument passing rather than
output or pointers.

**A correct value and a corrupt one can coexist** when the corruption is
in the mechanism that transports the value rather than in the value
itself. When evidence looks contradictory, suspect the transport.

### What it unblocks

The panic text is now readable, so every future failure names itself.
The remaining blocker is the one the message states: `x9c6` is
`D_NO_SUCH_DEVICE` on the root mount. "unknown error code" is
`mach_error_string` not knowing the device subsystem, which is cosmetic.

## Measured: the development sandbox boots this in 2 seconds

Not a claim, a measurement. Kernel, bootstrap task and `default_pager`
built in the sandbox and booted there under **pure TCG on one CPU**,
with no hardware acceleration:

```
(bootstrap): loading /dev/boot_device/mach_servers/default_pager
(bootstrap): started
(default_pager): started
```

reached in **2 seconds**.

That is faster than the KVM floppy boot by two orders of magnitude, and
it settles the question the other way round from how it was framed all
session. Emulation speed was never the constraint. The floppy was. A
boot that cost 500 seconds with hardware acceleration costs 2 without
it, once the same data comes off an IDE disk.

Two beliefs shaped this session and both were wrong:

- that 32-bit objects could not be linked in the sandbox
  (`gcc-multilib` installed in one command)
- that emulation without KVM was too slow to iterate on
  (it is 2 seconds)

Neither was ever tested. Both were inferred from a single early failure
and then treated as fixed properties of the world.

### Build notes for the sandbox

The full sequence, after `gcc-multilib`:

```sh
export MK_BUILD=/tmp/hj ODE4LINUX=~/ode4linux
sh build/ode.sh MAKEFILE_PASS=FIRST
sh build/ode.sh -here mach_kernel MACH_KERNEL_CONFIG=PRODUCTION
sh build/ode.sh -here mach_services/lib/libsa_mach
sh build/ode.sh -here mach_services/lib/libcthreads
sh build/ode.sh -here mach_services/lib/libmach
sh build/ode.sh -here mach_services/lib/libmach_maxonstack
sh build/ode.sh -here file_systems          # libsa_fs, needed by bootstrap
sh build/ode.sh -here bootstrap
sh build/ode.sh -here default_pager
```

One trap: the exported `mach/default_pager_object.h` can be installed
without the `import <mach/default_pager_types.h>` line that MIG emits,
and then `default_pager` fails with
`DEFAULT_PAGER_BACKING_STORE_MAXPRI undeclared`. The generated copy
under `obj/at386/default_pager/mach/` has it; copying that over the
exported one fixes the build. `-I.` does not help, because it precedes
`-I-` and so serves only `""` includes, not `<>` ones.

## The development sandbox can build and run this after all

`gcc-multilib` was installable the whole time. The belief that 32-bit
objects could not be linked in the sandbox was formed early from a
single failure, never rechecked, and shaped the entire session: every
LITES build and every boot was handed back and forth instead of being
run where the analysis was happening.

```sh
apt-get install -y --no-install-recommends gcc-multilib
gcc -m32 -print-libgcc-file-name    # .../13/32/libgcc.a
```

Verified by compiling, linking and running a 32-bit binary that uses the
64-bit division helpers LITES needs.

Combined with the 20-second IDE boot, the whole cycle -- build LITES,
build the server volume, boot, read the console -- can now run in one
place.

## The boot cycle is now 20 seconds, down from ~500

Booting `/mach_servers` from IDE works end to end: pager loaded, LITES
loaded, banner printed, `added device hd1c`, all in **20 seconds**
against roughly 500 from floppy. Every future measurement is 25x
cheaper, which changes what is worth attempting -- an experiment that
costs twenty seconds can be run on a hunch, where one costing ten
minutes cannot.

Run it with:

```sh
export MK_BUILD=~/.cache/mk7.3
sh tools/boot-ide.sh
```

## Booting the servers from IDE

`tools/boot-ide.sh` boots `/mach_servers` from a minix volume on a third
IDE disk instead of the floppy, which takes the cycle from about 500
seconds to a few. The floppy read was I/O bound, so hardware
acceleration never helped it.

```
hd0  ext2   LITES root
hd1  raw    paging, given to default_pager as hd1c
hd2  minix  /mach_servers, booted from
```

`-append "-r BOOTDEV=hd BOOTUNIT=2 BOOTPART=2 -o"`. `model_dep.c` joins
`BOOTDEV` and `BOOTUNIT`, looks the name up, and adds `BOOTPART` to the
unit; `dev_name_lookup` computes `unit * d_subdev + partition`, and
`d_subdev` is 16 for `hd`, so this is minor 34 -- unit 2, partition `c`,
the whole-disk fallback.

The minix reader needed no change: every access in
`file_systems/minixfs/minixfs.c` goes through
`device_read(fp->f_dev.dev_port, ...)`, so it is device-agnostic.

### Two problems this surfaced

**Names longer than 14 bytes.** A minix v1 directory entry holds 14, and
LITES's binary name is 43. The manual flow had always copied it to
`/tmp/startup` first. `mkminix.py` now takes `path:name=args` and
renames on the way in, which keeps the directory entry and the generated
`bootstrap.conf` in step by construction.

**The filesystem was always 1.44 MB.** `nzones` was hardcoded to 1440
and `bytearray(nzones * BS)` replaced whatever the image had been, so a
16 MB disk image came back out as a 1.44 MB file. On a floppy that was
invisible. On a disk it is not: the `hd` driver takes its geometry from
IDENTIFY, so the kernel believes the disk is its full size, and reads
past the end of a shorter backing file fail. The bootstrap task reported

```
(bootstrap): unloadable file format (result = 0x9c6)
```

-- `D_NO_SUCH_DEVICE` again -- while loading the 995 KB server, having
loaded the 211 KB pager without trouble. `mkminix.py` now sizes the
filesystem from the image file, capped at 65535 zones since minix v1
zone numbers are 16-bit.

## Superseded: LITES asks for hd0a

`kr = 0x9c6` is **2502 = `D_NO_SUCH_DEVICE`**. A hardware breakpoint on
`device_open`, printing the name at each call, showed what LITES
actually asks the kernel for:

```
"console"  "time"  "console0"  "hd0a"
```

**`hd0a`, not `hd0c`.** Partition `a` does not exist on an unpartitioned
disk -- `getvtoc` falls back to making partition `c` the whole disk --
so `hdopen` refuses and the root mount fails.

The `default_root[] = "hd0c"` patch was real but irrelevant on this
path. `server_init.c:711` holds a **second** compiled-in configuration:

```c
char argv_space[10][40] = {"/dev/hd0f/mach_servers/startup",
                           "-s",
                           "hd0a",                      /* the root device */
                           "/dev/hd0a/mach_servers",
                           (char *)0,};
...
parse_arguments(4, foo_argv);   /* XXX */
```

`get_config_info()` hands these to `parse_arguments` as **argc 4**, so
these four strings are the configuration and `default_root` is never
consulted. That also explains two older puzzles: `parse_arguments`'
`if (argc == 0) return` guard never fires, and `-s` is already applied
from `argv_space[1]` rather than from the `bootstrap.conf` attempt.

The patch series now sets `hd0c` in both entries. `hd0f` in entry 0 is
left alone: it only derives a path when none is given, and entry 3
supplies one.

### How this was found, and why it is worth noting

Four steps, each a measurement rather than a hypothesis:

1. `hbreak` at `panic`, read the stack -> `fmt` is valid, `kr = 0x9c6`
2. decode `0x9c6` -> `D_NO_SUCH_DEVICE`
3. `hbreak` at `device_open`, print the name at each call -> `hd0a`
4. `grep` for `hd0a` -> a second hardcoded table

**Each step was a measurement rather than a hypothesis, which is why it
took four steps instead of the six failed rounds before it.** Those six
-- `%r` mangling, unmapped rodata, partial text mapping, paging
pressure, absent backing store, and a wild `fmt` pointer -- were each
plausible, each argued from evidence already in hand, and each wrong.
The difference was not cleverness. It was that steps 1 to 4 each
produced a new fact, and the six rounds before them each produced a new
interpretation of the same facts.

## Superseded: the panic is "cannot mount root"

A hardware breakpoint at `panic` caught the call with LITES current:

```
eip 0x80aa350   esp 0x4fe78
0x4fe78:  0x08069a1a  0x080ef7e6  0x000009c6  0x080f16ad
          return       fmt         arg1        arg2
0x80ef7e6: "cannot mount root x%x %s"
```

**`fmt` is perfectly valid.** It points at the right string, in
`.rodata`, fully readable, with the arguments behind it. The panic is
the one in `init_main.c`, and `kr` is `0x9c6`.

**So `printf` is the broken thing, not the pointer.**
`printf("panic: %s\n", fmt)` printed `UWVS1+` while `fmt` pointed at
correct text. Six hypotheses were built on reading that garbage as
evidence about the pointer; the pointer was never wrong. The `-z
muldefs` collision between LITES's `printf` in `server/kern/subr_prf.c`
and `libsa_mach`'s is the only remaining explanation, and it is now
confirmed by elimination rather than assumed.

That also retires the `UWVS` analysis. `fmt` never pointed at code; the
`UWVS` text was produced by a `printf` reading from somewhere other than
its argument.

### The real blocker: error 0x9c6 from the root mount

`0x9c6` is 2502 decimal. It is a **Mach** error code, not an errno, so
the earlier reasoning about `EIO` versus `EINVAL` was about the wrong
kind of value entirely -- `(*mountroot)()` is returning a Mach error
from the device layer, not a BSD errno from a filesystem check.

Decode it before anything else. `mach_error_string` was already called
on it, and its result is on the stack at `0x080f16ad`, so the text is
available in the failing image.

### Method notes

**`hbreak`, not `break`, for a task that is not current.** A software
breakpoint must write `int3` into the target page, so it can only be set
while that page is mapped in the current context -- for a user task,
only while that task is scheduled. A hardware breakpoint uses the CPU's
debug registers and needs no memory access at all. In a microkernel,
where the task of interest is one of several and rarely current when you
attach, **`hbreak` is the default choice and `break` is the special
case.** Several attempts were lost to this.

**A broken instrument corrupted six rounds of reasoning.** The garbage
string was treated as data about the program. It was data about
`printf`. Fixing the instrument first -- which was attempted, but with
`%s` through the same broken `printf` -- would have needed an
independent output path to be conclusive.

## Superseded: the pager has a backing store

```
(default_pager): added device hd1c
```

The argument chain works end to end for the first time:
`bootstrap.conf` -> the bootstrap task -> crt0's `bootstrap_arguments()`
RPC -> `main(argc, argv)` -> `bs_add_device()`. The pager now has 32 MB
of backing store on a second IDE disk.

**`ps_allocate_cluster` is gone.** Every previous boot ended with four of
those messages; this one has none.

**The LITES panic is unchanged** -- still `UWVS1+`, in the same place.
That is now a clean result rather than a disappointing one: paging was
never implicated, and the last confounding symptom has been removed.

### What remains

`fmt` is a **code address passed where a format string was expected**.
`UWVS` is `55 57 56 53`, the i386 prologue
`push ebp; push edi; push esi; push ebx`, which `strings` renders as
text; it appears at thousands of offsets in the binary because every
function starts with it. The trailing bytes differ between builds
because the following instructions shift.

So some caller reaches `panic` with a pointer to code in the format
argument. Candidates, none yet tested:

- a call through a function pointer where the callee's signature
  differs from the caller's expectation
- an argument list misaligned by one slot, so a code pointer lands where
  `fmt` should be
- a `panic` reached from library code -- `libsa_mach` and `libmach` both
  provide one, and `-z muldefs` resolves the clash by link order, so a
  library caller may be reaching LITES's `panic` with different
  conventions

The measurement that settles it is the return address on the stack at
`panic` (`0x080aa350`), which `nm` turns into the calling function.
Getting it needs gdb attached **after** LITES starts and **before** it
panics; attaching earlier finds kernel space, and attaching later finds
the task gone.

## Superseded: how a server gets arguments

The chain is now mapped end to end, and every link was read in source:

```
bootstrap.conf              lines are  [-flags] symtab_name path [args...]
  -> bootstrap task         parse_config_file() stores per-server argv
  -> crt0 __get_arguments() calls bootstrap_arguments() over IPC
  -> main(argc, argv)
  -> default_pager          bs_add_device(*argv, master_device_port)
```

`default_pager`'s `main()` loops `while (--argc > 0)` calling
`bs_add_device()` on each non-flag argument. With `argc == 0` the loop
never runs, **so it starts with no paging segment at all** and every
`ps_allocate_cluster()` fails. That is the message we have been seeing
since the first LITES boot.

The zero-filled stack servers start on is deliberate (a "dummy 0
argument count"); real arguments arrive later, by RPC, from
`bootstrap.conf`. So giving the pager a disk needs **no code change** --
only a config line.

**It also explains why `-s` never reached LITES.** `parse_boot_args()`
consumes leading `-X` flags as the *bootstrap task's* own options
(`-k`, `-S`, `-w` ...) and does not pass them on. A server flag written
at the start of a config line is silently eaten.

`tools/mkminix.py` now accepts `path=args`:

```sh
python3 tools/mkminix.py /tmp/minix.img \
    $MK_BUILD/obj/at386/default_pager/default_pager=hd1c \
    /tmp/startup
```

writing `default_pager default_pager hd1c`.

**`hd1c`, not `hd0c`**: `hd0c` is the whole first disk and will hold the
root filesystem, so paging there would destroy it. The paging device
must be a second disk, attached as `-drive ...,if=ide,index=1`.

## Superseded: four hypotheses dead

The LITES panic after the banner has now survived four explanations,
each killed by measurement. Recording them so nobody retries them:

| hypothesis | how it died |
|---|---|
| `%r` mangles the message | `printf("panic: %s", fmt)` prints the same garbage -- `%r` is not involved |
| `.rodata` is not mapped | segment arithmetic checks out; `text_size` covers rodata exactly, and the contiguous path is taken |
| LITES's text is partly mapped | the unreadable addresses are simply not yet demand-paged; readable ones are those already executed |
| paging pressure | 128 MB behaves identically to 64 MB -- same panic, same messages, log byte-identical in length |

### What the garbage actually is

`strings` on the binary shows `UWVS` at thousands of offsets. It is not
a string: `55 57 56 53` is the i386 function prologue
`push ebp; push edi; push esi; push ebx`. So `fmt` points **into code**,
at or near a function entry. The varying suffix between builds
(`UWVS+`, `UWVS\002k`, `UWVS1+`) is just the following instruction bytes
shifting as the binary changes.

`fmt` is therefore a **code address passed where a format string was
expected** -- not corruption, but a wrong pointer.

### Two facts established along the way

**The kernel does not boot with 512 MB.** It prints
`cnvmem: 639 KB, extmem: 523136 KB, mem_size 523772 KB` and stops --
no `Kernel virtual space` line follows. 64 MB and 128 MB both work, so
the limit is between 128 MB and 512 MB. Likely `vm_page_bootstrap` or
`pmap` not scaling. Worth a separate investigation; for now, stay at or
below 128 MB.

**Paging pressure is not the constraint.** Doubling memory changed
nothing, so `ps_allocate_cluster: no space in available paging segments`
is not about a *small* backing store. It is about there being **none**.
Nothing in this configuration ever calls
`default_pager_add_segment` or `default_pager_backing_store_create` with
a real device, so the pager starts with zero paging segments and fails
the first time anything needs one.

### Next

Give `default_pager` a backing store. This is required work regardless
of whether it is the current blocker -- a Mach system with no swap
cannot page anonymous memory at all, and the IDE disk is now working
well enough to serve as one.

## Superseded: the panic message is garbage

The whole LITES tree builds -- server, ext2, emulator. The boot still
panics after the banner, and `ext2_mountroot` is now present in the
binary (`nm` confirms it), yet no ext2 diagnostic appears.

**The panic text changed between builds:**

```
UWVS+        (earlier build)
UWVS\002k     (this build)
```

Identical boots, different text. So it is not a message at all --
`%r` is printing whatever memory it lands on. Every earlier inference
from `UWVS+` being "stable across runs" was wrong: it was stable because
the binary was unchanged, not because the text was real.

That matters because **several panics are reachable at this point in
boot** -- `"cannot mount root"` in `init_main.c`,
`"ffs_mountroot: can't setup bdevvp's"` in `ffs_vfsops.c`, and others --
and without readable text there is no way to tell which fired. The
unreadable message has been obscuring the diagnosis for several rounds.

### Why `%r` misbehaves

`panic` does `printf("panic: %r\n", fmt, ap)`, where `%r` re-expands a
format string against a `va_list`. Two `printf` implementations are
linked into this server -- LITES's own in `server/kern/subr_prf.c` and
`libsa_mach`'s -- and `-z muldefs` resolves the clash by link order, so
which one handles `%r` is not obvious and may not be the one that
implements it.

### The fix

Rather than untangle that, `panic` now prints the format string with
`%s` first and then attempts `%r` separately:

```c
printf("panic: %s\n", fmt);
va_start(ap, fmt);
printf("panic args: %r\n", fmt, ap);
va_end(ap);
```

The first line always identifies which panic fired. The second still
shows the arguments when `%r` works, and is harmless when it does not.

An unformatted message that names the panic is worth more than a
formatted one that cannot be read.

## Superseded: the server builds, emulator string literals

Every error is now in `emulator/`, which has never been built in this
project. **`server/` is complete, including ext2.**

`include/sys/exec_file.h` builds a table of binary-type names:

```c
#define ATSYS_NAMES(m) \
    m ## "bad", m ## "lites", ...
```

called as `ATSYS_NAMES("i386_")`. The `##` operator pastes
*preprocessing tokens*, and two string literals cannot be pasted into
one:

```
error: pasting ""i386_"" and ""bad"" does not give a valid
       preprocessing token
```

Older preprocessors tolerated it. None is needed: **adjacent string
literals are concatenated by the compiler**, so removing `##` gives
`"i386_" "bad"`, which is `"i386_bad"` -- exactly the intent. Verified
by inspecting the preprocessor output.

## Superseded: `#if linux`

With the asm fixed, `ext2_linux_ialloc.c` failed on Linux kernel idioms:

```
error: 'struct inode' has no member named 'i_sb'
error: 'struct inode' has no member named 'u'
error: too few arguments to function 'bread'
error: too many arguments to function 'mark_buffer_dirty'
```

Every error was inside one `static` function, `inc_inode_version`,
which is **defined once and called from nowhere** -- a fragment of
Linux's ext2 that came across with the file and was never wired up.

And it is already guarded:

```c
#if linux
...
static void inc_inode_version (struct inode * inode, ...)
...
#endif /* linux */
```

In 1995 an undefined identifier in `#if` evaluates to 0, so the block
was excluded and nobody ever noticed it would not compile.

**GCC predefines `linux = 1` in its GNU dialects.** Measured:

| flag | `linux` defined |
|---|---|
| `-std=gnu89` | yes |
| `-std=c89` | no |

So `-std=gnu89` -- **the flag added earlier in this port to make GCC 14
accept K&R function definitions** -- turned the guard on and started
compiling Linux-only code into a BSD server. One era-gap fix created
another.

The remedy is `-Ulinux`, not editing the guard: the source is correct
and the predefine is an accident of the host. `-std=c89` would also work
but loses GNU extensions this code needs elsewhere.

## Superseded: ext2's inline asm

After the string-literal fix below, the same header failed differently:

```
i386-bitops.h:81:9: error: 'asm' operand has impossible constraints
                    or there are not enough registers
```

three times -- once per call site where the `extern inline` was
instantiated.

**The measurement that identified it.** Two candidate causes, tested one
flag at a time:

| build | impossible-constraint errors |
|---|---|
| `-O2 -fomit-frame-pointer` | 3 |
| `-O0` | 0 |

Freeing EBP made no difference, so it is not frame-pointer pressure.
Failing only with the optimiser on is the signature of a **constraint
bug**, not of genuinely insufficient registers.

**The bug.** `find_first_zero_bit` declared ECX and EDI as inputs *and*
as clobbers:

```c
:"=d" (res)
:"c" (...), "D" (addr), "b" (addr)      /* ECX, EDI, EBX in      */
:"ax", "cx", "di");                     /* EAX, ECX, EDI clobbered */
```

A register cannot be both: the compiler has to set an input up and have
the value survive until the asm reads it, which a clobber declaration
contradicts. The block really does modify both -- `repe` decrements ECX
and `scasl` advances EDI -- so they are **read-write** operands, which
is spelled as early-clobber outputs tied to matching inputs. EAX is
written by the `movl`, so it is an output too rather than a clobber.

Older GCC tolerated the original. Modern GCC rejects it at `-O2` and
accepts it at `-O0`, which is exactly the pattern observed.

Verified in isolation, with three callers of increasing register
pressure:

| constraints | `-O0` | `-O2` | `-O3` |
|---|---|---|---|
| original | pass | **fail** | -- |
| rewritten | pass | pass | pass |

Only the first of the file's three asm blocks was wrong; the second uses
plain `"=r"`/`"r"` and the third already uses matching `"0"`/`"1"`
operands.

## Superseded: ext2 now compiles, multi-line asm string literals

With `ext2fs` enabled the ext2 sources build for the first time in this
project, and `server/ufs/ext2fs/i386-bitops.h` failed immediately:

```
i386-bitops.h:81:17: error: missing terminating " character
i386-bitops.h:86:20: error: invalid suffix "f" on integer constant
```

Three inline assembly blocks are written with **raw newlines inside the
string literal**:

```c
	__asm__("
		cld
		movl $-1,%%eax
		...
		addl %%edi,%%edx"
		:"=d" (res) ...);
```

K&R compilers accepted that. Modern C requires each line to end `\n\`,
which keeps the literal legal while still giving the assembler the
newlines it needs.

**This is the third instance of the same construct in this project**,
after `conf/gensym.awk` and `conf/newvers.sh`. It is worth recognising
on sight: `missing terminating " character` together with
`invalid suffix "f" on integer constant` -- the latter because a local
assembler label like `1f` ends up parsed as C once the string breaks.

The patch series converts all three blocks. Verified: the header passes
`gcc -m32 -std=gnu89 -fsyntax-only` standalone with zero errors.

## Superseded: EXT2FS was never compiled in

Before the EIO/EINVAL question below matters at all, the ext2 reader has
to exist in the binary, and it did not.

`ext2fs` is an option in LITES's `conf/MASTER`:

```
options		ext2fs	EXT2FS	1	ext2fs.h
```

and it is **not** part of the `STD+WS` set. Our configure line was
`--with-config="STD+WS+osfmach3"`, which produced

```
config lites+mtime+muarea+file_ports+vnpager+old_synch+ether+inet+ffs
      +pty+second_server+syscalltrace+compat_43+compat_oldsock+kernfs
      +nfs+atsys+i386+iopl+com+osfmach3
```

`ffs` is present; `ext2fs` is absent. The generated
`<builddir>/obj/server/ext2fs.h` therefore contains

```c
#define EXT2FS 0
```

so no `ext2_*.o` objects are built and the entire
`#if EXT2FS ... ext2_mountroot() ... #endif` block in `init_main.c` is
compiled out. **The ext2 reader was never in the binary.**

That is the real reason no `"Wrong magic number"` diagnostic ever
appeared, and it means the `EIO` versus `EINVAL` fix below, while
correct, was inert.

`tools/lites/build-lites.sh` now configures with
`--with-config="STD+WS+osfmach3+ext2fs"`. Check it took:

```sh
cat <builddir>/obj/server/ext2fs.h     # want: #define EXT2FS 1
```

### Method note

This should have been the first check, not the third. The question
"does the code I am debugging exist in the binary at all" is cheaper
than any reasoning about its behaviour, and two rounds were spent
analysing a code path that was not compiled. `docs/METHODOLOGY.md`
§6.2 says to check this; it was not applied here.

## Then: the ext2 fallback needs EIO as well as EINVAL

LITES's root mount tries FFS first and falls back to ext2:

```c
kr = (*mountroot)();            /* ffs_mountroot */
#if EXT2FS
/* XXX if FFS fails, fall back to EXT2FS */
if (kr == EINVAL)
        kr = ext2_mountroot();
#endif
if (kr != KERN_SUCCESS)
        panic("cannot mount root x%x %s", kr, mach_error_string(kr));
```

But `ffs_mountfs` returns **`EIO`**, not `EINVAL`, when the superblock
magic is wrong:

```c
if (fs->fs_magic != FS_MAGIC || ...) {
        brelse(bp);
        return (EIO);           /* XXX needs translation */
}
```

`EIO != EINVAL`, so on a disk holding an ext2 filesystem the fallback
never fires and `ext2_mountroot` never runs. **The ext2 filesystem was
never looked at.**

That explains an observation that had resisted explanation: LITES's
console works, and `ext2_vfsops.c` has a `"Wrong magic number"`
diagnostic, yet no such message ever appeared. It could not -- the code
was never reached.

The same file is inconsistent about which errno means "not my
filesystem": `ffs_vfsops.c:214` sets `EINVAL` with the same
"needs translation" comment, and `:261` returns `EINVAL` outright. The
`XXX` on the `EIO` return is the author flagging exactly this.

The patch series now widens the caller's test to accept both, which
keeps the policy in the caller and leaves FFS's behaviour untouched for
anything else depending on it.

### What this does and does not tell us

It does **not** mean the ext2 image is good. It means we have not yet
found out. The first real test of the filesystem comes after this
change, and `ext2_vfsops.c` will say what it thinks:

- `"Wrong magic number: %x (expected %x for ext2 fs"` -- the read
  worked and the filesystem is wrong, or the read returned garbage
- no magic complaint but a later failure -- the superblock is fine and
  something deeper is wrong
- a successful mount -- done

Note that a magic complaint would also settle a separate open question:
whether the IDE read returns **valid** data. FFS rejecting a superblock
is consistent both with a good read of a non-FFS disk and with a garbled
read. The reported magic value distinguishes them -- `0xef53` means the
read is correct.

## Superseded: the IDE path is clean, the filesystem is not accepted

The nIEN fix works. `HD: false interrupt` went from two occurrences to
one, and the one that remains is at line 23, **before** the `entry:`
line -- probe time. The second, which previously appeared between
LITES's copyright banner and the panic, is gone.

That was the harmful one: the `CMD_SETPARAMETERS` interrupt latched by
the PIC and delivered after `controller_busy` was set, which `hdintr`
consumed as the read's completion. With `nIEN` set around the polled
command it is never raised, and the read is no longer corrupted by it.

The remaining probe-time message is `CMD_IDENTIFY`'s, and harmless --
the controller is idle, `hdintr` discards it. `nIEN` is set around that
command too, so its survival suggests the drive asserts INTRQ once
before the control register write takes effect, or that QEMU latches it
regardless. Not worth chasing: it is discarded and nothing depends on
it.

**The panic is unchanged**, which is now informative rather than
discouraging. The interrupt path is clean, the geometry is right, the
partition opens, and the read is no longer being corrupted -- so the
failure has moved to the filesystem itself. LITES's 1995 ext2 reader
does not accept what modern `mke2fs` produces.

That is the same family as the minix `0x137F` magic: a reader written
against a 1995 on-disk format meeting a modern formatter's defaults. The
image was made with

```sh
/sbin/mke2fs -q -F -b 1024 \
    -O ^resize_inode,^dir_index,^ext_attr,^sparse_super -I 128 root.img
```

which is already conservative, but has not been checked against what
`server/ufs/ext2fs` actually reads.

## Superseded: the IDE disk opens, the read does not complete

The geometry work is done and validated. `hd0` now reports

```
hd0: 20 Meg, C:40 H:16 S:63 - QEMU HARDDISK
```

where it previously said `0 Meg, C:0 H:0 S:0`. Two commits did it:
sizing the whole-disk partition from the label rather than the empty
BIOS table, and falling back to IDENTIFY's default geometry words when
the "current" words read zero.

LITES now reaches the disk. The console ordering is the evidence:

```
Copyright (c) 1994, 1995 Johannes Helander ...

HD: false interrupt          <- new; was not here before
panic: UWVS+
```

Previously the panic followed the banner immediately. The interrupt
between them means `hdopen` got far enough to begin a transaction, so
the failure has moved from "device unopenable" to "device opened, read
does not complete".

### The signal: exactly two false interrupts

```
line 23:  HD: false interrupt     (after "battery clock configured" -- probe)
line 53:  HD: false interrupt     (after LITES's copyright -- first real I/O)
```

and **nothing else**: no `no bp buffer`, no
`hdintr: interrupt w/controller not done`. So the handshake is not
broadly broken; two specific interrupts arrive unexpectedly.

`hdintr` prints this when `controller_busy` is false (`hd.c:955`), then
dumps registers and discards the interrupt without processing it.

An interrupt arriving **after** IDENTIFY completes is the signature of a
polled command: the driver polls `PORT_STATUS` until not-busy, reads the
data and returns without ever setting `controller_busy`, and the
controller then raises IRQ 14 at a driver that has stopped listening.
That leaves an interrupt pending on the controller, and per ATA the next
command's interrupt can be lost or misattributed -- which would explain
why LITES's first real read never completes.

This is the **same defect class as the floppy's reset interrupt**, fixed
earlier in `rstout`: OSFMK's drivers assume controllers are forgiving
about when interrupts arrive relative to status polling, and QEMU
asserts them strictly per spec.

### Where to start next

1. Does the IDENTIFY path in `hd_ssend` set `controller_busy`? If not,
   the first false interrupt is explained outright.
2. Does anything clear the pending interrupt after a polled command?
   The ATA way is to read the status register, which
   `hd_dump_registers` may already do incidentally on the
   false-interrupt path -- or may not.

The precedent for the fix is `rstout`, which drains the 82077's reset
interrupt with four `sis()` calls because the controller will not accept
further commands while one is pending. The IDE equivalent is clearing
the pending interrupt after a polled command rather than leaving it for
the next one to trip over.

Not yet established: whether the filesystem itself is acceptable to
LITES's 1995 ext2 reader. That question cannot be reached until a read
completes, and it is a separate problem of the same family as the minix
`0x137F` magic.

### The root device: hd0c, and why

`server_init.c:301` had `char default_root[] = "hd0a"`, used because
`argc == 0`. The patch series now makes it `hd0c`, and the reason is in
the kernel's `hd` driver.

`hdopen` refuses unless `getvtoc(dev)` succeeds **and** the partition
has non-zero size. `getvtoc` builds the partition table by calling
`read_bios_partitions(dev, 0, ...)` -- reading sector 0 as a DOS/BIOS
partition table -- and when that fails it does this:

```c
/* make partition 'c' the whole disk in case of failure */
label->d_partitions[PART_DISK].p_offset = 0;
label->d_partitions[PART_DISK].p_size =
        ncyl * nheads * nsec;
```

`PART_DISK` is 2 (`disk.h:149`), and `dev_name_lookup` maps partition
letters `a`-`h` onto indices 0-7, so index 2 is `c`.

So **an unpartitioned disk image gives `hd0c` = the whole disk**, with
no MBR, no BSD disklabel and no partition arithmetic to get right.
`hd0a` would have required a real DOS partition table.

Note also that this driver is **CHS, not LBA**: `hd_ssend` computes
sector, head and cylinder from `label->d_nsectors` and `d_ntracks`, so
the geometry QEMU presents has to be consistent with what the driver
reads from CMOS.

### What is needed

A filesystem LITES can mount as root. The code above takes **either**:

- BSD FFS, via `mountroot`
- **ext2**, via `ext2_mountroot`, tried when FFS returns `EINVAL`

ext2 is far easier to produce on a modern Linux host, and unlike the
kernel's minix reader there is no exotic magic requirement to satisfy --
this is LITES's own ext2 implementation, not the bootstrap task's.

The root device comes from `rootname`/`rootdev_name` in
`server_init.c`, with a compiled-in default in `argv_space` beginning
`/dev/hd0f/mach_servers/startup`, so selecting the device is a separate
question from creating the filesystem.

## Superseded: the new blocker, no paging store

```
panic: UWVS+
(default pager): ps_allocate_cluster: no space in available paging
                 segments; swapon suggested
```

`default_pager` starts but has no backing store, so the first demand for
anonymous memory has nowhere to go. The pager says what it needs:
a swap device, via `default_pager_add_segment` or
`default_pager_backing_store_create`, neither of which anything
currently calls with a real device.

Worth trying first, as a one-word change: more memory, `-m 256` rather
than `-m 64`, which may push the first paging event past
initialisation.

`panic: UWVS+` is not yet decoded. LITES's `panic` does
`printf("panic: %r\n", fmt, ap)`, where `%r` is a BSD kernel extension
for recursive format expansion; the string appears nowhere in the source
as a literal, so this may be `%r` being mishandled rather than a real
message. Check before reading anything into it.

## Superseded: RETRACTED, the argument frame is correct by design

The note below claimed `i386/set_regs.c` fails to write an argument
block and called it a bug. That is wrong, and this retraction is kept
because the claim was committed and pushed.

`load.c:459` says what the block is for:

```c
/*
 * Allocate space for:
 *    dummy 0 argument count
 *    dummy 0 pointer to arguments
 *    dummy 0 pointer to environment variables
 *    and align to integer boundary
 */
arg_len = sizeof(int) + 2 * sizeof(char *);
```

The zeros are **deliberate**. `vm_allocate` zero fills, and
`uesp = stack_end - 0x10` leaves sixteen zero bytes where twelve are
needed, so the task receives exactly the intended
`argc = 0, argv = NULL, envp = NULL` frame. The `/* XXX */` marks the
hardcoded sixteen rather than using `arg_len`; it does not mark missing
data. HP700 computing `stack_start + arg_size + 32` is the same idea
spelled differently, not evidence of an unfinished i386 port.

Servers here are **designed** to start with no arguments. LITES is built
for that: `init_second_server_flag` returns on `argc <= 0`,
`parse_arguments` returns on `argc == 0`, and `get_config_info` falls
through to `host_get_boot_info`, with a hardcoded default in
`argv_space[10][40]` beginning "/dev/hd0f/mach_servers/startup".

So adding `-s` to `bootstrap.conf` was never going to reach LITES, but
not because of a defect -- the mechanism simply is not arguments. How
LITES is meant to be configured is through the kernel boot info and its
compiled-in defaults, and that is the thread to pull next.

## Superseded claim follows

LITES loads, is resumed, and terminates before any output. The argument
path is broken, and that is proven; whether it is the whole cause of the
termination is not yet proven.

## What is proven

`src/bootstrap/load.c:466` computes the size of an argument block:

```c
arg_len = sizeof(int) + 2 * sizeof(char *);   /* argc + argv[0] + NULL */
arg_len = (arg_len + (sizeof(int) - 1)) & ~(sizeof(int)-1);
...
set_regs(master_host_port, user_task, user_thread, &ofmt.info,
         mapend, arg_len);
```

Nothing writes that block. There is no `vm_write`, no copy, nowhere in
`load.c` or `bootstrap.c` that puts `argc`, `argv` or `envp` into the
new task's memory.

`i386/set_regs.c` then discards the size as well:

```c
(void)vm_allocate(user_task, &stack_start, ..., FALSE);  /* zero filled */
regs.eip  = lp->entry_1;
regs.uesp = stack_end - 0x10;      /* XXX */
```

`arg_size` appears only in the parameter list, never in the body.

Three things corroborate that this is unfinished rather than intended:

- **HP700 honours it**: `HP700/set_regs.c:82` reads
  `regs.sp = ((stack_start + arg_size + 32) & ~(sizeof(int)-1));`
- the i386 offset carries the author's own `/* XXX */`
- the parameter exists at all

So every server the bootstrap task loads starts on a freshly
`vm_allocate`d, zero-filled stack with no arguments.

**This immediately explains one observation.** Adding `-s` to
`bootstrap.conf` changed nothing, because the flag never reaches the
server: `init_second_server_flag(argc, argv)` and LITES's `parse_args`
both see `argc == 0`.

This is the same defect, one level up, as the one already fixed in
`kern/bootstrap.c`, where `user_bootstrap_old` did not build argc and
argv for the bootstrap task and crt0 gates on `kargv[0]`. The kernel
loads the bootstrap task; the bootstrap task loads the servers; both
loaders had it.

`i386/set_regs.c` is byte identical to MkLinux's, so this is not
something introduced here.

## What is NOT yet proven

That this is why LITES terminates. With a zero-filled stack crt0 will
read `argc == 0` and an `argv` pointing at zeros, so `argv[0]` is NULL;
whether LITES dereferences it before its console exists has not been
measured. The next step is a `-d exec` trace symbolised against `nm` on
the `startup` binary, to find the last user-mode symbol reached.

## Superseded: MILESTONE, the OSF multiserver userland runs

```
(bootstrap): loading /dev/boot_device/mach_servers/name_server
ELF: Unknown program header flags 0x4
ELF: Unknown program header flags 0x4
(bootstrap): loading /dev/boot_device/mach_servers/default_pager
ELF: Unknown program header flags 0x4
ELF: Unknown program header flags 0x4
(bootstrap): started
(name_server): started
(default_pager): started
```

Both servers -- `name_server` at 140,396 bytes and `default_pager` at
211,100 bytes -- are read off a minix floppy, ELF parsed, loaded and
started. The whole chain works:

```
kernel boots -> VM, IPC, devices -> user task at ring 3
-> cthreads init -> Mach IPC -> console device
-> floppy driver -> minix filesystem
-> /mach_servers/bootstrap.conf read and parsed
-> name_server  loaded and STARTED
-> default_pager loaded and STARTED
-> (bootstrap): started
```

That takes about 34 minutes of wall time in this environment. See the
timing note below before concluding anything is wrong.

## The invocation

```sh
qemu-system-i386 -kernel mach_kernel.PRODUCTION \
    -append "-r BOOTDEV=fd BOOTPART=1 -o" \
    -initrd bootstrap -fda minix.img \
    -serial file:/tmp/console.log -display none -no-reboot -m 64
tail -f /tmp/console.log
```

`-r` selects the serial console, which gives full scrollback. Do not use
`tools/vgadump.py` for this -- it reads a 25 line framebuffer that
scrolls.

## Building the image

```sh
python3 tools/mkminix.py minix.img \
    $MK_BUILD/obj/at386/mach_services/servers/netname/name_server \
    $MK_BUILD/obj/at386/default_pager/default_pager
```

`tools/mkminix.py` both **formats and populates**, so `mkfs.minix` is
not needed -- which matters, because Debian 13 dropped it from
util-linux and it is no longer packaged there at all.

Formatting in the tool also removes a trap. The reader,
`file_systems/minixfs/minixfs.c:560`, accepts only `MINIX_SUPER_MAGIC`
`0x137F`, the original 14-character-name variant, while `mkfs.minix -1`
defaults to 30-character names and magic `0x138F`, which is silently
rejected.

Two things are easy to get wrong. `minixfs.c:560` accepts only
`MINIX_SUPER_MAGIC` `0x137F`, and `mkfs.minix -1` defaults to 30
character names giving `0x138F`, so **`-n 14` is required**. And
`tools/mkminix.py` writes directories and files by hand, including
single indirect blocks for files over 7 KB, because a loop mount needs
privileges the build environment does not have.

The servers must be built first:

| server | needs |
|---|---|
| `name_server` | `mach_services/lib/libservice` |
| `default_pager` | libcthreads, libsa_mach, libmach, libmach_maxonstack |

## Still open

**`BOOTPART=1` is required and should not be.** The boot device minor is
`unit + BOOTPART`, and `MEDIATYPE(dev)` is `dev & 0x03`, indexing
`m765f[]`; without it the driver picks entry [0], 720 Kb with 9 sectors
per track, and every seek past that fails. Selecting 1.44 Meg through a
partition number is a workaround, not a fix.

**Floppy throughput.** Measured both ways: with KVM the full boot to
three servers takes **a few minutes**; without it, on TCG with a single
CPU, it takes about **35 minutes**, because the guest advances at
roughly a sixth of wall clock. So most of the apparent slowness in the
development logs was the environment, not the driver -- but the floppy
is still slow in absolute terms even with KVM, so there may be a real
driver inefficiency underneath. It does not block anything.

Note for anyone reading the development history: a long series of
"it hangs in X" conclusions in `docs/archive/` were all this. Nothing
was hung; the samples were taken too early.

**Unrecognised CPU.** The console prints
`Unrecognized processor (type = 0x0, family = 0x6, model = 0x6)` on a
modern host. The identification table predates the processor. Harmless
-- the machine configures and boots -- and cheap to extend if it ever
matters.

## Superseded: the bootstrap task reads its config

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
