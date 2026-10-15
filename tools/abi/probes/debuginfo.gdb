# SPDX-License-Identifier: GPL-3.0-or-later
# gdb script for debuginfo.c / debuginfo.s (see debuginfo.c for how to
# build, debuginfo.sh for how to run).  "want" lines give what the
# registers hold; gdb's answer follows each.
set pagination off
set width 0
set confirm off
set print pretty off
break *di_rot_s1
break *di_rot_loop
break *di_rot_s2
break *di_rot_s3
break *di_leaf_s0
break *di_leaf_s1
break *di_wide_s
break *f10
break di_cfa_stop
break di_tls_stop
break f10
info breakpoints
run

echo \n=== 1 di_rot_s1: setbn rsz 3 rbs 4 rcur 1, %b[k] = %r(8 + (2 + k) % 8)\n
p $br
echo want vb0 = r10 0xa00, vb1 = r11 0xb00, vb5 = r15 0xf00, vb6 = r8 0x800, vb7 = r9 0x900, vb8 none, vr10 0xa00\n
p/x vb0
p/x vb1
p/x vb5
p/x vb6
p/x vb7
p/x vb8
p/x vr10
p/x $b0
p/x $__dwb0
p/x $b8
p/x $r10
info br-registers
continue

echo \n=== 2 di_rot_loop iteration 1 (rcur 1)\n
p $br
echo want vb0 = 0xa00 (r10), vb7 = 0x900 (r9)\n
p/x vb0
p/x vb7
continue
echo \n=== 3 di_rot_loop iteration 2 (one abn: rcur 0)\n
p $br
echo want vb0 = 0x800 (r8), vb7 = 0xf00 (r15)\n
p/x vb0
p/x vb7
continue
echo \n=== 4 di_rot_loop iteration 3 (two abn: rcur 3)\n
p $br
echo want vb0 = 0xe00 (r14), vb7 = 0xd00 (r13)\n
p/x vb0
p/x vb7
continue
echo \n=== 5 di_rot_s2 after the loop (three abn: rcur 2)\n
p $br
echo want vb0 = 0xc00 (r12)\n
p/x vb0
continue
echo \n=== 5a di_rot_s3: setbn rsz 3 rbs 0x20 rcur 0 (%b[0] = %r64)\n
p $br
echo want vb0 = vb7 - 7 = 0x4000, vb1 0x4001, vb7 0x4007, vb8 none, $r64 0x4000\n
p/x vb0
p/x vb1
p/x vb7
p/x vb8
p/x $r64
p/x $r71
continue

echo \n=== 6 di_leaf_s0 from di_outer: leaf before its setbn (inherits di_outer's br: rbs 4 rsz 3 rcur 2)\n
p $br
p $wd
p $cr1_lo
p $cr1_hi
echo want l_b0 = leaf r12 0x200c, l_b3 = leaf r15 0x200f, l_b4 = leaf r8 0x2008, l_r15 0x200f, l_r16/l_r20 beyond the leaf's window\n
p/x l_b0
p/x l_b3
p/x l_b4
p/x l_r15
p/x l_r16
p/x l_r20
p/x $r16
p/x $r20
p/x $r40
up
echo --- up: di_outer (call wbs 0xc, wsz 0x10, br rbs 4 rsz 3 rcur 2)\n
p/x $pc
info symbol $pc
p $br
p $wd
echo want o_r2 0x1002, o_r23 0x1017, o_r24 = leaf r0 0x2000, o_r31 = leaf r7 0x2007, o_r40 none\n
p/x o_r2
p/x o_r23
p/x o_r24
p/x o_r31
p/x o_r40
p/x $r24
p/x $r40
echo want o_b0 = r12 0x100c, o_b7 = r11 0x100b, o_b8 none\n
p/x o_b0
p/x o_b7
p/x o_b8
echo want o_ll_call 0x1002 (range covers the call instruction, gdb looks up pc - 1), o_ll_ret <optimized out> (range starts at the return pc)\n
p/x o_ll_call
p/x o_ll_ret
info frame
continue

echo \n=== 7 di_leaf_s1 from di_outer: leaf after setbn rsz 1 rbs 1 rcur 1 (%b[k] = %r(2 + (2 + k) % 4))\n
p $br
echo want l_b0 = r4 0x2004, l_b3 = r3 0x2003, l_b4 none\n
p/x l_b0
p/x l_b3
p/x l_b4
up
echo --- up: di_outer, its %b[] still through its own br (cr1_hi.br)\n
p $br
echo want o_b0 0x100c, o_b7 0x100b\n
p/x o_b0
p/x o_b7
continue

echo \n=== 8 di_wide_s: 16-byte values, innermost frame\n
echo want ld_pair 1.5 (also as pieces), ld_fx 1.5 (one register; %r7 = 0x4000, so read as N, N + 1 it is 3), vec2 {0x1111, 0x2222}, i128 0x4444...3333, f128 1, qv (v5) {0x6666, 0x5555} with %r15 = 0x7777\n
echo (the caller di_wcall holds long double 2.5 in its %r4/%r5 and __float128 2.0 in its %r12/%r13)\n
p ld_pair
p ld_pieces
p ld_piece16
p ld_fx
p ld_fx_piece16
p/x $r6
p/x $r7
p $xr6
p/x vec2
p/x vec2_pieces
p/x vec2_swapped
p/x i128
p f128
p f128_pieces
p/x qv
p/x qv_low
p/x $qpr14
p/x $r14
p/x $r15
continue

echo \n=== 9 di_leaf_s0 from di_wide\n
up
echo --- up: di_wide as an outer frame (call wbs 8); want as in 8\n
p ld_pair
p ld_pieces
p ld_piece16
p ld_fx
p ld_fx_piece16
p $xr6
p/x vec2
p/x i128
p f128
p f128_pieces
p/x qv
p/x qv_low
p/x $qpr14
up
echo --- up: di_wcall\n
p/x $r4
p/x $r5
p/x $r12
p/x $r13
continue
echo \n=== 10 di_leaf_s1 from di_wide\n
continue

echo \n=== 11 f10 at its first instruction (before setwd and getsp)\n
x/2i $pc
p/x $sp
p/x $fp
p/x $r8
info args
info locals
stepi
echo \n=== 12 f10 after one stepi (setwd, getsp done; %r8 not yet set)\n
p/x $pc
p/x $r8
info args
stepi
echo \n=== 13 f10 after two stepi (%r8 = frame base)\n
p/x $pc
p/x $sp
p/x $fp
p/x $r8
info args
continue

echo \n=== 14 di_cfa_stop: gdb's $sp and $fp per frame against _Unwind_GetCFA\n
bt
frame 1
p/x $pc
p/x $sp
p/x $fp
frame 2
p/x $pc
p/x $sp
p/x $fp
frame 3
p/x $pc
p/x $sp
p/x $fp
continue

echo \n=== 15 di_tls_stop in the second thread (t 42, st 43, di_tls 44 = 0x2c)\n
info threads
p t
p st
p &t
p/x di_tls
p/x di_tls_addr
p/x di_tls_gnu
p/x di_tls_form
p/x di_tls_4
thread 1
echo --- thread 1 (t 5, st 7, di_tls 0x1234)\n
p t
p/x di_tls_form
p/x di_tls_addr
continue
echo \n=== 16 di_tls_stop in the main thread after join (t 5, st 7, di_tls 0x1234)\n
p t
p st
p/x di_tls_addr
p/x di_tls_gnu
p/x di_tls_form
p/x di_tls_4
continue
