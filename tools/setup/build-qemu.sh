#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build qemu-e2k Linux user mode from both QEMU submodules:
#   OpenE2K/qemu-e2k-mcst      -> toolchains/qemu-mcst/bin/qemu-e2k
#   OpenE2K/qemu-e2k-community -> toolchains/qemu-community/bin/qemu-e2k{,32}
#
# MCST's emulator is the reference for instruction semantics; the community
# one is older but implements some things MCST's does not.
#
# Needs QEMU's usual build dependencies on the host (ninja, glib, ...).
#
# Usage: tools/setup/build-qemu.sh [mcst|community]...   (default: both)
set -e

root=$(cd "$(dirname "$0")/../.." && pwd)

build() {
	flavor=$1 targets=$2
	src=$root/OpenE2K/qemu-e2k-$flavor
	out=$root/toolchains/qemu-$flavor
	build=$root/toolchains/build/qemu-$flavor

	echo "== qemu-e2k-$flavor ($(git -C "$src" describe --always --tags))"
	rm -rf "$build" "$out"
	mkdir -p "$build"
	cd "$build"
	"$src/configure" --prefix="$out" --target-list="$targets" \
		--disable-system --disable-tools --disable-docs \
		--disable-guest-agent --disable-werror \
		> configure.log 2>&1 || { tail -20 configure.log; exit 1; }
	ninja > build.log 2>&1 || { grep -m5 -A5 'error' build.log; exit 1; }
	ninja install > install.log 2>&1
	for t in $(echo "$targets" | tr , ' '); do
		"$out/bin/qemu-${t%-linux-user}" --version | sed -n 1p
	done
}

[ $# -eq 0 ] && set -- mcst community
for flavor; do
	case $flavor in
	mcst) build mcst e2k-linux-user ;;
	community) build community e2k-linux-user,e2k32-linux-user ;;
	*) echo "unknown flavour: $flavor" >&2; exit 2 ;;
	esac
done
