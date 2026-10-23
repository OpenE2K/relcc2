#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Which functions load the GOT address (R_E2K_64_PC_LIT against
# _GLOBAL_OFFSET_TABLE_, spec/abi/elf.md "Code sequences"), at -O0 and
# -O2, non-PIC and -fPIC. MCST's module loader takes no such relocation
# (arch/e2k/kernel/module.c:63-105; spec/abi/kernel-build.md,
# "Modules"), and the kernel builds two modules at -O0:
# crypto/jitterentropy.c (CFLAGS_jitterentropy.o = -O0, crypto/Makefile:
# 171; #error under __OPTIMIZE__, jitterentropy.c:53-55) and smifb's
# software i2c (#pragma GCC optimize ("O0") for every compiler but clang,
# drivers/mcst/smi/ddk750/ddk750_swi2c.c:200-203,
# ddk768/ddk768_swi2c.c:275-278). Compiles a small file with lcc 1.29.16
# (e2k-v3, e2k-v5.8c2, e2k-v6.2c3) and prints, per function, the
# relocations its code carries; GCC='cmd' adds that compiler. Nothing
# runs.
#
# Usage: tools/abi/probes/foldp107_gotbase.sh  (output in
#        toolchains/foldp107/)
#
# 2026-10-24, lcc 1.29.16 (all three targets): non-PIC code at -O0
# (also with -fkernel -mdmodel=large) and -O2 has no GOT reference
# (R_E2K_64_ABS_LIT, R_E2K_DISP); -O0 -fPIC loads the GOT address in
# use_globals, calls and addr, not in leaf. GCC with a pseudo PIC
# register whose TARGET_INIT_PIC_REG loads it unconditionally (the
# proof-of-concept back end before v0.5-32) gave every function one
# R_E2K_64_PC_LIT at -O0, non-PIC included, and none at -O1 and -O2.

root=$(cd "$(dirname "$0")/../../.." && pwd)
out=$root/toolchains/foldp107
bu=$root/toolchains/binutils/bin
mkdir -p "$out"
cd "$out" || exit 1

cat > m.c <<'EOF'
extern int ext_var;
static int loc_var;
extern void ext_fn (int);
int leaf (int a) { return a * 3 + 1; }
int use_globals (int a) { loc_var += a; return ext_var + loc_var; }
void calls (int a) { ext_fn (a); ext_fn (loc_var); }
int (*addr (void)) (int) { return leaf; }
EOF

relocs () {
	$bu/e2k-linux-objdump -dr "$1" | awk '
		/^[0-9a-f]+ <[^>]+>:/ { f = $2; gsub(/[<>:]/, "", f) }
		/R_E2K_/ { r[f] = r[f] " " $2 "(" $3 ")" }
		END { for (f in r) print "    " f ":" r[f] }' | sort
}

for t in e2k-v3 e2k-v5.8c2 e2k-v6.2c3; do
	lcc=/opt/mcst/lcc-1.29.16.$t.linux-6.1/bin/lcc
	for o in "-O0" "-O0 -fkernel -mdmodel=large" "-O2" "-O0 -fPIC"; do
		# shellcheck disable=SC2086
		$lcc $o -c m.c -o m.o || exit 1
		echo "lcc $t $o:"
		relocs m.o
	done
done

if [ -n "$GCC" ]; then
	for o in "-O0" "-O1" "-O2" "-O0 -fPIC"; do
		# shellcheck disable=SC2086
		$GCC $o -c m.c -o m.o || exit 1
		echo "gcc $o:"
		relocs m.o
	done
fi
