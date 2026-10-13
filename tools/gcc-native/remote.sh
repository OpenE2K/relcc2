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
#   bash remote.sh status [NAME]
#   bash remote.sh clean NAME | --all
#
# The builds run under nice -n 19 and take no lock (they last hours);
# bench takes the machines' shared timing lock and pins one CPU.

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
    local src=$D/src-$name b=$D/build-$name inst=$D/inst-$name log=$D/log-$name
    [ -f "$src/configure" ] || die "no $src: push it first"
    mkdir -p "$log"
    exec 9> "$log/lock"
    flock -n 9 || die "$name is busy (another build or test of it is running)"
    echo $$ > "$log/pid"
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

[ $# -ge 1 ] || die "usage: remote.sh check|build|test|bench-inputs|bench|status|clean ..."
cmd=$1
shift
case $cmd in
check)        cmd_check "$@" ;;
build)        cmd_build "$@" ;;
test)         cmd_test "$@" ;;
bench-inputs) cmd_bench_inputs "$@" ;;
bench)        cmd_bench "$@" ;;
status)       cmd_status "$@" ;;
clean)        cmd_clean "$@" ;;
*)            die "unknown command $cmd" ;;
esac
