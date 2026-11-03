/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * foldp140_fpsign.c: how a compiler flips or clears the sign of a double
 * or a float (-x, fabs, copysign): with an integer xor/and (xord, andd)
 * or a packed one (pxord, pandd, pandnd), by where the value comes from
 * and where it goes.  An FP result reaches pxord in 4 cycles in its
 * cluster and xord in 6, and both give it to an FP operation in 1 there
 * (spec/isa/timing.md, "Latencies"); a packed result reaches an integer
 * operation only after 4.  doc/gcc-port-plan.md D8, "Sign operations".
 *
 * Compile only; read with -S and list each function's sign operations:
 *
 *   /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -O3 -S \
 *       tools/abi/probes/foldp140_fpsign.c -o - |
 *     awk '/^[a-z_0-9]+:/ { f = $1 }
 *          /(p?xor|p?and|p?andn|p?or)[sd],/ { print f, $1, $3 }'
 *
 * (also e2k-v5.8c2, e2k-v6.2c3, and a GCC in place of lcc).
 *
 * 2026-11-03: lcc 1.29.16 -O3 (v3, v5.8c2, v6.2c3 alike) uses pxord for
 * -x and pandd with 0x7fffffffffffffff for fabs where an FP operation
 * computes the double and FP operations read it (ff, ff2, cmp, cvt,
 * nar, div, fma: add, multiply, compare, conversion, divide), and xord
 * and andd where it is loaded (mm, lf), an argument (af), or goes to a
 * store (fs), the return value (fr) or an integer operation (fi).  For
 * a float it always uses xors and ands, also between FP operations
 * (ns_ff, as_ff).  copysign is andd, andd and ord (cs).
 */
#include <math.h>
#include <string.h>

/* An FP result, flipped, into an FP operation.  */
double nd_ff (double a, double b, double c) { return -(a + b) * c; }
double ad_ff (double a, double b, double c) { return fabs (a + b) * c; }
float ns_ff (float a, float b, float c) { return -(a + b) * c; }
float as_ff (float a, float b, float c) { return fabsf (a + b) * c; }
double nd_ff2 (double a, double b, double c, double d)
{
  double t = -(a * b);
  return t * c + t * d;
}

/* ... into a compare, a conversion to an integer and to a float, a
   divide, a combined multiply-add.  */
int cmp_f (double a, double b, double c) { return fabs (a + b) < c; }
long cvt_f (double a, double b) { return (long) -(a + b); }
float nar_f (double a, double b) { return (float) -(a + b); }
double div_f (double a, double b, double c) { return c / -(a + b); }
double fma_f (double a, double b, double c, double d)
{
  return -(a + b) * c + d;
}

/* An FP result, flipped, into a store, the return value, an integer
   operation.  */
void nd_fs (double a, double b, double *q) { *q = -(a + b); }
double nd_fr (double a, double b) { return -(a + b); }
long nd_fi (double a, double b)
{
  double d = -(a + b);
  long l;
  memcpy (&l, &d, sizeof l);
  return l + 1;
}

/* A loaded value or an argument, flipped.  */
void nd_mm (double *p, double *q) { *q = -*p; }
void ad_mm (double *p, double *q) { *q = fabs (*p); }
double nd_lf (double *p, double c) { return -*p * c; }
double nd_af (double a, double c) { return -a * c; }

double cs (double a, double b, double c) { return copysign (a + b, c); }
