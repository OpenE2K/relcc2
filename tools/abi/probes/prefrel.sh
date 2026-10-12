#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# R_E2K_PREF for local and global labels (spec/abi/elf.md, "Relocation
# types"). Assembles tools/abi/probes/prefrel.s with lcc 1.29.16's gas,
# links it with lcc, prints the relocations of the four prefs and, for
# each linked pref, the 128-byte block its disp field points at against
# the block that holds its label. Nothing is run.
#
# Usage: tools/abi/probes/prefrel.sh     (output in toolchains/prefrel/)
#
# lcc 1.29.16 (gas and ld 2.41), 2026-10-12: gas emits the .L label and
# the plain local label against *ABS* plus their section offsets
# (*ABS*+0x1004, *ABS*+0x1014), the global ones against the symbol
# (glob+0x4, main+0x4); after linking the two global prefs point at
# their labels' blocks and the two local ones at 0x1000, the section
# offset taken as an absolute address.

root=$(cd "$(dirname "$0")/../../.." && pwd)
out=$root/toolchains/prefrel
lcc=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1
bu=$lcc/binutils/bin
mkdir -p "$out"
cd "$out" || exit 1

$bu/e2k-linux-as -mcpu=elbrus-v3 -mptr64 "$root/tools/abi/probes/prefrel.s" \
	-o prefrel.o || exit 1
$lcc/bin/lcc prefrel.o -o prefrel || exit 1

echo "== relocations in prefrel.o"
$bu/e2k-linux-objdump -dr prefrel.o |
	awk '/pref/ { p = $0 } /R_E2K/ && p { print p " -> " $2, $3; p = "" }' |
	tr -s ' \t' ' ' | sed 's/^ //'

echo "== linked prefs"
$bu/e2k-linux-nm prefrel | awk '{ print $3, $1 }' | sort > syms.txt
$bu/e2k-linux-objdump -d prefrel | awk '
	/^ *[0-9a-f]+:/ { addr = $1; sub(/:$/, "", addr) }
	/pref %ipr/ {
		match($0, /disp = 0x[0-9a-f]+/)
		print addr, substr($0, RSTART + 7, RLENGTH - 7), $2
	}' | while read -r addr disp ipr; do
	a=$((0x$addr))
	d=$(($disp))
	[ $d -ge $((0x800000)) ] && d=$((d - 0x1000000))
	t=$(((a & ~127) + d * 128))
	case $ipr in
	%ipr0,) lab=lloc_at ;;
	%ipr1,) lab=glob ;;
	%ipr2,) lab=loc2_at ;;
	%ipr3,) lab=main ;;
	esac
	l=$((0x$(awk -v s=$lab '$1 == s { print $2 }' syms.txt)))
	if [ $t -eq $((l & ~127)) ]; then v=right; else v=wrong; fi
	printf '%s at 0x%x: target block 0x%x, label %s at 0x%x: %s\n' \
		"${ipr%,}" "$a" "$t" "$lab" "$l" "$v"
done
