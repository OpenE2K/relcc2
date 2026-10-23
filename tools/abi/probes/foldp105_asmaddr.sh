#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# An "m" asm operand printed right after a channel digit
# (spec/abi/source-compat.md, "Other constraints"; the C side and the
# kernel's uses are in foldp105_asmaddr.c's header). Compiles
# foldp105_asmaddr.c with lcc 1.29.16 (e2k-v3, e2k-v5.8c2, e2k-v6.2c3)
# and 1.31.05 (e2k-v3, toolchains/lcc-1.31.05-e2k-v3, skipped if absent;
# -O0 and -O2) and assembles lcc's output, then assembles the forms an
# address without a register base takes (a short literal base 0x0 and the
# symbol as a 64-bit literal offset), glued to the channel and after a
# space. GCC='cmd' also compiles and assembles the .c with that compiler.
# Nothing runs.
#
# Usage: tools/abi/probes/foldp105_asmaddr.sh  (output in
#        toolchains/foldp105/)
#
# 2026-10-24, lcc 1.29.16 and 1.31.05 and MCST gas 2.41
# (toolchains/binutils): every lcc prints every operand with a register
# base and a zero offset ("{std,5%g16, 0x0, %r0}" at -O2, "%r3, 0x0" at
# -O0), which gas takes; "{std,50x0, prev_sclkr, %g27}" is "invalid
# ALC50 specified" (channel 2: "invalid ALC20"), "{ldd,00x0, ...}"
# "Invalid argument in position of SRC1", "{ldd,10x0, ...}" "invalid
# ALC10"; with a space or a tab before 0x0 all assemble
# (R_E2K_64_ABS_LIT against the symbol).

root=$(cd "$(dirname "$0")/../../.." && pwd)
out=$root/toolchains/foldp105
as=$root/toolchains/binutils/bin/e2k-linux-as
mkdir -p "$out"
cd "$out" || exit 1

for t in e2k-v3 e2k-v5.8c2 e2k-v6.2c3 1.31.05-e2k-v3; do
	case $t in
	1.31.05-*) lcc=$root/toolchains/lcc-$t/lcc
		[ -x "$lcc" ] || continue ;;
	*) lcc=/opt/mcst/lcc-1.29.16.$t.linux-6.1/bin/lcc ;;
	esac
	for o in -O0 -O2; do
		$lcc $o -S "$root/tools/abi/probes/foldp105_asmaddr.c" \
			-o lcc.s || exit 1
		if $as -mcpu=elbrus-v3 lcc.s -o lcc.o 2>as.err; then
			r=assembles
		else
			r="gas: $(grep -m1 Error as.err)"
		fi
		echo "lcc $t $o: $(grep -E '\{(st|ld)d,[0-9]' lcc.s |
			tr -d '\t' | tr '\n' ' ')-> $r"
	done
done

if [ -n "$GCC" ]; then
	for o in -O0 -O2; do
		$GCC $o -S "$root/tools/abi/probes/foldp105_asmaddr.c" \
			-o gcc.s || exit 1
		if $as -mcpu=elbrus-v3 gcc.s -o gcc.o 2>as.err; then
			r=assembles
		else
			r="gas: $(grep -m1 Error as.err)"
		fi
		echo "gcc $o: $(grep -E '\{(st|ld)d,[0-9]' gcc.s |
			tr -d '\t' | tr '\n' ' ')-> $r"
	done
fi

for line in '{std,50x0, prev_sclkr, %g27}' '{std,20x0, prev_sclkr, %g27}' \
	'{ldd,00x0, prev_sclkr, %g27}' '{ldd,10x0, prev_sclkr, %g27}' \
	'{std,5 0x0, prev_sclkr, %g27}' '{std,5	0x0, prev_sclkr, %g27}' \
	'{ldd,0 0x0, prev_sclkr, %g27}' '{std,5%g16, 0x0, %g27}'; do
	printf '\t.text\n\t.ignore ld_st_style\nf:\n\t%s\n' "$line" > lit.s
	if $as -mcpu=elbrus-v3 lit.s -o lit.o 2>as.err; then
		r="assembles$("${as%as}objdump" -dr lit.o |
			awk '/R_E2K/ { printf ", %s %s", $2, $3 }')"
	else
		r="$(grep -m1 Error as.err | sed 's/.*Error: //')"
	fi
	echo "gas \"$line\": $r"
done
