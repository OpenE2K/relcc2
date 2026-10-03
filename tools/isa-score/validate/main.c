/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Driver for validate.py: calls every kernel directly CALLS times and
   prints the median %clkr cycles of one call, minus the cost of reading
   the clock.  The measured cycles include the call and the return
   (k_empty shows how much).  With the argument "count" it calls each
   kernel once, for count.py to single-step under gdb.  */

#include <stdio.h>
#include <stdlib.h>

#define N 512
#define CALLS 41

unsigned long rdclk (void);
struct node { struct node *next; long v; };

#define PROTOS(l) \
  long k_empty_##l (const long *, long); \
  long k_sum_##l (const long *, long); \
  double k_dot_##l (const double *, const double *, long); \
  void k_saxpy_##l (double *, const double *, double, long); \
  unsigned k_hash_##l (const unsigned char *, long); \
  long k_branchy_##l (const int *, long); \
  double k_poly_##l (const double *, long); \
  long k_list_##l (const struct node *); \
  void k_copy_##l (int *, const int *, long);
PROTOS (O0) PROTOS (O2) PROTOS (O3)

static long a[N];
static double x[N], y[N];
static unsigned char s[4 * N];
static int ia[N], ib[N];
static struct node list[N];
static unsigned long t[CALLS], clk;
static int calls = CALLS;

static int
cmp (const void *p, const void *q)
{
  unsigned long u = *(const unsigned long *) p, v = *(const unsigned long *) q;
  return u < v ? -1 : u > v;
}

static unsigned long
median (void)
{
  qsort (t, CALLS, sizeof *t, cmp);
  return t[CALLS / 2];
}

#define TIME(name, level, call) \
  do \
    { \
      for (int c = 0; c < calls; c++) \
        { \
          unsigned long t0 = rdclk (); \
          call; \
          t[c] = rdclk () - t0; \
        } \
      if (calls == CALLS) \
        printf ("%s_%s %s %lu\n", #name, #level, #level, median () - clk); \
    } \
  while (0)

#define LEVEL(l) \
  TIME (k_empty, l, k_empty_##l (a, N)); \
  TIME (k_sum, l, k_sum_##l (a, N)); \
  TIME (k_dot, l, k_dot_##l (x, y, N)); \
  TIME (k_saxpy, l, k_saxpy_##l (y, x, 0.5, N)); \
  TIME (k_hash, l, k_hash_##l (s, 4 * N)); \
  TIME (k_branchy, l, k_branchy_##l (ia, N)); \
  TIME (k_poly, l, k_poly_##l (x, N)); \
  TIME (k_list, l, k_list_##l (list)); \
  TIME (k_copy, l, k_copy_##l (ib, ia, N));

int
main (int argc, char **argv)
{
  if (argc > 1)
    calls = 1;
  for (int i = 0; i < N; i++)
    {
      a[i] = i * 7;
      x[i] = i * 0.25;
      y[i] = 1.0 / (i + 1);
      ia[i] = i * 13 - 400;
      list[i].next = i + 1 < N ? &list[i + 1] : 0;
      list[i].v = i;
    }
  for (int i = 0; i < 4 * N; i++)
    s[i] = i * 31;
  for (int c = 0; c < CALLS; c++)
    {
      unsigned long t0 = rdclk ();
      t[c] = rdclk () - t0;
    }
  clk = median ();
  LEVEL (O0) LEVEL (O2) LEVEL (O3)
  printf ("calls %d\n", CALLS);
  return 0;
}
