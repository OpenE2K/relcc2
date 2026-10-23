#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# _GLOBAL_OFFSET_TABLE_ and @GOTOFF in data directives (spec/abi/elf.md,
# "Assembler syntax"; docs/spec-poc-findings.md A34).  Assembles
# tools/abi/probes/gotdata.s with MCST gas, prints the relocations of the
# forms that work, links them with lcc into a PIE and a plain executable
# and checks each word against the linked _GLOBAL_OFFSET_TABLE_ and
# loc; then assembles each failing form (gotdata.s CASE 1-8) and prints
# gas's first two messages.  Last, it compiles a small C file with lcc
# -fPIC -g at -O0, -O2 and -O3 and counts how lcc's data and debug
# sections name addresses.  Nothing is run.
#
# Usage: tools/abi/probes/gotdata.sh     (output in toolchains/gotdata/)
#        B=<binutils bin directory> to use another gas (default: the
#        submodule's, toolchains/binutils/bin; setup/build-binutils.sh)
#        LCCS="<lcc> ..." for the last step (default: lcc 1.29.16 e2k-v3)
#
# MCST binutils b4bebd940a3 (gas 2.41.0-29.023) and lcc 1.29.16's
# (2.41-29.018), 2026-10-24, the same:
#   CASE 0: got_abs R_E2K_64_ABS _GLOBAL_OFFSET_TABLE_, got_pc R_E2K_64_PC
#     _GLOBAL_OFFSET_TABLE_, loc@GOTOFF R_E2K_64_GOTOFF (.dword) and
#     R_E2K_GOTOFF (.word); in .debug_info R_E2K_64_ABS
#     _GLOBAL_OFFSET_TABLE_, R_E2K_64_GOTOFF, R_E2K_64_ABS.  Linked (PIE
#     and executable) every word is right; in the PIE ld resolves the
#     .debug_info words to link-time values and turns got_abs into an
#     R_E2K_64_RELATIVE.
#   CASE 1, 2, 3, 5, 6, 7: "symbol definition loop encountered at `L0'"
#     and "Internal error in S_SET_SEGMENT"; CASE 4: the loop error and
#     "can't resolve glob - L0"; CASE 8: "invalid @GOT value size 8,
#     should be 4".
# lcc 1.29.16 (e2k-v3, e2k-v5.8c2, e2k-v6.2c3) and 1.31.05 (e2k-v3),
# 2026-10-24, the same at -O0, -O2 and -O3: no _GLOBAL_OFFSET_TABLE_
# outside .text; .debug_info gives a static's address as ".uadword
# counter" (after DW_OP_addr), .data's initialisers ".dword counter";
# at -O0 the switch's jump table in .rodata holds ".dword .LSC.N@GOTOFF"
# (9 entries), from -O2 there is none.

root=$(cd "$(dirname "$0")/../../.." && pwd)
out=$root/toolchains/gotdata
B=${B:-$root/toolchains/binutils/bin}
lcc=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
src=$root/tools/abi/probes/gotdata.s
mkdir -p "$out"
cd "$out" || exit 1

"$B/e2k-linux-as" --version | head -1
"$B/e2k-linux-as" -mcpu=elbrus-v3 -mptr64 --defsym CASE=0 "$src" \
	-o gotdata.o || exit 1

echo "== relocations in gotdata.o"
"$B/e2k-linux-readelf" -rW gotdata.o | awk '
	/^Relocation section/ { sec = $3; gsub(/\x27/, "", sec) }
	/R_E2K/ { print sec, $1, $3, $5, $6, $7 }'

# word FILE SECTION OFFSET SIZE: the little-endian word at OFFSET.
word() {
	"$B/e2k-linux-objcopy" --dump-section "$2=sec.bin" "$1" /dev/null \
		2>/dev/null || "$B/e2k-linux-objcopy" -O binary \
		--only-section="$2" "$1" sec.bin
	od -An -v -j "$3" -N "$4" -t "x$4" sec.bin | tr -d ' '
}

sym() {
	"$B/e2k-linux-nm" "$1" | awk -v s="$2" '$3 == s { print $1 }'
}

check() {	# name got-value expected
	if [ $(($2)) -eq $(($3)) ]; then v=right; else v=wrong; fi
	printf '  %-22s 0x%x (expected 0x%x): %s\n' "$1" $(($2)) $(($3)) $v
}

for kind in pie exe; do
	case $kind in
	pie) "$lcc" -pie gotdata.o -o gotdata-pie || exit 1 ;;
	exe) "$lcc" -no-pie gotdata.o -o gotdata-exe 2>/dev/null ||
		"$lcc" gotdata.o -o gotdata-exe || exit 1 ;;
	esac
	f=gotdata-$kind
	echo "== $f ($("$B/e2k-linux-readelf" -hW $f | awk '/Type:/ { print $2 }'))"
	got=0x$(sym $f _GLOBAL_OFFSET_TABLE_)
	loc=0x$(sym $f loc)
	data=0x$("$B/e2k-linux-readelf" -SW $f |
		awk '$2 == ".data" { print $4 }')
	printf '  _GLOBAL_OFFSET_TABLE_ 0x%x, loc 0x%x\n' $got $loc
	echo " .debug_info:"
	check "_GOT_@ABS" 0x$(word $f .debug_info 0 8) $got
	check "loc@GOTOFF" 0x$(word $f .debug_info 8 8) $((loc - got))
	check "loc" 0x$(word $f .debug_info 16 8) $loc
	echo " .data:"
	a=0x$(sym $f got_abs)
	rel=$("$B/e2k-linux-readelf" -rW $f |
		awk -v a=$(printf '%016x' $a) '$1 == a { print $3, $4 }')
	if [ -n "$rel" ]; then
		echo "  got_abs                $rel (dynamic)"
		set -- $rel
		[ "$1" = R_E2K_64_RELATIVE ] && check "  its addend" 0x$2 $got
	else
		check got_abs 0x$(word $f .data $((a - data)) 8) $got
	fi
	p=0x$(sym $f got_pc)
	check "_GOT_@ABS - ." 0x$(word $f .data $((p - data)) 8) \
		$(((got - p) & 0xffffffffffffffff))
	o=0x$(sym $f loc_gotoff64)
	check "loc@GOTOFF (.dword)" 0x$(word $f .data $((o - data)) 8) \
		$((loc - got))
	o=0x$(sym $f loc_gotoff32)
	check "loc@GOTOFF (.word)" 0x$(word $f .data $((o - data)) 4) \
		$(((loc - got) & 0xffffffff))
done

echo "== forms that fail"
for c in 1 2 3 4 5 6 7 8; do
	line=$(grep -A2 "CASE == $c\$" "$src" | grep -v -e CASE -e '^	\.section' |
		head -1 | sed 's/^[ 	]*//')
	echo "CASE $c: $line"
	"$B/e2k-linux-as" -mcpu=elbrus-v3 -mptr64 --defsym CASE=$c "$src" \
		-o fail.o 2>&1 | grep -v 'Assembler messages' | head -2 |
		sed "s|^.*gotdata\.s|  gotdata.s|; s| at /.*/gas/| at gas/|"
	rm -f fail.o
done

echo "== lcc -fPIC -g: addresses in data and debug sections"
cat > gd.c <<'EOF'
static int counter;
int glob;
static int *pc = &counter;
int *pg = &glob;
static int (*fp) (void);
int bump (void) { return ++counter + *pc + *pg; }
int sw (int x)
{
  switch (x)
    {
    case 0: return 11; case 1: return 22; case 2: return 37;
    case 3: return 41; case 4: return 53; case 5: return 67;
    case 6: return 71; case 7: return 89; case 8: return 97;
    }
  return fp ? fp () : 0;
}
EOF
for L in ${LCCS:-$lcc}; do
	for o in -O0 -O2 -O3; do
		"$L" $o -fPIC -g -S gd.c -o gd.s || exit 1
		printf '%s %s:\n ' "$L" $o
		awk '/^\t\.section/ { s = $2; next }
		/^\t\.text/ { s = ".text" } /^\t\.data$/ { s = ".data" }
		s != ".text" && /_GLOBAL_OFFSET_TABLE_/ { g++ }
		s == ".debug_info" && /\.uadword\tcounter$/ { u++ }
		s == ".data" && /\.dword\tcounter$/ { d++ }
		s != ".text" && /@GOTOFF/ { go++ }
		END { printf "GOT outside .text %d,", g
		      printf " .debug_info .uadword counter %d,", u
		      printf " .data .dword counter %d,", d
		      printf " @GOTOFF outside .text %d\n", go }' gd.s
	done
done
