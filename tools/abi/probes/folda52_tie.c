/* SPDX-License-Identifier: GPL-3.0-or-later */
/* An asm input tied ("0") to a wider output: what the output's upper
   half holds (spec/abi/source-compat.md, "Other constraints";
   docs/spec-poc-findings.md A52).  GCC's g++.dg/opt/pr23478.C ties
   "0" (1) to a long and loops on the result.  GCC leaves the upper
   bits undefined; on x86-64 a 32-bit write zero-extends anyway, on E2K
   a 32-bit operation under setwd dbl = 0 keeps the register's upper
   half (spec/abi/registers.md, "dbl").

   Read with -S, then run on hardware:

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   (or e2k-v5.8c2)
     $L -O2 -S tools/abi/probes/folda52_tie.c -o - | grep -A6 '^c1:'
     for o in -O0 -O2; do
       $L $o tools/abi/probes/folda52_tie.c tools/abi/probes/folda52_tie.s \
           -o folda52_tie$o
     done
     ./folda52_tie-O0; ./folda52_tie-O2         # 4C, 8C2 or 2C3

   fill (folda52_tie.s) leaves a pattern (upper half 0x5a5a5a5a) in the
   registers the next callee from main inherits; each case then calls a
   tied function 1,000 times with a new pattern and counts what the
   upper half held: 0 (zero-extended), the pattern (kept), or the sign
   of the 32-bit value (sign-extended).  The ww_* cases are the
   instruction lcc emits, written by hand: a 64-bit and a 32-bit write
   of one register in one wide instruction (spec/isa/bundle.md, "Two
   writes of one register"); "want" is the value if the higher channel
   writes only the low half and the lower channel the rest.

   lcc 1.29.16 (e2k-v3 and e2k-v5.8c2) and 1.31.05 (e2k-v3), 2026-11-03,
   -S: from -O1 on every tied function clears the output register with
   a 64-bit operation (addd,1 0x0, 0x0, %r3) and writes the input with a
   32-bit one under dbl = 0, in the same wide instruction for a
   constant or an argument (adds,2 0x1, 0x0, %r3; adds,3 0x0, %r0, %r3)
   or after it (a load, a narrow type).  At -O0 only the 32-bit write
   (adds,0,sm 0x1, 0x0, %r3).  A short or char input is computed in 32
   bits first: (short) (v + 1) became getfs (sign-extend 16 bits) and
   adds 1, so sh (0x7fff) is 0x8000, not 0xffffffffffff8000 (x86-64
   GCC 16.2.1 computes it the same way, leal 1(%rdi), %eax).  The same
   code for e2k-v5.8c2 and e2k-v6.2c3, and from 1.31.05.  c1nv (not
   volatile) gets %g16 at -O1 and up, outside the filled window, so
   there only lcc's own clear shows.

   Elbrus 4C (ALT; lcc 1.29.16 and 1.31.05 e2k-v3 binaries,
   2026-11-03) and Elbrus-8C2 (OS Elbrus 9.5.5; 1.29.16 e2k-v5.8c2
   binaries and the e2k-v3 -O2 one), the same on both:
     -O1, -O2: every tied case "zero-extended 1000"; sh (0x7fff) 0x8000,
          sh (-2) 0xffffffff, ch (-2) 0xffffffff;
     -O0: every tied case "kept 1000" (upper half 0x5a5a5a5a) except
          ldi (ldw zero-extends);
     ww_d0_s1, ww_d1_s3: 0x5a5a5a5a00000001 every time (the higher
     channel's 32-bit write replaces only the low half of the lower
     channel's 64-bit result, in one cluster or across); ww_s0_d1: the
     64-bit value (the 32-bit write lost); ww_d0_s1_dbl1: 0x1.
   Not run on the 2C3 (offline that day).
   Control, the proof-of-concept GCC 0.6+23 at -O2 (before it
   zero-extended such inputs) on the 4C: c1, cm1 and c1nv "kept 1000"
   (adds,0 0x0, 1, %r0 under dbl = 0), iarg (-1), sh (-2) and ch (-2)
   "sign-extended 1000" (the argument register as the caller extended
   it), the rest zero-extended.  */

#include <stdio.h>

void fill (long);
long ww_d0_s1 (long), ww_d1_s3 (long), ww_s0_d1 (long),
  ww_d0_s1_dbl1 (long);

#define NOINL __attribute__ ((noinline))

NOINL long c1 (void)
{ long r; __asm__ volatile ("" : "=r" (r) : "0" (1)); return r; }
NOINL long cm1 (void)
{ long r; __asm__ volatile ("" : "=r" (r) : "0" (-1)); return r; }
NOINL long iarg (int v)
{ long r; __asm__ volatile ("" : "=r" (r) : "0" (v)); return r; }
NOINL long uadd (unsigned v)
{ long r; __asm__ volatile ("" : "=r" (r) : "0" (v + 1)); return r; }
NOINL long iadd (int v)
{ long r; __asm__ volatile ("" : "=r" (r) : "0" (v - 2)); return r; }
NOINL long ldi (const int *p)
{ long r; __asm__ volatile ("" : "=r" (r) : "0" (*p)); return r; }
NOINL long sh (short v)
{ long r; __asm__ volatile ("" : "=r" (r) : "0" ((short) (v + 1))); return r; }
NOINL long ch (signed char v)
{ long r; __asm__ volatile ("" : "=r" (r) : "0" ((signed char) (v + 1))); return r; }
/* The pr23478 shape: a constant, another input, a memory clobber.  */
NOINL long xx16 (const void *x)
{ long r; __asm__ volatile ("" : "=r" (r) : "0" (1), "r" (x) : "memory"); return r; }
/* Non-volatile, the result used.  */
NOINL long c1nv (void)
{ long r; __asm__ ("" : "=r" (r) : "0" (1)); return r; }

static int minus3 = -3;

/* Upper half of the pattern: 0x5a5a5a5a; the low half varies.  */
#define PAT(k) (0x5a5a5a5a00000000L | (0x10000L * (k)) | 0xbeef)

#define RUN(label, call, low)                                           \
  do {                                                                  \
    unsigned long lo = (unsigned int) (low);                            \
    long sx = (int) (low);                                              \
    int z = 0, kept = 0, s = 0, other = 0;                              \
    long first = 0;                                                     \
    for (int k = 1; k <= 1000; k++)                                     \
      {                                                                 \
        fill (PAT (k));                                                 \
        long r = (call);                                                \
        if ((unsigned long) r == lo)                                    \
          z++;                                                          \
        else if (r == sx)                                               \
          s++;                                                          \
        else if ((unsigned long) r                                      \
                 == ((PAT (k) & 0xffffffff00000000L) | lo))             \
          kept++;                                                       \
        else if (!other++)                                              \
          first = r;                                                    \
      }                                                                 \
    printf ("%-16s zero-extended %4d  sign-extended %4d  kept %4d"      \
            "  other %4d", label, z, s, kept, other);                   \
    if (other)                                                          \
      printf (" (first %#lx)", first);                                  \
    printf ("\n");                                                      \
  } while (0)

#define WW(label, fn, want)                                             \
  do {                                                                  \
    int ok = 0, bad = 0;                                                \
    long first = 0;                                                     \
    for (int k = 1; k <= 1000; k++)                                     \
      {                                                                 \
        long x = PAT (k), r = fn (x);                                   \
        if (r == (want))                                                \
          ok++;                                                         \
        else if (!bad++)                                                \
          first = r;                                                    \
      }                                                                 \
    printf ("%-16s want %4d  other %4d", label, ok, bad);               \
    if (bad)                                                            \
      printf (" (first %#lx)", first);                                  \
    printf ("  e.g. %#lx\n", fn (PAT (1)));                             \
  } while (0)

int
main (void)
{
  RUN ("c1", c1 (), 1);
  RUN ("cm1", cm1 (), -1);
  RUN ("iarg (-1)", iarg (-1), -1);
  RUN ("iarg (5)", iarg (5), 5);
  RUN ("uadd (~0u-1)", uadd (~0u - 1), ~0u);
  RUN ("iadd (1)", iadd (1), -1);
  RUN ("ldi (-3)", ldi (&minus3), -3);
  RUN ("sh (0x7fff)", sh (0x7fff), 0x8000);
  RUN ("sh (-2)", sh (-2), -1);
  RUN ("ch (-2)", ch (-2), -1);
  RUN ("xx16", xx16 (0), 1);
  RUN ("c1nv", c1nv (), 1);
  printf ("sh (0x7fff) = %#lx, sh (-2) = %#lx, ch (-2) = %#lx\n",
          sh (0x7fff), sh (-2), ch (-2));
  WW ("ww_d0_s1", ww_d0_s1, (x & 0xffffffff00000000L) | 1);
  WW ("ww_d1_s3", ww_d1_s3, (x & 0xffffffff00000000L) | 1);
  WW ("ww_s0_d1", ww_s0_d1, x);
  WW ("ww_d0_s1_dbl1", ww_d0_s1_dbl1, 1);
  return 0;
}
