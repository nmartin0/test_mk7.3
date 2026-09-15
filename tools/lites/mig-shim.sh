#!/bin/sh
# Shim: LITES invokes mig with Mach4/GNU conventions; OSF's mig differs.
#   -cc <cmd>   ->  -cpp <cmd>     (preprocessor selection)
# Everything else is passed straight through.
REAL=${OSFMK_TOOLS:-$(dirname "$0")/../../osfmk7.3}/osfmk/tools/i386/i386_linux/hostbin/mig
args=""
while [ $# -gt 0 ]; do
    case "$1" in
        -cc) args="$args -cpp \"$2\""; shift 2;;
        *)   args="$args \"$1\""; shift;;
    esac
done
eval exec "$REAL" $args
