#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Where MCST's gdb stops on "step" into a function and on "break" on it
# (findings A32; spec/abi/stack-and-unwinding.md, "How gdb places
# breakpoints").  Builds folda32_step.c with the proof-of-concept GCC
# at -O0, -Og, -O1 and -O2 and with lcc 1.29.16 (e2k-v3) at -O0, all
# -g, copies them to an Elbrus machine and runs two gdb sessions on
# each: "break work" then run, and a breakpoint on main's call line
# (CALL_LINE), run, "step".  Each prints $pc and "info line *$pc".
#
#   tools/abi/probes/folda32_step.sh USER@HOST [SSH_PORT]
#   GCC=... (default: the PoC v3 cross in toolchains/poc-cross);
#   OUT=dir (default toolchains/gapfill/fold-a32) gets HOST-OPT.txt,
#   the binaries, and their objdump -d and readelf
#   --debug-dump=decodedline; RDIR (default relcc2/folda32) is the
#   remote working directory, removed afterwards unless KEEP=1.
#
# Results (4C, ALT's gdb 15.2-alt1.2; PoC 0.5+32 and lcc 1.29.16,
# 2026-10-24).  work's first wide instruction holds setwd at every
# level; at -O1 and -O2 also the store to sink (line 26), k * 2 (line
# 27) and the loop's first operations, at -Og only "return", at -O0
# nothing else (getsp and the frame-base addd follow in instructions of
# their own); lcc -O0's setwd is alone too (work has no getsp).
#   gcc -O0  break work: 0x12048 line 26; step: 0x12048 line 26
#   gcc -Og  break work: 0x12000 (entry) line 25; step: 0x12018 line 26
#   gcc -O1  break work: 0x12000 (entry) line 26; step: 0x12030 line 28
#   gcc -O2  break work: 0x12400 (entry) line 26; step: 0x12430 line 28
#   lcc -O0  break work: 0x11c10 line 26; step: 0x11c10 line 26
# break goes to the entry where gdb trusts the unit (GCC's -Og and up:
# location lists), else through e2k_skip_prologue, as step always does.
# At -O1 and -O2 step stops after lines 26 and 27 ran, in the second
# wide instruction.  At -O0 e2k_skip_prologue stops at the addd after
# getsp (0x12020), inside line 25's row, and both move on to the row's
# end, 0x12048.
set -eu
dest=$1
port=${2:-}
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
gcc=${GCC:-$root/toolchains/poc-cross/cross-0.5+32-v3/gcc-e2k-0.5+32-v3/bin/e2k-linux-gnu-gcc}
lcc=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
bu=$root/toolchains/binutils/bin
out=${OUT:-$root/toolchains/gapfill/fold-a32}
rdir=${RDIR:-relcc2/folda32}
host=${NAME:-${dest#*@}}
mkdir -p "$out"
cline=$(grep -n 'CALL_LINE \*/' "$here/folda32_step.c" | cut -d: -f1)
builds=()
for o in O0 Og O1 O2; do
  "$gcc" -$o -g "$here/folda32_step.c" -o "$out/step-gcc-$o"
  builds+=(gcc-$o)
done
"$lcc" -O0 -g "$here/folda32_step.c" -o "$out/step-lcc-O0"
builds+=(lcc-O0)
for b in "${builds[@]}"; do
  "$bu/e2k-linux-objdump" -d "$out/step-$b" |
    sed -n '/<work>:/,/<main>:/p' > "$out/step-$b.dis"
  "$bu/e2k-linux-readelf" --debug-dump=decodedline "$out/step-$b" \
    > "$out/step-$b.lines"
done
ssh=("$root/tools/setup/e2k-ssh")
scp=("$root/tools/setup/e2k-scp")
if [ -n "$port" ]; then ssh+=(-p "$port"); scp+=(-P "$port"); fi
"${ssh[@]}" "$dest" "mkdir -p $rdir"
"${scp[@]}" "${builds[@]/#/$out/step-}" "$dest:$rdir/"
for b in "${builds[@]}"; do
  "${ssh[@]}" "$dest" "cd $rdir && gdb --version | head -1 &&
    ./step-$b; echo exit \$? &&
    echo '== break work' &&
    gdb -batch -nx -ex 'break work' -ex run -ex 'p/x \$pc' \
      -ex 'info line *\$pc' ./step-$b 2>&1 &&
    echo '== step from line $cline' &&
    gdb -batch -nx -ex 'break folda32_step.c:$cline' -ex run -ex step \
      -ex 'p/x \$pc' \
      -ex 'info line *\$pc' ./step-$b 2>&1" \
    > "$out/$host-$b.txt" 2>&1 || true
  echo "$out/$host-$b.txt"
done
[ "${KEEP:-0}" = 1 ] || "${ssh[@]}" "$dest" "rm -rf $rdir"
