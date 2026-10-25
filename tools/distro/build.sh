#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# tools/distro (README.md): a small E2K Linux system built from source by
# the PoC alone: MCST's Linux and glibc (OpenE2K), busybox, dropbear,
# binutils, gdb, GCC and its runtime libraries, libffi.  Nothing in it
# comes from lcc's trees or an installed Elbrus system.
#
#   build.sh CROSS [--out DIR] [--stage STEP] [--only STEP] [--skip STEP] [-j N]
#
# CROSS is an installed PoC cross toolchain on a sysroot of our own
# (toolchains/poc-cross/cross-VERSION-CPU-own/gcc-e2k-VERSION-CPU, made by
# cross-cpu.sh --sysroot): its sysroot is the system's glibc, and it
# builds everything else, the kernel included.  OUT (default
# toolchains/distro/out) gets build/STEP, logs/STEP.log, root/ (the
# whole system), initrd/ (the part that boots from memory) and the
# images.  --stage starts at STEP, --only runs it alone, --skip (any
# number of times) leaves it out.  Steps, in order:
#
#   runtime   glibc and GCC's runtime libraries from CROSS
#   busybox   busybox 1.37.0, defconfig
#   dropbear  the SSH server
#   ncurses   for gdb's readline
#   gmp       GMP, MPFR and MPC, for gdb and GCC
#   expat     for gdb
#   libffi    OpenE2K/libffi
#   binutils  OpenE2K/binutils-gdb: as, ld and the rest, hosted on E2K
#   gdb       OpenE2K/gdb, hosted on E2K
#   gcc       the PoC as the system's compiler (a canadian cross, as
#             tools/gcc-replay/record.sh builds it)
#   kernel    toolchains/poc06/linux-src, config-mcst-e8c2: zImage (what
#             the kernel's make install puts in /boot) and the modules
#   rootfs    tools/distro/rootfs and relcc2's public key for root
#   initrd    initrd.img: busybox, glibc and dropbear only
#   check     no file names /opt/mcst or has lcc's mark (rootfs looks
#             before it strips); every ELF file's .comment is the PoC's
#             (OUT/logs/check.log lists the compilers seen)
#   image     distro.img (mkimage.sh)
{
set -e
D=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$D/../.." && pwd)
[ $# -ge 1 ] || { sed -n '9,9p' "$0" >&2; exit 1; }
X=$(cd "$1" && pwd); shift
out=$R/toolchains/distro/out stage=runtime only= skip= jobs=$(nproc)
while [ $# -gt 0 ]; do
	case $1 in
	--out) mkdir -p "$2"; out=$(cd "$2" && pwd); shift 2 ;;
	--stage) stage=$2; shift 2 ;;
	--only) stage=$2 only=1; shift 2 ;;
	--skip) skip="$skip $2 "; shift 2 ;;
	-j) jobs=$2; shift 2 ;;
	*) echo "build.sh: unknown argument $1" >&2; exit 1 ;;
	esac
done
T=e2k-linux-gnu
S=$X/$T/sys-root
DL=$R/toolchains/distro/dl
GMP=$R/toolchains/poc06/prereqs
ROOT=$out/root
B=$out/build
# What gdb links against, apart from ROOT: its libc.so is a linker script
# naming /lib64/libc.so.6, which ld takes from the host when found by -L
# outside the sysroot.
DEPS=$B/depsroot
L=$out/logs
mkdir -p "$ROOT" "$B" "$L"
export PATH=$X/bin:$PATH
# CROSS's configure line: its commit (in the pkgversion) and its -mtune.
cv=$("$X/bin/$T-gcc" -v 2>&1)
pkg=$("$X/bin/$T-gcc" --version | head -1 | sed -E 's/^[^(]*\(([^)]*)\).*/\1/')
tune=$(echo "$cv" | grep -o -- '--with-tune=[^ ]*' | head -1 | cut -d= -f2)
commit=$(echo "$pkg" | grep -o -E '\b[0-9a-f]{11}\b' | head -1)

conf() {		# src build-dir configure-args...: an autoconf package
	local src=$1 bd=$2; shift 2
	rm -rf "$bd"; mkdir -p "$bd"
	cd "$bd" && "$src/configure" --build=x86_64-pc-linux-gnu --host=$T \
		--prefix=/usr --libdir=/usr/lib64 "$@"
}
mk() { make -C "$1" -j"$jobs" "${@:2}"; }
# binutils' and gdb's readline declares "extern int tputs ();", which C23,
# GCC's default since 15, takes for no arguments.
gnu17="-g -O2 -std=gnu17"
unpack() {		# tarball: into $B/src once; prints the directory
	mkdir -p "$B/src"
	local n; n=$(basename "$1"); n=${n%.tar.*}
	[ -d "$B/src/$n" ] || tar -xf "$1" -C "$B/src"
	echo "$B/src/$n"
}

s_runtime() {
	# The system's glibc is CROSS's sysroot, headers and crt files
	# included, for the native compiler.
	cp -a "$S/." "$ROOT/"
	rm -f "$ROOT/.sysroot-src" "$ROOT/.lcc"
	mkdir -p "$ROOT/usr/lib64"
	local l
	for l in libgcc_s.so.1 libstdc++.so.6 libatomic.so.1 libgomp.so.1 \
		 libquadmath.so.0; do
		[ -e "$X/$T/lib64/$l" ] || { echo "CROSS has no $l"; continue; }
		cp -a "$X/$T/lib64/$l"* "$ROOT/usr/lib64/"
	done
	rm -f "$ROOT/usr/lib64/"*-gdb.py
}

s_busybox() {
	local src; src=$(unpack "$R/toolchains/gcc-native/apps/busybox-1.37.0.tar.bz2")
	rm -rf "$B/busybox"; mkdir -p "$B/busybox"
	local K="make -C $src O=$B/busybox CROSS_COMPILE=$T-"
	$K defconfig
	# tc needs CBQ, which newer kernel headers dropped; the SHA
	# accelerations are x86's.
	sed -i -e 's/^CONFIG_TC=y/# CONFIG_TC is not set/' \
	       -e 's/^CONFIG_SHA1_HWACCEL=y/# CONFIG_SHA1_HWACCEL is not set/' \
	       -e 's/^CONFIG_SHA256_HWACCEL=y/# CONFIG_SHA256_HWACCEL is not set/' \
	       "$B/busybox/.config"
	yes '' | $K oldconfig > /dev/null
	$K -j"$jobs"
	$K CONFIG_PREFIX="$ROOT" install
}

s_dropbear() {
	local src; src=$(unpack "$DL/dropbear-2026.94.tar.bz2")
	local P="PROGRAMS=dropbear dropbearkey dbclient scp"
	conf "$src" "$B/dropbear" --disable-zlib --disable-lastlog \
		--disable-utmp --disable-wtmp --disable-utmpx --disable-wtmpx
	mk "$B/dropbear" "$P"
	mk "$B/dropbear" "$P" DESTDIR="$ROOT" install
}

s_ncurses() {
	local src; src=$(unpack "$DL/ncurses-6.6.tar.gz")
	conf "$src" "$B/ncurses" --with-shared --without-debug --without-ada \
		--without-tests --without-manpages --enable-widec --with-termlib \
		--with-build-cc=gcc --disable-stripping \
		--with-pkg-config-libdir=/usr/lib64/pkgconfig --enable-pc-files
	mk "$B/ncurses"
	mk "$B/ncurses" DESTDIR="$ROOT" install
	mk "$B/ncurses" DESTDIR="$DEPS" install
}

s_gmp() {
	# Static, in a prefix of their own: they serve only gdb (GCC builds
	# its own in-tree), and libtool's install-time relink of a shared one
	# searches the host's /usr/lib64 ("libc.so.6: file in wrong format").
	local P=$B/deps st="--disable-shared --enable-static"
	rm -rf "$P"
	conf "$GMP/gmp-6.3.0" "$B/gmp" $st --prefix="$P" --libdir="$P/lib"
	mk "$B/gmp"; mk "$B/gmp" install
	conf "$GMP/mpfr-4.2.2" "$B/mpfr" $st --prefix="$P" --libdir="$P/lib" \
		--with-gmp="$P"
	mk "$B/mpfr"; mk "$B/mpfr" install
	conf "$GMP/mpc-1.3.1" "$B/mpc" $st --prefix="$P" --libdir="$P/lib" \
		--with-gmp="$P" --with-mpfr="$P"
	mk "$B/mpc"; mk "$B/mpc" install
}

s_expat() {
	conf "$R/toolchains/claude-e2k/real/src/expat-2.8.5" "$B/expat" \
		--without-docbook --without-examples --without-tests
	mk "$B/expat"; mk "$B/expat" DESTDIR="$ROOT" install
	mk "$B/expat" DESTDIR="$DEPS" install
	rm -f "$ROOT/usr/lib64/libexpat.la" "$DEPS/usr/lib64/libexpat.la"
}

s_libffi() {
	rm -rf "$B/libffi-src"; mkdir -p "$B/libffi-src"
	git -C "$R/OpenE2K/libffi" archive HEAD | tar -x -C "$B/libffi-src"
	(cd "$B/libffi-src" && ./autogen.sh)
	conf "$B/libffi-src" "$B/libffi" --disable-docs --disable-static
	mk "$B/libffi"; mk "$B/libffi" DESTDIR="$ROOT" install
	rm -f "$ROOT/usr/lib64/libffi.la"
}

s_binutils() {
	conf "$R/OpenE2K/binutils-gdb" "$B/binutils" --target=$T \
		--disable-gdb --disable-gdbserver --disable-sim --disable-gprofng \
		--disable-gold --disable-nls --disable-werror CFLAGS="$gnu17"
	mk "$B/binutils"; mk "$B/binutils" DESTDIR="$ROOT" install
}

s_gdb() {
	conf "$R/OpenE2K/gdb" "$B/gdb" --target=$T --disable-binutils \
		--disable-ld --disable-gas --disable-gold --disable-gprof \
		--disable-gprofng --disable-sim --disable-nls --disable-werror \
		--without-python --without-guile --without-debuginfod \
		--disable-source-highlight --with-gmp="$B/deps" \
		--with-libgmp-prefix="$B/deps" --with-mpfr="$B/deps" \
		--with-libmpfr-prefix="$B/deps" --with-expat \
		--with-libexpat-prefix="$DEPS/usr" --disable-rpath \
		CFLAGS="$gnu17 -I$DEPS/usr/include" \
		CXXFLAGS="-g -O2 -I$DEPS/usr/include" LDFLAGS="-L$DEPS/usr/lib64"
	mk "$B/gdb" all-gdb; mk "$B/gdb" DESTDIR="$ROOT" install-gdb
}

s_gcc() {
	[ -n "$commit" ] || { echo "no commit in '$pkg'"; return 1; }
	local src=$B/gcc-src p
	rm -rf "$src"; mkdir -p "$src"
	git -C "$R/toolchains/gcc-claude" archive "$commit" | tar -x -C "$src"
	for p in gmp-6.3.0:gmp mpfr-4.2.2:mpfr mpc-1.3.1:mpc; do
		cp -aL "$GMP/${p%%:*}" "$src/${p##*:}"
	done
	# --with-build-sysroot: the target's headers, or configure takes
	# glibc as 0.0 (tools/gcc-replay/README.md, "Pitfalls").
	conf "$src" "$B/gcc" --target=$T --with-build-sysroot="$S" \
		${tune:+--with-tune=$tune} --with-pkgversion="$pkg native" \
		--enable-languages=c,c++,lto --disable-multilib --disable-nls \
		--disable-libsanitizer --disable-libssp --disable-libitm \
		--enable-checking=yes --disable-werror --disable-bootstrap
	mk "$B/gcc"; mk "$B/gcc" DESTDIR="$ROOT" install-strip
}

s_kernel() {
	local K="make -C $R/toolchains/poc06/linux-src O=$B/kernel ARCH=e2k CROSS_COMPILE=$T- CC=$X/bin/$T-gcc"
	mkdir -p "$B/kernel"
	cp "$R/toolchains/poc06/linux-src/config-mcst-e8c2" "$B/kernel/.config"
	$K olddefconfig
	# zImage, not the uncompressed image.boot: the kernel's make install
	# and OS Elbrus boot the compressed image (its decompressor puts the
	# kernel in free, 4 MiB-aligned memory); the raw one hung after the
	# firmware's "Starting kernel..." on the first boot.
	$K -j"$jobs" zImage modules
	$K INSTALL_MOD_PATH="$ROOT" INSTALL_MOD_STRIP=1 modules_install
	cp "$B/kernel/zImage" "$out/zImage"
}

s_rootfs() {
	cp -a "$D/rootfs/." "$ROOT/"
	mkdir -p "$ROOT"/{proc,sys,dev,run,tmp,root/.ssh,etc/dropbear}
	chmod 1777 "$ROOT/tmp"; chmod 700 "$ROOT/root"
	cp "$R/secrets/id_ed25519.pub" "$ROOT/root/.ssh/authorized_keys"
	chmod 600 "$ROOT/root/.ssh/authorized_keys"
	printf '/usr/lib64\n/lib64\n' > "$ROOT/etc/ld.so.conf"
	# lcc puts a weak symbol elbrus_optimizing_compiler_vVERSION_DATE in
	# every object it compiles (and no .comment); look for it before the
	# strip below removes symbol tables.
	grep -rl 'elbrus_optimizing_compiler' "$ROOT" > "$B/lcc-marks" || true
	# Executables and shared libraries without their debug information
	# (gdb alone has 280 MB of it); objects and archives stay as built.
	local f
	find "$ROOT" -type f \( -perm -u+x -o -name '*.so*' \) ! -name '*.o' \
		! -name '*.a' ! -path '*/lib/modules/*' | while read -r f; do
		head -c 4 "$f" | grep -q 'ELF' || continue
		"$T-strip" --strip-unneeded "$f" 2>/dev/null || true
	done
}

s_initrd() {
	# Busybox, glibc's shared libraries and dropbear: what boots from
	# memory, so the system takes SSH before anything is mounted.
	local I=$out/initrd f
	rm -rf "$I"; mkdir -p "$I"
	cd "$ROOT"
	for f in bin sbin etc root proc sys dev run tmp \
		 usr/sbin/dropbear usr/bin/dropbearkey usr/bin/dbclient usr/bin/scp \
		 lib64/ld-linux*.so* lib64/lib*.so*; do
		[ -e "$f" ] || [ -L "$f" ] || continue
		cp -a --parents "$f" "$I/"
	done
	find usr/bin usr/sbin -maxdepth 1 -type l -lname '*busybox' \
		-exec cp -a --parents {} "$I/" \;
	(cd "$I" && find . | LC_ALL=C sort | cpio -o -H newc --owner=0:0 --quiet) |
		gzip -9 > "$out/initrd.img"
	ls -l "$out/initrd.img"
}

s_check() {
	local bad=0 f
	if [ -s "$B/lcc-marks" ]; then
		echo "files lcc compiled (elbrus_optimizing_compiler):"
		cat "$B/lcc-marks"; bad=1
	fi
	if grep -rl '/opt/mcst' "$ROOT" "$out/initrd" > "$B/mcst-paths"; then
		echo "files naming /opt/mcst:"; cat "$B/mcst-paths"; bad=1
	fi
	# Every ELF file's .comment strings, counted.
	find "$ROOT" -type f | while read -r f; do
		head -c 4 "$f" | grep -q 'ELF' || continue
		"$T-readelf" -p .comment "$f" 2>/dev/null |
			sed -n 's/^ *\[ *[0-9a-f]*\] *//p'
	done | sort | uniq -c | sort -rn > "$B/comments"
	cat "$B/comments"
	if grep -v -E 'GCC: \((relcc2 PoC|GNU)\) ' "$B/comments"; then
		echo "^ not the PoC's"; bad=1
	fi
	return $bad
}

s_image() { "$D/mkimage.sh" "$out"; }

steps="runtime busybox dropbear ncurses gmp expat libffi binutils gdb gcc kernel rootfs initrd check image"
case " $steps " in *" $stage "*) ;; *) echo "build.sh: no step $stage" >&2; exit 1 ;; esac
echo "build.sh: $pkg, tune ${tune:-none}, into $out"
go=
for step in $steps; do
	[ "$step" = "$stage" ] && go=1
	[ -n "$go" ] || continue
	[ -z "$only" ] || [ "$step" = "$stage" ] || continue
	case $skip in *" $step "*) continue ;; esac
	echo "$(date '+%F %T') $step"
	# A subshell whose status is not tested, so set -e works inside.
	set +e
	( set -e; "s_$step" ) > "$L/$step.log" 2>&1
	rc=$?
	set -e
	[ $rc -eq 0 ] || { echo "$step failed: $L/$step.log"; exit 1; }
done
echo "build.sh: done ($out)"
}; exit
