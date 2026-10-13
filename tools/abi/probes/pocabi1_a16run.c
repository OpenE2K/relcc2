/* SPDX-License-Identifier: GPL-3.0-or-later */
/* lcc's inline 16-byte atomics (v5+) on hardware (spec/abi/atomics.md,
   "16-byte atomics"; docs/spec-poc-findings.md A29).  The sequences are
   read from pocabi1_atom16.c's assembly; this runs them:

     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc   (or e2k-v6.2c3)
     $L -O3 -pthread pocabi1_a16run.c -o a16run
     ./a16run [count|tear|cas|time] [ITERATIONS] [CPU_A CPU_B]

   e2k-v5.8c2 programs don't run on the 2C3 ("Exec format error"); to run
   lcc's 8C2 code there, compile with the 8C2 lcc's -S and assemble and
   link that with the 2C3 lcc.

   count  two threads on CPU_A and CPU_B add A = 2^64 + 2^63 + 1 (a carry
          out of the low half every second add) ITERATIONS times each:
          fetch_add relaxed against fetch_add seq_cst, fetch_add against a
          compare-exchange loop, add_fetch against fetch_sub of -A; then
          exchange: the values put in and the values taken out (plus the
          last one left) must have the same sum.  Prints the result and
          the expected value.
   tear   one thread stores (k, k) for k = 1, 2, ... (relaxed, release or
          seq_cst store, or exchange); the other reads with a relaxed,
          acquire or seq_cst load and counts reads whose halves differ.
          lcc's v5 relaxed load is two ldd (one wide instruction through a
          pointer, two for a global), not one 16-byte access.  Built with
          -DGAPS and pocabi1_tear.s, it also reads the halves with two ldd
          in one wide instruction, in consecutive ones, and 8 and 32
          cycles apart.
   cas    one thread: results of strong and weak compare-exchange that
          succeed and fail, *expected afterwards, the object afterwards.
   time   one thread, uncontended: %clkr cycles per operation, the best of
          5 runs of ITERATIONS.  */

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned __int128 U;
#define R __ATOMIC_RELAXED
#define AQ __ATOMIC_ACQUIRE
#define RL __ATOMIC_RELEASE
#define SC __ATOMIC_SEQ_CST
#define NI __attribute__ ((noinline))
#define LINE __attribute__ ((aligned (64)))

static U obj LINE;              /* the shared object, through pointers */
static U gobj LINE;             /* the same, used by name (global form) */
static long iters = 1000000;
static int cpu[2] = { 0, 1 };
static volatile int start_flag LINE;
static volatile int stop_flag LINE;
static const U A = ((U) 1 << 64) + ((U) 1 << 63) + 1;
#ifdef GAPS
U rd_gap0 (U *), rd_gap1 (U *), rd_gap8 (U *), rd_gap32 (U *);
#endif

/* Every operation in its own function, as in pocabi1_atom16.c.  */
NI static U ld_r (U *p) { return __atomic_load_n (p, R); }
NI static U ld_a (U *p) { return __atomic_load_n (p, AQ); }
NI static U ld_s (U *p) { return __atomic_load_n (p, SC); }
NI static U gld_r (void) { return __atomic_load_n (&gobj, R); }
NI static U gld_s (void) { return __atomic_load_n (&gobj, SC); }
NI static void st_r (U *p, U v) { __atomic_store_n (p, v, R); }
NI static void st_l (U *p, U v) { __atomic_store_n (p, v, RL); }
NI static void st_s (U *p, U v) { __atomic_store_n (p, v, SC); }
NI static void gst_r (U v) { __atomic_store_n (&gobj, v, R); }
NI static U xchg_r (U *p, U v) { return __atomic_exchange_n (p, v, R); }
NI static U xchg_s (U *p, U v) { return __atomic_exchange_n (p, v, SC); }
NI static U fadd_r (U *p, U v) { return __atomic_fetch_add (p, v, R); }
NI static U fadd_s (U *p, U v) { return __atomic_fetch_add (p, v, SC); }
NI static U addf_s (U *p, U v) { return __atomic_add_fetch (p, v, SC); }
NI static U fsub_r (U *p, U v) { return __atomic_fetch_sub (p, v, R); }
NI static int
cas_s (U *p, U *e, U d)
{
  return __atomic_compare_exchange_n (p, e, d, 0, SC, SC);
}
NI static int
cas_r (U *p, U *e, U d)
{
  return __atomic_compare_exchange_n (p, e, d, 0, R, R);
}
NI static int
casw_a (U *p, U *e, U d)
{
  return __atomic_compare_exchange_n (p, e, d, 1, AQ, AQ);
}

static void
pin (int c)
{
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (c, &s);
  sched_setaffinity (0, sizeof s, &s);
}

static void
phex (const char *what, U v)
{
  printf ("%s%016lx_%016lx", what, (unsigned long) (v >> 64),
	  (unsigned long) v);
}

/* count */
static int count_mode;
static U xsum_in[2], xsum_out[2];

static void *
count_thread (void *arg)
{
  int t = (int) (long) arg;
  pin (cpu[t]);
  while (!start_flag)
    ;
  U x = 0;
  for (long i = 0; i < iters; i++)
    switch (count_mode * 2 + t)
      {
      case 0: fadd_r (&obj, A); break;
      case 1: fadd_s (&obj, A); break;
      case 2: fadd_s (&obj, A); break;
      case 3:
	{
	  U e = ld_r (&obj);
	  while (!casw_a (&obj, &e, e + A))
	    ;
	  break;
	}
      case 4: addf_s (&obj, A); break;
      case 5: fsub_r (&obj, -A); break;
      case 6: case 7:
	{
	  /* Distinct values with both halves changing.  */
	  U v = ((U) (t + 1) << 96) + ((U) i << 64) + (U) i * 0x9e3779b97f4a7c15ULL + t;
	  xsum_in[t] += v;
	  x += t ? xchg_s (&obj, v) : xchg_r (&obj, v);
	  break;
	}
      }
  xsum_out[t] = x;
  return 0;
}

static int
run_count (void)
{
  static const char *names[] = {
    "fetch_add relaxed || fetch_add seq_cst",
    "fetch_add seq_cst || weak CAS loop (acquire)",
    "add_fetch seq_cst || fetch_sub relaxed of -A",
    "exchange relaxed || exchange seq_cst",
  };
  int bad = 0;
  for (count_mode = 0; count_mode < 4; count_mode++)
    {
      pthread_t th[2];
      obj = count_mode == 3 ? (U) 12345 << 70 : 0;
      U initial = obj;
      start_flag = 0;
      xsum_in[0] = xsum_in[1] = xsum_out[0] = xsum_out[1] = 0;
      for (int t = 0; t < 2; t++)
	pthread_create (&th[t], 0, count_thread, (void *) (long) t);
      start_flag = 1;
      for (int t = 0; t < 2; t++)
	pthread_join (th[t], 0);
      U got, want;
      if (count_mode < 3)
	{
	  got = obj;
	  want = A * (U) (2 * iters);
	}
      else
	{
	  got = xsum_out[0] + xsum_out[1] + obj;
	  want = initial + xsum_in[0] + xsum_in[1];
	}
      printf ("count %-46s ", names[count_mode]);
      phex ("got ", got);
      phex (" want ", want);
      printf (" %s\n", got == want ? "ok" : "WRONG");
      bad += got != want;
    }
  return bad;
}

/* tear */
static int writer_kind, reader_kind;

static void *
tear_writer (void *arg)
{
  (void) arg;
  pin (cpu[1]);
  while (!start_flag)
    ;
  for (unsigned long k = 1; !stop_flag; k++)
    {
      U v = ((U) k << 64) | k;
      switch (writer_kind)
	{
	case 0: st_r (&obj, v); break;
	case 1: st_l (&obj, v); break;
	case 2: st_s (&obj, v); break;
	case 3: xchg_r (&obj, v); break;
	case 4: gst_r (v); break;
	}
    }
  return 0;
}

static int
run_tear (void)
{
  static const char *wn[] = { "store relaxed", "store release",
			      "store seq_cst", "exchange relaxed",
			      "store relaxed (global)" };
  static const char *rn[] = { "load relaxed", "load acquire", "load seq_cst",
			      "load relaxed (global)", "load seq_cst (global)",
			      "ldd + ldd, same instr", "ldd + ldd, 1 apart",
			      "ldd + ldd, 8 apart", "ldd + ldd, 32 apart" };
  static const int pairs[][2] = { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 1, 0 },
				  { 2, 0 }, { 3, 0 }, { 4, 3 }, { 4, 4 },
#ifdef GAPS
				  { 0, 5 }, { 0, 6 }, { 0, 7 }, { 0, 8 },
#endif
  };
  pin (cpu[0]);
  for (unsigned p = 0; p < sizeof pairs / sizeof pairs[0]; p++)
    {
      pthread_t th;
      writer_kind = pairs[p][0];
      reader_kind = pairs[p][1];
      obj = gobj = 0;
      start_flag = stop_flag = 0;
      pthread_create (&th, 0, tear_writer, 0);
      start_flag = 1;
      while (ld_s (&obj) == 0 && gld_s () == 0)
	;                       /* until the writer runs */
      long torn = 0, changed = 0;
      U last = 0;
      for (long i = 0; i < iters; i++)
	{
	  U v;
	  switch (reader_kind)
	    {
	    case 0: v = ld_r (&obj); break;
	    case 1: v = ld_a (&obj); break;
	    case 2: v = ld_s (&obj); break;
	    case 3: v = gld_r (); break;
	    case 4: v = gld_s (); break;
#ifdef GAPS
	    case 5: v = rd_gap0 (&obj); break;
	    case 6: v = rd_gap1 (&obj); break;
	    case 7: v = rd_gap8 (&obj); break;
	    case 8: v = rd_gap32 (&obj); break;
#endif
	    default: abort ();
	    }
	  torn += (unsigned long) v != (unsigned long) (v >> 64);
	  changed += v != last;
	  last = v;
	}
      stop_flag = 1;
      pthread_join (th, 0);
      printf ("tear  %-24s || %-22s reads %ld, changed %ld, torn %ld\n",
	      wn[writer_kind], rn[reader_kind], iters, changed, torn);
    }
  return 0;
}

/* cas */
static int
run_cas (void)
{
  int bad = 0;
  static const char *nm[] = { "strong seq_cst", "strong relaxed",
			      "weak acquire" };
  for (int k = 0; k < 3; k++)
    {
      U v0 = ((U) 0x1111 << 64) | 0x2222, v1 = ((U) 0x3333 << 64) | 0x4444;
      U e, r;
      int ok;
      /* Success.  */
      obj = v0;
      e = v0;
      ok = k == 0 ? cas_s (&obj, &e, v1) : k == 1 ? cas_r (&obj, &e, v1)
	: casw_a (&obj, &e, v1);
      r = obj;
      int good = ok == 1 && r == v1 && e == v0;
      printf ("cas   %-15s equal:       returned %d, object %s, expected %s\n",
	      nm[k], ok, r == v1 ? "new" : "OTHER", e == v0 ? "kept" : "OTHER");
      /* Failure in the low half only, then in the high half only.  */
      for (int half = 0; half < 2; half++)
	{
	  obj = v0;
	  e = half ? v0 ^ ((U) 1 << 64) : v0 ^ 1;
	  ok = k == 0 ? cas_s (&obj, &e, v1) : k == 1 ? cas_r (&obj, &e, v1)
	    : casw_a (&obj, &e, v1);
	  r = obj;
	  good &= ok == 0 && r == v0 && e == v0;
	  printf ("cas   %-15s %s differs: returned %d, object %s, expected %s\n",
		  nm[k], half ? "high half" : "low half ", ok,
		  r == v0 ? "kept" : "CHANGED", e == v0 ? "= object" : "OTHER");
	}
      bad += !good;
    }
  /* Exchange and fetch-ops return the old value.  */
  obj = A;
  U o1 = xchg_s (&obj, 7), o2 = fadd_s (&obj, A), o3 = obj;
  int good = o1 == A && o2 == 7 && o3 == 7 + A;
  printf ("ops   exchange, fetch_add return the old value: %s\n",
	  good ? "yes" : "NO");
  bad += !good;
  return bad;
}

/* time */
static long
clkr (void)
{
  long x;
  __asm__ volatile ("rrd %%clkr, %0" : "=r" (x));
  return x;
}

static int
run_time (void)
{
  static const char *nm[] = {
    "load relaxed", "load acquire", "load seq_cst", "store relaxed",
    "store release", "store seq_cst", "exchange relaxed", "exchange seq_cst",
    "fetch_add relaxed", "fetch_add seq_cst", "CAS seq_cst, succeeds",
    "CAS seq_cst, fails", "nothing (the loop alone)"
  };
  pin (cpu[0]);
  for (int op = 0; op < 13; op++)
    {
      long best = -1;
      for (int rep = 0; rep < 5; rep++)
	{
	  obj = 0;
	  U e = 0, sink = 0;
	  long t = clkr ();
	  for (long i = 0; i < iters; i++)
	    switch (op)
	      {
	      case 0: sink += ld_r (&obj); break;
	      case 1: sink += ld_a (&obj); break;
	      case 2: sink += ld_s (&obj); break;
	      case 3: st_r (&obj, i); break;
	      case 4: st_l (&obj, i); break;
	      case 5: st_s (&obj, i); break;
	      case 6: sink += xchg_r (&obj, i); break;
	      case 7: sink += xchg_s (&obj, i); break;
	      case 8: sink += fadd_r (&obj, A); break;
	      case 9: sink += fadd_s (&obj, A); break;
	      case 10: e = obj; sink += cas_s (&obj, &e, e + 1); break;
	      case 11: e = 99; sink += cas_s (&obj, &e, 5); break;
	      case 12: sink += i; __asm__ volatile ("" ::: "memory"); break;
	      }
	  t = clkr () - t;
	  if (best < 0 || t < best)
	    best = t;
	  if (sink == 1)
	    printf ("!");
	}
      printf ("time  %-26s %6.1f cycles\n", nm[op], (double) best / iters);
    }
  return 0;
}

int
main (int argc, char **argv)
{
  const char *what = argc > 1 ? argv[1] : "count";
  if (argc > 2)
    iters = atol (argv[2]);
  if (argc > 4)
    {
      cpu[0] = atoi (argv[3]);
      cpu[1] = atoi (argv[4]);
    }
  printf ("lock free: %d, always lock free: %d\n",
	  (int) __atomic_is_lock_free (16, &obj),
	  (int) __atomic_always_lock_free (16, 0));
  if (!strcmp (what, "count"))
    return run_count ();
  if (!strcmp (what, "tear"))
    return run_tear ();
  if (!strcmp (what, "cas"))
    return run_cas ();
  if (!strcmp (what, "time"))
    return run_time ();
  fprintf (stderr, "unknown test %s\n", what);
  return 2;
}
