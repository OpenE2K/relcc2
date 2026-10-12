/* SPDX-License-Identifier: GPL-3.0-or-later */
/* User contexts shared across threads on MCST's Linux 6.1
   (spec/abi/kernel-interface.md, "User contexts"), with the documented
   calls only (getcontext, setcontext, swapcontext, makecontext_e2k,
   freecontext_e2k, setjmp/longjmp, sigaction, sigaltstack, pthread_*,
   fork):

   1. A context made in one thread and first run in another.
   2. A context that migrates: suspended in thread A, resumed in thread
      B and back, 2,000 times; A resumes it with swapcontext, B with
      setcontext from a getcontext point.  What the context sees
      (thread id, TLS, its hardware stacks, its FP mode and signal mask)
      and what each thread keeps; what lcc folds across the switch.
   3. Hardware stacks that grew in A (deep recursion) unwound in B.
   4. Signals: a signal sent to the thread that runs a migrated context;
      a context suspended inside a handler in A whose handler returns
      in B; the same with SA_ONSTACK.
   5. freecontext_e2k of a context that runs in another thread, which
      then switches away, or exits while still in it, and the control
      cases.  After each: do four new contexts get distinct hardware
      stacks, and do deep calls in them, suspended in turn, survive?
      (5t: do two new threads running at once?)
   6. makecontext_e2k on the stack (key) of a context that runs in
      another thread; the same, then that thread exits in it.
   7. Two threads switching to one suspended context at once (then
      with random delays after the barrier).
   8. Threads that exit: a suspended context made by an exited thread;
      its uc_link naming the exited thread's context; a thread exiting
      while another runs its context; a thread exiting inside a made
      context (its own context stays: the next thread with the same
      stack); pthread_exit inside a made context.
   9. fork: a suspended context in the child, fork inside a made
      context, a context running in another thread at the fork.
  10. longjmp into a suspended context from another thread, and into
      one running in another thread.
  11. A thread's own (original) context resumed by another thread.
  12. The [signal stack] of a context that a thread exits in: is it
      unmapped twice (does a new thread's [signal stack], mapped there
      in between, vanish)?  And uc_link naming an exited thread's state
      when a new thread runs on that thread's old stack.
   Every case runs in a forked child with an alarm.  `./ctxthr N` runs
   part N only; `./ctxthr 7 R` runs R race rounds (default 20000; the
   other two races at least 200000); CTXTHR_MAPS=1 also prints the
   hardware and signal stack mappings in part 12.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc  # or v5.8c2, v6.2c3
     $L -O3 -rdynamic ctxthr.c -o ctxthr -lpthread -lm
     ./ctxthr [N [ROUNDS]]                                # on hardware  */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fenv.h>
#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#include <asm/e2k_syswork.h>

#define M48 ((1UL << 48) - 1)

/* Thread index: main 0, A 1, B 2, others 3.. (TLS: follows the kernel
   thread, not the context).  */
static __thread int me;
static long tids[8];
static void *tlss[8];

static long __attribute__ ((noinline))
cur_tid (void)
{
  return syscall (SYS_gettid);
}

static int __attribute__ ((noinline))
cur_me (void)
{
  return me;
}

static void *__attribute__ ((noinline))
cur_tls (void)
{
  return (void *) &me;
}

static unsigned long __attribute__ ((noinline))
rd_psp (void)
{
  unsigned long v;
  asm volatile ("rrd %%psp.lo, %0" : "=r" (v));
  return v & M48;
}

static unsigned long __attribute__ ((noinline))
rd_pcsp (void)
{
  unsigned long v;
  asm volatile ("rrd %%pcsp.lo, %0" : "=r" (v));
  return v & M48;
}

static unsigned long __attribute__ ((noinline))
rd_usd (void)
{
  unsigned long v;
  asm volatile ("rrd %%usd.lo, %0" : "=r" (v));
  return v & M48;
}

static const char *
mask_str (const sigset_t *m)
{
  static char out[16][96];
  static int k;
  char *o = out[k++ & 15];
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

static const char *
cur_mask (void)
{
  sigset_t m;
  sigemptyset (&m);
  pthread_sigmask (SIG_BLOCK, NULL, &m);
  return mask_str (&m);
}

static void
set_mask (int sig1, int sig2)
{
  sigset_t s;
  sigemptyset (&s);
  if (sig1)
    sigaddset (&s, sig1);
  if (sig2)
    sigaddset (&s, sig2);
  pthread_sigmask (SIG_SETMASK, &s, NULL);
}

static const char *
rname (int r)
{
  return r == FE_TONEAREST ? "nearest" : r == FE_UPWARD ? "upward"
    : r == FE_DOWNWARD ? "downward" : r == FE_TOWARDZERO ? "toward zero"
    : "?";
}

/* Where and as whom the calling code runs.  */
struct obs
{
  long tid;
  void *tls;
  int me, rnd;
  unsigned long psp, pcsp, usd;
  long pss, pcss;
  char mask[32];
};

static void __attribute__ ((noinline))
observe (struct obs *o)
{
  o->tid = cur_tid ();
  o->tls = cur_tls ();
  o->me = cur_me ();
  o->psp = rd_psp ();
  o->pcsp = rd_pcsp ();
  o->usd = rd_usd ();
  o->pss = o->pcss = -1;
  syscall (__NR_access_hw_stacks, E2K_GET_PROCEDURE_STACK_SIZE, NULL, NULL,
           0, &o->pss);
  syscall (__NR_access_hw_stacks, E2K_GET_CHAIN_STACK_SIZE, NULL, NULL, 0,
           &o->pcss);
  o->rnd = fegetround ();
  snprintf (o->mask, sizeof o->mask, "%s", cur_mask ());
}

static const char *
who (long tid)
{
  static char b[8][32];
  static int k;
  char *p = b[k++ & 7];
  static const char *const nm[] = { "main", "A", "B", "T3", "T4", "T5",
                                    "T6", "T7" };
  for (int i = 0; i < 8; i++)
    if (tids[i] == tid && tid)
      {
        snprintf (p, 32, "%s", nm[i]);
        return p;
      }
  snprintf (p, 32, "tid %ld", tid);
  return p;
}

static const char *
tls_who (void *t)
{
  static const char *const nm[] = { "main's", "A's", "B's", "T3's", "T4's",
                                    "T5's", "T6's", "T7's" };
  for (int i = 0; i < 8; i++)
    if (tlss[i] == t && t)
      return nm[i];
  return "unknown";
}

static void
print_obs (const char *what, const struct obs *o)
{
  printf ("    %-34s thread %s, TLS %s, psp.base %#lx (%ld B) pcsp.base "
          "%#lx (%ld B), usd.ptr %#lx, %s, mask %s\n", what, who (o->tid),
          tls_who (o->tls), o->psp, o->pss, o->pcsp, o->pcss, o->usd,
          rname (o->rnd), o->mask);
}

/* Lines of /proc/self/maps that contain S.  */
static int
maps_count (const char *s)
{
  int n = 0;
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    n += strstr (line, s) != NULL;
  if (f)
    fclose (f);
  return n;
}

static int
mapped (unsigned long a)
{
  int found = 0;
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    {
      unsigned long lo, hi;
      sscanf (line, "%lx-%lx", &lo, &hi);
      found |= a >= lo && a < hi;
    }
  if (f)
    fclose (f);
  return found;
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
      tids[0] = cur_tid ();
      tlss[0] = cur_tls ();
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

#define CHECK(call) \
  ({ errno = 0; long r_ = (call); \
     r_ < 0 ? -errno : r_; })

static const char *
err (long r)
{
  static char b[8][32];
  static int k;
  char *p = b[k++ & 7];
  if (r >= 0)
    snprintf (p, 32, "%ld", r);
  else
    snprintf (p, 32, "-1 (%s)", r == -ENOENT ? "ENOENT" : r == -EBUSY
              ? "EBUSY" : r == -ESRCH ? "ESRCH" : r == -EINVAL ? "EINVAL"
              : r == -EEXIST ? "EEXIST" : r == -EFAULT ? "EFAULT"
              : strerror ((int) -r));
  return p;
}

/* lcc treats setcontext as noreturn: call it through a pointer.  */
static int (*volatile p_setcontext) (const ucontext_t *) = setcontext;

static void
wait_eq (volatile int *p, int v)
{
  for (long i = 0; __atomic_load_n (p, __ATOMIC_ACQUIRE) != v; i++)
    if (i > 1000)
      sched_yield ();
}

static void
put (volatile int *p, int v)
{
  __atomic_store_n (p, v, __ATOMIC_RELEASE);
}

/* Threads: IDX's TLS index and tid are recorded before FN runs.  */
struct targ
{
  int idx;
  void (*fn) (void);
};

static struct targ targs[8];

static void *
thr_main (void *arg)
{
  struct targ *t = arg;
  me = t->idx;
  tids[me] = cur_tid ();
  tlss[me] = cur_tls ();
  t->fn ();
  return NULL;
}

static pthread_t
spawn (int idx, void (*fn) (void))
{
  pthread_t t;
  targs[idx].idx = idx;
  targs[idx].fn = fn;
  tids[idx] = 0;
  pthread_create (&t, NULL, thr_main, &targs[idx]);
  while (!__atomic_load_n (&tids[idx], __ATOMIC_ACQUIRE))
    sched_yield ();
  return t;
}

#define STK (64 << 10)
static char stk[8][STK] __attribute__ ((aligned (4096)));
static ucontext_t home[8];          /* each thread's saved state */
static ucontext_t uc_c, uc_cs, uc_n, uc_k[8];

static void
setup (ucontext_t *uc, void *s, ucontext_t *link)
{
  memset (uc, 0, sizeof *uc);
  uc->uc_stack.ss_sp = s;
  uc->uc_stack.ss_size = STK;
  uc->uc_link = link;
}

static int
work_ok (long n, long a, long b)
{
  long ea = 1, eb = 7;
  for (long i = 1; i <= n; i++)
    {
      ea = ea * 3 + 1;
      eb += ea ^ i;
    }
  return ea == a && eb == b;
}

/* Report a crash: which thread, what, where.  */
static void
h_crash (int sig, siginfo_t *si, void *ucv)
{
  ucontext_t *uc = ucv;
  char b[256];
  Dl_info di;
  unsigned long ip = uc->uc_mcontext.cr0_hi & M48 & ~7UL;
  const char *fn = dladdr ((void *) ip, &di) && di.dli_sname ? di.dli_sname
                   : "?";
  int n = snprintf (b, sizeof b, "      %s in thread %s (TLS index %d): "
                    "code %d, address %p, IP %#lx (%s)\n", strsignal (sig),
                    who (cur_tid ()), cur_me (), si->si_code, si->si_addr,
                    ip, fn);
  write (1, b, n);
  _exit (100 + sig);
}

static void
handler_crash (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = h_crash;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGSEGV, &sa, NULL);
  sigaction (SIGILL, &sa, NULL);
  sigaction (SIGBUS, &sa, NULL);
}

/* Suspend the running context into uc_cs and go to the home state of
   the thread that runs it.  */
static void __attribute__ ((noinline))
suspend (void)
{
  swapcontext (&uc_cs, &home[cur_me ()]);
}

/* 1.  Made in main, first run in B.  */
static struct obs o_ctx, o_main, o_b;
static volatile long r_b;

static void
c1_fn (void)
{
  observe (&o_ctx);
}

static void
c1_b (void)
{
  r_b = CHECK (swapcontext (&home[2], &uc_c));
  observe (&o_b);
}

static void
case_first_run (void)
{
  setup (&uc_c, stk[0], &home[2]);
  long r = CHECK (makecontext_e2k (&uc_c, c1_fn, 0));
  observe (&o_main);
  pthread_t t = spawn (2, c1_b);
  pthread_join (t, NULL);
  printf ("    makecontext_e2k in main: %s; B's swapcontext to it: %s; the "
          "function returned through uc_link = B's state\n", err (r),
          err (r_b));
  print_obs ("main:", &o_main);
  print_obs ("the context (started by B):", &o_ctx);
  print_obs ("B after the context returned:", &o_b);
}

/* 2.  Migration: A (swapcontext) and B (getcontext + setcontext) take
   turns resuming one context.  */
#define NMIG 2000
struct mrec
{
  long tid;
  void *tls, *tls_folded;
  unsigned long psp, pcsp, usd;
  int rnd_ok, mask_ok;
};
static struct mrec mrec[NMIG + 1];
static volatile int turn, mig_done, mig_ok, home_bad[3], sw_fail[3];
static const ucontext_t *volatile next_uc;
static long mig_n;
static int lcc_same;

static void
mig_fn (void)
{
  fesetround (FE_TOWARDZERO);
  set_mask (SIGUSR2, 0);
  long n = 0, a = 1, b = 7;
  for (int k = 0; k <= NMIG; k++)
    {
      struct mrec *m = &mrec[k];
      void *before = &me;          /* lcc folds &me across the switch */
      m->tid = cur_tid ();
      m->tls = cur_tls ();
      m->psp = rd_psp ();
      m->pcsp = rd_pcsp ();
      m->usd = rd_usd ();
      m->rnd_ok = fegetround () == FE_TOWARDZERO;
      sigset_t s;
      pthread_sigmask (SIG_BLOCK, NULL, &s);
      m->mask_ok = sigismember (&s, SIGUSR2) && !sigismember (&s, SIGHUP)
                   && !sigismember (&s, SIGTERM);
      for (int j = 0; j < 100; j++)
        {
          n++;
          a = a * 3 + 1;
          b += a ^ n;
        }
      if (k == NMIG)
        {
          mig_n = n;
          mig_ok = work_ok (n, a, b);
          put (&mig_done, 1);
        }
      suspend ();
      if (k == 1)
        {
          void *after = &me;
          lcc_same = before == after;
          m->tls_folded = cur_tls ();
        }
    }
}

static void
mig_thr (void)
{
  int my = me;
  static volatile int pass[3];
  fesetround (my == 1 ? FE_UPWARD : FE_DOWNWARD);
  set_mask (my == 1 ? SIGHUP : SIGTERM, 0);
  for (;;)
    {
      while (__atomic_load_n (&turn, __ATOMIC_ACQUIRE) != my
             && !__atomic_load_n (&mig_done, __ATOMIC_ACQUIRE))
        ;
      if (__atomic_load_n (&mig_done, __ATOMIC_ACQUIRE))
        break;
      const ucontext_t *nx = next_uc;
      if (my == 1)
        {
          if (swapcontext (&home[1], nx))
            sw_fail[1]++;
        }
      else
        {
          pass[my] = 0;
          getcontext (&home[my]);
          if (pass[my]++ == 0)
            {
              p_setcontext (nx);
              sw_fail[my]++;
            }
        }
      sigset_t s;
      pthread_sigmask (SIG_BLOCK, NULL, &s);
      int want = my == 1 ? SIGHUP : SIGTERM;
      if (fegetround () != (my == 1 ? FE_UPWARD : FE_DOWNWARD)
          || !sigismember (&s, want) || sigismember (&s, SIGUSR2))
        home_bad[my]++;
      next_uc = &uc_cs;
      put (&turn, 3 - my);
    }
}

static void
case_migrate (void)
{
  setup (&uc_c, stk[0], &home[0]);
  long r = CHECK (makecontext_e2k (&uc_c, mig_fn, 0));
  next_uc = &uc_c;
  turn = 1;
  pthread_t ta = spawn (1, mig_thr);
  pthread_t tb = spawn (2, mig_thr);
  pthread_join (ta, NULL);
  pthread_join (tb, NULL);
  int tid_ok = 0, tls_ok = 0, psp_same = 0, rnd_ok = 0, mask_ok = 0;
  int usd_same = 0;
  for (int k = 0; k <= NMIG; k++)
    {
      int t = k & 1 ? 2 : 1;
      tid_ok += mrec[k].tid == tids[t];
      tls_ok += mrec[k].tls == tlss[t];
      psp_same += mrec[k].psp == mrec[0].psp && mrec[k].pcsp == mrec[0].pcsp;
      usd_same += mrec[k].usd == mrec[0].usd;
      rnd_ok += mrec[k].rnd_ok;
      mask_ok += mrec[k].mask_ok;
    }
  printf ("    makecontext_e2k in main %s; %d resumptions, A (swapcontext) "
          "and B (setcontext to a getcontext point) in turn; failed "
          "switches A %d B %d\n", err (r), NMIG + 1, sw_fail[1], sw_fail[2]);
  printf ("    in the context: thread = the resuming thread %d/%d, TLS = "
          "its TLS %d/%d, same psp/pcsp bases %d/%d (%#lx, %#lx; uc_stack "
          "%p), same usd.ptr %d/%d\n", tid_ok, NMIG + 1, tls_ok, NMIG + 1,
          psp_same, NMIG + 1, mrec[0].psp, mrec[0].pcsp, (void *) stk[0],
          usd_same, NMIG + 1);
  printf ("    the context's own rounding (toward zero) %d/%d, its own mask "
          "(USR2 only) %d/%d; homes with their own rounding and mask "
          "(A upward + HUP, B downward + TERM) wrong %d/%d times; work "
          "loop of %ld iterations %s\n", rnd_ok, NMIG + 1, mask_ok,
          NMIG + 1, home_bad[1], home_bad[2], mig_n,
          mig_ok ? "intact" : "CORRUPTED");
  printf ("    &me (TLS) taken in B before a switch == &me taken after A "
          "resumed it: %s; a noinline function there returns %s TLS\n",
          lcc_same ? "yes (folded)" : "no", tls_who (mrec[1].tls_folded));
  r = CHECK (freecontext_e2k (&uc_cs));
  printf ("    freecontext_e2k of the suspended context from main: %s\n",
          err (r));
}

/* 3.  Hardware stacks grown in A, unwound in B.  */
#define DEPTH 3000
static volatile int do_switch;
static struct obs o3a, o3b;
static long rec_res;

static long __attribute__ ((noinline))
rec (int n, long x)
{
  if (n == 0)
    {
      if (do_switch)
        {
          observe (&o3a);
          suspend ();
          observe (&o3b);
        }
      return x;
    }
  long y = rec (n - 1, x * 3 + 1);
  return y ^ (x + n);
}

static struct obs o3s;

static void
c3_fn (void)
{
  observe (&o3s);
  rec_res = rec (DEPTH, 5);
}

static void
c3_a (void)
{
  swapcontext (&home[1], &uc_c);
  put (&turn, 2);
}

static void
c3_b (void)
{
  wait_eq (&turn, 2);
  r_b = CHECK (swapcontext (&home[2], &uc_cs));
}

static void
case_hwgrow (void)
{
  do_switch = 0;
  long want = rec (DEPTH, 5);
  do_switch = 1;
  setup (&uc_c, stk[0], &home[2]);
  long r = CHECK (makecontext_e2k (&uc_c, c3_fn, 0));
  int ps0 = maps_count ("[procedure stack]"), cs0 = maps_count ("[chain stack]");
  turn = 1;
  pthread_t ta = spawn (1, c3_a);
  pthread_t tb = spawn (2, c3_b);
  pthread_join (ta, NULL);
  pthread_join (tb, NULL);
  printf ("    makecontext_e2k %s; %d nested calls started in A, suspended at "
          "the deepest one, resumed by B (%s) and unwound there\n", err (r),
          DEPTH, err (r_b));
  print_obs ("at the start, in A:", &o3s);
  print_obs ("at the bottom, in A:", &o3a);
  print_obs ("at the bottom, resumed in B:", &o3b);
  printf ("    result %s; [procedure stack] maps %d -> %d, [chain stack] %d "
          "-> %d\n", rec_res == want ? "correct" : "WRONG", ps0,
          maps_count ("[procedure stack]"), cs0, maps_count ("[chain stack]"));
}

/* 4.  Signals.  */
static volatile int running, stop, hit;
static struct obs o_h, o_h2, o_c_after, o_home;
static unsigned long h_local;
static char altbuf[3][32 << 10] __attribute__ ((aligned (4096)));

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
h_obs (int sig, siginfo_t *si, void *uc)
{
  (void) sig, (void) si, (void) uc;
  volatile char loc = 0;
  h_local = (unsigned long) &loc;
  observe (&o_h);
  put (&stop, 1);
}

static void
c4a_fn (void)
{
  observe (&o_ctx);
  put (&running, 1);
  while (!__atomic_load_n (&stop, __ATOMIC_ACQUIRE))
    ;
}

static void
c4a_b (void)
{
  r_b = CHECK (swapcontext (&home[2], &uc_c));
}

static void
case_sig_directed (void)
{
  handler (SIGUSR1, h_obs, 0);
  setup (&uc_c, stk[0], &home[2]);
  makecontext_e2k (&uc_c, c4a_fn, 0);
  pthread_t tb = spawn (2, c4a_b);
  wait_eq (&running, 1);
  pthread_kill (tb, SIGUSR1);
  pthread_join (tb, NULL);
  printf ("    a context made in main runs in B; main sends USR1 to B "
          "(pthread_kill); B's switch %s\n", err (r_b));
  print_obs ("the context:", &o_ctx);
  print_obs ("the handler:", &o_h);
  printf ("    the handler's local %#lx is %s\n", h_local,
          h_local >= (unsigned long) stk[0]
          && h_local < (unsigned long) stk[0] + STK ? "in the context's "
          "uc_stack" : "elsewhere");
}

/* A context that takes USR2 in A; the handler suspends it; B resumes
   the handler, which returns (sigreturn) in B.  */
static int onstack_mode;
static ucontext_t uc_hs;
static volatile int hpattern_bad, nested_ran;
static char sigstk_a[64], sigstk_b[64], sigstk_end[64];
static stack_t alt_b_after;

static const char *
sigstacks (char *out)
{
  int n = maps_count ("[signal stack]");
  sprintf (out, "%d", n);
  return out;
}

static void
h_nested (int sig, siginfo_t *si, void *uc)
{
  (void) sig, (void) si, (void) uc;
  volatile char buf[8192];
  for (int i = 0; i < (int) sizeof buf; i++)
    buf[i] = 0x33;
  nested_ran++;
}

static void
h_out (int sig, siginfo_t *si, void *uc)
{
  (void) sig, (void) si, (void) uc;
  volatile char buf[2048];
  for (int i = 0; i < (int) sizeof buf; i++)
    buf[i] = (char) (i * 7);
  h_local = (unsigned long) buf;
  observe (&o_h);
  swapcontext (&uc_hs, &home[cur_me ()]);
  observe (&o_h2);
  for (int i = 0; i < (int) sizeof buf; i++)
    hpattern_bad += buf[i] != (char) (i * 7);
}

static long c4_n;
static int c4_ok;

static void
c4b_fn (void)
{
  set_mask (SIGTERM, 0);
  fesetround (FE_TOWARDZERO);
  long n = 0, a = 1, b = 7;
  for (int j = 0; j < 2000; j++)
    {
      if (j == 1000)
        pthread_kill (pthread_self (), SIGUSR2);
      n++;
      a = a * 3 + 1;
      b += a ^ n;
    }
  c4_n = n;
  c4_ok = work_ok (n, a, b);
  observe (&o_c_after);
}

static void
c4b_a (void)
{
  if (onstack_mode)
    {
      stack_t ss = { .ss_sp = altbuf[1], .ss_size = sizeof altbuf[1] };
      sigaltstack (&ss, NULL);
    }
  fesetround (FE_UPWARD);
  set_mask (SIGHUP, 0);
  swapcontext (&home[1], &uc_c);
  sigstacks (sigstk_a);
  if (onstack_mode == 2)
    {
      /* A takes another signal on its alternate stack while the first
         handler's frame, suspended, is still there.  */
      handler (SIGUSR1, h_nested, SA_ONSTACK);
      raise (SIGUSR1);
    }
  put (&turn, 2);
}

static void
c4b_b (void)
{
  fesetround (FE_DOWNWARD);
  set_mask (SIGALRM, 0);
  wait_eq (&turn, 2);
  r_b = CHECK (swapcontext (&home[2], &uc_hs));
  observe (&o_home);
  sigaltstack (NULL, &alt_b_after);
  sigstacks (sigstk_b);
}

static void
case_sig_migrate (void)
{
  handler (SIGUSR2, h_out, onstack_mode ? SA_ONSTACK : 0);
  setup (&uc_c, stk[0], &home[2]);
  makecontext_e2k (&uc_c, c4b_fn, 0);
  turn = 1;
  pthread_t ta = spawn (1, c4b_a);
  pthread_t tb = spawn (2, c4b_b);
  pthread_join (ta, NULL);
  pthread_join (tb, NULL);
  sigstacks (sigstk_end);
  printf ("    the context (mask TERM, toward zero) takes USR2 in A (home "
          "mask HUP, upward); the handler%s suspends it; B (home mask ALRM, "
          "downward) resumes the handler (%s), which returns there\n",
          onstack_mode ? " (SA_ONSTACK, A's alternate stack)" : "",
          err (r_b));
  print_obs ("handler, in A:", &o_h);
  print_obs ("handler, resumed in B:", &o_h2);
  print_obs ("the context after sigreturn:", &o_c_after);
  print_obs ("B back home:", &o_home);
  printf ("    the handler's local %#lx is %s; its 2 KiB pattern %s after "
          "the resumption%s; work loop of %ld iterations %s\n", h_local,
          h_local >= (unsigned long) stk[0]
          && h_local < (unsigned long) stk[0] + STK ? "in the context's "
          "uc_stack" : h_local >= (unsigned long) altbuf[1]
          && h_local < (unsigned long) altbuf[1] + sizeof altbuf[1]
          ? "in A's alternate stack" : "elsewhere",
          hpattern_bad ? "CHANGED" : "intact",
          onstack_mode == 2 ? (nested_ran ? " (A ran a second SA_ONSTACK "
                               "handler in between)"
                               : " (A's second handler did not run)")
          : "", c4_n, c4_ok ? "intact" : "CORRUPTED");
  printf ("    [signal stack] maps: after the handler suspended in A %s, "
          "after B's return home %s, after both threads ended %s\n",
          sigstk_a, sigstk_b, sigstk_end);
  if (onstack_mode)
    printf ("    B's sigaltstack after the handler returned there: ss_sp "
            "%p (A's buffer %p) size %zu flags %#x\n", alt_b_after.ss_sp,
            (void *) altbuf[1], alt_b_after.ss_size, alt_b_after.ss_flags);
}

static void case_sig_migrate0 (void) { onstack_mode = 0; case_sig_migrate (); }
static void case_sig_migrate1 (void) { onstack_mode = 1; case_sig_migrate (); }
static void case_sig_migrate2 (void) { onstack_mode = 2; case_sig_migrate (); }

/* 5 and 6.  freecontext_e2k / makecontext_e2k against a context that
   runs in B.  */
static volatile int go_on, exit_in_ctx, remade_ran;
static struct obs o5_ctx;
static volatile long r5_call;

static void
c5_fn (void)
{
  observe (&o5_ctx);
  put (&running, 1);
  wait_eq (&go_on, 1);
  if (exit_in_ctx)
    syscall (SYS_exit, 0);  /* ends thread B inside the context */
  char b[64];
  r5_call = snprintf (b, sizeof b, "%s %d", "still running", 42);
  suspend ();
  r5_call = -100;   /* must not run */
}

static void
c5_b (void)
{
  r_b = CHECK (swapcontext (&home[2], &uc_c));
}

/* Four fresh contexts, each started and suspended: their hardware
   stack bases.  Then each in turn goes 150 + 40 i calls deep and
   suspends at the bottom; then each in turn is resumed, unwinds and
   checks its result (values live across every call, so they are in the
   procedure stack, and the returns read the chain stack).  */
static void barrier2 (void);
static int use_threads;   /* check with two threads instead */
static unsigned long ak_psp[4], ak_pcsp[4];
static volatile int ak_i, ak_sw;
static long ak_res[4];

static long __attribute__ ((noinline))
recv (int n, long x, int i)
{
  if (n == 0)
    {
      if (ak_sw)
        swapcontext (&uc_k[i], &home[0]);
      return x;
    }
  long y = recv (n - 1, x * 5 + i + 1, i);
  return y ^ (x + n);
}

static void
ak_fn (void)
{
  int i = ak_i;
  ak_psp[i] = rd_psp ();
  ak_pcsp[i] = rd_pcsp ();
  swapcontext (&uc_k[i], &home[0]);
  ak_res[i] = recv (150 + 40 * i, 11 + i, i);
  swapcontext (&uc_k[i], &home[0]);
}

static void
alias_check (void)
{
  static ucontext_t m[4];
  long want[4];
  ak_sw = 0;
  for (int i = 0; i < 4; i++)
    want[i] = recv (150 + 40 * i, 11 + i, i);
  ak_sw = 1;
  for (int i = 0; i < 4; i++)
    {
      setup (&m[i], stk[4 + i], &home[0]);
      makecontext_e2k (&m[i], ak_fn, 0);
      ak_i = i;
      swapcontext (&home[0], &m[i]);
    }
  int same = 0;
  for (int i = 0; i < 4; i++)
    for (int j = i + 1; j < 4; j++)
      same += ak_psp[i] == ak_psp[j] || ak_pcsp[i] == ak_pcsp[j];
  printf ("    four new contexts, started and suspended in turn: psp.base");
  for (int i = 0; i < 4; i++)
    printf (" %#lx", ak_psp[i]);
  printf ("; pcsp.base");
  for (int i = 0; i < 4; i++)
    printf (" %#lx", ak_pcsp[i]);
  printf ("\n    pairs sharing a hardware stack: %d; the old context's "
          "(psp.base %#lx) reused by %d of them\n", same, o5_ctx.psp,
          (ak_psp[0] == o5_ctx.psp) + (ak_psp[1] == o5_ctx.psp)
          + (ak_psp[2] == o5_ctx.psp) + (ak_psp[3] == o5_ctx.psp));
  handler_crash ();
  fflush (stdout);
  for (int i = 0; i < 4; i++)
    swapcontext (&home[0], &uc_k[i]);     /* down 150 + 40 i calls */
  for (int i = 0; i < 4; i++)
    swapcontext (&home[0], &uc_k[i]);     /* back up */
  printf ("    each went 150 + 40 i calls deep and suspended, in turn, then "
          "unwound, in turn: results");
  for (int i = 0; i < 4; i++)
    printf (" %s", ak_res[i] == want[i] ? "correct" : "WRONG");
  printf ("\n");
}

/* Two new threads at once, each recursing 2,000 calls deep 20 times:
   do they get the same hardware stacks, and do their results survive?  */
static unsigned long ta_psp[8], ta_pcsp[8];
static volatile int ta_bad[8];

static void
ta_thr (void)
{
  ta_psp[me] = rd_psp ();
  ta_pcsp[me] = rd_pcsp ();
  barrier2 ();
  long want = rec (2000, 5 + me);
  for (int k = 0; k < 20; k++)
    {
      if (rec (2000, 5 + me) != want)
        ta_bad[me]++;
      sched_yield ();
    }
}

static void
thread_alias_check (void)
{
  do_switch = 0;
  handler_crash ();
  fflush (stdout);
  pthread_t t3 = spawn (3, ta_thr);
  pthread_t t4 = spawn (4, ta_thr);
  pthread_join (t3, NULL);
  pthread_join (t4, NULL);
  printf ("    two new threads T3, T4 at once: psp.base %#lx %#lx, pcsp.base "
          "%#lx %#lx (%s); 20 recursions 2,000 calls deep each, wrong "
          "results %d and %d\n", ta_psp[3], ta_psp[4], ta_pcsp[3], ta_pcsp[4],
          ta_psp[3] == ta_psp[4] || ta_pcsp[3] == ta_pcsp[4] ? "SHARED"
          : "distinct", ta_bad[3], ta_bad[4]);
}

static int mode5;   /* 0 free, then switch away; 1 free, then exit;
                       2 exit without free (control);
                       3 remake, then switch away; 4 remake, then exit */

static int mode8;

static void
f_new (void)
{
  remade_ran++;
  if (mode8 == 4)
    {
      printf ("      in the new context: psp.base %#lx pcsp.base %#lx\n",
              rd_psp (), rd_pcsp ());
      fflush (stdout);
    }
}

static void
case_busy_other (void)
{
  exit_in_ctx = mode5 == 1 || mode5 == 2 || mode5 == 4;
  setup (&uc_c, stk[0], &home[2]);
  makecontext_e2k (&uc_c, c5_fn, 0);
  pthread_t tb = spawn (2, c5_b);
  wait_eq (&running, 1);
  long r = 0;
  int ps0 = maps_count ("[procedure stack]");
  if (mode5 == 0 || mode5 == 1)
    {
      r = CHECK (freecontext_e2k (&uc_c));
      printf ("    freecontext_e2k from main of the context running in B: "
              "%s\n", err (r));
    }
  else if (mode5 == 3 || mode5 == 4)
    {
      setup (&uc_n, stk[0], &home[0]);
      r = CHECK (makecontext_e2k (&uc_n, f_new, 0));
      printf ("    makecontext_e2k from main on the stack (key) of the "
              "context running in B: %s\n", err (r));
    }
  put (&go_on, 1);
  pthread_join (tb, NULL);
  usleep (300000);
  if (!exit_in_ctx)
    {
      printf ("    the context went on in B (a call returned %ld) and "
              "switched back to B's state (%s)\n", (long) r5_call, err (r_b));
      printf ("    its psp.base %#lx still mapped 300 ms later: %s; "
              "[procedure stack] maps %d -> %d\n", o5_ctx.psp,
              mapped (o5_ctx.psp) ? "yes" : "no", ps0,
              maps_count ("[procedure stack]"));
      if (mode5 == 0)
        {
          long s = CHECK (swapcontext (&home[0], &uc_cs));
          long f = CHECK (freecontext_e2k (&uc_c));
          printf ("    main switching to the state it saved: %s; "
                  "freecontext_e2k again: %s\n", err (s), err (f));
        }
      else
        {
          long s = CHECK (swapcontext (&home[0], &uc_n));
          printf ("    main switching to the new context: %s, its function "
                  "ran %d time(s)\n", err (s), remade_ran);
          printf ("    main switching to the old context's saved state "
                  "(its key now names the new one):\n");
          fflush (stdout);
          s = CHECK (swapcontext (&home[0], &uc_cs));
          printf ("    ... returned %s, r5_call %ld\n", err (s),
                  (long) r5_call);
        }
    }
  else
    {
      printf ("    B ended (exit system call) inside the context%s\n",
              mode5 == 2 ? " (not freed: the control case)" : "");
      long f = CHECK (freecontext_e2k (&uc_c));
      printf ("    freecontext_e2k of it now: %s; its psp.base %#lx mapped: "
              "%s; [procedure stack] maps %d -> %d\n", err (f), o5_ctx.psp,
              mapped (o5_ctx.psp) ? "yes" : "no", ps0,
              maps_count ("[procedure stack]"));
      if (use_threads)
        thread_alias_check ();
      else
        alias_check ();
    }
}

/* Baselines for the hardware stack check: nothing before it (0); B
   runs a made context to its end and exits normally (1); B exits
   without touching contexts (2); B's only call is a setcontext to its
   own getcontext point (3).  */
static void
c5_short (void)
{
  observe (&o5_ctx);
}

static void
t_plain (void)
{
  o5_ctx.psp = rd_psp ();
}

static void
t_selfset (void)
{
  static volatile int pass;
  ucontext_t g;
  o5_ctx.psp = rd_psp ();
  pass = 0;
  getcontext (&g);
  if (pass++ == 0)
    r_b = CHECK (p_setcontext (&g));
}

static void
case_alias_base (int kind)
{
  if (kind == 1)
    {
      setup (&uc_c, stk[0], &home[2]);
      makecontext_e2k (&uc_c, c5_short, 0);
      pthread_t tb = spawn (2, c5_b);
      pthread_join (tb, NULL);
      long f = CHECK (freecontext_e2k (&uc_c));
      usleep (300000);
      printf ("    B ran a made context to its end, returned home and "
              "exited; freecontext_e2k of the context: %s\n", err (f));
    }
  else if (kind == 2 || kind == 3)
    {
      pthread_t tb = spawn (2, kind == 2 ? t_plain : t_selfset);
      pthread_join (tb, NULL);
      usleep (300000);
      printf ("    B (psp.base %#lx) %s, and exited\n", o5_ctx.psp,
              kind == 2 ? "did nothing with contexts"
              : "did getcontext, then setcontext to it");
    }
  else
    printf ("    no thread, no context before the check\n");
  if (use_threads)
    thread_alias_check ();
  else
    alias_check ();
}

static void case_base0 (void) { case_alias_base (0); }
static void case_base1 (void) { case_alias_base (1); }
static void case_base2 (void) { case_alias_base (2); }
static void case_base3 (void) { case_alias_base (3); }

static void case_busy0 (void) { mode5 = 0; case_busy_other (); }
static void case_busy1 (void) { mode5 = 1; case_busy_other (); }
static void case_busy2 (void) { mode5 = 2; case_busy_other (); }
static void case_busy3 (void) { mode5 = 3; case_busy_other (); }
static void case_busy4 (void) { mode5 = 4; case_busy_other (); }
static void case_busy1t (void) { use_threads = 1; case_busy1 (); }
static void case_busy2t (void) { use_threads = 1; case_busy2 (); }
static void case_base0t (void) { use_threads = 1; case_base0 (); }
static void case_base1t (void) { use_threads = 1; case_base1 (); }
static void case_base3t (void) { use_threads = 1; case_base3 (); }

/* 7.  Two threads switching to one suspended context at once.  */
static long nrace = 20000;
static volatile int inside, overlap, entries, fp_stale, race_end;
static volatile int bar_cnt, bar_gen;
static long succ[3], busy[3], other[3], home_fp_bad[3], both_in[1];
static volatile int round_in;

static void
barrier2 (void)
{
  int g = __atomic_load_n (&bar_gen, __ATOMIC_ACQUIRE);
  if (__atomic_add_fetch (&bar_cnt, 1, __ATOMIC_ACQ_REL) == 2)
    {
      __atomic_store_n (&bar_cnt, 0, __ATOMIC_RELAXED);
      __atomic_store_n (&bar_gen, g + 1, __ATOMIC_RELEASE);
    }
  else
    for (long i = 0; __atomic_load_n (&bar_gen, __ATOMIC_ACQUIRE) == g; i++)
      if ((i & 0xffff) == 0xffff)
        sched_yield ();
}

static int race_depth, race_jitter;
static volatile long deep_resumes;

/* The suspension one call deeper.  */
static void __attribute__ ((noinline))
suspend2 (void)
{
  suspend ();
  deep_resumes++;
}

static void
race_fn (void)
{
  for (int k = 0;; k++)
    {
      if (__atomic_exchange_n (&inside, 1, __ATOMIC_ACQ_REL))
        __atomic_add_fetch (&overlap, 1, __ATOMIC_RELAXED);
      __atomic_add_fetch (&entries, 1, __ATOMIC_RELAXED);
      __atomic_add_fetch (&round_in, 1, __ATOMIC_RELAXED);
      for (volatile int j = 0; j < 300; j++)
        ;
      int want = k & 1 ? FE_DOWNWARD : FE_TOWARDZERO;
      fesetround (want);
      __atomic_store_n (&inside, 0, __ATOMIC_RELEASE);
      if (race_depth && (k & 1))
        suspend2 ();
      else
        suspend ();
      if (fegetround () != want)
        __atomic_add_fetch (&fp_stale, 1, __ATOMIC_RELAXED);
    }
}

static void
race_thr (void)
{
  int my = me;
  int own = my == 1 ? FE_UPWARD : FE_TONEAREST;
  fesetround (own);
  unsigned long rs = 0x9e3779b97f4a7c15UL * (unsigned long) (my + 1);
  for (long i = 0; i < nrace; i++)
    {
      barrier2 ();
      if (race_jitter)
        {
          /* Spread the two arrivals over the context's run.  */
          rs ^= rs << 13;
          rs ^= rs >> 7;
          rs ^= rs << 17;
          for (volatile int d = (int) (rs & 4095); d > 0; d--)
            ;
        }
      errno = 0;
      long r = swapcontext (&home[my], &uc_cs);
      if (r == 0)
        succ[my]++;
      else if (errno == EBUSY)
        busy[my]++;
      else
        other[my]++;
      if (fegetround () != own)
        home_fp_bad[my]++;
      barrier2 ();
      if (my == 1)
        {
          if (round_in == 2)
            both_in[0]++;
          round_in = 0;
          if (race_depth && (i + 1) % 50000 == 0)
            printf ("      after %ld rounds: both ran it in %ld, stale "
                    "rounding %d\n", i + 1, both_in[0], fp_stale);
        }
      barrier2 ();
    }
}

static void
case_race (void)
{
  if (race_depth)
    {
      handler_crash ();
      printf ("    the context suspends alternately in a function and one "
              "call deeper\n");
    }
  setup (&uc_c, stk[0], &home[0]);
  makecontext_e2k (&uc_c, race_fn, 0);
  swapcontext (&home[0], &uc_c);          /* now suspended in uc_cs */
  round_in = 0;
  pthread_t ta = spawn (1, race_thr);
  pthread_t tb = spawn (2, race_thr);
  pthread_join (ta, NULL);
  pthread_join (tb, NULL);
  printf ("    %ld rounds: A and B meet at a barrier, then both "
          "swapcontext to the same suspended context (it runs ~300 "
          "loop steps, then switches back to the thread running it)\n",
          nrace);
  printf ("    A: %ld ran it, %ld EBUSY, %ld other; B: %ld ran it, %ld "
          "EBUSY, %ld other; rounds in which both ran it (one after the "
          "other) %ld\n", succ[1], busy[1], other[1], succ[2], busy[2],
          other[2], both_in[0]);
  printf ("    entries into the context %d (= 1 + successes: %s); "
          "overlapping entries %d; resumed with another rounding than it "
          "left with (a stale saved state) %d; homes with a wrong rounding "
          "A %ld B %ld\n", entries,
          entries == 1 + succ[1] + succ[2] ? "yes" : "NO", overlap, fp_stale,
          home_fp_bad[1], home_fp_bad[2]);
}

/* 8.  Threads that exit.  */
static volatile int t_exit_now, ctx_step;
static pthread_t t_maker;
static volatile long t2_r, t2_key, t1_key, t3_r, t3_key;

static void
c8_fn (void)
{
  ctx_step = 1;
  observe (&o_ctx);
  if (mode8 == 3)
    syscall (SYS_exit, 0);
  if (mode8 == 4)
    pthread_exit (NULL);
  if (mode8 == 2)
    {
      put (&t_exit_now, 1);
      pthread_join (t_maker, NULL);
      observe (&o_h);
      ctx_step = 2;
      return;
    }
  suspend ();
  ctx_step = 2;
  observe (&o_h);
}

static void
t8_maker (void)
{
  ucontext_t g;
  getcontext (&g);
  t1_key = g.uc_mcontext.nr_TIRs;
  setup (&uc_c, stk[0], mode8 == 1 ? &home[1] : &home[0]);
  makecontext_e2k (&uc_c, c8_fn, 0);
  if (mode8 == 2)
    {
      put (&running, 1);
      wait_eq (&t_exit_now, 1);
      return;
    }
  r_b = CHECK (swapcontext (&home[1], &uc_c));
}

static void
t8_next (void)
{
  ucontext_t g;
  getcontext (&g);
  if (me == 2)
    t2_key = g.uc_mcontext.nr_TIRs;
  else
    t3_key = g.uc_mcontext.nr_TIRs;
  setup (&uc_k[me], stk[me], &home[me]);
  long m = CHECK (makecontext_e2k (&uc_k[me], f_new, 0));
  if (mode8 == 4)
    {
      printf ("      thread %d: key %#lx, psp.base %#lx pcsp.base %#lx, "
              "makecontext_e2k %s, switching\n", me,
              (long) g.uc_mcontext.nr_TIRs, rd_psp (), rd_pcsp (), err (m));
      fflush (stdout);
    }
  long r = CHECK (swapcontext (&home[me], &uc_k[me]));
  if (me == 2)
    t2_r = r;
  else
    t3_r = r;
}

static void
case_exit (void)
{
  if (mode8 == 2)
    {
      t_maker = spawn (1, t8_maker);
      wait_eq (&running, 1);
      long s = CHECK (swapcontext (&home[0], &uc_c));
      printf ("    A made a context; main runs it; inside it A is told to "
              "exit and joined; the context went on (step %d) and returned "
              "to main through uc_link: swap %s\n", ctx_step, err (s));
      print_obs ("in the context before A exited:", &o_ctx);
      print_obs ("in the context after:", &o_h);
      return;
    }
  pthread_t t = spawn (1, t8_maker);
  pthread_join (t, NULL);
  usleep (100000);
  if (mode8 == 0 || mode8 == 1)
    {
      printf ("    A made a context (uc_link = %s), ran it until it "
              "suspended (%s), and returned (exited)\n",
              mode8 ? "A's saved state" : "main's saved state", err (r_b));
      fflush (stdout);
      long s = CHECK (swapcontext (&home[0], &uc_cs));
      printf ("    main resumed it: %s, step %d\n", err (s), ctx_step);
      print_obs ("the context, first in A:", &o_ctx);
      print_obs ("the context, resumed in main:", &o_h);
      return;
    }
  printf ("    A switched into a made context and %s inside it; "
          "pthread_join returned\n", mode8 == 3 ? "made the exit system call"
          : "called pthread_exit");
  long f = CHECK (freecontext_e2k (&uc_c));
  printf ("    freecontext_e2k of that context: %s; A's key (its stack "
          "top) %#lx\n", err (f), (long) t1_key);
  pthread_t t2 = spawn (2, t8_next);
  pthread_join (t2, NULL);
  printf ("    the next thread (B, key %#lx%s): its first swapcontext %s\n",
          (long) t2_key, t2_key == t1_key ? ", the same stack" : "",
          err (t2_r));
  f = CHECK (freecontext_e2k (&home[1]));
  printf ("    main: freecontext_e2k of A's own context (named by the "
          "state A saved): %s\n", err (f));
  pthread_t t3 = spawn (3, t8_next);
  pthread_join (t3, NULL);
  printf ("    the thread after that (T3, key %#lx%s): its first "
          "swapcontext %s\n", (long) t3_key,
          t3_key == t1_key ? ", the same stack" : "", err (t3_r));
}

static void case_exit0 (void) { mode8 = 0; case_exit (); }
static void case_exit1 (void) { mode8 = 1; case_exit (); }
static void case_exit2 (void) { mode8 = 2; case_exit (); }
static void case_exit3 (void) { mode8 = 3; case_exit (); }
static void case_exit4 (void) { mode8 = 4; handler_crash (); case_exit (); }

/* 9.  fork.  */
static volatile int fk_x;

static void
c9a_fn (void)
{
  volatile int local = 1;
  suspend ();
  local++;
  fk_x = local * 10;
  suspend ();
}

static void
case_fork_suspended (void)
{
  setup (&uc_c, stk[0], &home[0]);
  makecontext_e2k (&uc_c, c9a_fn, 0);
  swapcontext (&home[0], &uc_c);
  fflush (stdout);
  pid_t p = fork ();
  if (p == 0)
    {
      long s = CHECK (swapcontext (&home[0], &uc_cs));
      long f = CHECK (freecontext_e2k (&uc_cs));
      printf ("    child: resumed the suspended context: %s, it computed "
              "%d; freed it: %s\n", err (s), fk_x, err (f));
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (p, &st, 0);
  fk_x = 0;
  long s = CHECK (swapcontext (&home[0], &uc_cs));
  printf ("    parent after the child: resumed it: %s, it computed %d (its "
          "local was 1 again: the child's run and free did not touch it)\n",
          err (s), fk_x);
}

static volatile long fk_pid_in, fk_pid_child;

static void
c9b_fn (void)
{
  fflush (stdout);
  pid_t p = fork ();
  if (p == 0)
    {
      fk_pid_child = getpid ();
      suspend ();     /* back to the child's main */
      return;
    }
  int st;
  waitpid (p, &st, 0);
  printf ("    parent: in the context after fork, child exit %d\n",
          WIFEXITED (st) ? WEXITSTATUS (st) : -1);
}

static void
case_fork_inside (void)
{
  setup (&uc_c, stk[0], &home[0]);
  makecontext_e2k (&uc_c, c9b_fn, 0);
  long s = CHECK (swapcontext (&home[0], &uc_c));
  if (getpid () == fk_pid_child)
    {
      printf ("    child: forked inside the context, which switched to the "
              "child's copy of main's state: %s\n", err (s));
      fflush (stdout);
      _exit (7);
    }
  printf ("    parent: back in main: %s\n", err (s));
}

static ucontext_t uc_ds;

static void
c9c_fn (void)
{
  swapcontext (&uc_ds, &home[2]);
  put (&running, 1);
  wait_eq (&go_on, 1);
}

static void
c9c_b (void)
{
  swapcontext (&home[2], &uc_c);       /* D suspends once (uc_ds) */
  r_b = CHECK (swapcontext (&home[2], &uc_ds));
}

static void
case_fork_other (void)
{
  setup (&uc_c, stk[0], &home[2]);
  makecontext_e2k (&uc_c, c9c_fn, 0);
  pthread_t tb = spawn (2, c9c_b);
  wait_eq (&running, 1);
  int ps = maps_count ("[procedure stack]");
  fflush (stdout);
  pid_t p = fork ();
  if (p == 0)
    {
      long s = CHECK (swapcontext (&home[0], &uc_ds));
      long f = CHECK (freecontext_e2k (&uc_ds));
      long fb = CHECK (freecontext_e2k (&home[2]));
      printf ("    child: switching to D's saved state %s; freecontext_e2k "
              "of D %s; of B's context (suspended at the fork) %s; "
              "[procedure stack] maps %d (parent %d)\n", err (s), err (f),
              err (fb), maps_count ("[procedure stack]"), ps);
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (p, &st, 0);
  printf ("    [child exit %d]\n", WIFEXITED (st) ? WEXITSTATUS (st) : -1);
  put (&go_on, 1);
  pthread_join (tb, NULL);
  printf ("    parent: context D was running in B at the fork; B finished "
          "it afterwards (%s)\n", err (r_b));
}

/* 10.  longjmp into another context.  */
static jmp_buf jb;
static volatile int jumped;

static void
c10_fn (void)
{
  if (setjmp (jb) == 0)
    {
      if (mode8 == 0)
        suspend ();
      put (&running, 1);
      while (!stop)
        ;
      return;
    }
  observe (&o_ctx);
  jumped = 1;
  swapcontext (&uc_cs, &home[cur_me ()]);
}

static void
t10_jumper (void)
{
  static volatile int pass;
  pass = 0;
  getcontext (&home[2]);
  if (pass++ == 0)
    longjmp (jb, 1);
  observe (&o_b);
}

static void
t10_runner (void)
{
  swapcontext (&home[1], &uc_c);
}

static void
case_longjmp (void)
{
  setup (&uc_c, stk[0], &home[0]);
  makecontext_e2k (&uc_c, c10_fn, 0);
  if (mode8 == 0)
    {
      swapcontext (&home[0], &uc_c);      /* setjmp, then suspended */
      pthread_t tb = spawn (2, t10_jumper);
      pthread_join (tb, NULL);
      printf ("    a context did setjmp and was suspended; B (on its own "
              "stack) longjmps to that jmp_buf: the jump %s\n",
              jumped ? "arrived" : "did not arrive");
      print_obs ("in the context after the jump:", &o_ctx);
      print_obs ("B back at its getcontext point:", &o_b);
      return;
    }
  pthread_t ta = spawn (1, t10_runner);
  wait_eq (&running, 1);
  printf ("    a context did setjmp and runs in A; main longjmps to that "
          "jmp_buf:\n");
  fflush (stdout);
  longjmp (jb, 1);
  pthread_join (ta, NULL);
}

static void case_longjmp0 (void) { mode8 = 0; case_longjmp (); }
static void case_longjmp1 (void) { mode8 = 1; case_longjmp (); }

/* 11.  A thread's own context resumed by another thread: A runs a made
   context C; B switches to the state A saved when it entered C, runs
   A's code there and switches back; C then ends through uc_link = that
   state, and A finishes in its own context.  */
static struct obs o11_a0, o11_b, o11_a1;
static volatile long r11_back, r11_a;

static void
c11_fn (void)
{
  put (&running, 1);
  wait_eq (&go_on, 1);
}

static void
c11_a (void)
{
  observe (&o11_a0);
  r11_a = CHECK (swapcontext (&home[1], &uc_c));
  if (cur_tid () != tids[1])
    {
      /* A's code, run by another thread.  */
      observe (&o11_b);
      r11_back = CHECK (swapcontext (&home[1], &home[2]));
    }
  observe (&o11_a1);
}

static void
c11_b (void)
{
  wait_eq (&running, 1);
  r_b = CHECK (swapcontext (&home[2], &home[1]));
}

static void
case_own_ctx (void)
{
  setup (&uc_c, stk[0], &home[1]);
  makecontext_e2k (&uc_c, c11_fn, 0);
  pthread_t ta = spawn (1, c11_a);
  pthread_t tb = spawn (2, c11_b);
  pthread_join (tb, NULL);
  put (&go_on, 1);
  pthread_join (ta, NULL);
  printf ("    A switched into a made context C; B switched to the state A "
          "saved (%s) and ran A's code; that code switched back to B (%s); "
          "C ended through uc_link = A's state; A's swapcontext returned "
          "%s\n", err (r_b), err (r11_back), err (r11_a));
  print_obs ("A, before entering C:", &o11_a0);
  print_obs ("A's code, run by B:", &o11_b);
  print_obs ("A's code, back in A:", &o11_a1);
}

/* 12.  The [signal stack] of a context that a thread exits in.  A
   thread exit unmaps the thread's current [signal stack] at once
   (process.c), and a context freed at the exit unmaps its own record
   of one later, from a kernel worker (proc_context.c).  After the exit
   a new thread T3 takes a signal (the kernel maps T3's [signal stack]
   in the same privileged area), waits 300 ms and takes another.
   Mode 0: a made context C takes a signal in A (gets a [signal stack])
           and suspends; B resumes C and makes the exit system call in it.
   Mode 1: A takes a signal in its own context, switches into C and
           back, and returns (exits in its own context).
   Mode 2 (control): A takes a signal and exits, no contexts.
   Mode 3 (control): as 0, but C suspends again to B's home and B exits
           normally (C keeps its [signal stack]).
   Mode 4: as 3, but C returns, through uc_link = the state A saved when
           it entered C; A has exited and B runs on A's old stack.
   Mode 5: as 1, but main waits 300 ms before it starts T3.
   CTXTHR_MAPS=1 prints the privileged mappings at each step.  */
static int mode12;
static unsigned long s12_lo, s12_hi, t3_lo, t3_hi;
static volatile int t3_mapped_later, t3_stage;
static volatile long k12_a, k12_b;

static int
sigstk_ranges (unsigned long *lo, unsigned long *hi, int max)
{
  int n = 0;
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    if (strstr (line, "[signal stack]") && n < max)
      {
        sscanf (line, "%lx-%lx", &lo[n], &hi[n]);
        n++;
      }
  if (f)
    fclose (f);
  return n;
}

static void
h12 (int sig)
{
  (void) sig;
}

/* Raise USR1 and return the [signal stack] range that appeared.  */
static void __attribute__ ((noinline))
raise_and_find (unsigned long *rlo, unsigned long *rhi)
{
  unsigned long blo[16], bhi[16], alo[16], ahi[16];
  int nb = sigstk_ranges (blo, bhi, 16);
  raise (SIGUSR1);
  int na = sigstk_ranges (alo, ahi, 16);
  *rlo = *rhi = 0;
  for (int i = 0; i < na; i++)
    {
      int old = 0;
      for (int j = 0; j < nb; j++)
        old |= alo[i] == blo[j] && ahi[i] == bhi[j];
      if (!old)
        {
          *rlo = alo[i];
          *rhi = ahi[i];
        }
    }
}

static long
own_key (void)
{
  ucontext_t g;
  getcontext (&g);
  return (long) g.uc_mcontext.nr_TIRs;
}

static void
c12_fn (void)
{
  if (mode12 == 0 || mode12 == 3 || mode12 == 4)
    {
      raise_and_find (&s12_lo, &s12_hi);
      suspend ();                 /* back to A */
      if (mode12 == 0)
        syscall (SYS_exit, 0);    /* B ends inside C */
      if (mode12 == 3)
        suspend ();               /* back to B's home */
    }
}

static void
t12_a (void)
{
  k12_a = own_key ();
  if (mode12 == 1 || mode12 == 2 || mode12 == 5)
    raise_and_find (&s12_lo, &s12_hi);
  if (mode12 != 2)
    swapcontext (&home[1], &uc_c);
}

static void
t12_b (void)
{
  k12_b = own_key ();
  r_b = CHECK (swapcontext (&home[2], &uc_cs));
}

/* The privileged mappings (hardware and signal stacks), when
   CTXTHR_MAPS is set.  */
static void
priv_maps (const char *when)
{
  if (!getenv ("CTXTHR_MAPS"))
    return;
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  printf ("      maps %s:\n", when);
  while (f && fgets (line, sizeof line, f))
    if (strstr (line, " stack]") && !strstr (line, "[stack]"))
      printf ("        %s", line);
  if (f)
    fclose (f);
}

static void
t12_t3 (void)
{
  raise_and_find (&t3_lo, &t3_hi);
  t3_stage = 1;
  priv_maps ("after T3's first signal");
  usleep (300000);
  t3_stage = 2;
  t3_mapped_later = t3_lo && mapped (t3_lo);
  priv_maps ("300 ms later");
  printf ("    T3's first signal mapped its [signal stack] at %#lx-%#lx (%s); "
          "300 ms later that is %s; T3 takes a second signal:\n", t3_lo,
          t3_hi, t3_lo == s12_lo ? "where the old one was" : "elsewhere",
          t3_mapped_later ? "mapped" : "NOT MAPPED");
  fflush (stdout);
  t3_stage = 3;
  raise (SIGUSR1);
  t3_stage = 4;
}

static void
case_sigstk_exit (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_handler = h12;
  sigaction (SIGUSR1, &sa, NULL);
  handler_crash ();
  setup (&uc_c, stk[0], &home[1]);
  makecontext_e2k (&uc_c, c12_fn, 0);
  static const char *const what[] = {
    "C took a signal in A and suspended; B resumed C and made the exit "
    "system call in it",
    "A took a signal in its own context, switched into C and back, and "
    "exited",
    "A took a signal and exited (no contexts)",
    "C took a signal in A and suspended; B resumed C, which suspended to "
    "B's home; B exited",
    "C took a signal in A and suspended; A exited; B resumed C, which "
    "returned through uc_link = A's saved state",
    "as the second case, then main waits 300 ms before starting T3" };
  printf ("    %s\n", what[mode12]);
  fflush (stdout);
  pthread_t ta = spawn (1, t12_a);
  pthread_join (ta, NULL);
  if (mode12 == 0 || mode12 == 3 || mode12 == 4)
    {
      pthread_t tb = spawn (2, t12_b);
      pthread_join (tb, NULL);
      if (mode12 == 4)
        printf ("    B's switch %s; A's key %#lx, B's key %#lx (%s)\n",
                err (r_b), k12_a, k12_b,
                k12_a == k12_b ? "the same stack" : "different");
    }
  if (mode12 == 5)
    usleep (300000);
  int still = mapped (s12_lo);
  printf ("    the context's or A's [signal stack] %#lx-%#lx: mapped after "
          "pthread_join: %s\n", s12_lo, s12_hi, still ? "yes" : "no");
  priv_maps ("after pthread_join");
  fflush (stdout);
  pthread_t t3 = spawn (3, t12_t3);
  pthread_join (t3, NULL);
  static const char *const stage[] = {
    "T3 ended inside its first signal", "T3 ended during its 300 ms wait",
    "T3 ended after its wait, before its second signal",
    "T3 ended inside its second signal", "it ran" };
  printf ("    ... %s\n", stage[t3_stage]);
}

static void case_sigstk0 (void) { mode12 = 0; case_sigstk_exit (); }
static void case_sigstk1 (void) { mode12 = 1; case_sigstk_exit (); }
static void case_sigstk2 (void) { mode12 = 2; case_sigstk_exit (); }
static void case_sigstk3 (void) { mode12 = 3; case_sigstk_exit (); }
static void case_sigstk4 (void) { mode12 = 4; case_sigstk_exit (); }
static void case_sigstk5 (void) { mode12 = 5; case_sigstk_exit (); }

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  int part = argc > 1 ? atoi (argv[1]) : 0;
  if (argc > 2)
    nrace = atol (argv[2]);
  if (!part || part == 1)
    {
      printf ("1. a context made in main, first run in B\n");
      in_child ("first run", case_first_run, 10);
    }
  if (!part || part == 2)
    {
      printf ("2. a context resumed in turn by A and B\n");
      in_child ("migrate", case_migrate, 60);
    }
  if (!part || part == 3)
    {
      printf ("3. hardware stacks grown in A, unwound in B\n");
      in_child ("hw stacks", case_hwgrow, 30);
    }
  if (!part || part == 4)
    {
      printf ("4. signals\n");
      in_child ("directed signal", case_sig_directed, 10);
      in_child ("handler resumed in another thread", case_sig_migrate0, 10);
      in_child ("the same, SA_ONSTACK", case_sig_migrate1, 10);
      in_child ("the same, SA_ONSTACK, A takes another signal",
                case_sig_migrate2, 10);
    }
  if (!part || part == 5)
    {
      printf ("5. freecontext_e2k of a context running in another thread\n");
      in_child ("free, then it switches away", case_busy0, 10);
      in_child ("free, then its thread exits in it", case_busy1, 10);
      in_child ("its thread exits in it, not freed", case_busy2, 10);
      in_child ("baseline: nothing", case_base0, 10);
      in_child ("baseline: B ran a context and exited normally", case_base1,
                10);
      in_child ("baseline: B exited, no contexts", case_base2, 10);
      in_child ("baseline: B did setcontext to its getcontext point",
                case_base3, 10);
      printf ("5t. the same, then two new threads at once\n");
      in_child ("free, then its thread exits in it", case_busy1t, 20);
      in_child ("its thread exits in it, not freed", case_busy2t, 20);
      in_child ("baseline: nothing", case_base0t, 20);
      in_child ("baseline: B ran a context and exited normally",
                case_base1t, 20);
      in_child ("baseline: B did setcontext to its getcontext point",
                case_base3t, 20);
    }
  if (!part || part == 6)
    {
      printf ("6. makecontext_e2k on the key of a context running in "
              "another thread\n");
      in_child ("remake, then it switches away", case_busy3, 10);
      in_child ("remake, then its thread exits in it", case_busy4, 10);
    }
  if (!part || part == 7)
    {
      printf ("7. two threads switching to one suspended context\n");
      in_child ("race", case_race, 120);
      race_depth = 1;
      if (nrace < 200000)
        nrace = 200000;
      in_child ("race, two suspension depths", case_race, 300);
      race_jitter = 1;
      printf ("    (each thread now waits 0 to 4,095 loop steps, at random, "
              "after the barrier)\n");
      in_child ("race, two depths, random delays", case_race, 300);
    }
  if (!part || part == 8)
    {
      printf ("8. threads that exit\n");
      in_child ("made by an exited thread", case_exit0, 10);
      in_child ("the same, uc_link = the exited thread's state", case_exit1,
                10);
      in_child ("maker exits while main runs it", case_exit2, 10);
      in_child ("exit system call inside a made context", case_exit3, 10);
      in_child ("pthread_exit inside a made context", case_exit4, 10);
    }
  if (!part || part == 9)
    {
      printf ("9. fork\n");
      in_child ("a suspended context", case_fork_suspended, 10);
      in_child ("fork inside a context", case_fork_inside, 10);
      in_child ("a context running in another thread", case_fork_other, 10);
    }
  if (!part || part == 10)
    {
      printf ("10. longjmp into another context\n");
      in_child ("suspended, from another thread", case_longjmp0, 10);
      in_child ("running in another thread", case_longjmp1, 10);
    }
  if (!part || part == 11)
    {
      printf ("11. a thread's own context resumed by another thread\n");
      in_child ("own context", case_own_ctx, 10);
    }
  if (!part || part == 12)
    {
      printf ("12. the [signal stack] of a context a thread exits in\n");
      in_child ("exit inside a context that has one", case_sigstk0, 10);
      in_child ("signal in the thread's own context, a switch, exit",
                case_sigstk1, 10);
      in_child ("control: signal, exit, no contexts", case_sigstk2, 10);
      in_child ("control: the context's thread exits at home", case_sigstk3,
                10);
      in_child ("uc_link = an exited thread's state, its stack reused",
                case_sigstk4, 10);
      in_child ("the second case, T3 started 300 ms later", case_sigstk5,
                10);
    }
  return 0;
}
