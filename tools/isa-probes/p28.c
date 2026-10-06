/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A software-pipelined loop that uses DAM (a lock load in stage A, its
   check and rbranch in stage B) together with rotating predicates
   (setbp, abp), as the proof-of-concept back end's finding P28 asks
   (docs/spec-poc-findings.md; spec/isa/speculation.md, loops.md).
   p28.s is hand-written.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $LCC -O3 tools/isa-probes/p28.c tools/isa-probes/p28.s -o p28
     ./p28                                # on hardware

   Iteration j locks a[j] into %b[0] and puts its parity into %pred0;
   one kernel iteration later (as %b[2] and %pred1) it is checked and
   out[j] = parity even ? a[j] : -a[j].  The "full" and "norecompute"
   loops store a[j] ^ 1 right after locking a[j], so every check but the
   first reloads and its rbranch runs the compensation code: "full"
   recomputes the parity there, "norecompute" doesn't (so it must come
   out wrong); "nostore" stores nothing (no reload).  "deep" adds a
   stage: the parity is used one stage after the check, as %pred2 with
   the value in %b[4] (setbp psz = 2), so the compensation fixes a name
   the consumer reads only after the next rotation; "deep_norecompute"
   is its control.  */

#include <stdio.h>
#include <string.h>

long p28_full (long *, long *, unsigned long);
long p28_norecompute (long *, long *, unsigned long);
long p28_nostore (long *, long *, unsigned long);
long p28_deep (long *, long *, unsigned long);
long p28_deep_norecompute (long *, long *, unsigned long);

#define N 1000

int
main (void)
{
  static long a[N + 1], a0[N + 1], out[N];
  unsigned long x = 12345;
  for (int i = 0; i <= N; i++)
    {
      x = x * 6364136223846793005UL + 1442695040888963407UL;
      a0[i] = (long) (x >> 20);
    }
  static const struct { const char *name; long (*f) (long *, long *,
                                                     unsigned long);
                        int store; } v[] = {
    { "full", p28_full, 1 }, { "norecompute", p28_norecompute, 1 },
    { "nostore", p28_nostore, 0 }, { "deep", p28_deep, 2 },
    { "deep_norecompute", p28_deep_norecompute, 2 } };
  for (unsigned k = 0; k < sizeof v / sizeof *v; k++)
    for (int n = v[k].store == 2 ? 2 : 1; n <= N;
         n = n < 8 ? n + 1 : n * 3)
      {
        memcpy (a, a0, sizeof a);
        memset (out, 0x55, sizeof out);
        long comp = v[k].f (a, out, 1UL << 37 | (unsigned long) n);
        int bad = 0;
        for (int j = 0; j < n; j++)
          {
            long val = j > 0 && v[k].store ? a0[j] ^ 1 : a0[j];
            long want = (val & 1) == 0 ? val : -val;
            bad += out[j] != want;
          }
        printf ("%-16s n %4d: %4d wrong, compensation ran %ld times\n",
                v[k].name, n, bad, comp);
      }
  return 0;
}
