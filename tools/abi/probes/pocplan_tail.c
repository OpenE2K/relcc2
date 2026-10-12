/* SPDX-License-Identifier: GPL-3.0-or-later */
/* How far below its caller's stack pointer does lcc's tail call leave the
   callee?  Links pocplan.c's tail1, tail_frame and tail10 (compiled
   separately, so that lcc knows nothing of the callees) and prints, for
   each, how many bytes lower the final callee's local sits than when
   the caller calls it directly; then the same through chains of 0, 10
   and 20 mutually recursive tail calls (spec/abi/calling-convention.md,
   "Tail calls"; gcc-port-plan.md D4, "Sibling calls").

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O3 -c tools/abi/probes/pocplan.c -o pocplan.o
     $LCC -O3 -fno-inline tools/abi/probes/pocplan_tail.c pocplan.o \
       -o pocplan_tail -lm
     ./pocplan_tail

   Expected (4C, 8C2, 2C3 and MCST's qemu alike): tail1 16, tail_frame 48,
   tail10 80, chain 0 16, chain 10 176, chain 20 336, that is 16 bytes
   for every tail call of a one-argument function.

   The same happens in GCC's own tests: gcc.dg/sibcall-3.c and
   sibcall-9.c, built with `$LCC -O2 -fno-inline` (lcc ignores `noipa`,
   warning #1097, and without -fno-inline inlines `track`), abort, and
   sibcall-4.c and sibcall-10.c (no arguments) exit 0.  */

#include <stdio.h>

extern long tail1 (long);
extern long tail_frame (long);
extern long tail10 (long, long, long, long, long, long, long, long, long,
                    long);

char *volatile where;

long
g1 (long x)
{
  volatile char c = 0;
  where = (char *) &c;
  return x + c;
}

long
g10 (long a, long b, long c, long d, long e, long f, long g, long h,
     long i, long j)
{
  volatile char k = 0;
  where = (char *) &k;
  return a + b + c + d + e + f + g + h + i + j + k;
}

void
use (long *p)
{
  p[1] = p[0];
}

long chain_b (long);

long
chain_a (long n)
{
  if (n == 0)
    return g1 (0);
  return chain_b (n - 1);
}

long
chain_b (long n)
{
  if (n == 0)
    return g1 (0);
  return chain_a (n - 1);
}

int
main (void)
{
  g1 (0);
  char *d1 = where;
  g10 (0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  char *d10 = where;

  tail1 (0);
  printf ("tail1 %ld\n", (long) (d1 - where));
  tail_frame (0);
  printf ("tail_frame %ld\n", (long) (d1 - where));
  tail10 (0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  printf ("tail10 %ld\n", (long) (d10 - where));
  for (int n = 0; n <= 20; n += 10)
    {
      chain_a (n);
      printf ("chain %d %ld\n", n, (long) (d1 - where));
    }
  return 0;
}
