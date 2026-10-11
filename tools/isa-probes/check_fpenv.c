/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Recompute fpenv.c's lines on an x86-64 host with SSE (and FMA for the
   v6 fmad/fmas) under the same control word, whose layout MXCSR shares
   with %pfpfr (masks 7-12, rounding 13-14, DAZ 6, FZ 15), and report
   the lines that differ, per operation and control setting.

     cc -O1 -mfma tools/isa-probes/check_fpenv.c -o check_fpenv
     ./check_fpenv < hw.txt          # summary
     ./check_fpenv -v < hw.txt       # and every differing line
     ./check_fpenv -n < hw.txt       # ignore the denormal flag (bit 1)

   A 32-bit result keeps the destination's upper half (0x55555555).  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fpenv-sets.h"

typedef unsigned int u32;
#define KEEP 0x5555555500000000UL

static u64 in[NSETS][10];

/* Run "insn" with MXCSR = CTL on xmm operands; the result is in X.  */
#define SSE2(insn, CTL, X, Y)                                           \
  ({ unsigned _c = (CTL); u64 _x = (X), _y = (Y);                       \
     __asm__ volatile ("movq %0, %%xmm0\n\tmovq %2, %%xmm1\n\t"         \
                       "ldmxcsr %1\n\t" insn " %%xmm1, %%xmm0\n\t"     \
                       "stmxcsr %1\n\tmovq %%xmm0, %0"                  \
                       : "+r" (_x), "+m" (_c) : "r" (_y)                 \
                       : "xmm0", "xmm1");                               \
     *fl = _c & 0x3f; _x; })

/* Conversions to an integer: "insn xmm1, reg".  */
#define CVT2I(insn, CTL, X)                                             \
  ({ unsigned _c = (CTL); u64 _x = (X), _r;                             \
     __asm__ volatile ("movq %2, %%xmm1\n\tldmxcsr %1\n\t"              \
                       insn " %%xmm1, %0\n\tstmxcsr %1"                 \
                       : "=r" (_r), "+m" (_c) : "r" (_x) : "xmm1");      \
     *fl = _c & 0x3f; _r; })

/* The same into a 32-bit register.  */
#define CVT2I32(insn, CTL, X)                                           \
  ({ unsigned _c = (CTL); u64 _x = (X); u32 _r;                         \
     __asm__ volatile ("movq %2, %%xmm1\n\tldmxcsr %1\n\t"              \
                       insn " %%xmm1, %k0\n\tstmxcsr %1"                \
                       : "=r" (_r), "+m" (_c) : "r" (_x) : "xmm1");      \
     *fl = _c & 0x3f; _r; })

/* From an integer register: "insn reg, xmm0".  */
#define CVTFI(insn, CTL, X)                                             \
  ({ unsigned _c = (CTL); u64 _x = (X), _r;                             \
     __asm__ volatile ("pxor %%xmm0, %%xmm0\n\tldmxcsr %1\n\t"          \
                       insn " %2, %%xmm0\n\tstmxcsr %1\n\t"             \
                       "movq %%xmm0, %0"                                \
                       : "=r" (_r), "+m" (_c) : "r" (_x) : "xmm0");      \
     *fl = _c & 0x3f; _r; })

#define CVTFI32(insn, CTL, X)                                           \
  ({ unsigned _c = (CTL); u32 _x = (X); u64 _r;                         \
     __asm__ volatile ("pxor %%xmm0, %%xmm0\n\tldmxcsr %1\n\t"          \
                       insn " %k2, %%xmm0\n\tstmxcsr %1\n\t"            \
                       "movq %%xmm0, %0"                                \
                       : "=r" (_r), "+m" (_c) : "r" (_x) : "xmm0");      \
     *fl = _c & 0x3f; _r; })

/* Three operands: xmm0 = a, xmm1 = b, xmm2 = c; "insn" leaves the
   result in xmm0.  */
#define SSE3(insn, CTL, A, B, C)                                        \
  ({ unsigned _c = (CTL); u64 _a = (A), _b = (B), _cc = (C);            \
     __asm__ volatile ("movq %0, %%xmm0\n\tmovq %2, %%xmm1\n\t"         \
                       "movq %3, %%xmm2\n\tldmxcsr %1\n\t" insn "\n\t"  \
                       "stmxcsr %1\n\tmovq %%xmm0, %0"                  \
                       : "+r" (_a), "+m" (_c) : "r" (_b), "r" (_cc)     \
                       : "xmm0", "xmm1", "xmm2");                       \
     *fl = _c & 0x3f; _a; })

static int
model (const char *n, const u64 *v, unsigned ctl, u64 *r, unsigned *fl)
{
  u64 a = v[0], b = v[1], c = v[2], sa = v[3], sb = v[4], sc = v[5];
  unsigned f2;
#define IS(s) (!strcmp (n, s))
  if (IS ("faddd") || IS ("pfaddd"))
    *r = SSE2 ("addsd", ctl, a, b);
  else if (IS ("fsubd"))
    *r = SSE2 ("subsd", ctl, a, b);
  else if (IS ("fmuld") || IS ("pfmuld"))
    *r = SSE2 ("mulsd", ctl, a, b);
  else if (IS ("fdivd"))
    *r = SSE2 ("divsd", ctl, a, b);
  else if (IS ("fsqrtd"))
    *r = SSE2 ("sqrtsd", ctl, 0, a);
  else if (IS ("fmaxd"))
    *r = SSE2 ("maxsd", ctl, a, b);
  else if (IS ("fcmpltd"))
    *r = SSE2 ("cmpltsd", ctl, a, b);
  else if (IS ("fcmpeqd"))
    *r = SSE2 ("cmpeqsd", ctl, a, b);
  else if (IS ("fmul_addd"))
    {
      u64 p = SSE2 ("mulsd", ctl, a, b);
      f2 = *fl;
      *r = SSE2 ("addsd", ctl, p, c);
      *fl |= f2;
    }
  else if (IS ("fadds"))
    *r = KEEP | (u32) SSE2 ("addss", ctl, sa, sb);
  else if (IS ("fsubs"))
    *r = KEEP | (u32) SSE2 ("subss", ctl, sa, sb);
  else if (IS ("fmuls"))
    *r = KEEP | (u32) SSE2 ("mulss", ctl, sa, sb);
  else if (IS ("fdivs"))
    *r = KEEP | (u32) SSE2 ("divss", ctl, sa, sb);
  else if (IS ("fsqrts"))
    *r = KEEP | (u32) SSE2 ("sqrtss", ctl, 0, sa);
  else if (IS ("fmul_adds"))
    {
      u64 p = SSE2 ("mulss", ctl, sa, sb);
      f2 = *fl;
      *r = KEEP | (u32) SSE2 ("addss", ctl, p, sc);
      *fl |= f2;
    }
  else if (IS ("fdtois"))
    *r = KEEP | (u32) CVT2I32 ("cvtsd2si", ctl, a);
  else if (IS ("fdtoistr"))
    *r = KEEP | (u32) CVT2I32 ("cvttsd2si", ctl, a);
  else if (IS ("fstois"))
    *r = KEEP | (u32) CVT2I32 ("cvtss2si", ctl, sa);
  else if (IS ("fdtoid"))
    *r = CVT2I ("cvtsd2siq", ctl, a);
  else if (IS ("fstoid"))
    *r = CVT2I ("cvtss2siq", ctl, sa);
  else if (IS ("fdtofs"))
    *r = KEEP | (u32) SSE2 ("cvtsd2ss", ctl, 0, a);
  else if (IS ("fstofd"))
    *r = SSE2 ("cvtss2sd", ctl, 0, sa);
  else if (IS ("idtofd"))
    *r = CVTFI ("cvtsi2sdq", ctl, v[6]);
  else if (IS ("idtofs"))
    *r = KEEP | (u32) CVTFI ("cvtsi2ssq", ctl, v[6]);
  else if (IS ("istofs"))
    *r = KEEP | (u32) CVTFI32 ("cvtsi2ssl", ctl, (u32) v[7]);
  else if (IS ("istofd"))
    *r = CVTFI32 ("cvtsi2sdl", ctl, (u32) v[7]);
  else if (IS ("pfadds"))
    *r = SSE2 ("addps", ctl, v[8], v[9]);
  else if (IS ("pfsubs"))
    *r = SSE2 ("subps", ctl, v[8], v[9]);
  else if (IS ("pfmuls"))
    *r = SSE2 ("mulps", ctl, v[8], v[9]);
  else if (IS ("fmad"))
    *r = SSE3 ("vfmadd213sd %%xmm2, %%xmm1, %%xmm0", ctl, a, b, c);
  else if (IS ("fmas"))
    *r = KEEP | (u32) SSE3 ("vfmadd213ss %%xmm2, %%xmm1, %%xmm0", ctl,
                            sa, sb, sc);
  else
    return 0;
#undef IS
  return 1;
}

int
main (int argc, char **argv)
{
  int verbose = argc > 1 && !strcmp (argv[1], "-v");
  int nodenorm = argc > 1 && !strcmp (argv[1], "-n");
  fe_sets (in);
  char name[32];
  int s;
  unsigned ctl;
  char val[64];
  unsigned flags;
  /* Per operation and control word: lines, differing values, differing
     flags.  */
  static char names[64][16];
  static long tot[64][16], dv[64][16], df[64][16];
  int nn = 0;
  long lines = 0, bad = 0;
  char line[256];
  while (fgets (line, sizeof line, stdin))
    {
      if (sscanf (line, "%31s %d %x %63s %x", name, &s, &ctl, val, &flags)
          < 4)
        continue;
      int o;
      for (o = 0; o < nn && strcmp (names[o], name); o++)
        ;
      if (o == nn)
        strcpy (names[nn++], name);
      int k = (ctl >> 13 & 3) | (ctl & 0x40 ? 4 : 0) | (ctl & 0x8000 ? 8 : 0);
      tot[o][k]++;
      lines++;
      u64 r;
      unsigned fl = 0;
      if (!strcmp (val, "signal"))
        {
          dv[o][k]++;
          bad++;
          if (verbose)
            printf ("%s: signal\n", line);
          continue;
        }
      if (!model (name, in[s], ctl, &r, &fl))
        {
          printf ("no model for %s\n", name);
          return 1;
        }
      u64 got = strtoul (val, 0, 16);
      unsigned m = nodenorm ? 0x3d : 0x3f;
      int bv = got != r, bf = (flags & m) != (fl & m);
      dv[o][k] += bv;
      df[o][k] += bf;
      if (bv || bf)
        {
          bad++;
          if (verbose)
            printf ("%-9s %4d %04x got %016lx %02x want %016lx %02x  "
                    "(in %016lx %016lx | %08lx %08lx | %lx %lx)\n",
                    name, s, ctl, got, flags, r, fl, in[s][0], in[s][1],
                    in[s][3], in[s][4], in[s][6], in[s][7]);
        }
    }
  printf ("%ld lines, %ld differ\n", lines, bad);
  for (int o = 0; o < nn; o++)
    for (int k = 0; k < 16; k++)
      if (dv[o][k] || df[o][k])
        printf ("  %-9s rc %d%s%s: %ld of %ld values, %ld flags differ\n",
                names[o], k & 3, k & 4 ? " daz" : "", k & 8 ? " fz" : "",
                dv[o][k], tot[o][k], df[o][k]);
  return 0;
}
