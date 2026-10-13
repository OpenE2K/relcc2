/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 16-byte atomics in 32-bit mode (spec/abi/mode32.md; the 64-bit rules
   are in spec/abi/atomics.md, "16-byte atomics"): what lcc -mptr32
   emits for __atomic_* on an __int128, and whether it works.  Read with
   -S for each target, and run: two threads add 1 + 2^64 to one
   __int128 (so both halves stay equal) through fetch_add and a
   compare-exchange loop, while a third reads it and counts loads whose
   halves differ (three runs with acquire loads, which lcc makes a
   16-byte load on v5+, then three with relaxed ones, which v5 makes two
   8-byte loads in one wide instruction).

     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc    # v3, v6.2c3
     $L -mptr32 -O2 -S atom32.c -o -          # what lcc emits
     $L -mptr32 -O2 atom32.c -latomic -lpthread -o atom32
     ./atom32
     # Mixing: thread 1 in code built for v3 (libatomic), thread 2 in
     # code built for the machine (inline on v5+), on one object:
     V3=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1
     $V3/bin/lcc -mptr32 -O2 -DPART=1 -c atom32.c -o a1.o
     $L -mptr32 -O2 -DPART=2 -c atom32.c -o a2.o
     $L -mptr32 -O2 -DPART=0 atom32.c a1.o a2.o -latomic -lpthread -o atom32mix
     # the same through lcc's lock-based v3 libatomic (loses updates):
     $L -mptr32 -O2 -static -DPART=0 atom32.c a1.o a2.o \
       $V3/lcc-home/1.29.16/e2k-v3-linux.cross/lib32/libatomic.a \
       -lpthread -o atom32mixlock

   Prints the lock-free answers, then for each run the totals (want 2 N
   each, N = 1 million) and the torn loads (want 0).  */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef PART
#define PART 3			/* everything in this file */
#endif
#define N 1000000
#define STEP (((unsigned __int128) 1 << 64) + 1)

extern unsigned __int128 q16;
void add_fetch (int n);
void add_cas (int n);
int is16 (void);
int is16b (void);

/* The operations, as separate functions to read with -S.  */
#if PART & 1
unsigned __int128
ld16 (void)
{
  return __atomic_load_n (&q16, __ATOMIC_SEQ_CST);
}
unsigned __int128
ld16_relaxed (void)
{
  return __atomic_load_n (&q16, __ATOMIC_RELAXED);
}
void
st16 (unsigned __int128 v)
{
  __atomic_store_n (&q16, v, __ATOMIC_SEQ_CST);
}
unsigned __int128 xchg16 (unsigned __int128 v)
{
  return __atomic_exchange_n (&q16, v, __ATOMIC_SEQ_CST);
}
int
cas16 (unsigned __int128 *e, unsigned __int128 d)
{
  return __atomic_compare_exchange_n (&q16, e, d, 0, __ATOMIC_SEQ_CST,
				      __ATOMIC_SEQ_CST);
}
unsigned __int128 fadd16 (unsigned __int128 v)
{
  return __atomic_fetch_add (&q16, v, __ATOMIC_SEQ_CST);
}
int always16 (void) { return __atomic_always_lock_free (16, 0); }
int is16 (void) { return __atomic_is_lock_free (16, &q16); }

void
add_fetch (int n)
{
  for (int i = 0; i < n; i++)
    __atomic_fetch_add (&q16, STEP, __ATOMIC_SEQ_CST);
}
#endif

#if PART & 2
void
add_cas (int n)
{
  for (int i = 0; i < n; i++)
    {
      unsigned __int128 e = __atomic_load_n (&q16, __ATOMIC_RELAXED);
      while (!__atomic_compare_exchange_n (&q16, &e, e + STEP, 1,
					   __ATOMIC_SEQ_CST,
					   __ATOMIC_RELAXED))
	;
    }
}

int is16b (void) { return __atomic_is_lock_free (16, &q16); }
#endif

#if PART == 0 || PART == 3
unsigned __int128 q16 __attribute__ ((aligned (16)));
static volatile int done, relaxed;
static long torn, reads;

static void *
t_fetch (void *a)
{
  (void) a;
  add_fetch (N);
  return 0;
}

static void *
t_cas (void *a)
{
  (void) a;
  add_cas (N);
  return 0;
}

static void *
t_read (void *a)
{
  (void) a;
  if (relaxed)
    while (!done)
      {
	unsigned __int128 v = __atomic_load_n (&q16, __ATOMIC_RELAXED);
	reads++;
	if ((unsigned long long) v != (unsigned long long) (v >> 64))
	  torn++;
      }
  else
    while (!done)
      {
	unsigned __int128 v = __atomic_load_n (&q16, __ATOMIC_ACQUIRE);
	reads++;
	if ((unsigned long long) v != (unsigned long long) (v >> 64))
	  torn++;
      }
  return 0;
}

int
main (void)
{
#if PART == 3
  printf ("always_lock_free(16) %d, is_lock_free(16) %d",
	  __atomic_always_lock_free (16, 0), is16 ());
#else
  printf ("is_lock_free(16): part 1 %d, part 2 %d", is16 (), is16b ());
#endif
#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16
  printf (", __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16\n");
#else
  printf (", no __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16\n");
#endif
  for (int run = 0; run < 6; run++)
    {
      pthread_t a, b, r;
      q16 = 0;
      done = 0;
      relaxed = run >= 3;
      torn = reads = 0;
      pthread_create (&r, 0, t_read, 0);
      pthread_create (&a, 0, t_fetch, 0);
      pthread_create (&b, 0, t_cas, 0);
      pthread_join (a, 0);
      pthread_join (b, 0);
      done = 1;
      pthread_join (r, 0);
      printf ("run %d: low %llu high %llu (want %d), torn %s loads %ld "
	      "of %ld\n", run, (unsigned long long) q16,
	      (unsigned long long) (q16 >> 64), 2 * N,
	      relaxed ? "relaxed" : "acquire", torn, reads);
    }
  return 0;
}
#endif
