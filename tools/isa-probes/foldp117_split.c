/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Groups that must stay in one wide instruction when nothing bundles
   (recheck of docs/spec-poc-findings.md P117; doc/gcc-port-plan.md M0
   and section 2; spec/isa/loops.md, "Semantics"; spec/abi/elf.md,
   "Literal fields").  The kernels are top-level asm in this file.

   - p117_same (N): a counted loop, %lsr = N with vlc, whose body is one
     instruction of its own and whose last instruction holds `alc alcf=1,
     alct=1` with `ct %ctpr1 ? %NOT_LOOP_END`, as loops.md gives it;
     returns the number of iterations.
   - p117_split (N): the same with `alc` on a line of its own just before
     the `ct`, which is what gas makes of operations printed one per line
     outside braces: two instructions.
   - p117_split2 (N): `alc`, an empty instruction, then the `ct`.
   - p117_got_same / p117_got_split: `rrd %ip` and an `addd` of the
     _GLOBAL_OFFSET_TABLE_ literal (PC-relative, R_E2K_64_PC_LIT) in one
     instruction and in two; return the address computed.

   gas (2026-11-03, toolchains/binutils, -mcpu=elbrus-v3) assembles all of
   these without a message, while it rejects a split predicate-logic
   chain ("undefined @p0 used as an incoming parameter for CLP0") and an
   `rbranch ? %dt_alN` away from its speculative operation.  It also
   splits a DAM check or a wait-unlock store from its `rbranch` silently
   (spec/isa/speculation.md: same instruction; spec/abi/atomics.md:
   %MLOCK holds only in the store's instruction).

   Build (any lcc; no .s file, so gen_operations.py's PROBES is not
   involved):
     /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -O2 foldp117_split.c \
        -o foldp117_split
   Run on hardware: ./foldp117_split
   Output: the iterations of each loop kernel for N = 1, 2, 5 and 1024,
   then both GOT addresses and the readelf-able one (&_GLOBAL_OFFSET_TABLE_
   taken by the C compiler).

   Results (2026-11-03, the same on the 4C and the 8C2): alc with ct
   runs N iterations (1, 2, 5, 1024); alc 1 or 2 instructions before the
   ct runs 1, 1, 4, 1023: one iteration short from N = 2 on, with no
   message.  The GOT address from the split pair is 16 bytes low (the
   size of the rrd instruction); the paired one is &_GLOBAL_OFFSET_TABLE_.
   The 2C3 was offline: owed.  */

#include <stdio.h>

long p117_same (long n);
long p117_split (long n);
long p117_split2 (long n);
long p117_got_same (void);
long p117_got_split (void);

#define LOOP(NAME, ALC_BEFORE, GAP)					\
  "	.global	" #NAME "\n"						\
  "	.type	" #NAME ", @function\n"					\
  "	.align	8\n"							\
  #NAME ":\n"								\
  "	{\n"								\
  "	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0\n"			\
  "	  return	%ctpr3\n"					\
  "	}\n"								\
  "	{\n"								\
  "	  addd,0	0x0, 0x0, %dr2\n"				\
  "	  ord,1	%dr0, _f64,_lts0 0x2000000000, %dr1\n"			\
  "	  disp	%ctpr1, 1f\n"						\
  "	}\n"								\
  "	{\n"								\
  "	  rwd,0	%dr1, %lsr\n"						\
  "	}\n"								\
  "	{\n"								\
  "	  nop 5\n"							\
  "	}\n"								\
  "1:\n"								\
  "	{\n"								\
  "	  addd,0	%dr2, 0x1, %dr2\n"				\
  "	}\n"								\
  ALC_BEFORE								\
  GAP									\
  "	{\n"								\
  "	  " ALC_SAME "\n"						\
  "	  ct	%ctpr1 ? %NOT_LOOP_END\n"				\
  "	}\n"								\
  "	{\n"								\
  "	  nop 2\n"							\
  "	}\n"								\
  "	{\n"								\
  "	  addd,0	%dr2, 0x0, %dr0\n"				\
  "	  ct	%ctpr3\n"						\
  "	}\n"								\
  "	.size	" #NAME ", .-" #NAME "\n"

__asm__ ("	.ignore	ld_st_style\n"
	 "	.ignore	strict_delay\n"
	 "	.text\n"
#define ALC_SAME "alc	alcf=1, alct=1"
	 LOOP (p117_same, "", "")
#undef ALC_SAME
#define ALC_SAME ""
	 LOOP (p117_split, "	{\n	  alc	alcf=1, alct=1\n	}\n", "")
	 LOOP (p117_split2, "	{\n	  alc	alcf=1, alct=1\n	}\n",
	       "	{\n	  nop 0\n	}\n")
#undef ALC_SAME
	 "	.global	p117_got_same\n"
	 "	.type	p117_got_same, @function\n"
	 "	.align	8\n"
	 "p117_got_same:\n"
	 "	{\n"
	 "	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0\n"
	 "	  return	%ctpr3\n"
	 "	}\n"
	 "	{\n"
	 "	  rrd,0	%ip, %dr7\n"
	 "	  addd,1	0x0, _f64,_lts0 _GLOBAL_OFFSET_TABLE_, %dr6\n"
	 "	}\n"
	 "	{\n"
	 "	  nop 2\n"
	 "	}\n"
	 "	{\n"
	 "	  addd,0	%dr7, %dr6, %dr0\n"
	 "	}\n"
	 "	{\n"
	 "	  nop 5\n"
	 "	}\n"
	 "	{\n"
	 "	  ct	%ctpr3\n"
	 "	}\n"
	 "	.size	p117_got_same, .-p117_got_same\n"
	 "	.global	p117_got_split\n"
	 "	.type	p117_got_split, @function\n"
	 "	.align	8\n"
	 "p117_got_split:\n"
	 "	{\n"
	 "	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0\n"
	 "	  return	%ctpr3\n"
	 "	}\n"
	 "	{\n"
	 "	  rrd,0	%ip, %dr7\n"
	 "	}\n"
	 "	{\n"
	 "	  addd,1	0x0, _f64,_lts0 _GLOBAL_OFFSET_TABLE_, %dr6\n"
	 "	}\n"
	 "	{\n"
	 "	  nop 2\n"
	 "	}\n"
	 "	{\n"
	 "	  addd,0	%dr7, %dr6, %dr0\n"
	 "	}\n"
	 "	{\n"
	 "	  nop 5\n"
	 "	}\n"
	 "	{\n"
	 "	  ct	%ctpr3\n"
	 "	}\n"
	 "	.size	p117_got_split, .-p117_got_split\n");

extern char _GLOBAL_OFFSET_TABLE_[];

int
main (void)
{
  static const long ns[] = { 1, 2, 5, 1024 };
  for (unsigned i = 0; i < sizeof ns / sizeof ns[0]; i++)
    printf ("N %4ld: alc with ct %4ld, alc 1 instruction before %4ld,"
	    " 2 before %4ld\n", ns[i], p117_same (ns[i]), p117_split (ns[i]),
	    p117_split2 (ns[i]));
  long a = p117_got_same (), b = p117_got_split ();
  printf ("GOT: rrd %%ip with the literal %#lx, apart %#lx (%+ld),"
	  " &_GLOBAL_OFFSET_TABLE_ %#lx\n", a, b, b - a,
	  (long) _GLOBAL_OFFSET_TABLE_);
  return 0;
}
