#!/bin/sh
# LITES invokes mig with Mach4/GNU conventions; OSF's mig differs:
#     -cc <cmd>  ->  -cpp <cmd>
# OSF's mig silently treats an unknown -cc as a cpp flag and then tries
# to open the command name as a file, so this has to be translated.
#
# Point OSFMK_TOOLS at the osfmk directory (the one containing tools/).
# build-lites.sh sets it; set it yourself if invoking this directly.
REAL=${OSFMK_MIG:-${OSFMK_TOOLS:?set OSFMK_TOOLS to .../osfmk7.3/osfmk}/tools/i386/i386_linux/hostbin/mig}
[ -x "$REAL" ] || { echo "mig-shim: not executable: $REAL" >&2; exit 1; }
args=""
while [ $# -gt 0 ]; do
    case "$1" in
        -cc) args="$args -cpp \"$2\""; shift 2;;
        *)   args="$args \"$1\""; shift;;
    esac
done
eval exec "$REAL" $args
