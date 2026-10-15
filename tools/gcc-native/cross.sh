#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build an x86_64-hosted cross toolchain for E2K (target e2k-linux-gnu)
# and pack it: MCST's binutils (OpenE2K/binutils-gdb) and the PoC GCC of
# a commit of toolchains/gcc-claude-native, built against an lcc cross
# compiler's e2k-v3 sysroot.  The tarball leaves the sysroot out (MCST's
# glibc and system libraries); its setup-sysroot.sh links one in from an
# installed lcc.
#
#   cross.sh [-j N] [--rev REV] [--lcc DIR] [--os NAME] [--stage STEP]
#            [--smoke-host USER@HOST]
#
# REV defaults to the worktree's HEAD, DIR to lcc 1.29.16's e2k-v3 tree,
# NAME (the OS whose sysroot lcc ships) to elbrus-linux-9.5.5.  The
# version is the PoC's tag and the commits on top of it, "0.4" or
# "0.4+native-build.7"; the result is
# toolchains/gcc-native/gcc-e2k-VERSION-x86_64-cross-NAME.tar.xz, built in
# toolchains/gcc-native/cross-VERSION/.  --stage resumes at binutils,
# sysroot, gcc, smoke or pack.  The smoke test runs a C and two C++
# programs on --smoke-host (default the 4C, v3; "" skips it), through
# tools/setup/e2k-ssh in ~/relcc2/cross-smoke-REV, removed afterwards.
{
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
poc=$root/toolchains/gcc-claude-native
jobs=8 rev=HEAD lcc=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1
os=elbrus-linux-9.5.5 stage=binutils smoke_host=claude@ringo.lan
while [ $# -gt 0 ]; do
	case $1 in
	-j) jobs=$2; shift 2 ;;
	--rev) rev=$2; shift 2 ;;
	--lcc) lcc=$2; shift 2 ;;
	--os) os=$2; shift 2 ;;
	--stage) stage=$2; shift 2 ;;
	--smoke-host) smoke_host=$2; shift 2 ;;
	*) echo "cross.sh: unknown argument $1" >&2; exit 1 ;;
	esac
done

commit=$(git -C "$poc" rev-parse "$rev")
short=$(git -C "$poc" rev-parse --short=11 "$commit")
tag=$(git -C "$poc" describe --tags --abbrev=0 --match 'v[0-9]*' "$commit")
n=$(git -C "$poc" rev-list --count "$tag..$commit")
branch=$(git -C "$poc" rev-parse --abbrev-ref HEAD)
version=${tag#v}
[ "$n" = 0 ] || version=$version+$branch.$n
bu=$root/OpenE2K/binutils-gdb
bucommit=$(git -C "$bu" rev-parse --short=11 HEAD)
glibc=$(strings "$lcc/fs/lib64/libc.so.6" |
	sed -n 's/.*stable release version \([^ ,]*\).*/\1/p' | sed 's/\.$//' | head -1)

out=$root/toolchains/gcc-native
W=$out/cross-$version
name=gcc-e2k-$version
P=$W/$name
S=$P/e2k-linux-gnu/sys-root
tarball=$out/$name-x86_64-cross-$os.tar.xz
mkdir -p "$W/logs"
log=$W/logs
echo "version $version ($short), binutils $bucommit, lcc $lcc (glibc $glibc), $os"

step() { echo "$(date '+%F %T') $*"; }
after() {
	# Whether STEP comes at or after the --stage to resume from.
	for s in binutils sysroot gcc smoke pack; do
		[ "$s" = "$stage" ] && return 0
		[ "$s" = "$1" ] && return 1
	done
	return 0
}

if after binutils; then
	step binutils
	rm -rf "$W/build-binutils" "$P"
	mkdir -p "$W/build-binutils"
	(cd "$W/build-binutils" &&
	 nice -n 10 "$bu/configure" --target=e2k-linux-gnu --prefix="$P" \
		--with-sysroot="$S" --with-pkgversion="MCST $bucommit" \
		--disable-gdb --disable-gdbserver --disable-sim --disable-nls \
		--disable-werror --disable-gprofng &&
	 nice -n 10 make -j"$jobs" all-binutils all-gas all-ld &&
	 make install-strip-binutils install-strip-gas install-strip-ld) \
		> "$log/binutils.log" 2>&1
fi

if after sysroot; then
	step sysroot
	cp "$root/tools/gcc-native/setup-sysroot.sh" "$P/setup-sysroot.sh"
	chmod +x "$P/setup-sysroot.sh"
	"$P/setup-sysroot.sh" "$lcc"
fi

if after gcc; then
	step "gcc: source"
	rm -rf "$W/src" "$W/build-gcc"
	mkdir -p "$W/src" "$W/build-gcc"
	git -C "$poc" archive "$commit" | tar -x -C "$W/src"
	step "gcc: build"
	(cd "$W/build-gcc" && PATH=$P/bin:$PATH &&
	 nice -n 10 "$W/src/configure" --target=e2k-linux-gnu --prefix="$P" \
		--with-sysroot="$S" \
		--with-pkgversion="relcc2 PoC $version $short" \
		--enable-languages=c,c++,lto --disable-multilib --disable-nls \
		--disable-libsanitizer --disable-libssp --disable-libitm \
		--disable-libquadmath --enable-checking=yes --disable-werror \
		--disable-bootstrap &&
	 nice -n 10 make -j"$jobs" &&
	 make install-strip) > "$log/gcc.log" 2>&1
fi

if after smoke; then
	step smoke
	t=$W/smoke
	rm -rf "$t"; mkdir -p "$t"
	printf '#include <stdio.h>\nint main (void) { printf ("hello from %%s\\n", __VERSION__); return 0; }\n' > "$t/hello.c"
	printf '#include <iostream>\n#include <vector>\nint main () { std::vector<int> v {1, 2, 3}; int s = 0; for (int x : v) s += x; std::cout << "sum " << s << std::endl; return s != 6; }\n' > "$t/hello.cc"
	"$P/bin/e2k-linux-gnu-gcc" -O2 -o "$t/hello" "$t/hello.c"
	"$P/bin/e2k-linux-gnu-g++" -O2 -static-libstdc++ -static-libgcc \
		-o "$t/hellocc" "$t/hello.cc"
	"$P/bin/e2k-linux-gnu-g++" -O2 -o "$t/hellocc-dyn" "$t/hello.cc"
	cp "$P"/e2k-linux-gnu/lib64/libstdc++.so.6 "$P"/e2k-linux-gnu/lib64/libgcc_s.so.1 "$t/"
	# On hardware: MCST's qemu lacks operations the PoC uses (cctop*).
	if [ -n "$smoke_host" ]; then
		d=relcc2/cross-smoke-$short
		ssh="$root/tools/setup/e2k-ssh $smoke_host"
		$ssh "mkdir -p $d"
		(cd "$t" && tar -cf - hello hellocc hellocc-dyn libstdc++.so.6 libgcc_s.so.1) |
			$ssh "cd $d && tar -xf - && ./hello && ./hellocc && LD_LIBRARY_PATH=. ./hellocc-dyn; r=\$?; cd && rm -rf $d; exit \$r"
	fi
fi

if after pack; then
	step pack
	cat > "$P/README.txt" <<EOF
GCC for Elbrus (E2K), relcc2 proof of concept $version

An experimental GCC backend for E2K written by an AI agent from the
relcc2 specification of the ISA and ABI.  It is not the hand-written port
meant for upstream and is not supported by MCST or the GCC project.

  GCC         relcc2 PoC $version, commit $commit
              (tag $tag, branch $branch)
  binutils    MCST binutils 2.41 (OpenE2K/binutils-gdb $bucommit)
  host        x86_64-linux-gnu
  target      e2k-linux-gnu, 64-bit unprotected mode, default
              -march=elbrus-v3 (runs on every CPU from v3 on)
  system      $os: the sysroot of lcc's e2k-v3 cross compiler
              ($(basename "$lcc"), glibc $glibc)
  languages   C, C++, LTO; --enable-checking=yes
  left out    libsanitizer, libitm, libssp, libquadmath, Fortran

Setting up: the tarball has no sysroot.  Point it at an installed lcc
cross compiler (an e2k-v3 tree gives programs for every CPU):

  ./setup-sysroot.sh /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1

Using it:

  export PATH=\$PWD/bin:\$PATH
  e2k-linux-gnu-gcc -O2 -o hello hello.c
  e2k-linux-gnu-g++ -O2 -o hello hello.cc

C++ programs need this toolchain's libstdc++, newer than the
system's: link with -static-libstdc++ -static-libgcc, or copy
e2k-linux-gnu/lib64/libstdc++.so.6 and libgcc_s.so.1 to the target and
point LD_LIBRARY_PATH at them.  -march=elbrus-v5 (8C2) and
-march=elbrus-v6 (2C3) give code for one CPU generation.

Licence: GCC and binutils are GPL-3.0-or-later; libgcc and libstdc++
carry the GCC Runtime Library Exception.  Sources: the commits above.
EOF
	tar -C "$W" --owner=0 --group=0 --numeric-owner --sort=name \
		--exclude="$name/e2k-linux-gnu/sys-root/*" \
		-I 'xz -T0 -6' -cf "$tarball" "$name"
	ls -l "$tarball"
fi
step done
}; exit
