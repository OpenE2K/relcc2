/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What makes the wait-unlock store of lcc's atomic read-modify-write fail
   (spec/abi/atomics.md): the lock-wait load (mas 0x7), something in
   between, the wait-unlock store (mas 0x2) and rbranch.  Kernels from
   gen_lockloss.py (lockloss.s, lockloss-v5.s, lockloss-cases.h).

     python3 gen_lockloss.py
     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc    # or v5.8c2, v6.2c3
     $L -O3 lockloss.c lockloss.s -o lockloss -lpthread
     $L -O3 -DLL_V5 lockloss.c lockloss.s lockloss-v5.s -o lockloss -lpthread
     ./lockloss CPU_A CPU_B [CPU_B ...]          # on hardware
     ./lockloss hold CPU_A CPU_B                 # part 3 only

   Part 1 runs each kernel in one thread and counts failed stores per
   million attempts; part 2 runs a second thread on CPU_B against a
   fetch-add on CPU_A; part 3 finds how long a lock survives another
   core's loads and stores.  */

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LL_CASE(name, what) long ll_##name (long *, long *, long);
#include "lockloss-cases.h"
#undef LL_CASE

static const struct
{
  const char *name, *what;
  long (*fn) (long *, long *, long);
} cases[] = {
#define LL_CASE(name, what) { #name, what, ll_##name },
#include "lockloss-cases.h"
#undef LL_CASE
};

long ll_clkr (void);
long llb_load (long *, volatile long *);
long llb_store (long *, volatile long *, long);
long llb_rmw_0 (long *, volatile long *);
long llb_rmw_64 (long *, volatile long *);
long llb_rmw_512 (long *, volatile long *);
long llb_try_0 (long *, volatile long *);
long llb_try_64 (long *, volatile long *);
long llb_try_512 (long *, volatile long *);
long llb_try_1024 (long *, volatile long *);
long llb_try_1536 (long *, volatile long *);
long llb_try_2048 (long *, volatile long *);
long llb_try_3072 (long *, volatile long *);
long llb_try_4096 (long *, volatile long *);
long llb_maxlat (long *, volatile long *);
long llb_abandon_4096 (long *, volatile long *);
long llb_idle_4096 (long *, volatile long *);

/* Called between the lock and the unlock by ll_syscall.  */
void
ll_sys (void)
{
  syscall (SYS_getppid);
}

static long *mem;          /* the locked word at mem[0]; 1 MiB */
static long *aux;          /* another page */
static volatile long *stop;

static void
pin (int cpu)
{
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (cpu, &s);
  sched_setaffinity (0, sizeof s, &s);
}

/* Run FN in a child with a time limit.  */
static void
in_child (const char *what, void (*fn) (void *), void *arg)
{
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      alarm (20);
      fn (arg);
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("  %-14s [killed by signal %d]\n", what, WTERMSIG (st));
  else if (WEXITSTATUS (st))
    printf ("  %-14s [exit %d]\n", what, WEXITSTATUS (st));
  fflush (stdout);
}

static long
attempts (const char *name)
{
  if (!strcmp (name, "delay32768"))
    return 20000;
  if (!strcmp (name, "delay4096"))
    return 100000;
  if (!strcmp (name, "syscall"))
    return 200000;
  return 1000000;
}

/* Part 1: one kernel, one thread.  */
static void
run_case (void *arg)
{
  int i = (int) (long) arg;
  long n = attempts (cases[i].name);
  memset (mem, 0, 1 << 20);
  memset (aux, 0, 4096);
  long t0 = ll_clkr ();
  long f = cases[i].fn (mem, aux, n);
  long t1 = ll_clkr ();
  printf ("  %-14s %8ld fails per million (%ld of %ld); counter +%ld "
          "(%s); %.1f cycles each; p[1] %#lx, aux[0] %#lx: %s\n",
          cases[i].name, (long) (f * 1e6 / n), f, n, mem[0],
          mem[0] == n - f ? "= successes" : "!= successes",
          (double) (t1 - t0) / n, (unsigned long) mem[1],
          (unsigned long) aux[0], cases[i].what);
}

/* A page fault between the lock and the unlock: AUX's page is dropped
   before each attempt.  */
static void
fault_case (void *arg)
{
  long (*fn) (long *, long *, long) = (long (*) (long *, long *, long)) arg;
  for (int drop = 0; drop < 2; drop++)
    {
      long f = 0, n = 20000;
      memset (mem, 0, 64);
      for (long k = 0; k < n; k++)
        {
          if (drop)
            madvise (aux, 4096, MADV_DONTNEED);
          f += fn (mem, aux, 1);
        }
      printf ("    %s: %ld fails of %ld\n",
              drop ? "page dropped before each attempt (a fault between)"
              : "page present", f, n);
    }
}

/* Part 2: a second thread.  */
enum bkind { B_NONE, B_LOAD, B_STORE, B_RMW0, B_RMW512, B_TRY0, B_TRY512,
             B_LCC, B_TRY4096, B_ABANDON, B_IDLE };
static const char *bnames[] = { "nothing", "plain loads", "plain stores",
                                "fetch-add", "fetch-add, 512-cycle hold",
                                "fetch-add tries", "tries, 512-cycle hold",
                                "lcc's fetch-add loop",
                                "tries, 4096-cycle hold",
                                "lock, 4096 cycles, no store",
                                "4096 cycles, no lock" };

/* lcc's own code for comparison.  */
static long __attribute__ ((noinline))
lcc_fadd_loop (long *p, volatile long *s)
{
  long n = 0;
  while (!*s)
    {
      for (int i = 0; i < 16; i++)
        __atomic_fetch_add (p, 1, __ATOMIC_RELAXED);
      n += 16;
    }
  return n;
}

struct bthread
{
  int cpu, kind;
  long *addr;
  long result;
  volatile int ready, go;
};

static long
run_b (int kind, long *a)
{
  switch (kind)
    {
    case B_LOAD: return llb_load (a, stop);
    case B_STORE: return llb_store (a, stop, 0x55);
    case B_RMW0: return llb_rmw_0 (a, stop);
    case B_RMW512: return llb_rmw_512 (a, stop);
    case B_TRY0: return llb_try_0 (a, stop);
    case B_TRY512: return llb_try_512 (a, stop);
    case B_LCC: return lcc_fadd_loop (a, stop);
    case B_TRY4096: return llb_try_4096 (a, stop);
    case B_ABANDON: return llb_abandon_4096 (a, stop);
    case B_IDLE: return llb_idle_4096 (a, stop);
    default: while (!*stop); return 0;
    }
}

static void *
bmain (void *arg)
{
  struct bthread *b = arg;
  pin (b->cpu);
  b->ready = 1;
  while (!b->go)
    ;
  b->result = run_b (b->kind, b->addr);
  return NULL;
}

static int cpu_a, cpu_b;

static pthread_t
start_b (struct bthread *b)
{
  pthread_t t;
  sigset_t s, old;
  sigfillset (&s);
  pthread_sigmask (SIG_BLOCK, &s, &old);
  pthread_create (&t, NULL, bmain, b);
  pthread_sigmask (SIG_SETMASK, &old, NULL);
  while (!b->ready)
    ;
  return t;
}

static const char *
pair (long r)
{
  static char buf[2][64];
  static int k;
  k ^= 1;
  snprintf (buf[k], sizeof buf[k], "%ld ok, %ld failed", r & 0xffffffff,
            r >> 32);
  return buf[k];
}

/* A: ll_nothing, n attempts, while B does KIND at OFF bytes from the word.  */
struct contend { int kind; long off; };

static void
contend_case (void *arg)
{
  struct contend *c = arg;
  memset (mem, 0, 1 << 20);
  *stop = 0;
  struct bthread b = { cpu_b, c->kind, (long *) ((char *) mem + c->off), 0,
                       0, 0 };
  pin (cpu_a);
  pthread_t t = start_b (&b);
  b.go = 1;
  long n = 200000;
  long t0 = ll_clkr ();
  long f = ll_nothing (mem, aux, n);
  long t1 = ll_clkr ();
  *stop = 1;
  pthread_join (t, NULL);
  printf ("  B: %-26s at +%-4ld A: %8ld fails per million, %6.1f cycles "
          "an attempt; B: %s\n", bnames[c->kind], c->off,
          (long) (f * 1e6 / n), (double) (t1 - t0) / n,
          c->kind >= B_RMW0 ? pair (b.result) : "-");
  if (c->kind < B_RMW0 && c->kind != B_NONE)
    printf ("      (B made %ld accesses)\n", b.result);
}

/* Both threads run a fetch-add loop for 0.2 s.  */
static void
on_alarm (int sig)
{
  (void) sig;
  *stop = 1;
}

struct duel { int a_kind, b_kind; long off; int lcc; };

static long __attribute__ ((noinline))
lcc_cas_incr (long *p, long n, long *fails)
{
  long f = 0;
  for (long i = 0; i < n; i++)
    {
      long old = __atomic_load_n (p, __ATOMIC_RELAXED);
      while (!__atomic_compare_exchange_n (p, &old, old + 1, 0,
                                           __ATOMIC_RELAXED,
                                           __ATOMIC_RELAXED))
        f++;
    }
  *fails = f;
  return n;
}

static void
duel_case (void *arg)
{
  struct duel *d = arg;
  memset (mem, 0, 1 << 20);
  *stop = 0;
  long *pb = (long *) ((char *) mem + d->off);
  struct bthread b = { cpu_b, d->b_kind, pb, 0, 0, 0 };
  pin (cpu_a);
  signal (SIGALRM, on_alarm);
  if (d->lcc)
    b.kind = B_LCC;
  pthread_t t = start_b (&b);
  struct itimerval it = { { 0, 0 }, { 0, 200000 } };
  long t0 = ll_clkr ();
  setitimer (ITIMER_REAL, &it, NULL);
  b.go = 1;
  long ra;
  if (d->lcc)
    ra = lcc_fadd_loop (mem, stop);
  else
    ra = run_b (d->a_kind, mem);
  long t1 = ll_clkr ();
  pthread_join (t, NULL);
  if (d->lcc)
    printf ("  lcc's fetch-add loop on both, B at +%ld: A %ld, B %ld "
            "increments, word %ld; %.1f cycles per increment (A)\n", d->off,
            ra, b.result, mem[0], (double) (t1 - t0) / ra);
  else if (d->b_kind == B_LOAD || d->b_kind == B_STORE)
    printf ("  A: %s, B: %s at +%ld: A %ld rounds (%.0f cycles each); B "
            "%ld accesses in %.0f cycles (%.1f cycles each)\n",
            bnames[d->a_kind], bnames[d->b_kind], d->off, ra & 0xffffffff,
            (double) (t1 - t0) / ((ra & 0xffffffff) ? (ra & 0xffffffff) : 1),
            b.result, (double) (t1 - t0), (double) (t1 - t0) / b.result);
  else
    printf ("  A: %s, B: %s at +%ld: A %s; B %s; word %ld; %.1f cycles per "
            "A success\n", bnames[d->a_kind], bnames[d->b_kind], d->off,
            pair (ra), pair (b.result), mem[0],
            (double) (t1 - t0) / ((ra & 0xffffffff) ? (ra & 0xffffffff) : 1));
}

/* How long may a lock be held against another core's loads or stores?
   A tries a fetch-add with HOLD cycles between the lock and the unlock
   for 0.2 s; B loads (recording its longest load) or stores.  */
static long (*const holds[]) (long *, volatile long *) = {
  llb_try_0, llb_try_512, llb_try_1024, llb_try_1536, llb_try_2048,
  llb_try_3072, llb_try_4096,
};
static const int hold_cycles[] = { 0, 512, 1024, 1536, 2048, 3072, 4096 };

struct holdc { int h; int kind; };

static long
run_b2 (int kind, long *a)
{
  return kind == B_LOAD ? llb_maxlat (a, stop) : run_b (kind, a);
}

static void *
bmain2 (void *arg)
{
  struct bthread *b = arg;
  pin (b->cpu);
  b->ready = 1;
  while (!b->go)
    ;
  b->result = run_b2 (b->kind, b->addr);
  return NULL;
}

static void
hold_case (void *arg)
{
  struct holdc *c = arg;
  memset (mem, 0, 1 << 20);
  *stop = 0;
  struct bthread b = { cpu_b, c->kind, mem, 0, 0, 0 };
  pin (cpu_a);
  signal (SIGALRM, on_alarm);
  sigset_t s, old;
  sigfillset (&s);
  pthread_sigmask (SIG_BLOCK, &s, &old);
  pthread_t t;
  pthread_create (&t, NULL, bmain2, &b);
  pthread_sigmask (SIG_SETMASK, &old, NULL);
  while (!b.ready)
    ;
  struct itimerval it = { { 0, 0 }, { 0, 200000 } };
  setitimer (ITIMER_REAL, &it, NULL);
  b.go = 1;
  long ra = c->h < 0 ? (llb_idle_4096 (mem, stop), 0)
            : holds[c->h] (mem, stop);
  pthread_join (t, NULL);
  long ok = ra & 0xffffffff, bad = ra >> 32;
  printf ("  A holds %4d cycles, B %s: A %ld ok, %ld failed (%.1f%% ok)",
          c->h < 0 ? -1 : hold_cycles[c->h],
          c->kind == B_LOAD ? "loads" : "stores", ok, bad,
          ok + bad ? 100.0 * ok / (ok + bad) : 0.0);
  if (c->kind == B_LOAD)
    printf ("; B's longest load %ld cycles", b.result);
  printf ("\n");
}

/* A: lcc's CAS increment loop (100,000 increments) against B's plain
   stores to the same word (a relaxed atomic store) or to the line.  */
static void
starve_case (void *arg)
{
  struct contend *c = arg;
  memset (mem, 0, 1 << 20);
  *stop = 0;
  struct bthread b = { cpu_b, c->kind, (long *) ((char *) mem + c->off), 0,
                       0, 0 };
  pin (cpu_a);
  pthread_t t = start_b (&b);
  b.go = 1;
  long fails;
  long t0 = ll_clkr ();
  long n = lcc_cas_incr (mem, 100000, &fails);
  long t1 = ll_clkr ();
  *stop = 1;
  pthread_join (t, NULL);
  printf ("  lcc's CAS loop, B: %s at +%ld: %ld increments, %ld CAS "
          "failures, %.1f cycles per increment\n", bnames[c->kind], c->off,
          n, fails, (double) (t1 - t0) / n);
}

static long *
fresh (size_t size)
{
  return mmap (NULL, size, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  mem = fresh (1 << 20);
  aux = fresh (4096);
  stop = mmap (NULL, 4096, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  int hold_only = argc > 1 && !strcmp (argv[1], "hold");
  if (hold_only)
    argc--, argv++;
  cpu_a = argc > 1 ? atoi (argv[1]) : 0;
  pin (cpu_a);
  if (hold_only)
    {
      for (int k = 2; k < argc; k++)
        {
          cpu_b = atoi (argv[k]);
          printf ("3. lock hold time against CPU %d (A on CPU %d):\n", cpu_b,
                  cpu_a);
          for (int kind = B_LOAD; kind <= B_STORE; kind++)
            for (int h = -1; h < 7; h++)
              {
                struct holdc c = { h, kind };
                in_child ("hold", hold_case, &c);
              }
        }
      return 0;
    }

  printf ("1. one thread (CPU %d): lock-wait ldd,0, X, wait-unlock std,2 "
          "with rbranch, each attempt once:\n", cpu_a);
  for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    in_child (cases[i].name, run_case, (void *) (long) i);
  printf ("  page faults between (20,000 attempts):\n");
  printf ("   ldd,0 of the page:\n");
  in_child ("fault ld", fault_case, (void *) ll_ld0_other);
  printf ("   std,2 to the page:\n");
  in_child ("fault st", fault_case, (void *) ll_st_other);
  printf ("  10,000,000 attempts with nothing between (interrupts):\n");
  {
    memset (mem, 0, 64);
    long t0 = ll_clkr ();
    long f = ll_nothing (mem, aux, 10000000);
    long t1 = ll_clkr ();
    printf ("    %ld fails in %ld cycles\n", f, t1 - t0);
  }

  for (int k = 2; k < argc; k++)
    {
      cpu_b = atoi (argv[k]);
      printf ("2. second thread on CPU %d, A on CPU %d:\n", cpu_b, cpu_a);
      static const struct contend cs[] = {
        { B_NONE, 0 },
        { B_LOAD, 0 }, { B_LOAD, 8 }, { B_LOAD, 32 }, { B_LOAD, 64 },
        { B_STORE, 0 }, { B_STORE, 8 }, { B_STORE, 16 }, { B_STORE, 32 },
        { B_STORE, 48 }, { B_STORE, 64 }, { B_STORE, 128 },
        { B_STORE, 4096 },
        { B_RMW0, 0 }, { B_RMW0, 8 }, { B_RMW0, 64 },
        { B_TRY512, 0 }, { B_RMW512, 0 },
      };
      for (unsigned i = 0; i < sizeof cs / sizeof cs[0]; i++)
        in_child ("contend", contend_case, (void *) &cs[i]);
      static const struct duel ds[] = {
        { B_RMW0, B_RMW0, 0, 0 }, { B_RMW0, B_RMW0, 8, 0 },
        { B_RMW0, B_RMW0, 32, 0 }, { B_RMW0, B_RMW0, 64, 0 },
        { B_RMW0, B_RMW0, 4096, 0 },
        { B_TRY0, B_TRY0, 0, 0 }, { B_TRY0, B_TRY512, 0, 0 },
        { B_TRY512, B_TRY0, 0, 0 }, { B_RMW0, B_RMW512, 0, 0 },
        { 0, 0, 0, 1 }, { 0, 0, 64, 1 },
        /* Does a held or abandoned lock make another core wait?  */
        { B_IDLE, B_LOAD, 0, 0 }, { B_TRY4096, B_LOAD, 0, 0 },
        { B_ABANDON, B_LOAD, 0, 0 }, { B_ABANDON, B_LOAD, 64, 0 },
        { B_IDLE, B_STORE, 0, 0 }, { B_TRY4096, B_STORE, 0, 0 },
        { B_ABANDON, B_STORE, 0, 0 },
        { B_ABANDON, B_RMW0, 0, 0 }, { B_TRY4096, B_RMW0, 0, 0 },
      };
      for (unsigned i = 0; i < sizeof ds / sizeof ds[0]; i++)
        in_child ("duel", duel_case, (void *) &ds[i]);
      static const struct contend ss[] = {
        { B_NONE, 0 }, { B_STORE, 0 }, { B_STORE, 8 }, { B_LOAD, 0 },
        { B_RMW0, 0 },
      };
      for (unsigned i = 0; i < sizeof ss / sizeof ss[0]; i++)
        in_child ("starve", starve_case, (void *) &ss[i]);
    }
  return 0;
}
