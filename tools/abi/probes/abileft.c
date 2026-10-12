/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Leftovers of spec/abi/kernel-interface.md ("Not checked yet"), with the
   documented calls only (getcontext, setcontext, swapcontext,
   makecontext_e2k, freecontext_e2k, sigaction, sigaltstack, setitimer,
   pthread_*):

   1. Signals while a makecontext_e2k context runs: which data and
      hardware stacks the handler runs on, with and without SA_ONSTACK,
      whether returning resumes the context; a signal arriving when the
      context's stack has less free space than a signal frame; a timer
      signal during a swapcontext loop (which side it interrupts, what
      swapcontext returns, each context's FP rounding and signal mask).
   2. Switching contexts from inside a handler: swapcontext to a made
      context and back (with a nested signal in that context),
      setcontext to a getcontext point (raise and timer, many times),
      setcontext to a made context whose uc_link is a getcontext point,
      and a timer handler that schedules three contexts round robin.
      The signal mask at each point and the kernel's [signal stack]
      mappings.
   3. Data stack overflow in a thread (glibc's stacks and user stacks with
      mapped, PROT_NONE and unmapped memory below; small and large
      frames) and in the main thread.  A SIGSEGV handler on an alternate
      stack reports what arrived and how many bytes below the stack
      changed (for glibc's stacks: against a copy of the mapping below
      the guard taken just before; the cases also say where ld.so's
      link maps are).
   Every case runs in a forked child with an alarm.  `./abileft 3 N` runs
   only thread case N of part 3; ABILEFT_MAPS=1 also prints
   /proc/self/maps in part 3's threads.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc  # or v5.8c2, v6.2c3
     $L -O2 -rdynamic abileft.c abileft.s -o abileft -lpthread -lm
     ./abileft [1|2|3 [CASE]]                            # on hardware  */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fenv.h>
#include <link.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#include <asm/e2k_syswork.h>

long al_usd_lo (void), al_usd_hi (void), al_sbr (void);
long al_psp_lo (void), al_pcsp_lo (void);

#define M48 ((1UL << 48) - 1)
#define VA(x) ((unsigned long) (x) & M48)

/* Where the code that calls it runs.  */
struct where
{
  unsigned long sp, bottom, sbr, psp, pcsp;
  long pss, pcss;
};

static void __attribute__ ((noinline))
where_now (struct where *w)
{
  unsigned long lo = al_usd_lo (), hi = al_usd_hi ();
  w->sp = VA (lo);
  w->bottom = w->sp - (hi >> 32);
  w->sbr = VA (al_sbr ());
  w->psp = VA (al_psp_lo ());
  w->pcsp = VA (al_pcsp_lo ());
  w->pss = w->pcss = -1;
  syscall (__NR_access_hw_stacks, E2K_GET_PROCEDURE_STACK_SIZE, NULL, NULL,
           0, &w->pss);
  syscall (__NR_access_hw_stacks, E2K_GET_CHAIN_STACK_SIZE, NULL, NULL, 0,
           &w->pcss);
}

static void
print_where (const char *who, const struct where *w)
{
  printf ("    %-22s usd.ptr %#lx bottom %#lx sbr %#lx; psp.base %#lx "
          "(size %ld) pcsp.base %#lx (size %ld)\n", who, w->sp, w->bottom,
          w->sbr, w->psp, w->pss, w->pcsp, w->pcss);
}

/* The [signal stack] mappings, as "N KiB" each.  */
static const char *
sigstacks (void)
{
  static char out[8][256];
  static int k;
  char *o = out[k++ & 7];
  int n = 0, cnt = 0;
  o[0] = 0;
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    if (strstr (line, "[signal stack]") && n < 200)
      {
        unsigned long lo, hi;
        sscanf (line, "%lx-%lx", &lo, &hi);
        n += snprintf (o + n, 256 - n, "%s%lu KiB", cnt++ ? ", " : "",
                       (hi - lo) >> 10);
      }
  if (f)
    fclose (f);
  if (!cnt)
    snprintf (o, 256, "none");
  return o;
}

/* The /proc/self/maps line holding ADDR, or the gap it lies in.  */
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
    snprintf (o, 300, "unmapped (gap %#lx-%#lx)", below, above);
  return o;
}

static const char *
mask_str (const sigset_t *m)
{
  static char out[8][96];
  static int k;
  char *o = out[k++ & 7];
  int n = 0;
  static const int sigs[] = { SIGUSR1, SIGUSR2, SIGALRM, SIGTERM, SIGHUP };
  static const char *const names[] = { "USR1", "USR2", "ALRM", "TERM",
                                       "HUP" };
  o[0] = 0;
  for (int i = 0; i < 5; i++)
    if (sigismember (m, sigs[i]))
      n += snprintf (o + n, 96 - n, "%s%s", n ? "+" : "", names[i]);
  if (!n)
    snprintf (o, 96, "none");
  return o;
}

static void
cur_mask (sigset_t *m)
{
  sigemptyset (m);
  pthread_sigmask (SIG_BLOCK, NULL, m);
}

static const char *
sym (unsigned long ip)
{
  static char buf[8][128];
  static int k;
  char *b = buf[k++ & 7];
  Dl_info di;
  if (dladdr ((void *) ip, &di) && di.dli_sname)
    snprintf (b, 128, "%s", di.dli_sname);
  else if (dladdr ((void *) ip, &di) && di.dli_fname)
    snprintf (b, 128, "%s+%#lx", strrchr (di.dli_fname, '/')
              ? strrchr (di.dli_fname, '/') + 1 : di.dli_fname,
              ip - (unsigned long) di.dli_fbase);
  else
    snprintf (b, 128, "%#lx", ip);
  return b;
}

static void
in_child (const char *what, void (*fn) (void), int secs)
{
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      struct rlimit no = { 0, 0 };
      setrlimit (RLIMIT_CORE, &no);
      alarm (secs);
      fn ();
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("    [%s: child killed by signal %d (%s)]\n", what, WTERMSIG (st),
            strsignal (WTERMSIG (st)));
  else
    printf ("    [%s: child exit %d]\n", what, WEXITSTATUS (st));
  fflush (stdout);
}

static void
handler (int sig, void (*fn) (int, siginfo_t *, void *), int flags)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = fn;
  sa.sa_flags = SA_SIGINFO | flags;
  sigemptyset (&sa.sa_mask);
  sigaction (sig, &sa, NULL);
}

static void
timer_us (long first, long interval)
{
  struct itimerval it = { { 0, interval }, { 0, first } };
  setitimer (ITIMER_REAL, &it, NULL);
}

#define STK (64 << 10)
static char stk_a[STK] __attribute__ ((aligned (4096)));
static ucontext_t uc_main, uc_a, uc_a2, uc_h, uc_g;

static void
setup (ucontext_t *uc, void *stk, size_t sz, ucontext_t *link)
{
  memset (uc, 0, sizeof *uc);
  uc->uc_stack.ss_sp = stk;
  uc->uc_stack.ss_size = sz;
  uc->uc_link = link;
}

static int
in_range (unsigned long a, const void *lo, size_t sz)
{
  return a >= (unsigned long) lo && a < (unsigned long) lo + sz;
}

/* The work loop of a context: values kept in registers across whatever
   interrupts it, checked against a recomputation.  */
static int
check_work (long n, long a, long b)
{
  long ea = 1, eb = 7;
  for (long i = 1; i <= n; i++)
    {
      ea = ea * 3 + 1;
      eb += ea ^ i;
    }
  return ea == a && eb == b;
}

/* 1.  Signals in a made context.  */
static volatile int stop_flag;
static struct where w_main, w_ctx, w_hand;
static unsigned long h_isp, h_ip, h_local;
static int h_onalt, h_n;
static char altbuf[64 << 10] __attribute__ ((aligned (4096)));

static void
h_where (int sig, siginfo_t *si, void *ucv)
{
  (void) sig, (void) si;
  ucontext_t *uc = ucv;
  volatile char loc = 0;
  where_now (&w_hand);
  h_local = (unsigned long) &loc;
  h_isp = VA (uc->uc_mcontext.usd_lo);
  h_ip = uc->uc_mcontext.cr0_hi & M48 & ~7UL;
  stack_t ss;
  sigaltstack (NULL, &ss);
  h_onalt = !!(ss.ss_flags & SS_ONSTACK);
  h_n++;
  stop_flag = 1;
}

static long spin_n;
static int spin_ok;

static void
spin_fn (void)
{
  long n = 0, a = 1, b = 7;
  where_now (&w_ctx);
  while (!stop_flag)
    {
      n++;
      a = a * 3 + 1;
      b += a ^ n;
    }
  spin_n = n;
  spin_ok = check_work (n, a, b);
}

static void
sig_in_ctx (int onstack)
{
  if (onstack)
    {
      stack_t ss = { .ss_sp = altbuf, .ss_size = sizeof altbuf };
      sigaltstack (&ss, NULL);
    }
  handler (SIGALRM, h_where, onstack ? SA_ONSTACK : 0);
  where_now (&w_main);
  setup (&uc_a, stk_a, STK, &uc_main);
  long r = makecontext_e2k (&uc_a, spin_fn, 0);
  stop_flag = 0;
  timer_us (20000, 0);
  long s = swapcontext (&uc_main, &uc_a);
  printf ("    %s: makecontext_e2k %ld, swapcontext %ld; uc_stack %p-%p,"
          " alternate stack %p-%p\n", onstack ? "SA_ONSTACK" : "no SA_ONSTACK",
          r, s, (void *) stk_a, (void *) (stk_a + STK), (void *) altbuf,
          (void *) (altbuf + sizeof altbuf));
  print_where ("main:", &w_main);
  print_where ("context:", &w_ctx);
  print_where ("handler:", &w_hand);
  printf ("    handler ran %d time(s); its local %#lx is %s; interrupted "
          "usd.ptr %#lx is %s; interrupted IP in %s; SS_ONSTACK %d\n", h_n,
          h_local,
          in_range (h_local, stk_a, STK) ? "in uc_stack"
          : in_range (h_local, altbuf, sizeof altbuf) ? "in the alternate stack"
          : "elsewhere", h_isp,
          in_range (h_isp, stk_a, STK) ? "in uc_stack" : "elsewhere",
          sym (h_ip), h_onalt);
  printf ("    handler local - interrupted usd.ptr = %ld; handler's hardware"
          " stacks: psp.base %s the context's, pcsp.base %s the context's; "
          "procedure stack %+ld, chain stack %+ld bytes from the context's"
          " sizes\n", (long) (h_local - h_isp),
          w_hand.psp == w_ctx.psp ? "=" : "!=",
          w_hand.pcsp == w_ctx.pcsp ? "=" : "!=", w_hand.pss - w_ctx.pss,
          w_hand.pcss - w_ctx.pcss);
  printf ("    the context resumed after the handler: loop of %ld iterations"
          " %s, returned through uc_link to main\n", spin_n,
          spin_ok ? "intact" : "CORRUPTED");
}

static void sig_in_ctx0 (void) { sig_in_ctx (0); }
static void sig_in_ctx1 (void) { sig_in_ctx (1); }

/* A signal frame that does not fit below the context's stack pointer.
   uc_stack (16 KiB) lies in the middle of a 1 MiB mapping filled with
   0x5a; the function leaves FREE bytes above the bottom and spins.  */
#define LOW_AREA (1 << 20)
static unsigned char *low_area;
static long low_free;
static struct where w_low;

static void __attribute__ ((noinline))
low_fn (void)
{
  unsigned long fr = al_usd_hi () >> 32;
  long take = (long) fr - low_free;
  volatile char *p = __builtin_alloca (take > 0 ? take : 16);
  p[0] = 1;
  unsigned long lo = al_usd_lo (), hi = al_usd_hi ();
  w_low.sp = VA (lo);
  w_low.bottom = VA (lo) - (hi >> 32);
  while (!stop_flag)
    ;
  p[0] = 2;
}

static void
low_case (int kind)
{
  static const char *const names[] = { "mapped memory below",
                                       "512 KiB unmapped below",
                                       "mapped below, SA_ONSTACK" };
  low_area = mmap (NULL, LOW_AREA, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memset (low_area, 0x5a, LOW_AREA);
  unsigned char *base = low_area + LOW_AREA / 2;
  if (kind == 1)
    munmap (low_area, LOW_AREA / 2);
  if (kind == 2)
    {
      stack_t ss = { .ss_sp = altbuf, .ss_size = sizeof altbuf };
      sigaltstack (&ss, NULL);
    }
  handler (SIGALRM, h_where, kind == 2 ? SA_ONSTACK : 0);
  low_free = 1024;
  setup (&uc_a, base, 16 << 10, &uc_main);
  long r = makecontext_e2k (&uc_a, low_fn, 0);
  stop_flag = 0;
  h_n = 0;
  timer_us (20000, 0);
  printf ("    %s: uc_stack %p-%p, about %ld bytes left above its bottom;"
          " makecontext_e2k %ld\n", names[kind], (void *) base,
          (void *) (base + (16 << 10)), low_free, r);
  fflush (stdout);
  long s = swapcontext (&uc_main, &uc_a);
  long changed = 0, lowest = 0;
  if (kind != 1)
    for (long i = 0; i < LOW_AREA / 2; i++)
      if (low_area[i] != 0x5a)
        {
          changed++;
          if (!lowest)
            lowest = (long) i - LOW_AREA / 2;
        }
  printf ("    context usd.ptr %#lx bottom %#lx; handler ran %d time(s), "
          "local %#lx (%+ld from uc_stack's base); swapcontext %ld; %ld bytes"
          " below uc_stack changed (lowest %+ld)\n", w_low.sp, w_low.bottom,
          h_n, h_local, (long) (h_local - (unsigned long) base), s, changed,
          lowest);
}

static void low_case0 (void) { low_case (0); }
static void low_case1 (void) { low_case (1); }
static void low_case2 (void) { low_case (2); }

/* A timer signal during a swapcontext loop.  */
#define NSW 100000
static volatile long hits_main, hits_a, hits_ip_swap, hits_ip_other;
static long a_iters, a_bad_round, a_swap_err, a_errno;
static char ipname[8][48];
static long ipcount[8];

static void
h_count (int sig, siginfo_t *si, void *ucv)
{
  (void) sig, (void) si;
  ucontext_t *uc = ucv;
  unsigned long isp = VA (uc->uc_mcontext.usd_lo);
  unsigned long ip = uc->uc_mcontext.cr0_hi & M48 & ~7UL;
  if (in_range (isp, stk_a, STK))
    hits_a++;
  else
    hits_main++;
  const char *s = sym (ip);
  for (int i = 0; i < 8; i++)
    if (!ipname[i][0] || !strcmp (ipname[i], s))
      {
        if (!ipname[i][0])
          snprintf (ipname[i], 48, "%s", s);
        ipcount[i]++;
        break;
      }
}

static void
pingpong_a (void)
{
  fesetround (FE_TOWARDZERO);
  for (long i = 0; i < NSW; i++)
    {
      if (fegetround () != FE_TOWARDZERO)
        a_bad_round++;
      a_iters++;
      if (swapcontext (&uc_a, &uc_main) != 0)
        {
          a_swap_err++;
          a_errno = errno;
        }
    }
}

static void
swap_loop (int block_in_a)
{
  handler (SIGALRM, h_count, 0);
  sigset_t m0, m1;
  setup (&uc_a, stk_a, STK, &uc_main);
  makecontext_e2k (&uc_a, pingpong_a, 0);
  if (block_in_a)
    sigaddset (&uc_a.uc_sigmask, SIGALRM);
  fesetround (FE_UPWARD);
  long m_err = 0, m_errno = 0, m_bad_round = 0;
  cur_mask (&m0);
  struct timeval t0, t1;
  gettimeofday (&t0, NULL);
  timer_us (500, 500);
  for (long i = 0; i <= NSW; i++)
    {
      if (swapcontext (&uc_main, &uc_a) != 0)
        {
          m_err++;
          m_errno = errno;
        }
      if (fegetround () != FE_UPWARD)
        m_bad_round++;
    }
  timer_us (0, 0);
  gettimeofday (&t1, NULL);
  cur_mask (&m1);
  printf ("    %s: %ld round trips in %.2f s; signals in main %ld, in the "
          "context %ld; swapcontext failures main %ld (errno %ld), context "
          "%ld (errno %ld); rounding wrong main %ld, context %ld (of %ld); "
          "main's mask before %s, after %s\n",
          block_in_a ? "ALRM blocked in the context's uc_sigmask"
          : "nothing blocked", (long) NSW,
          (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6,
          hits_main, hits_a, m_err, m_errno, a_swap_err, a_errno,
          m_bad_round, a_bad_round, a_iters, mask_str (&m0), mask_str (&m1));
  printf ("    interrupted IPs:");
  for (int i = 0; i < 8 && ipname[i][0]; i++)
    printf (" %s %ld;", ipname[i], ipcount[i]);
  printf ("\n");
}

static void swap_loop0 (void) { swap_loop (0); }
static void swap_loop1 (void) { swap_loop (1); }

/* 2.  Switching contexts from a handler.  */
static volatile int depth;
static sigset_t m_hand1, m_hand2, m_ctx, m_nested, m_after;
static long r_swap1, r_swap2, r_back;
static unsigned long nested_local;
static const char *ss_main0, *ss_hand, *ss_ctx, *ss_nested, *ss_after;
static int ctx_finished;

static void
h_swap (int sig, siginfo_t *si, void *ucv)
{
  (void) sig, (void) si, (void) ucv;
  volatile char loc = 0;
  depth++;
  if (depth == 1)
    {
      cur_mask (&m_hand1);
      ss_hand = sigstacks ();
      r_swap1 = swapcontext (&uc_h, &uc_a);
      cur_mask (&m_hand2);
    }
  else
    {
      nested_local = (unsigned long) &loc;
      cur_mask (&m_nested);
      ss_nested = sigstacks ();
    }
  depth--;
}

static void
from_handler_fn (void)
{
  cur_mask (&m_ctx);
  ss_ctx = sigstacks ();
  raise (SIGUSR1);
  r_back = swapcontext (&uc_a2, &uc_h);
  ctx_finished = 1;
}

static void
swap_from_handler (void)
{
  sigset_t t;
  sigemptyset (&t);
  sigaddset (&t, SIGTERM);
  pthread_sigmask (SIG_BLOCK, &t, NULL);
  handler (SIGUSR1, h_swap, 0);
  ss_main0 = sigstacks ();
  setup (&uc_a, stk_a, STK, &uc_main);
  makecontext_e2k (&uc_a, from_handler_fn, 0);
  volatile long keep = 0x1234567;
  raise (SIGUSR1);
  cur_mask (&m_after);
  ss_after = sigstacks ();
  printf ("    main (TERM blocked) raised USR1; the handler swapped to a made "
          "context A and A swapped back:\n");
  printf ("    masks: handler %s, A %s, nested handler in A %s, handler after"
          " the swap back %s, main after the handler %s\n",
          mask_str (&m_hand1), mask_str (&m_ctx), mask_str (&m_nested),
          mask_str (&m_hand2), mask_str (&m_after));
  printf ("    the nested handler ran %s (local %#lx); swapcontext results "
          "%ld, %ld; main's local %s\n",
          nested_local ? (in_range (nested_local, stk_a, STK)
                          ? "on A's uc_stack" : "elsewhere") : "never",
          nested_local, r_swap1, r_back,
          keep == 0x1234567 ? "intact" : "CHANGED");
  printf ("    [signal stack] mappings: main before %s; in the handler %s; in"
          " A %s; in the nested handler %s; main after %s\n", ss_main0,
          ss_hand, ss_ctx, ss_nested, ss_after);
  r_swap2 = swapcontext (&uc_main, &uc_a2);
  printf ("    then main resumed A's saved state: swapcontext %ld, A %s and "
          "returned through uc_link\n", r_swap2,
          ctx_finished ? "finished" : "did NOT finish");
}

/* setcontext out of a handler to a getcontext point.  */
static volatile int nloop, h_set_fail, timer_mode;
static long h_set_errno;

static void
h_set (int sig, siginfo_t *si, void *ucv)
{
  (void) sig, (void) si, (void) ucv;
  if (timer_mode)
    timer_us (0, 0);
  setcontext (&uc_g);
  h_set_fail++;
  h_set_errno = errno;
}

static void
set_from_handler (int timer)
{
  enum { N = 1000 };
  handler (timer ? SIGALRM : SIGUSR1, h_set, 0);
  timer_mode = timer;
  const char *before = sigstacks ();
  sigset_t m;
  nloop = 0;
  getcontext (&uc_g);
  if (nloop == 1 || nloop == N / 2)
    {
      cur_mask (&m);
      printf ("    after %d: mask %s, [signal stack] %s\n", nloop,
              mask_str (&m), sigstacks ());
    }
  if (nloop < (timer ? N / 5 : N))
    {
      nloop++;
      if (timer)
        {
          timer_us (1000, 0);
          for (;;)
            ;
        }
      raise (SIGUSR1);
      printf ("    the handler returned (setcontext failed %d times, errno "
              "%ld)\n", h_set_fail, h_set_errno);
      return;
    }
  cur_mask (&m);
  printf ("    %s: %d times; setcontext failed %d times; mask now %s; "
          "[signal stack] before %s, after %s\n",
          timer ? "timer ALRM, handler setcontext to the point before the "
          "spin loop" : "raise USR1, handler setcontext to the point before "
          "the raise", nloop, h_set_fail, mask_str (&m), before, sigstacks ());
}

static void set_from_handler0 (void) { set_from_handler (0); }
static void set_from_handler1 (void) { set_from_handler (1); }

/* setcontext from a handler to a made context whose uc_link is a
   getcontext point.  */
static sigset_t m_made;
static int made_mask_bad, made_runs;
static const char *ss_made_first, *ss_made_last;

static void
made_fn (void)
{
  cur_mask (&m_made);
  made_mask_bad += sigismember (&m_made, SIGUSR1);
  made_runs++;
  if (made_runs == 1)
    ss_made_first = sigstacks ();
  ss_made_last = sigstacks ();
}

static void
h_set_made (int sig, siginfo_t *si, void *ucv)
{
  (void) sig, (void) si, (void) ucv;
  setcontext (&uc_a);
  h_set_fail++;
  h_set_errno = errno;
}

static void
set_made_from_handler (void)
{
  enum { N = 200 };
  handler (SIGUSR1, h_set_made, 0);
  const char *before = sigstacks ();
  long mk_fail = 0;
  nloop = 0;
  getcontext (&uc_g);
  if (nloop < N)
    {
      nloop++;
      setup (&uc_a, stk_a, STK, &uc_g);
      if (makecontext_e2k (&uc_a, made_fn, 0) != 0)
        mk_fail++;
      raise (SIGUSR1);
      printf ("    the handler returned (setcontext failed)\n");
      return;
    }
  sigset_t m;
  cur_mask (&m);
  printf ("    raise USR1, handler setcontext to a made context whose uc_link"
          " is the point before the raise: %d times, context ran %d times "
          "with USR1 blocked %d times; makecontext_e2k failed %ld times;"
          " main's mask %s; [signal stack] before %s, in the first run %s, in"
          " the last %s, after %s\n", nloop, made_runs, made_mask_bad,
          mk_fail, mask_str (&m), before, ss_made_first, ss_made_last,
          sigstacks ());
}

/* A timer handler that schedules three contexts round robin.  */
#define NT 3
#define RR_N 20000000L
static ucontext_t uc_t[NT];
static char stk_t[NT][STK] __attribute__ ((aligned (4096)));
static volatile int rr_cur, rr_done[NT];
static volatile long rr_switches, rr_ticks, rr_work[NT];
static long rr_badround[NT], rr_ok[NT];
static sigset_t rr_mask_start[NT];

static void
rr_fn (long i)
{
  static const int modes[NT] = { FE_TONEAREST, FE_UPWARD, FE_TOWARDZERO };
  cur_mask (&rr_mask_start[i]);
  fesetround (modes[i]);
  long a = 1, b = 7, n;
  for (n = 1; n <= RR_N; n++)
    {
      a = a * 3 + 1;
      b += a ^ n;
      rr_work[i] = n;
      if ((n & 1023) == 0 && fegetround () != modes[i])
        rr_badround[i]++;
    }
  rr_ok[i] = check_work (n - 1, a, b);
  rr_done[i] = 1;
  for (;;)
    ;
}

static void
h_rr (int sig, siginfo_t *si, void *ucv)
{
  (void) sig, (void) si, (void) ucv;
  int c = rr_cur;
  rr_ticks++;
  if (rr_done[0] && rr_done[1] && rr_done[2])
    {
      timer_us (0, 0);
      swapcontext (&uc_t[c], &uc_main);
      return;
    }
  int n = (c + 1) % NT;
  while (rr_done[n] && n != c)
    n = (n + 1) % NT;
  if (n == c)
    return;
  rr_cur = n;
  rr_switches++;
  if (swapcontext (&uc_t[c], &uc_t[n]) != 0)
    rr_switches += 1000000;
}

static void
round_robin (void)
{
  handler (SIGALRM, h_rr, 0);
  for (int i = 0; i < NT; i++)
    {
      setup (&uc_t[i], stk_t[i], STK, &uc_main);
      if (makecontext_e2k (&uc_t[i], (void (*) (void)) rr_fn, 1, (long) i))
        printf ("    makecontext_e2k %d failed: %s\n", i, strerror (errno));
    }
  struct timeval t0, t1;
  gettimeofday (&t0, NULL);
  rr_cur = 0;
  timer_us (2000, 2000);
  long s = swapcontext (&uc_main, &uc_t[0]);
  gettimeofday (&t1, NULL);
  sigset_t m;
  cur_mask (&m);
  printf ("    3 contexts x %ld iterations under a 2 ms ALRM that switches "
          "in its handler: back in main after %.2f s, swapcontext %ld; %ld "
          "ticks, %ld switches; main's mask %s; [signal stack] %s\n",
          RR_N, (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6, s,
          rr_ticks, rr_switches, mask_str (&m), sigstacks ());
  for (int i = 0; i < NT; i++)
    printf ("      context %d: work %ld, values %s, rounding wrong %ld times,"
            " mask at start %s\n", i, rr_work[i],
            rr_ok[i] ? "intact" : "CORRUPTED", rr_badround[i],
            mask_str (&rr_mask_start[i]));
  for (int i = 0; i < NT; i++)
    {
      errno = 0;
      long r = freecontext_e2k (&uc_t[i]);
      printf ("      freecontext_e2k (context %d, suspended in its handler)"
              " = %ld %s\n", i, r, r ? strerror (errno) : "");
    }
  printf ("    [signal stack] after the frees %s\n", sigstacks ());
}

/* 3.  Data stack overflow in a thread and in the main thread.  */
static size_t ov_frame;
static volatile int lv_n, mv_n;
static unsigned long last_sp, last_bottom;
static int mv_level[32];
static long mv_delta[32];
static unsigned char *ov_area, *ov_below_lo;
static size_t ov_below_len;
static unsigned long ov_stack_lo, ov_guard_lo, ov_guard_hi;
/* For a glibc stack: a copy of the top of the mapping below the guard,
   taken just before the dive, to count what the dive changed there.  */
#define SNAP_MAX (1 << 20)
static unsigned char ov_snap[SNAP_MAX];
static int ov_snapped;

static void __attribute__ ((noinline))
dive (int n)
{
  volatile unsigned char *p = __builtin_alloca (ov_frame);
  memset ((void *) p, 0xa5, ov_frame);
  unsigned long lo = al_usd_lo (), hi = al_usd_hi ();
  unsigned long b = VA (lo) - (hi >> 32);
  if (b != last_bottom && mv_n < 32)
    {
      mv_level[mv_n] = lv_n;
      mv_delta[mv_n++] = (long) (b - last_bottom);
    }
  last_sp = VA (lo);
  last_bottom = b;
  lv_n++;
  if (n > 1)
    dive (n - 1);
  p[0] = 1;
}

static long
count_changed (long *lowest)
{
  long c = 0;
  *lowest = 0;
  if (!ov_below_lo)
    return -1;
  for (size_t i = 0; i < ov_below_len; i++)
    if (ov_below_lo[i] != (ov_snapped ? ov_snap[i] : 0x5a))
      {
        if (!c)
          *lowest = (long) i - (long) ov_below_len;
        c++;
      }
  return c;
}

static const char *
code_name (int sig, int code)
{
  if (sig != SIGSEGV)
    return "";
  switch (code)
    {
    case SEGV_MAPERR: return "SEGV_MAPERR";
    case SEGV_ACCERR: return "SEGV_ACCERR";
#ifdef SEGV_BNDERR
    case SEGV_BNDERR: return "SEGV_BNDERR";
#endif
    case SI_KERNEL: return "SI_KERNEL";
    default: return "?";
    }
}

static void
ov_report (void)
{
  printf ("      levels completed %d", lv_n);
  if (lv_n)
    printf ("; last usd.ptr %#lx, bottom %#lx (%+ld from the stack's "
            "bottom at entry)", last_sp, last_bottom,
            (long) (last_bottom - ov_stack_lo));
  printf ("\n");
  for (int i = 0; i < mv_n; i++)
    printf ("      level %d: the bottom moved by %ld\n", mv_level[i],
            mv_delta[i]);
  long lowest, c = count_changed (&lowest);
  if (c >= 0)
    printf ("      bytes changed in the %zu bytes below the guard/stack: %ld"
            " (lowest %+ld)\n", ov_below_len, c, lowest);
}

static void
ov_segv (int sig, siginfo_t *si, void *ctx)
{
  ucontext_t *uc = ctx;
  volatile char loc = 0;
  unsigned long a = (unsigned long) si->si_addr;
  unsigned long isp = VA (uc->uc_mcontext.usd_lo);
  unsigned long ibot = isp - (uc->uc_mcontext.usd_hi >> 32);
  printf ("      SIGNAL %d %s (si_code %d), si_addr %#lx: %+ld from the stack's"
          " bottom at entry", sig, code_name (sig, si->si_code), si->si_code,
          a, (long) (a - ov_stack_lo));
  if (ov_guard_hi)
    printf (", %s the guard %#lx-%#lx",
            a >= ov_guard_lo && a < ov_guard_hi ? "INSIDE" : "outside",
            ov_guard_lo, ov_guard_hi);
  printf ("; the handler runs %s\n",
          in_range ((unsigned long) &loc, altbuf, sizeof altbuf)
          ? "on the alternate stack" : "NOT on the alternate stack");
  printf ("      si_addr %s\n", map_of (a));
  printf ("      interrupted usd.ptr %#lx, bottom %#lx (%+ld from the stack's"
          " bottom at entry; %s)\n", isp, ibot, (long) (ibot - ov_stack_lo),
          map_of (ibot));
  ov_report ();
  fflush (stdout);
  _exit (0);
}

struct ovcase
{
  const char *name;
  int kind;          /* 0 glibc stack, 1..6 user stack, see below */
  size_t stack, guard, frame;
  int levels;
};

/* User stacks: STACK bytes at ov_area + 1 MiB in a 1 MiB + STACK + 64 KiB
   mapping filled with 0x5a; below it
     1 the mapping itself (no guard)
     2 a 4 KiB PROT_NONE guard, then the mapping
     3 a 64 KiB PROT_NONE guard, then the mapping
     4 a 4 KiB PROT_NONE guard, then nothing (the lower 1 MiB unmapped)
     5 one unmapped page, then the mapping
     6 a 4 KiB PROT_NONE guard, 64 KiB unmapped, then the mapping.  */
static const struct ovcase ovcases[] = {
  { "glibc stack 64 KiB, default guard, 4 KiB frames", 0, 64 << 10, 0,
    4 << 10, 64 },
  { "glibc stack 64 KiB, default guard, 96 KiB frame", 0, 64 << 10, 0,
    96 << 10, 4 },
  { "glibc stack 64 KiB, 128 KiB guard, 96 KiB frame", 0, 64 << 10,
    128 << 10, 96 << 10, 4 },
  { "glibc stack of the default size, 4 KiB frames", 0, 0, 0, 4 << 10,
    8192 },
  { "user stack 64 KiB, mapped below, 4 KiB frames", 1, 64 << 10, 0,
    4 << 10, 64 },
  { "user stack 64 KiB, 4 KiB guard then mapped, 4 KiB frames", 2, 64 << 10,
    4 << 10, 4 << 10, 64 },
  { "user stack 64 KiB, 4 KiB guard then mapped, 96 KiB frame", 2, 64 << 10,
    4 << 10, 96 << 10, 4 },
  { "user stack 64 KiB, 64 KiB guard then mapped, 4 KiB frames", 3,
    64 << 10, 64 << 10, 4 << 10, 64 },
  { "user stack 64 KiB, 64 KiB guard then mapped, 96 KiB frame", 3,
    64 << 10, 64 << 10, 96 << 10, 4 },
  { "user stack 64 KiB, 4 KiB guard then unmapped, 4 KiB frames", 4,
    64 << 10, 4 << 10, 4 << 10, 64 },
  { "user stack 64 KiB, one unmapped page then mapped, 4 KiB frames", 5,
    64 << 10, 4 << 10, 4 << 10, 64 },
  { "user stack 64 KiB, 4 KiB guard + 64 KiB hole then mapped, 96 KiB "
    "frame", 6, 64 << 10, 4 << 10, 96 << 10, 4 },
};

static const struct ovcase *ovc;
static pthread_t main_self;

static void *
ov_thread (void *arg)
{
  (void) arg;
  stack_t ss = { .ss_sp = altbuf, .ss_size = sizeof altbuf };
  sigaltstack (&ss, NULL);
  pthread_attr_t a;
  void *addr;
  size_t size, guard = 0;
  pthread_getattr_np (pthread_self (), &a);
  pthread_attr_getstack (&a, &addr, &size);
  pthread_attr_getguardsize (&a, &guard);
  struct where w;
  where_now (&w);
  ov_stack_lo = last_bottom = w.bottom;
  if (ovc->kind == 0)
    {
      ov_guard_hi = (unsigned long) addr;
      ov_guard_lo = ov_guard_hi - guard;
    }
  printf ("      pthread stack %p-%p (%#zx), guard %#zx; usd bottom %#lx "
          "(%+ld from the stack's start), free %#lx\n", addr,
          (void *) ((char *) addr + size), size, guard, w.bottom,
          (long) (w.bottom - (unsigned long) addr), w.sp - w.bottom);
  if (getenv ("ABILEFT_MAPS"))
    {
      FILE *f = fopen ("/proc/self/maps", "r");
      char line[512];
      while (f && fgets (line, sizeof line, f))
        printf ("        | %s", line);
      if (f)
        fclose (f);
      printf ("        | main's pthread_self %#lx, this thread's %#lx\n",
              (unsigned long) main_self, (unsigned long) pthread_self ());
    }
  if (ov_guard_hi)
    printf ("      guard %s\n      below the guard: %s\n",
            map_of (ov_guard_lo), map_of (ov_guard_lo - 1));
  else
    printf ("      below the stack: %s\n", map_of (ov_stack_lo - 1));
  if (ovc->kind == 0)
    {
      /* A glibc stack: watch the top (up to 1 MiB) of a writable mapping
         that ends at the guard, and say where ld.so's link maps are.  */
      unsigned long lo = 0, hi = 0;
      char perm[8] = "";
      FILE *f = fopen ("/proc/self/maps", "r");
      char line[512];
      while (f && fgets (line, sizeof line, f))
        {
          unsigned long a, b;
          char p[8];
          if (sscanf (line, "%lx-%lx %7s", &a, &b, p) == 3
              && a < ov_guard_lo && ov_guard_lo <= b)
            {
              lo = a;
              hi = b;
              snprintf (perm, sizeof perm, "%s", p);
            }
        }
      if (f)
        fclose (f);
      if (hi == ov_guard_lo && perm[0] == 'r' && perm[1] == 'w')
        {
          if (hi - lo > SNAP_MAX)
            lo = hi - SNAP_MAX;
          ov_below_lo = (unsigned char *) lo;
          ov_below_len = hi - lo;
        }
      for (struct link_map *l = _r_debug.r_map; l; l = l->l_next)
        printf ("      link_map %p (%s): %s\n", (void *) l,
                l->l_name[0] ? l->l_name : "the executable",
                ov_below_lo && in_range ((unsigned long) l, ov_below_lo,
                                         ov_below_len)
                ? "in the watched mapping below the guard" : "elsewhere");
    }
  fflush (stdout);
  if (ovc->kind == 0 && ov_below_lo)
    {
      memcpy (ov_snap, ov_below_lo, ov_below_len);
      ov_snapped = 1;
    }
  dive (ovc->levels);
  printf ("      NO SIGNAL: dive returned\n");
  ov_report ();
  return NULL;
}

static void
ov_case (void)
{
  handler (SIGSEGV, ov_segv, SA_ONSTACK);
  handler (SIGBUS, ov_segv, SA_ONSTACK);
  ov_frame = ovc->frame;
  pthread_attr_t a;
  pthread_attr_init (&a);
  if (ovc->kind == 0)
    {
      if (ovc->stack)
        pthread_attr_setstacksize (&a, ovc->stack);
      if (ovc->guard)
        pthread_attr_setguardsize (&a, ovc->guard);
    }
  else
    {
      size_t len = (1 << 20) + ovc->stack + (64 << 10);
      ov_area = mmap (NULL, len, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      memset (ov_area, 0x5a, len);
      unsigned char *st = ov_area + (1 << 20);
      ov_below_lo = ov_area;
      ov_below_len = 1 << 20;
      switch (ovc->kind)
        {
        case 2:
        case 3:
          mprotect (st - ovc->guard, ovc->guard, PROT_NONE);
          ov_guard_lo = (unsigned long) st - ovc->guard;
          ov_guard_hi = (unsigned long) st;
          ov_below_len -= ovc->guard;
          break;
        case 4:
          mprotect (st - ovc->guard, ovc->guard, PROT_NONE);
          munmap (ov_area, (1 << 20) - ovc->guard);
          ov_guard_lo = (unsigned long) st - ovc->guard;
          ov_guard_hi = (unsigned long) st;
          ov_below_lo = NULL;
          break;
        case 5:
          munmap (st - 4096, 4096);
          ov_below_len -= 4096;
          break;
        case 6:
          mprotect (st - ovc->guard, ovc->guard, PROT_NONE);
          munmap (st - ovc->guard - (64 << 10), 64 << 10);
          ov_guard_lo = (unsigned long) st - ovc->guard;
          ov_guard_hi = (unsigned long) st;
          ov_below_len -= ovc->guard + (64 << 10);
          break;
        }
      pthread_attr_setstack (&a, st, ovc->stack);
    }
  pthread_t t;
  main_self = pthread_self ();
  int r = pthread_create (&t, &a, ov_thread, NULL);
  if (r)
    {
      printf ("      pthread_create: %s\n", strerror (r));
      return;
    }
  pthread_join (t, NULL);
}

/* The main thread: 4 KiB frames under a 2 MiB RLIMIT_STACK; 64 KiB
   frames under the default limit; and one frame that reaches into a
   mapping placed 3 MiB below [stack].  */
static int main_kind;

static void
main_case (void)
{
  stack_t ss = { .ss_sp = altbuf, .ss_size = sizeof altbuf };
  sigaltstack (&ss, NULL);
  handler (SIGSEGV, ov_segv, SA_ONSTACK);
  handler (SIGBUS, ov_segv, SA_ONSTACK);
  struct rlimit rl;
  getrlimit (RLIMIT_STACK, &rl);
  if (main_kind == 0)
    {
      rl.rlim_cur = 2 << 20;
      setrlimit (RLIMIT_STACK, &rl);
    }
  struct where w;
  where_now (&w);
  ov_stack_lo = last_bottom = w.bottom;
  ov_guard_lo = ov_guard_hi = 0;
  printf ("      RLIMIT_STACK %#lx; [stack]: %s; usd bottom %#lx free %#lx\n",
          (unsigned long) rl.rlim_cur, map_of (w.sp), w.bottom,
          w.sp - w.bottom);
  printf ("      below the bottom: %s\n", map_of (w.bottom - 1));
  if (main_kind == 2)
    {
      FILE *f = fopen ("/proc/self/maps", "r");
      char line[512];
      unsigned long slo = 0, shi = 0;
      while (f && fgets (line, sizeof line, f))
        if (strstr (line, "[stack]"))
          sscanf (line, "%lx-%lx", &slo, &shi);
      if (f)
        fclose (f);
      unsigned long want = (slo - (3 << 20)) & ~0xffffUL;
      ov_below_len = 1 << 20;
      ov_below_lo = mmap ((void *) (want - ov_below_len), ov_below_len,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                          -1, 0);
      if (ov_below_lo == MAP_FAILED)
        {
          printf ("      mmap below [stack] failed: %s\n", strerror (errno));
          return;
        }
      memset (ov_below_lo, 0x5a, ov_below_len);
      printf ("      a 1 MiB mapping at %p-%p, %ld KiB below the bottom\n",
              (void *) ov_below_lo, (void *) (ov_below_lo + ov_below_len),
              (long) (w.bottom - want) >> 10);
      ov_frame = w.bottom - ((unsigned long) ov_below_lo + (256 << 10));
      printf ("      one frame of %#zx bytes\n", ov_frame);
      fflush (stdout);
      dive (1);
    }
  else
    {
      ov_below_lo = NULL;
      ov_frame = main_kind == 0 ? 4 << 10 : 64 << 10;
      dive (main_kind == 0 ? 4096 : 512);
    }
  printf ("      NO SIGNAL: dive returned\n");
  ov_report ();
}

static void
run_part1 (void)
{
  printf ("1. signals while a made context runs\n");
  in_child ("timer in a context", sig_in_ctx0, 10);
  in_child ("timer in a context, SA_ONSTACK", sig_in_ctx1, 10);
  printf ("  a signal frame with 1 KiB free in the context's 16 KiB stack\n");
  in_child ("low, mapped below", low_case0, 10);
  in_child ("low, unmapped below", low_case1, 10);
  in_child ("low, SA_ONSTACK", low_case2, 10);
  printf ("  a 500 us timer during a swapcontext loop\n");
  in_child ("swap loop", swap_loop0, 60);
  in_child ("swap loop, ALRM blocked in A", swap_loop1, 60);
}

static void
run_part2 (void)
{
  printf ("2. switching contexts from a handler\n");
  in_child ("swapcontext from a handler", swap_from_handler, 10);
  in_child ("setcontext out of a handler (raise)", set_from_handler0, 30);
  in_child ("setcontext out of a handler (timer)", set_from_handler1, 30);
  in_child ("setcontext to a made context", set_made_from_handler, 30);
  in_child ("round robin", round_robin, 60);
}

static int only = -1;

static void
run_part3 (void)
{
  printf ("3. data stack overflow\n");
  for (size_t i = 0; i < sizeof ovcases / sizeof ovcases[0]; i++)
    {
      if (only >= 0 && (size_t) only != i)
        continue;
      ovc = &ovcases[i];
      printf ("  %s\n", ovc->name);
      in_child (ovc->name, ov_case, 60);
    }
  static const char *const mnames[] = {
    "main thread, 4 KiB frames, RLIMIT_STACK 2 MiB",
    "main thread, 64 KiB frames, default RLIMIT_STACK",
    "main thread, one frame reaching into a mapping 3 MiB below [stack]" };
  for (main_kind = 0; main_kind < 3 && only < 0; main_kind++)
    {
      printf ("  %s\n", mnames[main_kind]);
      in_child (mnames[main_kind], main_case, 120);
    }
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  int part = argc > 1 ? atoi (argv[1]) : 0;
  if (argc > 2)
    only = atoi (argv[2]);
  if (!part || part == 1)
    run_part1 ();
  if (!part || part == 2)
    run_part2 ();
  if (!part || part == 3)
    run_part3 ();
  return 0;
}
