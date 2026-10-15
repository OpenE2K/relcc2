/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What a call under a false predicate costs (spec/isa/predicates.md,
   control-transfer.md, timing.md).  The kernel says such a call still
   makes the hardware spill the chain stack (OpenE2K/linux
   arch/e2k/kernel/ttable.c:896-898); lcc predicates most calls at -O3.
   callsstack_pcall.s has the kernels:

   loop    cycles per iteration of an 8-instruction loop whose first
           instruction holds nothing (none), a false call (call), a
           false ct (ct), with the compare that writes the false
           predicate D = 1..4 instructions earlier or before the loop
           (D 0); and a true call to an empty leaf (tcall, against
           none2, its baseline).  At call depth 1 and below a recursion
           64 deep (the chain register file is full: 30 frames on the
           4C and 8C2, 29 on the 2C3);
   first   the cycles of the first iteration alone (n = 1) at both
           depths: a spill would show here;
   shtp    %pcshtp (chain frames in the register file, 32 bytes each)
           and %pcsp.hi (ind: frames in memory) before and after each of
           three false calls, at depths 1 to 64;
   rec     cycles per level of a recursion n deep (n = 8, 30, 100,
           1000) whose levels do nothing else (none), a false call
           before the recursive call (pre), after it (post) or both
           (both is 1 cycle a level slower than pre even at n = 8
           because gas inserted an empty wide instruction before its
           recursive call, keeping the return point out of offsets
           0x1c0-0x1ff: its CODE_PLACEMENT workaround, AGENTS.md
           "Gotchas"; not a cost of the calls);
   pmu     BUB_B_CHAIN_SPILLFILL (0x266), BUB_B_CHAIN_RET_NRDY (0x21c)
           and BUB_B_RF_SPILLFILL (0x267) during rec at n = 1000 and
           during the loops at depth 64 (per iteration), if perf allows.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc  # 4C; v6.2c3 2C3
     $L -O2 -fno-inline tools/isa-probes/callsstack_pcall.c \
        tools/isa-probes/callsstack_pcall.s -o callsstack_pcall
     ./callsstack_pcall [shtp]    # on hardware; pin it: taskset -c 1
                                  # shtp: only the register reads

   Best of 9 runs everywhere; loop figures are (t (2n) - t (n)) / n with
   n = 10,000.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pmu.h"

typedef long (*kfn) (long);

long pc_clkr (long);
void pc_shtp (long *);
#define LOOPS(X) \
  X (none, 0) X (call, 0) X (ct, 0) \
  X (none, 1) X (call, 1) X (ct, 1) \
  X (none, 2) X (call, 2) X (ct, 2) \
  X (none, 3) X (call, 3) X (ct, 3) \
  X (none, 4) X (call, 4) X (ct, 4) \
  X (tcall, 0) X (none2, 0)
#define DECL(k, d) long pl_##k##_d##d (long);
LOOPS (DECL)
long pr_none (long), pr_pre (long), pr_post (long), pr_both (long);

static const struct
{
  const char *name;
  kfn f;
} loops[] = {
#define ENT(k, d) { #k " d" #d, pl_##k##_d##d },
  LOOPS (ENT)
};

/* Run F (N) DEPTH calls deep (real calls: lcc keeps the + 0 from making
   it a tail call).  */
static volatile long sink;

static long __attribute__ ((noinline))
at_depth (int depth, kfn f, long n)
{
  if (depth <= 0)
    return f (n);
  long r = at_depth (depth - 1, f, n);
  sink = r;
  return r + 0 * sink;
}

static long
best (kfn f, long n, int depth)
{
  long b = -1;
  for (int i = 0; i < 9; i++)
    {
      long c = at_depth (depth, f, n);
      if (b < 0 || c < b)
        b = c;
    }
  return b;
}

static long n_rec;
static kfn rec_f;

static long __attribute__ ((noinline))
time_rec (long n)
{
  long t0 = pc_clkr (0);
  rec_f (n);
  long t1 = pc_clkr (0);
  return t1 - t0;
}

static long
pmu_rec (long n, long *p)
{
  (void) p;
  return rec_f (n);
}

static kfn pmu_loop_f;
static int pmu_depth;

static long
pmu_loop (long n, long *p)
{
  (void) p;
  return at_depth (pmu_depth, pmu_loop_f, n);
}

static void
shtp_at (int depth)
{
  long out[8];
  if (depth > 0)
    {
      shtp_at (depth - 1);
      sink = depth;
      return;
    }
  pc_shtp (out);
  printf ("    entry pcshtp %4ld pcsp.ind %5lu", out[0],
          (unsigned long) out[1] & 0xffffffff);
  for (int i = 1; i < 4; i++)
    printf (" | false call %d: pcshtp %4ld pcsp.ind %5lu", i, out[2 * i],
            (unsigned long) out[2 * i + 1] & 0xffffffff);
  printf ("\n");
}

static void
shtp_depth (int depth)
{
  printf ("  depth %3d:", depth);
  fflush (stdout);
  shtp_at (depth);
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  const long n = 10000;
  int nl = sizeof loops / sizeof loops[0];
  int only_shtp = argc > 1 && strcmp (argv[1], "shtp") == 0;

  if (only_shtp)
    goto shtp;
  printf ("loop: cycles per iteration (depth 1 / depth 64)\n");
  for (int i = 0; i < nl; i++)
    {
      double c1 = (best (loops[i].f, 2 * n, 0) - best (loops[i].f, n, 0))
                  / (double) n;
      double c64 = (best (loops[i].f, 2 * n, 64) - best (loops[i].f, n, 64))
                   / (double) n;
      printf ("  %-10s %6.2f %6.2f\n", loops[i].name, c1, c64);
    }

  printf ("first: cycles of one iteration (depth 1 / depth 64)\n");
  for (int i = 0; i < nl; i++)
    printf ("  %-10s %6ld %6ld\n", loops[i].name, best (loops[i].f, 1, 0),
            best (loops[i].f, 1, 64));

shtp:
  printf ("shtp: %%pcshtp and %%pcsp.hi ind (bytes) around three false "
          "calls\n");
  static const int depths[] = { 0, 8, 16, 24, 28, 30, 31, 32, 33, 40, 64 };
  for (unsigned i = 0; i < sizeof depths / sizeof depths[0]; i++)
    shtp_depth (depths[i]);

  if (only_shtp)
    return 0;
  printf ("rec: cycles per level (recursion n deep, total / n)\n");
  static const struct
  {
    const char *name;
    kfn f;
  } recs[] = { { "none", pr_none }, { "pre", pr_pre }, { "post", pr_post },
               { "both", pr_both } };
  static const long rn[] = { 8, 30, 100, 1000 };
  printf ("  %-6s", "n");
  for (unsigned j = 0; j < sizeof rn / sizeof rn[0]; j++)
    printf (" %8ld", rn[j]);
  printf ("\n");
  for (unsigned i = 0; i < sizeof recs / sizeof recs[0]; i++)
    {
      printf ("  %-6s", recs[i].name);
      rec_f = recs[i].f;
      for (unsigned j = 0; j < sizeof rn / sizeof rn[0]; j++)
        {
          n_rec = rn[j];
          long b = best (time_rec, rn[j], 0);
          printf (" %8.2f", b / (double) rn[j]);
        }
      printf ("\n");
    }

  static const struct
  {
    const char *name;
    unsigned long config;
  } evs[] = { { "BUB_B_CHAIN_SPILLFILL", 0x266 },
              { "BUB_B_CHAIN_RET_NRDY", 0x21c },
              { "BUB_B_RF_SPILLFILL", 0x267 },
              { "TICKS", 0x272 } };
  printf ("pmu: counts per level of rec n = 1000, and per iteration of the "
          "depth-64 loops (n = 10,000)\n");
  for (unsigned e = 0; e < sizeof evs / sizeof evs[0]; e++)
    {
      int fd = pmu_open (evs[e].config);
      if (fd < 0)
        {
          printf ("  %-22s cannot open\n", evs[e].name);
          continue;
        }
      printf ("  %-22s", evs[e].name);
      for (unsigned i = 0; i < sizeof recs / sizeof recs[0]; i++)
        {
          rec_f = recs[i].f;
          unsigned long b = ~0UL;
          for (int k = 0; k < 5; k++)
            {
              unsigned long c = pmu_count (fd, pmu_rec, 1000, NULL);
              if (c < b)
                b = c;
            }
          printf (" %s %.2f", recs[i].name, b / 1000.0);
        }
      static const char *const ln[] = { "none d0", "call d0", "ct d0" };
      for (int i = 0; i < 3; i++)
        {
          pmu_depth = 64;
          pmu_loop_f = loops[i].f;
          unsigned long b1 = ~0UL, b2 = ~0UL;
          for (int k = 0; k < 5; k++)
            {
              unsigned long c = pmu_count (fd, pmu_loop, n, NULL);
              if (c < b1)
                b1 = c;
              c = pmu_count (fd, pmu_loop, 2 * n, NULL);
              if (c < b2)
                b2 = c;
            }
          printf (" | %s %.3f (%lu at n)", ln[i],
                  ((double) b2 - (double) b1) / n, b1);
        }
      printf ("\n");
      close (fd);
    }
  return 0;
}
