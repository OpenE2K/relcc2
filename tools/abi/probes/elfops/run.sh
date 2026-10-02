#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build and run the ELF operator probes (spec/abi/elf.md):
#   islocal  @ISLOCAL32 and @DYNOPT for local, global, hidden, other-object,
#            shared-library and weak undefined symbols, linked as a non-PIE
#            executable, a PIE, a shared library and with -Bsymbolic
#   ifunc    an ifunc in a shared library and in an executable
# PIE and static links of ifunc are expected to fail with MCST ld 2.41.
#
# Usage: tools/abi/probes/elfops/run.sh [TARGET] [--remote [USER@]HOST]
set -e

root=$(cd "$(dirname "$0")/../../../.." && pwd)
src=$root/tools/abi/probes/elfops
target=v3
remote=
while [ $# -gt 0 ]; do
	case $1 in
	--remote) remote=$2; shift 2 ;;
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
for f in islocal other ext; do
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
progs="islocal-exe islocal-pie islocal-dso ifunc-dso ifunc-exe"
script="cd elfops-probe && for p in $progs; do echo \"== \$p\"; ./\$p; done"
if [ -n "$remote" ]; then
	key=$root/secrets/id_ed25519
	ssh_opts="-o BatchMode=yes"
	[ -f "$key" ] && ssh_opts="$ssh_opts -i $key -o IdentitiesOnly=yes"
	ssh $ssh_opts "$remote" 'mkdir -p elfops-probe'
	scp -q $ssh_opts libext.so libislocal.so libifunc.so $progs "$remote:elfops-probe/"
	ssh $ssh_opts "$remote" "$script"
else
	for p in $progs; do
		echo "== $p"
		qemu-e2k -cpu elbrus-v$isa -L "$lcc_dir/fs" ./$p
	done
fi
