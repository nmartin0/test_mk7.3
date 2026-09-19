#!/bin/sh
# Build LITES 1.1u3 against an OSFMK 7.3 export tree.
#
#   MK_BUILD=~/.cache/mk7.3 ./build-lites.sh <lites-src> <build-dir>
#
# Applies tools/lites/lites-osfmk73.patch to the LITES source (idempotent),
# constructs a MACH_RELEASE_DIR from the OSFMK export tree, configures and
# builds. Safe to re-run.
set -e

LITES=${1:?usage: build-lites.sh <lites-src> <build-dir>}
BUILD=${2:?usage: build-lites.sh <lites-src> <build-dir>}
MK_BUILD=${MK_BUILD:-$HOME/.cache/mk7.3}
HERE=$(cd "$(dirname "$0")" && pwd)

OSFMK_TOOLS=$(cd "$HERE/../../osfmk7.3/osfmk" && pwd)
export OSFMK_TOOLS
HB=$OSFMK_TOOLS/tools/i386/i386_linux/hostbin
EXPORT=$MK_BUILD/export/at386

for f in "$EXPORT/include" "$EXPORT/lib" "$HB/mig" "$HB/migcom"; do
    [ -e "$f" ] || { echo "missing: $f" >&2; exit 1; }
done

# --- LITES patches (idempotent) -------------------------------------
#
# This used to test one marker from one hunk -- `function bail` in
# vnode_if.sh -- and skip the whole patch if it was present. That is
# wrong whenever the patch has GROWN since the tree was patched: the
# marker is there, the new hunks are not, and the build silently
# produces a LITES without them. It happened with the pid-2 fix in
# kern_exit.c, where the symptom was a respawn loop that the commit
# claimed to have fixed.
#
# Ask the real question instead: does the patch reverse cleanly? If it
# does, every hunk is already in the tree. If it does not, apply with
# --forward, which puts in what is missing and skips what is present.
PATCHFILE="$HERE/lites-osfmk73.patch"

if patch -p1 -R --dry-run -s -f -d "$LITES" < "$PATCHFILE" >/dev/null 2>&1; then
    echo "LITES already patched (all hunks present), skipping"
else
    echo "patching LITES"
    # --forward exits non-zero when it skips an already-applied hunk,
    # which is not an error here, so the check is whether the tree is
    # fully patched afterwards rather than what patch returned.
    # -r - discards reject files and --no-backup-if-mismatch suppresses
    # .orig copies: an already-applied hunk is skipped here by design,
    # so its "reject" is noise, and 22 .rej files in a source tree look
    # like a failed patch to whoever finds them next.
    patch -p1 --forward -r - --no-backup-if-mismatch \
        -d "$LITES" < "$PATCHFILE" || true
    if patch -p1 -R --dry-run -s -f -d "$LITES" < "$PATCHFILE" >/dev/null 2>&1; then
        echo "LITES patched"
    else
        echo "build-lites: $LITES is not fully patched and could not be" >&2
        echo "  brought up to date. Check for .rej files, or start from" >&2
        echo "  a pristine clone:" >&2
        echo "    cd $LITES && git checkout -- ." >&2
        exit 1
    fi
fi

# --- MACH_RELEASE_DIR ------------------------------------------------
# lib must be a real directory: LITES wants library names we do not use,
# and crt0.o which OSFMK keeps inside libsa_mach.a rather than standalone.
MR=$BUILD/machrel
rm -rf "$MR"
mkdir -p "$MR/bin" "$MR/libexec" "$MR/lib" "$BUILD/obj"
ln -sfn "$EXPORT/include" "$MR/include"
for a in "$EXPORT"/lib/*.a; do ln -sf "$a" "$MR/lib/"; done
( cd "$MR/lib"
  # LITES says -lthreads; OSFMK builds libcthreads.a. Verified the right
  # mapping: libcthreads exports 36 cthread_* symbols including
  # cthread_wire and cthread_fork, which is what LITES calls.
  ln -sf libcthreads.a libthreads.a
  ar x "$EXPORT/lib/libsa_mach.a" crt0.o )
cp "$HERE/mig-shim.sh" "$MR/bin/mig"
chmod +x "$MR/bin/mig"
ln -sf "$HB/migcom" "$MR/bin/migcom"
ln -sf "$HB/migcom" "$MR/libexec/migcom"

# --- configure -------------------------------------------------------
# osfmach3 is essential: without it OSFMACH3 and OSF_LEDGERS stay unset
# and every device call has the wrong arity.
#
# ext2fs is needed for an ext2 root filesystem. It is an option in
# conf/MASTER ("options ext2fs EXT2FS 1 ext2fs.h") and is NOT in the
# STD+WS set, so without naming it here the generated server/ext2fs.h
# contains "#define EXT2FS 0", no ext2 objects are built, and the
# fallback to ext2_mountroot() in init_main.c is compiled out entirely.
# Confirm after configuring with:
#     cat <builddir>/obj/server/ext2fs.h        # want: #define EXT2FS 1
cd "$BUILD/obj"
sh "$LITES/configure" \
    --with-release="$MR" \
    --with-config="STD+WS+osfmach3+ext2fs" \
    --host=i386-unknown-mach3 --target=i386-unknown-mach3

# --- build -----------------------------------------------------------
# -std=gnu89          GCC 14 makes K&R definitions and implicit int errors
# -Ulinux             ...but gnu89 also predefines linux=1, and LITES
#                     guards Linux-only code with "#if linux". In 1995
#                     an undefined identifier in #if evaluated to 0 and
#                     the code was excluded; with the macro defined it
#                     compiles into a BSD server and fails on Linux
#                     kernel idioms such as inode->i_sb. -std=c89 would
#                     also avoid it but loses the GNU extensions this
#                     code needs elsewhere.
# -fno-builtin        BSD's kernel log(level,fmt,...) vs GCC's log(double)
# -fgnu89-inline      cthreads.h uses extern __inline__
# -fcommon            tentative definitions in headers; GCC 10+ defaults off
# -fno-stack-protector no __stack_chk_fail_local here
# -D__NO_UNDERSCORES__ ELF symbol names; without it the asm defines _htonl
# AWK=nawk            the generators need nawk extensions, not mawk
# LIBS repeated       libsa_mach and libmach reference each other
# --defsym __start=__start_mach
#                     LITES links -e __start; this crt0 defines
#                     __start_mach. Without this ld warns and silently
#                     defaults the entry to the first byte of .text,
#                     and the server dies before crt0 runs. Confirm
#                     with: readelf -h ... | grep -i entry
#                     It must be __start_mach's address, not .text's.
# -z muldefs          LITES defines its own printf, vsprintf, sprintf
#                     and sleep; libsa_mach provides standalone ones and
#                     its printf.o is pulled in for another symbol
GI=$(gcc -m32 -print-file-name=include)
LG=$(gcc -m32 -print-libgcc-file-name)

MAKEARGS="AWK=nawk \
  CXXX=-m32 -std=gnu89 -fno-builtin -fgnu89-inline -fcommon -fno-stack-protector"

# The first pass can fail on bsd_server.c: make resolves it through VPATH
# only once the MIG outputs exist. A second pass always succeeds.
CC_FLAGS="-m32 -std=gnu89 -Ulinux -fno-builtin -fgnu89-inline -fcommon -fno-stack-protector -isystem $GI -include $HERE/lites-compat.h"
LIB_LIST="-llites -lthreads -lmach_sa -lsa_mach -lmach_sa $LG"

# The server and the emulator need different entry-point handling, so
# the subdirectories are built individually rather than by one top-level
# make.
#
#   server   links ${CRT0}, which is libsa_mach's crt0.o. That crt0 is
#            the right one for a bootstrap-loaded server: it fetches
#            argv over IPC with bootstrap_arguments(), which is how this
#            system delivers arguments, since servers start on a
#            deliberately zero-filled stack. Its entry symbol is
#            __start_mach, which is the right entry for a personality
#            server on this kernel -- MkLinux's own i386 personality
#            says so at linux/arch/osfmach3_i386/Makefile:69,
#            "LDFLAGS = -e __start_mach -static -nostdlib".
#
#            That direct form cannot be used here. server/Makerules:66
#            links with $(LDFLAGS) $(TARGET_LDFLAGS), in that order, and
#            conf/i386/MASTER puts -e __start into TARGET_LDFLAGS; ld
#            honours the last -e it is given, so ours would be
#            overridden. Tested: the entry comes out as 0x8049000, the
#            first byte of .text, which is not a function. Overriding
#            TARGET_LDFLAGS instead would drop the -L paths it carries.
#
#            So --defsym makes LITES's own -e __start resolve to the
#            same address the reference uses. Verified: entry 0x8049320,
#            which nm gives as __start_mach.
#
#            (libmach/i386/crt0.c does define __start, but it reads argv
#            from the stack only and nothing in the tree builds it. Using
#            it would silently lose every server argument.)
#
#   emulator has its own crt0, emulator/i386/ecrt0.c, which defines
#            __start itself. It must NOT get the --defsym: that creates a
#            reference to __start_mach, which drags libsa_mach's crt0.o
#            into the link, and that crt0 calls main(), which the
#            emulator does not have -- it has emulator_main().
LD_COMMON="-m elf_i386 -z muldefs"

build_dir() {
    _dir=$1; shift
    make -C "$_dir" \
      AWK=nawk \
      CXXX="$CC_FLAGS" \
      CHXXX="-m32 -std=gnu89" \
      ASFLAGS="-m32 -D__NO_UNDERSCORES__" \
      LDFLAGS="$*" \
      LIBS="$LIB_LIST"
}

for pass in 1 2; do
    ( build_dir include  "$LD_COMMON" &&
      build_dir liblites "$LD_COMMON" &&
      build_dir server   "$LD_COMMON --defsym __start=__start_mach" &&
      build_dir emulator "$LD_COMMON" ) && break
    [ $pass = 1 ] && echo "=== first pass failed (expected); retrying ===" || exit 1
done
