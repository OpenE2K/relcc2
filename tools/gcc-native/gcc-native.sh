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
#       --stop-after configure|stage1
#   status              every build on the machine: state, last step
#   wait [MINUTES]      print status every MINUTES (default 10) until NAME
#                       is no longer running
#   test [RUNTESTFLAGS] make -k check-gcc in build NAME (detached)
#   bench-inputs        preprocess the benchmark sources with build NAME
#   bench [--cpu N] [--repeat R] [--flags F] SPEC...
#                       time cc1plus builds on those inputs under the
#                       timing lock; SPEC is NAME, NAME:stage1, NAME:stage2
#                       or NAME:inst (detached; see status, fetch)
#   log [FILE]          tail a log of build NAME (default make.log)
#   fetch               copy log-NAME and bench/ to toolchains/gcc-native/HOST/
#   compare OLD NEW     compare two fetched builds: status, step times,
#                       testsuite (GCC's contrib/compare_tests)
#   clean [--all]       remove build NAME (or the whole work directory)
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
usage() { sed -n '4,57p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }

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

# Copy remote.sh and run it in the work directory.
remote_sh() {
    ssh_ "mkdir -p $rdir && cat > $rdir/remote.sh" < "$here/remote.sh"
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
    ssh_ "mkdir -p $rdir && cat > $rdir/remote.sh" < "$here/remote.sh"
    ssh_ "cd $rdir && mkdir -p $(dirname "$log") && { setsid nohup bash remote.sh $(q "$@") > $log 2>&1 < /dev/null & } && echo started: $log"
}

commit=$(git -C "$src" rev-parse --verify "${rev:-HEAD}^{commit}" 2>/dev/null) \
    || die "$src: no commit ${rev:-HEAD}"
name=${name:-$(git -C "$src" rev-parse --short=11 "$commit")}

build_cmd() {
    local mode=$1 checking=yes s1=-O3 stop=all
    shift
    while [ $# -gt 0 ]; do
        case $1 in
        --checking) checking=$2; shift 2 ;;
        --stage1-flags) s1=$2; shift 2 ;;
        --stop-after) stop=$2; shift 2 ;;
        --with) mode=with:$2; shift 2 ;;
        --) shift; break ;;
        *) die "unknown build option $1" ;;
        esac
    done
    [ "$mode" != with ] || die "selfbuild needs --with OTHER"
    remote_bg "log-$name/driver.log" build "$name" "$mode" "$jobs" "$checking" "$s1" "$stop" "$@"
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
