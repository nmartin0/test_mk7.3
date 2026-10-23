# The OSF/1 interface, as the public record describes it

This is the specification our server is tested against. Each item is
one behaviour that a public document says OSF/1 has, written so that a
test can check it. Nothing here was taken from source code. Every item
names the document, section and page it comes from, and the documents
are listed in `docs/reference/public-sources.md`.

Begun 23 October 2026. It records what has been read so far; the
section "Not yet read" at the end says what has not.

## The baseline

OSF/1 MK is "specified by its conformance to OSF/1 Release 1.2, and
this conformance has been verified using the VSX and VSE validation
suites" (RN501 §3.2). The no-emulator server reached the emulator
server's VSX and VSE level within weeks (PAT §7), and MK5.0 had no VSE
failures and the same XPG4 results as the integrated kernel apart from
the list in BUG-30 (RN501 §4.4). So the baseline is OSF/1 R1.2, as
measured by VSX (XPG4) and VSE (the OSF AES). This document adds what
those suites cannot see: the multithreaded, Mach and boot behaviour.

## How to read an item

    SIG-4 [OSF/1] Statement.
      Source: where it is said.
      Test:   how a program can check it.

**Identifier.** A prefix for the area and a number. Numbers are never
reused; a withdrawn item stays, marked *Withdrawn*, with the reason.

**Scope tag.**

- `[OSF/1]` OSF/1 in general: the integrated kernel and every server
  that claims to be OSF/1. A conforming server must do this.
- `[MK]` OSF/1 on Mach specifically: the single server, its boot and
  its seam with the kernel.
- `[DEC]` DEC's OSF/1 or its descendants (Digital UNIX, Tru64). Used
  where OSF's own documents are silent; a guide, not a requirement.
- `[Kernel]` the OSF Mach kernel interface the server relies on.
  Tested only so far as the server depends on it.
- `[POSIX]` added by us from POSIX.1 where a document states a
  behaviour but not its error number. Marked so it is never mistaken
  for something an OSF document said.

**Kinds of item.** Most items are *requirements*. Three sections hold
other kinds:

- **BUG** lists defects OSF documented in its own releases. Each gives
  the correct behaviour, which is what we test; the defect is not to
  be reproduced.
- **GAP** lists questions the documents leave open, with the decision
  we took and where it is recorded.
- **ERR** lists places where the documents are wrong or disagree.

**Quotations** are kept short. The design book and the *Internals*
course are under copyright with all rights reserved; they are cited by
page and quoted only where the exact words decide a test.

## Source keys

| key | document | where |
|---|---|---|
| RN73 | *MK7.3 Release Notes* (The Open Group RI, 1997) | carried, `docs/reference/osf-ri/os/osfmk/mk7.3/` |
| RN61 | *OSF-RI MK6.1 Release Notes* (1994) | carried, `docs/reference/osf-ri/os/osfmk/mk6.1/` |
| RN501 | *OSF/1 MK5.0.1 Release Notes* v1.1 (OSF RI Grenoble, 19 May 1993) | cited |
| KI | OSF Mach kernel manual pages | carried, `docs/reference/xnu-792/osfmk/man/` |
| SL | *OSF Mach Server Library Interfaces*, final draft (3 May 1993) | carried, `spec93/server_library.ps.Z` |
| SWG | *OSF Mach Server Writer's Guide*, final draft (3 May 1993) | carried, `spec93/server_writer.ps.Z` |
| PAT | Patience, *Redirecting System Calls in Mach 3.0* (USENIX Mach III, 1993) | cited |
| COLD | Condict, *Optimizing Performance of Mach-based Systems By Server Co-Location: A Detailed Design* (OSF RI, 24 August 1993) | cited |
| COLP | Condict, Bolinger, Mitchell, McManus, *Microkernel Modularity with Integrated Kernel Performance* (OSF RI, 24 June 1994) | cited |
| BOOK | *Design of the OSF/1 Operating System, Release 1.2* (OSF; Prentice Hall, 1993) | cited |
| INT1, INT2 | *OSF/1 Internals*, Volumes I and II (IAPS for DEC, 1992) | cited |
| BOL | Bolinger, Mangalat, *Parallelizing Signal Handling and Process Management in OSF/1* (USENIX Mach Symposium, 1991) | cited |
| DEAN | Dean, *Porting OSF/1 AD* (OSF RI, 27 September 1993) | cited |
| PMA | Rabii, Patience, *The Process Management Architecture of OSF/1 AD Version 2* (OSF RI, 20 September 1993) | cited |
| PMD | Patience, Rabii, *The Design of the Process Management Component of OSF/1 AD Version 2* (OSF RI, November 1993) | cited |
| AD3 | Rabii, Noveck, Gilbane, *Distributing Process Management in OSF/1 AD Version 3* (OSF RI, March 1996) | cited |
| FSA | Roy, Noveck, Netterwala, *The File System Architecture of OSF/1 AD Version 2* (OSF RI, 1993) | cited |
| CMUX | Black et al., *The Mach Exception Handling Facility* (CMU) | cited |
| DRN3, DTO3 | *DEC OSF/1 3.0 Release Notes* and *Technical Overview* (DEC, August 1994) | cited |
| DEC10 | DEC OSF/1 1.0 (MIPS) manual pages, as kept at `typewritten.org/Manual/DEC/OSF:1/1.0/mips/` | read on the web |
| TRU | Tru64 UNIX 5.x manual pages, as kept at `typewritten.org/Manual/DEC/Tru64/` | read on the web |

The AD papers describe a distributed OSF/1; only what they say of
OSF/1 itself, or of the single server, is used here. DEC's manuals
describe DEC's integrated kernel, and are a guide only.

Page numbers are the ones printed in each document; "n/p" means the
document prints none, and the section is given instead. Bracketed numbers
such as [2271] are OSF's own problem-report numbers from the release
notes.

---

## BOOT — booting the system

**BOOT-1** [MK] The boot command has the form
`disk(unit, partition)/kernel [k-options] bootstrap [b-options]`, and
every field is optional. `disk` is `hd` (AT, IDE, ESDI) or `sd` (SCSI);
`unit` defaults to 0; `partition` is `a` to `p` and defaults to `a`;
`kernel` defaults to `mach_kernel` and `bootstrap` to
`/mach_servers/bootstrap`. A name with no directory is looked for in
the root directory.
  Source: RN73 §2.5.3, "Boot Options", pp. 48-49.
  Test:   boot with each field omitted in turn; the defaults load.

**BOOT-2** [MK] Kernel options: `-h` halts into the kernel debugger as
early as possible and is also passed to the server; `-s` asks for
single-user mode and is passed to the server; `--` ends the kernel
options.
  Source: RN73 p. 49.
  Test:   `boot: -s` reaches a single-user shell; the server sees `-s`.

**BOOT-3** [MK] Bootstrap option `-a` makes the bootstrap ask for the
name of its configuration file and then for each server, printing a
default it lets the operator edit. `boot: -- -a` is the documented way
to be asked for everything. In the documented example the prompts are,
in order, `Configuration file:`, `name_server:`, `default_pager:`,
`startup:`; the floppy boot (BOOT-8) shows `configuration file:` in
lower case and no `name_server:` prompt.
  Source: RN73 pp. 49-50 (the `/mach_srv.old` example).
  Test:   `boot: -- -a`; the prompts appear in that order, each with a
          default, and an edited answer is used. Case is not tested.

**BOOT-4** [MK] The bootstrap's configuration file has the default
pathname `/mach_servers/bootstrap.conf` on the root file system ("You
might also create" it: it is optional). It names the servers to start
and their options; the example has one per line. The documented
example is:

    name_server /mach_servers/name_server
    def_pager /mach_servers/default_pager sd0b
    -k startup /mach_servers/osf1_server.INK sd0a

  Source: RN73 §2.5.3, p. 48.
  Test:   boot with exactly this file; all three start.

**BOOT-5** [MK] `-k` before `startup` loads the OS server into the
kernel ("an in-kernel OSF/1 (UNIX) server"). *Inferred* from the
example's caption and from [1869], which reports a crash when "the -k
argument" is removed; no document defines the flag in words.
  Source: RN73 p. 48 and §3.2 [1869], p. 58.
  Test:   with and without `-k` the server boots (see BUG-9).

**BOOT-6** [MK] The word after the server's path on the `startup` line
is the root device (`sd0a` above). The floppy boot's `startup` line is
`/dev/boot_device/mach_servers/startup -s -a fd0b`: the server is
given `-s`, `-a` and the root device `fd0b`, and it asks
`root device: [fd0b]`, the default in brackets, before going on.
*Inferred*: that `-a` is what makes it ask.
  Source: RN73 §2.4.2.1, pp. 37-38; §3.3 [2271], p. 60 (the root device
          on the `startup` line).
  Test:   a `startup` line ending `-a fd0b` prints exactly
          `root device: [fd0b]`; pressing Enter uses `fd0b`.

**BOOT-7** [MK] With `-s`, the server stops at a `#` prompt: a
single-user root shell.
  Source: RN73 p. 38.
  Test:   boot `-s`; the console shows `#`; `id -u` prints 0.

**BOOT-8** [MK] During the boot, the bootstrap prompts for the
configuration file and each server in turn with the file names shown
under `/dev/boot_device/`, for instance
`configuration file: /dev/boot_device/mach_servers/bootstrap.conf`.
  Source: RN73 p. 37.
  Test:   with `-a`, the defaults printed use the `/dev/boot_device/`
          prefix.

**BOOT-9** [MK] Pressing the space bar at `boot:` stops the default
boot and waits for a command. With no command, the defaults boot
`/mach_kernel` and start `/mach_servers/startup`. New images are made
the defaults by linking them as `/mach_kernel` (a hard link) and
`/mach_servers/startup.INK` (a hard or symbolic link).
  Source: RN73 pp. 48-49.
  Test:   boot untouched; the default kernel and server start.

**BOOT-10** [MK] The older MK6.1 syntax is
`disk(unit, partition)/mach_kernel:server -a|-h|-s|-d|-<digit>
server_options`. `:server` names a server other than the default and is
looked for in `/mach_servers` when it has no directory. `-a` asks for
the server name; `-h` and `-s` as in BOOT-2; `-d` loads kernel
symbols; `-<digit>` is the number of processors to start.
  Source: RN61 "Boot Options", pp. 14-15.
  Test:   historical; not required of an MK7.3-shaped system.

**BOOT-11** [MK] The no-emulator bootstrap sequence. The kernel starts
the bootstrap task first, with its bootstrap port answered by the
kernel. The bootstrap asks that port for the privileged host and device
ports and the boot parameters, and serves the bootstrap and "service"
interfaces itself. It starts the name server, then the default pager,
gives the pager its paging device, then starts `startup`, the OS
server, and waits for bootstrap and service requests. The OS server
asks the bootstrap for the host and device ports and the boot
parameters, and itself serves the bootstrap interface to its own
children, controlling their bootstrap ports. The OS server runs
`/sbin/init`; there is no `mach_init`.
  Source: COLD §5.1, "Bootstrap Process Review", pp. 16-17.
  Test:   after boot, `init` is process 1 and its parent is not a
          `mach_init`; a child's bootstrap port is a port the server
          holds the receive right for.

**BOOT-12** [MK] The bootstrap also handles exceptions for the servers
it starts, "unless they decide to handle their own".
  Source: COLD §5.1, p. 17.
  Test:   not testable from user space.

**BOOT-13** [MK] Programs the bootstrap starts get no arguments on the
initial stack: "Program loading performed for all standalone Mach
tasks ... does not include any support of passing program parameters
on the initial stack". They ask for them instead (BOOT-14).
  Source: COLD §5.1, p. 17.
  Test:   the server reads its arguments through `bootstrap_arguments`.

**BOOT-14** [Kernel] `bootstrap_arguments` and `bootstrap_environment`:
the kernel answers task 1 with the arguments and environment given to
the boot loader, as NUL-separated strings. The bootstrap task may serve
the same interface to the tasks it creates. The standalone C start-up
in `libsa_mach.a` uses them to build `argc`, `argv` and `envp`.
  Source: KI `bootstrap_arguments`, `bootstrap_environment`.
  Test:   a server started by the bootstrap sees the options given on
          its `bootstrap.conf` line in `argv`.

**BOOT-15** [Kernel] `bootstrap_completed`: a server tells the
bootstrap it is ready. The default pager sends it so that the bootstrap
can wait for paging before starting the OS server; a flag in
`bootstrap.conf` makes the bootstrap wait, forever if the message never
comes. `KERN_INVALID_ARGUMENT` if the bootstrap does not know the
caller.
  Source: KI `bootstrap_completed`.
  Test:   not required of the OS server; the OS server need not send it.

**BOOT-16** [MK] `init` expects to be process 1. Under the older,
emulator-based start-up the server ran `mach_init`, which forked and
ran `/sbin/init` in the parent, so that "init has a Unix PID of 1, as
it expects".
  Source: COLD §5.1, p. 16.
  Test:   `getpid()` in `init` is 1.

**BOOT-17** [MK] A design that was proposed for the bootstrap loader:
`bootstrap [ -u | [ -s map_size ] ] [ server-pathname ]`, loading into
the kernel unless `-u`, with the server defaulting to
`/mach_servers/startup` and the submap to about 16 MB. MK7.3 shipped
`bootstrap.conf` (BOOT-4) instead. Recorded so that the two are not
confused; not a requirement.
  Source: COLD §5.1, "Design", pp. 18-19.

## BSVC — the bootstrap service and the privileged ports

**BSVC-1** [Kernel] `bootstrap_ports` returns send rights to the host
control port, the device master port, the two root ledgers and the host
security port. The kernel answers it on task 1's bootstrap port; the
bootstrap task decides who else gets them. "An OS personality can serve
as a server on the TASK_BOOTSTRAP_PORT for tasks or servers that it
manages, and can regulate or interpose on the ports in any way it deems
necessary."
  Source: KI `bootstrap_ports`.
  Test:   see BSVC-3.

**BSVC-2** [MK] The OS server serves the bootstrap interface to its
child tasks and controls their bootstrap ports.
  Source: COLD §5.1, p. 17.
  Test:   a process's bootstrap special port is answered by the server.

**BSVC-3** [MK] A user task asking for the privileged host port
(`mach_host_priv_self`) asks its bootstrap port. "How an OSF/1 server
responds is its own policy decision."
  Source: COLD §5.1, p. 17.
  Test:   the policy is ours (GAP-11); the request must be answered, not
          left hanging.

**BSVC-4** [MK] The server library's `mach_privileged_host_port` and
`mach_device_server_port` first send "a special message (ID 999999) to
the task's bootstrap port, and, failing that," use the CMU system call
`task_by_pid` (number −33). Both "will succeed only for privileged
tasks" and return `MACH_PORT_NULL` otherwise. The host port lets the
holder get rights to any other port on the node except the device
master port; the device master port lets the holder open any device.
  Source: SL, `mach_device_server_port` (p. 8) and
          `mach_privileged_host_port` (p. 16).
  Test:   as root, both return a port; as an ordinary user, both return
          `MACH_PORT_NULL`.

**BSVC-5** [MK] Under the older procedure: the server sent message
ID 999999 to its bootstrap port and was answered with the host and
device ports; a second server's 999999 message is ignored by the first
server, and the second server instead calls `task_by_pid(-1)` and
`task_by_pid(-2)` for the host and device ports. `mach_host_priv_self`
in other tasks also used `task_by_pid(-1)`.
  Source: COLD §5.1, p. 16.
  Test:   as root, `task_by_pid(-1)` returns the privileged host port
          and `task_by_pid(-2)` the device master port; as an ordinary
          user both fail [our requirement, after BSVC-4].

**BSVC-6** [MK] In the first no-emulator servers, a process needs the
same privilege to use Mach services directly as it needs to load
kernel extensions into the server, until Mach services can be brought
under the personality's security policy.
  Source: PAT §2.1.
  Test:   the policy is ours (GAP-12).

## SYS — the system-call path

**SYS-0a** [OSF/1] System calls are "the sole interface between the
user and the operating system": a subroutine call whose body is an
architecture-specific trap into privileged mode. A system call runs in
the context of the calling process.
  Source: INT1 p. 2-17 and p. 2-19.
  Test:   the same call made through libc and through a raw trap gives
          the same result.

**SYS-0b** [OSF/1] The return convention the course gives as typical.
On success the result is in the user's register 0. The carry bit of the
saved status word says whether the call failed: clear for success; set
for failure, with the error number in register 0. The library then
stores the number in `errno` and returns −1. Every other register is
saved at the trap and restored. The course calls this
architecture-dependent; see GAP-1.
  Source: INT1 pp. 2-21 to 2-23.
  Test:   a raw `close(-1)` trap leaves carry set and `EBADF` in the
          result register; a raw `getpid` leaves carry clear and the pid
          there; sentinel values in the other registers survive.

**SYS-0c** [OSF/1] The kernel does not trust the user stack pointer:
it validates the locations it points to before copying the arguments,
since it might point into the kernel.
  Source: INT1 p. 2-21.
  Test:   a raw trap with the stack pointer at a kernel address fails
          or faults the process; the system stays up [our requirement].

**SYS-0d** [OSF/1] Mach calls are kept separate from UNIX ones. A Mach
call's function value is only its status; results come back in output
parameters, and `errno` is not involved. A Mach call is in substance
an RPC to the kernel, generated by MIG, and it is carried out by the
calling thread itself.
  Source: INT1 p. 2-17 and p. 2-25.
  Test:   `task_info` on a bad port returns a Mach error and leaves
          `errno` alone.

**SYS-1** [MK] A system call is a trap. The kernel turns it into an
exception message and sends it to a port the server set up for system
calls. The server carries out the call as the integrated kernel would,
using copyin and copyout for the caller's memory, and returns the
changed registers in its reply; the kernel sets them and resumes the
thread.
  Source: PAT §3, figure 3; RN501 §4.2, "System calls are sent to the
          server as exception messages".
  Test:   every system call in the test suite exercises this.

**SYS-2** [Kernel] The exception types include `EXC_SYSCALL` and
`EXC_MACH_SYSCALL`. A thread-state flavour, `THREAD_STATE_SYSCALL`,
carries "a small subset of registers adequate for handling most system
calls"; on the i386 it is 5 registers against 17 for the full set.
  Source: PAT §4; KI `catch_exception_raise`.
  Test:   not testable from user space.

**SYS-3** [Kernel] An exception port is set per exception type, per
thread and per task, with a behaviour and a flavour. The behaviour
picks the message: `EXCEPTION_DEFAULT` (`catch_exception_raise`, no
state, flavour ignored), `EXCEPTION_STATE` (state in and out, no task or
thread ports), or `EXCEPTION_STATE_IDENTITY` (both). With the state
behaviours the reply carries the new state, which the kernel sets
before resuming the thread.
  Source: PAT §4; KI `catch_exception_raise`, `task_set_exception_ports`.
  Test:   not testable from user space.

**SYS-4** [Kernel] Delivery: the kernel tries the thread's port for
that exception type; if there is none or its reply is not
`KERN_SUCCESS`, it tries the task's port. The two may have different
behaviours and flavours. What happens when neither handles it: see
ERR-2.
  Source: PAT §4; KI `catch_exception_raise`, `task_set_exception_ports`.
  Test:   see SYS-10.

**SYS-5** [Kernel] Setting a task's exception port the old way,
`task_set_special_port`, is the same as setting every exception type
but one (`EXC_MASK_ALL & ~EXC_MASK_SYSCALL`) with the
default behaviour, and sends the old message format. Only `EXC_SYSCALL`
is left out; `EXC_MACH_SYSCALL` is included (compare SYS-6).
*Inferred*: a debugger that sets exception ports the old way never
receives UNIX system calls.
  Source: PAT §4.
  Test:   a debugger attached with the old call sees faults and
          breakpoints but the debuggee's system calls still work.

**SYS-6** [Kernel] If a thread's or task's `EXC_MACH_SYSCALL` port is
the host name port, Mach traps run in the kernel as usual; any other
value turns them into exceptions. "By applying this mechanism to Mach
system calls, they can be disabled or re-directed to a server."
  Source: KI `task_set_exception_ports`, NOTES; RN501 §3.1.
  Test:   see BSVC-6 and GAP-12.

**SYS-7** [MK] There is one way in: every system call reaches the
server through the single system-call exception port, so "all system
calls are, at least initially, handled by the same server", and the
request enters the trusted computing base at once.
  Source: PAT §8.
  Test:   not testable from user space.

**SYS-8** [MK] System calls from threads the server did not create
must work. MK5.0.1 fixed a panic "when a thread did a system call for
the first time" if it came from such a thread.
  Source: RN501 §4.6.
  Test:   a process creates a thread with `thread_create`, sets its state
          and resumes it; the new thread calls `getpid()` and gets the
          process's pid; the server does not fail.

**SYS-9** [MK] The server must defend itself against what a process
can do with Mach directly: calling `task_terminate` instead of `exit`;
creating threads with `thread_create` that then make system calls;
`vm_allocate` and `vm_deallocate` behind the server's back, confusing
its view of the break and of mapped regions; and sending malformed
messages to the system-call ports, with wrong contents or extra
out-of-line data meant to fill the server's address or port space.
None may hang, crash or slowly exhaust the server.
  Source: PAT §2.2-2.3.
  Test:   one test per attack; afterwards the server still answers and
          its port and memory counts return to their starting values.

**SYS-10** [MK] *Derived.* A thread that has no thread-level
system-call port still reaches the server through the task-level port
for `EXC_SYSCALL` (SYS-4). SYS-8 therefore requires a task-level port,
or an equivalent.
  Source: from SYS-4 and SYS-8.
  Test:   as SYS-8.

**SYS-11** [OSF/1] System-call arguments are copied from the user stack
to the kernel; the u-area has no argument field. With no emulator, the
arguments themselves must be fetched by copyin: "no user data is
passed as an argument in the exception RPCs".
  Source: BOOK §4.4.7, p. 4-17; COLD §2.3, p. 6.
  Test:   a system call whose arguments lie across a page boundary on
          the user stack works.

**SYS-12** [OSF/1] When a signal interrupts a system call, the call
returns through the normal path with an "interrupted" error. If the
call should be restarted, the user stack pointer is adjusted so that
the call is made again when the process next runs in user mode;
otherwise it fails with `EINTR`. Which signals restart calls is a
per-process setting made through `sigaction`; one thread cannot change
it without changing it for all the others.
  Source: BOOK §4.4.7, pp. 4-17 to 4-18.
  Test:   `read` on an empty pipe interrupted by a caught signal: fails
          `EINTR` without the restart flag, resumes with it; set the
          flag in thread A, check it applies in thread B.

**SYS-13** [OSF/1] Kernel extensions that add system calls are loaded
at boot and configure themselves. Under the emulator the call vector
was copied to every task at `init` and could not learn new calls.
*Inferred*: the exception path has no per-task vector.
  Source: PAT §2.4.
  Test:   not required until loadable subsystems are.

**SYS-14** [MK] With no emulator, nothing of the system's is mapped
into a process: the interface is a trap in and copyin and copyout back.
  Source: PAT §3; RN501 §2.
  Test:   `vm_region` over a fresh process finds only its program, its
          libraries, its data and its stack.

**SYS-15** [MK] `exec` of a setuid program must carry nothing over from
the old image that the old image could have changed. The emulator,
inherited without being reloaded, was a way to become root.
  Source: PAT §2.1.
  Test:   scribble over one's own memory, then `exec` a setuid test
          program: its behaviour and credentials are unaffected.

## COPY — copyin, copyout and bad addresses

**COPY-1** [MK] copyin and copyout have the integrated kernel's
semantics. In particular a copyout followed by a copyin of the same
address sees the data written, and copyout is not deferred to the end
of the call.
  Source: PAT §2.4 and §3; RN501 §4.2, "copyin/copyout semantics are
          now compatible with those of the IK".
  Test:   a system call that writes then reads the caller's memory
          (for example `readv` into overlapping vectors) sees its own
          writes.

**COPY-2** [MK] There is no limit on how much a call may copy out,
beyond the call's own limits; the emulator's limit (the size of a reply
message) does not apply.
  Source: PAT §2.4.
  Test:   `read` of 1 MB from a file into one buffer succeeds.

**COPY-3** [OSF/1] A bad address passed to a system call makes the
call fail with `EFAULT`; the program keeps running and takes no fault.
  Source: COLP §2, p. 4: "UNIX requires that when an illegal address is
          passed as a parameter to a system call, the system call
          return the error 'EFAULT' and the program continue to
          execute (that is, it cannot suffer an addressing fault)".
  Test:   `read(fd, (void *)1, 10)`, a kernel address, and an address
          just past the end of the stack each return −1 with `EFAULT`.

**COPY-4** [MK] Copyout writes exactly the bytes asked for. Copying
40 bytes must not rewrite the rest of the page, which another thread or
a task sharing the page may be changing at the same moment.
  Source: PAT §3, the `vm_write` problem.
  Test:   thread A keeps writing a byte next to a buffer while thread B
          reads into the buffer; A's writes are never lost.

**COPY-4a** [OSF/1] Copying to and from the caller recovers from a
fault: a bad user address gives an error, never a crash of the server.
  Source: DEAN §5.3; COLP §2.
  Test:   as COPY-3, with addresses in every unmapped hole of the
          process.

**COPY-5** [MK] *Derived.* Memory the process unmaps, by `munmap` or
by `vm_deallocate`, must not stay readable through system calls. OSF's
`vm_remap` cache could keep a stale mapping after a direct
`vm_deallocate`.
  Source: PAT §6.
  Test:   unmap a buffer with `vm_deallocate`, then `write(fd, buf, n)`
          fails with `EFAULT` rather than writing the old data.

## PROC — processes

**PROC-1** [OSF/1] A process is a Mach task and one or more threads.
Threads are scheduled; the process has no scheduling state of its own.
  Source: BOOK §4.1 and §4.1.1, pp. 4-2 to 4-3.
  Test:   not testable as such.

**PROC-2** [OSF/1] What the process holds and what each thread holds.
Per process: the descriptor table, the file creation mask, the
environment, the sizes and starts of the address-space regions, the
process-wide signal state, the identifiers (pid, parent pid, real and
effective uid and gid) and the parent and sibling links. Per thread:
its kernel stack, its name-lookup cache, and the state for signals
raised by its own exceptions.
  Source: BOOK §4.1.1-4.1.2.2, pp. 4-3 to 4-5.
  Test:   `umask`, `chdir`, `setuid` in one thread are seen by the
          others.

**PROC-3** [OSF/1] `fork` copies only the calling thread. The child has
one thread.
  Source: BOOK §4.3, p. 4-6.
  Test:   a process with four threads forks; the child's `task_threads`
          returns one thread.

**PROC-4** [OSF/1] `fork` while another thread is exiting: if `exit`
has been called, `fork` suspends the calling thread until the process
has terminated; the `fork` never completes.
  Source: BOOK §4.3, p. 4-6; BOL §6.2, p. 114.
  Test:   thread A calls `exit` while thread B is about to `fork`; no
          child is created after `exit` begins.

**PROC-5** [OSF/1] `exit` and `exec` wait until every `fork` already in
progress in the process has finished; once `exit` or `exec` has been
called, no new `fork` starts.
  Source: BOOK §4.3, p. 4-6; BOL §4.1, p. 110, and §6.2, pp. 114-115.
  Test:   many threads fork in a loop while one calls `exec`; every
          child created is complete, and none is created after the
          `exec` image starts.

**PROC-6** [OSF/1] `exec` halts every other thread of the process, as
`exit` does. *Inferred*: the new image starts with one thread (AD3
§10.4 says exec ensures "the single thread nature of the process").
  Source: BOL §4.1, p. 110; AD3 §10.4, p. 21.
  Test:   thread B calls `exec` while thread A loops; A stops; the new
          program has one thread.

**PROC-7** [OSF/1] `exit` in any thread terminates all the threads of
the process: the exiting thread "registers the current thread as
exiting, then halts all other threads", and no further signal
processing takes place.
  Source: BOL §3.2, p. 109.
  Test:   see BUG-6.

**PROC-8** [OSF/1] Only one thread of a parent can wait for a given
child. A `wait` for a child that another thread is already waiting for
fails. Each of several threads waiting on the same or overlapping sets
of children either gets a child or fails; none waits forever. A thread
looking for a child skips one that another thread has claimed.
  Source: BOOK §4.3, p. 4-6; BOL §4.2, p. 110, and §6.2, p. 114.
  Test:   two threads call `waitpid(pid)` for the same child; when it
          exits, one gets its status and the other fails (error number:
          GAP-5).

**PROC-9** [OSF/1] The parent is woken only after the child has
completely terminated, so a parent woken by a child's exit always
finds it.
  Source: BOL §6.2, p. 114.
  Test:   a parent catching `SIGCHLD` and then calling `waitpid(WNOHANG)`
          always reaps the child.

**PROC-10** [OSF/1] When a parent exits before its child, the child is
given to `init` as its new parent.
  Source: BOL §5.4, p. 112.
  Test:   `getppid()` in an orphan returns 1.

**PROC-11** [OSF/1] A child sends its parent `SIGCHLD` when it stops
because of a signal or because it is being traced.
  Source: BOL §5.4, p. 113.
  Test:   a child stops itself with `SIGSTOP`; the parent gets
          `SIGCHLD`.

**PROC-12** [OSF/1] A process that another process has looked up may
begin to exit, ending its other threads, but does not finish exiting
until the other process lets go of it.
  Source: BOL §5.2, p. 112.
  Test:   not testable from user space.

**PROC-13** [OSF/1] The kernel never changes a process's process group
itself; only an explicit system call does.
  Source: BOL §2.1.2, p. 107.
  Test:   a process's group is unchanged across `fork`, `exec` and its
          parent's exit.

**PROC-14** [OSF/1] A signal sent to a process group reaches every
member. A process cannot change groups while a signal to its group is
being delivered.
  Source: BOL §2.1.2, p. 107, and §7.2.2, p. 116.
  Test:   `kill(-pgid, SIGUSR1)` reaches each member.

**PROC-15** [OSF/1] Job control. One process group at a time is the
terminal's foreground group. A background process that tries to read
or write its terminal is suspended until it is brought to the
foreground.
  Source: BOL §2.1.3, p. 107.
  Test:   a background `read` from the terminal stops the process with
          `SIGTTIN`.

**PROC-16** [OSF/1] A session is the processes of one login. It may
hold several process groups, and is associated with one controlling
terminal, given to one group at a time.
  Source: BOL §2.1.4, pp. 107-108.
  Test:   `setsid` makes a new session with no controlling terminal.

**PROC-17** [OSF/1] Signals the terminal sends to a process group also
reach the current process, even when another of its threads has just
moved it to a different group.
  Source: BOL §7.2.3, p. 116.
  Test:   not reliably testable; a race.

**PROC-18** [OSF/1] When a process exceeds its CPU-time limit it is
sent `SIGXCPU`, and the limit is raised "to allow the process time to
process the signal".
  Source: BOL §6.1, p. 114.
  Test:   `setrlimit(RLIMIT_CPU)` with a soft limit of 1 s below the hard
          limit; spin; `SIGXCPU` is caught and the process can go on
          running for a time.

**PROC-19** [OSF/1] User profiling charges each clock tick to the
thread it interrupted; the flag that a profiling-buffer update is due
is kept per thread.
  Source: BOL §6.1, pp. 113-114.
  Test:   `profil` in a two-thread program counts ticks in both.

**PROC-20** [OSF/1] Every process has a user and group identity, a
parent, and possibly children. Each child is alive, terminated or
stopped. Its address space has limits on how far it can grow.
  Source: INT1 slide 2-2, p. 2-6, and its notes, p. 2-7.
  Test:   `getppid`; `waitpid` with `WUNTRACED` reports a stopped child;
          `sbrk` past `RLIMIT_DATA` fails.

**PROC-21** [OSF/1] After `exec` the address space has four regions.
The text is mapped from the file and cannot be written by the process.
The initialised data is mapped from the file copy-on-write, so changes
never reach the file. The BSS and the stack are fresh temporary memory.
  Source: INT1 slides 4-12 and 4-13, p. 4-25; workbook slide 4-13.
  Test:   change an initialised global and the executable's checksum
          is unchanged; a write to a function's address faults; BSS
          starts at zero (INT1 p. 4-29: a new page is "filled with
          zeros").

**PROC-22** [OSF/1] `sbrk` grows the BSS contiguously: the existing
region is extended, so new space starts at the old break.
  Source: INT1 p. 4-27.
  Test:   `p = sbrk(20480); q = sbrk(0);` then `q - p == 20480`, and the
          whole range can be read and written.

**PROC-23** [OSF/1] Under `fork`, each address range is shared, copied
or not inherited, according to its inheritance setting. Each mapping has
a maximum protection, and its current protection can never exceed it.
  Source: workbook pp. 4-9 to 4-10 (`vm_map_entry`).
  Test:   `vm_inherit(VM_INHERIT_NONE)` leaves the range unmapped in the
          child; `SHARE` shares writes; `vm_protect` above the maximum
          fails.

**PROC-24** [OSF/1] The integrated kernel did not export `task_create`
or `task_terminate`: a task and its first thread are made by `fork`.
`task_suspend` and `task_resume` were exported. On Mach a process can
call `task_terminate` itself, and SYS-9 says what the server must do
when it does.
  Source: workbook p. 1-19; PAT §2.2.
  Test:   as SYS-9.

**PROC-25** [OSF/1] Interruptible and uninterruptible waits. A signal
wakes a thread only from an interruptible wait. A signal that "forces
the termination of the thread" wakes it so that it can end. No wakeup may
be lost to a race between a waker and a sleeper.
  Source: INT1 pp. 2-29, 2-33, 2-53 to 2-55, 2-67.
  Test:   `SIGKILL` to a process blocked in a pipe `read` ends it at
          once; a long pipe stress test with many reader and writer
          pairs never hangs.

**PROC-26** [OSF/1] The status character (`^T`) shows, for foreground
processes, why each sleeping thread is waiting.
  Source: INT1 p. 2-53.
  Test:   on a pty with `VSTATUS` set, `^T` while the foreground process
          is blocked in `read` produces a status line.

**PROC-27** [OSF/1] Process IDs. A process is found by indexing a
table with its pid, in such a way that two live pids can never share a
slot; each slot cycles through pids that differ by the table's size, so
a pid is not reused at once. The table's size is the most processes
that can exist at once. The next pid for a slot is
`(pid + npid) % ((PID_MAX / npid) * npid)`.
  Source: PMA §5.5, p. 8; PMD §7.1-7.2, pp. 13-14.
  Test:   pids of live processes are unique and positive; fork until
          `fork` fails [POSIX: `EAGAIN`]. The slot rule is observed, not
          required.

**PROC-28** [OSF/1] Signalling a process that no longer exists fails
with `ESRCH`.
  Source: PMD §11, p. 17.
  Test:   `kill(reaped_pid, 0)` returns −1 with `ESRCH`.

**PROC-29** [OSF/1] `init`, process 1, is the root of the tree of
parents and children. When a process exits, its children are given to
`init`, which waits for them. The exiting process stays on its
parent's list as a zombie until the parent reaps it.
  Source: AD3 §3.0, p. 3, and §5.1, p. 6; PMD §5.4, p. 10.
  Test:   a zombie answers `kill(pid, 0)` until reaped and fails `ESRCH`
          afterwards; an orphan's `getppid()` is 1 and it does not stay a
          zombie after it exits.

**PROC-30** [OSF/1] A child joins its parent's list of children, its
session and its process group. Every process is in exactly one session
(its sid) and one process group (its pgid). `setsid` makes a session
whose sid is the caller's pid, with one process group, also named by
that pid, of which the caller is the only member.
  Source: AD3 §4.2, p. 4; §5.1, p. 6; §6.0, p. 8; §6.2.2, pp. 11-12.
  Test:   after `setsid`, `getsid() == getpgrp() == getpid()` and
          `open("/dev/tty")` fails; a child's sid and pgid are its
          parent's.

**PROC-31** [OSF/1] A process group's job-control count is the number
of its members whose parent is in the same session but a different
group; it changes at `fork`, `exit` and `setpgid`. A group whose count
is 0 is orphaned. When a group becomes orphaned and holds a stopped
process, `SIGHUP` and `SIGCONT` are posted to every member.
  Source: AD3 §4.3, pp. 4-5.
  Test:   P forks C; C makes its own group and stops itself; P exits; C
          receives both (`SIGHUP` first [POSIX]). With no stopped member,
          neither is sent.

**PROC-32** [OSF/1] When a session leader exits, `SIGHUP` is sent to
every member of the foreground process group, the session loses its
controlling terminal, and the terminal's vnode has its aliases cleared
(*inferred*: access through the old descriptors is revoked). The
exiting process closes its files and its usage is gathered for its
parent.
  Source: AD3 §10.2, pp. 19-20.
  Test:   a session leader on a pty with a foreground child exits; the
          child gets `SIGHUP`; its reads of the old descriptor then fail
          (error number: GAP-19); the parent's `RUSAGE_CHILDREN` grows
          after `wait`.

**PROC-33** [OSF/1] `wait` looks through the caller's children for one
that matches. If it finds one, it returns its information and, if it
had exited, reaps it. Otherwise the caller sleeps until a child changes
state.
  Source: AD3 §4.1, p. 4, and §10.3, p. 20.
  Test:   `waitpid(pid)`, `waitpid(-1)`, `waitpid(0)`, `waitpid(-pgid)`;
          `WNOHANG` returns 0 when children exist but none has changed;
          a stopped child is reported only with `WUNTRACED` and is not
          reaped; with no children at all, `ECHILD` [POSIX].

**PROC-34** [OSF/1] A child gets from `fork` its parent's open files
(sharing their offsets), credentials, limits, signal state, current
and root directories.
  Source: AD3 §10.1, p. 19; FSA §3.11 (both of AD's remote fork, which
          copies what `fork` gives).
  Test:   each inherited item checked in the child; pending signals are
          not inherited [POSIX].

**PROC-35** [OSF/1] The process operations that act on another process
are: `fork`, `exit`, `wait`, the nice value of a process and of a
process group (`setpriority`), signalling a process, a process group,
and a process group for terminal access (`SIGTTIN`, `SIGTTOU`),
`setsid`, `setpgid`, reading another process's attributes, making a
process a child of `init`, and `ptrace` of a child. `ptrace` works only
on the caller's children.
  Source: PMA §5.8, table 3, pp. 11-12; PMD appendix A, p. 19; CMUX §4.
  Test:   `setpriority(PRIO_PGRP)` changes every member; `ptrace` of a
          process that is not a child fails; a background write with
          `TOSTOP` set sends `SIGTTOU`.

**PROC-36** [OSF/1] Some calls act on the whole system: getting and
setting the host id, the host name and the domain name, and halting or
rebooting.
  Source: PMA §5.10, p. 12; PMD §9, table 1, p. 16.
  Test:   as root, each round-trips and another process sees the new
          value; as a user, each setter and `reboot` fail [POSIX:
          `EPERM`].

**PROC-37** [OSF/1] The `table` system call reads system and process
tables by command: `TBL_U_TTYD`, `TBL_UAREA`, `TBL_LOADAVG`,
`TBL_INCLUDE_VERSION`, `TBL_ARGUMENTS`, `TBL_MAXUPRC`, `TBL_PROCINFO`,
`TBL_ENVIRONMENT`, `TBL_SYSINFO`, `TBL_DKINFO`, `TBL_TTYINFO`,
`TBL_MSGDS`, `TBL_SEMDS`, `TBL_SHMDS`, `TBL_MSGINFO`, `TBL_SEMINFO`,
`TBL_SHMINFO`, `TBL_INTR`, `TBL_PGINFO`, `TBL_VERSION`, `TBL_MALLOCS`,
`TBL_MALLOCT`, `TBL_MAXUTHREADS`, `TBL_THREAD_MAX`, `TBL_TASK_MAX`,
`TBL_NGROUPS_MAX`, `TBL_KERNEL_ARG_MAX`, `TBL_CLK_TCK`,
`TBL_DEFAULT_STACK_SIZE`, `TBL_DEFAULT_DATA_SIZE`,
`TBL_DEFAULT_ADDR_SPACE`, `TBL_PAGEDAEMONPARAM`, `TBL_STR_PARMS`,
`TBL_MAJNUMSHIFT`, `TBL_GETMODEL`, `TBL_KERNEL_VERSION`,
`TBL_VMSTATS`. Some read another process (`TBL_UAREA`,
`TBL_ENVIRONMENT`, `TBL_U_TTYD`, `TBL_PROCINFO`); PMD's table gives
`TBL_ARGUMENTS` as served for the calling process.
  Source: PMD §9-10 and table 2, pp. 16-17. Five entries are marked
          "????" there (GAP-20).
  Test:   `TBL_ENVIRONMENT` of another process returns its environment;
          `TBL_ARGUMENTS` returns an `argv`; `TBL_NGROUPS_MAX` equals
          `sysconf(_SC_NGROUPS_MAX)`.

**PROC-38** [MK] OSF/1 MK's commands and libraries are R1.2's, except
`ps`, `w`, `uptime`, `vmstat`, `sysversion` (which prints the server's
version string), `motd` and `kloadsrv`. On MK, `ps` makes no Mach
calls; the server provides `getprocthreads()` for it, which must not
leak, and parallel `ps` runs must not deadlock.
  Source: RN501 §3.3 and §4.6.
  Test:   twenty `ps` loops for an hour; the server's size is steady.

**PROC-39** [MK] The system can build itself, and profiling of user
programs works.
  Source: RN501 §4.4 and §4.6.
  Test:   `cc -p` and `-pg` programs write sane profiles; a full build
          of the tree on the system.

## SIG — signals

**SIG-1** [OSF/1] A process receives a signal only by running. The
kernel checks for pending signals each time a thread goes from kernel
mode to user mode, and delivers them then.
  Source: BOOK §4.4.1, p. 4-8.
  Test:   a signal sent to a process blocked in a system call is
          delivered when the call returns or is interrupted.

**SIG-2** [OSF/1] A process may take the default action for a signal,
ignore it, or catch it with a handler.
  Source: BOOK §4.4.2, p. 4-9.
  Test:   each of the three, for `SIGUSR1`.

**SIG-3** [OSF/1] Default actions are: discard (for example `SIGCHLD`);
terminate (for example `SIGKILL`); terminate with a core file (for
example `SIGSEGV`); suspend.
  Source: BOOK §4.4.2.1, p. 4-9.
  Test:   for each signal, the default action matches the signal table
          (GAP-13).

**SIG-4** [OSF/1] `SIGKILL` and `SIGSTOP` can be neither ignored nor
caught.
  Source: BOOK §4.4.2.2, p. 4-10.
  Test:   `sigaction` to ignore or catch either fails [POSIX: `EINVAL`];
          `SIGKILL` still kills.

**SIG-5** [OSF/1] A masked signal is kept pending and delivered when it
is unmasked.
  Source: BOOK §4.4.2.3, p. 4-10.
  Test:   block `SIGUSR1`, send it, check `sigpending`, unblock, the
          handler runs.

**SIG-6** [OSF/1] The signal system calls are POSIX's: `sigaction`,
`sigprocmask`, `sigsuspend` and `sigpending`. BSD and System V signal
interfaces are provided for compatibility (see ERR-4 for whether by
library or by system call).
  Source: BOOK §4.4.3, pp. 4-10 to 4-11; BOL §3, p. 108.
  Test:   each call, and `sigvec`, `sigblock`, `sigsetmask`,
          `sigpause`, `signal`.

**SIG-7** [OSF/1] A signal caused by an exception (`SIGSEGV`, `SIGILL`,
`SIGBUS` and the like) goes to the thread that caused it. Any other
signal goes to the process's "first" thread, the oldest thread still
active.
  Source: BOOK §4.4.4.1, p. 4-11; BOL §3.1, p. 109.
  Test:   thread B divides by zero: the `SIGFPE` handler runs in B.
          `kill(getpid(), SIGUSR1)`: the handler runs in the oldest
          live thread, also after the initial thread has exited.

**SIG-8** [OSF/1] Pending process signals and pending thread signals
are kept apart: a signal raised by a thread's exception is pending on
that thread only.
  Source: BOOK §4.4.4.1, p. 4-11.
  Test:   `sigpending` in another thread does not show a thread's
          pending `SIGSEGV`.

**SIG-9** [OSF/1] Handlers for process signals are per process: if two
threads install different handlers for the same process signal, "the
second installation will overwrite the first". Handlers for thread
signals, those raised by exceptions, are per thread: two threads can
install different handlers for the same one.
  Source: BOOK §4.4.6, pp. 4-16 to 4-17.
  Test:   A and B install different `SIGSEGV` handlers; each faults and
          its own handler runs. A and B install different `SIGUSR1`
          handlers; the second one wins.

**SIG-10** [OSF/1] `sigsuspend` works on the calling thread alone: each
thread has its own saved signal mask for it.
  Source: BOL §4.2, p. 110.
  Test:   two threads each `sigsuspend` with different masks; each wakes
          only for a signal its mask lets through.

**SIG-11** [OSF/1] When a traced process is stopped for its tracing
parent, every thread is stopped; no signal processing happens until the
parent continues it; then all threads continue together.
  Source: BOL §3.2, p. 109.
  Test:   `ptrace` a two-thread program; at a stop, neither thread runs.

**SIG-12** [OSF/1] Once a thread has begun to exit the process, no
further signal processing takes place.
  Source: BOL §3.2, p. 109.
  Test:   a handler is never entered after `exit` has begun.

**SIG-13** [OSF/1] A signal sent from interrupt level, for instance by
the terminal driver, is queued and posted by a kernel thread at base
level.
  Source: BOOK §4.4.4.2, p. 4-12; BOL §7.1, p. 115.
  Test:   not testable; it is why such signals arrive a moment late.

**SIG-14** [OSF/1] Job control by signal: the suspend character stops
the foreground job and the shell's `fg` continues it with the continue
signal. See ERR-1 for which signal the suspend character sends.
  Source: BOOK §4.4, p. 4-7, and §4.4.2.1, p. 4-9.
  Test:   `^Z` stops a foreground `cat`; `fg` resumes it.

**SIG-15** [OSF/1] A process with no handler for `SIGHUP` terminates
when it receives one.
  Source: BOOK §4.4, pp. 4-7 to 4-8.
  Test:   `kill(pid, SIGHUP)` on a default child; it dies of `SIGHUP`.

**SIG-16** [OSF/1] At shutdown the kernel sends `SIGKILL` to every
active process.
  Source: BOOK §4.4.2.1, p. 4-9.
  Test:   not required of the server; `init` and `reboot` do this.

**SIG-17** [MK] The goal is the integrated kernel's signal semantics.
The emulator-based OSF/1 MK never matched them after more than twelve
releases, which is one reason the emulator was removed.
  Source: PAT §2.4.
  Test:   the whole SIG section.

**SIG-18** [OSF/1] The signal frame is built on the real user stack,
the same however the signal is delivered, and it must be exactly what
the C library's signal code expects; the return from a handler goes
through `sigreturn`, which undoes it.
  Source: DEAN §5.1; PAT §2.4.
  Test:   a handler returns normally and the interrupted code continues
          with all registers intact; `longjmp` out of a handler works.

**SIG-19** [OSF/1] Most signal state belongs to the process, not to
the thread: the signal mask, the dispositions and the set of pending
signals. Only synchronous signals, those raised by exceptions, also
have a per-thread disposition.
  Source: INT1 p. 2-71; workbook slide 2-34, p. 2-45.
  Test:   thread A blocks `SIGUSR1` with `sigprocmask`; thread B's mask
          shows it blocked; `sigpending` is the same in both.

**SIG-20** [OSF/1] An asynchronous signal goes to the first thread
created in the process. If that thread has ended, it goes to the
second, and so on.
  Source: INT1 slide 2-34, p. 2-71; workbook p. 2-45.
  Test:   threads T1 (initial), T2, T3; the `SIGUSR1` handler runs on T1;
          end T1 and it runs on T2; end T2 and it runs on T3.

**SIG-21** [OSF/1] The course itself says there is "no universally
accepted semantics" for signals in multithreaded processes. Beyond
SIG-7 to SIG-10 and SIG-19 to SIG-20, the behaviour is not specified.
  Source: INT1 p. 2-71.

**SIG-22** [OSF/1] `ptrace`. A child lets its parent trace it by
calling `ptrace` with request 0. From then on it stops whenever it
receives a signal. The parent learns of the stop through `wait`, and
sends requests with positive request numbers. It can read and change
the child's memory and registers, and decide what the child does with
the signal. The memory transfers use Mach's calls for reading and
writing another task's address space.
  Source: INT1 slide 2-35, p. 2-73; workbook p. 2-46.
  Test:   the child calls `ptrace(0, 0, 0, 0)`, then
          `kill(getpid(), SIGUSR1)`; the parent's `wait` shows it
          stopped with `SIGUSR1`; peek and poke a variable; continuing
          with signal 0 discards the signal; a breakpoint written into
          text works. `SIGKILL` kills without a stop [4.3BSD].

**SIG-23** [OSF/1] Sending a signal makes it pending on the target; if
the thread that will take it is blocked in a system call, the call is
broken off before delivery is attempted.
  Source: PMA appendix A, `kill()`, p. 16; PMA §5.9, p. 12.
  Test:   a child blocked in `read` on a pipe takes a caught signal; the
          handler runs and `read` returns `EINTR` or restarts (SYS-12).
          A signal sent just as a call finishes, in a stress loop, never
          hangs anything.

**SIG-24** [OSF/1] A handler returns through `sigreturn`, which unwinds
the stack and clears the kernel's signal state for the handler [POSIX:
the mask in force before the handler is restored]. A handler sees only
part of the machine's registers.
  Source: CMUX §4-5.
  Test:   after a handler installed with `sa_mask` returns, the old mask
          is back.

**SIG-25** [OSF/1] The default actions, completed: nothing, stop,
continue from a stop, terminate, terminate with a core file.
  Source: CMUX §4; INT1 p. 2-69.
  Test:   `SIGCHLD` ignored, `SIGTSTP` stops, `SIGCONT` continues,
          `SIGTERM` terminates, `SIGQUIT` dumps core.

**SIG-26** [MK] A process can dump core; machine-specific information
for the core file is supplied per machine. [BSD: the core carries the
registers and a debugger can read it.]
  Source: DEAN §4.1; PAT §2.4.
  Test:   `SIGQUIT` with cores enabled; `gdb` shows the stack.

**SIG-27** [DEC] DEC OSF/1 3.0 changed SIG-19 and SIG-20 for threaded
programs (each thread its own mask; `kill` delivers to the main thread
only) and the pipe restart rule (GAP-18). Those are DEC's changes, not
OSF/1's, and are not required.
  Source: DRN3 §4.4.2, pp. 4-12 to 4-13, and §4.4.18.3, p. 4-20.

## EXC — exceptions and the UNIX exception handler

**EXC-1** [OSF/1] A system-wide "UNIX exception handler" fields every
exception no other handler takes, turns it into the matching signal and
posts it to the thread. It listens on `init`'s task exception port.
Every task inherits its task exception port from its parent, so every
process reaches the UNIX handler unless an ancestor installed another.
  Source: BOOK §4.4.5.1, p. 4-14.
  Test:   a fault in any process raises the matching signal.

**EXC-2** [OSF/1] Exception handlers may be per task (debuggers use
these) or per thread (error handlers). A thread's own exception port is
null by default; a thread that sets none sends its exceptions to the
task's port.
  Source: BOOK §4.4.5.2-4.4.5.3, pp. 4-15 to 4-16.
  Test:   a program sets a thread exception port and catches its own
          fault there; no signal is raised.

**EXC-3** [OSF/1] Exception handlers and signal handlers are different
things: the UNIX exception handler turns an exception into a signal,
and the process's signal handler, if any, then runs.
  Source: BOOK §4.4.5.2, p. 4-15.
  Test:   as EXC-1 with a `SIGSEGV` handler installed.

**EXC-4** [OSF/1] The exception RPC uses three ports: the thread
exception port, the task exception port, and the thread's "exception
clear port", on which the handler's reply arrives. A thread without one has it
allocated at its first exception.
  Source: BOOK §4.4.5.3, pp. 4-15 to 4-16.
  Test:   not testable from user space. (This is the Mach 2.5
          mechanism; on OSF Mach the reply goes to a send-once right,
          EXC-6.)

**EXC-5** [MK] Which signal each exception becomes is machine-dependent
and is set by a per-machine table.
  Source: DEAN §4.5.
  Test:   see GAP-7.

**EXC-6** [Kernel] A handler's return value decides what happens:
`KERN_SUCCESS`, the thread continues from the exception, perhaps with
new state; `MIG_NO_REPLY`, the handler has already restarted or ended
the thread; `MIG_DESTROY_REQUEST`, the kernel tries the next handler or
ends the thread. The kernel sends the exception with a send-once right,
which `thread_abort` destroys.
  Source: KI `catch_exception_raise`.
  Test:   not testable from user space.

**EXC-7** [Kernel] An exception port may be set as *protected*.
`thread_abort` on a thread handling a protected exception fails with
`KERN_EXCEPTION_PROTECTED`.
  Source: KI `task_set_exception_ports`, `thread_abort`.
  Test:   not testable from user space.

**EXC-8** [OSF/1] A thread that raises an exception waits until a
handler has dealt with it. The handler either clears the exception, and
the thread resumes, or ends the thread. By default the exception goes
to the handler that turns it into a signal: the signal is made pending
on the faulting thread, the exception is cleared, and the thread deals
with the signal in its own context.
  Source: INT1 slides 2-36 and 2-37, pp. 2-75 to 2-77.
  Test:   the `SIGFPE` handler for a divide by zero in thread B runs on
          B's stack.

**EXC-9** [OSF/1] A child task inherits its parent's task exception
port.
  Source: INT1 p. 2-77; workbook p. 2-48.
  Test:   the parent sets its task exception port to P and forks; a
          fault in the child produces a message on P.

**EXC-10** [Kernel] An exception RPC stops only the thread that took
the exception; the task's other threads keep running.
  Source: CMUX §8.
  Test:   thread A faults while B counts; B's count keeps rising until
          the signal is delivered.

## THR — stopping and redirecting a thread

**THR-0a** [OSF/1] `thread_create` makes a thread in the suspended
state, with no user context. The caller gives it registers, a stack and
a starting address with `thread_set_state`, and starts it with
`thread_resume`.
  Source: INT1 slide 2-38, p. 2-79.
  Test:   a new thread does not run until resumed; then it runs from the
          given address.

**THR-0b** [OSF/1] `thread_suspend` stops a thread at the point where
it is about to return to user mode, after a trap has finished changing
its user state, so that the state can be changed reliably. Suspends
nest: each needs its own resume. A thread suspended in an interruptible
wait stops when it wakes; in an uninterruptible wait, the suspension
takes effect afterwards.
  Source: INT1 slide 2-39, p. 2-81, and p. 2-51.
  Test:   suspend twice and resume once: the thread is still stopped.

**THR-0c** [OSF/1] `thread_terminate` ends another thread at a clean
point. A thread may also end itself; it is removed after it has given up
the processor.
  Source: INT1 slide 2-40, p. 2-83.
  Test:   T1 ends T2 while T2 is blocked in `read`; T2 leaves
          `task_threads`; the process's other threads go on.

**THR-0d** [OSF/1] Pthreads is a user-level library over these calls.
It keeps a cache of kernel threads: when a Pthread ends, its kernel
thread may only be suspended, ready for reuse.
  Source: INT1 p. 2-85.
  Test:   see GAP-15.

**THR-1** [Kernel] To deliver a signal to a thread the documented
sequence is `thread_suspend`, `thread_abort` (to break off any system
call in progress and make it return "interrupted"), `thread_set_state`
(to make it look as if the handler had been called), `thread_resume`.
  Source: KI `thread_abort`, NOTES.
  Test:   SIG items above.

**THR-2** [Kernel] After `thread_abort` the thread "will appear to have
just returned from the kernel". An interrupted call may have been
completed, abandoned or done in part. A page fault that was aborted is
retried if the thread's state is not changed. `mach_msg` returns
`MACH_SEND_INTERRUPTED` or `MACH_RCV_INTERRUPTED`; the `mach_msg`
library routine retries these by default.
  Source: KI `thread_abort`.
  Test:   a server-side unit test; not visible to a process.

**THR-3** [Kernel] `thread_abort_safely` refuses, with `KERN_FAILURE`,
to break off an operation that cannot be restarted, exception
processing included; the caller must let the thread run and try again.
`thread_abort` aborts such operations at an arbitrary point (but see
EXC-7).
  Source: KI `thread_abort_safely`.
  Test:   a server-side unit test.

**THR-4** [Kernel] Neither should be used on a thread that is not
suspended.
  Source: KI `thread_abort`, `thread_abort_safely`, CAUTIONS.
  Test:   a review rule, not a test.

## FD — descriptors

**FD-1** [OSF/1] A descriptor selects an entry in the process's open
file table; the entry points to an open file description holding the
offset, the type, a reference count, the operations, the access the
file was opened with (read-only, read-write) and the vnode or socket.
  Source: BOOK §11.1, p. 11-3; INT2 p. 5-9.
  Test:   by the items below.

**FD-2** [OSF/1] `open` allocates the lowest-numbered free descriptor.
  Source: INT2 p. 5-9.
  Test:   close 3, open twice: 3, then the next free.

**FD-3** [OSF/1] All threads share the open file table. A descriptor one
thread obtains is at once usable by all the others.
  Source: BOOK §11.1, p. 11-3.
  Test:   A opens, B reads through the number A got.

**FD-4** [OSF/1] `fork` copies the table, so parent and child share every
open file description and see the same offset. Descriptors either gets
afterwards are not shared.
  Source: BOOK §11.1, p. 11-3, and §11.1.1, p. 11-5.
  Test:   the child reads 10 bytes; the parent's offset has moved by 10.

**FD-5** [OSF/1] A descriptor marked close-on-exec is closed by `exec`;
the others stay open.
  Source: BOOK §11.1, p. 11-4.
  Test:   `FD_CLOEXEC` on one of two descriptors; after `exec` only the
          other is open.

**FD-6** [OSF/1] A second, independent `open` of the same file gives a
separate description with its own offset.
  Source: BOOK §11.1.1, p. 11-5.
  Test:   two `open`s; reading through one does not move the other.

**FD-7** [OSF/1] `dup` shares the description: both descriptors move one
offset, and the description's reference count counts both.
  Source: INT2 p. 5-10 (the `dup` example).
  Test:   `write(fd, 20 bytes)`; `lseek(dup, 0, SEEK_CUR)` is 20.

**FD-8** [OSF/1] Processes "do not normally share sockets, except by
inheritance through fork()".
  Source: BOOK §11.1.1, p. 11-5.
  Test:   (descriptor passing is outside this statement.)

**FD-9** [OSF/1] While `open` runs, the descriptor it has picked is
reserved: no other thread can be given that number, and it does not
refer to the file until the `open` has finished.
  Source: INT2 p. 5-9.
  Test:   thread A opens a FIFO, blocking; thread B opens files in a
          loop and never gets A's number; until A's `open` returns, B's
          `fstat` on that number fails `EBADF`.

**FD-10** [OSF/1] A thread can never be handed a descriptor that refers
to nothing because another thread closed it at the same moment.
  Source: BOOK §11.1.2, p. 11-7.
  Test:   threads `dup` and `close` the same descriptors in a loop; no
          call returns a descriptor that then fails with `EBADF` for
          that reason.

**FD-11** [OSF/1] If one thread closes a descriptor while another is in
the middle of I/O on it, the file stays open until the I/O call
returns. Every I/O call except `close` holds its own reference for its
duration; `open` holds one until it returns.
  Source: INT2 p. 5-9.
  Test:   A blocks in `read` on a pipe; B closes the descriptor; a write
          to the pipe completes A's `read` normally.

**FD-12** [OSF/1] Operations that use or move the offset (`read`,
`write`, `lseek`) are serialised on the description, so concurrent
writes through one description never overwrite each other.
  Source: BOOK §11.1.2, p. 11-7.
  Test:   two threads, and then two processes after `fork`, each write
          1000 records of 100 bytes through one description; the file
          is 200,000 bytes and every record is whole.

**FD-13** [OSF/1] Two separate opens of one file overwrite each other:
1,000 bytes written through one leave the other's offset at 0, so a
write through the other starts at the beginning of the file.
  Source: INT2 slide 5-4, p. 5-11, and slide 5-5, p. 5-13.
  Test:   write 1,000 bytes through fd 1, then "XYZ" through fd 2; the
          file is 1,000 bytes long and begins "XYZ".

**FD-14** [OSF/1] Every `read` and `write` verifies "that the user has
permission to perform the desired operation", read here (ERR-11) as a
check against the descriptor's open mode. Permission on the file itself is
checked only at `open`: changing the file's mode later does not take
away access through descriptors already open.
  Source: INT2 slide 5-14, p. 5-31; workbook p. 5-77.
  Test:   `write` on an `O_RDONLY` descriptor fails [POSIX: `EBADF`];
          open read-write, `fchmod` to 0444, `write` still succeeds.

**FD-15** [OSF/1] `readv` and `writev` move data to and from a buffer in
several pieces, in order, as one request.
  Source: INT2 slide 5-15, p. 5-33.
  Test:   `writev` three pieces and read back their concatenation; the
          offset moves by exactly the count returned.

**FD-16** [OSF/1] FD-12's lock applies to "regular files and
directories". The documents do not say whether it applies to pipes,
terminals or other devices, or to `O_APPEND` writes through separate
opens.
  Source: INT2 p. 5-31.

## FILE — files, special files and terminals

**FILE-1** [OSF/1] Two kinds of file lock are supported: POSIX record
locks (`fcntl`) and BSD `flock`.
  Source: BOOK §11.3.4, p. 11-24.
  Test:   both kinds, including the POSIX rule that a process does not
          conflict with its own locks.

**FILE-2** [OSF/1] FIFOs and device special files use common
file-system-independent operations on every file system that supports
them, with some help from the file system where needed. A write to one
updates its modification time.
  Source: BOOK §11.3.5, p. 11-24.
  Test:   write to a FIFO; its `st_mtime` changes.

**FILE-3** [OSF/1] Several device files for the same device behave as
one device. Access to a character device such as a terminal can be
revoked: every file referring to it then fails, while a fresh open of
the same name works.
  Source: BOOK §11.3.5.1, p. 11-25.
  Test:   `revoke` a pty slave held by another process; that process's
          reads fail; a new `open` succeeds.

**FILE-4** [OSF/1] The block device of a mounted file system cannot be
opened, and a file system whose block device is open cannot be
mounted.
  Source: BOOK §11.3.5.1, p. 11-25.
  Test:   open the root's block device: fails (error number: GAP-9).

**FILE-5** [OSF/1] Two block-device files for one device share one
buffer cache, so what is written through one is read through the other.
  Source: BOOK §11.3.5.1, p. 11-25.
  Test:   `mknod` a second name for a spare partition; write through one,
          read through the other.

**FILE-6** [OSF/1] Some character devices are clonable: each open makes
a new device with a new, unique device number. `fstat` and modification
times work on the clone.
  Source: BOOK §11.3.5.2, p. 11-27.
  Test:   if a clone device is provided, two opens give two `st_rdev`.

**FILE-7** [OSF/1] FIFOs are always available; `mkfifo` is a library
routine over `mknod`, needs no privilege, and sets the new file's
access, change and modification times and the directory's change and
modification times. Making anything other than a FIFO with `mknod`
needs privilege (`EPERM` otherwise). `mkfifo`'s errors: `EACCES`,
`EPERM`, `EEXIST`, `EROFS`, `ENOSPC`, `EDQUOT`, `ELOOP`,
`ENAMETOOLONG`, `ENOENT`, `ENOTDIR`; on error nothing is created.
  Source: DEC10 `mkfifo(3)`.
  Test:   each error, and an unprivileged `mkfifo`.

**FILE-8** [OSF/1] UFS stores a symbolic link short enough to fit in the
inode's block pointers in the inode itself, marked by a flag. Nothing
outside UFS can see the difference.
  Source: BOOK §11.4.2.3, p. 11-35.
  Test:   only if file systems are shared with OSF/1's UFS.

**FILE-9** [OSF/1] The file system's user interface is UNIX's; the
implementation is mainly 4.4BSD's. The local file systems are UFS and
System V's S5, and NFS is supported.
  Source: INT2 p. 5-5 and p. 5-17.
  Test:   the suite runs on UFS and NFS; S5 is out of scope (GAP-16).

**FILE-10** [OSF/1] A file system is a device named by a special file
in `/dev`, and it is seen only once mounted. Mounting puts the file
system's root over the directory mounted on, whose earlier contents can
no longer be seen. `..` from the root of a mounted file system leads to
the parent of the directory it covers.
  Source: INT2 slides 5-9 and 5-10, pp. 5-21 to 5-23; slide 5-35,
          p. 5-73.
  Test:   as root, mount on a directory holding `hidden`: `hidden` is
          gone and `stat` shows the new root; `/mnt/x/..` is `/mnt`;
          after unmounting `hidden` is back.

**FILE-11** [OSF/1] Path lookup starts at the root directory for an
absolute path and at the current directory otherwise, takes one
component at a time, separated by `/`, and fails at the first
component that is missing.
  Source: INT2 slides 5-32 and 5-34, pp. 5-67 and 5-71.
  Test:   relative and absolute opens; `a//b` is `a/b`; a missing
          middle component fails [POSIX: `ENOENT`].

**FILE-12** [OSF/1] When lookup meets a symbolic link, the part of the
path followed so far is replaced by the link's contents and the search
starts again. No one lookup may pass through more than `MAXSYMLINKS`,
32, links.
  Source: INT2 slide 5-36, p. 5-75.
  Test:   `/A/E -> /B/C`, then `open("/A/E/G")` opens `/B/C/G`; a chain of
          32 links resolves and 33 fails [POSIX: `ELOOP`]; a link to
          itself fails. A relative link resolves from its own directory
          (ERR-8).

**FILE-13** [OSF/1] An NFS client sees a remote file system without the
server's own mounts; only the client splits a path into components.
  Source: INT2 slide 5-34, pp. 5-71 to 5-73.
  Test:   with an NFS server whose export has a mount inside it, the
          client sees the directory underneath.

**FILE-14** [OSF/1] When two threads or processes remove the same
directory entry at once, exactly one succeeds. If a directory changes
between the lookup and the change that depends on it, the lookup is
repeated; if the second lookup fails, the operation fails. An operation
never acts on a different entry that has taken the old one's place.
  Source: INT2 slide 5-37, p. 5-77.
  Test:   two threads `unlink("f")` together, many times: one returns 0,
          the other −1 [POSIX: `ENOENT`]. Two threads unlink `A` while a
          third creates `B` in the same directory: `B` always survives.

**FILE-15** [OSF/1] A removed name never resolves to a different file
that has taken over the old file's internal node.
  Source: INT2 slide 5-39, p. 5-81.
  Test:   `stat("a")`, `unlink("a")`, create and remove many files;
          `stat("a")` fails.

**FILE-16** [OSF/1] `mmap` maps a file or makes anonymous memory. A
private mapping's changes are not seen by others and never reach the
file. A shared mapping's changes are seen by the other shared mappings
and reach the file. A private mapping does not see changes made through
shared mappings. Anonymous memory is shared with all the process's
descendants (ERR-9).
  Source: INT2 slide 5-31, p. 5-65.
  Test:   write through `MAP_PRIVATE`, then `read` the file: unchanged.
          Two `MAP_SHARED` mappings see each other's writes, and `read`
          sees them after `msync`. P maps privately and touches the page;
          S changes it through a shared mapping; P still sees the old
          value.

**FILE-17** [OSF/1] *Documented defect.* A file used through both
`mmap` and `read`/`write` could show two different versions of a block,
one in the buffer cache and one in a page. Not to be reproduced: the
two must agree.
  Source: INT2 p. 5-65.
  Test:   write through `write`, read through a shared mapping, and the
          reverse.

**FILE-18** [OSF/1] A file that is open survives the removal of its
last name and is freed when it is closed. Holes in a file read as
zeros.
  Source: workbook p. 5-76 and p. 5-51; INT2 p. 5-89.
  Test:   create, unlink, write and read back; `fstat` shows
          `st_nlink == 0`. Write one byte at offset 2,000,000,000:
          the size is that plus one, and earlier bytes read as zero.

**FILE-19** [OSF/1] NFS departs from UNIX in documented ways. Removing
a file that is open is turned into a rename to a temporary name, with
the real removal at close. The server gives the file's owner read,
write and execute access whatever the mode; the client refuses writes
through a read-only descriptor. Processes on different machines may
see different data. A hard mount blocks while the server is down,
interruptibly only if so mounted, and then not at once; a soft mount
returns `ETIMEDOUT`.
  Source: workbook pp. 5-76 to 5-77 and 5-91 to 5-92.
  Test:   needs an NFS server that can be stopped.

**FILE-20** [OSF/1] FIFOs, sockets and STREAMS devices have names in
the file system, and descriptors can be passed from one process to
another.
  Source: FSA §3.6 and §3.12.
  Test:   `bind` an `AF_UNIX` socket and `stat` shows `S_IFSOCK`; pass a
          descriptor over it and the receiver shares the offset.

**FILE-21** [OSF/1] Process management posts `SIGXFSZ` when a write
gets `EFBIG` ("e.g., SIGXFZ in the event that EFBIG is returned from a
remote file server"). [POSIX: writing past `RLIMIT_FSIZE` raises it;
if caught or ignored, the write fails with `EFBIG`.]
  Source: FSA §3.11.
  Test:   `setrlimit(RLIMIT_FSIZE)` small, then write past it.

**FILE-22** [OSF/1] STREAMS is part of the system: R1.2's SVR4 STREAMS,
reached through `open`, `close`, `putmsg`, `getmsg` and `ioctl`, with a
STREAMS terminal interface beside the BSD one. OSF's own MK server
could not be built without it ([2395]).
  Source: DTO3 §3.4.3, p. 3-16; RN73 §3.3 [2395], p. 59.
  Test:   when STREAMS is provided, `putmsg` and `getmsg` on a STREAMS
          pipe, and `I_PUSH` of a module. See GAP-21.

**FILE-23** [OSF/1] The C2 configuration's object reuse: storage is
cleared before it is given to another user, so new memory and the new
part of a lengthened file read as zero.
  Source: DTO3 §11.1-11.3, pp. 11-1 to 11-6 (C2 from OSF); RN501 §4.2
          (C2 and B1 implemented in MK5.0).
  Test:   `sbrk` and `vm_allocate` memory, and an `ftruncate` upward,
          read as zero.

**FILE-24** [DEC] UFS allows 255-byte path components and 1,023-byte
paths. DEC's own limit; the OSF documents read give none.
  Source: DTO3 §4.3, p. 4-3, and appendix A, p. A-10.
  Test:   a 255-byte name works and 256 fails `ENAMETOOLONG`.

## TTY — terminals and job control

**TTY-1** [OSF/1] A session is a terminal session: the processes that
share one terminal. Signals from the keyboard go to the foreground
process group only.
  Source: INT2 slide 6-17, p. 6-35.
  Test:   on a pty, the interrupt character sends `SIGINT` to the
          foreground group and not to a background one.

**TTY-2** [OSF/1] A process that makes a new process group leads it,
and the group's ID is that process's pid. Its children stay in the group
unless they move.
  Source: INT2 p. 6-35.
  Test:   `setpgid(0, 0)`, then `getpgrp() == getpid()`; a child has the
          parent's group.

**TTY-3** [OSF/1] Each process group is a job that can be suspended,
put in the foreground or put in the background. A stop signal to the
group suspends every thread of every process in it; a continue signal
resumes a stopped process.
  Source: INT2 p. 6-35.
  Test:   `killpg(SIGSTOP)` on a group holding a multithreaded process:
          no thread runs; `SIGCONT` resumes it. See ERR-12.

**TTY-4** [OSF/1] A background process reading its terminal is sent
`SIGTTIN`.
  Source: INT2 p. 6-35.
  Test:   `waitpid(WUNTRACED)` shows the reader stopped by `SIGTTIN`.

**TTY-5** [OSF/1] In an orphaned process group, a read from the
terminal that would have raised a stop signal does not stop the
process; the read fails with `EIO`. (The course notes that BSD killed
the process instead.) A group is orphaned when its parent leaves and
`init` adopts its members, and stops being orphaned when a process whose
parent is in the session joins it. ERR-10 gives the course's three
definitions.
  Source: INT2 p. 6-35 and slide 6-18, p. 6-37.
  Test:   as slide 6-18: D makes a group with children E and F and exits;
          E's background `read` returns −1 with `EIO`, and E is neither
          stopped nor killed. Move C into D's group and E's `read` raises
          `SIGTTIN` again.

**TTY-6** [OSF/1] The kernel and libraries are 8-bit clean: no special
use is made of the eighth bit, and quoting is not done by setting it.
  Source: INT2 slide 6-16, p. 6-33, and p. 6-45.
  Test:   bytes 0x80 to 0xff pass through a pty unchanged in raw mode;
          in canonical mode a byte with the high bit set is never taken
          for an editing character.

**TTY-7** [OSF/1] Canonical ("cooked") input can be edited and becomes
readable only when a line delimiter arrives. In raw and cbreak modes
input is not edited and can be read at once; in raw mode output is sent
without processing. With echo on, input is echoed.
  Source: INT2 slide 6-21, p. 6-43; workbook glossary.
  Test:   write `abX<ERASE>c` with no newline: a non-blocking read gets
          nothing; after the newline it gets `abc\n`. In non-canonical
          mode each byte is readable at once. `ECHO` on and off.

**TTY-8** [OSF/1] A terminal read with nothing to read blocks, if
blocking is allowed, until characters arrive, then copies what is
available.
  Source: INT2 p. 6-43.
  Test:   a blocking read waits for the master to write; a non-blocking
          one fails [POSIX: `EAGAIN`].

**TTY-9** [OSF/1] Output to a terminal goes through output processing
in the line discipline. A quoted input character is plain data, never
an editing or job-control character.
  Source: INT2 pp. 6-43 to 6-45.
  Test:   with `OPOST|ONLCR`, `\n` reads back as `\r\n`; `^V` then
          ERASE puts the ERASE byte in the line.

**TTY-10** [OSF/1] A pseudo-terminal is a pair of devices, a control
side and a slave side. What is written to the control side reaches the
slave as terminal input, through the line discipline; what is written
to the slave goes through the line discipline and is read from the
control side. The line discipline runs on the slave's machine, so an
application on the slave can change the terminal's modes.
  Source: INT2 slide 6-23, p. 6-47.
  Test:   round trips both ways; `tcsetattr` on the slave takes effect.

**TTY-11** [OSF/1] Each terminal has a line discipline, found from its
tty structure; the course's example uses the standard one.
  Source: INT2 p. 6-43.
  Test:   `TIOCGETD` returns the standard discipline.

**TTY-12** [OSF/1] A character device read or write reports an I/O
error as `EIO` and a bad buffer address as `EFAULT`; a partial transfer
reports only the bytes moved.
  Source: workbook p. 6-17.
  Test:   `read(ttyfd, (void *)1, 10)` fails with `EFAULT`.

## EXEC — running programs, and the secure configurations

**EXEC-1** [OSF/1] A program that is in a format the kernel knows, fully
relocated and with no unresolved references, is loaded directly by
`exec` and runs when `exec` returns to user mode.
  Source: INT2 slide 10-3, p. 10-7.
  Test:   `exec` a static program.

**EXEC-2** [OSF/1 1.0-1.2] Any other program was started by loading the
user-mode run-time loader with the program's name. That loader handled
formats the kernel did not know, shared libraries, packages, the known
package tables, symbol substitution, pre-relocated libraries and
loading and unloading modules while running. *Out of scope*: OSF/1 1.3
and later, MK6 and MK7 use ELF and `rtld` (`docs/roadmap.md`, "What is
not in scope").
  Source: INT2 slides 10-3 to 10-13, pp. 10-7 to 10-27.

**EXEC-3** [OSF/1] Kernel modules are loaded and unloaded by a
privileged user-mode kernel-loader server, through special system
calls. *Inferred*: unprivileged callers are refused.
  Source: INT2 slide 10-14, p. 10-29.
  Test:   not required until loadable subsystems are.

**EXEC-4** [OSF/1] In the secure configurations, privilege sets govern
`exec` and `setpriv`, writing an executable clears its privileges, and
running a setuid program may need the `execsuid` privilege. *Out of
scope* until a secure configuration is built (MK5.0 shipped C2 and B1
configurations, RN501 §4.2).
  Source: INT2 slides 11-17 to 11-20, pp. 11-35 to 11-41; workbook
          pp. 11-20 and 11-26.

**EXEC-5** [OSF/1] `exec` and `fork`, among other calls, can be
audited.
  Source: workbook slide 11-6.
  Test:   not required.

## DEV — devices

**DEV-1** [MK] Major and minor numbers are the integrated kernel's for
the same devices, and a device's kernel name is usually its name in
`/dev`.
  Source: DEAN §4.4.
  Test:   `ls -l /dev` numbers match the table (GAP-14).

**DEV-2** [MK] The SCSI disk `sd0a` is major 3, minor 0.
  Source: RN73 §3.3 [2271], p. 60.
  Test:   `stat /dev/sd0a`.

**DEV-3** [MK] Device names: `hd` for AT, IDE and ESDI disks, `sd` for
SCSI disks, units from 0 and partitions `a` to `p`; floppies `fd0`;
Ethernet `wd0`, `wd1` (SMC/WD 80x3) and `tu0` (the DEC DE-450
"TULIP" PCI adapter).
  Source: RN73 §2.4.2.1, p. 37 (`fd0b`), §2.4.2.3, p. 39, §2.5.3,
          p. 49, and §2.7.1, p. 51.
  Test:   the names exist.

**DEV-4** [MK] `/dev/zero` exists.
  Source: RN73 §3.3 [277], p. 61; RN61 [277].
  Test:   read 1 MB from it; all zeros.

## MEM — memory and paging

**MEM-1** [MK] Anonymous `mmap` memory is backed by the server's vnode
pager. Memory made by Mach services, `vm_allocate` and copy-on-write,
is backed by the default pager. Programs with shared libraries are
loaded by `mmap` and so backed by the vnode pager; statically linked
programs are backed by the default pager.
  Source: RN73 §2.7.3, p. 52; RN61 "OSF/1 MK Paging", p. 16.
  Test:   not visible to a program except through paging statistics.

**MEM-2** [MK] The paging partition is given either as the default
pager's argument in `bootstrap.conf` (`sd0b`), or by an `sw` entry in
`/etc/fstab` used by `swapon`.
  Source: RN73 §2.7.3, p. 52.
  Test:   both ways.

**MEM-2a** [MK] Under MK5.0 there was no swapping and no clustered
paging, and one paging file, fixed at boot, could not be changed
afterwards. MK7.3 allows `swapon` from `/etc/fstab` (MEM-2).
  Source: RN501 §4.1.
  Test:   historical.

**MEM-3** [MK] Under MK6.1 the default pager's paging file was named by
the symbolic link `/mach_servers/paging_file`: `/dev/hd0b/filename`
for a file in the root of the file system on `hd0b`, or `/dev/hd0b` for
the whole partition. A larger file is used from the next boot. The
vnode pager's file came from `/etc/rc.config` or `/etc/fstab`.
  Source: RN61 pp. 16-17.
  Test:   historical; not required.

## SEC — a second server

**SEC-1** [MK] A second OSF/1 server behaves like the first and "should
not be able to tell that it is not the first server". The first server
may interpose on the host and device ports it hands on.
  Source: COLD §5.1, p. 17.
  Test:   boot a second server under the first; it reaches single-user
          mode (see BUG-11).

**SEC-2** [MK] A server started as a second server is given the `-2`
option.
  Source: RN73 §3.6 [105], p. 63; RN61 [105].
  Test:   as SEC-1.

**SEC-3** [MK] A second server has "unix system call access to an
underlying unix", the first server, through private copies of `read`,
`write`, `open`, `close` and the like, since it implements those names
itself and cannot link the C library's.
  Source: DEAN §4.6 (of the emulator-based AD1 server).
  Test:   as SEC-1.

## COLO — the server loaded into the kernel

**COLO-1** [MK] The same server runs as a user task or loaded into the
kernel and need not know which; a collocated program "runs almost
exactly as if it were in user space".
  Source: COLP §3.1, p. 6; RN73 §1.3, pp. 15-16.
  Test:   the whole suite passes against both builds.

**COLO-2** [MK] Kernel-loaded programs share one port name space, in
which names are the kernel's own port addresses. So they "may not
rename or destroy ports".
  Source: COLP §3.1, p. 6.
  Test:   a review rule.

**COLO-3** [MK] A kernel-loaded server's threads stay preemptible and
its text and data pageable; on the i386 its thread stacks are wired.
  Source: COLP §3.1, p. 6 and note 3; COLD §4, objections 6-7.
  Test:   not visible to a process.

**COLO-4** [MK] A kernel-loaded server's use of a dead port name
"cannot be detected", so it must never use one: it must know of
every task or thread that ends.
  Source: RN73 §3.3 [653], pp. 60-61.
  Test:   `ps ax` in a loop while processes exit; nothing fails (see
          BUG-12).

**COLO-5** [MK] A "uniprocessor" configuration runs all significant
server work under one global lock, so server threads are not preempted
by one another and the fine-grained locks can be left out.
  Source: COLP §4, p. 12; RN61 [615].
  Test:   not visible to a process; see BUG-13 and BUG-14.

## LIB — the C threads library the server is built on

**LIB-1** [Kernel] A C thread's stack size is `cthread_stack_size`,
fixed at compile time by `vm_size_t cthread_stack_size = N;` and a
multiple of the page size. All C thread stacks are the same size and
page aligned.
  Source: SL `cthread_stack_size`, p. 61; SWG, "Stack Management",
          p. 51.
  Test:   a server-side unit test.

**LIB-2** [Kernel] A C thread ends when it returns from its top-level
function or calls `cthread_exit`. An undetached thread waits for a
join. The initial thread, on returning from `main`, waits for every
other thread and then ends the task. Joining oneself deadlocks.
  Source: SWG, "Termination" and "Thread Management", pp. 50-51.
  Test:   a server-side unit test.

**LIB-3** [Kernel] `cthread_mach_msg` is `mach_msg` with a limit on how
many C threads may wait on one port; a thread over the limit blocks as
a C thread, freeing its Mach thread, and is woken when the number of
waiting threads falls below the minimum. `cthread_msg_active` reserves
a place in advance; `cthread_msg_busy` gives it up. The library is
"Not defined anywhere".
  Source: SL pp. 51-54; SWG p. 52.
  Test:   only if we provide them.

---

## BUG — defects OSF documented, and the behaviour we test instead

**BUG-1** [MK] [2356] An `open` of a path longer than the maximum
returned `EFAULT`. Required: `ENAMETOOLONG`.
  Source: RN73 §3.2, p. 56.
  Test:   `open` of a 2,000-byte path.

**BUG-2** [MK] [2362] `getrusage` always reported zero page faults.
Required: real counts.
  Source: RN73 §3.2, p. 56.
  Test:   touch 100 new pages; `ru_minflt` grows.

**BUG-3** [MK] [2119] Under a kernel-loaded server, addresses a process
passed were not checked in copyin and copyout, so a process could
corrupt the kernel. Required: COPY-3 holds for kernel addresses.
  Source: RN73 §3.2, p. 57.
  Test:   `read(fd, (void *)0xc0001000, 10)` fails `EFAULT`.

**BUG-4** [MK] [1843] Truncating a file upward beyond the file-size
limit posted signal 25, `SIGXFSZ`. "POSIX says it shouldn't do this on
a truncate." Required: no signal.
  Source: RN73 §3.2, p. 58.
  Test:   `setrlimit(RLIMIT_FSIZE, 1000)`; `ftruncate(fd, 2000)` and
          `truncate` raise no signal.

**BUG-5** [MK] [1914] `sleep()` called from a C threads child never
returned. Required: it returns.
  Source: RN73 §3.3, p. 60.
  Test:   `sleep(1)` in a second thread returns after about a second.

**BUG-6** [MK] [484], [106] `exit` from a process with several threads
hung; with 15 or more threads the server deadlocked. Required: `exit`
always ends the process.
  Source: RN61 "Server", pp. 21-22.
  Test:   create 1, 2, 15 and 100 threads, then `exit`; the parent's
          `wait` returns each time.

**BUG-7** [MK] [1955] VSX failed `c_iflag` tests with `ISTRIP` or
`BRKINT` set. Required: termios input flags as POSIX says.
  Source: RN73 §3.3, p. 60.
  Test:   a pty pair; with `ISTRIP` an 8-bit byte arrives stripped; with
          `BRKINT` a break flushes and sends `SIGINT`.

**BUG-8** [MK] [2017] VSX `cfset{i,o}speed` tests failed at 19200 and
38400 baud. Required: both speeds can be set and read back.
  Source: RN73 §3.2, p. 57.
  Test:   on a serial line.

**BUG-9** [MK] [1869] With `-k` removed from `bootstrap.conf`, the
server crashed reading its arguments, because `bootstrap_arguments`
returned a null pointer with a good length. Required: the server reads
its arguments the same either way.
  Source: RN73 §3.2, p. 58.
  Test:   boot with and without `-k`.

**BUG-10** [MK] [2271] With no root device on the `startup` line the
server took "major=5, minor=0" as root, where `sd0a` is major 3,
minor 0. Required: with no root device given, the documented default
(GAP-3).
  Source: RN73 §3.3, pp. 59-60.
  Test:   boot with no root word.

**BUG-11** [MK] [591] Second servers hung after "configure done";
[105] a second kernel-loaded server started with `-2` crashed printing
"Root". Required: SEC-1.
  Source: RN61 p. 21; RN73 §3.6, p. 63.

**BUG-12** [MK] [653], [593] `ps ax` while processes exit could pass a
dead port name to `thread_info`, or hang calling `thread_info` on one
of the server's own threads. Required: COLO-4; `ps` never hangs.
  Source: RN73 §3.3, pp. 60-61; RN61 p. 21.

**BUG-13** [MK] [1809] The uniprocessor lock was held across
`device_read` and `device_write`, stopping the whole server for each
disk transfer. Required: release it while waiting for a device.
  Source: RN73 §3.3, p. 60.
  Test:   a process reading a slow device does not stop `getpid` in
          another.

**BUG-14** [MK] [2189] A thread yielding in the server kept the
uniprocessor lock. Required: release it and switch with priority
depression.
  Source: RN73 §3.3, p. 60.

**BUG-15** [MK] [135] A server thread that depresses its priority
briefly gets no processor "until all the blocked C-threads in front of
it get their time first", because blocked C threads now only depress
their priority. Required: it runs as soon as it can make progress.
  Source: RN73 §3.3, p. 61; RN61 pp. 21-22.

**BUG-16** [MK] [618] Forwarding a thread's exception to the task port,
for debugging a second server, fell through every exception form and
panicked. Required: each form forwarded once.
  Source: RN61 p. 20.

**BUG-17** [MK] [602] Replies `D_DEVICE_DOWN` arriving after
`device_close` returned referred to freed server state. Required:
late device replies are discarded safely.
  Source: RN61 pp. 20-21.
  Test:   close the mouse while reads are queued.

**BUG-18** [MK] [91] `utimes` checked uids in the file-system-independent
layer; the check belongs to each file system. [90] A race with `sync`
could set a file's times to the current time instead of those asked
for. Required: `utimes` sets exactly the times given, and a user with
write access but not ownership may set them only to the current time
[POSIX].
  Source: RN61 p. 22.
  Test:   `utimes` with explicit times while `sync` runs in a loop.

**BUG-19** [MK] [89] `mmap` of a device passed its arguments to the
device pager in the wrong order, so the protection was wrong. Required:
`mmap` of a device with the protection asked for.
  Source: RN61 pp. 22-23.
  Test:   `mmap` the frame buffer or `/dev/mem` read-only; writing
          faults.

**BUG-20** [MK] [88] Loops in `exit()` and `fake_u()` ran over signals
0 to NSIG−1; "The index
of valid signals goes from 1 to NSIG". Required: signal 0 is never
treated as a signal.
  Source: RN61 p. 23.
  Test:   `kill(pid, 0)` checks existence only.

**BUG-21** [MK] [87] `getrusage` could not report resident-set
figures. Required: `ru_maxrss` is set.
  Source: RN61 p. 23.

**BUG-22** [MK] [83] Cancelling a blocked system call asynchronously
failed. Required: a blocked call can be interrupted (SYS-12).
  Source: RN61 p. 23.

**BUG-23** [MK] [86] Console opens during boot could fault; modem
control was missing. Required: modem control on serial lines.
  Source: RN61 p. 23; RN501 §4.7.
  Test:   with `CLOCAL` off, carrier loss sends `SIGHUP`.

**BUG-24** [MK] [2226] Heavy serial input with software flow control
hung the line, because a CTRL-Q was dropped when the input queue was
full. Required: start and stop characters are acted on even when the
queue is full.
  Source: RN73 §3.2, p. 57.

**BUG-25** [MK] [1978] A failed `vm_map` during `exec` killed the
process and then hung the system on exit. Required: a failed `exec`
leaves the system up.
  Source: RN73 §3.3, p. 60.
  Test:   `exec` a file whose program headers ask for an impossible
          mapping; `exec` fails or the process dies; nothing hangs.

**BUG-26** [MK] MK5.0.1 fixed: `select` for writing on a character
device was queued as a read select; a memory leak in
`getprocthreads()` after many `ps`; the `iopl` device could not be
mapped.
  Source: RN501 §4.6.
  Test:   `select` for write on a tty returns writable.

**BUG-27** [MK] [2276] A second disk in `/etc/fstab` broke mounting at
boot. Required: several file systems mount at boot.
  Source: RN73 §3.3, p. 59.

**BUG-28** [Kernel] [2114] `task_by_pid` called `mach_port_deallocate`
with the name −1. Required: `task_by_pid` never deallocates an
invalid name.
  Source: RN73 §3.2, p. 57.

**BUG-29** [Kernel] [1018] With `vm_remap` off, `vm_write` and
`vm_read_overwrite` used copy-on-write copies whose faults were always
taken, leaking virtual space. Required: copyin and copyout do not grow
the address space.
  Source: RN73 §3.2, pp. 58-59.

**BUG-30** [MK] MK5.0's VSX4 results differed from R1.2's on:
`POSIX.os/devclass/c_cflag` 1 and `tcsetattr` 19 (an option "not
supported for OSF/1 MK5.0"; `VSX_SUPPORTED_CFLAG` unset),
`ANSI.os/streamio/freopen` 13 and `POSIX.os/files/stat` 6 (the "stat/
fstat flags" pair; neither fails when run alone), and
`XPG4.os/ipc/msgrcv` 5 (a waiver). Also listed, but shared with the
integrated kernel: `POSIX.os/devclass/tcflow` 5, 7 and 8 ("an IK
problem as origin"); modem control, unimplemented; and in MK5.0.1
`XPG4.os/ipc/msgsnd` 2 ("Unresolved", not systematic, "probably a test
error"). Required: all of them pass.
  Source: RN501 §4.4.

## GAP — what the documents leave open

Each gap records our decision and where it is argued. A gap closed by a
later document is kept, with the document named.

**GAP-1** The i386 system-call convention. The course gives the
typical shape (SYS-0b: result in register 0, carry for failure, error
number in register 0) and calls it architecture-dependent; the trap
instruction, the call numbers and the argument registers on the i386
are not documented. Decision: the one our libc uses; recorded in
`docs/deferred.md` under the system-call path.

**GAP-2** Words after the root device on the `startup` line. OSF's
documents show the root device as the last word. Decision: ignored,
with a message, as 0107 does; nothing is passed on to `init`.

**GAP-3** Which root wins when both the kernel's boot arguments and the
`startup` line name one, and the root when neither does. Not
documented (BUG-10 shows only that OSF meant a sensible default).
Decision: ours, recorded with 0107.

**GAP-4** The exit status a halted child server exits with
(`docs/deferred.md`, S-21). Not
documented. Decision: Lites' existing behaviour.

**GAP-5** The error number for a second `wait` on a child another
thread is waiting for (PROC-8). Not documented. Candidate: `ECHILD`.

**GAP-6** The exact set of "thread signals" (SIG-7, SIG-9): the book
names `SIGSEGV`, `SIGILL` and `SIGBUS` as exception signals. Whether
`SIGFPE`, `SIGTRAP`, `SIGEMT` and `SIGSYS` are also per thread is not
said. Candidate: every signal an exception can produce.

**GAP-7** The table from Mach exception to signal on the i386 (EXC-5).
Not documented.

**GAP-8** *Closed by INT1 p. 2-71.* Whether the signal mask is per
thread or per process: per process (SIG-19), with the per-thread saved
mask for `sigsuspend` (SIG-10).

**GAP-9** The error number for opening a mounted file system's block
device, or mounting one whose block device is open (FILE-4).
Candidate: `EBUSY`.

**GAP-10** How a task-level system-call port is set up for threads the
server did not create (SYS-10). The mechanism is documented; the
server's use of it is ours.

**GAP-11** What the server answers a task that asks its bootstrap port
for the privileged host port (BSVC-3). "Its own policy decision."
Candidate: the port for root, nothing for others, as BSVC-4.

**GAP-12** Whether ordinary processes may make Mach system calls
(BSVC-6, SYS-6). MK5.0's documented policy: "Mach interfaces are not
exposed for unprivileged processes", as "a short term solution"
(RN501 §4.1; PAT §2.1). Later releases do not say. Decision pending.

**GAP-13** The signal table: numbers, names and default actions. Not
in the documents read so far; DEC's `signal(4)` page is the next place
to look.

**GAP-14** The device number table (DEV-1). Only `sd0a` = 3,0 is
documented.

**GAP-15** Whether a kernel thread kept suspended in Pthreads' cache
(THR-0d) still counts as "the first thread" for SIG-20. Not documented.
Candidate: it does not; a signal goes to the oldest thread that can run
user code.

**GAP-16** The System V file system (FILE-9). Not in our tree.
Decision: out of scope.

**GAP-18** A `read` or `write` on a pipe interrupted before moving any
data, when the handler asked for restart. DEC OSF/1 3.0 changed this to
fail with `EINTR` always, "in order to be POSIX compliant"; earlier
releases restarted or not by `SA_RESTART` (DRN3 §4.4.18.3, p. 4-20).
*Inferred*: R1.2 followed the earlier rule (stated by DEC for `read`
only). Candidate: that rule, with SYS-12.

**GAP-19** The error a process gets using a terminal descriptor after
its session's controlling terminal was revoked (PROC-32). Candidate:
4.4BSD-Lite2's.

**GAP-20** `table` commands `TBL_DKINFO`, `TBL_TTYINFO`, `TBL_INTR`,
`TBL_STR_PARMS` and `TBL_MAJNUMSHIFT` are marked "????" in PMD.

**GAP-21** Whether our server provides STREAMS (FILE-22). Lites has
none. Decision pending; out of scope until then.

**GAP-22** Which thread library a program is expected to use. MK5.0.1
says "The pthreads library does not work" (RN501 §4.7), and fixed a
panic in multithreaded programs (§4.6); DEC shipped DECthreads (draft 4 of
1003.4a). Candidate: what our libc provides.

**GAP-23** Whether `/proc`, which DEC documents (DTO3 §4.7), is part of
OSF/1. No OSF document mentions it. Candidate: not required.

**GAP-17** Which system calls sleep interruptibly, and so can end with
`EINTR` or be restarted (SYS-12, PROC-25). Not listed per call.
Candidate: 4.4BSD-Lite2's choices.

## ERR — errors in the documents, and disagreements

**ERR-1** The book says the suspend character "sends a SIGSTOP signal"
(§4.4.2.1, p. 4-9) and elsewhere "a SUSPEND signal" (§4.4, p. 4-7).
POSIX and BSD send `SIGTSTP`, which can be caught. We follow POSIX.

**ERR-2** When no handler takes an exception, Patience says "the kernel
terminates the task" (PAT §4); the kernel manual says "the thread is
terminated" (KI `catch_exception_raise`). For a process this does not
arise: the UNIX exception handler always answers (EXC-1).

**ERR-3** Patience writes `task_set_special_port(task,
THREAD_EXCEPTION_PORT, new_port)` (PAT §4). The task call takes the
task's exception port; the name is a slip.

**ERR-4** The book says the BSD and System V signal interfaces are
provided by "compatibility libraries" (§4.4.3); Bolinger says OSF/1
"provides the full set of 4.3BSD-derived system calls" and names
`sigvec` among the system calls (BOL §1, §3). Either way the programs
work; which side of the trap they are on is ours to choose.

**ERR-5** The book first says the utask and the np_uthread structures
hold the two handler arrays, then that the second array is in the
uthread structure (§4.4.6, p. 4-16). Not a behaviour; noted so that no
test is built on either.

**ERR-6** The server library describes `cthread_mach_msg`'s `min` as
"The maximum number of threads that can be left waiting for messages
... before other threads are released" (SL p. 52). Read as a minimum.

**ERR-7** *Withdrawn.* MK6.1 names the default kernel `/mach` and
`mach` (RN61 pp. 14-15), but a name with no directory is looked for in
the root, so they are the same file. MK7.3's is `mach_kernel`.

**ERR-8** INT2 says a symbolic link's contents replace "that portion of
the path that has already been followed" and the search restarts
(p. 5-75). Read literally, a relative link would be resolved from where
the lookup began. 4.3BSD and POSIX resolve it from the link's own
directory, and so do we.

**ERR-9** "Anonymous memory is shared with all of the process's
descendants" (INT2 p. 5-65) is stated without qualification. It cannot
mean private anonymous memory or the heap, which are copied on `fork`.
We read it as `MAP_SHARED|MAP_ANON`.

**ERR-10** INT2 defines an orphaned process group three ways: members
with "no parents within the session" (p. 6-35); none with "an ancestor
that is the session leader" (p. 6-37); and by a count of members whose
parents are outside the group and "qualified for job control"
(p. 6-39). POSIX's definition (no member has a parent in a different
group of the same session) is the third, and the slide 6-18 example
agrees with it. We follow POSIX.

**ERR-11** INT2 says permission is verified on every `read` and
`write` (p. 5-31) and the workbook says only at `open`
(p. 5-77). They agree if the check on each transfer is against the
descriptor's open mode (FD-14).

**ERR-12** INT2 says a stop signal "suspends a process group" and a
continue signal "resumes a stopped process" (p. 6-35). A signal sent to
one process acts on that process; one sent with `killpg` acts on the
group.

**ERR-14** AD3 says `init` "will no longer become the parent" of
orphans (§5.2) while claiming traditional semantics; that is AD3's
change. PROC-29 is OSF/1's. AD3 also says a new process group can be
made only by `setsid` (§6.2.2), against its own mention of `setpgrp`;
`setpgid` makes groups too (TTY-2).

**ERR-15** *Withdrawn.* RN501's table has eight rows against "five
additional failures", but three of the rows are shared with the
integrated kernel; the five match (§4.4).

**ERR-17** Where system-call arguments go: INT1 says `syscall` copies
them "to the uthread structure" (pp. 2-19, 2-21); the book says to the
kernel stack, and that the u-area has no such field (§4.4.7). Not a
behaviour; SYS-11 does not depend on it.

**ERR-16** PMA's table 3 describes `VPOP_SETPGID` with `VPOP_SETSID`'s
words, and calls `VPOP_SETPINIT` "Converts process to a child of init"
where PMD says "kernelize a process" (PMA pp. 11-12; PMD p. 19).

**ERR-13** The multithreaded signal rules (SIG-19, SIG-20) differ from
what POSIX threads became: a per-process mask, and asynchronous signals
to the first thread rather than to any thread not blocking them. The
course says only that there is "no universally accepted semantics".
OSF/1's rules are the ones specified here.

---

## Not yet read

These hold more of the interface and are to be worked through next.

- *OSF/1 Internals*, Volumes I and II, beyond the modules cited above
  (VM internals, IPC, scheduling, sockets, STREAMS, LVM: mostly not
  interface).
- The DEC OSF/1 1.0 manual pages at `typewritten.org`, sections 2 and 3:
  the per-call interface, with each call's errors. This is the closest
  public text to OSF/1's own manual pages, and the source a test suite
  most needs.
