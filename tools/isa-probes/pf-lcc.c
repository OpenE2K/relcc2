/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A plain stream sum over 256 MiB as lcc compiles it: with array
   prefetch (APB, its default at -O3), without (-fno-loop-apb), and
   without APB but with __builtin_prefetch PF bytes ahead (MAS 0x20 for
   locality 3, 0x40 for 1, 0x60 for 0; pf.c has the hand-written loops).
   Cycles per 32-byte line (%clkr; spec/isa/timing.md, "Memory").

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $LCC -O3 tools/isa-probes/pf-lcc.c -o pf-apb
     $LCC -O3 -fno-loop-apb tools/isa-probes/pf-lcc.c -o pf-noapb
     $LCC -O3 -fno-loop-apb -DPF=2048 tools/isa-probes/pf-lcc.c -o pf-pf3
     $LCC -O3 -fno-loop-apb -DPF=2048 -DLOC=0 tools/isa-probes/pf-lcc.c \
       -o pf-pf0  */

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#ifndef LOC
#define LOC 3
#endif

static long
clkr (void)
{
  long x;
  __asm__ volatile ("rrd %%clkr, %0" : "=r" (x));
  return x;
}

static unsigned long __attribute__ ((noinline))
sum (const unsigned long *a, long n)
{
  unsigned long s = 0;
  for (long i = 0; i < n; i++)
    {
#ifdef PF
      __builtin_prefetch ((const char *) &a[i] + PF, 0, LOC);
#endif
      s += a[i];
    }
  return s;
}

int
main (void)
{
  const long bytes = 256L << 20, n = bytes / 8 - 4096;
  unsigned long *a = mmap (NULL, bytes, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memset (a, 1, bytes);
  for (int rep = 0; rep < 3; rep++)
    {
      long t = clkr ();
      unsigned long s = sum (a, n);
      t = clkr () - t;
      printf ("%.1f cycles per 32-byte line (%lu)\n",
              (double) t / (n / 4), s & 1);
    }
  return 0;
}
