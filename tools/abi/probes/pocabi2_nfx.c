/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Which functions with a long double get setwd nfx = 0 (registers.md,
   "nfx"; pocabi2.c measures what nfx = 1 loses).  Compile to assembly
   and read each function's setwd:

     for t in v3 v5.8c2 v6.2c3; do
       /opt/mcst/lcc-1.29.16.e2k-$t.linux-6.1/bin/lcc -O3 -S \
         tools/abi/probes/pocabi2_nfx.c -o - | grep -E '^[a-z0-9_]+:|setwd'
     done
     toolchains/unipro/bin/clang --target=e2k64-linux -march=elbrus-v3 -O2 \
       -S tools/abi/probes/pocabi2_nfx.c -o - | grep -E '^[a-z0-9_]+:|setwd'

   lcc 1.29.16 -O3 (all three targets alike, and 1.31.05): nfx = 0 in the
   functions that compute with an 80-bit value (leaf ... callpass), nfx = 1
   in those that only move the two 64-bit parts (cp ... tail), except
   pass2_after: a move-only function that follows one with nfx = 0 in the
   file gets nfx = 0 as well (from -O1 on; a function without a long
   double in a register between them, such as cp or an int function, stops
   it).  Unipro 20.1.8: nfx = 0 in the computing functions only.  */

extern long double ext (long double);
extern void ext2 (long double);
extern long double ext3 (void);
extern void use (double);

/* Only moves of the two parts (ldd/ldh, std/sth, register copies).  */
void cp (long double *a, long double *b) { *a = *b; }
long double pass (long double x) { return x; }
long double pass2 (long double x, long double y) { (void) x; return y; }
void cp2 (long double *a, long double b) { *a = b; }
void cp3 (long double *a, long double b) { use (1.0); *a = b; }
void callv (long double *p) { ext2 (*p); }
long double callr (void) { return ext3 (); }
long double callr1 (void) { long double r = ext3 (); use (1.0); return r; }
long double callcp (long double *p) { return ext (*p); }
long double tail (long double x) { return ext (x); }

/* Arithmetic, compares, conversions: an extended register.  */
long double leaf (long double a, long double b) { return a * b + a; }
double leaf2 (double a, long double b) { return (double) (b * b + a); }
void loop (long double *x, int n)
{
  for (int i = 0; i < n; i++)
    x[i] = x[i] * x[i] + 1.0L;
}
int cmp (long double a, long double b) { return a < b; }
long double sel (int c, long double x, long double y) { return c ? x : y; }
int isnz (long double x) { return x != 0; }
void cvt (long double *p, int i) { *p = i; }
void st (long double *p, double d) { *p = d; }
double callr2 (void) { return (double) ext3 (); }
long double callpass (long double x) { return ext (x) + 1; }

/* pass2 again, right after a function with nfx = 0.  */
long double pass2_after (long double x, long double y) { (void) x; return y; }
