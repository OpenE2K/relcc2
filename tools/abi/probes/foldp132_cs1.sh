#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Which control operations an AAU register access (aaurw/aaurr, whose
# MAS 0x3f goes in CS1) can share a wide instruction with
# (spec/isa/bundle.md, "CS1" and "Bundling restrictions";
# doc/gcc-port-plan.md 7.4; the proof of concept's finding P132: an
# aaurw stopping array prefetch beside setbn's setwd). Assembles one
# small file per case with MCST gas for elbrus-v3, -v5 and -v6 and
# prints "assembles" or gas's first error. Nothing runs.
#
# Usage: tools/abi/probes/foldp132_cs1.sh  (output in toolchains/foldp132/)
#
# 2026-11-03, MCST gas 2.41 (toolchains/binutils), the same on v3, v5
# and v6: aaurw with setwd, with setbn, with setwd + setbn, with wait and
# with call is "cannot encode MAS: CS1 syllable is busy with
# set-instructions"; aaurw with a lock load in ALC0, with an aaurrd in
# ALC5 and with a MAS store in ALC5 assembles (one MAS field per
# channel).

root=$(cd "$(dirname "$0")/../../.." && pwd)
out=$root/toolchains/foldp132
as=$root/toolchains/binutils/bin/e2k-linux-as
mkdir -p "$out"
cd "$out" || exit 1

aaurw='aaurw,2 %r0, %aabf0'
check () {
	name=$1
	shift
	for v in 3 5 6; do
		{
			printf '\t.text\n\t.global f\nf:\n'
			printf '\t{\n\t  disp %%ctpr1, f\n\t}\n'
			printf '\t{\n'
			for op in "$@"; do
				printf '\t  %s\n' "$op"
			done
			printf '\t}\n'
		} > case.s
		if $as -mcpu=elbrus-v$v case.s -o case.o 2>as.err; then
			r=assembles
		else
			r=$(grep -m1 Error as.err | sed 's/.*Error: //')
		fi
		echo "v$v $name: $r"
	done
}

check "aaurw + setwd" "$aaurw" "setwd wsz = 0x8, nfx = 0x1, dbl = 0x0"
check "aaurw + setbn" "$aaurw" "setbn rsz = 0x3, rbs = 0x4, rcur = 0x0"
check "aaurw + setwd + setbn" "$aaurw" \
	"setwd wsz = 0x8, nfx = 0x1, dbl = 0x0" \
	"setbn rsz = 0x3, rbs = 0x4, rcur = 0x0"
check "aaurw + wait" "$aaurw" "wait all_c = 1"
check "aaurw + call" "$aaurw" "call %ctpr1, wbs = 0x4"
check "aaurw + lock load" "$aaurw" "ldd,0,sm %dr0, 0x0, %dr1, mas = 0x4"
check "aaurw + aaurrd" "$aaurw" "aaurrd,5 %aasti1, %dr2"
check "aaurw + MAS store" "$aaurw" "std,5 %dr0, 0x0, %dr1, mas = 0x2"
