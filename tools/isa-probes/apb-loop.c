/* SPDX-License-Identifier: GPL-3.0-or-later */
/* How an array prefetch (APB) stream survives traps (spec/isa/apb.md).
   apb-loop.s has three hand-written loops that sum an array through
   movad:

     loopA  loop_mode loop counted by %lsr, %lsr written before bap
     loopB  ordinary branch loop, no %lsr
     loopC  as loopA, but %lsr is written after bap, a page fault comes
            in between, and a 777-iteration loop before it left the
            loop counters stale
     loopE  as loopA with a given %lsr: what movad returns in epilogue
            iterations (ecnt) for each ldmc and ldovl (spec/isa/loops.md)
     loopF  loopA with a given %lsr: short loops (lcnt 8, ecnt 31,
            ldmc 1) run many times, so that some interrupts land in
            epilogue iterations

   After a trap the kernel rebuilds each area's position from the loop
   counters (%ilcr - %lsr), so only loopA stays correct on v3-v5
   hardware.  v6 (the 2C3) keeps the positions, and qemu-e2k does not
   model this: all three pass there.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O1 tools/isa-probes/apb-loop.c tools/isa-probes/apb-loop.s -o apb-loop
     ./apb-loop          # timer interrupts only
     ./apb-loop timer    # plus a SIGALRM every 100 us  */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

long loopA (const long *, long, long *);
long loopB (const long *, long, long *);
long loopC (const long *, long, long *, char *);
long loopE (const long *, unsigned long, long *);
long loopF (const long *, unsigned long, long *);

static volatile long ticks;

static void
tick (int s)
{
  (void) s;
  ticks++;
}

int
main (int argc, char **argv)
{
  long n = 1 << 20, reps = 200, pg = sysconf (_SC_PAGESIZE);
  long *a = aligned_alloc (64, 8 * (n + 64));
  for (long i = 0; i < n + 64; i++)
    a[i] = i + 1;
  signal (SIGALRM, tick);
  struct itimerval it = { { 0, 100 }, { 0, 100 } };
  if (argc > 1)
    setitimer (ITIMER_REAL, &it, NULL);

  /* Sums of 1M elements: long enough for timer interrupts.  */
  long (*f[2]) (const long *, long, long *) = { loopA, loopB };
  for (int k = 0; k < 2; k++)
    {
      long bad = 0;
      ticks = 0;
      for (long r = 0; r < reps; r++)
        {
          long m = n - r, iters = -1, s = f[k] (a, m, &iters);
          if (iters != m || s != m * (m + 1) / 2)
            bad++;
        }
      printf ("loop%c: %ld wrong sums of %ld, %ld signals: %s\n", 'A' + k,
              bad, reps, ticks,
              k == 0 ? (bad ? "FAIL (expected none)" : "PASS")
                     : (bad ? "PASS (wrong, as expected on hardware)"
                            : "no wrong sums (v6, qemu, or no interrupt hit)"));
    }

  /* loopC on a fresh page (the load faults) and on a touched one.  */
  for (int r = 0; r < 4; r++)
    {
      char *p = mmap (0, pg, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (r >= 2)
        p[0] = 1;
      long m = 1000, iters = -1, s = loopC (a, m, &iters, p);
      long shift = (s - m * (m + 1) / 2) / m;
      printf ("loopC %s: stream shifted by %ld elements: %s\n",
              r < 2 ? "with a page fault" : "without a fault  ", shift,
              (r < 2 ? shift == 777 : shift == 0) ? "PASS"
              : r < 2 && shift == 0 ? "no shift (v6 or qemu)" : "FAIL");
      munmap (p, pg);
    }

  /* movad in epilogue iterations: lcnt 5, ecnt 2.  */
  static const struct { int ldmc, ldovl; } ep[] = {
    { 0, 0 }, { 1, 0 }, { 0, 255 }, { 1, 255 }, { 0, 1 }, { 0, 2 } };
  for (unsigned k = 0; k < sizeof ep / sizeof *ep; k++)
    {
      long out[32] = { 0 };
      unsigned long lsr = 5 | 2UL << 32 | 1UL << 37
                          | (unsigned long) ep[k].ldmc << 39
                          | (unsigned long) ep[k].ldovl << 40;
      long it = loopE (a, lsr, out);
      printf ("loopE ldmc %d ldovl %3d: %ld iterations, movad gave",
              ep[k].ldmc, ep[k].ldovl, it);
      for (long i = 0; i < it && i < 16; i++)
        printf (" %ld%s", out[2 * i], out[2 * i + 1] ? "(tag)" : "");
      printf ("\n");
    }
  /* Epilogue iterations with movas kept on (ldmc 1), under the same
     interrupts: lcnt 8 + ecnt 31 iterations sum a[0..38].  */
  for (int ldmc = 1; ldmc >= 0; ldmc--)
    {
      unsigned long lsr = 8 | 31UL << 32 | 1UL << 37 | (unsigned long) ldmc << 39
                          | 255UL << 40;
      long bad = 0, reps2 = 1000000, first = -1, firstit = -1;
      ticks = 0;
      for (long r = 0; r < reps2; r++)
        {
          long iters = -1, s = loopF (a, lsr, &iters);
          if (r == 0)
            first = s, firstit = iters;
          else if (s != first || iters != firstit)
            bad++;
        }
      printf ("loopF ldmc %d: %ld iterations, sum %ld (%s); %ld of %ld "
              "runs differ, %ld signals\n", ldmc, firstit, first,
              first == 39 * 40 / 2 ? "all 39 elements" : "not 1..39", bad,
              reps2, ticks);
    }
  return 0;
}
