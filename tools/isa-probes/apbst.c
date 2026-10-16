/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Which stores into a streamed array a later mova sees: isaleft.c's
   ovst loop (std, 12 cycles an iteration) against the same loop
   storing with an AAU staad, as lcc's APB loops do, and both in a
   shorter loop, by %lsr.ldovl and the store's distance d in iterations
   (spec/isa/loops.md, "Stores into a streamed array"; apb.md).
   gen_apbst.py writes apbst.s and apbst-cases.h, and apbst-v5.s and
   apbst-v5-cases.h: the v5+ cases (built in with -DAPBST_V5) that
   write %lsr1 (l1), address the area through %aaind1 (ix), store in
   the alc instruction (lm) or keep the movad's results in rotating
   registers (rot2, rot5); gen_apbst.py's docstring describes each.

     D=tools/isa-probes
     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   # runs on all three
     $L -O1 $D/apbst.c $D/apbst.s -o apbst
     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc  # 8C2 (v6.2c3: 2C3)
     $L -O1 -DAPBST_V5 $D/apbst.c $D/apbst.s $D/apbst-v5.s -o apbst
     flock -w 3600 /tmp/relcc2-claude-timing.lock ./apbst [FILTER [D [STRMD]]]
                          # on hardware; FILTER is a substring of names

   Per case and ldovl, two runs: for each d, how many of the iterations
   i >= d read the stored value (the rest the old one; "(bad)" if
   neither), then the cycles of the last d's 64-iteration call.  With D, only
   that distance, and which iterations i >= D saw the store ('s') or
   read the old value ('.'); STRMD goes into %lsr.strmd (default 0;
   an _lm case drops the last iteration's store unless it is 1).  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CASE(n) long apbst_##n (unsigned long *, unsigned long,         \
                                unsigned long *, long);
#include "apbst-cases.h"
#ifdef APBST_V5
#include "apbst-v5-cases.h"
#endif
#undef CASE
unsigned long apbst_clkr (void);

static const struct
{
  const char *name;
  long (*f) (unsigned long *, unsigned long, unsigned long *, long);
} cases[] = {
#define CASE(n) { #n, apbst_##n },
#include "apbst-cases.h"
#ifdef APBST_V5
#include "apbst-v5-cases.h"
#endif
#undef CASE
};

#define N 4096
static unsigned long arr[N] __attribute__ ((aligned (4096)));
static unsigned long out[2 * N];

int
main (int argc, char **argv)
{
  static const int ovl[] = { 0, 1, 2, 4, 8, 255 };
  static const int ds[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48 };
  const long n = 64;
  const unsigned long seen = 0x5555555555555555UL;
  const int only = argc > 2 ? atoi (argv[2]) : 0;
  const unsigned long strmd = argc > 3 ? atol (argv[3]) : 0;
  printf ("loop of %ld iterations; per store distance d, how many of the\n"
          "iterations i >= d read the stored value (the rest the old one)\n",
          n);
  for (unsigned c = 0; c < sizeof cases / sizeof *cases; c++)
    {
      if (argc > 1 && !strstr (cases[c].name, argv[1]))
        continue;
      for (unsigned k = 0; k < sizeof ovl / sizeof *ovl; k++)
        for (int rep = 0; rep < 2; rep++)
          {
            unsigned long cyc = 0;
            printf ("%-14s ldovl %3d:", cases[c].name, ovl[k]);
            for (unsigned j = 0; j < sizeof ds / sizeof *ds; j++)
              {
                int d = ds[j];
                if (only && d != only)
                  continue;
                for (long i = 0; i < N; i++)
                  arr[i] = 0x8182838485000000UL + 8 * i;
                memset (out, 0, sizeof out);
                /* rot cases record three iterations late, from out - 6 */
                int rot = strstr (cases[c].name, "_rot") != NULL;
                unsigned long *o = rot ? out + 8 : out;
                long last = rot ? n - 4 : n;
                unsigned long lsr = n | 1UL << 37
                                    | (unsigned long) ovl[k] << 40
                                    | strmd << 53;
                unsigned long t0 = apbst_clkr ();
                long it = cases[c].f (arr, lsr, o, d);
                unsigned long t1 = apbst_clkr ();
                cyc = t1 - t0;
                int nw = 0, bad = 0;
                for (long t = d; t < last; t++)
                  if (o[2 * t] == seen)
                    nw++;
                  else if (o[2 * t] != 0x8182838485000000UL + 8 * t
                           || o[2 * t + 1])
                    bad++;
                printf (" d%d %d/%ld%s", d, nw, last - d,
                        it != n ? "(it!)" : bad ? "(bad)" : "");
                if (only)
                  {
                    printf (" ");
                    for (long t = d; t < last; t++)
                      putchar (o[2 * t] == seen ? 's' : '.');
                  }
              }
            printf ("  [%lu cycles]\n", cyc);
          }
    }
  return 0;
}
