#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the protected-mode probes pmasm.c, pmrun.c and pmcxx.cc and run
# them on an Elbrus machine over SSH: pmasm, pmcxx and pmrun once, then pmrun's
# malloc cases ("uaf") under each of the PM_* environment settings that
# the kernel turns into the process's protected-mode flags and lib128's
# malloc and free read, one malloc and free without a child process
# ("free1") with the dangling-pointer check off and on, and its system
# call cases ("sys") with the kernel's soft mode on (the default) and off.
# spec/abi/protected-mode.md has the results.
#
# Usage: tools/abi/probes/protected/run.sh --remote [USER@]HOST
#            [--ssh-port N] [--remote-dir DIR] [--static TARGET] [--keep]
#
#   By default the sources are copied over and built there by the
#   machine's own lcc (lcc -mptr128 -O1) against its /lib128 (OS Elbrus
#   on the 8C2 and 2C3 ships one).  --static TARGET builds static
#   programs here with /opt/mcst/lcc-1.29.16.e2k-TARGET.linux-6.1 instead,
#   for machines without /lib128 (ALT Linux on the 4C: --static v3).  A
#   dynamic program linked by a binutils configured without
#   --enable-new-pm-abi (such as the crossdev one the /opt/mcst/lcc-1.29.16
#   trees may link with) does not run against the machines' /lib128: its
#   PLT hands over 4 register pairs, not 8 (protected-mode.md, "ELF and
#   linking"); a static program has no PLT.
#   DIR (default relcc2/protected-probe, relative to $HOME) is created and,
#   unless --keep, removed afterwards.
set -e

root=$(cd "$(dirname "$0")/../../../.." && pwd)
src=$root/tools/abi/probes/protected
remote=
port=22
rdir=relcc2/protected-probe
static=
keep=
while [ $# -gt 0 ]; do
	case $1 in
	--remote) remote=$2; shift 2 ;;
	--ssh-port) port=$2; shift 2 ;;
	--remote-dir) rdir=$2; shift 2 ;;
	--static) static=$2; shift 2 ;;
	--keep) keep=1; shift ;;
	*) echo "usage: $0 --remote [USER@]HOST [--ssh-port N] [--remote-dir DIR] [--static TARGET] [--keep]" >&2; exit 2 ;;
	esac
done
[ -n "$remote" ] || { echo "$0: --remote is required" >&2; exit 2; }

# relcc2's key and nothing else (tools/setup/e2k-ssh refuses without it)
ssh_() { "$root/tools/setup/e2k-ssh" -p "$port" "$remote" "$@"; }
scp_() { "$root/tools/setup/e2k-scp" -q -P "$port" "$@"; }

ssh_ "mkdir -p $rdir"
if [ -n "$static" ]; then
	lcc=/opt/mcst/lcc-1.29.16.e2k-$static.linux-6.1/bin/lcc
	[ -x "$lcc" ] || lcc=$root/toolchains/lcc-1.29.16-e2k-$static/lcc
	out=$root/toolchains/protected-probe
	mkdir -p "$out"
	"$lcc" -mptr128 -O1 -static "$src/pmrun.c" -o "$out/pmrun" -lpthread
	"$lcc" -mptr128 -O1 -static "$src/pmasm.c" -o "$out/pmasm"
	"$(dirname "$lcc")/l++" -mptr128 -O1 -static "$src/pmcxx.cc" -o "$out/pmcxx"
	scp_ "$out/pmrun" "$out/pmasm" "$out/pmcxx" "$remote:$rdir/"
else
	scp_ "$src/pmrun.c" "$src/pmasm.c" "$src/pmcxx.cc" "$remote:$rdir/"
	ssh_ "cd $rdir && lcc -mptr128 -O1 pmrun.c -o pmrun -lpthread && \
		lcc -mptr128 -O1 pmasm.c -o pmasm && \
		l++ -mptr128 -O1 pmcxx.cc -o pmcxx"
fi

ssh_ "cd $rdir && uname -r && \
	for f in /proc/sys/kernel/e2k/SCM/*; do echo \"\$f=\$(cat \$f)\"; done; \
	echo '== pmasm'; ./pmasm 2>&1; \
	echo '== pmcxx'; ./pmcxx 2>&1; \
	echo '== pmrun'; ./pmrun 2>&1; \
	for e in PMRUN_DEFAULT=1 PM_MM_CHECK_4_DANGLING_POINTERS=1 \
		 PM_MM_EMPTYING_FREED_POINTERS=1 PM_MM_ZEROING_FREED_POINTERS=1 \
		 PM_MALLOC_MODE=0 PM_MALLOC_MODE=2; do \
		echo \"== uaf \$e\"; env \$e ./pmrun uaf 2>&1; done; \
	for e in PMRUN_DEFAULT=1 PM_MM_CHECK_4_DANGLING_POINTERS=1; do \
		echo \"== free1 \$e\"; env \$e ./pmrun free1 2>&1; \
		echo \"free1: exit \$?\"; done; \
	for e in PMRUN_DEFAULT=1 PROTECTED_MODE_SOFT=0; do \
		echo \"== sys \$e\"; env \$e ./pmrun sys 2>&1; done" || true

[ -n "$keep" ] || ssh_ "rm -rf $rdir"
