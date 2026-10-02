#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Download the open-source libraries that MCST ships with lcc and unpack
# them under toolchains/src/:
#   lcc-VERSION/  GCC 11's libgcc, libatomic, libquadmath and libstdc++
#                 with E2K changes (GPL with the GCC runtime exception);
#                 the E2K libgcc files are in lib/GNU/gcc-11/libgcc/config/e2k/
#   libffi-*/     libffi with an E2K port (src/e2k/)
#   gcov-*/       MCST's gcov
#
# Usage: tools/setup/fetch-lcc-libs.sh [VERSION]
#   VERSION defaults to 1.29.16.
set -e

version=${1:-1.29.16}
root=$(cd "$(dirname "$0")/../.." && pwd)
series=$(echo "$version" | cut -d. -f1-2)
base=https://setwd.ws/sp/$series/$version/src
out=$root/toolchains/src

mkdir -p "$out"
files=$(curl -fsL "$base/" | grep -o 'href="[^"]*\.tar\.xz"' | sed 's/href="\(.*\)"/\1/' \
	| grep -E '^(internal-lcc-libs-open-source|libffi|gcov)-')
[ -n "$files" ] || { echo "no archives found at $base/" >&2; exit 1; }
for file in $files; do
	rm -rf "$out/${file%.tar.xz}"
	curl -fL --progress-bar -o "$out/$file" "$base/$file"
	tar -xJf "$out/$file" -C "$out"
	rm "$out/$file"
	echo "$file"
done
test -f "$out/lcc-$version/lib/GNU/gcc-11/libgcc/config/e2k/unwind-dw2-e2k.c" \
	|| { echo "unexpected archive layout" >&2; exit 1; }
