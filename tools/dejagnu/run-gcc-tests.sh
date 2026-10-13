#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Run GCC's testsuite for an E2K cross compiler with relcc2's DejaGnu
# boards: under qemu-e2k or on an Elbrus machine over SSH.
#
# Usage:
#   tools/dejagnu/run-gcc-tests.sh -b BUILD [options] [RUNTESTFLAGS...]
#   tools/dejagnu/run-gcc-tests.sh -i GCC [options] [RUNTESTFLAGS...]
#
#   -b BUILD   a GCC build tree configured with --target=e2k-linux-gnu:
#              runs "make -k check-gcc" (or check-g++) in BUILD/gcc; the
#              results are in BUILD/gcc/testsuite/gcc/gcc.sum (g++/g++.sum)
#   -i GCC     test an installed compiler instead (the path of
#              e2k-linux-gnu-gcc) with runtest and the testsuite of -s
#   -s SRC     GCC source tree for -i (default toolchains/gcc)
#   -o OUT     -i only: directory for the .sum and .log files
#              (default toolchains/dejagnu/gcc-BOARD)
#   -t BOARD   e2k-qemu (default), e2k-4c, e2k-8c2, e2k-2c3 or e2k-ssh,
#              with DejaGnu variants if wanted: 'e2k-qemu/-O0' or
#              'e2k-4c{-O0,-O2}'; settings in tools/dejagnu/README.md
#   -l LANG    c (check-gcc, default) or c++ (check-g++)
#   -j N       parallel runtest processes (default 1; at most 4 on
#              e2k-4c, e2k-8c2 and e2k-ssh, 2 on e2k-2c3); with -i they
#              share the tests as GCC's "make -jN check" does, each in
#              OUT/TOOLn, and the results are merged into OUT
#   -B BINDIR  MCST binutils, named e2k-linux-* (default: $E2K_LCC's
#              binutils/bin, else lcc v3's, else toolchains/binutils/bin)
#
#   RUNTESTFLAGS  which tests, e.g. execute.exp, 'execute.exp=2000*',
#              'dg.exp=pr1*.c', or runtest options such as -v
#
# Examples:
#   run-gcc-tests.sh -b ~/build-gcc-e2k execute.exp
#   E2K_CPU=elbrus-v5 run-gcc-tests.sh -b ~/build-gcc-e2k -j 8 execute.exp
#   run-gcc-tests.sh -b ~/build-gcc-e2k -t e2k-4c -j 4 'execute.exp=pr1*'
#   run-gcc-tests.sh -i ~/gcc-e2k-install/bin/e2k-linux-gnu-gcc dg.exp
#
# GCC's testsuite finds objdump, nm, readelf and friends as TARGET-objdump
# (TARGET = the configured target, e.g. e2k-linux-gnu) in PATH; without
# them the weak, alias and visibility tests fail. MCST's binutils are
# named e2k-linux-*, so this script puts TARGET-* links to BINDIR in
# toolchains/dejagnu/bin-TARGET and that directory first in PATH.

set -e
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)

build= gcc= src="$repo/toolchains/gcc" out= board=e2k-qemu lang=c jobs=1
bindir=
while getopts b:i:s:o:t:l:j:B:h opt; do
    case $opt in
    b) build=$OPTARG ;;
    i) gcc=$OPTARG ;;
    s) src=$OPTARG ;;
    o) out=$OPTARG ;;
    t) board=$OPTARG ;;
    l) lang=$OPTARG ;;
    j) jobs=$OPTARG ;;
    B) bindir=$OPTARG ;;
    *) sed -n '4,/^$/p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
    esac
done
shift $((OPTIND - 1))

die() { echo "run-gcc-tests.sh: $*" >&2; exit 1; }

if [ -n "$build" ] && [ -n "$gcc" ] || [ -z "$build$gcc" ]; then
    die "give one of -b BUILD and -i GCC (-h for help)"
fi
case $lang in
c) check=check-gcc tool=gcc ;;
c++) check=check-g++ tool=g++ ;;
*) die "-l c or -l c++" ;;
esac
case $jobs in
''|*[!0-9]*|0) die "-j takes a number of jobs" ;;
esac

# Shared machines: keep the load low (AGENTS.md, "Hardware").
case $board in
e2k-2c3*) max=2 ;;
e2k-4c*|e2k-8c2*|e2k-ssh*) max=4 ;;
*) max=$jobs ;;
esac
if [ "$jobs" -gt "$max" ]; then
    echo "run-gcc-tests.sh: $board is a shared machine; -j $max instead of $jobs" >&2
    jobs=$max
fi

# The target name GCC's testsuite prefixes binutils with.
if [ -n "$build" ]; then
    build=$(cd "$build" && pwd)
    [ -f "$build/gcc/Makefile" ] || die "$build/gcc/Makefile: not a configured GCC build tree"
    alias=$(sed -n 's/^target_noncanonical *:*= *//p' "$build/gcc/Makefile" | head -n 1)
else
    [ -x "$gcc" ] || die "$gcc: not executable"
    gcc=$(cd "$(dirname "$gcc")" && pwd)/$(basename "$gcc")
    alias=$("$gcc" -dumpmachine)
fi
[ -n "$alias" ] || die "cannot tell the compiler's target"

if [ -z "$bindir" ]; then
    for d in ${E2K_LCC:+"$E2K_LCC/binutils/bin"} \
             /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/binutils/bin \
             "$repo/toolchains/binutils/bin"; do
        if [ -x "$d/e2k-linux-objdump" ]; then
            bindir=$d
            break
        fi
    done
fi
[ -n "$bindir" ] && [ -x "$bindir/e2k-linux-objdump" ] ||
    die "no MCST binutils (e2k-linux-objdump) found; give -B BINDIR"
bindir=$(cd "$bindir" && pwd)
links="$repo/toolchains/dejagnu/bin-$alias"
mkdir -p "$links"
# Replace a link only when it points elsewhere (another -B), so that runs
# sharing the directory never see it missing.
for t in addr2line ar as c++filt elfedit ld nm objcopy objdump ranlib \
         readelf size strings strip; do
    if [ -x "$bindir/e2k-linux-$t" ] &&
       [ "$(readlink "$links/$alias-$t" 2>/dev/null)" != "$bindir/e2k-linux-$t" ]; then
        ln -sf "$bindir/e2k-linux-$t" "$links/$alias-$t"
    fi
done
PATH="$links:$PATH"
DEJAGNU="$here/site.exp"
export PATH DEJAGNU

if [ -n "$build" ]; then
    echo "make -C $build/gcc -k -j$jobs $check RUNTESTFLAGS=\"--target_board=$board $*\""
    make -C "$build/gcc" -k -j"$jobs" "$check" \
        RUNTESTFLAGS="--target_board=$board $*" || true
    sum="$build/gcc/testsuite/$tool/$tool.sum"
else
    [ -f "$src/gcc/testsuite/lib/gcc.exp" ] || die "$src: not a GCC source tree"
    src=$(cd "$src" && pwd)
    [ -n "$out" ] || out="$repo/toolchains/dejagnu/gcc-$(echo "$board" | tr '/{},' '____')"
    mkdir -p "$out"
    out=$(cd "$out" && pwd)
    gxx=$(echo "$gcc" | sed 's/gcc\([^/]*\)$/g++\1/')
    guess=$("$src/config.guess")
    triplet=$("$src/config.sub" "$alias")
    # What GCC's Makefile writes into BUILD/gcc/site.exp, for a compiler
    # outside a build tree, in directory $1.
    site() {
        cat > "$1/site.exp" <<EOF
set rootme "$1"
set srcdir "$src/gcc/testsuite"
set host_triplet $guess
set build_triplet $guess
set target_triplet $triplet
set target_alias $alias
set tmpdir "$1"
set CFLAGS ""
set CXXFLAGS ""
set HOSTCC "cc"
set HOSTCFLAGS ""
set TEST_ALWAYS_FLAGS ""
set GCC_UNDER_TEST "$gcc"
set GXX_UNDER_TEST "$gxx"
EOF
    }
    sum="$out/$tool.sum"
    if [ "$jobs" -eq 1 ]; then
        site "$out"
        echo "cd $out && runtest --tool $tool --target_board=$board $*"
        (cd "$out" && runtest --tool "$tool" --target_board="$board" "$@") || true
    else
        # GCC's parallel check (gcc/Makefile.in, check-parallel-%): every
        # runtest process walks all the tests and runs the batches it
        # claims first in GCC_RUNTEST_PARALLELIZE_DIR (lib/gcc-defs.exp);
        # contrib/dg-extract-results.sh merges the results.
        GCC_RUNTEST_PARALLELIZE_DIR="$out/$tool-parallel"
        export GCC_RUNTEST_PARALLELIZE_DIR
        rm -rf "$GCC_RUNTEST_PARALLELIZE_DIR" "$sum" "$out/$tool.log"
        mkdir -p "$GCC_RUNTEST_PARALLELIZE_DIR"
        echo "$jobs x (cd $out/$tool<n> && runtest --tool $tool --target_board=$board $*)"
        i=1
        while [ "$i" -le "$jobs" ]; do
            d="$out/$tool$i"
            rm -rf "$d"
            mkdir -p "$d"
            site "$d"
            (cd "$d" && runtest --tool "$tool" --target_board="$board" "$@" \
                 > runtest.out 2>&1) &
            i=$((i + 1))
        done
        wait
        sums= logs=
        i=1
        while [ "$i" -le "$jobs" ]; do
            if [ -f "$out/$tool$i/$tool.sum" ]; then
                sums="$sums $out/$tool$i/$tool.sum"
                logs="$logs $out/$tool$i/$tool.log"
            else
                echo "run-gcc-tests.sh: runtest $i gave no results" \
                     "(see $out/$tool$i/runtest.out)" >&2
            fi
            i=$((i + 1))
        done
        if [ -n "$sums" ]; then
            # Word splitting wanted: lists of paths.
            sh "$src/contrib/dg-extract-results.sh" $sums > "$sum"
            sh "$src/contrib/dg-extract-results.sh" -L $logs > "$out/$tool.log"
        fi
        rm -rf "$GCC_RUNTEST_PARALLELIZE_DIR"
    fi
fi

[ -f "$sum" ] || die "no $sum"
echo
sed -n '/=== .* Summary/,$p' "$sum"
echo "results: $sum (${sum%.sum}.log)"
