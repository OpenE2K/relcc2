#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Whether MCST ld gives a copy relocation to data from a shared library
# that an executable reaches only through the GOT (spec/abi/elf.md,
# "Rarely used operators, ifunc and gold"; docs/spec-poc-findings.md
# A37).  Compiles a -fPIC object that reads stdout (R_E2K_GOT only) and
# a non-PIC main, links them -no-pie six times each with glibc's
# MALLOC_PERTURB_ unset, 1, 2, 3 and 4 (an odd value fills new malloc
# memory with an even byte, an even value with an odd byte), and prints
# for each setting the distinct outputs: their checksum, the number of
# R_E2K_64_COPY relocations and what relocates stdout's GOT slot.  On an
# Elbrus it also runs each distinct output (it prints "hi").
#
# Usage: tools/abi/probes/folda37_copyrel.sh [CC [LD]]
#   CC  the compiler driver (default: lcc 1.29.16 e2k-v3 in /opt/mcst on
#       x86, lcc on an Elbrus)
#   LD  a linker to run in place of the driver's own, with the driver's
#       ld command line (CC -v); e.g. the PoC cross's e2k-linux-gnu-ld
#   READELF (environment): an E2K readelf (default: the submodule's,
#       toolchains/binutils/bin, on x86; readelf on an Elbrus)
#   Output in toolchains/folda37/ (on an Elbrus: ./folda37/).
#
# Reading: "copy=0 slot=R_E2K_64_ABS" is the GOT slot bound to libc's
# stdout; "copy=1 slot=R_E2K_64_ABS" adds an R_E2K_64_COPY and the slot
# names the executable's copy.  Both are valid.  With the source of MCST
# binutils 2.41-29.023 (OpenE2K/binutils-gdb b4bebd940a3), the choice is
# the bit _bfd_e2k_elf_adjust_dynamic_symbol reads as eh->gotoff_ref
# (bfd/elfxx-e2k.c:4104), which _bfd_e2k_elf_link_hash_newfunc
# (:2604-2640) never sets, so it is whatever the hash table's memory
# held (bfd_hash_allocate does not clear it).
#
# 2026-11-03, lcc 1.29.16 e2k-v3 objects:
#   - The PoC cross's ld ("GNU ld (MCST b4bebd940a3) 2.41.0-29.023", an
#     x86 build of the submodule's source), LD=...: unset, 1 and 3:
#     copy=0; 2 and 4: copy=1, the output differing.
#   - The host's MCST ld 2.41-29.018 (Gentoo cross, /usr/bin/
#     e2k-mcst-linux-gnu-ld, which lcc in /opt/mcst runs): copy=0 for
#     every setting.
#   - Natively, lcc's driver with the system ld: the 4C (ALT's
#     binutils-2.41-alt2.29.023.1) and the 8C2 (OS Elbrus 9.5.5,
#     "GNU ld (GNU Binutils) 2.41.0-29.023"): unset, 1 and 3: copy=0; 2
#     and 4: copy=1; every output prints "hi".  The PIE (lcc -fPIE,
#     -pie) behaves the same with the PoC cross's ld.
#   With a bigger link (l++ -no-pie -static-libstdc++ of a program using
#   iostream and shared_ptr: lcc's libstdc++.a reaches stdin, stdout and
#   stderr through 8 R_E2K_GOT only), the PoC cross's ld makes copies of
#   all three under MALLOC_PERTURB_=2 and none under 1.

root=$(cd "$(dirname "$0")/../../.." && pwd)
if [ "$(uname -m)" = e2k ]; then
	native=1
	CC=${1:-lcc}
	READELF=${READELF:-readelf}
	out=$PWD/folda37
else
	native=
	CC=${1:-/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc}
	READELF=${READELF:-$root/toolchains/binutils/bin/e2k-linux-readelf}
	out=$root/toolchains/folda37
fi
LD=$2
mkdir -p "$out" && cd "$out" || exit 1

cat > got.c <<'EOF'
#include <stdio.h>
int f (void) { return fputs ("hi\n", stdout); }
EOF
cat > main.c <<'EOF'
int f (void);
int main (void) { return f () < 0; }
EOF
$CC -O2 -fPIC -c got.c -o got.o || exit 1
$CC -O2 -c main.c -o main.o || exit 1
echo "== got.o: references to stdout"
$READELF -rW got.o | awk '$5 == "stdout" { print "  " $3 }' | sort | uniq -c

if [ -n "$LD" ]; then
	$CC -v -no-pie main.o got.o -o ldline.tmp 2>&1 |
		sed -n 's/^-> //p' | grep -- ' -o ldline.tmp' | tail -n 1 |
		cut -d' ' -f2- > ldline.txt
	[ -s ldline.txt ] || { echo "no ld line in $CC -v"; exit 1; }
	echo "== linker: $LD"
	$LD --version | head -n 1
else
	echo "== linker: $CC's"
fi

for p in unset 1 2 3 4; do
	for i in 1 2 3 4 5 6; do
		if [ -n "$LD" ]; then
			args=$(sed "s| ldline.tmp | p$p.$i |" ldline.txt)
			if [ $p = unset ]; then
				$LD $args || exit 1
			else
				MALLOC_PERTURB_=$p $LD $args || exit 1
			fi
		elif [ $p = unset ]; then
			$CC -no-pie main.o got.o -o p$p.$i || exit 1
		else
			MALLOC_PERTURB_=$p $CC -no-pie main.o got.o -o p$p.$i ||
				exit 1
		fi
		sum=$(cksum < p$p.$i | cut -d' ' -f1)
		copy=$($READELF -rW p$p.$i | grep -c R_E2K_64_COPY)
		slot=$($READELF -rW p$p.$i |
			awk '$5 ~ /^stdout/ && $3 != "R_E2K_64_COPY" { print $3 }' |
			head -n 1)
		echo "$sum copy=$copy slot=${slot:-none} p$p.$i"
	done > runs.$p
	printf 'MALLOC_PERTURB_=%-5s' $p
	cut -d' ' -f1-3 runs.$p | sort | uniq -c | tr -s ' ' |
		tr '\n' ';' | sed 's/;$//'
	echo
	if [ -n "$native" ]; then
		cut -d' ' -f1 runs.$p | sort -u | while read -r s; do
			f=$(awk -v s="$s" '$1 == s { print $4; exit }' runs.$p)
			printf '  run %s (%s): %s\n' "$f" "$s" "$(./$f)"
		done
	fi
done
