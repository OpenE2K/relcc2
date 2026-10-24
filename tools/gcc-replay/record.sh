#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# gcc-replay, the x86 half (README.md): builds the native compiler N, GCC
# for E2K hosted on E2K, as a canadian cross with the x86-hosted cross
# toolchain C of the same commit, through wrap/driver, which keeps C's
# preprocessed source, assembly and compiler-proper command for every
# object it compiles.  replay.py then runs those commands with N on an
# Elbrus machine and compares.
#
#   record.sh NAME CROSS [--src DIR] [--rev REV] [--tune CPU] [-j N]
#             [--prereqs DIR] [--out DIR] [--machine-dir DIR]
#
# CROSS is C's prefix (CROSS/bin/e2k-linux-gnu-gcc and its sysroot
# CROSS/e2k-linux-gnu/sys-root), built from the same commit.  --src is
# the GCC repository (default toolchains/gcc-claude), --rev its commit
# (default HEAD, resolved here once).  --tune CPU: as C was configured
# (elbrus-8c2 for the 8C2's cross; none for v3), so N is that CPU's
# compiler and C compiles N for that CPU.  --prereqs: gmp, mpfr and mpc
# source trees, built in-tree (default toolchains/gcc-replay/prereqs, made
# by contrib/download_prerequisites --directory=DIR --no-isl).
#
# The result is OUT/NAME (default toolchains/gcc-replay/NAME): src, build
# (the records beside the objects), inst (N, installed with DESTDIR) and
# the logs.  The build runs in bwrap with OUT/NAME mounted at the
# directory it will have on the machine (default
# /home/claude/relcc2/gcc-replay/NAME), so the recorded directories exist
# there as they are; that path is kept in OUT/NAME/machine-dir.
{
set -e
T=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$T/../.." && pwd)
[ $# -ge 2 ] || { sed -n '11,12p' "$0" >&2; exit 1; }
name=$1 X=$(cd "$2" && pwd); shift 2
src=$root/toolchains/gcc-claude rev=HEAD tune= jobs=$(nproc)
prereqs=$root/toolchains/gcc-replay/prereqs out=$root/toolchains/gcc-replay
mdir=/home/claude/relcc2/gcc-replay
while [ $# -gt 0 ]; do
	case $1 in
	--src) src=$(cd "$2" && pwd); shift 2 ;;
	--rev) rev=$2; shift 2 ;;
	--tune) tune=$2; shift 2 ;;
	-j) jobs=$2; shift 2 ;;
	--prereqs) prereqs=$(cd "$2" && pwd); shift 2 ;;
	--out) mkdir -p "$2"; out=$(cd "$2" && pwd); shift 2 ;;
	--machine-dir) mdir=${2%/}; shift 2 ;;
	*) echo "record.sh: unknown argument $1" >&2; exit 1 ;;
	esac
done
case $HOME in
/home/*) ;;
*) echo "record.sh: \$HOME must be under /home (bwrap mounts a tmpfs there)" >&2; exit 1 ;;
esac
for p in gmp mpfr mpc; do
	[ -d "$prereqs/$p" ] || { echo "record.sh: no $prereqs/$p; run" \
		"$src/contrib/download_prerequisites --directory=$prereqs --no-isl" >&2; exit 1; }
done
[ -x "$X/bin/e2k-linux-gnu-gcc" ] || { echo "record.sh: no $X/bin/e2k-linux-gnu-gcc" >&2; exit 1; }
commit=$(git -C "$src" rev-parse "$rev^{commit}")
short=$(git -C "$src" rev-parse --short=11 "$commit")
if tag=$(git -C "$src" describe --tags --abbrev=0 --match 'v[0-9]*' "$commit" 2>/dev/null); then
	version=${tag#v}+$(git -C "$src" rev-list --count "$tag..$commit")
else
	version=$short
fi
# C must be this commit's cross (its pkgversion names the commit).
cv=$("$X/bin/e2k-linux-gnu-gcc" --version | head -1)
case $cv in
*"$short"*) ;;
*) echo "record.sh: $X is not commit $short: $cv" >&2; exit 1 ;;
esac
H=$out/$name			# on this host
W=$mdir/$name			# inside bwrap and on the machine
case $W in
/home/*) ;;
*) echo "record.sh: --machine-dir must be under /home" >&2; exit 1 ;;
esac
prefix=/opt/relcc2/gcc-e2k-$version
rm -rf "$H"; mkdir -p "$H/src" "$H/build"
git -C "$src" archive "$commit" | tar -x -C "$H/src"
for p in gmp mpfr mpc; do
	cp -aL "$prereqs/$p" "$H/src/$p"
done
echo "$W" > "$H/machine-dir"
printf 'commit %s\nversion %s\ncross %s\ntune %s\n' \
	"$commit" "$version" "$X" "${tune:-none}" > "$H/record.txt"
echo "native $version ($short) by $X, tune ${tune:-none}, in $W"
# --with-build-sysroot: without it, configure (build != host) looks for
# the target's headers in $prefix/e2k-linux-gnu/sys-include, takes glibc
# as 0.0, and N then lacks @gnu_unique_object (and lowers -fhardened's
# _FORTIFY_SOURCE), which C has.
bwrap --dev-bind / / --tmpfs /home --bind "$HOME" "$HOME" --bind "$H" "$W" \
	env DN_REAL="$X/bin" PATH="$T/wrap:$X/bin:$PATH" bash -e -c '
	cd "$1/build"
	"$1/src/configure" --build=x86_64-pc-linux-gnu \
		--host=e2k-linux-gnu --target=e2k-linux-gnu --prefix="$2" \
		--with-build-sysroot="$3" ${7:+--with-tune="$7"} \
		--with-pkgversion="relcc2 PoC $4 $5 native" \
		--enable-languages=c,c++,lto --disable-multilib --disable-nls \
		--disable-libsanitizer --disable-libssp --disable-libitm \
		--disable-libquadmath --enable-checking=yes --disable-werror \
		--disable-bootstrap > "$1/configure.log" 2>&1
	make -j"$6" > "$1/make.log" 2>&1 || { echo "make failed: $1/make.log"; exit 1; }
	make DESTDIR="$1/inst" install-strip > "$1/install.log" 2>&1
	' record "$W" "$prefix" "$X/e2k-linux-gnu/sys-root" "$version" "$short" "$jobs" "$tune"
echo "$name: $(find "$H/build" -name '*.cc1' | wc -l) commands recorded"
# TARGET_GLIBC_* must be the sysroot's glibc, not 0.0 (above).
grep -E 'TARGET_GLIBC_M|GNU_UNIQUE' "$H/build/gcc/auto-host.h"
echo "$name done: $H/inst$prefix"
}; exit
