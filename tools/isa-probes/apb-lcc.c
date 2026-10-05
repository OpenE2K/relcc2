/* SPDX-License-Identifier: GPL-3.0-or-later */
/* lcc's array prefetch (APB) loops on hardware (spec/isa/apb.md):
   correct under timer signals, at the edge of unmapped memory, and how
   much faster than the same loops without APB.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O3 -faligned tools/isa-probes/apb-lcc.c -o apb-lcc
     $LCC -O3 -faligned -fno-loop-apb tools/isa-probes/apb-lcc.c -o apb-lcc-noapb
     ./apb-lcc            # PASS/FAIL lines, then ns per element
     taskset -c 5 ./apb-lcc & taskset -c 5 ./apb-lcc & wait
                          # context switches as well

   On v3, lcc uses APB for loads wider than a byte only when it can
   prove or assume alignment (-faligned, -ffast); these loops take plain
   pointers, so build with -faligned.  Check with -S that sumd, suml and
   dot contain fapb.  */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

__attribute__ ((noinline)) long
sumd (const long *a, long n)
{
  long s = 0;
  for (long i = 0; i < n; i++)
    s += a[i];
  return s;
}

/* Two arrays, a stride of 2 and an offset: several areas.  */
__attribute__ ((noinline)) long
suml (const long *a, const long *b, long n)
{
  long s = 0;
  for (long i = 0; i < n; i++)
    s += a[2 * i] ^ b[i + 3];
  return s;
}

__attribute__ ((noinline)) double
dot (const double *a, const double *b, long n)
{
  double s = 0;
  for (long i = 0; i < n; i++)
    s += a[i] * b[i];
  return s;
}

static volatile long ticks;

static void
tick (int s)
{
  (void) s;
  ticks++;
}

static double
now (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}

#define N (1L << 23)

int
main (void)
{
  long *a = aligned_alloc (64, 8 * N), *b = aligned_alloc (64, 8 * N);
  double *x = aligned_alloc (64, 8 * N), *y = aligned_alloc (64, 8 * N);
  for (long i = 0; i < N; i++)
    {
      a[i] = 3 * i + 1;
      b[i] = 0;
      x[i] = 1.0;
      y[i] = 2.0;
    }

  /* 1. Correct under a SIGALRM every 100 us.  */
  signal (SIGALRM, tick);
  struct itimerval it = { { 0, 100 }, { 0, 100 } };
  setitimer (ITIMER_REAL, &it, NULL);
  long bad = 0;
  for (long r = 0; r < 300; r++)
    {
      long m = (1L << 20) - r;
      if (sumd (a, m) != 3 * m * (m - 1) / 2 + m)
        bad++;
      /* a[2i] = 6i + 1, b[] = 0.  */
      if (suml (a, b, m / 2) != 3 * (m / 2) * (m / 2 - 1) + m / 2)
        bad++;
      if (dot (x, y, m) != 2.0 * m)
        bad++;
    }
  it.it_value.tv_usec = it.it_interval.tv_usec = 0;
  setitimer (ITIMER_REAL, &it, NULL);
  printf ("%s: %ld wrong of 900 results, %ld timer signals\n",
          bad ? "FAIL" : "PASS", bad, ticks);

  /* 2. Arrays that end at an unmapped page: the prefetch runs ahead of
     the loop but must not fault.  */
  long pg = sysconf (_SC_PAGESIZE);
  char *m = mmap (0, 6 * pg, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  mprotect (m + 5 * pg, pg, PROT_NONE);
  long *end = (long *) (m + 5 * pg), cnt = 4 * pg / 8;
  for (long i = 0; i < cnt; i++)
    end[i - cnt] = 3 * i + 1;
  bad = 0;
  for (long n = 1; n <= cnt; n += n < 40 ? 1 : 97)
    if (sumd (end - n, n) != 3 * (cnt - n) * n + 3 * n * (n - 1) / 2 + n)
      bad++;
  printf ("%s: guard page, %ld wrong\n", bad ? "FAIL" : "PASS", bad);

  /* 3. Speed from L1, L2 and memory.  */
  long sizes[] = { 2048, 32768, N };
  for (int k = 0; k < 3; k++)
    {
      long n = sizes[k], reps = (1L << 26) / n;
      volatile long s = 0;
      volatile double d = 0;
      double t0 = now ();
      for (long r = 0; r < reps; r++)
        s += sumd (a, n);
      double t1 = now ();
      for (long r = 0; r < reps; r++)
        d += dot (x, y, n);
      double t2 = now ();
      printf ("%6ld KiB: sumd %.2f ns/element, dot %.2f ns/element\n",
              n * 8 / 1024, (t1 - t0) / (reps * n) * 1e9,
              (t2 - t1) / (reps * n) * 1e9);
    }
  return 0;
}
