/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What lcc does with conditional loads, FP selects, volatile and atomic
   loads under a condition, system-call asm and tail calls: the code
   behind gcc-port-plan.md's if-conversion, speculation, inline-asm and
   sibling-call notes (PoC findings P86-P88, P91).  Read the assembly;
   only the tail calls are run, by pocplan_tail.c.

     for t in v3.4c v5.8c2 v6.2c3; do
       /opt/mcst/lcc-1.29.16.e2k-$t.linux-6.1/bin/lcc -O3 -S \
         tools/abi/probes/pocplan.c -o pocplan-$t-O3.s
     done
     # also -O2, and lcc 1.31.05 (toolchains/lcc-1.31.05-e2k-v3/lcc)

   Each function is small enough to read its wide instructions by hand.  */

#include <math.h>
#include <stdatomic.h>

/* P86: a select whose arm is a load, under an FP compare.  */

double
sel_once (const double *p, double a, double b, double x)
{
  if (a < b)
    x = *p;
  return x;
}

double
sel_loop (const double *a, const double *b, double t, int n)
{
  double x = 0;
  for (int i = 0; i < n; i++)
    x = (a[i] < t) ? b[i] : x;
  return x;
}

double
sel_isless (const double *a, const double *b, int n)
{
  double x = 0;
  for (int i = 0; i < n; i++)
    x = __builtin_isless (a[i], x) ? b[i] : x;
  return x;
}

/* Linpack's idamax: a rare new maximum under an FP compare.  */
int
idamax (int n, const double *dx)
{
  double dmax = fabs (dx[0]);
  int itemp = 0;
  for (int i = 1; i < n; i++)
    if (fabs (dx[i]) > dmax)
      {
        itemp = i;
        dmax = fabs (dx[i]);
      }
  return itemp;
}

/* P91: c ? *a : *b (GCC 17's phiopt makes one load from a selected
   address).  */
double
sel_ab (const double *a, const double *b, double u, double v)
{
  return u < v ? *a : *b;
}

long
sel_ab_int (const long *a, const long *b, long u, long v)
{
  return u < v ? *a : *b;
}

/* P88: volatile and atomic loads under a condition.  */

long
vol_loop (volatile long *v, int n)
{
  long s = 0;
  for (int i = 0; i < n; i++)
    if (i & 1)
      s += *v;
  return s;
}

long
vol_once (volatile long *v, long c)
{
  long s = 1;
  if (c)
    s = *v;
  return s + 3;
}

long
plain_once (long *v, long c)
{
  long s = 1;
  if (c)
    s = *v;
  return s + 3;
}

long
atomic_once (_Atomic long *v, long c)
{
  long s = 1;
  if (c)
    s = atomic_load_explicit (v, memory_order_relaxed);
  return s + 3;
}

/* P87: a system call through the uapi macro's asm (copied shape:
   arch/e2k/include/uapi/asm/e2k_api.h:121-134).  Where does lcc put
   `return %ctpr3`, and what is %# ?  */

long
sys_getpid (void)
{
  register unsigned long res;
  asm volatile ("{\n"
                "addd,s 0x0, %[sys_num], %%b[0]\n\t"
                "sdisp %%ctpr1, %[trap]\n\t"
                "}\n"
                "call %%ctpr1, wbs = %#\n\t"
                "addd,s 0x0, %%b[0], %[res]"
                : [res] "=r" (res)
                : [trap] "i" (3), [sys_num] "ri" (172UL)
                : "ctpr1", "ctpr2", "ctpr3", "b[0]", "b[1]", "b[2]",
                  "b[3]", "b[4]", "b[5]", "b[6]", "b[7]");
  return res;
}

long
sys_read_twice (int fd, char *buf)
{
  /* buf[0] read before and after a read(2) whose asm lists no
     "memory": does lcc load it again?  */
  long before = buf[0];
  register unsigned long res;
  asm volatile ("{\n"
                "addd,s 0x0, %[sys_num], %%b[0]\n\t"
                "addd,s 0x0, %[a1], %%b[1]\n\t"
                "addd,s 0x0, %[a2], %%b[2]\n\t"
                "addd,s 0x0, 0x1, %%b[3]\n\t"
                "sdisp %%ctpr1, %[trap]\n\t"
                "}\n"
                "call %%ctpr1, wbs = %#\n\t"
                "addd,s 0x0, %%b[0], %[res]"
                : [res] "=r" (res)
                : [trap] "i" (3), [sys_num] "ri" (3UL),
                  [a1] "ri" ((unsigned long) fd),
                  [a2] "ri" ((unsigned long) buf)
                : "ctpr1", "ctpr2", "ctpr3", "b[0]", "b[1]", "b[2]",
                  "b[3]", "b[4]", "b[5]", "b[6]", "b[7]");
  return before + buf[0] + (long) res;
}

/* P91: tail calls.  */

extern long g1 (long);
extern long g10 (long, long, long, long, long, long, long, long, long,
                 long);
extern void use (long *);

long
tail1 (long x)
{
  return g1 (x + 1);
}

long
tail_frame (long x)
{
  long buf[4];
  buf[0] = x;
  use (buf);
  return g1 (buf[1]);
}

long
tail10 (long a, long b, long c, long d, long e, long f, long g, long h,
        long i, long j)
{
  return g10 (j, i, h, g, f, e, d, c, b, a);
}

/* P91: long double inside a leaf that takes and returns double.  */

double
ld_leaf (double a, double b)
{
  return (double) ((long double) a * b + 1.0L);
}
