#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build MCST binutils (as, ld, objdump, ...) for the e2k-linux target on the
# host, from OpenE2K/binutils-gdb, into toolchains/binutils.
#
# Usage: tools/setup/build-binutils.sh [SOURCE_DIR]
set -e

root=$(cd "$(dirname "$0")/../.." && pwd)
src=${1:-$root/OpenE2K/binutils-gdb}
out=$root/toolchains/binutils
build=$root/toolchains/build/binutils

rm -rf "$build" "$out"
mkdir -p "$build"
cd "$build"
"$src/configure" --target=e2k-linux --prefix="$out" \
	--disable-gdb --disable-gdbserver --disable-sim --disable-nls \
	--disable-werror --disable-gprofng > configure.log 2>&1
make -j"$(nproc)" all-binutils all-gas all-ld > build.log 2>&1
make install-binutils install-gas install-ld > install.log 2>&1
"$out/bin/e2k-linux-as" --version | sed -n '1p'
