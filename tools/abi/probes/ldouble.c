/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 80-bit long double on E2K: arithmetic, memory image, argument slots,
   FP state and the extended register parts (data-layout.md,
   registers.md, calling-convention.md).

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O1 tools/abi/probes/ldouble.c tools/abi/probes/ldouble.s -lm -o ldouble
     ./ldouble                     # on hardware, or under qemu-e2k

   Every line prints what was observed; lines starting with "FAIL"
   disagree with x87 extended precision.  */

#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

unsigned rd_pfpfr (void), rd_fpcr (void), rd_fpsr (void);
void wr_pfpfr (unsigned), wr_fpcr (unsigned), wr_fpsr (unsigned);
unsigned long keep_ext_nfx1 (unsigned long, unsigned long, void (*) (int), int);
unsigned long keep_ext_nfx0 (unsigned long, unsigned long, void (*) (int), int);

struct img { unsigned long mant; unsigned short exp; };

static struct img
bits (long double x)
{
  struct img r;
  memcpy (&r.mant, &x, 8);
  memcpy (&r.exp, (char *) &x + 8, 2);
  return r;
}

static long double
make (unsigned long mant, unsigned exp)
{
  long double x = 0;
  memcpy (&x, &mant, 8);
  memcpy ((char *) &x + 8, &exp, 2);
  return x;
}

static void
show (const char *what, long double x)
{
  struct img b = bits (x);
  printf ("  %-34s exp %04x mant %016lx\n", what, b.exp, b.mant);
}

static void
check (const char *what, int ok)
{
  printf ("%s %s\n", ok ? "ok  " : "FAIL", what);
}

__attribute__ ((noinline)) long double
add (long double a, long double b)
{
  return a + b;
}

volatile long double one = 1.0L, three = 3.0L, zero = 0.0L;
volatile int sink;

__attribute__ ((noinline)) void
deep (int n)
{
  volatile char pad[16];
  pad[0] = n;
  if (n > 0)
    deep (n - 1);
  sink = pad[0];
}

int
main (void)
{
  setvbuf (stdout, NULL, _IOLBF, 0);

  /* 1. Precision and rounding of the arithmetic.  */
  long double eps = ldexpl (1.0L, -63);
  check ("LDBL_MANT_DIG == 64", LDBL_MANT_DIG == 64);
  check ("(1 + 2^-63) - 1 == 2^-63", (one + eps) - one == eps);
  check ("1 + 2^-64 == 1 (ties to even)", one + eps / 2 == one);
  struct img third = bits (one / three);
  show ("1/3", one / three);
  check ("1/3 mantissa aaaa...aaab (round to nearest)",
         third.mant == 0xaaaaaaaaaaaaaaabUL && third.exp == 0x3ffd);
  long double r2 = sqrtl (2.0L);
  show ("sqrtl(2)", r2);
  check ("sqrtl(2) == 0xb504f333f9de6484 * 2^-63",
         bits (r2).mant == 0xb504f333f9de6484UL);
  long double tiny = ldexpl (1.0L, -16445);
  show ("2^-16445 (smallest denormal)", tiny);
  check ("smallest denormal is mantissa 1, exponent 0",
         bits (tiny).mant == 1 && bits (tiny).exp == 0);
  check ("2^-16445 / 2 == 0", tiny / 2 == 0);
  long double big = ldexpl (1.0L, 16383);
  check ("2^16383 * 2 == inf", isinf (big * 2));

  /* 2. Memory image: which of the 16 bytes a store writes.  */
  unsigned char buf[16];
  memset (buf, 0xaa, sizeof buf);
  *(volatile long double *) buf = one / three;
  printf ("  stored 1/3, buffer pre-filled with aa:");
  for (int i = 0; i < 16; i++)
    printf (" %02x", buf[i]);
  printf ("\n");
  check ("bytes 10..15 left untouched (padding undefined)",
         buf[10] == 0xaa && buf[15] == 0xaa);

  /* 3. Exponent slot: does the callee use only bits 0..15?  */
  long double (*addu) (unsigned long, unsigned long, unsigned long,
                       unsigned long) = (void *) add;
  long double s = addu (0x8000000000000000UL, 0x123456789abc3fffUL,
                        0x8000000000000000UL, 0xfedcba9876543fffUL);
  show ("1 + 1 with garbage in exponent slots", s);
  check ("callee ignores bits 16..63 of the exponent slot", s == 2.0L);

  /* 4. Rounding mode: which register controls 80-bit operations.  */
  unsigned p0 = rd_pfpfr (), c0 = rd_fpcr (), s0 = rd_fpsr ();
  printf ("  initial %%pfpfr %08x %%fpcr %08x %%fpsr %08x\n", p0, c0, s0);
  fesetround (FE_UPWARD);
  printf ("  FE_UPWARD: %%pfpfr %08x %%fpcr %08x\n", rd_pfpfr (), rd_fpcr ());
  struct img up = bits (one / three);
  fesetround (FE_DOWNWARD);
  struct img down = bits (one / three);
  fesetround (FE_TONEAREST);
  check ("fesetround affects long double (up ...ab, down ...aa)",
         up.mant == 0xaaaaaaaaaaaaaaabUL && down.mant == 0xaaaaaaaaaaaaaaaaUL);
  /* Downward in %pfpfr only, then in %fpcr only (x87 RC, bits 10-11).  */
  wr_pfpfr (p0 | 0x2000);
  struct img via_p = bits (one / three);
  wr_pfpfr (p0);
  wr_fpcr ((c0 & ~0xc00u) | 0x400);
  struct img via_c = bits (one / three);
  wr_fpcr (c0);
  printf ("  1/3 with RC=down only in %%pfpfr: mant %016lx\n", via_p.mant);
  printf ("  1/3 with RC=down only in %%fpcr:  mant %016lx\n", via_c.mant);

  /* 5. x87 precision control (%fpcr bits 8-9: 3 extended, 2 double).  */
  wr_fpcr ((c0 & ~0x300u) | 0x200);
  struct img pc = bits (one / three);
  wr_fpcr (c0);
  printf ("  1/3 with %%fpcr PC=double: mant %016lx (%s)\n", pc.mant,
          pc.mant == 0xaaaaaaaaaaaaa800UL ? "rounded to 53 bits"
          : pc.mant == 0xaaaaaaaaaaaaaaabUL ? "PC ignored" : "other");

  /* 6. Exception flags of 80-bit operations.  */
  feclearexcept (FE_ALL_EXCEPT);
  unsigned pf = rd_pfpfr (), sf = rd_fpsr ();
  volatile long double q = one / three;
  (void) q;
  printf ("  after 1/3: %%pfpfr flags %02x -> %02x, %%fpsr %04x -> %04x, "
          "fetestexcept(FE_INEXACT) %d\n", pf & 0x3f, rd_pfpfr () & 0x3f,
          sf & 0xffff, rd_fpsr () & 0xffff, !!fetestexcept (FE_INEXACT));
  feclearexcept (FE_ALL_EXCEPT);
  volatile long double z = one / zero;
  (void) z;
  printf ("  after 1/0: %%pfpfr flags %02x, %%fpsr %04x, "
          "fetestexcept(FE_DIVBYZERO) %d\n", rd_pfpfr () & 0x3f,
          rd_fpsr () & 0xffff, !!fetestexcept (FE_DIVBYZERO));
  feclearexcept (FE_ALL_EXCEPT);

  /* 7. Extended parts across a window spill.  */
  for (int d = 0; d <= 1000; d += 1000)
    {
      unsigned long e1 = keep_ext_nfx1 (0x8000000000000000UL, 0x3fff, deep, d);
      unsigned long e0 = keep_ext_nfx0 (0x8000000000000000UL, 0x3fff, deep, d);
      printf ("  depth %4d: 16-bit part after call, nfx=1 %04lx, nfx=0 %04lx\n",
              d, e1 & 0xffff, e0 & 0xffff);
    }

  /* 8. x87 special encodings (x86 treats unnormals and pseudo-NaNs as
     invalid operands).  */
  feclearexcept (FE_ALL_EXCEPT);
  show ("unnormal (int bit 0) + 0", make (0x4000000000000000UL, 0x3fff) + zero);
  printf ("    invalid raised: %d\n", !!fetestexcept (FE_INVALID));
  feclearexcept (FE_ALL_EXCEPT);
  show ("pseudo-NaN (exp 7fff, int bit 0) + 0",
        make (0x4000000000000000UL, 0x7fff) + zero);
  printf ("    invalid raised: %d\n", !!fetestexcept (FE_INVALID));
  feclearexcept (FE_ALL_EXCEPT);
  show ("pseudo-denormal (exp 0, int bit 1) + 0",
        make (0x8000000000000000UL, 0) + zero);
  show ("NaN + 1", (long double) NAN + one);
  return 0;
}
