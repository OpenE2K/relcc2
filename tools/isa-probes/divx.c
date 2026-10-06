/* SPDX-License-Identifier: GPL-3.0-or-later */
/* When the 64/32 divides sdivx, udivx, smodx and umodx trap
   (spec/isa/semantics.md, "Other scalar operations"): for divisors Y
   and quotients Q around 2^31..2^34 (dividend Q * Y + R, R = 0 or
   Y - 1), each operation's 32-bit result or SIGFPE.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $LCC -O3 tools/isa-probes/divx.c tools/isa-probes/divx.s -o divx
     ./divx > hw.txt                      # on hardware

   One line per operation, dividend and divisor: X Y, the true quotient
   and remainder, then the result (the destination's low 32 bits) or
   "SIGFPE".  */

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>

typedef unsigned long u64;

u64 sdivx_ (u64, u64), udivx_ (u64, u64), smodx_ (u64, u64),
    umodx_ (u64, u64);

static sigjmp_buf env;

static void
handler (int s)
{
  (void) s;
  siglongjmp (env, 1);
}

static void
run (const char *name, u64 (*f) (u64, u64), u64 x, u64 y, int sgn)
{
  printf ("%s %016lx %08lx ", name, x, y & 0xffffffff);
  if (sgn)
    {
      long sx = x, sy = (int) y;
      if (sy)
        printf ("q %lx r %lx ", sx / sy, sx % sy);
    }
  else if ((unsigned) y)
    printf ("q %lx r %lx ", x / (unsigned) y, x % (unsigned) y);
  if (sigsetjmp (env, 1))
    {
      printf ("SIGFPE\n");
      return;
    }
  printf ("%08lx\n", f (x, y) & 0xffffffff);
}

int
main (void)
{
  static const u64 ys[] = { 1, 3, 0x10, 0x12345678, 0x7fc00000, 0x7fffffff,
                            0x80000000, 0x80000001, 0xc0000000, 0xffffffff };
  static const u64 qs[] = { 0x7fffffff, 0x80000000, 0xffffffff, 0x100000000,
                            0x100000001, 0x17fffffff, 0x180000000,
                            0x1fffffffe, 0x1ffffffff, 0x200000000,
                            0x200000001, 0x300000000, 0x3ffffffff };
  signal (SIGFPE, handler);
  setvbuf (stdout, NULL, _IOLBF, 0);
  for (unsigned i = 0; i < sizeof ys / sizeof ys[0]; i++)
    for (unsigned j = 0; j < sizeof qs / sizeof qs[0]; j++)
      for (int k = 0; k < 2; k++)
        {
          u64 y = ys[i], r = k ? y - 1 : 0;
          unsigned __int128 x128 = (unsigned __int128) qs[j] * y + r;
          if (x128 >> 64)
            continue;
          u64 x = x128;
          run ("udivx", udivx_, x, y, 0);
          run ("umodx", umodx_, x, y, 0);
          if ((long) y > 0 && (int) y > 0 && (long) x >= 0)
            {
              run ("sdivx", sdivx_, x, y, 1);
              run ("smodx", smodx_, x, y, 1);
              run ("sdivx", sdivx_, -x, y, 1);
              run ("smodx", smodx_, -x, y, 1);
            }
        }
  return 0;
}
