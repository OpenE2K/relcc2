#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Run an E2K program under MCST's qemu-e2k and write how often each
# instruction executed, for score.py --counts.
#
#   tools/isa-score/count.sh [-o COUNTS] [-L SYSROOT] [-cpu elbrus-vN] PROGRAM [ARGS...]
#
# COUNTS defaults to PROGRAM.counts.  SYSROOT defaults to lcc 1.29.16's
# for v3.  The plugin is built once into toolchains/isa-score/.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
qemu=$root/toolchains/qemu-mcst
build=$root/toolchains/isa-score
sysroot=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/fs
cpu=elbrus-v3
out=
while [ $# -gt 0 ]; do
  case $1 in
    -o) out=$2; shift 2 ;;
    -L) sysroot=$2; shift 2 ;;
    -cpu) cpu=$2; shift 2 ;;
    *) break ;;
  esac
done
[ $# -ge 1 ] || { sed -n '3,9p' "$0"; exit 2; }
prog=$1
out=${out:-$prog.counts}
plugin=$build/libbbcount.so
if [ ! -f "$plugin" ] || [ "$here/bbcount.c" -nt "$plugin" ]; then
  mkdir -p "$build"
  cc -O2 -shared -fPIC -I"$qemu/include" $(pkg-config --cflags glib-2.0) \
    "$here/bbcount.c" -o "$plugin" $(pkg-config --libs glib-2.0)
fi
"$qemu/bin/qemu-e2k" -cpu "$cpu" -L "$sysroot" \
  -plugin "$plugin,out=$(realpath -m "$out")" "$@"
