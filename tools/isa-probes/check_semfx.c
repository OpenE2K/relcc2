/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Recompute semfx.c's output lines with the host's x87 (x86 only) and
   report the lines that differ (spec/isa/semantics.md, "80-bit
   operations").

     cc -O1 tools/isa-probes/check_semfx.c -o check_semfx
     ./check_semfx < hw.txt               # summary per operation
     ./check_semfx -v < hw.txt            # and every differing line

   An operation fx<op><b><r> is loaded as x87 operands (an 80-bit value,
   or a single or double converted exactly), computed under %fpcr's own
   value as the x87 control word, and stored as r, as x87 code would: an
   s or d result is rounded first to the precision control's precision,
   then to its own format (twice, when PC is the wider).  The column
   "once" counts the lines that match only a single rounding to the
   result's format instead.  Compares use fucomi (quiet) or fcomi
   (signalling).  fxdivt* and the separate square-root steps
   have no x87 counterpart and are skipped.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long u64;
typedef unsigned short u16;

struct x80 { u64 m; u16 e; } __attribute__ ((packed));

static unsigned short sw;

static void
setcw (unsigned cw)
{
  unsigned short c = cw;
  __asm__ volatile ("fninit; fldcw %0" : : "m" (c));
}

static void
getsw (void)
{
  __asm__ volatile ("fnstsw %0" : "=m" (sw));
}

/* Push operand of type T from LO/HI.  */
static void
push (char t, int intop, u64 lo, u64 hi)
{
  if (intop)
    {
      if (t == 'd')
        {
          long v = lo;
          __asm__ volatile ("fildq %0" : : "m" (v));
        }
      else
        {
          int v = lo;
          __asm__ volatile ("fildl %0" : : "m" (v));
        }
      return;
    }
  if (t == 'x')
    {
      struct x80 v = { lo, hi };
      __asm__ volatile ("fldt %0" : : "m" (v));
    }
  else if (t == 'd')
    __asm__ volatile ("fldl %0" : : "m" (lo));
  else
    {
      unsigned v = lo;
      __asm__ volatile ("flds %0" : : "m" (v));
    }
}

/* Pop ST0 as type T (int: rounding by the control word, or truncating).  */
static void
pop (char t, int intop, int trunc, u64 *lo, u64 *hi)
{
  *lo = 0;
  *hi = 0;
  if (intop)
    {
      if (t == 'd')
        {
          long v;
          if (trunc)
            __asm__ volatile ("fisttpq %0" : "=m" (v));
          else
            __asm__ volatile ("fistpq %0" : "=m" (v));
          *lo = v;
        }
      else
        {
          int v;
          if (trunc)
            __asm__ volatile ("fisttpl %0" : "=m" (v));
          else
            __asm__ volatile ("fistpl %0" : "=m" (v));
          *lo = (unsigned) v;
        }
      return;
    }
  if (t == 'x')
    {
      struct x80 v;
      __asm__ volatile ("fstpt %0" : "=m" (v));
      *lo = v.m;
      *hi = v.e;
    }
  else if (t == 'd')
    __asm__ volatile ("fstpl %0" : "=m" (*lo));
  else
    {
      unsigned v;
      __asm__ volatile ("fstps %0" : "=m" (v));
      *lo = v;
    }
}

/* Compute NAME on the operands; returns 0 if it has no reference.  */
static int
ref (const char *name, unsigned cw, u64 a0, u64 a1, u64 b0, u64 b1,
     u64 *r0, u64 *r1, unsigned *flags, int once)
{
  char op[16] = "", tb = 0, tr = 0;
  int n = strlen (name);
  if (!strncmp (name, "fxcmp", 5))
    {
      char c[8] = "";
      int k = n - 2;                     /* type, then b or f */
      tb = name[k];
      memcpy (c, name + 5, k - 5);
      c[k - 5] = 0;
      setcw (cw);
      push ('x', 0, a0, a1);
      push (tb, 0, b0, b1);              /* ST0 = b, ST1 = a */
      __asm__ volatile ("fxch");         /* ST0 = a, ST1 = b */
      unsigned char cf, zf, pf;
      /* od into a predicate is quiet, into a register (f) signalling,
         as fcmpod*f.  */
      int quiet = !strcmp (c, "eq") || !strcmp (c, "neq") || !strcmp (c, "uod")
                  || (!strcmp (c, "od") && name[n - 1] == 'b')
                  || !strcmp (c, "ud");
      if (quiet)
        __asm__ volatile ("fucomip %%st(1), %%st; setc %0; setz %1; setp %2"
                          : "=q" (cf), "=q" (zf), "=q" (pf));
      else
        __asm__ volatile ("fcomip %%st(1), %%st; setc %0; setz %1; setp %2"
                          : "=q" (cf), "=q" (zf), "=q" (pf));
      __asm__ volatile ("fstp %st(0)");
      getsw ();
      int un = pf, lt = cf && !pf, eq = zf && !pf;
      *flags = sw & 0x3f;
      *r0 = 0;
      if (name[n - 1] == 'f')
        {
          /* Register result: x86 flags layout (ZF 0x40, PF 0x04, CF 0x01).  */
          *r0 = un ? 0x45 : lt ? 0x01 : eq ? 0x40 : 0;
          *r1 = 0;
          return 2;
        }
      int v = !strcmp (c, "eq") ? eq : !strcmp (c, "lt") ? lt
              : !strcmp (c, "le") ? (lt || eq) : !strcmp (c, "uod") ? un
              : !strcmp (c, "neq") ? !eq : !strcmp (c, "nlt") ? !lt
              : !strcmp (c, "nle") ? !(lt || eq) : !un;
      *r0 = 0x5555555555555555UL;
      *r1 = v;
      return 1;
    }
  if (!strncmp (name, "fxsqrtchain", 11))
    {
      /* fxsqrti, fxsqrtu, fxsqrtt as lcc chains them: the 80-bit root.  */
      setcw (cw);
      push (name[11], 0, a0, a1);
      __asm__ volatile ("fsqrt");
      pop ('x', 0, 0, r0, r1);
      getsw ();
      *flags = sw & 0x23f;
      return 1;
    }
  if (!strncmp (name, "fxdivt", 6) || !strncmp (name, "fxsqrt", 6))
    return 0;
  setcw (cw);
  if (!strcmp (name, "movx") || !strcmp (name, "movxa")
      || !strcmp (name, "movxc"))
    {
      push ('x', 0, a0, a1);
      if (name[4] == 'a')
        __asm__ volatile ("fabs");
      else if (name[4] == 'c')
        __asm__ volatile ("fchs");
      pop ('x', 0, 0, r0, r1);
    }
  else if (!strcmp (name, "idtofx") || !strcmp (name, "istofx"))
    {
      push (name[1] == 'd' ? 'd' : 's', 1, a0, 0);
      pop ('x', 0, 0, r0, r1);
    }
  else if (!strcmp (name, "fstofx") || !strcmp (name, "fdtofx"))
    {
      push (name[1], 0, a0, 0);
      pop ('x', 0, 0, r0, r1);
    }
  else if (!strncmp (name, "fxto", 4))
    {
      const char *t = name + 4;
      push ('x', 0, a0, a1);
      if (!strcmp (t, "fs") || !strcmp (t, "fd"))
        pop (t[1], 0, 0, r0, r1);
      else
        pop (t[1], 1, strstr (t, "tr") != NULL, r0, r1);
    }
  else if (!strncmp (name, "fxscale", 7))
    {
      push ('s', 1, b0, 0);
      push ('x', 0, a0, a1);
      /* x87 fscale is exact; fxscale rounds to the precision control
         like an arithmetic operation, which fmul by 1 reproduces (only
         below extended precision: on a tiny result it would also raise
         the denormal flag).  */
      __asm__ volatile ("fscale; fstp %st(1)");
      if (((cw >> 8) & 3) != 3)
        __asm__ volatile ("fld1; fmulp");
      pop ('x', 0, 0, r0, r1);
    }
  else
    {
      /* fx<op><b><r> */
      tb = name[n - 2];
      tr = name[n - 1];
      memcpy (op, name + 2, n - 4);
      op[n - 4] = 0;
      if (once && tr != 'x')
        setcw ((cw & ~0x300) | (tr == 'd' ? 0x200 : 0));
      push ('x', 0, a0, a1);
      push (tb, 0, b0, b1);              /* ST0 = b, ST1 = a */
      if (!strcmp (op, "add"))
        __asm__ volatile ("faddp");
      else if (!strcmp (op, "sub"))
        __asm__ volatile ("fsubrp");     /* ST1 - ST0 = a - b */
      else if (!strcmp (op, "rsub"))
        __asm__ volatile ("fsubp");      /* b - a */
      else if (!strcmp (op, "mul"))
        __asm__ volatile ("fmulp");
      else if (!strcmp (op, "div"))
        __asm__ volatile ("fdivrp");     /* a / b */
      else
        return 0;
      pop (tr, 0, 0, r0, r1);
    }
  getsw ();
  *flags = sw & 0x23f;
  return 1;
}

int
main (int argc, char **argv)
{
  int verbose = argc > 1 && !strcmp (argv[1], "-v");
  char name[32], arrow[4];
  unsigned cw;
  u64 a0, a1, b0, b1, r0, r1, fl;
  static char names[256][32];
  static long tot[256], bad[256], badfl[256], skip[256], once[256];
  int nn = 0;
  while (scanf ("%31s %x %lx %lx %lx %lx %3s %lx %lx %lx", name, &cw, &a0,
                &a1, &b0, &b1, arrow, &r0, &r1, &fl) == 10)
    {
      int k;
      for (k = 0; k < nn && strcmp (names[k], name); k++)
        ;
      if (k == nn)
        strcpy (names[nn++], name);
      u64 e0, e1;
      unsigned ef;
      int kind = ref (name, cw, a0, a1, b0, b1, &e0, &e1, &ef, 0);
      if (!kind)
        {
          skip[k]++;
          continue;
        }
      tot[k]++;
      int n = strlen (name);
      char tr = !strncmp (name, "fxto", 4) ? name[5] : name[n - 1];
      if (!strncmp (name, "fxsqrtchain", 11))
        tr = 'x';
      /* 32-bit results (singles, 32-bit integers, register compares)
         keep the destination's upper half.  */
      u64 mask = tr == 's' || (!strncmp (name, "fxcmp", 5) && tr == 'f')
                 ? 0xffffffff : ~0UL;
      int same = (e0 & mask) == (r0 & mask) && (e1 & 0xffff) == (r1 & 0xffff);
      if (kind == 1 && !same && strncmp (name, "fxcmp", 5))
        {
          u64 t0, t1;
          unsigned tf;
          ref (name, cw, a0, a1, b0, b1, &t0, &t1, &tf, 1);
          if ((t0 & mask) == (r0 & mask) && (t1 & 0xffff) == (r1 & 0xffff))
            {
              once[k]++;
              same = 1;
              ef = tf;
            }
        }
      if (!same)
        {
          bad[k]++;
          if (verbose)
            printf ("VALUE %s %03x %016lx %04lx %016lx %04lx: e2k %016lx %04lx,"
                    " x87 %016lx %04lx\n", name, cw, a0, a1, b0, b1, r0, r1,
                    e0, e1 & 0xffff);
        }
      else if ((ef & 0x3f) != (fl & 0x3f))
        {
          badfl[k]++;
          if (verbose)
            printf ("FLAGS %s %03x %016lx %04lx %016lx %04lx: e2k %02lx x87 %02x\n",
                    name, cw, a0, a1, b0, b1, fl & 0x3f, ef & 0x3f);
        }
    }
  printf ("%-12s %6s %6s %6s %6s %6s\n", "op", "lines", "value", "flags",
          "once", "noref");
  for (int k = 0; k < nn; k++)
    printf ("%-12s %6ld %6ld %6ld %6ld %6ld\n", names[k], tot[k], bad[k],
            badfl[k], once[k], skip[k]);
  return 0;
}
