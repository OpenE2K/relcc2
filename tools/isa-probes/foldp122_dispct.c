/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A `disp` and a `ct` through the same CTPR in one wide instruction, on
   v3 and v5 (recheck of docs/spec-poc-findings.md P122;
   spec/isa/control-transfer.md, "Restrictions").

   gas refuses the pair before v6 ("the wide instruction contains CT and
   CTP instructions making use of the same %ctpr register (%ctpr1) which
   is not supported on elbrus-v3"), so the function below is gas's v6
   encoding of this text (toolchains/binutils e2k-linux-as
   -mcpu=elbrus-v6, 2026-11-03), included as words.  The encodings of
   these operations are the same on v3 and v5, and the disp
   displacements are relative to the instruction, so the words run
   wherever they are placed:

	p122_disp_ct:
	{ setwd wsz = 0x8, nfx = 0x1, dbl = 0x0 ; return %ctpr3 }
	{ disp %ctpr1, 1f }                  ! the old target
	{ nop 5 }
	{ nop 5 }
	{ disp %ctpr1, 2f ; ct %ctpr1 }      ! the pair
	{ addd,0 0x0, 0x0, %dr0 ; ct %ctpr3 } ! fell through: 0
	1: { addd,0 0x0, 0x1, %dr0 ; ct %ctpr3 } ! old target: 1
	2: { addd,0 0x0, 0x2, %dr0 ; ct %ctpr3 } ! new target: 2

   The movtd form of the pair (which gas accepts) is control-transfer.md's
   `bench.py` hazard `movtd_ct`: the old target on v3 and v5, no transfer
   on v6.

   Build (no .s file, so gen_operations.py's PROBES is not involved):
     /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -O2 foldp122_dispct.c \
        -o foldp122_dispct
   Run on hardware: ./foldp122_dispct
   Output: "disp + ct in one instruction: 0 (fell through)", "1 (the old
   target)" or "2 (the new target)", for 3 calls.

   Results (2026-11-03): "1 (the old target)" in all 3 calls on the 4C
   and on the 8C2, as for movtd + ct.  The 2C3 (v6), where gas allows
   the pair and control-transfer.md, "v6", measured a fall-through, was
   offline: running this there is owed.  */

#include <stdio.h>

long p122_disp_ct (void);

__asm__ ("	.text\n"
	 "	.align	8\n"
	 "	.global	p122_disp_ct\n"
	 "	.type	p122_disp_ct, @function\n"
	 "p122_disp_ct:\n"
	 "	.word	0x0000c012, 0xf0000000, 0x00000000, 0x00000110\n"
	 "	.word	0x00004001, 0x40000007\n"
	 "	.word	0x00000280, 0x00000000\n"
	 "	.word	0x00000280, 0x00000000\n"
	 "	.word	0x00005012, 0xc0000420, 0x40000006, 0x00000000\n"
	 "	.word	0x04001012, 0xc0000c20, 0x11c0c080, 0x00000000\n"
	 "	.word	0x04001012, 0xc0000c20, 0x11c0c180, 0x00000000\n"
	 "	.word	0x04001012, 0xc0000c20, 0x11c0c280, 0x00000000\n"
	 "	.size	p122_disp_ct, .-p122_disp_ct\n");

int
main (void)
{
  static const char *what[] = { "fell through", "the old target",
				"the new target" };
  for (int i = 0; i < 3; i++)
    {
      long r = p122_disp_ct ();
      printf ("disp + ct in one instruction: %ld (%s)\n", r,
	      r >= 0 && r <= 2 ? what[r] : "?");
    }
  return 0;
}
