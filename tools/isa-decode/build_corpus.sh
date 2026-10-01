#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build E2K object files from corpus/ for every ISA version:
#   - corpus/*.c with the Unipro clang at -O1..-O3, assembled by MCST gas;
#   - corpus/*.s (hand-written) with MCST gas directly, for the versions
#     given by the "isets: N-M" marker on their first line.
#
# Usage: build_corpus.sh OUTDIR
# Environment (defaults point into toolchains/, see tools/setup/):
#   CLANG     Unipro clang (an x86_64 host build with the Elbrus target)
#   BINUTILS  directory containing e2k-linux-as
#
# Unipro's assembly needs small fixes to assemble with MCST gas:
#   - %empty.hi is spelled %empty (same encoding);
#   - no .addrsig directives (-fno-addrsig);
#   - non-PIC only: gas rejects its GOT label arithmetic;
#   - its locked-store syntax for atomics isn't accepted, so the corpus avoids
#     atomics for now.
set -e

out=${1:?usage: $0 OUTDIR}
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
CLANG=${CLANG:-$root/toolchains/unipro/bin/clang}
BINUTILS=${BINUTILS:-$root/toolchains/binutils/bin}
mkdir -p "$out"

for v in 3 4 5 6 7; do
	for src in "$here"/corpus/*.c; do
		name=$(basename "$src" .c)
		for opt in O1 O2 O3; do
			base="$out/$name-v$v-$opt"
			"$CLANG" --target=e2k64-linux -march=elbrus-v$v -$opt \
				-fno-PIC -fno-addrsig -S -o "$base.s" "$src" 2>/dev/null
			sed -i 's/%empty\.hi\b/%empty/g' "$base.s"
			"$BINUTILS/e2k-linux-as" -mcpu=elbrus-v$v -o "$base.o" "$base.s"
		done
	done
	for src in "$here"/corpus/*.s; do
		name=$(basename "$src" .s)
		# The first lines of each .s file say which versions it is for,
		# e.g. "/* isets: 3-5".
		range=$(sed -n '1,3s/.*isets: *\([0-9]\)-\([0-9]\).*/\1 \2/p' "$src")
		set -- $range
		[ "$v" -ge "${1:-3}" ] && [ "$v" -le "${2:-7}" ] || continue
		"$BINUTILS/e2k-linux-as" -mcpu=elbrus-v$v -o "$out/$name-v$v.o" "$src"
	done
done
