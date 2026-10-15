#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# What MCST gas (toolchains/binutils) does with debug-information input,
# on the host, no hardware:
#   a   GCC configure's .loc probe (.file, .loc 1 3 0, nop): any
#       .debug_line?
#   b   three lines, each preceded by ".loc 1 N 0": where do the rows go?
#   c   the same with "view" operands
#   d   .loc and a label inside a wide instruction's braces
#   e   plain assembly with --gdwarf-5, --gdwarf2, -g
#   f   .label inside braces (lcc -gline's syllable labels)
#   g   which data directives take @TLS_DTPREL
#
#   tools/abi/probes/debuginfo_loc.sh [OUTDIR]   (default toolchains/debuginfo/loc)
#
# Results (binutils b4bebd940a3, 2026-10-16):
#   a   no .debug_line at all: the probe fails, so GCC's configure leaves
#       HAVE_AS_DWARF2_DEBUG_LINE undefined for E2K whether or not e2k is
#       in its cpu_type list.
#   b   tc-e2k.c never calls dwarf2_emit_insn, so a .loc's row is made
#       only when the next .loc is read, at that later address: line 10
#       at 0x8 (line 11's start), line 11 at 0x18, and the last .loc
#       (line 12) gets no row.
#   c   with "view" each row is made at once, at the right address
#       (0, 0x8, 0x18).
#   d   with "view" a .loc inside braces gets the start of the wide
#       instruction being built (view 1 if a row is already there); a
#       plain label inside braces too (.dword .Lmid: .text + 0x10).
#   e   no debug sections at all (no rows, so no .debug_line, .debug_info
#       or .debug_aranges).
#   f   ".label NAME" before an operation gives NAME the address of the
#       syllable that encodes it (needs -mcpu for setwd's dbl): in
#       { setwd; disp; addd,1; addd,3 } the addd,1 label is +4 (ALS1),
#       the addd,3 one +8 (ALS3), the disp one +0xc (CS0).
#   g   .dword, .8byte, .quad, .uadword: R_E2K_TLS_64_DTPREL; .word,
#       .4byte, .long, .uaword: R_E2K_TLS_32_DTPREL; .2byte rejected.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
B=$root/toolchains/binutils/bin
out=${1:-$root/toolchains/debuginfo/loc}
mkdir -p "$out"
cd "$out" || exit 1

body='	.text
	.file 1 "loc.c"
	.global f
	.type f, @function
f:
	.loc 1 10 0
	{ addd,0 %dr0, 1, %dr0 }
	.loc 1 11 0
	{ addd,0 %dr0, 2, %dr0 }
	{ addd,0 %dr0, 3, %dr0 }
	.loc 1 12 0
	{ addd,0 %dr0, 4, %dr0 }
	{ return %ctpr3 }
	{ ct %ctpr3 }
	.size f, .-f'

echo "=== a: configure's probe"
printf '\t.file 1 "conftest.s"\n\t.loc 1 3 0\n\tnop\n' > a.s
"$B/e2k-linux-as" a.s -o a.o && "$B/e2k-linux-objdump" -h a.o | grep -c debug_line

echo "=== b: .loc without view"
printf '%s\n' "$body" > b.s
"$B/e2k-linux-as" b.s -o b.o && "$B/e2k-linux-readelf" --debug-dump=decodedline b.o | sed -n '/File name/,$p'

echo "=== c: .loc with view"
sed 's/\(\.loc 1 1\([0-9]\) 0\)/\1 view .LVU\2/' b.s > c.s
"$B/e2k-linux-as" c.s -o c.o && "$B/e2k-linux-readelf" --debug-dump=decodedline c.o | sed -n '/File name/,$p'
"$B/e2k-linux-objdump" -d c.o | grep -E '^ +[0-9a-f]+:'

echo "=== d: .loc and a label inside braces"
cat > d.s <<'EOF'
	.text
	.file 1 "loc.c"
f:
	.loc 1 10 0 view .LVU0
	{
	  addd,0	%dr0, 1, %dr0
	.loc 1 11 0 view .LVU1
	  addd,1	%dr1, 2, %dr1
	}
	{
	  addd,0	%dr0, 3, %dr0
	.loc 1 12 0 view .LVU2
.Lmid:
	  addd,1	%dr1, 4, %dr1
	}
	{ return %ctpr3 }
	{ ct %ctpr3 }
	.data
	.dword	.Lmid
EOF
"$B/e2k-linux-as" d.s -o d.o && "$B/e2k-linux-readelf" --debug-dump=decodedline d.o | sed -n '/File name/,$p'
"$B/e2k-linux-readelf" -r d.o | grep -A2 rela.data

echo "=== e: --gdwarf-5, --gdwarf2, -g on plain assembly"
grep -v '\.loc\|\.file' b.s > e.s
for o in --gdwarf-5 --gdwarf2 -g; do
  "$B/e2k-linux-as" $o e.s -o e.o
  echo "$o: $("$B/e2k-linux-objdump" -h e.o | grep -c '\.debug_') debug sections"
done

echo "=== f: .label inside braces"
cat > f.s <<'EOF'
	.text
f:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	.label .LA:
	  disp	%ctpr1, f
	.label .LB:
	  addd,1	%dr1, 2, %dr1
	.label .LC:
	  addd,3	%dr3, 2, %dr3
	}
	.data
	.dword	.LA, .LB, .LC
EOF
"$B/e2k-linux-as" -mcpu=elbrus-v3 f.s -o f.o && "$B/e2k-linux-readelf" -r f.o | grep R_E2K
"$B/e2k-linux-objdump" -d f.o | sed -n '/<f>/,$p'

echo "=== g: @TLS_DTPREL"
for d in .uadword .dword .8byte .quad .word .4byte .uaword .long .2byte; do
  printf '\t.section .tbss,"awT",@nobits\nx:\t.zero 4\n\t.section .debug_info\n\t%s x@TLS_DTPREL\n' $d > g.s
  printf '%-9s ' $d
  if "$B/e2k-linux-as" g.s -o g.o 2> g.err; then
    "$B/e2k-linux-readelf" -rW g.o | grep -o 'R_E2K[A-Z0-9_]*'
  else
    echo "rejected: $(sed -n 2p g.err)"
  fi
done
