#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Run the boards' smoke suite (tools/dejagnu/smoke) on one or more boards
# and say whether each board works.
#
# Usage: tools/dejagnu/run-smoke.sh [-o OUTDIR] BOARD... [-- RUNTEST-ARGS...]
#
#   BOARD     e2k-qemu, e2k-4c, e2k-8c2, e2k-2c3 or e2k-ssh;
#             e2k-qemu:elbrus-vN runs qemu with -cpu elbrus-vN (E2K_CPU);
#             a DejaGnu variant adds compiler flags: e2k-8c2/-mptr32
#   -o OUTDIR results go to OUTDIR/BOARD/smoke.{sum,log}
#             (default toolchains/dejagnu/smoke in this checkout)
#
# The board settings come from the environment (tools/dejagnu/README.md);
# e2k-2c3 needs E2K_2C3_HOST and E2K_2C3_PORT. Exit status 0 when every
# board passed every check.

set -e
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
out="$repo/toolchains/dejagnu/smoke"

if [ "$1" = "-o" ]; then
    out=$2
    shift 2
fi
boards=
while [ $# -gt 0 ] && [ "$1" != "--" ]; do
    boards="$boards $1"
    shift
done
[ "$1" = "--" ] && shift
if [ -z "$boards" ]; then
    sed -n '4,/^$/p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi
mkdir -p "$out"
out=$(cd "$out" && pwd)

export DEJAGNU="$here/site.exp"
status=0
for spec in $boards; do
    board=${spec%%:*}
    cpu=
    [ "$board" != "$spec" ] && cpu=${spec#*:}
    dir="$out/$(echo "$spec" | tr ':/' '__')"
    mkdir -p "$dir"
    echo "== $spec"
    (
        cd "$dir"
        if [ -n "$cpu" ]; then
            E2K_CPU=$cpu
            export E2K_CPU
        fi
        runtest --tool smoke --srcdir "$here/smoke" --target=e2k-linux-gnu \
            --target_board="$board" "$@" > runtest.out 2>&1
    ) || true
    sum="$dir/smoke.sum"
    if [ ! -f "$sum" ]; then
        echo "   no smoke.sum; see $dir/runtest.out"
        status=1
        continue
    fi
    passes=$(sed -n 's/^# of expected passes[[:space:]]*//p' "$sum")
    bad=$(grep -c '^\(FAIL\|UNRESOLVED\|ERROR\|XPASS\|UNTESTED\):' "$sum" || true)
    grep '^\(FAIL\|UNRESOLVED\|ERROR\|XPASS\|UNTESTED\|WARNING\):' "$sum" \
        | grep -v 'WARNING: program timed out' | sed 's/^/   /' || true
    if [ "${bad:-0}" -eq 0 ] && [ -n "$passes" ]; then
        echo "   ok: $passes checks passed ($sum)"
    else
        echo "   BOARD PROBLEM: $bad bad results ($sum, smoke.log)"
        status=1
    fi
done
exit $status
