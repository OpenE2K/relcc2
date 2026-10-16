/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What lcc's debug information gives a debugger for optimized code
   (findings A30): parameters and locals of four functions (two loops,
   an inlined function, a struct argument, a lexical block), read with
   readelf --debug-dump at -O0 to -O4 and -Og (by hand: lcc -g -c, both
   lcc versions), and by MCST's gdb on hardware through
   folddebug_opt.sh (lcc 1.29.16, -O0 and -O2).

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O2 -g tools/abi/probes/folddebug_opt.c -o opt
     gdb -batch -ex 'break sum_sq' -ex run -ex 'info args' \
         -ex 'info locals' -ex 'break folddebug_opt.c:LOOP_LINE' \
         -ex 'info breakpoints' ./opt

   (folddebug_opt.sh runs it with LOOP_LINE filled in.)

   The program prints "r 1150 36 40". Results: folddebug_opt.sh's
   header.  */

#include <stdio.h>

struct pt
{
  long x, y;
};

volatile long sink;
long glob = 7;

static inline long
sq (long v)
{
  long t = v * v;
  return t + 1;
}

__attribute__ ((noinline)) long
sum_sq (const long *a, int n)
{
  long s = 0;
  for (int i = 0; i < n; i++)
    s += sq (a[i]);		/* LOOP_LINE */
  return s;
}

__attribute__ ((noinline)) long
dot (struct pt p, struct pt q)
{
  long d = p.x * q.x + p.y * q.y;
  {
    long e = d + glob;
    sink = e;
  }
  return d;
}

__attribute__ ((noinline)) long
scan (const long *a, int n, long key)
{
  int found = -1;
  for (int j = 0; j < n; j++)
    if (a[j] == key)
      {
	found = j;
	break;
      }
  return found;
}

int
main (void)
{
  long a[10];
  for (int k = 0; k < 10; k++)
    a[k] = 2 * k;
  struct pt p = { 3, 4 }, q = { 2, 3 };
  long r = sum_sq (a, 10);	/* CALL_LINE */
  long d = dot (p, q);
  long f = scan (a, 10, 6);
  printf ("r %ld %ld %ld\n", r, d + sink - glob, f * 10 + 10);
  return 0;
}
