/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * folda44_retext.c: whether a caller extends a narrow integer result
 * again before using it as a 64-bit value (spec/abi/calling-convention.md,
 * "Results" and "Integer promotion").  MCST glibc's clone.S (:84, :93),
 * clone2.S (:91, :100) and longjmp.S (:63, __SYSCALL_OUTPUT) return their
 * int with `adds 0x0, ..., %r0` under `setwd wsz = 0x8` (dbl = 0), which
 * leaves %r0's upper half as it was: clone's first argument, the fn
 * pointer.  lcc's callers extend the result again; a caller that trusts
 * the callee takes that upper half (the proof-of-concept back end's
 * finding A44: glibc's io/tst-getcwd-smallbuff opened
 * /proc/76300094021936/uid_map, 0x456500002130, for PID 0x2130).
 * folda44_retext.s has callees of that kind: each returns its value in
 * %r0's low half only, with the argument's upper half above it.
 *
 *   D=tools/abi/probes
 *   L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
 *   $L -O2 $D/folda44_retext.c $D/folda44_retext.s -o folda44_retext
 *   ./folda44_retext                    # on hardware (also -O0, -O3)
 *   $L -O2 -S $D/folda44_retext.c -o -  # the extension after each call
 *   e2k-linux-gnu-gcc -O2 -S $D/folda44_retext.c   (a GCC port; link the
 *                                       .s it writes with $L as above)
 *
 * One line per caller: the 64-bit value it computed from the result,
 * the value due, and "ok" or "WRONG".
 *
 * 2026-11-03, the 4C (e2k-v3 builds) and the 8C2 (e2k-v5.8c2): lcc
 * 1.29.16 (-O0, -O2, -O3) and 1.31.05 (-O3) all ok: lcc's caller
 * extends from the type's own width (sxt 2 for int, 6 unsigned, 1 short,
 * 4 unsigned char).  The proof-of-concept back end of 2026-11-02 all ok
 * (sxt after the call); its build of 2026-10-05 moved %b[0] with addd and
 * got all five WRONG (4565000000002130, 45650000ffffffff, ...).  Unipro's
 * clang 20.1.8 (-march=elbrus-v3 -O2) extends from 32 bits only (sxt 2
 * for int and short, 6 for unsigned and unsigned char): int and unsigned
 * ok, short 000000001234fff0 and unsigned char 1281 WRONG.
 */

#include <stdio.h>

int dirty_int (long junk);
int dirty_int_neg (long junk);
unsigned dirty_uint (long junk);
short dirty_short (long junk);
unsigned char dirty_uchar (long junk);

__attribute__ ((noinline)) long l_int (long j) { return dirty_int (j); }
__attribute__ ((noinline)) long l_int_neg (long j) { return dirty_int_neg (j); }
__attribute__ ((noinline)) unsigned long l_uint (long j) { return dirty_uint (j); }
__attribute__ ((noinline)) long l_short (long j) { return dirty_short (j); }
__attribute__ ((noinline)) unsigned long l_uchar (long j) { return dirty_uchar (j); }

static int bad;

static void
check (const char *what, unsigned long got, unsigned long want)
{
  printf ("%-12s %016lx  want %016lx  %s\n", what, got, want,
          got == want ? "ok" : "WRONG");
  bad += got != want;
}

int
main (void)
{
  long j = 0x4565000000000000L;
  check ("int", l_int (j), 0x2130);
  check ("int -1", l_int_neg (j), -1L);
  check ("unsigned", l_uint (j), 0x80000001UL);
  check ("short", l_short (j), -16L);
  check ("uchar", l_uchar (j), 0x81);
  printf ("%d wrong\n", bad);
  return 0;
}
