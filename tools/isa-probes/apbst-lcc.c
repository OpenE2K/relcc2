/* SPDX-License-Identifier: GPL-3.0-or-later */
/* lcc's own loops that store into an element a later iteration reads:
   a[i + d] = a[i] * 0.5 + 1 for d = 1 to 40.  lcc 1.29.16 -O3 streams
   them through array prefetch from d = 5 on v5 and v6, with a small
   %lsr.ldovl (spec/isa/loops.md, "Stores into a streamed array"); this checks
   every result against a reference that reads through a volatile.
   apbst.c measures the hardware rule behind it.

     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc  # or v6.2c3, v3
     $L -O3 tools/isa-probes/apbst-lcc.c -o apbst-lcc
     ./apbst-lcc [ROUNDS]          # on hardware; exit status 1 if wrong
     $L -O3 -S tools/isa-probes/apbst-lcc.c   # the %lsr values  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define H(d)                                                            \
  __attribute__ ((noinline)) void h##d (double *a, long n)              \
  {                                                                     \
    for (long i = 0; i < n; i++)                                        \
      a[i + d] = a[i] * 0.5 + 1.0;                                      \
  }
H (1) H (2) H (3) H (4) H (5) H (6) H (7) H (8) H (10) H (12) H (14)
H (16) H (20) H (24) H (32) H (40)

typedef void (*fn) (double *, long);
static const fn F[] = { h1, h2, h3, h4, h5, h6, h7, h8, h10, h12, h14,
                        h16, h20, h24, h32, h40 };
static const int D[] = { 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24,
                         32, 40 };

__attribute__ ((noinline)) static void
ref (double *a, long n, int d)
{
  for (long i = 0; i < n; i++)
    {
      volatile double x = a[i];
      a[i + d] = x * 0.5 + 1.0;
    }
}

int
main (int argc, char **argv)
{
  static const long N[] = { 8, 30, 64, 100, 257, 1000, 4096, 20000 };
  long rounds = argc > 1 ? atol (argv[1]) : 20, bad = 0, tot = 0;
  double *a = malloc ((20000 + 64) * 8), *b = malloc ((20000 + 64) * 8);
  for (unsigned k = 0; k < sizeof D / sizeof *D; k++)
    {
      long kb = 0, kt = 0;
      for (long r = 0; r < rounds; r++)
        for (unsigned j = 0; j < sizeof N / sizeof *N; j++)
          {
            long n = N[j];
            for (long i = 0; i < n + 64; i++)
              a[i] = b[i] = (double) ((i * 7919 + r) % 1000);
            F[k] (a, n);
            ref (b, n, D[k]);
            kt++;
            if (memcmp (a, b, (n + D[k]) * 8))
              {
                long i = 0;
                while (a[i] == b[i])
                  i++;
                if (!kb++)
                  printf ("  d=%d n=%ld first difference at %ld: %g, not %g\n",
                          D[k], n, i, a[i], b[i]);
              }
          }
      printf ("d=%-3d wrong %ld of %ld\n", D[k], kb, kt);
      bad += kb;
      tot += kt;
    }
  printf ("total wrong %ld of %ld\n", bad, tot);
  return bad != 0;
}
