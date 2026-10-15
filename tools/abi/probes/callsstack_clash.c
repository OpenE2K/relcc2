/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stack-clash probing of the data stack (spec/abi/kernel-interface.md,
   "Overflow"; GCC's -fstack-clash-protection).  A frame larger than the
   guard below a stack, filled from its low end, writes the memory below
   the guard before any store reaches the guard (abileft.c part 3).  Do
   probes make a one-page guard enough?  callsstack_clash.s allocates a
   frame and touches it in three ways:

     one    one getsp of the whole frame, then a store every STEP bytes
            from the top down, then the fill from the bottom up;
     inc    getsp -STEP and a store at the new stack pointer, repeated
            (as GCC's x86 prologue probes), then the fill;
     spin   one getsp, no store, a register-only loop while a 100 us
            SIGALRM timer runs with its handler on the thread's own stack
            (the kernel writes each signal frame below the stack pointer).

   Cases (each in a forked child with an alarm; SIGSEGV and SIGBUS are
   caught on an alternate stack, which reports and exits):

     glibc thread, 64 KiB stack, default one-page guard: 96 KiB frames,
       one/4 KiB, one/8 KiB at two phases, inc/4 KiB, one/4 KiB 1 MiB;
     own stack (pthread_attr_setstack): 64 KiB, a 4 KiB PROT_NONE guard
       and 2 MiB of read-write memory filled with 0x5a below it: the
       same, 1 MiB frames, and spin;
     main thread: one/4 KiB 1 MiB (the stack grows); a 1 MiB mapping
       placed about 3 MiB below [stack] and one getsp whose bottom lies
       768 KiB below the mapping's end, then one/4 KiB; and inc/4 KiB
       towards the same mapping.

   For each: whether and where SIGSEGV arrived (si_code, si_addr against
   the guard's start; in the main thread, against [stack]'s start, or
   with the mapping, against the mapping's end), how far the probes and
   the fill got (st[]), and how many
   bytes changed in the memory below the guard (glibc stacks: the top
   1 MiB at most of the mapping that ends at the guard, against a copy
   taken just before; own stacks and the main-thread mapping: against
   0x5a).

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   # or v5.8c2, v6.2c3
     $L -O2 -Wl,-z,now tools/abi/probes/callsstack_clash.c \
        tools/abi/probes/callsstack_clash.s -o callsstack_clash -lpthread
     ./callsstack_clash [CASE]
     ./callsstack_clash cost

   cost: in the main thread of a fresh child, the time of a first and a
   second 64 KiB, 1 MiB and 4 MiB frame allocated by one (one getsp,
   probes every 4 KiB, fill) and by inc (getsp -4096 and a probe at a
   time, fill): the first grows [stack], the kernel stepping in at each
   getsp that leaves the part USD covers.

   -z now: a probe that misses the guard may write into ld.so's data, and
   the handler then must not need the lazy binder (abileft.c).  */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

long cl_one (long, long, long, long *);
long cl_inc (long, long, long, long *);
long cl_spin (long, long, long, long *);

static long st[8];
static unsigned long guard_lo, guard_hi;
static unsigned char *below_lo;
static size_t below_len;
static int below_vs_snap;
#define SNAP_MAX (1 << 20)
static unsigned char snap[SNAP_MAX];
static char altbuf[64 << 10];

enum kind { ONE, INC, SPIN };

struct ccase
{
  const char *name;
  int stack;  /* 0 glibc thread, 1 own stack, 2 main, 3 main + mapping */
  enum kind kind;
  long size, step, phase;
};

static const struct ccase cases[] = {
  { "glibc 64K, guard 4K: one 96K, probes 4K", 0, ONE, 96 << 10, 4096, 0 },
  { "glibc 64K, guard 4K: one 96K, probes 8K", 0, ONE, 96 << 10, 8192, 0 },
  { "glibc 64K, guard 4K: one 96K, probes 8K, phase 4K", 0, ONE, 96 << 10,
    8192, 4096 },
  { "glibc 64K, guard 4K: inc 96K, steps 4K", 0, INC, 96 << 10, 4096, 0 },
  { "glibc 64K, guard 4K: one 1M, probes 4K", 0, ONE, 1 << 20, 4096, 0 },
  { "own 64K, guard 4K, 2M rw below: one 96K, probes 4K", 1, ONE, 96 << 10,
    4096, 0 },
  { "own 64K, guard 4K, 2M rw below: one 1M, probes 4K", 1, ONE, 1 << 20,
    4096, 0 },
  { "own 64K, guard 4K, 2M rw below: one 1M, probes 8K", 1, ONE, 1 << 20,
    8192, 0 },
  { "own 64K, guard 4K, 2M rw below: one 1M, probes 8K, phase 4K", 1, ONE,
    1 << 20, 8192, 4096 },
  { "own 64K, guard 4K, 2M rw below: inc 1M, steps 4K", 1, INC, 1 << 20,
    4096, 0 },
  { "own 64K, guard 4K, 2M rw below: one 96K, no probes, timer", 1, SPIN,
    96 << 10, 0, 0 },
  { "main: one 1M, probes 4K", 2, ONE, 1 << 20, 4096, 0 },
  { "main, mapping 3M below [stack]: one getsp into it, probes 4K", 3, ONE,
    0, 4096, 0 },
  { "main, mapping 3M below [stack]: inc, steps 4K", 3, INC, 0, 4096, 0 },
};

static const struct ccase *cc;

static const char *
map_of (unsigned long addr)
{
  static char out[4][300];
  static int k;
  char *o = out[k++ & 3];
  unsigned long below = 0, above = ~0UL;
  snprintf (o, 300, "?");
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  int found = 0;
  while (f && fgets (line, sizeof line, f))
    {
      unsigned long lo, hi;
      sscanf (line, "%lx-%lx", &lo, &hi);
      line[strcspn (line, "\n")] = 0;
      if (addr >= lo && addr < hi)
        {
          snprintf (o, 300, "in %s", line);
          found = 1;
        }
      if (hi <= addr && hi > below)
        below = hi;
      if (lo > addr && lo < above)
        above = lo;
    }
  if (f)
    fclose (f);
  if (!found)
    snprintf (o, 300, "in a hole %#lx-%#lx", below, above);
  return o;
}

static long
changed (long *lowest, long *highest)
{
  long c = 0;
  *lowest = *highest = 0;
  if (!below_lo)
    return -1;
  for (size_t i = 0; i < below_len; i++)
    if (below_lo[i] != (below_vs_snap ? snap[i] : 0x5a))
      {
        if (!c)
          *lowest = (long) i - (long) below_len;
        *highest = (long) i - (long) below_len;
        c++;
      }
  return c;
}

static void
report (void)
{
  long lo, hi, c = changed (&lo, &hi);
  printf ("      st: bottom %+ld from the guard's start, probes %ld, probing "
          "%s, fill %s, last probe %+ld\n",
          st[0] ? (long) ((unsigned long) st[0] - guard_lo) : 0, st[1],
          st[2] ? "done" : "not done", st[3] ? "done" : "not done",
          st[4] ? (long) ((unsigned long) st[4] - guard_lo) : 0);
  if (c >= 0)
    printf ("      bytes changed in the %zu bytes below the guard: %ld "
            "(from %+ld to %+ld)\n", below_len, c, lo, hi);
}

static void
on_segv (int sig, siginfo_t *si, void *ctx)
{
  (void) ctx;
  unsigned long a = (unsigned long) si->si_addr;
  printf ("      SIGNAL %d si_code %d, si_addr %+ld from the guard's start "
          "(%s)\n", sig, si->si_code, (long) (a - guard_lo),
          a >= guard_lo && a < guard_hi ? "INSIDE the guard"
          : a < guard_lo ? "below the guard" : "above the guard");
  printf ("      si_addr %s\n", map_of (a));
  report ();
  fflush (stdout);
  _exit (0);
}

static volatile int nalrm;
static volatile unsigned long alrm_lo = ~0UL, alrm_hi;

static void
on_alrm (int sig)
{
  (void) sig;
  volatile char loc[16];
  loc[0] = 1;
  unsigned long a = (unsigned long) loc;
  if (a < alrm_lo)
    alrm_lo = a;
  if (a > alrm_hi)
    alrm_hi = a;
  nalrm++;
}

static void
handler (int sig, void (*f) (int, siginfo_t *, void *))
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = f;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigaction (sig, &sa, NULL);
}

static void
dive (void)
{
  long r = 0;
  memset (st, 0, sizeof st);
  if (below_vs_snap)
    memcpy (snap, below_lo, below_len);
  switch (cc->kind)
    {
    case ONE:
      r = cl_one (cc->size, cc->step, cc->phase, st);
      break;
    case INC:
      r = cl_inc (cc->size, cc->step, cc->phase, st);
      break;
    case SPIN:
      {
        struct sigaction sa;
        memset (&sa, 0, sizeof sa);
        sa.sa_handler = on_alrm;
        sigaction (SIGALRM, &sa, NULL);
        struct itimerval it = { { 0, 100 }, { 0, 100 } };
        setitimer (ITIMER_REAL, &it, NULL);
        r = cl_spin (cc->size, 2000000, cc->phase, st);
        struct itimerval z = { { 0, 0 }, { 0, 0 } };
        setitimer (ITIMER_REAL, &z, NULL);
        printf ("      %d SIGALRMs; the handler's locals from %+ld to %+ld "
                "from the guard's start\n", nalrm,
                (long) (alrm_lo - guard_lo), (long) (alrm_hi - guard_lo));
        break;
      }
    }
  printf ("      NO SIGSEGV: returned, bottom %+ld from the guard's start\n",
          (long) ((unsigned long) r - guard_lo));
  report ();
}

static void
watch_below_guard (void)
{
  /* The top (up to 1 MiB) of a writable mapping that ends at the guard.  */
  unsigned long lo = 0, hi = 0;
  char perm[8] = "";
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    {
      unsigned long a, b;
      char p[8];
      if (sscanf (line, "%lx-%lx %7s", &a, &b, p) == 3 && a < guard_lo
          && guard_lo <= b)
        {
          lo = a;
          hi = b;
          snprintf (perm, sizeof perm, "%s", p);
        }
    }
  if (f)
    fclose (f);
  if (hi == guard_lo && perm[0] == 'r' && perm[1] == 'w')
    {
      if (hi - lo > SNAP_MAX)
        lo = hi - SNAP_MAX;
      below_lo = (unsigned char *) lo;
      below_len = hi - lo;
      below_vs_snap = 1;
    }
}

static void *
thread (void *arg)
{
  (void) arg;
  sigset_t m;
  sigemptyset (&m);
  sigaddset (&m, SIGALRM);
  pthread_sigmask (SIG_UNBLOCK, &m, NULL);
  stack_t ss = { .ss_sp = altbuf, .ss_size = sizeof altbuf };
  sigaltstack (&ss, NULL);
  if (cc->stack == 0)
    {
      pthread_attr_t a;
      void *addr;
      size_t size, guard = 0;
      pthread_getattr_np (pthread_self (), &a);
      pthread_attr_getstack (&a, &addr, &size);
      pthread_attr_getguardsize (&a, &guard);
      guard_hi = (unsigned long) addr;
      guard_lo = guard_hi - guard;
      watch_below_guard ();
    }
  printf ("      guard %s\n      below it: %s\n", map_of (guard_lo),
          map_of (guard_lo - 1));
  fflush (stdout);
  dive ();
  return NULL;
}

static void
run_case (void)
{
  handler (SIGSEGV, on_segv);
  handler (SIGBUS, on_segv);
  if (cc->stack >= 2)
    {
      stack_t ss = { .ss_sp = altbuf, .ss_size = sizeof altbuf };
      sigaltstack (&ss, NULL);
      FILE *f = fopen ("/proc/self/maps", "r");
      char line[512];
      unsigned long slo = 0, shi = 0;
      while (f && fgets (line, sizeof line, f))
        if (strstr (line, "[stack]"))
          sscanf (line, "%lx-%lx", &slo, &shi);
      if (f)
        fclose (f);
      struct rlimit rl;
      getrlimit (RLIMIT_STACK, &rl);
      printf ("      [stack] %#lx-%#lx, RLIMIT_STACK %#lx\n", slo, shi,
              (unsigned long) rl.rlim_cur);
      /* No guard page: "guard" offsets are from [stack]'s start.  */
      guard_lo = guard_hi = slo;
      if (cc->stack == 3)
        {
          unsigned long want = (slo - (3 << 20)) & ~0xffffUL;
          below_len = 1 << 20;
          below_lo = mmap ((void *) (want - below_len), below_len,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                           -1, 0);
          if (below_lo == MAP_FAILED)
            {
              printf ("      mmap: %s\n", strerror (errno));
              return;
            }
          memset (below_lo, 0x5a, below_len);
          /* Offsets are reported from the mapping's end.  */
          guard_lo = guard_hi = want;
          unsigned long sp = (unsigned long) __builtin_frame_address (0);
          struct ccase *c = malloc (sizeof *c);
          *c = *cc;
          c->size = (long) (sp - (want - (768 << 10))) & ~0xfffL;
          cc = c;
          printf ("      a 1 MiB mapping at %p-%#lx; frame %#lx bytes, its "
                  "bottom 768 KiB below the mapping's end (offsets below are "
                  "from the mapping's end)\n", (void *) below_lo, want,
                  (unsigned long) cc->size);
        }
      fflush (stdout);
      dive ();
      return;
    }
  pthread_attr_t a;
  pthread_attr_init (&a);
  if (cc->stack == 0)
    pthread_attr_setstacksize (&a, 64 << 10);
  else
    {
      size_t len = (2 << 20) + (4 << 10) + (64 << 10);
      unsigned char *m = mmap (NULL, len, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      memset (m, 0x5a, len);
      unsigned char *stk = m + (2 << 20) + (4 << 10);
      mprotect (stk - 4096, 4096, PROT_NONE);
      guard_lo = (unsigned long) stk - 4096;
      guard_hi = (unsigned long) stk;
      below_lo = m;
      below_len = 2 << 20;
      below_vs_snap = 0;
      pthread_attr_setstack (&a, stk, 64 << 10);
    }
  /* Only the new thread takes SIGALRM.  */
  sigset_t m;
  sigemptyset (&m);
  sigaddset (&m, SIGALRM);
  pthread_sigmask (SIG_BLOCK, &m, NULL);
  pthread_t t;
  int r = pthread_create (&t, &a, thread, NULL);
  if (r)
    {
      printf ("      pthread_create: %s\n", strerror (r));
      return;
    }
  pthread_join (t, NULL);
}

static double
now_us (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

static void
cost (void)
{
  static const long sizes[] = { 64 << 10, 1 << 20, 4 << 20 };
  for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++)
    for (int k = 0; k < 2; k++)
      {
        fflush (stdout);
        pid_t pid = fork ();
        if (pid == 0)
          {
            double t0 = now_us ();
            (k ? cl_inc : cl_one) (sizes[i], 4096, 0, st);
            double t1 = now_us ();
            (k ? cl_inc : cl_one) (sizes[i], 4096, 0, st);
            double t2 = now_us ();
            printf ("cost: main thread, %4ld KiB, %s: first %9.1f us, "
                    "second %9.1f us\n", sizes[i] >> 10,
                    k ? "inc (getsp per 4 KiB)  " : "one getsp, probes 4 KiB",
                    t1 - t0, t2 - t1);
            fflush (stdout);
            _exit (0);
          }
        int s;
        waitpid (pid, &s, 0);
      }
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  if (argc > 1 && strcmp (argv[1], "cost") == 0)
    {
      cost ();
      return 0;
    }
  int only = argc > 1 ? atoi (argv[1]) : -1;
  for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
      if (only >= 0 && (unsigned) only != i)
        continue;
      cc = &cases[i];
      printf ("%u. %s\n", i, cc->name);
      fflush (stdout);
      pid_t pid = fork ();
      if (pid == 0)
        {
          alarm (60);
          run_case ();
          fflush (stdout);
          _exit (0);
        }
      int s;
      waitpid (pid, &s, 0);
      if (WIFSIGNALED (s))
        printf ("      child killed by signal %d\n", WTERMSIG (s));
    }
  return 0;
}
