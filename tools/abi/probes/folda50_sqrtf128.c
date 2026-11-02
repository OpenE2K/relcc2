/* SPDX-License-Identifier: GPL-3.0-or-later */
/* MCST glibc's sqrtf128 and the _Float128 functions built on it
   (spec/abi/registers.md, "MCST glibc's FP bugs";
   docs/spec-poc-findings.md A50).

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   # or v5.8c2
     $L -O3 tools/abi/probes/folda50_sqrtf128.c -lm -o sqrtf128
     $L -O3 -static tools/abi/probes/folda50_sqrtf128.c -lm \
         -o sqrtf128-static
     ./sqrtf128                 # on hardware
     gcc -O2 tools/abi/probes/folda50_sqrtf128.c -lm -o sqrtf128-x86
                                # the reference: x86-64 glibc >= 2.27

   Each line is CALL -> the result's 32 hex digits (sign and exponent
   first) and its value rounded to double; "ok" or "WRONG" compares the
   bits with the reference value the line carries (x86-64 glibc 2.43's
   result for the same call; any NaN matches a NaN).  The functions are called through
   volatile pointers, with the flags left set (MCST's libm traps after
   feclearexcept; registers.md).

   OpenE2K glibc's sysdeps/e2k/fpu/e_sqrtf128.c:11-15 is
   __ieee754_sqrtf128 (a) { return a; } ("This fake implementation just
   lets one formally make sysdeps/ieee754/float128 for e2k").  The
   float128 build of sysdeps/ieee754/ldbl-128 reaches it through sqrtl
   (float128_private.h:192, 371): e_hypotl.c:46, e_asinl.c:237,
   e_acosl.c:274, e_acoshl.c:55-58, s_asinhl.c:67-72, e_powl.c:238,
   e_gammal_r.c:111, e_j0l.c, e_j1l.c, e_jnl.c, s_fsqrtl.c and
   s_dsqrtl.c, and math/s_csqrt_template.c and k_casinh_template.c.

   2026-11-03, lcc 1.29.16 -O3: the 4C (ALT's libm; the v3 static
   libm.a; lcc 1.31.05's v3 libm.a, glibc 2.41) and the 8C2 (its libm and
   the v5.8c2 static libm.a), all alike: sqrtf128 returns its argument
   (2, 4 and -42 bit for bit), f64sqrtf128 (2) and f32sqrtf128 (2) are
   2, powf128 (2, 0.5) is 2, hypotf128 (3, 4) is 13 (e_hypotl.c's
   correction step applied to sqrtl (25) = 25), and asinf128,
   acosf128 (0.75), acoshf128, asinhf128 (3), tgammaf128 (30.5) and
   j0f128 (50) are wrong from the second or third digit.  expf128,
   logf128, sinf128 and sqrtl are right.  The 2C3 is not run yet.  */

#include <stdio.h>
#include <string.h>

typedef __float128 Q;

extern Q sqrtf128 (Q);
extern Q hypotf128 (Q, Q);
extern Q asinf128 (Q);
extern Q acosf128 (Q);
extern Q acoshf128 (Q);
extern Q asinhf128 (Q);
extern Q powf128 (Q, Q);
extern Q tgammaf128 (Q);
extern Q j0f128 (Q);
extern Q expf128 (Q);
extern Q logf128 (Q);
extern Q sinf128 (Q);
extern double f64sqrtf128 (Q);
extern float f32sqrtf128 (Q);
extern long double sqrtl (long double);

static Q (*volatile p_sqrt) (Q) = sqrtf128;
static Q (*volatile p_hypot) (Q, Q) = hypotf128;
static Q (*volatile p_asin) (Q) = asinf128;
static Q (*volatile p_acos) (Q) = acosf128;
static Q (*volatile p_acosh) (Q) = acoshf128;
static Q (*volatile p_asinh) (Q) = asinhf128;
static Q (*volatile p_pow) (Q, Q) = powf128;
static Q (*volatile p_tgamma) (Q) = tgammaf128;
static Q (*volatile p_j0) (Q) = j0f128;
static Q (*volatile p_exp) (Q) = expf128;
static Q (*volatile p_log) (Q) = logf128;
static Q (*volatile p_sin) (Q) = sinf128;
static double (*volatile p_f64sqrt) (Q) = f64sqrtf128;
static float (*volatile p_f32sqrt) (Q) = f32sqrtf128;
static long double (*volatile p_sqrtl) (long double) = sqrtl;

struct c
{
  const char *what;
  int kind;                     /* 0 Q, 1 double, 2 float, 3 long double */
  const char *ref;
};

/* The reference bits: x86-64 glibc 2.43 (GCC 16, -O2), 2026-11-03.  */
static const struct c cases[] = {
  { "sqrtf128 (2)", 0, "3fff6a09e667f3bcc908b2fb1366ea95" },
  { "sqrtf128 (4)", 0, "40000000000000000000000000000000" },
  { "sqrtf128 (-42)", 0, "ffff8000000000000000000000000000" },
  { "hypotf128 (3, 4)", 0, "40014000000000000000000000000000" },
  { "asinf128 (0.75)", 0, "3ffeb235315c680dc081583db360d5e2" },
  { "acosf128 (0.75)", 0, "3ffe720a392c1d954851badbd6cd2d8e" },
  { "acoshf128 (3)", 0, "3fffc34366179d426cc1b1f33d1ba4a3" },
  { "asinhf128 (3)", 0, "3fffd185b507edc0dfdf653c5b01df94" },
  { "powf128 (2, 0.5)", 0, "3fff6a09e667f3bcc908b2fb1366ea95" },
  { "tgammaf128 (30.5)", 0, "4068305adf049c817f43d9299ad37aa1" },
  { "j0f128 (50)", 0, "3ffac936ef41b2c4fec25cac05c744a8" },
  { "f64sqrtf128 (2)", 1, "3ff6a09e667f3bcd" },
  { "f32sqrtf128 (2)", 2, "3fb504f3" },
  { "expf128 (1)", 0, "40005bf0a8b1457695355fb8ac404e7a" },
  { "logf128 (2)", 0, "3ffe62e42fefa39ef35793c7673007e6" },
  { "sinf128 (1)", 0, "3ffeaed548f090cee0418dd3d2138a1e" },
  { "sqrtl (2)", 3, "3fffb504f333f9de6484" },
};

static void
show (const struct c *c, const void *r)
{
  unsigned long long w[2] = { 0, 0 };
  char hex[40];
  double d;
  switch (c->kind)
    {
    case 0:
      memcpy (w, r, 16);
      snprintf (hex, sizeof hex, "%016llx%016llx", w[1], w[0]);
      d = (double) *(const Q *) r;
      break;
    case 1:
      memcpy (w, r, 8);
      snprintf (hex, sizeof hex, "%016llx", w[0]);
      d = *(const double *) r;
      break;
    case 2:
      memcpy (w, r, 4);
      snprintf (hex, sizeof hex, "%08llx", w[0]);
      d = *(const float *) r;
      break;
    default:
      memcpy (w, r, 10);
      snprintf (hex, sizeof hex, "%04llx%016llx", w[1] & 0xffff, w[0]);
      d = (double) *(const long double *) r;
      break;
    }
  int ok = !strcmp (hex, c->ref) || (d != d && c->kind == 0
                                      && !strncmp (c->ref + 1, "fff", 3)
                                      && strcmp (c->ref + 4, "0000000000000000000000000000"));
  printf ("%-20s -> %-32s %-24.17g %s\n", c->what, hex, d,
          ok ? "ok" : "WRONG");
}

int
main (void)
{
  volatile Q two = 2, m42 = -42, four = 4, three = 3, q34 = 0.75,
    half = 0.5, g = 30.5, fifty = 50, one = 1;
  volatile long double twol = 2;
  Q r[17];
  double rd;
  float rf;
  long double rl;
  r[0] = p_sqrt (two);
  r[1] = p_sqrt (four);
  r[2] = p_sqrt (m42);
  r[3] = p_hypot (three, four);
  r[4] = p_asin (q34);
  r[5] = p_acos (q34);
  r[6] = p_acosh (three);
  r[7] = p_asinh (three);
  r[8] = p_pow (two, half);
  r[9] = p_tgamma (g);
  r[10] = p_j0 (fifty);
  rd = p_f64sqrt (two);
  rf = p_f32sqrt (two);
  r[13] = p_exp (one);
  r[14] = p_log (two);
  r[15] = p_sin (one);
  rl = p_sqrtl (twol);
  for (int i = 0; i < 17; i++)
    show (&cases[i], i == 11 ? (const void *) &rd
                     : i == 12 ? (const void *) &rf
                     : i == 16 ? (const void *) &rl : (const void *) &r[i]);
  return 0;
}
