/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Store-to-load round trips (spec/isa/timing.md, "Memory"): a store
   followed by a load of the same, a narrower, a wider or a nearby
   address, the next store depending on the load; and a store followed
   after N cycles by an independent load.  stld.s, stld-cases.h.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $LCC -O3 tools/isa-probes/stld.c tools/isa-probes/stld.s -o stld
     ./stld                               # on hardware

   Each line: the case and the best of 50 runs, in cycles per step
   (32 steps).  */

#include <stdio.h>

#define CASE(n) long stld_##n (long, void *);
#include "stld-cases.h"
#undef CASE

static long buf[1024] __attribute__ ((aligned (4096)));

int
main (void)
{
#define CASE(n) \
  { \
    long b = 1L << 60; \
    for (int r = 0; r < 50; r++) \
      { \
        long c = stld_##n (0, buf); \
        b = c < b ? c : b; \
      } \
    printf ("%-14s %6.2f\n", #n, b / 32.0); \
  }
#include "stld-cases.h"
#undef CASE
  return 0;
}
