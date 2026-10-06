#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the dso probe (lib.c as a shared library; main.c as a PIE, a
# non-PIE and a static executable) with lcc and run every variant, lazily
# and with LD_BIND_NOW=1. Each line must read "152 42 5 1".
#
# Usage: tools/abi/probes/dso/run.sh [TARGET] [--remote [USER@]HOST]
#            [--remote-dir DIR] [--ssh-port N]
#   TARGET: lcc target directory suffix (default v3), see AGENTS.md.
#   Without --remote the programs run under qemu-e2k. DIR (default
#   relcc2/dso-probe, relative to $HOME) is the remote working directory.
set -e

root=$(cd "$(dirname "$0")/../../../.." && pwd)
src=$root/tools/abi/probes/dso
target=v3
remote=
port=
rdir=relcc2/dso-probe
while [ $# -gt 0 ]; do
	case $1 in
	--remote) remote=$2; shift 2 ;;
	--remote-dir) rdir=$2; shift 2 ;;
	--ssh-port) port=$2; shift 2 ;;
	*) target=$1; shift ;;
	esac
done
lcc_dir=/opt/mcst/lcc-1.29.16.e2k-$target.linux-6.1
lcc=$lcc_dir/bin/lcc
out=$root/toolchains/dso-$target
mkdir -p "$out"

$lcc -O2 -fPIC -shared "$src/lib.c" -o "$out/libdso.so"
$lcc -O2 -fPIE -pie "$src/main.c" -L"$out" -ldso -Wl,-rpath,'$ORIGIN' -o "$out/main-pie"
$lcc -O2 -fno-pic "$src/main.c" -L"$out" -ldso -Wl,-rpath,'$ORIGIN' -o "$out/main-nopie"
$lcc -O2 -static "$src/main.c" "$src/lib.c" -o "$out/main-static"

progs="main-pie main-nopie main-static"
script="cd $rdir && for p in $progs; do echo \"\$p: \$(./\$p)\"; echo \"\$p now: \$(LD_BIND_NOW=1 ./\$p)\"; done"
if [ -n "$remote" ]; then
	key=$root/secrets/id_ed25519
	ssh_opts="-o BatchMode=yes"
	[ -f "$key" ] && ssh_opts="$ssh_opts -i $key -o IdentitiesOnly=yes"
	[ -n "$port" ] && ssh_opts="$ssh_opts -o Port=$port"
	ssh $ssh_opts "$remote" "mkdir -p $rdir"
	scp -q $ssh_opts "$out/libdso.so" "$out/main-pie" "$out/main-nopie" "$out/main-static" "$remote:$rdir/"
	ssh $ssh_opts "$remote" "$script"
else
	isa=$(echo "$target" | sed 's/^v\([0-9]*\).*/\1/')
	cd "$out"
	for p in $progs; do
		echo "$p: $(qemu-e2k -cpu elbrus-v$isa -L "$lcc_dir/fs" ./$p)"
	done
fi
