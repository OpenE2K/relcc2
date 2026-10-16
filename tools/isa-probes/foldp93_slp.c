/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Does lcc pair straight-line scalar code into 128-bit operations
   (basic-block SLP)?  The proof-of-concept back end's P93 said it
   doesn't; spec/isa/operations-v5.yaml, "How lcc uses them".  Read
   with -S, not run:

     for t in v5.8c2 v6.2c3; do
       /opt/mcst/lcc-1.29.16.e2k-$t.linux-6.1/bin/lcc -O3 -S \
         tools/isa-probes/foldp93_slp.c -o foldp93_slp-$t.s
     done

   (also -O3 -faligned, -O3 -ffast, -O4, and lcc 1.31.05).  Look for qp*
   operations, qppackdl, qpswitchd, stqp and ldqp in each function.

   Results (lcc 1.29.16 -O3 and -O4 and 1.31.05 -O3, v5.8c2 and v6.2c3
   alike unless noted, 2026-10-16): add2, addl, addi, addc, addf, mulg
   and scale5 are paired into qp operations (v5: two ldd + qppackdl an
   operand, v6: one ldqp); the results go out with stqp to the aligned
   globals, as qpswitchd + two std through add2's pointers (add2a's
   too unless -faligned), on v6 as well; scale5 is two qpfmuld and an
   fmuld, two a qpfmuld and a qpfaddd (no qpfmul_addd); addcl stays two
   64-bit paddb.  noresr, cmul, blk, blkr and mm4p stay scalar (fmuld
   with faddd/fsubd; on v5 the products of blkr, blkg and mm4 fused
   into fmul_subd/fmul_addd), blkg and mm4 too except their results,
   packed with qppackdl and stored with stqp.  mc16 and mc16d copy 16 bytes
   as ldd + ldd + qppackdl (v6 ldqp), qpswitchd and sixteen stb (mc16d
   with -faligned: two ldd and two std).  pr reads its three streams
   with movaqp from fmt 5 areas (mrng 16), one qpfmul_subd a step, and
   stores the result as two staad, the low half and qpswitchd's at
   + 8; with -faligned, and in gl over global rows, the loop is
   versioned on the address (cmpandedb, bit 3) between a staaqp loop
   and one of movad and staad.  */

#include <string.h>

/* Two doubles of a structure, the pointers restrict.  */
struct v2 { double x, y; };
void
add2 (struct v2 *restrict r, const struct v2 *restrict a,
      const struct v2 *restrict b)
{
  r->x = a->x + b->x;
  r->y = a->y + b->y;
}

/* The same with a 16-byte-aligned type.  */
typedef struct { double x, y; } __attribute__ ((aligned (16))) v2a;
void
add2a (v2a *restrict r, const v2a *restrict a, const v2a *restrict b)
{
  r->x = a->x + b->x;
  r->y = a->y + b->y;
}

/* Aligned global arrays: doubles, floats, longs, ints and bytes.  */
#define A16 __attribute__ ((aligned (16)))
double ga[4] A16, gb[4] A16, gr[4] A16;
float fa[8] A16, fb[8] A16, fr[8] A16;
long la[2] A16, lb[2] A16, lr[2] A16;
int ia[4] A16, ib[4] A16, ir[4] A16;
unsigned char ca[16] A16, cb[16] A16, cr[16] A16;

void
mulg (void)
{
  gr[0] = ga[0] * gb[0]; gr[1] = ga[1] * gb[1];
  gr[2] = ga[2] * gb[2]; gr[3] = ga[3] * gb[3];
}

void
addf (void)
{
  for (int i = 0; i < 8; i++)   /* fully unrolled */
    fr[i] = fa[i] + fb[i];
}

void
addl (void)
{
  lr[0] = la[0] + lb[0];
  lr[1] = la[1] + lb[1];
}

void
addi (void)
{
  ir[0] = ia[0] + ib[0]; ir[1] = ia[1] + ib[1];
  ir[2] = ia[2] + ib[2]; ir[3] = ia[3] + ib[3];
}

/* Sixteen byte adds written out, and as a loop lcc unrolls fully.  */
void
addc (void)
{
  cr[0] = ca[0] + cb[0]; cr[1] = ca[1] + cb[1];
  cr[2] = ca[2] + cb[2]; cr[3] = ca[3] + cb[3];
  cr[4] = ca[4] + cb[4]; cr[5] = ca[5] + cb[5];
  cr[6] = ca[6] + cb[6]; cr[7] = ca[7] + cb[7];
  cr[8] = ca[8] + cb[8]; cr[9] = ca[9] + cb[9];
  cr[10] = ca[10] + cb[10]; cr[11] = ca[11] + cb[11];
  cr[12] = ca[12] + cb[12]; cr[13] = ca[13] + cb[13];
  cr[14] = ca[14] + cb[14]; cr[15] = ca[15] + cb[15];
}

void
addcl (void)
{
  for (int i = 0; i < 16; i++)
    cr[i] = ca[i] + cb[i];
}

/* Five doubles scaled by one value: an odd group.  */
void
scale5 (double *restrict r, const double *restrict a, double s)
{
  r[0] = a[0] * s; r[1] = a[1] * s; r[2] = a[2] * s;
  r[3] = a[3] * s; r[4] = a[4] * s;
}

/* A multiply and an add per lane.  */
void
two (double *restrict r, const double *restrict a,
     const double *restrict b, double s)
{
  r[0] = a[0] * s + b[0];
  r[1] = a[1] * s + b[1];
}

/* Without restrict: the store may change what the next load reads.  */
void
noresr (double *r, const double *a, const double *b)
{
  r[0] = a[0] + b[0];
  r[1] = a[1] + b[1];
}

/* A complex multiply: two lanes that do different things.  */
typedef struct { double re, im; } cplx;
void
cmul (cplx *restrict r, const cplx *a, const cplx *b)
{
  r->re = a->re * b->re - a->im * b->im;
  r->im = a->re * b->im + a->im * b->re;
}

/* A 5x5 block update, c -= a * b, as in a block-tridiagonal solver
   (NPB BT's matmul_sub has this shape), through pointers and on
   aligned globals.  */
void
blk (double a[5][5], double b[5][5], double c[5][5])
{
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 5; j++)
      c[i][j] = c[i][j] - a[i][0] * b[0][j] - a[i][1] * b[1][j]
                - a[i][2] * b[2][j] - a[i][3] * b[3][j] - a[i][4] * b[4][j];
}

void
blkr (double (*restrict a)[5], double (*restrict b)[5],
      double (*restrict c)[5])
{
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 5; j++)
      c[i][j] = c[i][j] - a[i][0] * b[0][j] - a[i][1] * b[1][j]
                - a[i][2] * b[2][j] - a[i][3] * b[3][j] - a[i][4] * b[4][j];
}

double A[5][5] A16, B[5][5] A16, C[5][5] A16;
void
blkg (void)
{
  blk (A, B, C);
}

/* A 4x4 product, on aligned globals and through restrict pointers.  */
double P[4][4] A16, Q[4][4] A16, R[4][4] A16;
void
mm4 (void)
{
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      R[i][j] = P[i][0] * Q[0][j] + P[i][1] * Q[1][j]
                + P[i][2] * Q[2][j] + P[i][3] * Q[3][j];
}

void
mm4p (double (*restrict r)[4], double (*restrict p)[4],
      double (*restrict q)[4])
{
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      r[i][j] = p[i][0] * q[0][j] + p[i][1] * q[1][j]
                + p[i][2] * q[2][j] + p[i][3] * q[3][j];
}

/* A 16-byte copy of unknown alignment, through void * and double *.  */
void
mc16 (void *d, const void *s)
{
  memcpy (d, s, 16);
}

void
mc16d (double *d, const double *s)
{
  memcpy (d, s, 16);
}

/* A multiply-subtract stream: through restrict pointers of unknown
   alignment, and on rows of global arrays (NPB SP's back substitution
   has this shape).  */
void
pr (double *restrict x, const double *restrict y,
    const double *restrict z, int n)
{
  for (int k = 0; k < n; k++)
    x[k] = x[k] - y[k] * z[k];
}

double X[5][65][65], Y[5][65][65];
int nk;
void
gl (int m, int i)
{
  for (int k = 1; k < nk; k++)
    X[m][i][k] = X[m][i][k] - Y[m][i][k] * X[m][i + 1][k];
}
