#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build and run the 32-bit mode probes (spec/abi/mode32.md) with lcc
# -mptr32: run32 (dynamic, with librun32.so), run32s (static), run32t
# (static, -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64), cxx32 (C++, dynamic
# and static), run64 (the same probe as a 64-bit program, static), and
# the mixing tests: each mode's object linked into the other mode's
# program (on the build host) and each mode's library dlopen()ed by the
# other mode's dlmix (dlmix32pie: the 32-bit one as a PIE), rt32
# (lcc's ASan, OpenMP and -pg runtimes in 32-bit mode), lat32 (ldgd*,
# stgd* and getpl timing, run under the shared timing lock; lat32.s and
# the -cases.h files come from gen_lat32.py), asan32 (why ASan stops at
# start on some kernels), atom32 (16-byte atomics; for v5 and later
# targets also atom32mix, v3 and inline code on one object through the
# system's libatomic, and atom32mixlock, the same with lcc's lock-based
# v3 libatomic.a), and dbg32 (gdb and core dumps: dbg32.sh with
# dbg32.gdb, dynamic and static).
#
# Usage: tools/abi/probes/mode32/run.sh [TARGET] [--remote [USER@]HOST]
#            [--remote-dir DIR] [--ssh-port N] [--ship-libs]
#   TARGET: lcc target directory suffix (default v3), see AGENTS.md.
#   Without --remote the 32-bit programs run under the community
#   qemu-e2k32 (MCST's qemu-e2k loads only ELFCLASS64). DIR (default
#   relcc2/mode32, relative to $HOME) is the remote working directory; it
#   is removed at the end. --ship-libs copies lcc's lib32 ld.so and
#   libraries and runs the dynamic programs through that ld.so, for hosts
#   without /lib32 (ALT Linux on the 4C).
set -e

root=$(cd "$(dirname "$0")/../../../.." && pwd)
src=$root/tools/abi/probes/mode32
target=v3
remote=
port=
ship=
rdir=relcc2/mode32
while [ $# -gt 0 ]; do
	case $1 in
	--remote) remote=$2; shift 2 ;;
	--remote-dir) rdir=$2; shift 2 ;;
	--ssh-port) port=$2; shift 2 ;;
	--ship-libs) ship=1; shift ;;
	*) target=$1; shift ;;
	esac
done
lcc_dir=/opt/mcst/lcc-1.29.16.e2k-$target.linux-6.1
lcc=$lcc_dir/bin/lcc
lxx=$lcc_dir/bin/l++
fs=$lcc_dir/fs
v3_dir=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1
out=$root/toolchains/mode32/$target
mkdir -p "$out"
cd "$out"

# Compile-only checks.
$lcc -mptr32 -c "$src/layout32.c" -o /dev/null
echo "layout32.c: compiles"

$lcc -mptr32 -O2 -fPIC -shared "$src/run32-lib.c" -o librun32.so
$lcc -mptr32 -O2 "$src/run32.c" "$src/mode32.s" -L. -lrun32 \
	-Wl,-rpath,'$ORIGIN' -lpthread -o run32
$lcc -mptr32 -O2 -static "$src/run32.c" "$src/run32-lib.c" "$src/mode32.s" \
	-lpthread -o run32s
$lcc -mptr32 -O2 -static -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 \
	"$src/run32.c" "$src/run32-lib.c" "$src/mode32.s" -lpthread -o run32t
$lcc -O2 -static "$src/run32.c" "$src/run32-lib.c" "$src/mode32.s" \
	-lpthread -o run64
$lxx -mptr32 -O2 "$src/cxx32.cc" -o cxx32
$lxx -mptr32 -O2 -static "$src/cxx32.cc" -o cxx32s
$lcc -mptr32 -O2 -fPIC -shared "$src/mixlib.c" -o libmix32.so
$lcc -O2 -fPIC -shared "$src/mixlib.c" -o libmix64.so
$lcc -mptr32 -O2 "$src/dlmix.c" -ldl -o dlmix32
$lcc -mptr32 -O2 -fPIE -pie "$src/dlmix.c" -ldl -o dlmix32pie
$lcc -O2 "$src/dlmix.c" -ldl -o dlmix64
$lcc -mptr32 -g -fsanitize=address -DRT_ASAN "$src/rt32.c" -o rt32-asan
$lcc -mptr32 -O2 -fopenmp "$src/rt32.c" -o rt32-omp
$lcc -mptr32 -O1 -pg "$src/rt32.c" -o rt32-pg
case $target in
v[5-9]*) lat_v5="-DLAT32_V5 $src/lat32-v5.s"; quad=1 ;;
*) lat_v5=; quad= ;;
esac
$lcc -mptr32 -O1 -static $lat_v5 "$src/lat32.c" "$src/lat32.s" -o lat32
$lcc -mptr32 -O1 "$src/asan32.c" -o asan32
$lcc -mptr32 -O1 -fsanitize=address "$src/asan32.c" -o asan32-asan
$lcc -mptr32 -O2 "$src/atom32.c" -latomic -lpthread -o atom32
mix=
if [ -n "$quad" ]; then
	$v3_dir/bin/lcc -mptr32 -O2 -DPART=1 -c "$src/atom32.c" -o atom32-1.o
	$lcc -mptr32 -O2 -DPART=2 -c "$src/atom32.c" -o atom32-2.o
	$lcc -mptr32 -O2 -DPART=0 "$src/atom32.c" atom32-1.o atom32-2.o \
		-latomic -lpthread -o atom32mix
	$lcc -mptr32 -O2 -static -DPART=0 "$src/atom32.c" atom32-1.o \
		atom32-2.o \
		$v3_dir/lcc-home/1.29.16/e2k-v3-linux.cross/lib32/libatomic.a \
		-lpthread -o atom32mixlock
	mix="atom32mix atom32mixlock"
fi
$lcc -mptr32 -O0 -g "$src/dbg32.c" -o dbg32
$lcc -mptr32 -O0 -g -static "$src/dbg32.c" -o dbg32s
cp "$src/dbg32.gdb" "$src/dbg32.sh" .

# Link-time mixing: expected to fail; print what ld says.
$lcc -mptr32 -O2 -c "$src/mixlib.c" -o mix32.o
$lcc -O2 -c "$src/mixlib.c" -o mix64.o
printf 'int mix_fn (int); int main (void) { return mix_fn (0) - 1; }\n' > mixmain.c
$lcc -mptr32 -O2 -c mixmain.c -o mixmain32.o
$lcc -O2 -c mixmain.c -o mixmain64.o
echo "link 32-bit main with 64-bit object:"
$lcc -mptr32 mixmain32.o mix64.o -o mixa 2>&1 | sed 's/^/  /' || true
echo "link 64-bit main with 32-bit object:"
$lcc mixmain64.o mix32.o -o mixb 2>&1 | sed 's/^/  /' || true

progs="run32 run32s run32t run64 librun32.so cxx32 cxx32s dlmix32 dlmix32pie dlmix64 libmix32.so libmix64.so rt32-asan rt32-omp rt32-pg lat32 asan32 asan32-asan atom32 $mix dbg32 dbg32s dbg32.gdb dbg32.sh"
libs=
if [ -n "$ship" ]; then
	for l in ld-linux.so.2 libc.so.6 libm.so.6 libdl.so.2 libpthread.so.0; do
		cp "$fs/lib32/$l" "$out/$l"
	done
	for l in libstdc++.so.6 libgcc_s.so.1 libomp.so.1 libatomic.so.1; do
		cp -L "$fs/usr/lib32/$l" "$out/$l"
	done
	libs="ld-linux.so.2 libc.so.6 libm.so.6 libdl.so.2 libpthread.so.0 libstdc++.so.6 libgcc_s.so.1 libomp.so.1 libatomic.so.1"
	dyn="./ld-linux.so.2 --library-path ."
else
	dyn=
fi

script="cd $rdir || exit 1
echo '== run32 (dynamic)'; $dyn ./run32 maps
echo '== run32s (static)'; ./run32s
echo '== run32t (static, _TIME_BITS=64)'; ./run32t t64
echo '== run64 (static, 64-bit)'; ./run64
echo '== cxx32'; $dyn ./cxx32; ./cxx32s
echo '== dlmix'; $dyn ./dlmix32 ./libmix64.so ./libmix32.so; ./dlmix64 ./libmix32.so ./libmix64.so
echo '== dlmix32pie'; $dyn ./dlmix32pie ./libmix32.so; $dyn ./dlmix32pie ./libmix32.so
echo '== rt32'; $dyn ./rt32-asan 2>&1 | head -4; $dyn ./rt32-omp
$dyn ./rt32-pg && { gprof -b -p rt32-pg gmon.out 2>&1 | head -6; }
echo '== lat32 (under the timing lock)'
flock -w 3600 /tmp/relcc2-claude-timing.lock ./lat32
echo '== asan32'; $dyn ./asan32 | grep -v '^[0-9a-f]\{8\}-[0-9a-f]\{8\} '
$dyn ./asan32-asan 2>&1 | grep -v '^[0-9a-f]\{8\}-[0-9a-f]\{8\} '
echo '== atom32'; $dyn ./atom32
if [ -n '$mix' ]; then
	echo '-- atom32mix'; ./atom32mix
	echo '-- atom32mixlock'; ./atom32mixlock
fi
echo '== dbg32'; DBG32_LOADER='$dyn' sh dbg32.sh dbg32; sh dbg32.sh dbg32s
echo '== file'; file -L run32 run32s 2>/dev/null || true"

if [ -n "$remote" ]; then
	# relcc2's key and nothing else: tools/setup/e2k-ssh and e2k-scp
	# refuse to run without it. The port goes to each call directly.
	rssh () {
		if [ -n "$port" ]; then
			"$root/tools/setup/e2k-ssh" -p "$port" "$@"
		else
			"$root/tools/setup/e2k-ssh" "$@"
		fi
	}
	rscp () {
		if [ -n "$port" ]; then
			"$root/tools/setup/e2k-scp" -P "$port" "$@"
		else
			"$root/tools/setup/e2k-scp" "$@"
		fi
	}
	rssh "$remote" "mkdir -p $rdir"
	rscp -q $progs $libs "$remote:$rdir/"
	rssh "$remote" "$script" || true
	rssh "$remote" "rm -rf $rdir"
else
	q=$root/toolchains/qemu-community/bin/qemu-e2k32
	isa=$(echo "$target" | sed 's/^v\([0-9]*\).*/\1/')
	# host paths: with a RUNPATH, ld.so otherwise falls back to the
	# host's /etc/ld.so.cache through qemu's -L redirection
	run="$q -cpu elbrus-v$isa -L $fs -E LD_LIBRARY_PATH=$fs/lib32:$fs/usr/lib32"
	for p in "run32 qemu" "run32s qemu" cxx32 cxx32s "run32t t64" dlmix32pie; do
		echo "== $p (qemu-e2k32)"
		timeout 60 $run ./$p || echo "exit status $?"
	done
	echo "== run32s (MCST qemu-e2k)"
	timeout 60 "$root/toolchains/qemu-mcst/bin/qemu-e2k" -cpu "elbrus-v$isa" \
		./run32s 2>&1 || echo "exit status $?"
fi
