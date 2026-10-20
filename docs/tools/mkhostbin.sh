#!/bin/sh
#
# mkhostbin.sh -- build the host tools, config, migcom, compose and
# ptbldump, from this tree.
#
#   cd osfmk/src && sh ../../docs/tools/mkhostbin.sh
#
# Run once in a sandbox made by docs/tools/mksandbox.sh, in the same
# environment as build_world (~/.sandboxrc, set_ode_path.sh, ODE's
# build on PATH), before build_world.
#
# What.  The build runs four host tools from tools_base/hostbin, which
# mksandbox.sh points out of the tree:
#
#   mig, makeboot   shell scripts, copied from DR3's
#                   tools/ppc/ppc_linux/hostbin -- nothing in them is
#                   PowerPC's.  DR3 ships them mode 644.
#   migcom          mach_services/lib/migcom
#   config          mach_services/cmds/config
#
# and two more that the NORMA+net and +flipc kernels' uk_xkern/ runs
# (makedefs/osf.osc.mk, COMPOSE and PTBLDUMP), from OSF's cmk1.1, the
# commands package DR3 shipped beside the kernel (imports.md, kind F):
#
#   compose         mach_services/cmds/compose
#   ptbldump        mach_services/cmds/ptbldump, with
#                   xkern/framework's idmap.c, prottbl.c, xk_debug.c
#                   and s_xkern/framework/prottbl_parse.c
#
# DR3 shipped migcom and config only as PowerPC Linux binaries, without
# their sources; the sources here are OSF's, from the MkLinux CVS
# repository (docs/provenance/imports.md).
#
# How.  DR3's own procedure, tools/ppc/ppc_linux/hostbin/README:
#
#   1. "install as much of the mach_kernel include files as possible
#      ... build MAKEFILE_PASS=FIRST ... [it] will terminate when it
#      can't find migcom".  make -k carries it past the MIG-generated
#      headers it cannot make, so every plain header is exported.  The
#      README copies mach/*.h by hand for the same end.  migcom needs
#      mach/std_types.h, which the setup pass's own list, SETUP_INCLUDES
#      in mach_kernel/mach/Makefile, does not export.
#   2. "Now build migcom and config".  The setup pass does, with the
#      host compiler (Makeconf, "CCTYPE=host"), and installs them into
#      TOOLS, which mksandbox.sh makes the same directory as
#      tools_base/hostbin.  migcom is under SETUP_SUBDIRS; config only
#      when PURE_MACH is unset, and Buildconf sets it, so config is
#      built in its own directory.
#   3. "It is recommended that you then delete everything under
#      export/powermac and obj/powermac and start your building of the
#      kernel by re-exporting the include files."  For us, at386.
#
# Host needs: gcc with -m32, flex and its 32-bit library, and byacc --
# config's grammar is older than bison accepts.  On Debian and Ubuntu,
# with i386 as a foreign architecture: flex libfl-dev:i386 byacc.
#
# compose and ptbldump are built here with the host compiler, as
# their Makefiles' SETUP_PROGRAMS would build them, with three
# stand-ins for the OSF/1 or MkLinux host OSF built them on:
#   - their CFLAGS' -fwritable-strings, which GCC dropped in 4.0: both
#     write into string literals (mktemp() on a literal template).
#     Each object's read-only data is moved into writable data with
#     objcopy, the same effect.
#   - compose's <standards.h> and <sys/dir.h>, OSF/1 headers: an empty
#     one, and one that includes <dirent.h>.
#   - xkern's <types.h>, which on OSF's hosts was sa_mach's, for
#     bit32_t and its kin: those six typedefs over the host's
#     <sys/types.h>; sa_mach's own clashes with the host's.
# objcopy is binutils'.
#
set -eu

OSFMK=$(cd "$(dirname "$0")/../../osfmk" && pwd)
HOSTBIN="${OSFMK}/tools/i386/i386_linux/hostbin"

[ -d "${HOSTBIN}" ] || {
	echo "no ${HOSTBIN} -- run docs/tools/mksandbox.sh first" >&2; exit 1; }
command -v build >/dev/null || {
	echo "ODE's build is not on PATH -- see mksandbox.sh's output" >&2
	exit 1; }

cd "${OSFMK}/src"

for f in mig makeboot; do
	cp "${OSFMK}/tools/ppc/ppc_linux/hostbin/$f" "${HOSTBIN}/$f"
	chmod 755 "${HOSTBIN}/$f"
done

# 1.  Fails, by design, where migcom is first wanted.
build -k MAKEFILE_PASS=FIRST > "${OSFMK}/obj/hostbin-first.log" 2>&1 || :
[ -f "${OSFMK}/export/at386/include/mach/std_types.h" ] || {
	echo "the headers were not exported; see ${OSFMK}/obj/hostbin-first.log" >&2
	exit 1; }

# 2.
build setup_all
(cd mach_services/cmds/config && build setup_all)

# 2b.  compose and ptbldump, while the exported headers are still there.
XK=$(mktemp -d)
mkdir "${XK}/shim" "${XK}/shim/sys" "${XK}/compose" "${XK}/ptbldump"
: > "${XK}/shim/standards.h"
echo '#include <dirent.h>' > "${XK}/shim/sys/dir.h"
cat > "${XK}/shim/types.h" <<'TYPES'
#include <sys/types.h>
typedef signed char	bit8_t;
typedef unsigned char	u_bit8_t;
typedef short		bit16_t;
typedef unsigned short	u_bit16_t;
typedef int		bit32_t;
typedef unsigned int	u_bit32_t;
TYPES
writable() {
	for sec in $(objdump -h "$1" | awk '/rodata/ { print $2 }'); do
		objcopy --rename-section \
		    "${sec}=.data.rw${sec},alloc,load,data,contents" "$1"
	done
}
HOSTCC="gcc -m32 -std=gnu89 -fcommon -w"
CMDS=mach_services/cmds
for f in compose error parse util; do
	${HOSTCC} -I"${XK}/shim" -I${CMDS}/compose \
	    -c -o "${XK}/compose/$f.o" ${CMDS}/compose/$f.c
	writable "${XK}/compose/$f.o"
done
${HOSTCC} -o "${HOSTBIN}/compose" "${XK}"/compose/*.o
for f in ${CMDS}/ptbldump/ptbldump.c xkern/framework/idmap/idmap.c \
    xkern/framework/prottbl.c xkern/framework/xk_debug.c \
    mach_services/servers/s_xkern/framework/prottbl_parse.c; do
	o="${XK}/ptbldump/$(basename "$f" .c).o"
	${HOSTCC} -DXK_DEBUG -DALLOC_DONT_USE_INLINE -DXKSTANDALONE \
	    -I"${XK}/shim" -I${CMDS}/ptbldump -I. -Imach_services/servers \
	    -Ixkern/include -idirafter "${OSFMK}/export/at386/include" \
	    -c -o "$o" "$f"
	writable "$o"
done
${HOSTCC} -o "${HOSTBIN}/ptbldump" "${XK}"/ptbldump/*.o
rm -rf "${XK}"

for f in mig makeboot migcom config compose ptbldump; do
	[ -x "${HOSTBIN}/$f" ] || { echo "no ${HOSTBIN}/$f" >&2; exit 1; }
done

# 3.
rm -rf "${OSFMK}/obj/at386" "${OSFMK}/obj/hostbin-first.log"
mkdir "${OSFMK}/obj/at386"
find "${OSFMK}/export/at386/" -mindepth 1 -delete

echo "mkhostbin: ${HOSTBIN}"
ls -l "${HOSTBIN}"
