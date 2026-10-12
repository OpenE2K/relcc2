/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Driver for bapent.py: %clkr cycles per call (median of 101, less the
   cost of an empty pair of reads) of lcc's -O3 array-prefetch loops from
   tools/isa-score/validate/kernels.c, built with N more cycles between
   their bap and the loop.  bapent.py builds one binary per N; run them
   on the machine:

     for n in 0 1 2 3 4 5 6 7 8; do taskset -c 0 ./bapent$n +$n; done

   Each prints its argument, then the cycles of sum, saxpy and copy at
   n = 512 and 2048 elements (spec/isa/apb.md, "v6").  */

#include <stdio.h>
#include <stdlib.h>

#define N 512
unsigned long rdclk (void);
long k_sum_O3 (const long *, long);
void k_saxpy_O3 (double *, const double *, double, long);
void k_copy_O3 (int *, const int *, long);

static long a[4 * N];
static double x[4 * N], y[4 * N];
static int ia[4 * N], ib[4 * N];
static unsigned long t[101];

static int
cmp (const void *p, const void *q)
{
  unsigned long u = *(const unsigned long *) p;
  unsigned long v = *(const unsigned long *) q;
  return u < v ? -1 : u > v;
}

#define T(name, call)                                                   \
  do                                                                    \
    {                                                                   \
      for (int c = 0; c < 101; c++)                                     \
        {                                                               \
          unsigned long t0 = rdclk ();                                  \
          call;                                                         \
          t[c] = rdclk () - t0;                                         \
        }                                                               \
      qsort (t, 101, sizeof *t, cmp);                                   \
      printf (" %s %lu", name, t[50] - clk);                            \
    }                                                                   \
  while (0)

int
main (int argc, char **argv)
{
  for (int i = 0; i < 4 * N; i++)
    {
      a[i] = i * 7;
      x[i] = i * 0.25;
      y[i] = 1.0 / (i + 1);
      ia[i] = i;
    }
  unsigned long clk = ~0UL;
  for (int c = 0; c < 101; c++)
    {
      unsigned long t0 = rdclk (), t1 = rdclk ();
      if (t1 - t0 < clk)
        clk = t1 - t0;
    }
  for (long n = N; n <= 4 * N; n *= 4)
    {
      printf ("%s n %ld:", argc > 1 ? argv[1] : "", n);
      T ("sum", k_sum_O3 (a, n));
      T ("saxpy", k_saxpy_O3 (y, x, 0.5, n));
      T ("copy", k_copy_O3 (ib, ia, n));
      printf ("\n");
    }
  return 0;
}
