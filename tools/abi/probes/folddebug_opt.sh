#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# What lcc's debug information gives MCST's gdb for optimized code
# (findings A30).  Builds folddebug_opt.c with lcc 1.29.16 at -O0 and
# -O2 (-g), copies both to an Elbrus machine and runs gdb in batch mode:
# the parameters and locals of sum_sq at its breakpoint, where a
# breakpoint on a line inside its loop lands, and what "next" over the
# call line in main (or, where it has no row, from main's breakpoint)
# does.
#
#   tools/abi/probes/folddebug_opt.sh USER@HOST TARGET [SSH_PORT]
#     TARGET: v3, v3.4c, v5.8c2, v6.2c3 (the lcc directory's target)
#   OUT=dir (default toolchains/gapfill/fold-debug) gets
#   HOST-TARGET-O0.txt and -O2.txt; RDIR (default relcc2/fold-debug) is
#   the remote working directory, removed afterwards unless KEEP=1.
#
# The object files, read on the host with readelf --debug-dump (lcc
# 1.29.16 and 1.31.05, e2k-v3, 2026-10-16): at -O0 and -Og every
# parameter and local has a DW_AT_location and lexical blocks have
# DW_AT_low_pc/high_pc; from -O1 to -O4 only the globals (sink, glob)
# have one, lexical blocks have no range, the inlined sq has no
# DW_TAG_inlined_subroutine, there is no .debug_loc or .debug_ranges,
# and the line table has one row per function, at its opening brace
# (37 rows at -O0, 5 at -O1, where sq is not inlined, 4 at -O2 and up).
# -Og gives the same assembly as -O0.
#
# Results (4C, ALT's gdb 15.2-alt1.2, target v3; 8C2, OS Elbrus's gdb
# 15.2-29.023, target v5.8c2; 2026-10-16, the same on both):
#   -O0  arguments and locals print right; "break" on the loop line
#        stops in it; "next" steps line by line.
#   -O2  every argument and local of sum_sq and dot is <optimized out>
#        (i too, at the function's start: a lexical block without a
#        range doesn't scope it); the loop line "is at address <dot>
#        but contains no code", and a breakpoint on it goes to dot's
#        opening line, the next line with a row (gdb's
#        find_line_common), after dot's prologue, where "break dot"
#        lands too; main's call line has no row, and main being the
#        file's last function no later line has one ("No line N in
#        file"); "next" from main's breakpoint runs main to its end
#        (it stops in libc, in main's caller).
set -eu
dest=$1
target=$2
port=${3:-}
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
lcc=/opt/mcst/lcc-1.29.16.e2k-$target.linux-6.1/bin/lcc
out=${OUT:-$root/toolchains/gapfill/fold-debug}
rdir=${RDIR:-relcc2/fold-debug}
host=${NAME:-${dest#*@}}
mkdir -p "$out"
for o in O0 O2; do
  "$lcc" -$o -g "$here/folddebug_opt.c" -o "$out/folddebug_opt-$target-$o"
done
lline=$(grep -n 'LOOP_LINE \*/' "$here/folddebug_opt.c" | cut -d: -f1)
cline=$(grep -n 'CALL_LINE \*/' "$here/folddebug_opt.c" | cut -d: -f1)
ssh=("$root/tools/setup/e2k-ssh")
scp=("$root/tools/setup/e2k-scp")
if [ -n "$port" ]; then ssh+=(-p "$port"); scp+=(-P "$port"); fi
"${ssh[@]}" "$dest" "mkdir -p $rdir"
"${scp[@]}" "$out/folddebug_opt-$target-O0" "$out/folddebug_opt-$target-O2" \
  "$dest:$rdir/"
for o in O0 O2; do
  "${ssh[@]}" "$dest" "cd $rdir && gdb --version | head -1 &&
    ./folddebug_opt-$target-$o &&
    gdb -batch -nx -ex 'break sum_sq' -ex 'break dot' -ex run \
    -ex 'info args' -ex 'info locals' -ex 'p s' -ex 'p i' \
    -ex 'info line folddebug_opt.c:$lline' \
    -ex 'break folddebug_opt.c:$lline' -ex 'info breakpoints' \
    -ex continue -ex 'info args' -ex 'p d' -ex bt \
    -ex 'delete' -ex 'break folddebug_opt.c:$cline' -ex 'break main' \
    -ex run -ex 'info line *\$pc' -ex next -ex 'info line *\$pc' \
    -ex next -ex 'info line *\$pc' -ex 'info locals' \
    ./folddebug_opt-$target-$o 2>&1" \
    > "$out/$host-$target-$o.txt" 2>&1 || true
  echo "$out/$host-$target-$o.txt"
done
[ "${KEEP:-0}" = 1 ] || "${ssh[@]}" "$dest" "rm -rf $rdir"
