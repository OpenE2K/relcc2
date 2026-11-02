/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * foldp112_quadspec.c: libquadmath called after feclearexcept.  sinq of
 * a large argument reduces it with __quadmath_rem_pio2q, whose kernel
 * (libquadmath/math/rem_pio2q.c) is double-precision loops.  Built with
 * speculative FP operations (lcc's default -fspec-fp; a GCC port's
 * -mspec-fp), a speculative operation that would raise a flag the
 * program has cleared gives a diagnostic value, and its first
 * non-speculative use is SIGILL ILL_ILLOPN
 * (spec/isa/speculation.md, "Speculative operations";
 * doc/gcc-port-plan.md, 3.3, "Runtime libraries").
 *
 * Run on hardware, without and with an argument (which clears the
 * flags first):
 *
 *   /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -O3 \
 *       tools/abi/probes/foldp112_quadspec.c -lquadmath -lm -o quadspec
 *   tools/setup/e2k-scp quadspec claude@ringo.lan:relcc2/<task>/
 *   tools/setup/e2k-ssh claude@ringo.lan \
 *       'cd relcc2/<task>; ./quadspec; ./quadspec clear; echo $?'
 *
 * For a GCC-built libquadmath, link this file statically against the
 * port's libquadmath.a, with or without its rem_pio2q.c rebuilt by the
 * port with and without -mno-spec-fp (the object goes before the
 * archive).
 *
 * Both runs should print sinq (1e30) = -9.0116901912138058030386429e-02;
 * the cleared run raises only inexact (flags 0x20).
 *
 * 2026-11-03, 4C: the machine's own libquadmath.so.0 (lcc-built, ALT's
 * /lib64) is right with the flags set and dies with SIGILL (exit 132)
 * after feclearexcept.  The proof-of-concept back end's 0.5+41
 * libquadmath.a (built -mno-spec-fp) is right both ways; with only its
 * rem_pio2q.o rebuilt without -mno-spec-fp (62 FP operations ,sm in
 * pipelined loops, none with it) the cleared run is SIGILL too.
 */

#include <fenv.h>
#include <quadmath.h>
#include <stdio.h>

int
main (int argc, char **argv)
{
  volatile __float128 x = 1e30Q;
  if (argc > 1)
    feclearexcept (FE_ALL_EXCEPT);
  __float128 y = sinq (x);
  char buf[64];
  quadmath_snprintf (buf, sizeof buf, "%.25Qe", y);
  printf ("cleared %d: sinq (1e30) = %s, flags %#x\n", argc > 1, buf,
	  fetestexcept (FE_ALL_EXCEPT));
  return 0;
}
