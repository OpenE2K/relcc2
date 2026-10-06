#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build and run the ELF operator probes (spec/abi/elf.md):
#   islocal  @ISLOCAL32 and @DYNOPT for local, global, hidden, other-object,
#            shared-library and weak undefined symbols, linked as a non-PIE
#            executable, a PIE, a shared library and with -Bsymbolic
#   ifunc    an ifunc in a shared library and in an executable
#   gotplt   calls through @GOTPLT slots to a shared-library function and
#            a local one, from a non-PIE executable, a PIE (also with
#            -z now) and a shared library
# PIE and static links of ifunc are expected to fail with MCST ld 2.41.
#
# Usage: tools/abi/probes/elfops/run.sh [TARGET] [--remote [USER@]HOST]
#            [--remote-dir DIR] [--ssh-port N]
# DIR (default relcc2/elfops-probe, relative to $HOME) is the working
# directory on the remote host.
set -e

root=$(cd "$(dirname "$0")/../../../.." && pwd)
src=$root/tools/abi/probes/elfops
target=v3
remote=
port=
rdir=relcc2/elfops-probe
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
as=$lcc_dir/binutils/bin/e2k-linux-as
out=$root/toolchains/elfops-$target
mkdir -p "$out"
cd "$out"

isa=$(echo "$target" | sed 's/^v\([0-9]*\).*/\1/')
for f in islocal other ext gotplt; do
	"$as" -mcpu=elbrus-v$isa "$src/$f.s" -o $f.o
done
"$lcc" -shared ext.o -o libext.so
rp='-Wl,-rpath,$ORIGIN'
"$lcc" -O1 -fno-pic "$src/islocal-main.c" islocal.o other.o -L. -lext "$rp" -o islocal-exe
"$lcc" -O1 -fPIE -pie "$src/islocal-main.c" islocal.o other.o -L. -lext "$rp" -o islocal-pie
"$lcc" -shared islocal.o other.o -L. -lext -o libislocal.so
# PIE, so that dyn_tab is read from the library rather than a copy.
"$lcc" -O1 -fPIE -pie "$src/islocal-main.c" -L. -lislocal -lext "$rp" -o islocal-dso
"$lcc" -O1 -fPIC -shared "$src/ifunc.c" -o libifunc.so
"$lcc" -O1 "$src/ifunc-main.c" -L. -lifunc "$rp" -o ifunc-dso
"$lcc" -O1 "$src/ifunc-main.c" "$src/ifunc.c" -o ifunc-exe
"$lcc" -O1 -fno-pic "$src/gotplt-main.c" gotplt.o -L. -lext "$rp" -o gotplt-exe
"$lcc" -O1 -fPIE -pie "$src/gotplt-main.c" gotplt.o -L. -lext "$rp" -o gotplt-pie
"$lcc" -O1 -fPIE -pie "$src/gotplt-main.c" gotplt.o -L. -lext "$rp" \
	-Wl,-z,now -o gotplt-now
"$lcc" -shared gotplt.o -L. -lext "$rp" -o libgotplt.so
"$lcc" -O1 -fPIE -pie "$src/gotplt-main.c" -L. -lgotplt -lext "$rp" -o gotplt-dso
progs="islocal-exe islocal-pie islocal-dso ifunc-dso ifunc-exe gotplt-exe gotplt-pie gotplt-now gotplt-dso"
libs="libext.so libislocal.so libifunc.so libgotplt.so"
script="cd $rdir && for p in $progs; do echo \"== \$p\"; ./\$p; done"
if [ -n "$remote" ]; then
	key=$root/secrets/id_ed25519
	ssh_opts="-o BatchMode=yes"
	[ -f "$key" ] && ssh_opts="$ssh_opts -i $key -o IdentitiesOnly=yes"
	[ -n "$port" ] && ssh_opts="$ssh_opts -o Port=$port"
	ssh $ssh_opts "$remote" "mkdir -p $rdir"
	scp -q $ssh_opts $libs $progs "$remote:$rdir/"
	ssh $ssh_opts "$remote" "$script"
else
	for p in $progs; do
		echo "== $p"
		qemu-e2k -cpu elbrus-v$isa -L "$lcc_dir/fs" ./$p
	done
fi
