#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The Elbrus side of tools/gcc-native/gcc-native.sh: build, test and time
# a GCC natively on an E2K machine.  gcc-native.sh copies this file into
# its work directory on the machine (default ~/relcc2/gcc-native) and runs
# it there, detached for the long commands.  Everything lives in that
# directory:
#
#   src-NAME/     the source tree (gcc-native.sh push)
#   build-NAME/   the object directory
#   inst-NAME/    the installed compiler
#   log-NAME/     configure.log, make.log, install.log, check.log and the
#                 .sum files; status (running, ok, or failed: STEP); times
#                 (when each step started; when each stage's cc1plus was
#                 linked)
#   bench/        inputs/*.ii, the preprocessed compiler sources the bench
#                 command compiles, and results-*.txt
#   dejagnu/      a copy of DejaGnu if the machine has none
#                 (gcc-native.sh push-dejagnu)
#
#   bash remote.sh check
#   bash remote.sh build NAME MODE JOBS CHECKING STAGE1-FLAGS STOP [CONFIGURE-ARGS...]
#       MODE bootstrap   make bootstrap: stage 1 built by lcc, stages 2
#                        and 3 by the stage before, stage 2 and 3 objects
#                        compared
#            lcc         one stage built by lcc (the quickest check)
#            with:OTHER  one stage built by the installed compiler of the
#                        build OTHER (a revision compiled by another)
#       STOP  all, configure (stop after configure) or stage1 (bootstrap:
#             stop after stage 1)
#   bash remote.sh test NAME JOBS [RUNTESTFLAGS]
#   bash remote.sh bench-inputs NAME
#   bash remote.sh bench CPU REPEAT FLAGS SPEC...
#       SPEC  NAME (its last stage), NAME:stage1 (the lcc-built stage 1 of
#             a bootstrap), NAME:stage2, or NAME:inst (the installed one)
#   bash remote.sh pack NAME VERSION COMMIT OS
#       a stripped copy of build NAME's install as gcc-e2k-VERSION, with
#       a README, packed as gcc-e2k-VERSION-e2k-CPU-native-OS.tar.xz
#   bash remote.sh diffcc SPEC-A SPEC-B JOBS [FLAGS...]
#       compile GCC's C torture tests (and with two installed compilers
#       the C++ ones) with both compilers at each FLAGS (default -O0,
#       -O2, -O3, "-O2 -march=elbrus-v5") and compare the assembly
#       (diffcc.py); SPEC as for bench, NAME:inst for C++ too
#   bash remote.sh triage REF SUT JOBS
#       recompile every test that failed in build SUT's testsuite with
#       the installed REF and SUT compilers and compare (diffcc.py)
#   bash remote.sh status [NAME]
#   bash remote.sh clean NAME | --all
#
# A build's source is src-NAME, or src-$SRCNAME when SRCNAME is set (so
# that builds of one revision share one tree and its paths).
#
# The builds run under nice -n 19 and take no lock (they last hours);
# bench takes the machines' shared timing lock and pins one CPU.

# One compound command, read in full before it runs (see gcc-native.sh).
{
set -u
D=$(cd "$(dirname "$0")" && pwd)
cd "$D" || exit 1
TIMING_LOCK=/tmp/relcc2-claude-timing.lock     # the shared timing lock

die() { echo "remote.sh: $*" >&2; exit 1; }
stamp() { date '+%F %T'; }

# The configure options every build gets, besides --prefix: the languages
# and libraries of the PoC's own cross build (toolchains/build-gcc-claude*
# config.log), without libquadmath: glibc's e2k <bits/fenv.h> fails under
# C23 (spec/abi/source-compat.md, "Headers"), and the machines'
# /usr/include has that header (the PoC's cross sysroot patches it).
# shellcheck disable=SC2054  # the commas belong to the option
COMMON=(--enable-languages=c,c++,lto --disable-multilib --disable-nls
        --disable-libsanitizer --disable-libssp --disable-libitm
        --disable-libquadmath --disable-werror
        --with-as=/usr/bin/as --with-ld=/usr/bin/ld)

cmd_check() {
    echo "host $(hostname), kernel $(uname -r), $(nproc) CPUs," \
         "$(free -g | awk '/^Mem/ {print $2}') GiB, max processes $(ulimit -u)"
    df -h "$D" | tail -1
    local t h
    for t in lcc l++ as ld make flex bison makeinfo perl python3 gawk m4 \
             zstd tar expect runtest; do
        printf '  %-9s %s\n' "$t" "$(command -v "$t" || echo MISSING)"
    done
    [ -x "$D/dejagnu/bin/runtest" ] && echo "  runtest   $D/dejagnu/bin/runtest (pushed)"
    echo "  $(lcc --version 2>&1 | head -1); $(as --version | head -1)"
    for h in gmp.h mpfr.h mpc.h; do
        if [ -f "/usr/include/$h" ]; then
            printf '  %-9s ok\n' "$h"
        else
            printf '  %-9s MISSING: push with --prereqs\n' "$h"
        fi
    done
    if grep -q '^#if __GLIBC_USE (IEC_60559_BFP_EXT)$' /usr/include/bits/fenv.h 2>/dev/null; then
        echo "  bits/fenv.h has the C23 femode_t bug (libquadmath stays disabled)"
    fi
    ls -d "$D"/src-* "$D"/build-* "$D"/inst-* 2>/dev/null | sed "s|^$D/|  have |"
}

cmd_build() {
    [ $# -ge 6 ] || die "build NAME MODE JOBS CHECKING STAGE1-FLAGS STOP [CONFIGURE-ARGS...]"
    local name=$1 mode=$2 jobs=$3 checking=$4 s1=$5 stop=$6
    shift 6
    local src=$D/src-${SRCNAME:-$name} b=$D/build-$name inst=$D/inst-$name log=$D/log-$name
    [ -f "$src/configure" ] || die "no $src: push it first"
    mkdir -p "$log"
    exec 9> "$log/lock"
    flock -n 9 || die "$name is busy (another build or test of it is running)"
    echo $$ > "$log/pid"
    echo "$src" > "$log/srcdir"
    echo running > "$log/status"
    : > "$log/times"
    ulimit -c 0
    step() { echo "$(stamp) $*" >> "$log/times"; }
    fail() {
        local why=$1
        grep -q 'Bootstrap comparison failure' "$log/make.log" 2>/dev/null && why=compare
        echo "failed: $why" > "$log/status"
        step "failed: $why"
        exit 1
    }
    rm -rf "$b" "$inst"
    mkdir -p "$b"

    local cc cxx target extra=() vars=() makevars=()
    case $mode in
    bootstrap)
        cc=lcc cxx=l++ target=bootstrap
        extra=(--enable-bootstrap)
        [ "$stop" = stage1 ] && target=stage1-bubble
        makevars=("STAGE1_CFLAGS=$s1" "STAGE1_CXXFLAGS=$s1")
        ;;
    lcc)
        cc=lcc cxx=l++ target=all
        extra=(--disable-bootstrap)
        vars=("CFLAGS=$s1" "CXXFLAGS=$s1")
        ;;
    with:*)
        local other=$D/inst-${mode#with:}
        [ -x "$other/bin/gcc" ] || die "no compiler in $other"
        cc=$other/bin/gcc cxx=$other/bin/g++ target=all
        extra=(--disable-bootstrap)
        # The result must not need the other build's libstdc++ at run time.
        vars=("LDFLAGS=-static-libstdc++ -static-libgcc")
        ;;
    *) die "unknown mode $mode" ;;
    esac

    {
        echo "source: $(cat "$src/.relcc2-source" 2>/dev/null || echo unknown)"
        echo "mode $mode, jobs $jobs, checking $checking, stage-1 flags '$s1', stop $stop"
        echo "configure ${COMMON[*]} ${extra[*]} $* CC=$cc CXX=$cxx ${vars[*]}"
    } > "$log/settings"

    step configure
    (cd "$b" && nice -n 19 "$src/configure" --prefix="$inst" \
        --enable-checking="$checking" "${COMMON[@]}" "${extra[@]}" "$@" \
        CC="$cc" CXX="$cxx" "${vars[@]}") > "$log/configure.log" 2>&1 \
        || fail configure
    if [ "$stop" = configure ]; then
        step finished; echo "ok (configured)" > "$log/status"; return
    fi
    step "make $target"
    (cd "$b" && nice -n 19 make -j"$jobs" "$target" "${makevars[@]}") \
        > "$log/make.log" 2>&1 || fail make
    local d
    for d in stage1-gcc prev-gcc gcc; do
        [ -f "$b/$d/cc1plus" ] && \
            echo "$(date -r "$b/$d/cc1plus" '+%F %T') $d/cc1plus linked" >> "$log/times"
    done
    if [ "$target" = stage1-bubble ]; then
        step finished; echo "ok (stage 1)" > "$log/status"; return
    fi
    step install
    (cd "$b" && nice -n 19 make install) > "$log/install.log" 2>&1 || fail install
    step finished
    echo ok > "$log/status"
}

cmd_test() {
    [ $# -ge 2 ] || die "test NAME JOBS [RUNTESTFLAGS]"
    local name=$1 jobs=$2 rtf=${3-}
    local b=$D/build-$name log=$D/log-$name
    [ -f "$b/Makefile" ] || die "no build $name"
    if ! command -v runtest > /dev/null; then
        [ -x "$D/dejagnu/bin/runtest" ] || die "no runtest: gcc-native.sh push-dejagnu"
        PATH=$D/dejagnu/bin:$PATH
    fi
    command -v expect > /dev/null || die "no expect on this machine: DejaGnu needs it"
    exec 9> "$log/lock"
    flock -n 9 || die "$name is busy (another build or test of it is running)"
    echo "$(stamp) test ${rtf:-(all of check-gcc)}" >> "$log/times"
    ulimit -c 0
    (cd "$b" && nice -n 19 make -k -j"$jobs" check-gcc RUNTESTFLAGS="$rtf") \
        > "$log/check.log" 2>&1
    find "$b/gcc/testsuite" -maxdepth 2 -name '*.sum' -exec cp {} "$log/" \;
    echo "$(stamp) tested" >> "$log/times"
    grep -h '^# of \|^=== .* Summary' "$log"/*.sum
}

# Preprocess a fixed set of the compiler's own sources with build NAME's
# include paths (those GCC compiles itself with), so that every compiler
# bench times reads the same input.  Remade only by hand (rm -r bench/inputs).
cmd_bench_inputs() {
    [ $# -eq 1 ] || die "bench-inputs NAME"
    local name=$1 src=$D/src-$1 b=$D/build-$1
    local gxx="$b/gcc/xg++ -B$b/gcc/"
    [ -x "$b/gcc/xg++" ] || die "build $name has no xg++"
    mkdir -p "$D/bench/inputs"
    local inc="-I$b/gcc -I$src/gcc -I$src/gcc/../include -I$src/gcc/../libcpp/include
        -I$src/gcc/../libcody -I$src/gcc/../libdecnumber
        -I$src/gcc/../libdecnumber/dpd -I$b/libdecnumber -I$src/gcc/../libbacktrace
        -D_GNU_SOURCE -DIN_GCC -DHAVE_CONFIG_H"
    local f out
    for f in "$src/gcc/combine.cc" "$src/gcc/fold-const.cc" \
             "$src/gcc/tree-vect-stmts.cc" "$src/gcc/cp/parser.cc" \
             "$src/gcc/config/e2k/e2k-bundle.cc" "$b/gcc/insn-recog.cc"; do
        out=$D/bench/inputs/$(basename "${f%.cc}").ii
        [ -f "$f" ] || { echo "skip $f (missing)"; continue; }
        # shellcheck disable=SC2086
        (cd "$b/gcc" && $gxx $inc -fno-exceptions -fno-rtti -E "$f" -o "$out") \
            || die "cannot preprocess $f"
        echo "$(basename "$out"): $(wc -c < "$out") bytes"
    done
    echo "from $name: $(cat "$src/.relcc2-source" 2>/dev/null)" > "$D/bench/inputs/SOURCE"
}

cc1plus_of() {
    local name=${1%%:*} which=${1#*:}
    [ "$which" = "$1" ] && which=last
    case $which in
    stage1) echo "$D/build-$name/stage1-gcc/cc1plus" ;;
    stage2) echo "$D/build-$name/prev-gcc/cc1plus" ;;
    last)   echo "$D/build-$name/gcc/cc1plus" ;;
    inst)   ls "$D/inst-$name"/libexec/gcc/*/*/cc1plus 2>/dev/null | head -1 ;;
    *)      return 1 ;;
    esac
}

cmd_bench() {
    [ $# -ge 4 ] || die "bench CPU REPEAT FLAGS SPEC..."
    local cpu=$1 rep=$2 flags=$3
    shift 3
    ls "$D"/bench/inputs/*.ii > /dev/null 2>&1 || die "no inputs: bench-inputs NAME first"
    local specs=() s p
    for s in "$@"; do
        p=$(cc1plus_of "$s") && [ -x "$p" ] || die "no cc1plus for $s"
        specs+=("$s=$p")
    done
    local out
    out=$D/bench/results-$(date +%Y%m%d-%H%M%S).txt
    echo "waiting for $TIMING_LOCK"
    flock -w 3600 "$TIMING_LOCK" python3 - "$cpu" "$rep" "$flags" "$out" \
        "$D/bench/inputs" "${specs[@]}" <<'EOF'
import glob, hashlib, os, statistics, subprocess, sys
cpu, rep, flags, out, inputs = sys.argv[1:6]
specs = [s.split("=", 1) for s in sys.argv[6:]]
rows, total = [], {}
for ii in sorted(glob.glob(inputs + "/*.ii")):
    for label, cc1 in specs:
        times, rss, sums = [], 0, set()
        for _ in range(int(rep)):
            s = out + ".s"
            p = subprocess.Popen(["taskset", "-c", cpu, cc1, "-quiet"]
                                 + flags.split() + [ii, "-o", s])
            _, status, ru = os.wait4(p.pid, 0)
            if status:
                sys.exit(f"{label}: {cc1} failed on {ii}")
            times.append(ru.ru_utime)
            rss = max(rss, ru.ru_maxrss)
            sums.add(hashlib.md5(open(s, "rb").read()).hexdigest()[:12])
            os.unlink(s)
        med = statistics.median(times)
        total[label] = total.get(label, 0) + med
        rows.append(f"{label:24} {os.path.basename(ii):24} {med:9.2f} s "
                    f"(min {min(times):.2f}, max {max(times):.2f}) "
                    f"{rss // 1024:6} MiB  {'/'.join(sorted(sums))}")
rows.append("")
base = specs[0][0]
for label, _ in specs:
    rows.append(f"total {label:24} {total[label]:9.2f} s user"
                f"  ({total[label] / total[base]:.3f} x {base})")
rows.append(f"flags: {flags}; CPU {cpu}; median of {rep}")
for label, cc1 in specs:
    rows.append(f"{label}: {cc1}")
text = "\n".join(rows) + "\n"
open(out, "w").write(text)
print(text, end="")
print("written", out)
EOF
}

# The driver of a build, for C (or with c++ as $2, C++): NAME is its last
# stage, NAME:stage1 and NAME:stage2 the earlier stages of a bootstrap
# (C only: their libstdc++ isn't installed anywhere), NAME:inst the
# installed compiler.
driver_of() {
    local spec=$1 lang=${2:-c} name=${1%%:*} which=${1#*:} d
    [ "$which" = "$spec" ] && which=last
    case $which in
    inst)
        d=$D/inst-$name/bin
        [ "$lang" = c++ ] && echo "$d/g++" || echo "$d/gcc"
        return ;;
    stage1) d=$D/build-$name/stage1-gcc ;;
    stage2) d=$D/build-$name/prev-gcc ;;
    last)   d=$D/build-$name/gcc ;;
    *)      return 1 ;;
    esac
    [ "$lang" = c ] || return 1
    echo "$d/xgcc -B$d/"
}

# The source tree a build was made from.
srcdir_of() {
    local name=${1%%:*}
    cat "$D/log-$name/srcdir" 2>/dev/null || echo "$D/src-$name"
}

cmd_diffcc() {
    [ $# -ge 3 ] || die "diffcc SPEC-A SPEC-B JOBS [FLAGS...]"
    local a=$1 b=$2 jobs=$3 ca cb xa xb ts out f
    shift 3
    ca=$(driver_of "$a") && cb=$(driver_of "$b") || die "bad compiler spec"
    [ -x "${ca%% *}" ] || die "no compiler $ca"
    [ -x "${cb%% *}" ] || die "no compiler $cb"
    xa=$(driver_of "$a" c++) || xa=
    xb=$(driver_of "$b" c++) || xb=
    ts=$(srcdir_of "$a")/gcc/testsuite
    out=$D/bench/diffcc-${a//:/-}-${b//:/-}-$(date +%Y%m%d-%H%M%S)
    [ $# -gt 0 ] || set -- -O0 -O2 -O3 "-O2 -march=elbrus-v5"
    local args=(compare --a "$ca" --b "$cb" --out "$out" --jobs "$jobs")
    # "--flags=X": argparse would take a bare "-O2" for an option.
    for f in "$@"; do args+=("--flags=$f"); done
    if [ -n "$xa" ] && [ -n "$xb" ]; then
        args+=(--a-cxx "$xa" --b-cxx "$xb" --cxx-flags=-O0 --cxx-flags=-O2)
    fi
    echo "$(stamp) diffcc $a $b -> $out"
    python3 "$D/diffcc.py" "${args[@]}" \
        "$ts/gcc.c-torture/compile" "$ts/gcc.c-torture/execute" \
        "$ts/gcc.dg/torture" "$ts/g++.dg/torture"
}

cmd_triage() {
    [ $# -eq 3 ] || die "triage REF SUT JOBS"
    local ref=$1 sut=$2 jobs=$3 out
    local sums=("$D/log-$sut"/*.sum)
    [ -f "${sums[0]}" ] || die "no .sum files in log-$sut: test it first"
    out=$D/log-$sut/triage-$ref
    rm -rf "$out"
    python3 "$D/diffcc.py" triage --jobs "$jobs" --out "$out" \
        --testsuite "$(srcdir_of "$sut")/gcc/testsuite" \
        --a "$(driver_of "$ref:inst")" --b "$(driver_of "$sut:inst")" \
        --a-cxx "$(driver_of "$ref:inst" c++)" --b-cxx "$(driver_of "$sut:inst" c++)" \
        "${sums[@]}"
}

cmd_status() {
    local log name st
    for log in "$D"/log-${1:-*}; do
        [ -d "$log" ] || continue
        name=${log#"$D"/log-}
        st=$(cat "$log/status" 2>/dev/null || echo "?")
        if [ "$st" = running ] && ! kill -0 "$(cat "$log/pid" 2>/dev/null)" 2>/dev/null; then
            st="died (no process)"
        fi
        echo "== $name: $st"
        tail -2 "$log/times" 2>/dev/null | sed 's/^/   /'
        [ -f "$D/build-$name/stage_current" ] && \
            echo "   now in $(cat "$D/build-$name/stage_current")"
        if [ "$st" != ok ] && [ -f "$log/make.log" ]; then
            tail -3 "$log/make.log" | cut -c1-200 | sed 's/^/   | /'
        fi
    done
    du -sh "$D" 2>/dev/null | sed 's/^/disk: /'
}

cmd_pack() {
    [ $# -eq 4 ] || die "pack NAME VERSION COMMIT OS"
    local name=$1 version=$2 commit=$3 os=$4
    local b=$D/build-$1 log=$D/log-$1 stage=$D/pack-$1 top=gcc-e2k-$2 cpu tarball
    [ "$(cat "$log/status" 2>/dev/null)" = ok ] || die "build $name has not finished"
    rm -rf "$stage"
    mkdir -p "$stage"
    # A stripped copy of the install, moved to a directory of its own: GCC
    # finds its programs and libraries relative to bin/, wherever it lies.
    (cd "$b" && nice -n 19 make install-strip DESTDIR="$stage/root") \
        > "$log/pack.log" 2>&1 || die "make install-strip failed: $log/pack.log"
    mv "$stage/root$D/inst-$name" "$stage/$top"
    rm -rf "$stage/root"
    # The CPU the binaries are marked for: ld intersects its inputs', so
    # the machine's system libraries decide (elf.md, "Mixing targets").
    cpu=$(readelf -h "$stage/$top/bin/gcc" | sed -n 's/^ *Flags: *[^,]*, *\([^,]*\),.*/\1/p')
    cpu=${cpu:-unknown}
    cat > "$stage/$top/README.txt" <<README
GCC for Elbrus (E2K), relcc2 proof of concept $version, native

An experimental GCC back end for E2K written by an AI agent from the
relcc2 specification of the ISA and ABI.  It is not the hand-written port
meant for upstream and is not supported by MCST or the GCC project.

  GCC         relcc2 PoC $version, commit $commit
  host        e2k, binaries marked $cpu: they run on that CPU only,
              because they link with the build machine's system libraries
  system      $os ($(ldd --version | sed -n '1s/.*) //p')), built on
              $(hostname) with its /usr/bin/as and /usr/bin/ld
  built by    a GCC bootstrap: stage 1 by lcc, stages 2 and 3 by the PoC
              itself, stage 2 and 3 objects compared equal
  target      e2k-unknown-linux-gnu, 64-bit unprotected mode, default
              -march=elbrus-v3 (programs link with the system libraries,
              which mark them for this machine's CPU too)
  languages   C, C++, LTO; --enable-checking=yes (slower than release)
  left out    libsanitizer, libitm, libssp, libquadmath, Fortran

Using it: unpack anywhere and put bin/ in PATH.  It uses the system's
binutils and glibc.  C++ programs need this compiler's libstdc++, newer
than the system's: link with -static-libstdc++ -static-libgcc, or point
LD_LIBRARY_PATH (or -Wl,-rpath) at lib64/.

Configured with:
$(sed -n 's/^configure //p' "$log/settings")

Licence: GCC is GPL-3.0-or-later; libgcc and libstdc++ carry the GCC
Runtime Library Exception.  Source: the commit above.
README
    tarball=$D/$top-e2k-$cpu-native-$os.tar.xz
    tar -C "$stage" --owner=0 --group=0 --numeric-owner --sort=name -cf - "$top" \
        | xz -T4 -6 > "$tarball"
    rm -rf "$stage"
    ls -l "$tarball"
}

cmd_clean() {
    [ $# -eq 1 ] || die "clean NAME | --all"
    if [ "$1" = --all ]; then
        for log in "$D"/log-*; do
            [ "$(cat "$log/status" 2>/dev/null)" = running ] && \
                kill -0 "$(cat "$log/pid")" 2>/dev/null && die "${log#"$D"/} is running"
        done
        cd / && rm -rf "$D"
        echo "removed $D"
        return
    fi
    local log=$D/log-$1
    [ "$(cat "$log/status" 2>/dev/null)" = running ] && \
        kill -0 "$(cat "$log/pid")" 2>/dev/null && die "$1 is running"
    rm -rf "$D/src-$1" "$D/build-$1" "$D/inst-$1" "$log"
    echo "removed $1"
}

[ $# -ge 1 ] || die "usage: remote.sh check|build|test|bench-inputs|bench|diffcc|triage|status|pack|clean ..."
cmd=$1
shift
case $cmd in
check)        cmd_check "$@" ;;
build)        cmd_build "$@" ;;
test)         cmd_test "$@" ;;
bench-inputs) cmd_bench_inputs "$@" ;;
bench)        cmd_bench "$@" ;;
diffcc)       cmd_diffcc "$@" ;;
triage)       cmd_triage "$@" ;;
status)       cmd_status "$@" ;;
pack)         cmd_pack "$@" ;;
clean)        cmd_clean "$@" ;;
*)            die "unknown command $cmd" ;;
esac
}
exit
