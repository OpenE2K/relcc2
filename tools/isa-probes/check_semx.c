/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Recompute semx.c's output lines on the host (x86-64 with SSE4.1) and
   report the lines that differ (spec/isa/semantics.md, "Other scalar
   operations").

     cc -O1 -msse4.1 tools/isa-probes/check_semx.c -o check_semx -lm
     ./check_semx < hw.txt                # summary per operation
     ./check_semx -v < hw.txt             # and every differing line

   Every operation has a model: the FP ones run the matching SSE
   instruction (subss, maxss, cmpltss, cvttsd2si, roundsd, ...) and take
   its MXCSR flags, whose layout %pfpfr shares; fscale is ldexp; the
   integer ones are written out here, the x86 flag forms (_fb, _fh, _fw,
   _fd) giving an EFLAGS image.  A 32-bit result keeps the destination's
   upper half (0x55555555, setwd dbl = 0).  */

#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semx-sets.h"

typedef unsigned long u64;
typedef unsigned int u32;

#define KEEP 0x5555555500000000UL
#define CF 0x1
#define PF 0x4
#define AF 0x10
#define ZF 0x40
#define SF 0x80
#define OF 0x800
#define FLAGS (CF | PF | AF | ZF | SF | OF)

/* What an operation should give: a signal, or a value and flags.  */
struct res { int sig; u64 v; unsigned fl; };

static unsigned mxcsr;

#define SSE_BEGIN unsigned _c = 0x1f80;
#define SSE_END mxcsr = _c & 0x3f;

/* Scalar FP op on bit patterns: A = A op B, as "insn B, A".  */
#define SSE_FF(insn, sfx, A, B)                                         \
  ({ double _x, _y; u64 _a = (A), _b = (B), _r;                         \
     memcpy (&_x, &_a, 8); memcpy (&_y, &_b, 8);                        \
     SSE_BEGIN                                                          \
     __asm__ volatile ("ldmxcsr %1\n\t" insn sfx " %2, %0\n\t"          \
                       "stmxcsr %1" : "+x" (_x), "+m" (_c) : "x" (_y));  \
     SSE_END memcpy (&_r, &_x, 8); _r; })

static int
parity (u64 r)
{
  return !__builtin_parity (r & 0xff);
}

static unsigned
szp (u64 r, int bits)
{
  u64 m = bits == 64 ? ~0UL : (1UL << bits) - 1;
  r &= m;
  return (r == 0 ? ZF : 0) | (r >> (bits - 1) & 1 ? SF : 0)
         | (parity (r) ? PF : 0);
}

static long
sext (u64 v, int bits)
{
  return bits == 64 ? (long) v : (long) (v << (64 - bits)) >> (64 - bits);
}

/* The x86 flag forms: OP of A and B at BITS; OLD is src3 (inc, dec,
   shifts and rotates keep or take flags from it).  The register is 32
   bits wide for the s forms and 64 for d.  */
static unsigned
xflags (const char *op, u64 a, u64 b, u64 old, int bits, int rbits)
{
  u64 m = bits == 64 ? ~0UL : (1UL << bits) - 1;
  u64 rm = rbits == 64 ? ~0UL : (1UL << rbits) - 1;
  old &= FLAGS;
  if (!strcmp (op, "add") || !strcmp (op, "sub") || !strcmp (op, "inc")
      || !strcmp (op, "dec"))
    {
      /* inc and dec add and subtract src2 and keep CF from src3.  */
      int add = !strcmp (op, "add") || !strcmp (op, "inc");
      a &= m, b &= m;
      u64 r = (add ? a + b : a - b) & m;
      unsigned cf = add ? r < a : a < b;
      unsigned of = (add ? ~(a ^ b) & (a ^ r) : (a ^ b) & (a ^ r))
                    >> (bits - 1) & 1;
      if (op[0] == 'i' || op[0] == 'd')
        cf = old & CF;
      return (cf ? CF : 0) | (of ? OF : 0) | ((a ^ b ^ r) & 0x10 ? AF : 0)
             | szp (r, bits);
    }
  if (!strncmp (op, "and", 3) || !strncmp (op, "or", 2)
      || !strncmp (op, "xor", 3))
    {
      u64 y = op[strlen (op) - 1] == 'n' ? ~b : b;
      u64 r = op[0] == 'a' ? a & y : op[0] == 'o' ? a | y : a ^ y;
      return szp (r, bits);
    }
  if (!strcmp (op, "umulx") || !strcmp (op, "smulx"))
    {
      /* The product of the 32-bit operands; CF = OF = 1 when bits
         BITS..2*BITS-1 are not the extension of bits 0..BITS-1.  */
      u64 p = op[0] == 'u' ? (u64) (u32) a * (u32) b
                           : (u64) ((long) (int) a * (int) b);
      u64 lo = p & m, hi = bits == 32 ? p >> 32 : (p >> bits) & m;
      int f = op[0] == 'u' ? hi != 0 : hi != ((lo >> (bits - 1)) & 1 ? m : 0);
      return f ? CF | OF : 0;
    }
  /* Shifts and rotates: the count is src2 modulo the register width; a
     zero count keeps src3's flags.  */
  unsigned c = b & (rbits - 1);
  if (!c)
    return old;
  if (!strcmp (op, "scl") || !strcmp (op, "scr"))
    {
      unsigned k = c % bits;
      u64 v = a & m, r = k ? (op[2] == 'l' ? v << k | v >> (bits - k)
                                           : v >> k | v << (bits - k)) & m
                           : v;
      unsigned cf = op[2] == 'l' ? r & 1 : r >> (bits - 1) & 1;
      unsigned of = op[2] == 'l' ? (r >> (bits - 1) ^ cf) & 1
                                 : (r >> (bits - 1) ^ r >> (bits - 2)) & 1;
      if (c != 1)
        of = 0;
      return (old & ~(CF | OF)) | cf | (of ? OF : 0);
    }
  /* SZP from the shift of the whole register; CF from the BITS-wide
     operand (zero- or sign-extended); OF only for a count of 1; AF 0.  */
  u64 r, v = a & rm;
  unsigned cf, of = 0;
  if (!strcmp (op, "shl"))
    {
      r = (v << c) & rm;
      cf = c <= (unsigned) bits ? (a & m) >> (bits - c) & 1 : 0;
      of = (r >> (bits - 1) & 1) ^ cf;
    }
  else if (!strcmp (op, "shr"))
    {
      r = v >> c;
      cf = c <= (unsigned) bits ? (a & m) >> (c - 1) & 1 : 0;
      of = (a >> (bits - 1)) & 1;
    }
  else
    {
      r = (u64) (sext (v, rbits) >> c) & rm;
      long n = sext (a, bits);
      cf = (c - 1 < 63 ? n >> (c - 1) : n >> 63) & 1;
    }
  if (c != 1)
    of = 0;
  return (cf ? CF : 0) | (of ? OF : 0) | szp (r, bits);
}

static int
cctop (const char *c, u64 f)
{
  int cf = f & CF, zf = !!(f & ZF), sf = !!(f & SF), of = !!(f & OF);
  if (!strcmp (c, "o")) return of;
  if (!strcmp (c, "b")) return cf;
  if (!strcmp (c, "e")) return zf;
  if (!strcmp (c, "be")) return cf || zf;
  if (!strcmp (c, "s")) return sf;
  if (!strcmp (c, "p")) return !!(f & PF);
  if (!strcmp (c, "l")) return sf != of;
  return zf || sf != of;
}

static int
fcmp_sse (const char *c, int dbl, u64 a, u64 b)
{
  u64 r;
#define C(n)                                                            \
  if (!strcmp (c, #n))                                                  \
    r = dbl ? SSE_FF ("cmp" #n "sd", "", a, b)                          \
            : SSE_FF ("cmp" #n "ss", "", a, b);                         \
  else
  C (eq) C (lt) C (le) C (neq) C (nlt) C (nle)
  if (!strcmp (c, "uod"))
    r = dbl ? SSE_FF ("cmpunordsd", "", a, b) : SSE_FF ("cmpunordss", "", a, b);
  else
    r = dbl ? SSE_FF ("cmpordsd", "", a, b) : SSE_FF ("cmpordss", "", a, b);
#undef C
  return r & 1;
}

static u64
round_sse (int dbl, unsigned imm, u64 b)
{
  double x, y;
  u64 r;
  memcpy (&y, &b, 8);
  x = y;
  SSE_BEGIN
#define R(i)                                                            \
  case i:                                                               \
    if (dbl)                                                            \
      __asm__ volatile ("ldmxcsr %1\n\troundsd $" #i ", %2, %0\n\t"     \
                        "stmxcsr %1" : "+x" (x), "+m" (_c) : "x" (y));  \
    else                                                                \
      __asm__ volatile ("ldmxcsr %1\n\troundss $" #i ", %2, %0\n\t"     \
                        "stmxcsr %1" : "+x" (x), "+m" (_c) : "x" (y));  \
    break;
  switch (imm & 15)
    {
      R (0) R (1) R (2) R (3) R (4) R (5) R (6) R (7)
      R (8) R (9) R (10) R (11) R (12) R (13) R (14) R (15)
    }
#undef R
  SSE_END
  memcpy (&r, &x, 8);
  return dbl ? r : (r & 0xffffffff) | KEEP;
}

static u64
cvt (const char *n, u64 a)
{
  u64 r = 0;
  SSE_BEGIN
#define CV(name, asmtext, out, in)                                      \
  if (!strcmp (n, name))                                                \
    __asm__ volatile ("ldmxcsr %1\n\t" asmtext "\n\tstmxcsr %1"         \
                      : "=&" out (r), "+m" (_c) : in (a) : "xmm7");     \
  else
  CV ("fstoid", "movq %2, %%xmm7\n\tcvtss2si %%xmm7, %0", "r", "r")
  CV ("fstoidtr", "movq %2, %%xmm7\n\tcvttss2si %%xmm7, %0", "r", "r")
  CV ("fdtoidtr", "movq %2, %%xmm7\n\tcvttsd2si %%xmm7, %0", "r", "r")
  CV ("fdtoistr", "movq %2, %%xmm7\n\tcvttsd2si %%xmm7, %k0", "r", "r")
  CV ("idtofs", "cvtsi2ssq %2, %%xmm7\n\tmovq %%xmm7, %0", "r", "r")
  CV ("istofd", "cvtsi2sdl %k2, %%xmm7\n\tmovq %%xmm7, %0", "r", "r")
  CV ("istofs", "cvtsi2ssl %k2, %%xmm7\n\tmovq %%xmm7, %0", "r", "r")
  CV ("fdtoid", "movq %2, %%xmm7\n\tcvtsd2si %%xmm7, %0", "r", "r")
  CV ("idtofd", "cvtsi2sdq %2, %%xmm7\n\tmovq %%xmm7, %0", "r", "r")
  CV ("fdtofs", "movq %2, %%xmm7\n\tcvtsd2ss %%xmm7, %%xmm7\n\t"
      "movq %%xmm7, %0", "r", "r")
  CV ("fstofd", "movq %2, %%xmm7\n\tcvtss2sd %%xmm7, %%xmm7\n\t"
      "movq %%xmm7, %0", "r", "r")
    abort ();
#undef CV
  SSE_END
  if (!strcmp (n, "fdtoistr") || !strcmp (n, "idtofs") || !strcmp (n, "istofs")
      || !strcmp (n, "fdtofs"))
    r = (r & 0xffffffff) | KEEP;
  return r;
}

static int
denormal (u64 v, int dbl)
{
  return dbl ? (v & 0x7ff0000000000000UL) == 0 && (v & 0xfffffffffffffUL)
             : (v & 0x7f800000) == 0 && (v & 0x7fffff);
}

static u64
fscale (int dbl, u64 a, u64 b)
{
  int n = (int) b;
  u64 r = 0;
  feclearexcept (FE_ALL_EXCEPT);
  if (dbl)
    {
      double x;
      memcpy (&x, &a, 8);
      x = ldexp (x, n);
      memcpy (&r, &x, 8);
    }
  else
    {
      float x;
      u32 v = a, w;
      memcpy (&x, &v, 4);
      x = ldexpf (x, n);
      memcpy (&w, &x, 4);
      r = w | KEEP;
    }
  int e = fetestexcept (FE_ALL_EXCEPT);
  mxcsr = (e & FE_INVALID ? 1 : 0) | (e & FE_DIVBYZERO ? 4 : 0)
          | (e & FE_OVERFLOW ? 8 : 0) | (e & FE_UNDERFLOW ? 0x10 : 0)
          | (e & FE_INEXACT ? 0x20 : 0) | (denormal (a, dbl) ? 2 : 0);
  return r;
}

/* The 64/32 divides: a signal for a zero divisor, a quotient that
   doesn't fit 32 bits (div), or a dividend that overflows when shifted
   left by the divisor's leading zeros (mod; signed: by |divisor|'s, into
   a 65-bit signed value).  */
static struct res
divx (const char *n, u64 a, u64 b)
{
  struct res r = { 0, 0, 0 };
  if (n[0] == 'u')
    {
      u64 y = (u32) b;
      if (!y)
        return (struct res) { 8, 0, 0 };
      unsigned z = __builtin_clz (y);
      if (!strcmp (n, "udivx") ? a / y >> 32 : z && a >> (64 - z))
        return (struct res) { 8, 0, 0 };
      r.v = (strcmp (n, "udivx") ? a % y : a / y) & 0xffffffff;
    }
  else
    {
      long x = a, y = (int) b;
      if (!y)
        return (struct res) { 8, 0, 0 };
      __int128 q = (__int128) x / y;
      unsigned z = __builtin_clz ((u32) (y < 0 ? -y : y));
      __int128 s = (__int128) x * ((__int128) 1 << z);
      __int128 lim = (__int128) 1 << 64;
      if (!strcmp (n, "sdivx") ? q < -0x80000000L || q > 0x7fffffffL
                               : s < -lim || s >= lim)
        return (struct res) { 8, 0, 0 };
      r.v = (u64) (strcmp (n, "sdivx") ? (long) ((__int128) x % y) : (long) q)
            & 0xffffffff;
    }
  r.v |= KEEP;
  return r;
}

/* The reference for operation N on set (A, B, C); 0 if N has none.  */
static int
model (const char *n, u64 a, u64 b, u64 c, struct res *r)
{
  static const char *cc[] = { "o", "b", "e", "be", "s", "p", "l", "le" };
  static const char *fc[] = { "eq", "lt", "le", "uod", "neq", "nlt", "nle",
                              "od" };
  char op[16];
  int bits, len = strlen (n);
  r->sig = 0, r->fl = 0;
  mxcsr = 0;
  for (int i = 0; i < 8; i++)
    {
      char s[24];
      /* cmp<c>sb: 32-bit integer compare into a predicate.  */
      snprintf (s, sizeof s, "cmp%ssb", cc[i]);
      if (!strcmp (n, s))
        {
          r->v = cctop (cc[i], xflags ("sub", a, b, 0, 32, 32));
          return 1;
        }
      snprintf (s, sizeof s, "cmpand%ssb", cc[i]);
      if (!strcmp (n, s))
        {
          r->v = cctop (cc[i], szp ((u32) (a & b), 32));
          return 1;
        }
      snprintf (s, sizeof s, "cctop%s", cc[i]);
      if (!strcmp (n, s))
        {
          r->v = cctop (cc[i], a);
          return 1;
        }
    }
  for (int i = 0; i < 8; i++)
    {
      char s[24];
      for (int d = 0; d < 2; d++)
        {
          snprintf (s, sizeof s, "fcmp%s%c", fc[i], d ? 'd' : 's');
          if (!strcmp (n, s))
            {
              int t = fcmp_sse (fc[i], d, a, b);
              r->v = d ? (t ? ~0UL : 0) : (t ? 0xffffffff : 0) | KEEP;
              r->fl = mxcsr;
              return 1;
            }
        }
      snprintf (s, sizeof s, "fcmp%ssb", fc[i]);
      if (!strcmp (n, s))
        {
          r->v = fcmp_sse (fc[i], 0, a, b);
          r->fl = mxcsr;
          return 1;
        }
    }
  if (!strcmp (n, "fsubd") || !strcmp (n, "fsubs") || !strcmp (n, "fmaxs")
      || !strcmp (n, "fmins") || !strcmp (n, "fmuls"))
    {
      int s = n[len - 1] == 's';
      if (n[1] == 'm' && n[2] == 'u')
        r->v = SSE_FF ("mulss", "", a, b);
      else if (n[1] == 's')
        r->v = s ? SSE_FF ("subss", "", a, b) : SSE_FF ("subsd", "", a, b);
      else if (n[2] == 'a')
        r->v = SSE_FF ("maxss", "", a, b);
      else
        r->v = SSE_FF ("minss", "", a, b);
      if (s)
        r->v = (r->v & 0xffffffff) | KEEP;
      r->fl = mxcsr;
      return 1;
    }
  if (!strcmp (n, "fdtoifd") || !strcmp (n, "fstoifs"))
    {
      r->v = round_sse (n[1] == 'd', a, b);
      r->fl = mxcsr;
      return 1;
    }
  if (!strncmp (n, "fscale", 6))
    {
      r->v = fscale (n[6] == 'd', a, b);
      r->fl = mxcsr;
      return 1;
    }
  if (!strcmp (n, "fstoid") || !strcmp (n, "fstoidtr")
      || !strcmp (n, "fdtoidtr") || !strcmp (n, "fdtoistr")
      || !strcmp (n, "idtofs") || !strcmp (n, "istofd")
      || !strcmp (n, "istofs") || !strcmp (n, "fdtoid")
      || !strcmp (n, "idtofd") || !strcmp (n, "fdtofs")
      || !strcmp (n, "fstofd"))
    {
      r->v = cvt (n, a);
      r->fl = mxcsr;
      return 1;
    }
  if (!strcmp (n, "insfs"))
    {
      /* Rotate src1 right by src2 bits 0-4, then take the low
         (src2 bits 6-10) bits from src3.  */
      u32 x = a, k = b & 31, l = (b >> 6) & 31, m = (1U << l) - 1;
      u32 rot = k ? x >> k | x << (32 - k) : x;
      r->v = ((rot & ~m) | ((u32) c & m)) | KEEP;
      return 1;
    }
  if (!strcmp (n, "insfd"))
    {
      /* The same with 6-bit fields: bits 0-5 and 6-11.  */
      u64 k = b & 63, l = (b >> 6) & 63, m = (1UL << l) - 1;
      u64 rot = k ? a >> k | a << (64 - k) : a;
      r->v = (rot & ~m) | (c & m);
      return 1;
    }
  if (!strcmp (n, "mpsadbh"))
    {
      r->v = 0;
      for (int i = 0; i < 4; i++)
        {
          unsigned s = 0;
          for (int j = 0; j < 4; j++)
            s += abs ((int) (a >> 8 * (i + j) & 0xff)
                      - (int) (b >> 8 * j & 0xff));
          r->v |= (u64) s << 16 * i;
        }
      return 1;
    }
  if (!strcmp (n, "umulx"))
    {
      r->v = (u64) (u32) a * (u32) b;
      return 1;
    }
  if (!strcmp (n, "smulx"))
    {
      r->v = (u64) ((long) (int) a * (int) b);
      return 1;
    }
  if (len == 5 && strstr (n, "divx") == n + 1)
    {
      *r = divx (n, a, b);
      return 1;
    }
  if (len == 5 && strstr (n, "modx") == n + 1)
    {
      *r = divx (n, a, b);
      return 1;
    }
  if (!strncmp (n, "merge_", 6))
    {
      /* merge_<y>: y (src3, merge (src1, src2, pred)); merge picks src1
         when the predicate is false.  */
      char y[16];
      snprintf (y, sizeof y, "%.*s", len - 8, n + 6);
      int wide = y[strlen (y) - 1] == 'd';
      y[strlen (y) - 1] = 0;
      u64 t = n[len - 1] == 't' ? b : a, v;
      if (!strcmp (y, "and")) v = c & t;
      else if (!strcmp (y, "andn")) v = c & ~t;
      else if (!strcmp (y, "or")) v = c | t;
      else if (!strcmp (y, "orn")) v = c | ~t;
      else if (!strcmp (y, "xor")) v = c ^ t;
      else if (!strcmp (y, "xorn")) v = c ^ ~t;
      else if (!strcmp (y, "add")) v = c + t;
      else if (!strcmp (y, "sub")) v = c - t;
      else if (!strcmp (y, "rsub")) v = t - c;
      else return 0;
      r->v = wide ? v : (v & 0xffffffff) | KEEP;
      return 1;
    }
  /* <op>{s,d}_f{b,h,w,d}: the x86 flags into a 32-bit result.  */
  if (len > 4 && n[len - 3] == '_' && n[len - 2] == 'f')
    {
      bits = n[len - 1] == 'b' ? 8 : n[len - 1] == 'h' ? 16
             : n[len - 1] == 'w' ? 32 : 64;
      int base = len - 3;
      int rbits = 32;
      if (!strncmp (n, "umulx", 5) || !strncmp (n, "smulx", 5))
        snprintf (op, sizeof op, "%.5s", n);
      else
        {
          rbits = n[base - 1] == 'd' ? 64 : 32;
          snprintf (op, sizeof op, "%.*s", base - 1, n);
        }
      r->v = xflags (op, a, b, c, bits, rbits) | KEEP;
      return 1;
    }
  return 0;
}

int
main (int argc, char **argv)
{
  int verbose = argc > 1 && !strcmp (argv[1], "-v");
  static u64 in[NSETS][3];
  semx_sets (in);
  char line[256], name[32], val[32];
  static char names[256][32];
  static long tot[256], bad[256], badfl[256], badsig[256], skip[256];
  int nn = 0, set;
  unsigned long fl;
  while (fgets (line, sizeof line, stdin))
    {
      int sig = 0;
      if (sscanf (line, "%31s %d %31s", name, &set, val) != 3
          || set < 0 || set >= NSETS)
        continue;
      if (!strcmp (val, "signal"))
        sig = atoi (strstr (line, "signal") + 6);
      else if (sscanf (line, "%*s %*d %*s flags %lx", &fl) != 1)
        continue;
      int k;
      for (k = 0; k < nn && strcmp (names[k], name); k++)
        ;
      if (k == nn)
        strcpy (names[nn++], name);
      struct res e;
      const u64 *s = in[set];
      if (!model (name, s[0], s[1], s[2], &e))
        {
          skip[k]++;
          continue;
        }
      tot[k]++;
      if (sig || e.sig)
        {
          if (sig != e.sig)
            {
              badsig[k]++;
              if (verbose)
                printf ("SIGNAL %s %2d: e2k %d model %d\n", name, set, sig,
                        e.sig);
            }
          continue;
        }
      u64 v = strtoul (val, NULL, 16);
      if (v != e.v)
        {
          bad[k]++;
          if (verbose)
            printf ("VALUE %s %2d %016lx %016lx %016lx: e2k %016lx model %016lx\n",
                    name, set, s[0], s[1], s[2], v, e.v);
        }
      else if ((fl & 0x3f) != e.fl)
        {
          badfl[k]++;
          if (verbose)
            printf ("FLAGS %s %2d %016lx %016lx: e2k %02lx model %02x\n", name,
                    set, s[0], s[1], fl & 0x3f, e.fl);
        }
    }
  long t = 0, b = 0, f = 0, g = 0, n0 = 0;
  for (int k = 0; k < nn; k++)
    {
      if (bad[k] || badfl[k] || badsig[k] || skip[k] || verbose)
        printf ("%-14s %4ld lines %3ld value %3ld flags %3ld signal %3ld noref\n",
                names[k], tot[k], bad[k], badfl[k], badsig[k], skip[k]);
      t += tot[k], b += bad[k], f += badfl[k], g += badsig[k], n0 += skip[k];
    }
  printf ("%d operations, %ld lines: %ld value, %ld flags, %ld signal "
          "differences, %ld without a model\n", nn, t, b, f, g, n0);
  return 0;
}
