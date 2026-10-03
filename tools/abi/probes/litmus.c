/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Memory-model litmus tests and 16-byte atomics on hardware
   (spec/abi/atomics.md).

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O2 -pthread tools/abi/probes/litmus.c tools/abi/probes/litmus.s \
         -latomic -o litmus
     ./litmus [ITERATIONS] [CPU_A CPU_B [TEST]]

   Two threads, pinned to CPU_A and CPU_B (default 0 and 1: one chip;
   0 and 4 are on different chips), run each test ITERATIONS times
   (default 200000), meeting at a barrier before every iteration, and
   count the outcomes that sequential consistency forbids:

     SB  x = 1; r1 = y   ||  y = 1; r2 = x      r1 = r2 = 0  (store -> load)
     MP  x = 1; y = 1    ||  r1 = y; r2 = x     r1 = 1, r2 = 0
     LB  r1 = x; y = 1   ||  r2 = y; x = 1      r1 = r2 = 1  (load -> store)

   with each access in its own wide instruction, both in one, or lcc's
   fence (wait) between them, and MP also with lcc's release store and
   acquire load.  Then 16-byte
   atomics (libatomic on v3): both threads add (1, 1) to an __int128
   pair with __atomic_fetch_add and CAS loops, and one reader checks that
   no load sees two different halves.  */

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE __attribute__ ((aligned (64)))

static long iters = 200000;
static int cpu[2] = { 0, 1 };

static volatile long x LINE, y LINE;
static long r1 LINE, r2 LINE;

/* Sense-reversing barrier for two threads.  */
static int bcount LINE, bsense;

static void
barrier (int *local)
{
  *local = !*local;
  if (__atomic_add_fetch (&bcount, 1, __ATOMIC_SEQ_CST) == 2)
    {
      __atomic_store_n (&bcount, 0, __ATOMIC_RELAXED);
      __atomic_store_n (&bsense, *local, __ATOMIC_SEQ_CST);
    }
  else
    while (__atomic_load_n (&bsense, __ATOMIC_ACQUIRE) != *local)
      ;
}

/* A short random delay so the two sides overlap at varying offsets.  */
static void
jitter (unsigned *seed)
{
  *seed = *seed * 1103515245 + 12345;
  for (volatile unsigned i = (*seed >> 16) & 7; i; i--)
    ;
}

/* The sides, in litmus.s (hand-written, so the hardware sees exactly
   this order) and below (lcc's acquire/release code).  All take
   (a, b, out); st_ld and ld_st return the value loaded.  */
typedef long (*sidefn) (volatile long *, volatile long *, long *);
#define SIDES(f) long f (volatile long *, volatile long *, long *);
SIDES (st_ld) SIDES (st_ld_same) SIDES (st_ld_wait) SIDES (st_ld_waitst)
SIDES (ld_st) SIDES (ld_st_same) SIDES (st_st) SIDES (st_st_same)
SIDES (st_st_wait) SIDES (st_st_same_rev) SIDES (ld_ld) SIDES (ld_ld_same) SIDES (ld_ld_wait)

static long
st_st_rel (volatile long *a, volatile long *b, long *out)
{
  (void) out;
  __atomic_store_n (a, 1, __ATOMIC_RELAXED);
  __atomic_store_n (b, 1, __ATOMIC_RELEASE);
  return 0;
}

static long
ld_ld_acq (volatile long *a, volatile long *b, long *out)
{
  out[0] = __atomic_load_n (a, __ATOMIC_ACQUIRE);
  out[1] = __atomic_load_n (b, __ATOMIC_RELAXED);
  return 0;
}

enum kind { SB, MP, LB };
static const struct test
{
  const char *name;
  enum kind kind;
  sidefn a, b;
} tests[] = {
  { "SB", SB, st_ld, st_ld },
  { "SB same", SB, st_ld_same, st_ld_same },
  { "SB wait", SB, st_ld_wait, st_ld_wait },
  { "SB wait st_c", SB, st_ld_waitst, st_ld_waitst },
  { "MP", MP, st_st, ld_ld },
  { "MP st same", MP, st_st_same, ld_ld },
  { "MP st same, x in ALC5", MP, st_st_same_rev, ld_ld },
  { "MP ld same", MP, st_st, ld_ld_same },
  { "MP st wait", MP, st_st_wait, ld_ld },
  { "MP ld wait", MP, st_st, ld_ld_wait },
  { "MP rel/acq", MP, st_st_rel, ld_ld_acq },
  { "LB", LB, ld_st, ld_st },
  { "LB same", LB, ld_st_same, ld_st_same },
};
#define NTESTS (int) (sizeof tests / sizeof tests[0])

/* Thread T's side of test K: SB and LB are symmetric (thread 1 swaps x
   and y); in MP thread 0 writes x then y, thread 1 reads y then x.  */
static void
side (int t, int k)
{
  const struct test *e = &tests[k];
  long out[2];
  if (e->kind == MP)
    {
      if (t)
        {
          e->b (&y, &x, out);
          r1 = out[0];
          r2 = out[1];
        }
      else
        e->a (&x, &y, out);
    }
  else if (t)
    r2 = e->b (&y, &x, out);
  else
    r1 = e->a (&x, &y, out);
}

static int
forbidden (int k)
{
  switch (tests[k].kind)
    {
    case SB: return r1 == 0 && r2 == 0;
    case MP: return r1 == 1 && r2 == 0;
    case LB: return r1 == 1 && r2 == 1;
    }
  return 0;
}

static long seen[NTESTS];
static const char *only;

/* With a fifth argument, run only the tests whose names contain it.  */
static int
want (int k)
{
  return !only || strstr (tests[k].name, only);
}

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

static void *
run (void *arg)
{
  int t = (int) (long) arg, local = 0;
  unsigned seed = 1 + t;
  pin (cpu[t]);
  for (int k = 0; k < NTESTS; k++)
    for (long i = 0; want (k) && i < iters; i++)
      {
        if (t == 0)
          {
            x = 0;
            y = 0;
            r1 = r2 = -1;
          }
        barrier (&local);
        jitter (&seed);
        side (t, k);
        barrier (&local);
        if (t == 0)
          seen[k] += forbidden (k);
      }
  return NULL;
}

/* 16-byte atomics.  */
static unsigned __int128 q LINE;
static long torn;
static volatile int done;

static void *
q_add (void *arg)
{
  int t = (int) (long) arg;
  pin (cpu[t]);
  unsigned __int128 one = ((unsigned __int128) 1 << 64) | 1;
  for (long i = 0; i < iters; i++)
    {
      if (i & 1)
        __atomic_fetch_add (&q, one, __ATOMIC_SEQ_CST);
      else
        {
          unsigned __int128 old = __atomic_load_n (&q, __ATOMIC_RELAXED);
          while (!__atomic_compare_exchange_n (&q, &old, old + one, 1,
                                               __ATOMIC_SEQ_CST,
                                               __ATOMIC_RELAXED))
            ;
        }
    }
  return NULL;
}

static void *
q_read (void *arg)
{
  (void) arg;
  pin (cpu[0] + 2);
  while (!done)
    {
      unsigned __int128 v = __atomic_load_n (&q, __ATOMIC_ACQUIRE);
      torn += (unsigned long) v != (unsigned long) (v >> 64);
    }
  return NULL;
}

int
main (int argc, char **argv)
{
  if (argc > 1)
    iters = atol (argv[1]);
  if (argc > 3)
    {
      cpu[0] = atoi (argv[2]);
      cpu[1] = atoi (argv[3]);
    }
  if (argc > 4)
    only = argv[4];
  setvbuf (stdout, NULL, _IOLBF, 0);
  pthread_t th[3];
  for (long t = 0; t < 2; t++)
    pthread_create (&th[t], NULL, run, (void *) t);
  for (int t = 0; t < 2; t++)
    pthread_join (th[t], NULL);
  printf ("CPUs %d, %d; %ld iterations each\n", cpu[0], cpu[1], iters);
  for (int k = 0; k < NTESTS; k++)
    if (want (k))
      printf ("  %-22s forbidden outcome %ld times\n", tests[k].name, seen[k]);

  pthread_create (&th[2], NULL, q_read, NULL);
  for (long t = 0; t < 2; t++)
    pthread_create (&th[t], NULL, q_add, (void *) t);
  for (int t = 0; t < 2; t++)
    pthread_join (th[t], NULL);
  done = 1;
  pthread_join (th[2], NULL);
  printf ("16-byte: halves %lu, %lu (expected %ld each), torn reads %ld,"
          " lock-free %d\n", (unsigned long) q, (unsigned long) (q >> 64),
          2 * iters, torn, (int) __atomic_is_lock_free (16, &q));
  return 0;
}
