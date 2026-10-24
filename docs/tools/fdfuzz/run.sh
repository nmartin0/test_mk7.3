#!/bin/sh
# run.sh [<old kern_descrip.c>] -- build and run the descriptor fuzzers
# against the tree's osfmk/src/lites/server/kern/kern_descrip.c, with
# AddressSanitizer (docs/deferred.md, S-23):
#
#   table.c  the descriptor table: reservations while a file is built
#            (S-23's first half, 0137)
#   hold.c   a call's hold on its file (S-23's second half)
#
# hold.c is also built -DOLD, without holds, to show it finds the
# use-after-free the holds remove: that run is expected to stop.  Given
# the kern_descrip.c from before 0137, table.c is built -DOLD on it too.
# Needs a host cc with -fsanitize=address and python3.
set -eu
D=$(cd "$(dirname "$0")" && pwd)
K=$D/../../../osfmk/src/lites/server/kern/kern_descrip.c
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
python3 "$D/extract.py" "$K" "$T/tab.c" fdalloc fdavail falloc fdinstall \
	fdunreserve ffree finishdup dup dup2 close fdcopy
python3 "$D/extract.py" "$K" "$T/hold.c" closef closef_last fhold_call frele_call
CC="cc -g -O1 -w -fsanitize=address"
$CC -DFUNCS="\"$T/tab.c\"" "$D/table.c" -o "$T/table"
$CC -DFUNCS="\"$T/hold.c\"" "$D/hold.c" -o "$T/hold"
$CC -DOLD -DFUNCS="\"$T/hold.c\"" "$D/hold.c" -o "$T/hold-old"
echo "== table.c"; "$T/table" 1000 400
echo "== hold.c"; for s in 1 7 99 12345 31337; do "$T/hold" 5000000 $s; done
echo "== hold.c -DOLD (expected to stop at a use-after-free)"
if "$T/hold-old" 5000000 1 2>"$T/old.log"; then
	echo "FAIL: the old code ran clean"; exit 1
fi
grep -m1 "heap-use-after-free" "$T/old.log"
if [ $# -ge 1 ]; then
	python3 "$D/extract.py" "$1" "$T/tabold.c" fdalloc fdavail falloc \
		ffree finishdup dup dup2 close fdcopy
	$CC -DOLD -DFUNCS="\"$T/tabold.c\"" "$D/table.c" -o "$T/table-old"
	echo "== table.c -DOLD (expected to fail)"
	if "$T/table-old" 1000 400; then
		echo "FAIL: the old table code ran clean"; exit 1
	fi
fi
