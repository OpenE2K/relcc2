#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds debuginfo.c + debuginfo.s with lcc 1.29.16 (dynamic and
# static), copies them to an Elbrus machine and runs debuginfo.gdb in
# batch-mode gdb there, one -ex per line so that an error doesn't stop
# the script.  User mode only.
#
#   tools/abi/probes/debuginfo.sh USER@HOST TARGET [SSH_PORT]
#     TARGET: v3, v3.4c, v5.8c2, v6.2c3 (the lcc directory's target);
#     v5 and later add the 128-bit register case (--defsym V5=1).
#   OUT=dir (default toolchains/debuginfo) gets HOST-TARGET-dyn.txt,
#   -static.txt, -misc-dyn.txt, -misc-static.txt and -gline.txt (NAME
#   replaces HOST in them); RDIR (default relcc2/debuginfo) is the
#   remote working directory, removed afterwards unless KEEP=1.
#
# Results (MCST's gdb 15.2: ALT 15.2-alt1.2 on the 4C, OS Elbrus
# 15.2-29.023 on the 8C2 and the 2C3, 2026-10-16; the three agree except
# where a v5 case is marked).  Numbers are debuginfo.gdb's stops.
#   1-5  %b[k] (DWARF 64 + k) in the stopped function is read through the
#        live %br: rbs 4 rsz 3 rcur 1 gives %b[0] = %r10, and each abn
#        moves it (%b[0] = %r10, %r8, %r14 in three iterations, %r12 after
#        the loop); %b[8], beyond the 8-register ring, is <unavailable>.
#   5a   under rbs 0x20 %b[0..7] are %r64..%r71 and DWARF 64..71 reach
#        them (gdb has no $r64 name; $r64 is void).
#   6-7  a callee without setbn runs on its caller's %br (l_b0 = its own
#        %r12); after its setbn %b[0] is its %r4.  In the caller's frame
#        %b[] goes through the %br saved at the call (cr1_hi.br): o_b0 is
#        still the caller's %r12.  The caller's %r0..%r23 (below 2 x wbs)
#        hold its values, %r24..%r31 show the callee's %r0..%r7, and a
#        DWARF register beyond the caller's window (%r40, %b[8]) is an
#        error, "Cannot access memory at address 0x0" (gdb's own $r40
#        says <unavailable>); in the stopped function such registers are
#        <unavailable>.  The caller's pc is the return address and
#        locations are looked up at pc - 1: a location list entry covering
#        only the call's wide instruction is found, one starting at the
#        return address is <optimized out>.
#   8-10 16-byte values: vectors, __int128 and pieces are read from DWARF
#        N and N + 1 of the right frame; a long double or __float128 at
#        a bare DW_OP_regN (any 16-byte float) is read from N and N + 1
#        of the CALLER's frame (2.5 and 2 from di_wcall instead of 1.5
#        and 1), in the stopped function and in outer frames alike: gdb
#        15's get_frame_register_bytes takes the next frame and
#        e2k_register_to_value still passes the frame itself.  As pieces
#        (DW_OP_reg4 piece 8, DW_OP_reg5 piece 8, or DW_OP_reg4 piece 16)
#        they read right (1.5, 1).  No DWARF location reaches an extended
#        part: the movif long double in %r6 read as a 16-byte piece is 3
#        (%r7 = 0x4000 taken as its exponent); nor a v5 128-bit
#        register's high half: qv shows %r15 as its upper lane, a
#        low-half piece plus an empty piece shows <optimized out> there
#        (8C2, 2C3).  gdb's own $xr6 and $qpr14 show both, also in an
#        outer frame.
#   11-13 at a function's first instruction lcc -O0's frame base
#        (DW_OP_reg8) is outside the window (stack arguments
#        <unavailable>); after setwd and getsp, before the copy into %r8,
#        it is a register the window just gained (0, empty: "Cannot access
#        memory at address 0x40"); then right.  "break f10" lands after
#        the copy.
#   14   gdb's $sp of a frame is its data-stack pointer at the call
#        (bottom of its frame), $fp its entry stack pointer ($sp of its
#        caller); MCST libgcc's _Unwind_GetCFA is the frame's $sp.
#   15-16 TLS: lcc's DW_OP_addr + DW_OP_GNU_push_tls_address and GCC's
#        DW_OP_const8u (or const4u) + DW_OP_GNU_push_tls_address or
#        DW_OP_form_tls_address read the right thread's value, dynamic
#        and static (this program links pthread_create).
#   misc a program without threads: TLS works dynamic, static is "Cannot
#        find thread-local storage"; "next" over a call that longjmps
#        runs to the end of the program (e2k_get_longjmp_target fails:
#        it reads the caller's %r0, like stops 8-10); lcc -O0's
#        __float128 parameter (DW_OP_reg2) prints the caller's %r2/%r3.
#   gline lcc -gline's .debug_info has a compile unit with no address
#        range, so gdb uses none of its line table ("No line number
#        information").
set -eu
dest=$1
target=$2
port=${3:-}
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
lcc=/opt/mcst/lcc-1.29.16.e2k-$target.linux-6.1/bin/lcc
out=${OUT:-$root/toolchains/debuginfo}
rdir=${RDIR:-relcc2/debuginfo}
host=${NAME:-${dest#*@}}
mkdir -p "$out"
v5=
case $target in v3*) ;; *) v5=-Wa,--defsym,V5=1 ;; esac
for kind in dyn static; do
  flags=
  [ $kind = static ] && flags=-static
  # shellcheck disable=SC2086
  "$lcc" -O0 -g -pthread $v5 $flags "$here/debuginfo.c" "$here/debuginfo.s" \
    -o "$out/debuginfo-$target-$kind"
  # shellcheck disable=SC2086
  "$lcc" -O0 -g $flags "$here/debuginfo_misc.c" -o "$out/debuginfo_misc-$target-$kind"
done
"$lcc" -O3 -gline "$here/debuginfo_gline.c" -o "$out/debuginfo_gline-$target"
jline=$(grep -n 'JUMP_LINE \*/' "$here/debuginfo_misc.c" | cut -d: -f1)
lline=$(grep -n 'LOOP_LINE \*/' "$here/debuginfo_gline.c" | cut -d: -f1)
ssh=("$root/tools/setup/e2k-ssh")
scp=("$root/tools/setup/e2k-scp")
if [ -n "$port" ]; then ssh+=(-p "$port"); scp+=(-P "$port"); fi
"${ssh[@]}" "$dest" "mkdir -p $rdir"
"${scp[@]}" "$out/debuginfo-$target-dyn" "$out/debuginfo-$target-static" \
  "$out/debuginfo_misc-$target-dyn" "$out/debuginfo_misc-$target-static" \
  "$out/debuginfo_gline-$target" \
  "$here/debuginfo.gdb" "$dest:$rdir/"
for kind in dyn static; do
  "${ssh[@]}" "$dest" "cd $rdir && bash -c '
    args=(); while IFS= read -r l; do
      case \"\$l\" in \"\"|\"#\"*) ;; *) args+=(-ex \"\$l\");; esac
    done < debuginfo.gdb
    gdb --version | head -1; uname -r
    echo \"--- program alone\"; ./debuginfo-$target-$kind
    echo \"--- gdb\"; gdb -batch -nx \"\${args[@]}\" ./debuginfo-$target-$kind 2>&1'" \
    > "$out/$host-$target-$kind.txt" 2>&1 || true
  echo "$out/$host-$target-$kind.txt"
  "${ssh[@]}" "$dest" "cd $rdir && gdb -batch -nx -ex 'break misc_stop' \
    -ex 'break fq' -ex run -ex 'p tv' -ex 'p &tv' -ex 'info threads' \
    -ex continue -ex 'info args' -ex 'p \$r2' -ex 'p \$r3' -ex up \
    -ex 'p \$r2' -ex 'p \$r3' \
    -ex 'break debuginfo_misc.c:$jline' -ex continue -ex next \
    -ex 'p \$pc' -ex 'info line *\$pc' -ex next -ex 'p \$pc' \
    ./debuginfo_misc-$target-$kind 2>&1" \
    > "$out/$host-$target-misc-$kind.txt" 2>&1 || true
  echo "$out/$host-$target-misc-$kind.txt"
done
"${ssh[@]}" "$dest" "cd $rdir && gdb -batch -nx \
  -ex 'info line debuginfo_gline.c:$lline' \
  -ex 'break debuginfo_gline.c:$lline' -ex 'info breakpoints' \
  -ex 'info line f' -ex 'x/3i f' ./debuginfo_gline-$target 2>&1" \
  > "$out/$host-$target-gline.txt" 2>&1 || true
echo "$out/$host-$target-gline.txt"
[ "${KEEP:-0}" = 1 ] || "${ssh[@]}" "$dest" "rm -rf $rdir"
