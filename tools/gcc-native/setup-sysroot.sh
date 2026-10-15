#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Give the E2K cross toolchain this script ships with its sysroot,
# e2k-linux-gnu/sys-root next to bin/, out of an lcc cross compiler's fs/
# (MCST's glibc and system libraries, not part of the toolchain):
#
#   setup-sysroot.sh [LCC-DIR]
#
# LCC-DIR defaults to /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1; the e2k-v3
# tree gives programs that run on every CPU from v3 on (spec/abi/elf.md,
# "Mixing targets in one program").  The sysroot holds symlinks to lcc's
# files, except for
#   - real copies of the GNU ld scripts in lib64 (ld applies the sysroot
#     only to a script that lies inside it);
#   - empty lib and usr/lib, which GCC's search paths go through
#     (usr/lib/../lib64);
#   - a fixed bits/fenv.h: MCST glibc guards femode_t with
#     IEC_60559_BFP_EXT while fenv.h uses it under IEC_60559_BFP_EXT_C2X,
#     so <fenv.h> fails under C23, GCC's default.
# Run it again to point the toolchain at another lcc tree.
{
set -e
here=$(cd "$(dirname "$0")" && pwd)
lcc=${1:-/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1}
F=$(cd "$lcc/fs" 2>/dev/null && pwd) || {
	echo "setup-sysroot.sh: no $lcc/fs (an lcc cross compiler's tree)" >&2
	exit 1
}
[ -f "$F/usr/include/stdio.h" ] || {
	echo "setup-sysroot.sh: $F has no usr/include/stdio.h" >&2
	exit 1
}
S=$here/e2k-linux-gnu/sys-root
rm -rf "$S"
mkdir -p "$S/lib" "$S/usr/lib"
for d in etc lib128 lib32 sbin var; do
	[ -e "$F/$d" ] && ln -s "$F/$d" "$S/$d"
done
for d in bin lib128 lib32 sbin share; do
	[ -e "$F/usr/$d" ] && ln -s "$F/usr/$d" "$S/usr/$d"
done
for d in lib64 usr/lib64; do
	mkdir -p "$S/$d"
	for f in "$F/$d"/*; do
		b=$(basename "$f")
		if [ -f "$f" ] && [ ! -L "$f" ] &&
		   head -c 200 "$f" | grep -q "GNU ld script"; then
			cp "$f" "$S/$d/$b"
		else
			ln -s "$f" "$S/$d/$b"
		fi
	done
done
mkdir -p "$S/usr/include/bits"
for f in "$F/usr/include"/*; do
	[ "$(basename "$f")" = bits ] || ln -s "$f" "$S/usr/include/"
done
for f in "$F/usr/include/bits"/*; do
	[ "$(basename "$f")" = fenv.h ] || ln -s "$f" "$S/usr/include/bits/"
done
sed 's/^#if __GLIBC_USE (IEC_60559_BFP_EXT)$/#if __GLIBC_USE (IEC_60559_BFP_EXT_C2X)/' \
	"$F/usr/include/bits/fenv.h" > "$S/usr/include/bits/fenv.h"
echo "$lcc" > "$S/.lcc"
echo "sysroot: $S -> $F"
}; exit
