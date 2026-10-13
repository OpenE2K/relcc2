/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The timing questions spec/isa/timing.md and the other spec/isa pages
   left open, for v3 to v6: store data, narrow 80-bit operands, loads
   into the divider, fxcmp*b into predicates, combined operations across
   clusters, 32-bit results read as 64 bits, aaurw operands (lat);
   divider pairs, far pairs, triples and runs (div); byte and halfword
   throughput (tp); the store bank rule across pages (bank); instruction
   fetch after a taken transfer (fetch); misaligned AAU accesses (aau);
   store and load address aliasing (alias); and on v6 the speculative
   lock load MAS 0x17, the wait-unlock store MAS 0x33 / 0x73 and icall
   (v6).  timeleft.s, timeleft-v6.s and the -cases.h files are generated
   by gen_timeleft.py.

     python3 tools/isa-probes/gen_timeleft.py
     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc  # the 4C;
                          # v5.8c2 for the 8C2; for the 2C3 v6.2c3 and
                          # -DTL_V6 tools/isa-probes/timeleft-v6.s
     $LCC -O1 tools/isa-probes/timeleft.c tools/isa-probes/timeleft.s \
          -o timeleft
     ./timeleft [lat|div|tp|bank|fetch|aau|alias|v6] [SUBSTRING]
                          # every section without an argument; run
                          # under the shared timing lock

   Lines:
     lat P -> C dbl D: S1 .. Sn | latency L
       Sg is the stall of a consumer C placed g cycles after the producer
       P (cycles a step minus g + 4, the best of 9 runs of 12 independent
       steps after 4); L is the smallest g from which no gap stalls
     dp A -> B: S1 .. S6; dq A -> B g7..: S7 .. S16
       the stall of divider operation B placed g cycles after A (a step
       is A, g - 1 empty cycles, B, 15 empty cycles)
     dt A D1: S(D2 = 1) .. S(D2 = 8)
       the stall of three A, D1 and D2 cycles apart
     dr A xN g G: S
       the stall of a run of N copies of A, G cycles apart (per run)
     tp NAME: C cycles an instruction (a loop of 32 or 64 instructions)
     bank KIND D: C cycles an instruction (two accesses D bytes apart)
     fe NAME: C cycles a block (the slope between 16 and 32 blocks)
     aau value / tp / lat: AAU loads from a base +0, +4, +1
     alias KIND D: a store and a load D bytes apart: in one instruction
       (cycles an instruction), and a load G cycles after the store whose
       value feeds the next store (stld: cycles a step)
     v6 ...: cycles a load, a step, a read-modify-write or a call block

   Results of 2026-10-13 (4C, 8C2, 2C3): spec/isa/timing.md,
   predicates.md, control-transfer.md (fetch, icall) and speculation.md
   (v6); timeleftv.c checked some of them.  */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

typedef long (*tfn) (long *, long, long);
typedef long (*pfn) (long *, long *, long);

#define LAT(f, l, g) long tl_##f (long *, long, long);
#define DP(f, l, g) long tl_##f (long *, long, long);
#define DR(f, l, g, n) long tl_##f (long *, long, long);
#define DQ(f, l, g) long tl_##f (long *, long, long);
#define DT(f, l, a, b) long tl_##f (long *, long, long);
#define AL(f, l, g) long tl_##f (long *, long, long);
#define AV(f, l) long tl_##f (long *, long, long);
#define SL(f, l) long tl_##f (long *, long, long);
#define TP(f, l) long tl_##f##_k32 (long *, long *, long); \
                 long tl_##f##_k64 (long *, long *, long);
#define FE(f, l) long tl_##f##_k16 (long *, long, long); \
                 long tl_##f##_k32 (long *, long, long);
#include "timeleft-cases.h"
#undef LAT
#undef DP
#undef DR
#undef DQ
#undef DT
#undef AL
#undef AV
#undef SL
#undef TP
#undef FE
#define SL(f, l)
long tl_sl (long *, long, long);
long tl_sl_gap (long *, long, long);
long tl_sl_gap2 (long *, long, long);
long tl_sl_gap3 (long *, long, long);
long tl_sl_gap5 (long *, long, long);

struct tcase
{
  const char *label;
  int g, n;
  tfn f;
};
struct pcase
{
  const char *label;
  pfn f32, f64;
};
struct fcase
{
  const char *label;
  tfn f16, f32;
};

#define NONE_LAT(f, l, g)
#define NONE_DP(f, l, g)
#define NONE_DR(f, l, g, n)
#define DQ(f, l, g)
#define DT(f, l, a, b)
#define AL(f, l, g)
#define AV(f, l)
#define NONE_TP(f, l)
#define NONE_FE(f, l)

static const struct tcase lat[] = {
#define LAT(f, l, g) { l, g, 12, tl_##f },
#define DP NONE_DP
#define DR NONE_DR
#define TP NONE_TP
#define FE NONE_FE
#include "timeleft-cases.h"
#undef LAT
#undef DP
#undef DR
#undef TP
#undef FE
};
static const struct tcase dp[] = {
#define LAT NONE_LAT
#define DP(f, l, g) { l, g, 12, tl_##f },
#define DR NONE_DR
#define TP NONE_TP
#define FE NONE_FE
#include "timeleft-cases.h"
#undef LAT
#undef DP
#undef DR
#undef TP
#undef FE
};
static const struct tcase dr[] = {
#define LAT NONE_LAT
#define DP NONE_DP
#define DR(f, l, g, n) { l, g, n, tl_##f },
#define TP NONE_TP
#define FE NONE_FE
#include "timeleft-cases.h"
#undef LAT
#undef DP
#undef DR
#undef TP
#undef FE
};
static const struct pcase tp[] = {
#define LAT NONE_LAT
#define DP NONE_DP
#define DR NONE_DR
#define TP(f, l) { l, tl_##f##_k32, tl_##f##_k64 },
#define FE NONE_FE
#include "timeleft-cases.h"
#undef LAT
#undef DP
#undef DR
#undef TP
#undef FE
};
static const struct fcase fe[] = {
#define LAT NONE_LAT
#define DP NONE_DP
#define DR NONE_DR
#define TP NONE_TP
#define FE(f, l) { l, tl_##f##_k16, tl_##f##_k32 },
#include "timeleft-cases.h"
#undef LAT
#undef DP
#undef DR
#undef TP
#undef FE
};

#undef DQ
#undef DT
static const struct tcase dq[] = {
#define LAT NONE_LAT
#define DP NONE_DP
#define DR NONE_DR
#define DQ(f, l, g) { l, g, 12, tl_##f },
#define DT(f, l, a, b)
#define TP NONE_TP
#define FE NONE_FE
#include "timeleft-cases.h"
#undef LAT
#undef DP
#undef DR
#undef DQ
#undef DT
#undef TP
#undef FE
};
static const struct tcase dt[] = {
#define LAT NONE_LAT
#define DP NONE_DP
#define DR NONE_DR
#define DQ(f, l, g)
#define DT(f, l, a, b) { l, a, b, tl_##f },
#define TP NONE_TP
#define FE NONE_FE
#include "timeleft-cases.h"
#undef LAT
#undef DP
#undef DR
#undef DQ
#undef DT
#undef TP
#undef FE
};
#undef AL
#undef AV
#define DQ(f, l, g)
#define DT(f, l, a, b)
static const struct tcase al[] = {
#define LAT NONE_LAT
#define DP NONE_DP
#define DR NONE_DR
#define AL(f, l, g) { l, g, 12, tl_##f },
#define AV(f, l)
#define TP NONE_TP
#define FE NONE_FE
#include "timeleft-cases.h"
#undef LAT
#undef DP
#undef DR
#undef AL
#undef AV
#undef TP
#undef FE
};
static const struct tcase av[] = {
#define LAT NONE_LAT
#define DP NONE_DP
#define DR NONE_DR
#define AL(f, l, g)
#define AV(f, l) { l, 0, 0, tl_##f },
#define TP NONE_TP
#define FE NONE_FE
#include "timeleft-cases.h"
#undef LAT
#undef DP
#undef DR
#undef AL
#undef AV
#undef TP
#undef FE
};
#undef DQ
#undef DT

#define N(a) (sizeof (a) / sizeof (a)[0])

static long mem[1024] __attribute__ ((aligned (4096)));
static long tpbuf[8192] __attribute__ ((aligned (4096)));

static long
best (tfn f, long *m, long a, long b, int reps)
{
  long r = 1L << 60;
  for (int i = 0; i < reps; i++)
    {
      long t = f (m, a, b);
      r = t < r ? t : r;
    }
  return r;
}

static long
pbest (pfn f, long *p, long *q, long iters, int reps)
{
  long r = 1L << 60;
  for (int i = 0; i < reps; i++)
    {
      long t = f (p, q, iters);
      r = t < r ? t : r;
    }
  return r;
}

/* Cycles a step of a straight case of N steps (the first %clkr read's
   instruction counts 1).  */
static double
slope (tfn f, int n)
{
  return (double) (best (f, mem, 0, 0, 9) - 1) / n;
}

/* Cycles an instruction of a loop case: the slope between 32 and 64
   instructions a loop iteration.  */
static double
pslope (const struct pcase *c, long *p, long *q, int reps)
{
  enum { ITERS = 256 };
  long a = pbest (c->f32, p, q, ITERS, reps);
  long b = pbest (c->f64, p, q, ITERS, reps);
  return (double) (b - a) / (ITERS * 32.0);
}

static int
want (int argc, char **argv, const char *s)
{
  return argc < 2 || strcmp (argv[1], s) == 0;
}

static const char *filter;

static int
pass (const char *label)
{
  return !filter || strstr (label, filter);
}

static void
init_mem (void)
{
  union { double d; long l; } one = { 1.0 };
  memset (mem, 0, sizeof mem);
  mem[0] = one.l;
  mem[2] = 0x3f800000;
}

static void
run_lat (void)
{
  for (size_t i = 0; i < N (lat); )
    {
      const char *l = lat[i].label;
      int L = 0, show = pass (l);
      if (show)
        printf ("lat %-36s", l);
      for (; i < N (lat) && strcmp (lat[i].label, l) == 0; i++)
        {
          if (!show)
            continue;
          double s = slope (lat[i].f, lat[i].n) - (lat[i].g + 4);
          printf (" %5.2f", s);
          if (s > 0.5)
            L = 0;
          else if (!L)
            L = lat[i].g;
        }
      if (show)
        printf (" | latency %d\n", L);
    }
}

static void
run_div (void)
{
  for (size_t i = 0; i < N (dp); )
    {
      const char *l = dp[i].label;
      int show = pass (l);
      if (show)
        printf ("dp %-22s", l);
      for (; i < N (dp) && strcmp (dp[i].label, l) == 0; i++)
        if (show)
          printf (" %5.2f", slope (dp[i].f, dp[i].n) - (dp[i].g + 16));
      if (show)
        printf ("\n");
    }
  for (size_t i = 0; i < N (dq); )
    {
      const char *l = dq[i].label;
      int show = pass (l);
      if (show)
        printf ("dq %-22s g7..", l);
      for (; i < N (dq) && strcmp (dq[i].label, l) == 0; i++)
        if (show)
          printf (" %5.2f", slope (dq[i].f, dq[i].n) - (dq[i].g + 16));
      if (show)
        printf ("\n");
    }
  /* Triples: rows D1 = 1..8, columns D2 = 1..8; the stall of the
     triple (cycles a step minus D1 + D2 + 1 + 24).  */
  for (size_t i = 0; i < N (dt); )
    {
      const char *l = dt[i].label;
      int show = pass (l);
      if (show)
        printf ("dt %s (rows: first gap 1..8; columns: second gap 1..8)\n", l);
      for (; i < N (dt) && strcmp (dt[i].label, l) == 0; i++)
        if (show)
          {
            double s = slope (dt[i].f, 8) - (dt[i].g + dt[i].n + 1 + 24);
            if (dt[i].n == 1)
              printf ("dt %-10s %d:", l, dt[i].g);
            printf (" %5.2f", s);
            if (dt[i].n == 8)
              printf ("\n");
          }
    }
  for (size_t i = 0; i < N (dr); i++)
    if (pass (dr[i].label))
      {
        int n = dr[i].n, g = dr[i].g;
        /* The run without a stall: n instructions g apart, then 20 empty
           cycles.  */
        double s = slope (dr[i].f, 8) - ((n - 1) * g + 1 + 20);
        printf ("dr %-14s g %d: %5.2f a run\n", dr[i].label, g, s);
      }
}

static void
run_tp (void)
{
  for (size_t i = 0; i < N (tp); i++)
    if (strstr (tp[i].label, "_pq") == NULL
        && strncmp (tp[i].label, "aa_", 3) != 0 && pass (tp[i].label))
      printf ("tp %-14s %.2f cycles an instruction\n", tp[i].label,
              pslope (&tp[i], tpbuf, tpbuf, 5));
}

static const struct pcase *
find_tp (const char *label)
{
  for (size_t i = 0; i < N (tp); i++)
    if (strcmp (tp[i].label, label) == 0)
      return &tp[i];
  abort ();
}

/* AnonHugePages of the mapping that holds P, from /proc/self/smaps.  */
static long
huge_kb (void *p)
{
  FILE *f = fopen ("/proc/self/smaps", "r");
  char line[512];
  int in = 0;
  long r = -1;
  if (!f)
    return -1;
  while (fgets (line, sizeof line, f))
    {
      unsigned long a, b;
      char *dash = line + strspn (line, "0123456789abcdef");
      if (dash != line && *dash == '-'
          && sscanf (line, "%lx-%lx", &a, &b) == 2)
        in = (uintptr_t) p >= a && (uintptr_t) p < b;
      else if (in && strncmp (line, "AnonHugePages:", 14) == 0)
        {
          r = atol (line + 14);
          in = 0;
        }
    }
  fclose (f);
  return r;
}

static uint64_t rng = 0x9e3779b97f4a7c15ULL;

static uint64_t
next (void)
{
  rng ^= rng << 13, rng ^= rng >> 7, rng ^= rng << 17;
  return rng;
}

static void
run_bank (void)
{
  const struct pcase *sd = find_tp ("std_pq"), *sb = find_tp ("stb_pq");
  const struct pcase *sl = find_tp ("stdld_pq");
  size_t big = 64UL << 20;
  char *r4 = mmap (NULL, big, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  char *rh = mmap (NULL, 8UL << 20, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (r4 == MAP_FAILED || rh == MAP_FAILED)
    {
      perror ("mmap");
      return;
    }
#ifdef MADV_NOHUGEPAGE
  madvise (r4, big, MADV_NOHUGEPAGE);
#endif
  char *h = (char *) (((uintptr_t) rh + (2UL << 20) - 1) & ~((2UL << 20) - 1));
#ifdef MADV_HUGEPAGE
  madvise (h, 4UL << 20, MADV_HUGEPAGE);
#endif
  memset (r4, 1, big);
  memset (h, 1, 4UL << 20);
  printf ("bank: 4 KiB region AnonHugePages %ld kB, huge region %ld kB\n",
          huge_kb (r4), huge_kb (h));
  static const long dist[] = { 0, 8, 16, 32, 48, 64, 96, 128, 192, 256,
                               320, 512, 1024, 2048, 2048 + 64, 4096,
                               4096 + 8, 4096 + 64, 4096 + 128, 8192,
                               8192 + 64, 12288, 16384, 16384 + 64, 65536,
                               65536 + 64, 1L << 20, (1L << 20) + 64 };
  for (int reg = 0; reg < 2; reg++)
    for (size_t i = 0; i < N (dist); i++)
      {
        char *base = reg ? h : r4 + (8UL << 20);
        long *p = (long *) (base + 0x100), *q = (long *) (base + 0x100 + dist[i]);
        if (reg && dist[i] >= (1L << 20))
          continue;
        printf ("bank %s D %7ld: std %.2f  stb %.2f  std+ldd %.2f\n",
                reg ? "huge" : "4k  ", dist[i], pslope (sd, p, q, 5),
                pslope (sb, p, q, 5), pslope (sl, p, q, 5));
      }
  /* Random pairs of 4 KiB pages: the same page offset (bits 6-7 equal)
     or 64 bytes more.  */
  for (int delta = 0; delta <= 64; delta += 64)
    {
      int lo = 0, hi = 0;
      double mn = 9, mx = 0;
      for (int t = 0; t < 96; t++)
        {
          size_t pa = next () % (big / 4096), pb = next () % (big / 4096);
          long off = (next () % 48) * 64;
          if (pa == pb)
            continue;
          long *p = (long *) (r4 + pa * 4096 + off);
          long *q = (long *) (r4 + pb * 4096 + off + delta);
          double c = pslope (sd, p, q, 3);
          mn = c < mn ? c : mn;
          mx = c > mx ? c : mx;
          if (c > 1.3)
            hi++;
          else
            lo++;
        }
      printf ("bank random 4 KiB page pairs, offset +%d: %d at about 1, "
              "%d above 1.3 (min %.2f, max %.2f)\n", delta, lo, hi, mn, mx);
    }
  munmap (r4, big);
  munmap (rh, 8UL << 20);
}

/* AAU accesses with an aligned and a misaligned base: values, then
   throughput, then latency (a misaligned AAU load may trap: the values
   come first).  */
static void
run_aau (void)
{
  static const int offs[] = { 0, 4, 1 };
  unsigned char *b = (unsigned char *) tpbuf;
  for (int i = 0; i < 4096; i++)
    b[i] = (unsigned char) (i * 7 + 1);
  for (size_t o = 0; o < N (offs); o++)
    for (size_t i = 0; i < N (av); i++)
      {
        unsigned long want = 0;
        int sz = av[i].label[4] == 'd' ? 8 : av[i].label[4] == 'w' ? 4 : 2;
        memcpy (&want, b + 0x100 + offs[o], sz);
        long got = av[i].f (tpbuf, (long) (b + 0x100 + offs[o]), 0);
        printf ("aau value %s base +%d: %016lx (memory %016lx) %s\n",
                av[i].label, offs[o], (unsigned long) got, want,
                (unsigned long) got == want ? "same" : "DIFFERENT");
      }
  for (size_t o = 0; o < N (offs); o++)
    {
      long *q = (long *) (b + 0x200 + offs[o]);
      static const char *const names[] = { "aa_staad_1", "aa_staad_2",
                                           "aa_std_2", "aa_ldaad_1",
                                           "aa_ldaad_2", "aa_ldd_2" };
      for (size_t i = 0; i < N (names); i++)
        printf ("aau tp %-11s base +%d: %.2f cycles an instruction\n",
                names[i], offs[o], pslope (find_tp (names[i]), tpbuf, q, 5));
    }
  memset (b + 0x200, 0, 64);
  for (size_t o = 0; o < N (offs); o++)
    for (size_t i = 0; i < N (al); )
      {
        const char *l = al[i].label;
        int L = 0;
        printf ("aau lat %-6s base +%d:", l, offs[o]);
        for (; i < N (al) && strcmp (al[i].label, l) == 0; i++)
          {
            long t = best (al[i].f, mem, (long) (b + 0x200 + offs[o]), 0, 9);
            double s = (double) (t - 1) / 12 - (al[i].g + 4);
            printf (" %.0f", s);
            if (s > 0.5)
              L = 0;
            else if (!L)
              L = al[i].g;
          }
        printf (" | latency %d\n", L);
      }
}

/* Address aliasing of a store and a load: in one instruction (the
   load in ALC0 or ALC5), and a load in the instruction after the store
   whose value feeds the next store (stld.c's 7.6-cycle penalty), with
   4 KiB and 2 MiB pages.  */
static void
run_alias (void)
{
  const struct pcase *s0 = find_tp ("stdld_pq"), *s5 = find_tp ("stdld5_pq");
  size_t big = 16UL << 20;
  char *r4 = mmap (NULL, big, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  char *rh = mmap (NULL, 8UL << 20, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (r4 == MAP_FAILED || rh == MAP_FAILED)
    {
      perror ("mmap");
      return;
    }
#ifdef MADV_NOHUGEPAGE
  madvise (r4, big, MADV_NOHUGEPAGE);
#endif
  char *h = (char *) (((uintptr_t) rh + (2UL << 20) - 1) & ~((2UL << 20) - 1));
#ifdef MADV_HUGEPAGE
  madvise (h, 4UL << 20, MADV_HUGEPAGE);
#endif
  memset (r4, 0, big);
  memset (h, 0, 4UL << 20);
  printf ("alias: 4 KiB region AnonHugePages %ld kB, huge region %ld kB\n",
          huge_kb (r4), huge_kb (h));
  static const long dist[] = { 0, 8, 16, 24, 32, 64, 256, 2048, 2048 + 8,
                               4096, 4096 + 8, 4096 + 16, 4096 + 24,
                               4096 + 32, 4096 + 64, 8192, 12288 + 16,
                               65536, 65536 + 8, 1L << 20, (1L << 20) + 8 };
  for (int reg = 0; reg < 2; reg++)
    for (size_t i = 0; i < N (dist); i++)
      {
        char *base = reg ? h : r4 + (4UL << 20);
        long *p = (long *) (base + 0x100), *q = (long *) (base + 0x100 + dist[i]);
        if (reg && dist[i] >= (1L << 20))
          continue;
        printf ("alias %s D %7ld: std+ldd,0 %.2f  std+ldd,5 %.2f  "
                "std; ldd; use %.2f  std; gap; ldd; use %.2f\n",
                reg ? "huge" : "4k  ", dist[i], pslope (s0, p, q, 5),
                pslope (s5, p, q, 5),
                (double) best (tl_sl, mem, (long) p, (long) q, 50) / 32,
                (double) best (tl_sl_gap, mem, (long) p, (long) q, 50) / 32);
      }
  /* Store, G empty cycles, load D bytes away, use: cycles a step.  */
  static const long dist2[] = { 0, 8, 16, 24, 32, 64, 4096, 16384, 32768,
                                49152, 65536 - 8, 65536, 65536 + 8,
                                65536 + 16, 65536 + 32, 65536 + 64,
                                65536 + 256, 65536 + 4096, 131072,
                                196608, 1L << 20, 2L << 20, 3L << 20 };
  static const struct { const char *n; tfn f; } sl[] = {
    { "0", tl_sl }, { "1", tl_sl_gap }, { "2", tl_sl_gap2 },
    { "3", tl_sl_gap3 }, { "5", tl_sl_gap5 } };
  for (int reg = 0; reg < 2; reg++)
    for (size_t i = 0; i < N (dist2); i++)
      {
        char *base = reg ? h : r4 + (4UL << 20);
        long *p = (long *) (base + 0x100), *q = (long *) (base + 0x100 + dist2[i]);
        if (reg && dist2[i] >= (2L << 20))
          continue;
        printf ("stld %s D %7ld:", reg ? "huge" : "4k  ", dist2[i]);
        for (size_t k = 0; k < N (sl); k++)
          printf ("  gap %s %.2f", sl[k].n,
                  (double) best (sl[k].f, mem, (long) p, (long) q, 50) / 32);
        printf ("\n");
      }
  munmap (r4, big);
  munmap (rh, 8UL << 20);
}

static void
run_fetch (void)
{
  for (size_t i = 0; i < N (fe); i++)
    if (pass (fe[i].label))
      {
        long a = best (fe[i].f16, mem, 0, 0, 9);
        long b = best (fe[i].f32, mem, 0, 0, 9);
        printf ("fe %-12s %.2f cycles a block\n", fe[i].label,
                (double) (b - a) / 16);
      }
}

#ifdef TL_V6
#define V6S(f, l) long tl_##f##_n16 (long *, long, long); \
                  long tl_##f##_n32 (long *, long, long);
#define V6C(f, l, n) long tl_##f (long *, long, long);
#define V6A(f, l, n) long tl_##f (long *, long, long);
#define V6K(f, l) long tl_##f##_k16 (long *, long, long); \
                  long tl_##f##_k32 (long *, long, long);
#include "timeleft-v6-cases.h"
#undef V6S
#undef V6C
#undef V6A
#undef V6K
long tl_v6icall_val (long *, long, long);

static long cell[1024] __attribute__ ((aligned (4096)));

static void
run_v6 (void)
{
#define V6S(f, l) \
  { cell[0] = (long) cell; \
    long a = best (tl_##f##_n16, cell, 0, 0, 9); \
    long b = best (tl_##f##_n32, cell, 0, 0, 9); \
    printf ("v6 %-40s %.2f cycles a load\n", l, (double) (b - a) / 16); }
#define V6C(f, l, n) \
  printf ("v6 %-40s %.2f cycles a step\n", l, \
          (double) best (tl_##f, cell, 0, 0, 9) / n);
#define V6A(f, l, n) \
  { memset (cell, 0, sizeof cell); \
    printf ("v6 %-40s %.2f cycles each\n", l, \
            (double) best (tl_##f, cell, 0, 0, 50) / n); }
#define V6K(f, l) \
  { long a = best (tl_##f##_k16, cell, 0, 0, 9); \
    long b = best (tl_##f##_k32, cell, 0, 0, 9); \
    printf ("v6 call %-35s %.2f cycles a block\n", l, (double) (b - a) / 16); }
  printf ("v6 icall value: %ld (1008 = called with a = 7, %%dr2 kept)\n",
          tl_v6icall_val (cell, 7, 0));
#include "timeleft-v6-cases.h"
#undef V6S
#undef V6C
#undef V6A
#undef V6K
}
#endif

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  if (argc > 2)
    filter = argv[2];
  init_mem ();
  if (want (argc, argv, "lat"))
    run_lat ();
  if (want (argc, argv, "div"))
    run_div ();
  if (want (argc, argv, "tp"))
    run_tp ();
  if (want (argc, argv, "bank"))
    run_bank ();
  if (want (argc, argv, "fetch"))
    run_fetch ();
  if (want (argc, argv, "aau"))
    run_aau ();
  if (want (argc, argv, "alias"))
    run_alias ();
#ifdef TL_V6
  if (want (argc, argv, "v6"))
    run_v6 ();
#endif
  return 0;
}
