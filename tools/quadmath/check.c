// SPDX-License-Identifier: GPL-3.0-or-later
/* tools/quadmath (README.md): every libquadmath function, and libgcc's
   __float128 arithmetic and conversions, on fixed inputs.

     check list            the names of the tests
     check NAME [MODES]    run one; MODES is any of n d u z (rounding to
                           nearest, down, up, toward zero; default all)

   Prints one line per call: NAME MODE INDEX RESULT FLAGS.  A __float128
   result is its 32 hex digits, sign and exponent first; FLAGS are the
   exceptions the call raised (i z o u x: invalid, divide by zero,
   overflow, underflow, inexact; - for none).  The inputs are built from
   integer bit patterns and a fixed xorshift sequence, so they are the
   same on every host, and two runs compare line by line.  Build it with
   the library's own quadmath.h (-I GCC-SOURCE/libquadmath).  */
#include <fenv.h>
#include <quadmath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef __float128 Q;
typedef __complex128 C;

#define BIAS 16383

static uint64_t rs;

static uint64_t
rnd (void)
{
  rs ^= rs << 13;
  rs ^= rs >> 7;
  rs ^= rs << 17;
  return rs;
}

static Q
raw (uint64_t hi, uint64_t lo)
{
  uint64_t w[2] = { lo, hi };
  Q q;
  memcpy (&q, w, sizeof q);
  return q;
}

static Q
mk (unsigned s, unsigned e, uint64_t hi, uint64_t lo)
{
  return raw (((uint64_t) s << 63) | ((uint64_t) (e & 0x7fff) << 48)
	      | (hi & 0xffffffffffffull), lo);
}

static Q
neg (Q q)
{
  uint64_t w[2];
  memcpy (w, &q, sizeof w);
  w[1] ^= 1ull << 63;
  memcpy (&q, w, sizeof q);
  return q;
}

/* A value with a random mantissa and a biased exponent in [lo, hi].  */
static Q
rexp (unsigned lo, unsigned hi)
{
  /* One rnd () per statement: C leaves the order of a call's arguments
     unspecified, and x86 GCC and the PoC differ.  */
  unsigned e = lo + rnd () % (hi - lo + 1);
  unsigned s = rnd () & 1;
  uint64_t h = rnd ();
  uint64_t l = rnd ();
  return mk (s, e, h, l);
}

#define MAXIN 1024
static Q in[MAXIN];
static int nin;

static void
add (Q q)
{
  if (nin < MAXIN)
    in[nin++] = q;
}

/* The special values, each with both signs.  */
static void
specials (void)
{
  static const uint64_t s[][2] = {
    { 0, 0 },					  /* 0 */
    { 0, 1 },					  /* smallest subnormal */
    { 0x0000ffffffffffffull, ~0ull },		  /* largest subnormal */
    { 0x0001000000000000ull, 0 },		  /* smallest normal */
    { 0x3fff000000000000ull, 0 },		  /* 1 */
    { 0x3fff000000000000ull, 1 },		  /* 1 + ulp */
    { 0x3ffeffffffffffffull, ~0ull },		  /* 1 - ulp/2 */
    { 0x3ffe000000000000ull, 0 },		  /* 0.5 */
    { 0x4000000000000000ull, 0 },		  /* 2 */
    { 0x3fff800000000000ull, 0 },		  /* 1.5 */
    { 0x4000800000000000ull, 0 },		  /* 3 */
    { 0x4002400000000000ull, 0 },		  /* 10 */
    { 0x3ffb999999999999ull, 0x999999999999999aull }, /* 0.1 */
    { 0x4000921fb54442d1ull, 0x8469898cc51701b8ull }, /* pi */
    { 0x3fff921fb54442d1ull, 0x8469898cc51701b8ull }, /* pi/2 */
    { 0x3ffe921fb54442d1ull, 0x8469898cc51701b8ull }, /* pi/4 */
    { 0x40005bf0a8b14576ull, 0x95355fb8ac404e7aull }, /* e */
    { 0x3ffe62e42fefa39eull, 0xf35793c7673007e6ull }, /* ln 2 */
    { 0x400c62e42fefa39eull, 0xf35793c7673007e6ull }, /* 11356.5, exp's limit */
    { 0x403f000000000000ull, 0 },		  /* 2^64 */
    { 0x406f000000000000ull, 0 },		  /* 2^112 */
    { 0x406f000000000000ull, 1 },		  /* 2^112 + 1 */
    { 0x4070000000000000ull, 0 },		  /* 2^113 */
    { 0x43e7000000000000ull, 0 },		  /* 2^1000 */
    { 0x3c17000000000000ull, 0 },		  /* 2^-1000 */
    { 0x7e7f000000000000ull, 0 },		  /* 2^16000 */
    { 0x017f000000000000ull, 0 },		  /* 2^-16000 */
    { 0x7ffeffffffffffffull, ~0ull },		  /* largest */
    { 0x7fff000000000000ull, 0 },		  /* infinity */
    { 0x7fff800000000000ull, 0 },		  /* quiet NaN */
    { 0x7fff800000000000ull, 0x123 },		  /* quiet NaN, payload */
    { 0x7fff400000000000ull, 0 },		  /* signalling NaN */
    { 0x7fff000000000000ull, 1 },		  /* signalling NaN, payload 1 */
  };
  for (unsigned i = 0; i < sizeof s / sizeof s[0]; i++)
    {
      add (raw (s[i][0], s[i][1]));
      add (neg (raw (s[i][0], s[i][1])));
    }
}

/* The one-argument inputs: specials, then random values by range.  */
static void
inputs (void)
{
  nin = 0;
  rs = 0x9e3779b97f4a7c15ull;
  specials ();
  for (int i = 0; i < 80; i++)	/* any bits */
    {
      uint64_t h = rnd ();
      add (raw (h, rnd ()));
    }
  for (int i = 0; i < 160; i++)	/* |x| in [2^-8, 2^9) */
    add (rexp (BIAS - 8, BIAS + 8));
  for (int i = 0; i < 40; i++)	/* small */
    add (rexp (BIAS - 120, BIAS - 9));
  for (int i = 0; i < 40; i++)	/* large */
    add (rexp (BIAS + 9, BIAS + 200));
  for (int i = 0; i < 30; i++)	/* subnormal and tiny */
    add (rexp (0, 120));
  for (int i = 0; i < 30; i++)	/* huge */
    add (rexp (0x7ffe - 120, 0x7ffe));
  for (int i = 0; i < 20; i++)	/* exp's and lgamma's edges */
    add (rexp (BIAS + 12, BIAS + 14));
  for (int k = 1; k <= 24; k++)	/* integers, halves and their neighbours */
    {
      Q n = mk (k & 1, BIAS + k % 7, (uint64_t) k << 40, 0);
      uint64_t w[2];
      memcpy (w, &n, sizeof w);
      add (n);
      add (raw (w[1], w[0] + 1));
      add (raw (w[1] - (w[0] == 0), w[0] - 1));
    }
}

static const char *
hex (Q q, char *buf)
{
  uint64_t w[2];
  memcpy (w, &q, sizeof w);
  sprintf (buf, "%016llx%016llx", (unsigned long long) w[1],
	   (unsigned long long) w[0]);
  return buf;
}

static const char *
flags (void)
{
  static char b[8];
  int f = fetestexcept (FE_ALL_EXCEPT);
  char *p = b;
  if (f & FE_INVALID)
    *p++ = 'i';
  if (f & FE_DIVBYZERO)
    *p++ = 'z';
  if (f & FE_OVERFLOW)
    *p++ = 'o';
  if (f & FE_UNDERFLOW)
    *p++ = 'u';
  if (f & FE_INEXACT)
    *p++ = 'x';
  if (p == b)
    *p++ = '-';
  *p = 0;
  return b;
}

static const char *name;
static char mode;

#define CLEAR feclearexcept (FE_ALL_EXCEPT)

static void
outq (int i, Q r)
{
  char b[40];
  const char *f = flags ();
  printf ("%s %c %d %s %s\n", name, mode, i, hex (r, b), f);
}

static void
outq2 (int i, Q r, Q s)
{
  char b[40], c[40];
  const char *f = flags ();
  printf ("%s %c %d %s %s %s\n", name, mode, i, hex (r, b), hex (s, c), f);
}

static void
outqi (int i, Q r, long long n)
{
  char b[40];
  const char *f = flags ();
  printf ("%s %c %d %s %lld %s\n", name, mode, i, hex (r, b), n, f);
}

static void
outi (int i, long long n)
{
  const char *f = flags ();
  printf ("%s %c %d %lld %s\n", name, mode, i, n, f);
}

/* The specials (by their index in specials ()) that two-argument tests
   pair with each other, each with both signs.  */
static const int pairs[] = { 0, 1, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 16,
			     19, 22, 23, 24, 25, 26, 27, 28, 29, 31 };
#define NPAIR (2 * (int) (sizeof pairs / sizeof pairs[0]))
#define PAIR(k) in[2 * pairs[(k) / 2] + (k) % 2]
#define NSPEC 66

/* Two-argument inputs: pairs of those specials, then random pairs of
   comparable and of unrelated magnitudes.  */
static Q in2[2][MAXIN * 2];
static int nin2;

static void
inputs2 (void)
{
  inputs ();
  nin2 = 0;
  for (int i = 0; i < NPAIR; i++)
    for (int j = i % 3; j < NPAIR; j += 3)
      {
	in2[0][nin2] = PAIR (i);
	in2[1][nin2++] = PAIR (j);
      }
  for (int i = 0; i < 300; i++)
    {
      unsigned e = BIAS - 20 + rnd () % 41;
      in2[0][nin2] = rexp (e - 3, e + 3);
      in2[1][nin2++] = rexp (e - 3, e + 3);
    }
  for (int i = 0; i < 300; i++)
    {
      in2[0][nin2] = in[rnd () % nin];
      in2[1][nin2++] = in[rnd () % nin];
    }
}

typedef Q (*q_q) (Q);
typedef Q (*q_qq) (Q, Q);
typedef int (*i_q) (Q);
typedef long (*l_q) (Q);
typedef long long (*ll_q) (Q);
typedef C (*c_c) (C);
typedef Q (*q_c) (C);

static Q qadd (Q a, Q b) { return a + b; }
static Q qsub (Q a, Q b) { return a - b; }
static Q qmul (Q a, Q b) { return a * b; }
static Q qdiv (Q a, Q b) { return a / b; }
static Q qneg (Q a) { return -a; }
static int qeq (Q a, Q b) { return a == b; }
static int qlt (Q a, Q b) { return a < b; }
static int qle (Q a, Q b) { return a <= b; }
static int qgt (Q a, Q b) { return a > b; }
static int qunord (Q a, Q b) { return __builtin_isunordered (a, b); }
static int qisless (Q a, Q b) { return __builtin_isless (a, b); }

enum kind { Q_Q, Q_QQ, Q_QQQ, I_Q, L_Q, LL_Q, Q_QI, Q_QL, Q_IQ, FREXP, MODF,
	    REMQUO, SINCOS, C_C, C_CC, Q_C, C_Q, NAN_S, STRTO, PRINTF,
	    I_QQ, CONV_TO, CONV_FROM };

struct test
{
  const char *name;
  enum kind kind;
  void (*fn) (void);
};

#define T(n, k) { #n, k, (void (*) (void)) n }
static const struct test tests[] = {
  T (acosq, Q_Q), T (acoshq, Q_Q), T (asinq, Q_Q), T (asinhq, Q_Q),
  T (atanq, Q_Q), T (atanhq, Q_Q), T (cbrtq, Q_Q), T (ceilq, Q_Q),
  T (coshq, Q_Q), T (cosq, Q_Q), T (erfq, Q_Q), T (erfcq, Q_Q),
  T (exp2q, Q_Q), T (expq, Q_Q), T (expm1q, Q_Q), T (fabsq, Q_Q),
  T (floorq, Q_Q), T (j0q, Q_Q), T (j1q, Q_Q), T (lgammaq, Q_Q),
  T (log10q, Q_Q), T (log1pq, Q_Q), T (log2q, Q_Q), T (logbq, Q_Q),
  T (logq, Q_Q), T (nearbyintq, Q_Q), T (rintq, Q_Q), T (roundq, Q_Q),
  T (sinhq, Q_Q), T (sinq, Q_Q), T (sqrtq, Q_Q), T (tanq, Q_Q),
  T (tanhq, Q_Q), T (tgammaq, Q_Q), T (truncq, Q_Q), T (y0q, Q_Q),
  T (y1q, Q_Q),
  T (atan2q, Q_QQ), T (copysignq, Q_QQ), T (fdimq, Q_QQ), T (fmaxq, Q_QQ),
  T (fminq, Q_QQ), T (fmodq, Q_QQ), T (hypotq, Q_QQ), T (nextafterq, Q_QQ),
  T (powq, Q_QQ), T (remainderq, Q_QQ),
  T (fmaq, Q_QQQ),
  T (finiteq, I_Q), T (ilogbq, I_Q), T (isinfq, I_Q), T (isnanq, I_Q),
  T (issignalingq, I_Q), T (signbitq, I_Q),
  T (lrintq, L_Q), T (lroundq, L_Q), T (llrintq, LL_Q), T (llroundq, LL_Q),
  T (ldexpq, Q_QI), T (scalbnq, Q_QI), T (scalblnq, Q_QL),
  T (jnq, Q_IQ), T (ynq, Q_IQ),
  T (frexpq, FREXP), T (modfq, MODF), T (remquoq, REMQUO),
  T (sincosq, SINCOS),
  T (cacosq, C_C), T (cacoshq, C_C), T (casinq, C_C), T (casinhq, C_C),
  T (catanq, C_C), T (catanhq, C_C), T (ccosq, C_C), T (ccoshq, C_C),
  T (cexpq, C_C), T (clogq, C_C), T (clog10q, C_C), T (conjq, C_C),
  T (cprojq, C_C), T (csinq, C_C), T (csinhq, C_C), T (csqrtq, C_C),
  T (ctanq, C_C), T (ctanhq, C_C),
  T (cpowq, C_CC),
  T (cabsq, Q_C), T (cargq, Q_C), T (cimagq, Q_C), T (crealq, Q_C),
  T (cexpiq, C_Q),
  T (nanq, NAN_S), T (strtoflt128, STRTO), T (quadmath_snprintf, PRINTF),
  /* libgcc's soft-fp, which every __float128 operation calls.  */
  T (qadd, Q_QQ), T (qsub, Q_QQ), T (qmul, Q_QQ), T (qdiv, Q_QQ),
  T (qneg, Q_Q),
  T (qeq, I_QQ), T (qlt, I_QQ), T (qle, I_QQ), T (qgt, I_QQ),
  T (qunord, I_QQ), T (qisless, I_QQ),
  { "conv_to", CONV_TO, 0 }, { "conv_from", CONV_FROM, 0 },
};

static void
run_q_q (q_q f)
{
  inputs ();
  for (int i = 0; i < nin; i++)
    {
      volatile Q x = in[i];
      CLEAR;
      Q r = f (x);
      outq (i, r);
    }
}

static void
run_q_qq (q_qq f)
{
  inputs2 ();
  for (int i = 0; i < nin2; i++)
    {
      volatile Q x = in2[0][i], y = in2[1][i];
      CLEAR;
      Q r = f (x, y);
      outq (i, r);
    }
}

static void
run_i_qq (int (*f) (Q, Q))
{
  inputs2 ();
  for (int i = 0; i < nin2; i++)
    {
      volatile Q x = in2[0][i], y = in2[1][i];
      CLEAR;
      int r = f (x, y);
      outi (i, r);
    }
}

static void
run_fma (void)
{
  inputs2 ();
  int n = 0;
  for (int i = 0; i < nin2; i++)
    {
      volatile Q x = in2[0][i], y = in2[1][i];
      /* c: a special, a value near -x*y (cancellation), or random.  */
      Q c;
      switch (i % 3)
	{
	case 0: c = PAIR (rnd () % NPAIR); break;
	case 1: c = rexp (BIAS - 30, BIAS + 30); break;
	default: c = in[rnd () % nin]; break;
	}
      if (i % 5 == 0)
	{
	  /* -x*y rounded: fma must give the low part exactly.  */
	  Q p = x * y;
	  c = -p;
	}
      volatile Q z = c;
      CLEAR;
      Q r = fmaq (x, y, z);
      outq (n++, r);
    }
}

static void
run_i_q (i_q f)
{
  inputs ();
  for (int i = 0; i < nin; i++)
    {
      volatile Q x = in[i];
      CLEAR;
      int r = f (x);
      outi (i, r);
    }
}

static void
run_l_q (l_q f)
{
  inputs ();
  for (int i = 0; i < nin; i++)
    {
      volatile Q x = in[i];
      CLEAR;
      long r = f (x);
      outi (i, r);
    }
}

static void
run_ll_q (ll_q f)
{
  inputs ();
  for (int i = 0; i < nin; i++)
    {
      volatile Q x = in[i];
      CLEAR;
      long long r = f (x);
      outi (i, r);
    }
}

static const long ns[] = { -20000, -16500, -16494, -200, -3, -1, 0, 1, 2, 3,
			   10, 100, 16383, 16384, 20000 };
#define NNS (sizeof ns / sizeof ns[0])

static void
run_q_qi (const struct test *t)
{
  inputs ();
  int n = 0;
  for (int i = 0; i < nin; i += 2)
    for (unsigned j = i < NSPEC ? 0 : (i / 2) % 4; j < NNS;
	 j += i < NSPEC ? 1 : 4)
      {
	volatile Q x = in[i];
	CLEAR;
	Q r = (t->kind == Q_QL ? ((Q (*) (Q, long)) t->fn) (x, ns[j])
	       : ((Q (*) (Q, int)) t->fn) (x, (int) ns[j]));
	outq (n++, r);
      }
}

static void
run_q_iq (Q (*f) (int, Q))
{
  static const int bn[] = { -5, -1, 0, 1, 2, 5, 20, 60 };
  inputs ();
  int n = 0;
  for (int i = 0; i < nin; i += 2)
    for (unsigned j = 0; j < sizeof bn / sizeof bn[0]; j++)
      {
	volatile Q x = in[i];
	CLEAR;
	Q r = f (bn[j], x);
	outq (n++, r);
      }
}

static C
mkc (Q re, Q im)
{
  C c;
  __real__ c = re;
  __imag__ c = im;
  return c;
}

/* Complex inputs: pairs of specials, then random pairs.  */
static void
run_c (const struct test *t)
{
  inputs2 ();
  for (int i = 0; i < nin2; i++)
    {
      volatile C z = mkc (in2[0][i], in2[1][i]);
      CLEAR;
      switch (t->kind)
	{
	case C_C:
	  {
	    C r = ((c_c) t->fn) (z);
	    outq2 (i, __real__ r, __imag__ r);
	    break;
	  }
	case C_CC:
	  {
	    volatile C w = mkc (in2[1][(i * 7) % nin2], in2[0][(i * 13) % nin2]);
	    CLEAR;
	    C r = cpowq (z, w);
	    outq2 (i, __real__ r, __imag__ r);
	    break;
	  }
	default:
	  {
	    Q r = ((q_c) t->fn) (z);
	    outq (i, r);
	    break;
	  }
	}
    }
}

static void
run_misc (const struct test *t)
{
  inputs ();
  for (int i = 0; i < nin; i++)
    {
      volatile Q x = in[i];
      CLEAR;
      switch (t->kind)
	{
	case FREXP:
	  {
	    int e = 0;
	    Q r = frexpq (x, &e);
	    outqi (i, r, e);
	    break;
	  }
	case MODF:
	  {
	    Q ip = 0;
	    Q r = modfq (x, &ip);
	    outq2 (i, r, ip);
	    break;
	  }
	case REMQUO:
	  {
	    int qo = 0;
	    volatile Q y = in[(i * 7 + 3) % nin];
	    CLEAR;
	    Q r = remquoq (x, y, &qo);
	    outqi (i, r, qo);
	    break;
	  }
	case SINCOS:
	  {
	    Q s = 0, c = 0;
	    sincosq (x, &s, &c);
	    outq2 (i, s, c);
	    break;
	  }
	case C_Q:
	  {
	    C r = cexpiq (x);
	    outq2 (i, __real__ r, __imag__ r);
	    break;
	  }
	default:
	  abort ();
	}
    }
}

static void
run_nan (void)
{
  static const char *s[] = { "", "0", "1", "0x123", "0x7fffffffffffffffffff",
			     "123456789012345678901234567890", "abc", "-1" };
  for (unsigned i = 0; i < sizeof s / sizeof s[0]; i++)
    {
      CLEAR;
      Q r = nanq (s[i]);
      outq (i, r);
    }
}

/* Decimal and hex strings built from the xorshift sequence alone.  */
static void
run_strto (void)
{
  static const char *s[] = {
    "0", "-0", "inf", "-Infinity", "nan", "-nan(0x123)", "nan(abc)",
    "0x1.8p3", "0x1p-16494", "0x1p-16495", "0x1.ffffffffffffffffffffffffffffp16383",
    "0x1p16384", "1e-4966", "1e-4950", "3.36210314311209350626267781732175260e-4932",
    "6.475175119438025110924438958227646552e-4966",
    "1.18973149535723176508575932662800702e4932",
    "1.18973149535723176508575932662800703e4932", "1e4933", "0.1", "  +12.5e+3x",
    "1.000000000000000000000000000000000096296497219361792652798897",
    "4.4501477170144027661805e-308", "123456789012345678901234567890123456789",
    ".5", "5.", "e5", "0x", "-0x.8p1",
  };
  unsigned ns = sizeof s / sizeof s[0];
  rs = 0x2545f4914f6cdd1dull;
  for (unsigned i = 0; i < ns + 400; i++)
    {
      char b[128];
      const char *p = i < ns ? s[i] : b;
      if (i >= ns)
	{
	  char *q = b;
	  int nd = 1 + rnd () % 45;
	  if (rnd () & 1)
	    *q++ = '-';
	  if (i % 4 == 3)
	    {
	      q += sprintf (q, "0x1.");
	      for (int k = 0; k < nd && k < 30; k++)
		*q++ = "0123456789abcdef"[rnd () % 16];
	      sprintf (q, "p%d", (int) (rnd () % 33000) - 16500);
	    }
	  else
	    {
	      for (int k = 0; k < nd; k++)
		{
		  *q++ = '0' + rnd () % 10;
		  if (k == 0)
		    *q++ = '.';
		}
	      int range = (i % 4 == 0 ? 9900 : i % 4 == 1 ? 60 : 4980);
	      sprintf (q, "e%d", (int) (rnd () % (2 * range + 1)) - range);
	    }
	}
      char *end;
      CLEAR;
      Q r = strtoflt128 (p, &end);
      outqi (i, r, end - p);
    }
}

static void
run_printf (void)
{
  static const char *fmt[] = { "%.0Qf", "%.3Qf", "%.20Qe", "%.36Qg", "%Qa",
			       "%.10Qa", "%+-#45.30Qe", "%Qg", "%.40Qe" };
  inputs ();
  int n = 0;
  for (unsigned j = 0; j < sizeof fmt / sizeof fmt[0]; j++)
    for (int i = 0; i < nin; i++)
      {
	char b[6000];
	volatile Q x = in[i];
	/* %Qf of huge values runs to 4,933 digits; keep it to the
	   moderate inputs.  */
	if (fmt[j][strlen (fmt[j]) - 1] == 'f' && finiteq (x)
	    && ilogbq (x) > 400)
	  continue;
	CLEAR;
	int r = quadmath_snprintf (b, sizeof b, fmt[j], x);
	const char *f = flags ();
	printf ("%s %c %d %d %s %s\n", name, mode, n++, r, f, b);
      }
}

/* Conversions from __float128 to the other types.  Each result goes to
   a volatile before the flags are read: GCC takes soft-fp's conversion
   functions for const and may otherwise call them after fetestexcept
   (x86 GCC 16 did).  */
static void
run_conv_to (void)
{
  inputs ();
  for (int i = 0; i < nin; i++)
    {
      volatile Q x = in[i];
      volatile float f;
      volatile double d;
      volatile long double l;
      unsigned char lb[16];
      CLEAR;
      f = x;
      {
	uint32_t u;
	float g = f;
	memcpy (&u, &g, 4);
	const char *fl = flags ();
	printf ("%s.f %c %d %08x %s\n", name, mode, i, u, fl);
      }
      CLEAR;
      d = x;
      {
	uint64_t u;
	double e = d;
	memcpy (&u, &e, 8);
	const char *fl = flags ();
	printf ("%s.d %c %d %016llx %s\n", name, mode, i, (unsigned long long) u, fl);
      }
      CLEAR;
      l = x;
      {
	long double m0 = l;
	memset (lb, 0, sizeof lb);
	memcpy (lb, &m0, 10);
	uint64_t m;
	uint16_t e;
	memcpy (&m, lb, 8);
	memcpy (&e, lb + 8, 2);
	const char *fl = flags ();
	printf ("%s.ld %c %d %04x%016llx %s\n", name, mode, i, e,
		(unsigned long long) m, fl);
      }
      CLEAR;
      {
	volatile long long v = (long long) x;
	const char *fl = flags ();
	printf ("%s.i64 %c %d %lld %s\n", name, mode, i, v, fl);
      }
      CLEAR;
      {
	volatile unsigned long long v = (unsigned long long) x;
	const char *fl = flags ();
	printf ("%s.u64 %c %d %llu %s\n", name, mode, i, v, fl);
      }
      CLEAR;
      {
	volatile int v = (int) x;
	const char *fl = flags ();
	printf ("%s.i32 %c %d %d %s\n", name, mode, i, v, fl);
      }
      CLEAR;
      {
	volatile __int128 v = (__int128) x;
	const char *fl = flags ();
	printf ("%s.i128 %c %d %016llx%016llx %s\n", name, mode, i,
		(unsigned long long) (v >> 64), (unsigned long long) v, fl);
      }
      CLEAR;
      {
	volatile unsigned __int128 v = (unsigned __int128) x;
	const char *fl = flags ();
	printf ("%s.u128 %c %d %016llx%016llx %s\n", name, mode, i,
		(unsigned long long) (v >> 64), (unsigned long long) v, fl);
      }
    }
}

/* Conversions to __float128 from values built from bits.  */
static char sub[64];

static void
subname (const char *sfx)
{
  snprintf (sub, sizeof sub, "conv_from.%s", sfx);
  name = sub;
}

static void
run_conv_from (void)
{
  rs = 0x3c6ef372fe94f82bull;
  for (int i = 0; i < 600; i++)
    {
      uint64_t a = rnd (), b = rnd ();
      /* Every eighth input is a special pattern of the source type.  */
      if (i % 8 == 0)
	{
	  static const uint64_t sp[] = { 0, 1, 0x7ff0000000000000ull,
					 0x7ff8000000000000ull,
					 0x7ff0000000000001ull,
					 0x000fffffffffffffull,
					 0x8000000000000000ull,
					 0xffffffffffffffffull };
	  a = sp[(i / 8) % 8];
	  b = (i / 64) & 1 ? a : 0;
	}
      volatile uint32_t u32 = (uint32_t) a;
      volatile uint64_t u64 = a;
      float f;
      double d;
      long double l;
      uint32_t fu = u32;
      if (i % 8 == 0)
	fu = (uint32_t) (a >> 32) | (uint32_t) a;
      memcpy (&f, &fu, 4);
      memcpy (&d, (const void *) &u64, 8);
      {
	unsigned char lb[16];
	memset (lb, 0, sizeof lb);
	uint64_t m = b | (i % 3 ? 1ull << 63 : 0);
	uint16_t e = (uint16_t) (a >> 48);
	if (i % 5 == 0)
	  e &= 0x8000;		/* zero or denormal */
	memcpy (lb, &m, 8);
	memcpy (lb + 8, &e, 2);
	memcpy (&l, lb, sizeof l < 16 ? sizeof l : 16);
      }
      volatile float vf = f;
      volatile double vd = d;
      volatile long double vl = l;
      CLEAR;
      Q r = vf;
      subname ("f"), outq (i, r);
      CLEAR;
      r = vd;
      subname ("d"), outq (i, r);
      CLEAR;
      r = vl;
      subname ("ld"), outq (i, r);
      CLEAR;
      r = (Q) (int64_t) u64;
      subname ("i64"), outq (i, r);
      CLEAR;
      r = (Q) (uint64_t) u64;
      subname ("u64"), outq (i, r);
      CLEAR;
      r = (Q) (int32_t) u32;
      subname ("i32"), outq (i, r);
      __int128 w = ((__int128) (int64_t) a << 64) | b;
      volatile __int128 vw = w >> (i % 100);
      CLEAR;
      r = (Q) vw;
      subname ("i128"), outq (i, r);
      volatile unsigned __int128 vu = (unsigned __int128) w >> (i % 100);
      CLEAR;
      r = (Q) vu;
      subname ("u128"), outq (i, r);
    }
}

static void
run (const struct test *t)
{
  name = t->name;
  switch (t->kind)
    {
    case Q_Q: run_q_q ((q_q) t->fn); break;
    case Q_QQ: run_q_qq ((q_qq) t->fn); break;
    case Q_QQQ: run_fma (); break;
    case I_Q: run_i_q ((i_q) t->fn); break;
    case L_Q: run_l_q ((l_q) t->fn); break;
    case LL_Q: run_ll_q ((ll_q) t->fn); break;
    case Q_QI: case Q_QL: run_q_qi (t); break;
    case Q_IQ: run_q_iq ((Q (*) (int, Q)) t->fn); break;
    case FREXP: case MODF: case REMQUO: case SINCOS: case C_Q:
      run_misc (t); break;
    case C_C: case C_CC: case Q_C: run_c (t); break;
    case NAN_S: run_nan (); break;
    case STRTO: run_strto (); break;
    case PRINTF: run_printf (); break;
    case I_QQ: run_i_qq ((int (*) (Q, Q)) t->fn); break;
    case CONV_TO: run_conv_to (); break;
    case CONV_FROM: run_conv_from (); break;
    }
}

int
main (int argc, char **argv)
{
  static const struct { char c; int m; } modes[] = {
    { 'n', FE_TONEAREST }, { 'd', FE_DOWNWARD }, { 'u', FE_UPWARD },
    { 'z', FE_TOWARDZERO } };
  unsigned nt = sizeof tests / sizeof tests[0];
  if (argc < 2)
    {
      fprintf (stderr, "usage: check list | check NAME [MODES]\n");
      return 2;
    }
  if (strcmp (argv[1], "list") == 0)
    {
      for (unsigned i = 0; i < nt; i++)
	puts (tests[i].name);
      return 0;
    }
  const char *ms = argc > 2 ? argv[2] : "nduz";
  for (unsigned i = 0; i < nt; i++)
    if (strcmp (tests[i].name, argv[1]) == 0)
      {
	for (const char *m = ms; *m; m++)
	  for (unsigned k = 0; k < 4; k++)
	    if (modes[k].c == *m)
	      {
		mode = *m;
		fesetround (modes[k].m);
		run (&tests[i]);
		fesetround (FE_TONEAREST);
		fflush (stdout);
	      }
	return 0;
      }
  fprintf (stderr, "check: no test %s\n", argv[1]);
  return 2;
}
