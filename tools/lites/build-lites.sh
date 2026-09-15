#!/bin/sh
# Build LITES 1.1u3 against an OSFMK 7.3 export tree.
#
#   MK_BUILD=~/.cache/mk7.3 ./build-lites.sh /path/to/lites /path/to/builddir
#
# Applies the shims in this directory, constructs a MACH_RELEASE_DIR
# from the OSFMK export tree, configures, and builds.
set -e

LITES=${1:?usage: build-lites.sh <lites-src> <build-dir>}
BUILD=${2:?usage: build-lites.sh <lites-src> <build-dir>}
MK_BUILD=${MK_BUILD:-$HOME/.cache/mk7.3}
HERE=$(cd "$(dirname "$0")" && pwd)
OSFMK=$HERE/../../osfmk7.3/osfmk

MR=$BUILD/machrel
mkdir -p "$MR/bin" "$MR/libexec" "$BUILD/obj"
ln -sfn "$MK_BUILD/export/at386/include" "$MR/include"
ln -sfn "$MK_BUILD/export/at386/lib"     "$MR/lib"
HB=$OSFMK/tools/i386/i386_linux/hostbin
ln -sf "$HERE/mig-shim.sh" "$MR/bin/mig"
ln -sf "$HB/migcom"        "$MR/bin/migcom"
ln -sf "$HB/migcom"        "$MR/libexec/migcom"
OSFMK_TOOLS=$OSFMK export OSFMK_TOOLS

# LITES patches, applied once, idempotently
for p in gensym-newline radix-bsd-malloc; do
    [ -f "$HERE/$p.patch" ] || continue
    patch -p1 -N -r /dev/null -d "$LITES" < "$HERE/$p.patch" >/dev/null 2>&1 || true
done

cd "$BUILD/obj"
sh "$LITES/configure" \
    --with-release="$MR" \
    --with-config="STD+WS+osfmach3" \
    --host=i386-unknown-mach3 --target=i386-unknown-mach3

GI=$(gcc -m32 -print-file-name=include)
exec make \
    CXXX="-m32 -fno-builtin -isystem $GI -include $HERE/lites-compat.h" \
    CHXXX="-m32" \
    ASFLAGS="-m32"
