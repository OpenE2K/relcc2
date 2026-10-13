#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build a GCC for E2K natively on an Elbrus machine, rebuild it with
# itself, test it there and time it as a program: drives
# tools/gcc-native/remote.sh over relcc2's SSH wrapper.  Any GCC source
# tree with an e2k-*-linux-gnu port works; the default is the PoC's
# worktree toolchains/gcc-claude-native (README.md in this directory).
#
# Usage: tools/gcc-native/gcc-native.sh [OPTIONS] COMMAND [ARGS...]
#
# Options (before the command):
#   --remote USER@HOST  the machine (default claude@geofront.lan, the 8C2)
#   --ssh-port N        its SSH port
#   --remote-dir DIR    work directory there, relative to $HOME
#                       (default relcc2/gcc-native)
#   --src DIR           GCC git checkout (default toolchains/gcc-claude-native)
#   --rev REV           commit to push (default: --src's HEAD)
#   --name NAME         name of the build (default: the commit's short hash)
#   -j N                parallel jobs (default 4: half the 8C2)
#
# Commands:
#   check               what the machine has for a native build
#   push [--prereqs]    export the commit (git archive, so committed files
#                       only) to src-NAME; --prereqs adds GCC's gmp, mpfr,
#                       mpc and isl (contrib/download_prerequisites, run
#                       here once) for machines without their headers
#   push-dejagnu        copy this host's DejaGnu (runtest and its library)
#                       for machines that have expect but no runtest
#   bootstrap [BUILD-OPTIONS] [-- CONFIGURE-ARGS]
#                       make bootstrap: stage 1 by lcc, then by itself
#   lccbuild [BUILD-OPTIONS] [-- CONFIGURE-ARGS]
#                       one stage built by lcc only
#   selfbuild --with OTHER [BUILD-OPTIONS] [-- CONFIGURE-ARGS]
#                       one stage built by build OTHER's installed compiler
#       BUILD-OPTIONS: --checking LIST (default yes; release for timing
#       the compiler), --stage1-flags FLAGS (lcc's flags, default -O3),
#       --stop-after configure|stage1, --source NAME (build from
#       src-NAME, e.g. a second build of a pushed revision under
#       another --name: the same tree keeps the paths in the objects)
#   status              every build on the machine: state, last step
#   wait [MINUTES]      print status every MINUTES (default 10) until NAME
#                       is no longer running
#   test [RUNTESTFLAGS] make -k check-gcc in build NAME (detached)
#   bench-inputs        preprocess the benchmark sources with build NAME
#   bench [--cpu N] [--repeat R] [--flags F] SPEC...
#                       time cc1plus builds on those inputs under the
#                       timing lock; SPEC is NAME, NAME:stage1, NAME:stage2
#                       or NAME:inst (detached; see status, fetch)
#   diffcc SPEC-A SPEC-B [FLAGS...]
#                       compile GCC's C (with two NAME:inst also C++)
#                       torture tests with both compilers and compare the
#                       assembly: two builds of one revision must agree,
#                       so a difference means one of them is miscompiled
#                       (detached; result in bench/diffcc-*)
#   triage REF          recompile build NAME's failing tests with the
#                       installed REF and NAME compilers and compare
#                       (detached; result in log-NAME/triage-REF)
#   log [FILE]          tail a log of build NAME (default make.log)
#   fetch               copy log-NAME and bench/ to toolchains/gcc-native/HOST/
#   compare OLD NEW     compare two fetched builds: status, step times,
#                       testsuite (GCC's contrib/compare_tests)
#   clean [--all]       remove build NAME (or the whole work directory)
#   xcheck [--fresh] [--lcc DIR]
#                       on this host, not the machine: compile all of
#                       GCC's host code with lcc's x86-hosted cross
#                       compiler (a canadian cross, make -k all-gcc in
#                       toolchains/gcc-native/xcheck), the code stage 1
#                       of a bootstrap compiles; lists what lcc rejects
#                       in half an hour instead of hours into a stage 1.
#                       Uses --src's working tree as it is, uncommitted
#                       changes included; --lcc defaults to the
#                       v5.8c2 tree, gmp, mpfr and mpc come from
#                       --remote once
#
# Typical first run on the 8C2:
#   gcc-native.sh check
#   gcc-native.sh push
#   gcc-native.sh bootstrap
#   gcc-native.sh wait
#   gcc-native.sh push-dejagnu && gcc-native.sh test 'execute.exp'
#   gcc-native.sh bench-inputs && gcc-native.sh bench NAME:stage1 NAME
#   gcc-native.sh fetch

set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
here=$root/tools/gcc-native
remote=claude@geofront.lan
port=
rdir=relcc2/gcc-native
src=$root/toolchains/gcc-claude-native
rev=
name=
jobs=4

die() { echo "gcc-native.sh: $*" >&2; exit 1; }
usage() { awk 'NR > 3 && /^#/ { sub(/^# ?/, ""); print; next } NR > 3 { exit }' "$0"; exit 2; }

while [ $# -gt 0 ]; do
    case $1 in
    --remote) remote=$2; shift 2 ;;
    --ssh-port) port=$2; shift 2 ;;
    --remote-dir) rdir=$2; shift 2 ;;
    --src) src=$(cd "$2" && pwd); shift 2 ;;
    --rev) rev=$2; shift 2 ;;
    --name) name=$2; shift 2 ;;
    -j) jobs=$2; shift 2 ;;
    -h|--help) usage ;;
    -*) die "unknown option $1" ;;
    *) break ;;
    esac
done
[ $# -ge 1 ] || usage
cmd=$1
shift

# relcc2's key only (AGENTS.md, "Connect only with relcc2's key").
ssh_() { "$root/tools/setup/e2k-ssh" ${port:+-p "$port"} "$remote" "$@"; }

# Quote arguments for the remote shell.
q() { printf '%q ' "$@"; }

# Copy remote.sh (and the Python helpers it runs) to the work directory.
# Each file is replaced with mv, never rewritten in place: bash reads a
# running script as it goes, so a build started by an older remote.sh
# would otherwise go on reading the new file at its old offset.
upload() {
    tar -C "$here" -c remote.sh diffcc.py \
        | ssh_ "mkdir -p $rdir/.upload && tar -C $rdir/.upload -xf - && mv -f $rdir/.upload/remote.sh $rdir/.upload/diffcc.py $rdir/"
}

# Run remote.sh in the work directory.
remote_sh() {
    upload
    ssh_ "bash $rdir/remote.sh $(q "$@")"
}

# The same, detached: the command runs in its own session with its output
# in LOG (relative to the work directory), so it keeps running when ssh
# returns or this host goes away.  The braces matter: a bare "a && b &"
# backgrounds the whole list in a subshell that keeps ssh's output open,
# and ssh then waits for the command to end.
remote_bg() {
    local log=$1
    shift
    upload
    ssh_ "cd $rdir && mkdir -p $(dirname "$log") && { ${SRCNAME:+SRCNAME=$(q "$SRCNAME")} setsid nohup bash remote.sh $(q "$@") > $log 2>&1 < /dev/null & } && echo started: $log"
}

commit=$(git -C "$src" rev-parse --verify "${rev:-HEAD}^{commit}" 2>/dev/null) \
    || die "$src: no commit ${rev:-HEAD}"
name=${name:-$(git -C "$src" rev-parse --short=11 "$commit")}

build_cmd() {
    local mode=$1 checking=yes s1=-O3 stop=all source=
    shift
    while [ $# -gt 0 ]; do
        case $1 in
        --checking) checking=$2; shift 2 ;;
        --stage1-flags) s1=$2; shift 2 ;;
        --stop-after) stop=$2; shift 2 ;;
        --with) mode=with:$2; shift 2 ;;
        --source) source=$2; shift 2 ;;
        --) shift; break ;;
        *) die "unknown build option $1" ;;
        esac
    done
    [ "$mode" != with ] || die "selfbuild needs --with OTHER"
    SRCNAME=$source remote_bg "log-$name/driver.log" build "$name" "$mode" "$jobs" "$checking" "$s1" "$stop" "$@"
}

case $cmd in
check)
    remote_sh check
    ;;
push)
    prereqs=
    [ "${1-}" = --prereqs ] && prereqs=1
    if [ -n "$(git -C "$src" status --porcelain --untracked-files=no)" ]; then
        echo "note: $src has uncommitted changes; only commit $commit is pushed" >&2
    fi
    branch=$(git -C "$src" branch --show-current)
    echo "pushing $commit (${branch:-detached}) as src-$name to $remote:$rdir"
    git -C "$src" archive --format=tar --prefix="src-$name/" "$commit" \
        | zstd -q -T0 -6 \
        | ssh_ "mkdir -p $rdir && cd $rdir && rm -rf src-$name && zstd -q -d | tar xf -"
    git -C "$src" log -1 --format="%H %s (${branch:-detached})" "$commit" \
        | ssh_ "cat > $rdir/src-$name/.relcc2-source"
    if [ -n "$prereqs" ]; then
        pre=$root/toolchains/gcc-native/prereqs
        if [ ! -d "$pre" ]; then
            mkdir -p "$pre"
            "$src/contrib/download_prerequisites" --directory="$pre"
        fi
        # The unpacked packages and the gmp, mpfr, mpc and isl links to them.
        tar -C "$pre" -c --exclude='*.tar.*' . \
            | ssh_ "cd $rdir/src-$name && tar xf -"
    fi
    ssh_ "du -sh $rdir/src-$name"
    ;;
push-dejagnu)
    rt=$(command -v runtest) || die "no runtest on this host"
    dg=$(dirname "$(dirname "$rt")")
    [ -d "$dg/share/dejagnu" ] || die "no $dg/share/dejagnu"
    tar -C "$dg" -c bin/runtest share/dejagnu \
        | ssh_ "mkdir -p $rdir/dejagnu && tar -C $rdir/dejagnu -xf - && $rdir/dejagnu/bin/runtest --version | head -1"
    ;;
bootstrap) build_cmd bootstrap "$@" ;;
lccbuild)  build_cmd lcc "$@" ;;
selfbuild) build_cmd with "$@" ;;
status)
    remote_sh status
    ;;
wait)
    every=${1:-10}
    while :; do
        out=$(remote_sh status "$name")
        echo "$(date '+%T') $out"
        echo "$out" | grep -q "^== $name: running$" || break
        sleep $((every * 60))
    done
    ;;
test)
    remote_bg "log-$name/test-driver.log" test "$name" "$jobs" "${1-}"
    ;;
bench-inputs)
    remote_sh bench-inputs "$name"
    ;;
bench)
    cpu=1 rep=3 flags="-O2 -fno-exceptions -fno-rtti"
    while [ $# -gt 0 ]; do
        case $1 in
        --cpu) cpu=$2; shift 2 ;;
        --repeat) rep=$2; shift 2 ;;
        --flags) flags=$2; shift 2 ;;
        *) break ;;
        esac
    done
    [ $# -ge 1 ] || set -- "$name:stage1" "$name"
    remote_bg "bench/driver-$name.log" bench "$cpu" "$rep" "$flags" "$@"
    ;;
diffcc)
    [ $# -ge 2 ] || die "diffcc SPEC-A SPEC-B [FLAGS...]"
    remote_bg "bench/diffcc-driver-${1//:/-}-${2//:/-}.log" diffcc "$1" "$2" "$jobs" "${@:3}"
    ;;
triage)
    [ $# -eq 1 ] || die "triage REF"
    remote_bg "log-$name/triage-driver.log" triage "$1" "$name" "$jobs"
    ;;
log)
    ssh_ "tail -n 40 $rdir/log-$name/${1:-make.log}"
    ;;
fetch)
    host=$(ssh_ hostname)
    out=$root/toolchains/gcc-native/$host
    mkdir -p "$out"
    ssh_ "cd $rdir && tar -c --exclude=lock --exclude=pid log-$name \$(ls -d bench 2>/dev/null)" \
        | tar -C "$out" -xmf -
    echo "fetched into $out"
    ;;
compare)
    [ $# -eq 2 ] || die "compare OLD NEW (names of fetched builds)"
    host=$(ssh_ hostname)
    a=$root/toolchains/gcc-native/$host/log-$1
    b=$root/toolchains/gcc-native/$host/log-$2
    [ -d "$a" ] && [ -d "$b" ] || die "fetch both builds first"
    for d in "$a" "$b"; do
        echo "== ${d##*/log-}: $(cat "$d/status")"
        sed -n '1p' "$d/settings" 2>/dev/null
        cat "$d/times"
    done
    for s in "$b"/*.sum; do
        [ -f "$s" ] || continue
        o=$a/$(basename "$s")
        if [ -f "$o" ]; then
            echo "== $(basename "$s"): $1 -> $2"
            "$src/contrib/compare_tests" "$o" "$s" || true
        fi
    done
    ;;
xcheck)
    fresh='' lccdir=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1
    while [ $# -gt 0 ]; do
        case $1 in
        --fresh) fresh=1; shift ;;
        --lcc) lccdir=$2; shift 2 ;;
        *) die "unknown xcheck option $1" ;;
        esac
    done
    x=$root/toolchains/gcc-native/xcheck
    deps=$x/deps bu=$lccdir/binutils/bin
    [ -x "$lccdir/bin/l++" ] || die "no lcc in $lccdir"
    if [ ! -f "$deps/include/gmp.h" ]; then
        # The machine's own gmp, mpfr and mpc: lcc's sysroot has none.
        mkdir -p "$deps/include" "$deps/lib64"
        ssh_ "cd /usr/include && tar -c gmp.h mpfr.h mpf2mpfr.h mpc.h" \
            | tar -C "$deps/include" -xmf -
        ssh_ "cd /usr/lib64 && tar -c libgmp.so* libmpfr.so* libmpc.so*" \
            | tar -C "$deps/lib64" -xmf -
    fi
    [ -n "$fresh" ] && rm -rf "$x/build"
    mkdir -p "$x/build"
    cd "$x/build"
    if [ ! -f Makefile ]; then
        echo "configuring $x/build"
        "$src/configure" --build=x86_64-pc-linux-gnu \
            --host=e2k-unknown-linux-gnu --target=e2k-unknown-linux-gnu \
            --prefix="$x/inst" --enable-languages=c,c++,lto \
            --disable-multilib --disable-nls --disable-libsanitizer \
            --disable-libssp --disable-libitm --disable-libquadmath \
            --disable-werror --disable-bootstrap --enable-checking=yes \
            --with-gmp-include="$deps/include" --with-gmp-lib="$deps/lib64" \
            --with-mpfr-include="$deps/include" --with-mpfr-lib="$deps/lib64" \
            --with-mpc-include="$deps/include" --with-mpc-lib="$deps/lib64" \
            CC="$lccdir/bin/lcc" CXX="$lccdir/bin/l++" \
            CFLAGS=-O3 CXXFLAGS=-O3 LDFLAGS="-Wl,-rpath-link,$deps/lib64" \
            AR="$bu/e2k-linux-ar" RANLIB="$bu/e2k-linux-ranlib" NM="$bu/e2k-linux-nm" \
            CC_FOR_BUILD=gcc CXX_FOR_BUILD=g++ \
            AS_FOR_TARGET="$bu/e2k-linux-as" LD_FOR_TARGET="$bu/e2k-linux-ld" \
            NM_FOR_TARGET="$bu/e2k-linux-nm" OBJDUMP_FOR_TARGET="$bu/e2k-linux-objdump" \
            AR_FOR_TARGET="$bu/e2k-linux-ar" RANLIB_FOR_TARGET="$bu/e2k-linux-ranlib" \
            READELF_FOR_TARGET="$bu/e2k-linux-readelf" > ../configure.log 2>&1 \
            || die "configure failed: $x/configure.log"
    fi
    echo "make -k -j$jobs all-gcc (log $x/make.log)"
    start=$(date +%s)
    touch ../make.start
    # The specs file needs an x86-hosted e2k gcc, which a canadian cross
    # lacks: the one failure every run has.
    nice -n 19 make -k -j"$jobs" all-gcc > ../make.log 2>&1 || true
    echo "$(( ($(date +%s) - start) / 60 )) min"
    errs=$(grep -E '^lcc: ".*", line [0-9]+: (error|catastrophic error)' ../make.log \
        | sed 's/^lcc: //' | sort | uniq -c)
    echo "${errs:-no lcc errors}"
    grep -E '\*\*\* \[' ../make.log | grep -v 'specs\] Error 127\|all-gcc\] Error' || true
    for f in cc1 cc1plus lto1 xgcc; do
        if [ ! -f "gcc/$f" ]; then
            echo "gcc/$f MISSING"
        elif [ "gcc/$f" -nt ../make.start ]; then
            echo "gcc/$f linked"
        else
            echo "gcc/$f up to date"
        fi
    done
    ;;
clean)
    if [ "${1-}" = --all ]; then
        remote_sh clean --all
    else
        remote_sh clean "$name"
    fi
    ;;
*)
    die "unknown command $cmd (--help)"
    ;;
esac
