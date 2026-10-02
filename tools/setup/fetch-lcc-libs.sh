#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Download the open-source runtime libraries that MCST ships with lcc
# (GCC 11's libgcc, libatomic, libquadmath and libstdc++ with E2K changes,
# GPL with the GCC runtime exception) and unpack them under
# toolchains/src/lcc-VERSION/.
#
# Usage: tools/setup/fetch-lcc-libs.sh [VERSION]
#   VERSION defaults to 1.29.16. The E2K libgcc files end up in
#   toolchains/src/lcc-VERSION/lib/GNU/gcc-11/libgcc/config/e2k/.
set -e

version=${1:-1.29.16}
root=$(cd "$(dirname "$0")/../.." && pwd)
series=$(echo "$version" | cut -d. -f1-2)
file=internal-lcc-libs-open-source-$version.tar.xz
url=https://setwd.ws/sp/$series/$version/src/$file
out=$root/toolchains/src

mkdir -p "$out"
rm -rf "$out/lcc-$version"
curl -fL --progress-bar -o "$out/$file" "$url"
tar -xJf "$out/$file" -C "$out"
rm "$out/$file"
test -f "$out/lcc-$version/lib/GNU/gcc-11/libgcc/config/e2k/unwind-dw2-e2k.c" \
	|| { echo "unexpected archive layout" >&2; exit 1; }
echo "$out/lcc-$version"
