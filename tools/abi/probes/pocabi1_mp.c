/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Message passing (MP) with the lines starting in different caches
   (spec/abi/atomics.md, "Memory ordering on hardware";
   docs/spec-poc-findings.md A29).  litmus.c's MP test always starts with
   both lines in the writer's cache; here the reader can hold them too.
   Also: whether a store in ALC5, plain or a wait-unlock store, becomes
   visible before an earlier store in ALC2 (lcc puts the unlock store of
   relaxed and acquire read-modify-writes in ALC5 on v5+).

     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc    (the 4C, -DV3:
       no ALC5 unlock store, which is SIGILL on v3; e2k-v5.8c2 for the
       8C2, e2k-v6.2c3 for the 2C3, without -DV3)
     $L -O2 -pthread -DV3 pocabi1_mp.c pocabi1_mp.s -o mp
     ./mp [ITERATIONS] [CPU_A CPU_B [SEED [SETUPS]]]

   SEED (default 0) changes the random delays; SETUPS (default RWS)
   picks the setups to run.

   Writer (pocabi1_mp.s): x = v; y = v, with v = 1, 2, 3, ... a new value
   each try, in consecutive wide instructions, with the variations the
   names give ("wait st_c": a wait between the stores; "rel": lcc's
   release store of y, { wait ld_c = 1, st_c = 1; std y }; "lock loop":
   y = v by lcc's exchange loop, which lcc uses at every order, with no
   wait).  Reader:
   r1 = y; r2 = x, in consecutive wide instructions, or with a wait
   ld_c = 1 between them, or in one wide instruction, or with lcc's
   acquire (a dummy use of r1, adds,sm r1, 0x0, %empty) between them.
   Counted per try: r1 and r2 as 1 (this try's v), 0 (the reset value)
   or "other" (an earlier try's value).  r1 = 1, r2 = 0 is the outcome
   that sequential consistency (and TSO) forbids.

   Setup, the letter after MP: R, the reader resets x and y before each
   try, so both lines start in its own cache; W, the writer does
   (litmus.c's setup); S, the writer does and then the reader reads x
   (a separate step between two barriers), so that x starts shared and y
   in the writer's cache alone.  The two threads meet at a barrier
   before each try and wait 0-15 empty loop iterations (random) so that
   their accesses overlap.  */

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE __attribute__ ((aligned (64)))
static long x LINE, y LINE, out[2] LINE;
static volatile long arrive[2][8] LINE;
static long iters = 1000000;
static int cpu[2] = { 0, 1 };
static void (*writer) (long *, long *, long);
static void (*reader) (long *, long *, long *);
static volatile long sink;
/* Who resets x and y before each try: 0 R the reader, 1 W the writer,
   2 S the writer, and then the reader reads x.  */
static int mode;
static unsigned seed0;

typedef void wfn (long *, long *, long);
typedef void rfn (long *, long *, long *);
wfn w_22, w_25, w_52, w_55, w_2L2, w_2L5, w_22w, w_22r;
rfn r_yx, r_yxw, r_yx1, r_yxu;

static void
pin (int c)
{
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (c, &s);
  sched_setaffinity (0, sizeof s, &s);
}

static void
barrier (int t, long k)
{
  __atomic_store_n (&arrive[t][0], k, __ATOMIC_SEQ_CST);
  while (__atomic_load_n (&arrive[!t][0], __ATOMIC_SEQ_CST) < k)
    ;
}

static void
delay (unsigned *seed)
{
  *seed = *seed * 1103515245 + 12345;
  for (volatile unsigned i = (*seed >> 16) & 15; i; i--)
    ;
}

static void *
writer_thread (void *arg)
{
  (void) arg;
  unsigned seed = 7 + seed0;
  long k = 0;
  pin (cpu[1]);
  for (long i = 0; i < iters; i++)
    {
      if (mode)
	x = y = 0;
      barrier (1, ++k);
      if (mode == 2)
	barrier (1, ++k);	/* while the reader reads x */
      delay (&seed);
      writer (&x, &y, i + 1);
      barrier (1, ++k);
    }
  return 0;
}

/* 1: this try's value; 0: the reset value; 2: anything else.  */
static int
kind (long got, long v)
{
  return got == v ? 1 : got == 0 ? 0 : 2;
}

int
main (int argc, char **argv)
{
  static const struct
  {
    const char *name;
    wfn *fn;
    rfn *rd;
  } v[] = {
    { "std,2 x; std,2 y", w_22, r_yx },
    { "std,2 x; std,5 y", w_25, r_yx },
    { "std,5 x; std,2 y", w_52, r_yx },
    { "std,5 x; std,5 y", w_55, r_yx },
    { "std,2 x; lock loop, unlock std,2 y", w_2L2, r_yx },
#ifndef V3
    { "std,2 x; lock loop, unlock std,5 y", w_2L5, r_yx },
#endif
    { "std,2 x; std,2 y || loads in one instr", w_22, r_yx1 },
    { "std,2 x; std,2 y || wait ld_c", w_22, r_yxw },
    { "std,2 x; std,2 y || acquire (use of y)", w_22, r_yxu },
    { "std,2 x; wait st_c; std,2 y", w_22w, r_yx },
    { "std,2 x; rel y", w_22r, r_yx },
    { "std,2 x; wait st_c; std,2 y || wait", w_22w, r_yxw },
    { "std,2 x; wait st_c; std,2 y || acquire", w_22w, r_yxu },
    { "std,2 x; rel y || acquire", w_22r, r_yxu },
    { "std,2 x; lock loop, unlock std,2 y || acquire", w_2L2, r_yxu },
#ifndef V3
    { "std,2 x; lock loop, unlock std,5 y || acquire", w_2L5, r_yxu },
#endif
  };
  const unsigned nv = sizeof v / sizeof v[0];
  const char *setups = "RWS";
  if (argc > 1)
    iters = atol (argv[1]);
  if (argc > 3)
    {
      cpu[0] = atoi (argv[2]);
      cpu[1] = atoi (argv[3]);
    }
  if (argc > 4)
    seed0 = atoi (argv[4]);
  if (argc > 5)
    setups = argv[5];
  pin (cpu[0]);
  for (unsigned n = 0; n < 3 * nv; n++)
    {
      long seen[3][3] = { { 0 } };
      unsigned seed = 1 + seed0;
      long k = 0;
      pthread_t th;
      unsigned m = n % nv;
      mode = n / nv;
      if (!strchr (setups, "RWS"[mode]))
	continue;
      writer = v[m].fn;
      reader = v[m].rd;
      arrive[0][0] = arrive[1][0] = 0;
      pthread_create (&th, 0, writer_thread, 0);
      for (long i = 0; i < iters; i++)
	{
	  if (!mode)
	    x = y = 0;
	  barrier (0, ++k);
	  if (mode == 2)
	    {
	      sink = *(volatile long *) &x;
	      barrier (0, ++k);
	    }
	  delay (&seed);
	  reader (&x, &y, out);
	  barrier (0, ++k);
	  seen[kind (out[0], i + 1)][kind (out[1], i + 1)]++;
	}
      pthread_join (th, 0);
      long other = 0;
      for (int a = 0; a < 3; a++)
	other += seen[a][2] + seen[2][a];
      other -= seen[2][2];
      printf ("MP %c %-46s r1,r2 = 0,0: %7ld  0,1: %7ld  1,1: %7ld  "
	      "other: %ld  1,0: %ld\n", "RWS"[mode], v[m].name, seen[0][0],
	      seen[0][1], seen[1][1], other, seen[1][0]);
    }
  return 0;
}
