/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Timings and user-mode checks of facilities made for the x86 binary
   translator, for native code (spec/isa/x86-support.md): the cost of
   srp/crp, of libgcc's __clear_cache, what UPSR.fsm changes in FP
   compares, the delays of the %bgr rotation, and on v6 the wait for a
   memory change.  native.s, native-rot.s (v3-v5) and native6.s (v6;
   the last two from gen_native.py).

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O3 native.c native.s native-rot.s -o native -lpthread
     L6=/opt/mcst/lcc-1.29.16.e2k-v6.2c3.linux-6.1/bin/lcc
     $L6 -O3 -DNAT_V6 native.c native.s native6.s -o native6 -lpthread  */

#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

long nat_clkr (void);
long nat_loop (long), nat_srpcrp (long), nat_srp (long);
long nat_rd_upsr (void);
void nat_wr_upsr (long);
long nat_fcmp (long *, long);
#ifdef NAT_V6
# define ROTS(X) X (abgc) X (rbgc) X (rws)
long nat_watch (long *, long);
#else
# define ROTS(X) X (abgd) X (abgi) X (rws)
#endif
#define DECL(k) long nat_rot_##k##_d0 (void), nat_rot_##k##_d1 (void), \
  nat_rot_##k##_d2 (void), nat_rot_##k##_d3 (void), \
  nat_rot_##k##_d4 (void), nat_rot_##k##_d5 (void), \
  nat_rot_##k##_d8 (void), nat_rot_##k##_d9 (void), \
  nat_rot_##k##_d10 (void), nat_rot_##k##_d11 (void), \
  nat_rot_##k##_d12 (void);
long nat_rot_rws_wait (void);
void nat_bgr_rws (long *);
#ifdef NAT_V6
void nat_bgr_abgc (long *);
#endif
ROTS (DECL)

static sigjmp_buf env;
static int code;

static void
handler (int sig, siginfo_t *si, void *uc)
{
  (void) uc;
  code = si->si_code;
  siglongjmp (env, sig);
}

static long
best (long (*f) (long), long n, int reps)
{
  long b = -1;
  for (int i = 0; i < reps; i++)
    {
      long c = f (n);
      if (b < 0 || c < b)
        b = c;
    }
  return b;
}

static unsigned char buf[1 << 20] __attribute__ ((aligned (4096)));

static long
flush_cost (long size, int dirty)
{
  long b = -1;
  for (int i = 0; i < 20; i++)
    {
      if (dirty)
        memset (buf, i, size);
      long t0 = nat_clkr ();
      __builtin___clear_cache ((char *) buf, (char *) buf + size);
      long t1 = nat_clkr ();
      if (b < 0 || t1 - t0 < b)
        b = t1 - t0;
    }
  return b;
}

#ifdef NAT_V6
static volatile long word;
static long delay_ns;

/* Ping-pong between two threads on two cache lines, each side waiting
   for the other's store by spinning (watch 0) or by the v6 wait
   (watch 1).  */
static volatile long ping __attribute__ ((aligned (256)));
static volatile long pong __attribute__ ((aligned (256)));
static int pp_watch;
#define PP_N 20000

static void
pp_wait (volatile long *p, long old)
{
  while (*p == old)
    if (pp_watch)
      nat_watch ((long *) p, old);
}

static void *
pp_peer (void *arg)
{
  (void) arg;
  for (long i = 0; i < PP_N; i++)
    {
      pp_wait (&ping, i);
      pong = i + 1;
    }
  return NULL;
}

static void *
writer (void *arg)
{
  (void) arg;
  struct timespec ts = { delay_ns / 1000000000, delay_ns % 1000000000 };
  nanosleep (&ts, NULL);
  word = nat_clkr ();
  return NULL;
}
#endif

int
main (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = handler;
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;
  sigaction (SIGILL, &sa, NULL);
  sigaction (SIGSEGV, &sa, NULL);
  sigaction (SIGBUS, &sa, NULL);
  sigaction (SIGFPE, &sa, NULL);
  setvbuf (stdout, NULL, _IOLBF, 0);

  /* srp and crp in a loop (on v6 a region can end in the recovery
     point exception, SIGBUS).  */
  long n = 100000;
  static const char *const lname[] = { "plain", "with srp and crp",
                                       "with srp" };
  long (*const lf[]) (long) = { nat_loop, nat_srpcrp, nat_srp };
  printf ("loop of 5 instructions, cycles an iteration:");
  for (int i = 0; i < 3; i++)
    {
      int sig = sigsetjmp (env, 1);
      if (!sig)
        printf (" %s %.2f", lname[i], (double) best (lf[i], n, 7) / n);
      else
        printf (" %s signal %d code %d", lname[i], sig, code);
      printf (i < 2 ? "," : "\n");
    }

  /* libgcc's __clear_cache (MAS 0xf line flushes).  */
  printf ("__clear_cache, cycles (minimum of 20):\n");
  static const long sizes[] = { 256, 4096, 65536, 1 << 20 };
  for (int i = 0; i < 4; i++)
    {
      long clean = flush_cost (sizes[i], 0), dirty = flush_cost (sizes[i], 1);
      printf ("  %7ld bytes: clean %8ld (%.1f a line)  just written %8ld "
              "(%.1f a line)\n", sizes[i], clean,
              (double) clean / (sizes[i] / 256), dirty,
              (double) dirty / (sizes[i] / 256));
    }

  /* UPSR.fsm and FP compares.  */
  long upsr = -1;
  if (sigsetjmp (env, 1) == 0)
    upsr = nat_rd_upsr ();
  printf ("%%upsr at start %#lx\n", upsr);
  static const struct { const char *what; double a, b; } pairs[] = {
    { "1 < 2", 1, 2 }, { "2 > 1", 2, 1 }, { "1 == 1", 1, 1 },
    { "+0 vs -0", 0.0, -0.0 }, { "inf vs inf", __builtin_inf (),
                                 __builtin_inf () },
    { "qNaN vs 1", __builtin_nan (""), 1 }, { "1 vs qNaN", 1,
                                              __builtin_nan ("") },
    { "sNaN vs 1", __builtin_nans (""), 1 },
  };
  for (int fsm = 0; fsm < 2 && upsr >= 0; fsm++)
    {
      long u = fsm ? (upsr | 0x100) : (upsr & ~0x100L);
      if (sigsetjmp (env, 1))
        {
          printf ("  fsm %d: signal, code %d\n", fsm, code);
          continue;
        }
      nat_wr_upsr (u);
      long now = nat_rd_upsr ();
      printf ("  fsm %d (%%upsr %#lx): lt le eq neq uod | fcmpuddf "
              "fcmpoddf | flags\n", fsm, now);
      for (unsigned j = 0; j < sizeof pairs / sizeof pairs[0]; j++)
        {
          long ab[2];
          memcpy (&ab[0], &pairs[j].a, 8);
          memcpy (&ab[1], &pairs[j].b, 8);
          long r = -1;
          int sig = sigsetjmp (env, 1);
          if (!sig)
            r = nat_fcmp (ab, 0);
          if (sig)
            printf ("    %-11s signal %d code %d\n", pairs[j].what, sig,
                    code);
          else
            printf ("    %-11s %ld  %ld  %ld  %ld   %ld | %#04lx %#04lx | "
                    "%#04lx\n", pairs[j].what, r & 1, r >> 1 & 1,
                    r >> 2 & 1, r >> 3 & 1, r >> 4 & 1, r >> 8 & 0xff,
                    r >> 16 & 0xff, r >> 32 & 0x3f);
        }
    }
  if (upsr >= 0)
    nat_wr_upsr (upsr);

  /* %bgr rotation: what %g24 reads D cycles after it.  */
  printf ("rotation, then a read of %%g24 D cycles later "
          "(D 0..5, 8..12; 24 = old name, else new):\n");
#define ROW(k) \
  { \
    long (*f[11]) (void) = { nat_rot_##k##_d0, nat_rot_##k##_d1, \
                             nat_rot_##k##_d2, nat_rot_##k##_d3, \
                             nat_rot_##k##_d4, nat_rot_##k##_d5, \
                             nat_rot_##k##_d8, nat_rot_##k##_d9, \
                             nat_rot_##k##_d10, nat_rot_##k##_d11, \
                             nat_rot_##k##_d12 }; \
    printf ("  %-5s", #k); \
    for (int d = 0; d < 11; d++) \
      { \
        int sig = sigsetjmp (env, 1); \
        if (!sig) \
          printf (" %ld", f[d] ()); \
        else \
          printf (" sig%d", sig); \
      } \
    printf ("\n"); \
  }
  ROTS (ROW)
  if (sigsetjmp (env, 1) == 0)
    printf ("  rws %%bgr, wait all_e, then a read: %ld\n",
            nat_rot_rws_wait ());
  {
    long st[3];
    if (sigsetjmp (env, 1) == 0)
      {
        nat_bgr_rws (st);
        printf ("  after rws 0x1ff: %%bgr %#lx, %%g24 %ld, %%g25 %ld\n",
                st[0], st[1], st[2]);
      }
#ifdef NAT_V6
    if (sigsetjmp (env, 1) == 0)
      {
        nat_bgr_abgc (st);
        printf ("  after abgc 1: %%bgr %#lx, %%g24 %ld, %%g25 %ld\n",
                st[0], st[1], st[2]);
      }
#endif
  }

#ifdef NAT_V6
  /* The wait for a memory change, from user mode: the writer stores
     after a delay; the waiter waits until the word changes, counting
     the waits it took (each core has its own %clkr, so the wake-up
     latency comes from the ping-pong below).  */
  printf ("wait for a change of a watched word (v6):\n");
  static const long delays[] = { 1000000, 2000000, 20000000 };
  for (int i = 0; i < 3; i++)
    {
      pthread_t t;
      word = 0;
      delay_ns = delays[i];
      long t0 = nat_clkr (), waits = 0, tw = 0;
      pthread_create (&t, NULL, writer, NULL);
      int sig = sigsetjmp (env, 1);
      if (!sig)
        {
          while (word == 0)
            {
              nat_watch ((long *) &word, 0);
              waits++;
            }
          tw = nat_clkr ();
        }
      pthread_join (t, NULL);
      if (sig)
        printf ("  writer after %8ld ns: signal %d code %d\n", delays[i],
                sig, code);
      else
        printf ("  writer after %8ld ns: %ld waits, %.1f us in all\n",
                delays[i], waits, (tw - t0) / 2000.0);
    }
  word = 1;
  printf ("  word already changed: %ld cycles\n",
          nat_watch ((long *) &word, 0));
  for (pp_watch = 0; pp_watch < 2; pp_watch++)
    {
      pthread_t t;
      ping = pong = 0;
      pthread_create (&t, NULL, pp_peer, NULL);
      long t0 = nat_clkr ();
      for (long i = 0; i < PP_N; i++)
        {
          ping = i + 1;
          pp_wait (&pong, i);
        }
      long t1 = nat_clkr ();
      pthread_join (t, NULL);
      printf ("  ping-pong, both sides %s: %.0f cycles a round trip\n",
              pp_watch ? "waiting on the line" : "spinning",
              (double) (t1 - t0) / PP_N);
    }
#endif
  return 0;
}
