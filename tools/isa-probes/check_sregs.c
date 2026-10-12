/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Recompute the "dt" lines of sregs.c (the fxdivt* operations) with the
   host's x87 (x86 only) and report the lines that differ, and sort the
   "c1" lines by what x87 does to C1 (spec/isa/semantics.md, "80-bit
   operations"; spec/abi/registers.md, "Floating-point control and
   status").

     cc -O1 tools/isa-probes/check_sregs.c -o check_sregs
     ./check_sregs < out.txt             # summary per operation and %fpcr
     ./check_sregs -v < out.txt          # and every differing line

   "c1" lines: the operation is redone with x87 code (fld, the
   arithmetic, then fstpt, fstpl or fistpq for the result's type); a
   line's class is its x87 exception (ie, ze, oe, ue, pe, de, by that
   priority, or exact), "up" when x87 sets C1 (the arithmetic or the
   store rounded up), and the table counts, per operation, %fpsr preset
   and class, the lines where the e2k operation left C1 clear or set.
   Values are compared too ("value" counts lines that differ).

   Model: fxdivt<a><r> src1, src2 is src1 / src2 with src1 a single (a =
   s) or double (d) and src2 an 80-bit value, computed as x87 code would
   (fld src1; fld src2; fdivrp under %fpcr as the control word) and
   stored as r (s, d or the 80-bit x): an s or d result is rounded to the
   precision control first, then to its own format.  The column "once"
   counts the lines that match only a single rounding straight to the
   result's format (precision control set to it), "flags" the lines
   whose value matches but flags don't, and "tell" the lines where the
   two roundings give different values.  Flags compared: %fpsr bits 0-5
   and 9 (C1).  x87 code's C1 is that of its last instruction, and an
   exact store clears it, so the reference C1 is the division's for an
   80-bit result and, for s and d, the store's if it rounded up, else the
   division's.  */

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
ref (char a, char r, unsigned cw, u64 src1, u64 ym, u64 ye, u64 *r0,
     u64 *r1, unsigned *flags, unsigned *c1div)
{
  setcw (cw);
  if (a == 'd')
    __asm__ volatile ("fldl %0" : : "m" (src1));
  else
    {
      unsigned v = src1;
      __asm__ volatile ("flds %0" : : "m" (v));
    }
  struct x80 y = { ym, ye };
  __asm__ volatile ("fldt %0" : : "m" (y));     /* ST0 = y, ST1 = a */
  __asm__ volatile ("fdivrp");                  /* a / y */
  __asm__ volatile ("fnstsw %0" : "=m" (sw));
  *c1div = sw & 0x200;
  *r0 = *r1 = 0;
  if (r == 'x')
    {
      struct x80 v;
      __asm__ volatile ("fstpt %0" : "=m" (v));
      *r0 = v.m;
      *r1 = v.e;
    }
  else if (r == 'd')
    __asm__ volatile ("fstpl %0" : "=m" (*r0));
  else
    {
      unsigned v;
      __asm__ volatile ("fstps %0" : "=m" (v));
      *r0 = v;
    }
  __asm__ volatile ("fnstsw %0" : "=m" (sw));
  *flags = sw & 0x23f;
}

/* x87 code for a "c1" line: OP on A (and B), result in R0/R1, status
   words after the arithmetic (SW1) and after the store (SW2).  */
static int
c1ref (const char *op, u64 am, u64 ae, u64 bm, u64 be, u64 *r0, u64 *r1,
       unsigned *sw1, unsigned *sw2)
{
  struct x80 a = { am, ae }, b = { bm, be };
  unsigned short w;
  char res = 'x';
  setcw (0x37f);
  __asm__ volatile ("fldt %0" : : "m" (a));
  if (!strcmp (op, "fxtofd"))
    res = 'd';
  else if (!strcmp (op, "fxtoid"))
    res = 'i';
  else if (!strcmp (op, "fxsqrt"))
    __asm__ volatile ("fsqrt");
  else
    {
      __asm__ volatile ("fldt %0" : : "m" (b));     /* ST0 = b, ST1 = a */
      if (!strcmp (op, "fxaddxx"))
        __asm__ volatile ("faddp");
      else if (!strcmp (op, "fxsubxx"))
        __asm__ volatile ("fsubrp");                /* a - b */
      else if (!strcmp (op, "fxmulxx"))
        __asm__ volatile ("fmulp");
      else if (!strcmp (op, "fxdivxx") || !strcmp (op, "fxdivxd"))
        __asm__ volatile ("fdivrp");                /* a / b */
      else
        return 0;
      if (op[6] == 'd')
        res = 'd';
    }
  __asm__ volatile ("fnstsw %0" : "=m" (w));
  *sw1 = w;
  *r0 = *r1 = 0;
  if (res == 'x')
    {
      struct x80 v;
      __asm__ volatile ("fstpt %0" : "=m" (v));
      *r0 = v.m;
      *r1 = v.e;
    }
  else if (res == 'd')
    __asm__ volatile ("fstpl %0" : "=m" (*r0));
  else
    {
      long v;
      __asm__ volatile ("fistpq %0" : "=m" (v));
      *r0 = v;
    }
  __asm__ volatile ("fnstsw %0" : "=m" (w));
  *sw2 = w;
  return res;
}

static struct c1row { char op[16]; unsigned pre; char cls[8]; int up;
                      long n[2]; } c1rows[512];
static int nc1;
static long c1lines, c1value;

static void
c1line (const char *line, int verbose)
{
  char op[32];
  unsigned pre;
  u64 am, ae, bm, be, h0, h1, hf, r0, r1;
  unsigned sw1, sw2;
  if (sscanf (line, "c1 %31s %x %lx %lx %lx %lx -> %lx %lx %lx", op, &pre,
              &am, &ae, &bm, &be, &h0, &h1, &hf) != 9)
    return;
  int res = c1ref (op, am, ae, bm, be, &r0, &r1, &sw1, &sw2);
  if (!res)
    return;
  c1lines++;
  if (h0 != r0 || (res == 'x' && (h1 & 0xffff) != r1))
    {
      c1value++;
      if (verbose)
        printf ("C1VALUE %s    x87 %016lx %04lx %04x\n", line, r0, r1, sw2);
    }
  unsigned f = sw2 & 0x3f;
  const char *cls = f & 1 ? "ie" : f & 4 ? "ze" : f & 8 ? "oe"
                    : f & 0x10 ? "ue" : f & 0x20 ? "pe" : f & 2 ? "de"
                    : "exact";
  int up = ((sw1 | sw2) & 0x200) != 0;
  int k;
  for (k = 0; k < nc1; k++)
    if (!strcmp (c1rows[k].op, op) && c1rows[k].pre == pre
        && !strcmp (c1rows[k].cls, cls) && c1rows[k].up == up)
      break;
  if (k == nc1)
    {
      strcpy (c1rows[k].op, op);
      c1rows[k].pre = pre;
      strcpy (c1rows[k].cls, cls);
      c1rows[k].up = up;
      nc1++;
    }
  c1rows[k].n[(hf & 0x200) != 0]++;
}

int
main (int argc, char **argv)
{
  int verbose = argc > 1 && !strcmp (argv[1], "-v");
  char line[512], name[32];
  struct { char name[32]; unsigned cw; long n, bad, once, flagsonly, tell; } st[64];
  int ns = 0;
  long total = 0, bad = 0;
  while (fgets (line, sizeof line, stdin))
    {
      unsigned cw;
      u64 a, ym, ye, h0, h1, hf;
      if (!strncmp (line, "c1 ", 3))
        {
          line[strcspn (line, "\n")] = 0;
          c1line (line, verbose);
          continue;
        }
      if (sscanf (line, "dt %31s %x %lx %lx %lx -> %lx %lx %lx", name, &cw,
                  &a, &ym, &ye, &h0, &h1, &hf) != 8)
        continue;
      char ta = name[6], tr = name[7];
      u64 r0, r1, r0o, r1o;
      unsigned f, fo, c1d, c1do;
      ref (ta, tr, cw, a, ym, ye, &r0, &r1, &f, &c1d);
      /* A single rounding to the result's format.  */
      ref (ta, tr, tr == 'x' ? cw : (cw & ~0x300) | (tr == 'd' ? 0x200 : 0),
           a, ym, ye, &r0o, &r1o, &fo, &c1do);
      /* x87 sets C1 from the last instruction: the store clears it when
         it is exact.  Take the division's C1 for an 80-bit result, and
         the store's or else the division's for s and d.  */
      if (tr == 'x' || !(f & 0x200))
        f = (f & ~0x200) | c1d;
      if (tr == 'x' || !(fo & 0x200))
        fo = (fo & ~0x200) | c1do;
      int k;
      for (k = 0; k < ns; k++)
        if (!strcmp (st[k].name, name) && st[k].cw == cw)
          break;
      if (k == ns)
        {
          strcpy (st[ns].name, name);
          st[ns].cw = cw;
          st[ns].n = st[ns].bad = st[ns].once = st[ns].flagsonly = st[ns].tell = 0;
          ns++;
        }
      st[k].n++;
      total++;
      if (r0 != r0o || r1 != r1o)
        st[k].tell++;
      int same = h0 == r0 && h1 == r1;
      if (same && hf == f)
        continue;
      if (h0 == r0o && h1 == r1o && hf == fo)
        st[k].once++;
      if (same)
        st[k].flagsonly++;
      st[k].bad++;
      bad++;
      if (verbose)
        printf ("DIFF %s    x87 %016lx %04lx %04x\n", strtok (line, "\n"),
                r0, r1, f);
    }
  printf ("%-10s %5s %6s %6s %6s %6s %6s\n", "op", "fpcr", "lines",
          "differ", "once", "flags", "tell");
  for (int k = 0; k < ns; k++)
    printf ("%-10s %5x %6ld %6ld %6ld %6ld %6ld\n", st[k].name, st[k].cw,
            st[k].n, st[k].bad, st[k].once, st[k].flagsonly, st[k].tell);
  printf ("total %ld lines, %ld differ from x87\n", total, bad);
  if (c1lines)
    {
      printf ("\nc1: %ld lines, %ld values differ from x87\n"
              "%-8s %4s %-6s %4s %8s %8s\n", c1lines, c1value, "op",
              "pre", "class", "x87", "hw C1=0", "hw C1=1");
      for (int k = 0; k < nc1; k++)
        printf ("%-8s %4x %-6s %4s %8ld %8ld\n", c1rows[k].op,
                c1rows[k].pre, c1rows[k].cls, c1rows[k].up ? "up" : "-",
                c1rows[k].n[0], c1rows[k].n[1]);
    }
  return 0;
}
