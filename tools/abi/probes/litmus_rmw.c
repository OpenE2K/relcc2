/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Ordering of atomic read-modify-writes as lcc compiles them, across
   processor chips (spec/abi/atomics.md, spec/isa/hwbugs.md: MCST's
   kernel says that on the E4C (E2S) with several nodes "atomic
   operations have fully relaxed memory ordering", erratum #89527, and
   puts `wait st_c, ma_c` around its own atomics).

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O3 -pthread tools/abi/probes/litmus_rmw.c -o litmus_rmw
     ./litmus_rmw [ITERATIONS] [CPU_A CPU_B]

   Two threads pinned to CPU_A and CPU_B (default 0, 1; on the 4C 1 and 9
   are on different chips) meet at a barrier before every iteration and
   count outcomes that sequential consistency forbids, with seq_cst
   atomics as lcc emits them (a lock-wait load and a wait-unlock store):

     MP-xchg   x = 1; xchg (y, 1)      ||  r1 = y (acquire); r2 = x
     MP-fadd   x = 1; y = 1            ||  r1 = fetch_add (y, 0); r2 = x
     SB-xchg   xchg (x, 1); r1 = y     ||  xchg (y, 1); r2 = x  (seq_cst loads)
     SB-fadd   fetch_add (x, 1); r1 = y || fetch_add (y, 1); r2 = x  (seq_cst)

   r1 = 1, r2 = 0 for MP and r1 = r2 = 0 for SB are forbidden.  */

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

#define LINE __attribute__ ((aligned (64)))
static long x LINE, y LINE, r1 LINE, r2 LINE;
static volatile long go[2] LINE;
static long iters = 2000000;
static int cpu[2] = { 0, 1 };
static int test;
static long seen;

static void
pin (int c)
{
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (c, &s);
  if (pthread_setaffinity_np (pthread_self (), sizeof s, &s))
    {
      fprintf (stderr, "cannot pin to CPU %d\n", c);
      exit (1);
    }
}

/* Sense-reversing barrier for two threads.  */
static void
barrier (int t, long *phase)
{
  ++*phase;
  __atomic_store_n (&go[t], *phase, __ATOMIC_RELEASE);
  while (__atomic_load_n (&go[1 - t], __ATOMIC_ACQUIRE) < *phase)
    ;
}

static void
side (int t)
{
  switch (test * 2 + t)
    {
    case 0: x = 1; __atomic_exchange_n (&y, 1, __ATOMIC_SEQ_CST); break;
    case 1: r1 = __atomic_load_n (&y, __ATOMIC_ACQUIRE); r2 = x; break;
    case 2: x = 1; __atomic_store_n (&y, 1, __ATOMIC_RELEASE); break;
    case 3: r1 = __atomic_fetch_add (&y, 0, __ATOMIC_SEQ_CST); r2 = x; break;
    case 4:
      __atomic_exchange_n (&x, 1, __ATOMIC_SEQ_CST);
      r1 = __atomic_load_n (&y, __ATOMIC_SEQ_CST);
      break;
    case 5:
      __atomic_exchange_n (&y, 1, __ATOMIC_SEQ_CST);
      r2 = __atomic_load_n (&x, __ATOMIC_SEQ_CST);
      break;
    case 6:
      __atomic_fetch_add (&x, 1, __ATOMIC_SEQ_CST);
      r1 = __atomic_load_n (&y, __ATOMIC_SEQ_CST);
      break;
    case 7:
      __atomic_fetch_add (&y, 1, __ATOMIC_SEQ_CST);
      r2 = __atomic_load_n (&x, __ATOMIC_SEQ_CST);
      break;
    }
}

static void *
run (void *arg)
{
  int t = (int) (long) arg;
  long phase = 0;
  pin (cpu[t]);
  for (long i = 0; i < iters; i++)
    {
      if (t == 0)
        {
          x = y = 0;
          r1 = r2 = -1;
        }
      barrier (t, &phase);
      side (t);
      barrier (t, &phase);
      if (t == 0)
        seen += test < 2 ? (r1 == 1 && r2 == 0) : (r1 == 0 && r2 == 0);
    }
  return NULL;
}

int
main (int argc, char **argv)
{
  static const char *names[] = { "MP-xchg", "MP-fadd", "SB-xchg", "SB-fadd" };
  if (argc > 1)
    iters = atol (argv[1]);
  if (argc > 3)
    {
      cpu[0] = atoi (argv[2]);
      cpu[1] = atoi (argv[3]);
    }
  printf ("CPUs %d, %d; %ld iterations each\n", cpu[0], cpu[1], iters);
  for (test = 0; test < 4; test++)
    {
      pthread_t th[2];
      seen = 0;
      go[0] = go[1] = 0;
      for (long t = 0; t < 2; t++)
        pthread_create (&th[t], NULL, run, (void *) t);
      for (int t = 0; t < 2; t++)
        pthread_join (th[t], NULL);
      printf ("  %-8s forbidden outcome %ld times\n", names[test], seen);
    }
  return 0;
}
