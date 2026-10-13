#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# gdb and core dumps of a 32-bit program (spec/abi/mode32.md, "C
# runtime"), run on the target machine in a directory holding dbg32.gdb
# and the programs (dbg32.c says how they are built; run.sh copies them):
#
#   sh dbg32.sh PROG...
#   DBG32_LOADER="./lib32/ld-linux.so.2 --library-path lib32" sh dbg32.sh PROG
#                       # (the 4C: a dynamic 32-bit program through lcc's
#                       # own ld.so, which gdb then runs as the program;
#                       # gdb doesn't load the symbols of a program that
#                       # ld.so maps, so add-symbol-file supplies them)
#
# For each PROG: gdb runs dbg32.gdb (which writes dbg32.gcore), then
# reads that gcore file back; then PROG is run with `abort' and with
# `segv' under `ulimit -c unlimited', and a core file, if the kernel
# wrote one, is described (its ELF class, program headers) and read by
# gdb. Core files are removed afterwards.
set -u
loader=${DBG32_LOADER:-}
ulimit -c unlimited 2>/dev/null ||
	echo "ulimit -c unlimited refused (hard limit $(ulimit -Hc))"
echo "core_pattern: $(cat /proc/sys/kernel/core_pattern)," \
	"core_uses_pid: $(cat /proc/sys/kernel/core_uses_pid)," \
	"ulimit -c: $(ulimit -c)"
gdb --version | head -1
for p in "$@"; do
	echo
	echo "######## $p: $(file -L "$p" | sed 's/^[^:]*: //' | cut -c1-90)"
	rm -f dbg32.gcore core core.*
	# Each line of dbg32.gdb as its own -ex, so that a failing command
	# (gdb -x stops a file at the first error) doesn't stop the rest.
	grep -v '^#' dbg32.gdb | tr '\n' '\0' | xargs -0 sh -c \
		'p=$1; shift; for c; do set -- "$@" -ex "$c"; shift; done;
		 if [ -n "${DBG32_LOADER:-}" ]; then
			exec gdb -q -batch -ex "add-symbol-file ./$p" "$@" \
				--args $DBG32_LOADER "./$p"
		 fi
		 exec gdb -q -batch "$@" "./$p"' sh "$p" 2>&1
	echo
	echo "== gcore file: $(file dbg32.gcore 2>/dev/null | sed 's/^[^:]*: //')"
	if [ -f dbg32.gcore ]; then
		gdb -q -batch -ex bt -ex 'p/x gll' -ex 'p gpt.name' \
			-ex 'p $pc' "./$p" dbg32.gcore 2>&1 | tail -8
	fi
	for how in abort segv; do
		rm -f core core.*
		echo
		echo "== $p $how"
		$loader "./$p" "$how"
		echo "exit status $?"
		c=$(ls core core.* 2>/dev/null | head -1)
		if [ -z "$c" ]; then
			echo "no core file"
			continue
		fi
		echo "core file $c: $(file "$c" | sed 's/^[^:]*: //')"
		readelf -h "$c" 2>&1 | grep -E 'Class|Machine|Flags'
		readelf -lW "$c" 2>&1 | grep -E 'NOTE|LOAD|LOPROC|0x7000' | head -40
		readelf -nW "$c" 2>&1 | grep -E 'NT_|Owner' | sort | uniq -c
		gdb -q -batch -ex bt -ex 'p/x gll' -ex 'p gpt.name' -ex 'p $pc' \
			-ex 'info registers ip' "./$p" "$c" 2>&1 | tail -12
	done
	rm -f dbg32.gcore core core.*
done
