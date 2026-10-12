/* SPDX-License-Identifier: GPL-3.0-or-later */
/* User contexts and signal frames on MCST's Linux 6.1
   (spec/abi/kernel-interface.md, "User contexts" and "Signals"):

   1. makecontext_e2k, swapcontext, setcontext, getcontext and
      freecontext_e2k: arguments, uc_link, contexts named by their key,
      the results of freecontext_e2k, the hardware stacks of a new
      context, the FP control registers and the signal mask per context;
      then which uc_link glibc's helper and the kernel's trampoline read
      (low 32 bits, a copy, a second thread, an unknown key) and
      freecontext_e2k of the suspended main context.
   2. What sigreturn takes from a signal frame the handler edited
      (uc_sigmask, uc_extra.g/gtag, the FP fields, usd, cr1), for a
      signal sent by a system call and for a timer signal.  Never
      touches uc_mcontext.cr0_hi.
   3. What %g16..%g31 hold when a handler starts and what every global
      holds after a system call and the fast calls (cs_sysg), how many
      window pairs a system call may be handed (cs_sysw*), whether a
      non-speculative read or store of that "empty" value traps, and
      which tagged values puttagd and addd accept.
   4. The chain stack as access_hw_stacks shows it from a handler and
      from a context, get_backtrace and glibc's backtrace there.
   Risky cases run in forked children.  `./ctxsig 13` runs parts 1 and
   3 only; `./ctxsig cost` measures only what a switch costs, pinned to
   the last CPU, with %clkr (run it under the shared timing lock).

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   # or the machine's
     $L -O2 -rdynamic ctxsig.c ctxsig.s -o ctxsig -lpthread -lm
     ./ctxsig; ./ctxsig cost                           # on hardware  */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
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
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <asm/e2k_api.h>
#include <asm/e2k_syswork.h>
#include <asm/unistd.h>

long cs_clkr (void);
void cs_rd_fp (unsigned *);
long cs_gsig (volatile long *);
extern char cs_gsig_end[];
void cs_ghandler (int, siginfo_t *, void *);
void cs_ghandler_w (int, siginfo_t *, void *);
void cs_diag_g (long *);
void cs_empty_tag (long *, long);
void cs_sysg (long *, long);
void cs_sysw2 (void), cs_sysw4 (void), cs_sysw6 (void), cs_sysw8 (void);
void cs_sysf6 (void);
void cs_empty_add (long *);
void cs_empty_st (long *);
void cs_tagadd (long, long, long *);
void cs_dv_and (long *, long), cs_dv_or (long *, long);
void cs_diag_r (long *);
void cs_ghandler_add (int, siginfo_t *, void *);
void cs_ghandler_st (int, siginfo_t *, void *);

/* Filled by the assembly handlers.  */
long cs_gval[16];
int cs_gtag[16];
long cs_gns[4];

static void (*hc) (int, siginfo_t *, void *);

void
cs_handler_c (int sig, siginfo_t *si, void *uc)
{
  if (hc)
    hc (sig, si, uc);
}

#define M48 ((1UL << 48) - 1)

static unsigned long tramp_lo, tramp_hi;

static void
find_trampolines (void)
{
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    if (strstr (line, "[trampolines]"))
      sscanf (line, "%lx-%lx", &tramp_lo, &tramp_hi);
  if (f)
    fclose (f);
}

static const char *
sym (unsigned long ip)
{
  static char buf[8][160];
  static int k;
  char *b = buf[k++ & 7];
  Dl_info di;
  if (ip == 0)
    snprintf (b, 160, "0");
  else if (ip >= tramp_lo && ip < tramp_hi)
    snprintf (b, 160, "[trampolines]+%#lx", ip - tramp_lo);
  else if (ip == (unsigned long) -1)
    snprintf (b, 160, "-1");
  else if (dladdr ((void *) ip, &di) && di.dli_sname)
    snprintf (b, 160, "%s+%#lx", di.dli_sname,
              ip - (unsigned long) di.dli_saddr);
  else if (dladdr ((void *) ip, &di) && di.dli_fname)
    snprintf (b, 160, "%s+%#lx", strrchr (di.dli_fname, '/')
              ? strrchr (di.dli_fname, '/') + 1 : di.dli_fname,
              ip - (unsigned long) di.dli_fbase);
  else
    snprintf (b, 160, "?");
  return b;
}

/* Run FN in a child; print how it ended.  */
static void
in_child (const char *what, void (*fn) (void))
{
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      struct rlimit no = { 0, 0 };
      setrlimit (RLIMIT_CORE, &no);
      alarm (20);
      fn ();
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("    [%s: child killed by signal %d]\n", what, WTERMSIG (st));
  else
    printf ("    [%s: child exit %d]\n", what, WEXITSTATUS (st));
  fflush (stdout);
}

/* Lines of /proc/self/maps that mention a stack.  */
static char maps_old[64][200];
static int maps_n;

static void
maps_snap (const char *what, int print_diff)
{
  char now[64][200];
  int n = 0;
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    if (strstr (line, "stack") && n < 64)
      {
        line[strcspn (line, "\n")] = 0;
        snprintf (now[n++], 200, "%s", line);
      }
  if (f)
    fclose (f);
  if (print_diff)
    {
      printf ("    maps %s:\n", what);
      for (int i = 0; i < n; i++)
        {
          int old = 0;
          for (int j = 0; j < maps_n; j++)
            old |= !strcmp (now[i], maps_old[j]);
          unsigned long lo, hi;
          sscanf (now[i], "%lx-%lx", &lo, &hi);
          printf ("      %s %s  (%lu KiB)\n", old ? " " : "+", now[i],
                  (hi - lo) >> 10);
        }
      for (int j = 0; j < maps_n; j++)
        {
          int still = 0;
          for (int i = 0; i < n; i++)
            still |= !strcmp (now[i], maps_old[j]);
          if (!still)
            printf ("      - %s\n", maps_old[j]);
        }
    }
  memcpy (maps_old, now, sizeof now);
  maps_n = n;
}

/* 1. Contexts.  */
#define STK (64 << 10)
static char stk_a[STK] __attribute__ ((aligned (256)));
static char stk_b[STK] __attribute__ ((aligned (256)));
static char stk_c[STK] __attribute__ ((aligned (256)));
static ucontext_t uc_main, uc_a, uc_a2, uc_b, uc_c, uc_f, uc_g, uc_tmp;
static unsigned long got[16];
static volatile int got_n, ran, order_n;
static char order[16];

static unsigned long
V (int i)
{
  return 0x0101010101010101UL * (unsigned long) (i + 1);
}

static void
f0 (void)
{
  got_n = 0;
  ran++;
  order[order_n++] = 'z';
}

static void
f3 (long a0, long a1, long a2)
{
  got[0] = a0; got[1] = a1; got[2] = a2;
  got_n = 3;
  ran++;
}

static void
f8 (long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7)
{
  got[0] = a0; got[1] = a1; got[2] = a2; got[3] = a3;
  got[4] = a4; got[5] = a5; got[6] = a6; got[7] = a7;
  got_n = 8;
  ran++;
}

static void
f12 (long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7,
     long a8, long a9, long a10, long a11)
{
  got[0] = a0; got[1] = a1; got[2] = a2; got[3] = a3;
  got[4] = a4; got[5] = a5; got[6] = a6; got[7] = a7;
  got[8] = a8; got[9] = a9; got[10] = a10; got[11] = a11;
  got_n = 12;
  ran++;
}

static void
fa (void)
{
  order[order_n++] = 'A';
}

static void
fb (void)
{
  order[order_n++] = 'B';
}

static void
setup (ucontext_t *uc, char *stk, ucontext_t *link)
{
  memset (uc, 0, sizeof *uc);
  uc->uc_stack.ss_sp = stk;
  uc->uc_stack.ss_size = STK;
  uc->uc_link = link;
}

#define CHECK(call) \
  ({ errno = 0; long r_ = (call); \
     r_ < 0 ? -errno : r_; })

static const char *
err (long r)
{
  static char b[4][32];
  static int k;
  char *p = b[k++ & 3];
  if (r >= 0)
    snprintf (p, 32, "%ld", r);
  else
    snprintf (p, 32, "-1 (%s)", r == -ENOENT ? "ENOENT" : r == -EBUSY
              ? "EBUSY" : r == -ESRCH ? "ESRCH" : r == -EINVAL ? "EINVAL"
              : r == -EFAULT ? "EFAULT" : r == -ENOMEM ? "ENOMEM"
              : strerror ((int) -r));
  return p;
}

static void
args_case (void)
{
  static const int argcs[] = { 0, 3, 8, 12 };
  for (int k = 0; k < 4; k++)
    {
      int n = argcs[k];
      setup (&uc_a, stk_a, &uc_main);
      memset (got, 0, sizeof got);
      got_n = -1;
      ran = 0;
      long r;
      switch (n)
        {
        case 0:
          r = CHECK (makecontext_e2k (&uc_a, f0, 0));
          break;
        case 3:
          r = CHECK (makecontext_e2k (&uc_a, (void (*) (void)) f3, 3, V (0),
                                      V (1), V (2)));
          break;
        case 8:
          r = CHECK (makecontext_e2k (&uc_a, (void (*) (void)) f8, 8, V (0),
                                      V (1), V (2), V (3), V (4), V (5),
                                      V (6), V (7)));
          break;
        default:
          r = CHECK (makecontext_e2k (&uc_a, (void (*) (void)) f12, 12, V (0),
                                      V (1), V (2), V (3), V (4), V (5),
                                      V (6), V (7), V (8), V (9), V (10),
                                      V (11)));
          break;
        }
      if (k == 0)
        maps_snap ("after the first makecontext_e2k (+ = new)", 1);
      long s = CHECK (swapcontext (&uc_main, &uc_a));
      int bad = 0;
      for (int i = 0; i < n; i++)
        bad += got[i] != V (i);
      printf ("    argc %2d: makecontext_e2k %s, swapcontext %s, the "
              "function ran %d time(s), got %d arguments, %d wrong",
              n, err (r), err (s), ran, got_n, bad);
      if (n)
        printf (" (first %#lx, last %#lx)", got[0], got[n - 1]);
      printf ("\n");
      if (k == 0)
        printf ("    key (uc_mcontext.nr_TIRs) %#llx = uc_stack top %#lx\n",
                uc_a.uc_mcontext.nr_TIRs, (long) stk_a + STK);
    }
}

static void
link_case (void)
{
  setup (&uc_a, stk_a, &uc_b);
  setup (&uc_b, stk_b, &uc_main);
  order_n = 0;
  long r1 = CHECK (makecontext_e2k (&uc_a, fa, 0));
  long r2 = CHECK (makecontext_e2k (&uc_b, fb, 0));
  long s = CHECK (swapcontext (&uc_main, &uc_a));
  order[order_n++] = 'm';
  order[order_n] = 0;
  printf ("    A (uc_link B), B (uc_link main): makecontext %s %s, swap %s, "
          "order %s\n", err (r1), err (r2), err (s), order);
  /* uc_link changed after makecontext_e2k.  */
  setup (&uc_a, stk_a, &uc_main);
  order_n = 0;
  r1 = CHECK (makecontext_e2k (&uc_a, fa, 0));
  r2 = CHECK (makecontext_e2k (&uc_b, fb, 0));
  uc_a.uc_link = &uc_b;
  s = CHECK (swapcontext (&uc_main, &uc_a));
  order[order_n++] = 'm';
  order[order_n] = 0;
  printf ("    A made with uc_link main, then uc_link set to B before the "
          "switch: order %s\n", order);
  /* Started from a copy whose uc_link differs.  */
  setup (&uc_a, stk_a, &uc_main);
  order_n = 0;
  r1 = CHECK (makecontext_e2k (&uc_a, fa, 0));
  r2 = CHECK (makecontext_e2k (&uc_b, fb, 0));
  memcpy (&uc_c, &uc_a, sizeof uc_c);
  uc_c.uc_link = &uc_b;
  s = CHECK (swapcontext (&uc_main, &uc_c));
  order[order_n++] = 'm';
  order[order_n] = 0;
  printf ("    A made with uc_link main, started from a copy whose uc_link "
          "is B: swap %s, order %s\n", err (s), order);
}

static void
atexit_fn (void)
{
  printf ("      atexit handler ran\n");
  fflush (stdout);
}

static void
null_link_child (void)
{
  atexit (atexit_fn);
  setup (&uc_a, stk_a, NULL);
  long r = CHECK (makecontext_e2k (&uc_a, fa, 0));
  printf ("      makecontext_e2k %s; switching to a context with uc_link "
          "NULL\n", err (r));
  fflush (stdout);
  swapcontext (&uc_main, &uc_a);
  printf ("      swapcontext returned (not expected)\n");
  fflush (stdout);
  _exit (5);
}

/* uc_link changed between makecontext_e2k and the context's return.  */
static int relink_to_null;

static void
relink_child (void)
{
  atexit (atexit_fn);
  setup (&uc_a, stk_a, relink_to_null ? &uc_main : NULL);
  long r = CHECK (makecontext_e2k (&uc_a, fa, 0));
  uc_a.uc_link = relink_to_null ? NULL : &uc_main;
  printf ("      made with uc_link %s, then set to %s: makecontext_e2k %s\n",
          relink_to_null ? "main" : "NULL", relink_to_null ? "NULL" : "main",
          err (r));
  fflush (stdout);
  order_n = 0;
  swapcontext (&uc_main, &uc_a);
  order[order_n] = 0;
  printf ("      back in main (order %s)\n", order);
  fflush (stdout);
  _exit (5);
}

static volatile int sus_step;

static void
fsus (void)
{
  sus_step++;
  swapcontext (&uc_a2, &uc_main);
  sus_step += 10;
}

static void
reuse_case (void)
{
  /* A copy of a made context starts it.  */
  setup (&uc_a, stk_a, &uc_main);
  long r = CHECK (makecontext_e2k (&uc_a, f0, 0));
  memcpy (&uc_c, &uc_a, sizeof uc_c);
  ran = 0;
  long s = CHECK (swapcontext (&uc_main, &uc_c));
  printf ("    started from a memcpy of the made ucontext_t: makecontext %s, "
          "swap %s, ran %d\n", err (r), err (s), ran);
  /* Suspended, resumed through a copy of the saved state.  */
  setup (&uc_a, stk_a, &uc_main);
  r = CHECK (makecontext_e2k (&uc_a, fsus, 0));
  sus_step = 0;
  s = CHECK (swapcontext (&uc_main, &uc_a));
  int st1 = sus_step;
  memcpy (&uc_c, &uc_a2, sizeof uc_c);
  long s2 = CHECK (swapcontext (&uc_main, &uc_c));
  printf ("    suspended (step %d), resumed from a copy of its saved state: "
          "swap %s, step %d (11 = resumed and returned)\n", st1, err (s2),
          sus_step);
  /* Made again with the same stack while suspended.  */
  setup (&uc_a, stk_a, &uc_main);
  r = CHECK (makecontext_e2k (&uc_a, fsus, 0));
  sus_step = 0;
  s = CHECK (swapcontext (&uc_main, &uc_a));
  st1 = sus_step;
  setup (&uc_c, stk_a, &uc_main);
  long r2 = CHECK (makecontext_e2k (&uc_c, fsus, 0));
  s2 = CHECK (swapcontext (&uc_main, &uc_c));
  printf ("    suspended at step %d, then makecontext_e2k on the same stack "
          "(same key): %s; switching to the new one: %s, step %d (2 = "
          "started again from the top)\n", st1, err (r2), err (s2),
          sus_step);
  /* Let the remade one finish so the stack is idle again.  */
  s2 = CHECK (swapcontext (&uc_main, &uc_a2));
  printf ("    resuming it: %s, step %d\n", err (s2), sus_step);
}

/* The stale saved state of a context that was made again.  */
static void
stale_child (void)
{
  setup (&uc_a, stk_a, &uc_main);
  makecontext_e2k (&uc_a, fsus, 0);
  sus_step = 0;
  swapcontext (&uc_main, &uc_a);
  memcpy (&uc_tmp, &uc_a2, sizeof uc_tmp);
  setup (&uc_c, stk_a, &uc_main);
  long r = CHECK (makecontext_e2k (&uc_c, f0, 0));
  printf ("      suspended at step %d, made again with f0: %s; now switching "
          "to the old saved state\n", sus_step, err (r));
  fflush (stdout);
  ran = 0;
  long s = CHECK (swapcontext (&uc_main, &uc_tmp));
  printf ("      swapcontext %s, step %d, f0 ran %d\n", err (s), sus_step,
          ran);
}

static volatile long busy_r;

static void
fbusy (void)
{
  setup (&uc_f, stk_a, &uc_main);
  busy_r = CHECK (makecontext_e2k (&uc_f, f0, 0));
  swapcontext (&uc_a2, &uc_main);
  order[order_n++] = '!';
}

static void
busy_child (void)
{
  maps_snap ("", 0);
  setup (&uc_a, stk_a, &uc_main);
  makecontext_e2k (&uc_a, fbusy, 0);
  order_n = 0;
  swapcontext (&uc_main, &uc_a);
  printf ("      makecontext_e2k on the running context's own stack (same "
          "key) from inside it: %s\n", err (busy_r));
  ran = 0;
  long s = CHECK (swapcontext (&uc_main, &uc_f));
  printf ("      switching to the new one: %s, f0 ran %d\n", err (s), ran);
  s = CHECK (swapcontext (&uc_main, &uc_a2));
  order[order_n] = 0;
  printf ("      switching to the old one's saved state: %s, order \"%s\" "
          "(! = it resumed)\n", err (s), order);
  usleep (200000);
  maps_snap ("in the child after that (+ new, - gone)", 1);
}

static volatile long t_r;

static void *
t_busy (void *arg)
{
  (void) arg;
  ucontext_t t;
  t_r = CHECK (swapcontext (&t, &uc_main));
  return NULL;
}

static void
thread_busy_child (void)
{
  setup (&uc_a, stk_a, &uc_main);
  makecontext_e2k (&uc_a, f0, 0);
  swapcontext (&uc_main, &uc_a);
  pthread_t t;
  t_r = 12345;
  pthread_create (&t, NULL, t_busy, NULL);
  pthread_join (t, NULL);
  printf ("      another thread switching to the main context (running): %s\n",
          err (t_r));
}

/* lcc at -O1 and above treats setcontext as a function that never
   returns (it drops the code after the call, and a failing setcontext
   then traps), so call it through a pointer.  */
static int (*volatile p_setcontext) (const ucontext_t *) = setcontext;

static void
esrch_child (void)
{
  ucontext_t z;
  memset (&z, 0, sizeof z);
  z.uc_mcontext.nr_TIRs = 0x123456700UL;
  long r = CHECK (p_setcontext (&z));
  printf ("      setcontext to an unknown key: %s\n", err (r));
  r = CHECK (swapcontext (&uc_tmp, &z));
  printf ("      swapcontext to an unknown key: %s\n", err (r));
}

/* The same failing setcontext called directly: with lcc -O1 and above
   nothing after the call is compiled, and the return lands on a
   `ct %ctpr3' that the call emptied.  */
static void __attribute__ ((noinline))
direct_setcontext_child (void)
{
  static ucontext_t z;
  memset (&z, 0, sizeof z);
  z.uc_mcontext.nr_TIRs = 0x123456700UL;
  long r = setcontext (&z);
  printf ("      direct setcontext to an unknown key returned %ld\n", r);
}

static volatile long self_free;

static void
fself (void)
{
  self_free = CHECK (freecontext_e2k (&uc_a));
}

static void
free_case (void)
{
  setup (&uc_a, stk_a, &uc_main);
  makecontext_e2k (&uc_a, fself, 0);
  swapcontext (&uc_main, &uc_a);
  printf ("    freecontext_e2k of the running context, from inside it: %s\n",
          err (self_free));
  long r = CHECK (freecontext_e2k (&uc_main));
  printf ("    freecontext_e2k of the main context (saved by swapcontext), "
          "from main: %s\n", err (r));
  maps_snap ("", 0);
  r = CHECK (freecontext_e2k (&uc_a));
  long r2 = CHECK (freecontext_e2k (&uc_a));
  printf ("    freecontext_e2k of a finished context: %s; again: %s\n",
          err (r), err (r2));
  usleep (200000);
  maps_snap ("200 ms after freecontext_e2k (- = gone)", 1);
  setup (&uc_c, stk_c, &uc_main);
  r = CHECK (makecontext_e2k (&uc_c, f0, 0));
  printf ("    makecontext_e2k with a third stack (a new key): %s\n", err (r));
  maps_snap ("after it (+ = new)", 1);
  setup (&uc_b, stk_b, &uc_main);
  makecontext_e2k (&uc_b, f0, 0);
  memcpy (&uc_c, &uc_b, sizeof uc_c);
  r = CHECK (freecontext_e2k (&uc_c));
  r2 = CHECK (freecontext_e2k (&uc_b));
  printf ("    freecontext_e2k through a copy: %s, then the original: %s\n",
          err (r), err (r2));
  /* Errors from makecontext_e2k.  */
  setup (&uc_b, stk_b, &uc_main);
  uc_b.uc_stack.ss_size = 0;
  r = CHECK (makecontext_e2k (&uc_b, f0, 0));
  setup (&uc_b, stk_b, &uc_main);
  uc_b.uc_stack.ss_size = 16;
  r2 = CHECK (makecontext_e2k (&uc_b, f0, 0));
  printf ("    makecontext_e2k with a 0-byte stack: %s, 16-byte: %s\n",
          err (r), err (r2));
}

/* FP control registers per context.  */
static int fp_r[4], fp_f[4];
static unsigned fp_raw[4][3];

static void
fp_rec (int k)
{
  fp_r[k] = fegetround ();
  fp_f[k] = fetestexcept (FE_ALL_EXCEPT);
  cs_rd_fp (fp_raw[k]);
}

static const char *
rname (int r)
{
  return r == FE_TONEAREST ? "nearest" : r == FE_UPWARD ? "upward"
    : r == FE_DOWNWARD ? "downward" : r == FE_TOWARDZERO ? "toward zero"
    : "?";
}

static void
ffp (void)
{
  fp_rec (0);
  fesetround (FE_TOWARDZERO);
  feclearexcept (FE_ALL_EXCEPT);
  feraiseexcept (FE_OVERFLOW);
  swapcontext (&uc_a2, &uc_main);
  fp_rec (2);
}

static void
fp_case (void)
{
  fesetround (FE_UPWARD);
  feclearexcept (FE_ALL_EXCEPT);
  feraiseexcept (FE_INEXACT);
  unsigned m[3];
  cs_rd_fp (m);
  setup (&uc_a, stk_a, &uc_main);
  makecontext_e2k (&uc_a, ffp, 0);
  printf ("    main: upward, inexact (%%fpcr %#x %%fpsr %#x %%pfpfr %#x); "
          "makecontext_e2k wrote fpcr %#x fpsr %#x pfpfr %#x\n", m[0], m[1],
          m[2], uc_a.uc_extra.fpcr, uc_a.uc_extra.fpsr, uc_a.uc_extra.pfpfr);
  swapcontext (&uc_main, &uc_a);
  fp_rec (1);
  swapcontext (&uc_main, &uc_a2);
  fp_rec (3);
  static const char *what[4] = { "the new context starts with",
    "main after the context set toward zero + overflow and switched back",
    "the context when resumed", "main after the context returned (uc_link)" };
  for (int k = 0; k < 4; k++)
    printf ("    %-70s %s, flags %#x (%%fpcr %#x %%fpsr %#x %%pfpfr %#x)\n",
            what[k], rname (fp_r[k]), fp_f[k], fp_raw[k][0], fp_raw[k][1],
            fp_raw[k][2]);
  /* getcontext / setcontext on the same stack.  */
  static volatile int pass;
  pass = 0;
  fesetround (FE_UPWARD);
  feclearexcept (FE_ALL_EXCEPT);
  feraiseexcept (FE_INEXACT);
  getcontext (&uc_g);
  if (pass++ == 0)
    {
      fesetround (FE_DOWNWARD);
      feclearexcept (FE_ALL_EXCEPT);
      feraiseexcept (FE_DIVBYZERO);
      setcontext (&uc_g);
    }
  printf ("    getcontext under upward + inexact, then downward + divbyzero, "
          "setcontext: %s, flags %#x\n", rname (fegetround ()),
          fetestexcept (FE_ALL_EXCEPT));
  fesetround (FE_TONEAREST);
  feclearexcept (FE_ALL_EXCEPT);
}

/* Signal mask per context.  */
static char sm[3][32];

static void
sm_rec (int k)
{
  sigset_t s;
  sigprocmask (SIG_BLOCK, NULL, &s);
  snprintf (sm[k], 32, "%s%s%s%s", sigismember (&s, SIGUSR1) ? "USR1 " : "",
            sigismember (&s, SIGUSR2) ? "USR2 " : "",
            sigismember (&s, SIGTERM) ? "TERM " : "",
            sigismember (&s, SIGUSR1) || sigismember (&s, SIGUSR2)
            || sigismember (&s, SIGTERM) ? "" : "none");
}

static void
fsm (void)
{
  sm_rec (0);
  sigset_t s;
  sigemptyset (&s);
  sigaddset (&s, SIGUSR2);
  sigprocmask (SIG_BLOCK, &s, NULL);
  swapcontext (&uc_a2, &uc_main);
  sm_rec (2);
}

static void
sigmask_case (void)
{
  sigset_t s, old;
  sigemptyset (&s);
  sigaddset (&s, SIGUSR1);
  sigprocmask (SIG_BLOCK, &s, &old);
  setup (&uc_a, stk_a, &uc_main);
  memset (&uc_a.uc_sigmask, 0xff, sizeof uc_a.uc_sigmask);
  makecontext_e2k (&uc_a, fsm, 0);
  int cleared = !sigismember (&uc_a.uc_sigmask, SIGINT);
  sigaddset (&uc_a.uc_sigmask, SIGTERM);
  swapcontext (&uc_main, &uc_a);
  char m1[32];
  sm_rec (1);
  strcpy (m1, sm[1]);
  swapcontext (&uc_main, &uc_a2);
  sm_rec (1);
  printf ("    makecontext_e2k %s uc_sigmask; main blocks USR1, the "
          "context's uc_sigmask gets TERM before the first switch\n",
          cleared ? "cleared" : "kept");
  printf ("    the context starts with: %s; after it blocked USR2 and "
          "switched back, main has: %s; the context resumed has: %s; main "
          "after the context returned: %s\n", sm[0], m1, sm[2], sm[1]);
  sigprocmask (SIG_SETMASK, &old, NULL);
}

/* uc_link once more.  glibc's makecontext_helper tests uc_link with ldw
   (its low 32 bits); the trampoline uses the 64-bit value the kernel
   recorded at makecontext_e2k and ends with the exit system call (the
   thread only), status the failed setcontext's error number.  */
static unsigned long lo32_off;

static void
lo32_child (void)
{
  atexit (atexit_fn);
  void *p = mmap ((void *) 0x700000000UL, 0x4000, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  if (p != (void *) 0x700000000UL)
    {
      printf ("      mmap at 0x700000000 failed\n");
      return;
    }
  ucontext_t *lk = (ucontext_t *) (0x700000000UL + lo32_off);
  setup (&uc_a, stk_a, lk);
  long r = CHECK (makecontext_e2k (&uc_a, fa, 0));
  printf ("      uc_link %p: makecontext_e2k %s; switching\n", (void *) lk,
          err (r));
  fflush (stdout);
  swapcontext (lk, &uc_a);
  printf ("      back in main\n");
  fflush (stdout);
  _exit (5);
}

static void
copy_null_child (void)
{
  atexit (atexit_fn);
  setup (&uc_a, stk_a, &uc_main);
  long r = CHECK (makecontext_e2k (&uc_a, fa, 0));
  memcpy (&uc_c, &uc_a, sizeof uc_c);
  uc_c.uc_link = NULL;
  printf ("      made with uc_link main, started from a copy whose uc_link is "
          "NULL: makecontext_e2k %s\n", err (r));
  fflush (stdout);
  swapcontext (&uc_main, &uc_c);
  printf ("      back in main\n");
  fflush (stdout);
  _exit (5);
}

static ucontext_t uc_t, uc_tm;

static void *
t_relink (void *arg)
{
  (void) arg;
  setup (&uc_t, stk_b, NULL);
  long r = CHECK (makecontext_e2k (&uc_t, fa, 0));
  uc_t.uc_link = &uc_tm;
  printf ("      second thread: made with uc_link NULL, then set to its main "
          "context: makecontext_e2k %s; switching\n", err (r));
  fflush (stdout);
  swapcontext (&uc_tm, &uc_t);
  printf ("      second thread: back (not expected)\n");
  fflush (stdout);
  return NULL;
}

static void
thread_relink_child (void)
{
  atexit (atexit_fn);
  pthread_t t;
  pthread_create (&t, NULL, t_relink, NULL);
  int j = pthread_join (t, NULL);
  printf ("      first thread: pthread_join %d, the process goes on\n", j);
  fflush (stdout);
  _exit (6);
}

static void
unknown_link_child (void)
{
  static ucontext_t z;
  atexit (atexit_fn);
  memset (&z, 0, sizeof z);
  z.uc_mcontext.nr_TIRs = 0x123456700UL;
  setup (&uc_a, stk_a, &z);
  long r = CHECK (makecontext_e2k (&uc_a, fa, 0));
  printf ("      uc_link naming an unknown key: makecontext_e2k %s; "
          "switching\n", err (r));
  fflush (stdout);
  swapcontext (&uc_main, &uc_a);
  printf ("      back in main (not expected)\n");
  fflush (stdout);
  _exit (5);
}

static volatile long free_main_r;

static void
ffree_main (void)
{
  free_main_r = CHECK (freecontext_e2k (&uc_main));
  printf ("      in the context: freecontext_e2k of the suspended main "
          "context: %s; returning (uc_link main)\n", err (free_main_r));
  fflush (stdout);
}

static void
free_main_child (void)
{
  atexit (atexit_fn);
  setup (&uc_a, stk_a, &uc_main);
  makecontext_e2k (&uc_a, ffree_main, 0);
  swapcontext (&uc_main, &uc_a);
  printf ("      back in main\n");
  fflush (stdout);
  _exit (5);
}

static void
link_more_case (void)
{
  printf ("  uc_link whose low 32 bits are 0, and a control, in a child:\n");
  lo32_off = 0;
  in_child ("uc_link 0x700000000", lo32_child);
  lo32_off = 0x100;
  in_child ("uc_link 0x700000100", lo32_child);
  printf ("  started from a copy whose uc_link is NULL, in a child:\n");
  in_child ("copy NULL", copy_null_child);
  printf ("  the trampoline's exit in a second thread, in a child:\n");
  in_child ("thread relink", thread_relink_child);
  printf ("  uc_link naming an unknown key, in a child (status = the "
          "trampoline's):\n");
  in_child ("unknown uc_link", unknown_link_child);
  printf ("  freecontext_e2k of the suspended main context, in a child:\n");
  in_child ("free main", free_main_child);
}

/* 1. Context switch costs.  */
static ucontext_t cu_main, cu_a, cu_g;

static void
fping (void)
{
  for (;;)
    swapcontext (&cu_a, &cu_main);
}

static jmp_buf jb;
static sigjmp_buf sjb;

static void __attribute__ ((noinline))
do_longjmp (void)
{
  longjmp (jb, 1);
}

static void __attribute__ ((noinline))
do__longjmp (void)
{
  _longjmp (jb, 1);
}

static void __attribute__ ((noinline))
do_siglongjmp (void)
{
  siglongjmp (sjb, 1);
}

static void
cost (void)
{
  long ncpu = sysconf (_SC_NPROCESSORS_ONLN);
  cpu_set_t cs;
  CPU_ZERO (&cs);
  CPU_SET (ncpu - 1, &cs);
  sched_setaffinity (0, sizeof cs, &cs);
  const long n = 20000;
  long t0, t1;
  static volatile long i;

  memset (&cu_a, 0, sizeof cu_a);
  cu_a.uc_stack.ss_sp = stk_a;
  cu_a.uc_stack.ss_size = STK;
  cu_a.uc_link = &cu_main;
  makecontext_e2k (&cu_a, fping, 0);
  swapcontext (&cu_main, &cu_a);
  t0 = cs_clkr ();
  for (i = 0; i < n; i++)
    swapcontext (&cu_main, &cu_a);
  t1 = cs_clkr ();
  printf ("  cycles (CPU %ld, %ld iterations):\n", ncpu - 1, n);
  printf ("    swapcontext round trip (two switches between two "
          "contexts): %ld\n", (t1 - t0) / n);

  t0 = cs_clkr ();
  for (i = 0; i < n; i++)
    getcontext (&cu_g);
  t1 = cs_clkr ();
  printf ("    getcontext: %ld\n", (t1 - t0) / n);

  static volatile long k;
  k = 0;
  t0 = cs_clkr ();
  getcontext (&cu_g);
  if (++k < n)
    setcontext (&cu_g);
  t1 = cs_clkr ();
  printf ("    getcontext + setcontext back to it (same stack): %ld\n",
          (t1 - t0) / n);

  t0 = cs_clkr ();
  for (i = 0; i < n; i++)
    if (!setjmp (jb))
      do_longjmp ();
  t1 = cs_clkr ();
  printf ("    setjmp + longjmp from one call down: %ld\n", (t1 - t0) / n);

  t0 = cs_clkr ();
  for (i = 0; i < n; i++)
    if (!_setjmp (jb))
      do__longjmp ();
  t1 = cs_clkr ();
  printf ("    _setjmp + _longjmp from one call down: %ld\n", (t1 - t0) / n);

  t0 = cs_clkr ();
  for (i = 0; i < n; i++)
    if (!sigsetjmp (sjb, 0))
      do_siglongjmp ();
  t1 = cs_clkr ();
  printf ("    sigsetjmp (0) + siglongjmp: %ld\n", (t1 - t0) / n);

  t0 = cs_clkr ();
  for (i = 0; i < n; i++)
    if (!sigsetjmp (sjb, 1))
      do_siglongjmp ();
  t1 = cs_clkr ();
  printf ("    sigsetjmp (1) + siglongjmp: %ld\n", (t1 - t0) / n);

  ucontext_t m2, b;
  t0 = cs_clkr ();
  for (i = 0; i < n / 10; i++)
    {
      memset (&b, 0, sizeof b);
      b.uc_stack.ss_sp = stk_b;
      b.uc_stack.ss_size = STK;
      b.uc_link = &m2;
      makecontext_e2k (&b, f0, 0);
      swapcontext (&m2, &b);
    }
  t1 = cs_clkr ();
  printf ("    makecontext_e2k (reusing the key) + switch + return through "
          "uc_link: %ld\n", (t1 - t0) / (n / 10));

  t0 = cs_clkr ();
  for (i = 0; i < n; i++)
    getppid ();
  t1 = cs_clkr ();
  printf ("    getppid (a plain system call, for scale): %ld\n",
          (t1 - t0) / n);
}

/* 2. sigreturn and an edited frame.  */
static volatile long io[24];
static int edit_mode;
static struct
{
  long ip_in, g[4], usd, cr1hi;
  int gtag[4];
  unsigned bgr, fp[3], put[3];
} er;

static void
edit_c (int sig, siginfo_t *si, void *ucv)
{
  (void) sig;
  (void) si;
  ucontext_t *uc = ucv;
  unsigned long ip = uc->uc_mcontext.cr0_hi & M48 & ~7UL;
  er.ip_in = ip >= (unsigned long) cs_gsig && ip < (unsigned long) cs_gsig_end;
  for (int i = 0; i < 4; i++)
    {
      er.g[i] = uc->uc_extra.g[i];
      er.gtag[i] = uc->uc_extra.gtag[i];
    }
  er.fp[0] = uc->uc_extra.fpcr;
  er.fp[1] = uc->uc_extra.fpsr;
  er.fp[2] = uc->uc_extra.pfpfr;
  er.usd = uc->uc_mcontext.usd_lo;
  er.cr1hi = uc->uc_mcontext.cr1_hi;
  er.bgr = uc->uc_extra.bgr;
  switch (edit_mode)
    {
    case 'G':
      uc->uc_extra.g[0] += 1;
      uc->uc_extra.g[1] = 0x5151515151515151L;
      uc->uc_extra.g[2] = ~uc->uc_extra.g[2];
      uc->uc_extra.gtag[3] = 5;
      break;
    case 'M':
      sigaddset (&uc->uc_sigmask, SIGUSR2);
      sigdelset (&uc->uc_sigmask, SIGHUP);
      break;
    case 'F':
      {
        fenv_t e;
        fegetenv (&e);
        fesetround (FE_TOWARDZERO);
        feclearexcept (FE_ALL_EXCEPT);
        feraiseexcept (FE_DIVBYZERO);
        cs_rd_fp (er.put);
        fesetenv (&e);
        uc->uc_extra.fpcr = er.put[0];
        uc->uc_extra.fpsr = er.put[1];
        uc->uc_extra.pfpfr = er.put[2];
      }
      break;
    case 'U':
      uc->uc_mcontext.usd_lo -= 256;
      break;
    case 'C':
      if ((uc->uc_mcontext.cr1_hi & 0x3f) == 6 && er.ip_in)
        uc->uc_mcontext.cr1_hi = (uc->uc_mcontext.cr1_hi & ~0x3fUL) | 7;
      break;
    }
  io[5] = 1;
}

static int edit_timer, edit_nosig;

static void
edit_child (void)
{
  sigset_t s;
  sigemptyset (&s);
  sigaddset (&s, SIGHUP);
  sigprocmask (SIG_SETMASK, &s, NULL);
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = edit_c;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGUSR1, &sa, NULL);
  sigaction (SIGALRM, &sa, NULL);
  fesetround (FE_UPWARD);
  feclearexcept (FE_ALL_EXCEPT);
  unsigned before[3];
  cs_rd_fp (before);
  io[0] = 0x1616161616161616L;
  io[1] = 0x1717171717171717L;
  io[2] = 0x1818181818181818L;
  io[3] = 0x1919191919191919L;
  io[4] = edit_timer || edit_nosig ? 0 : SIGUSR1;
  io[5] = 0;
  io[6] = getpid ();
  io[7] = gettid ();
  io[20] = edit_timer;
  if (edit_timer)
    {
      struct itimerval it = { { 0, 0 }, { 0, 20000 } };
      setitimer (ITIMER_REAL, &it, NULL);
    }
  cs_gsig (io);
  int r = fegetround (), f = fetestexcept (FE_ALL_EXCEPT);
  unsigned after[3];
  cs_rd_fp (after);
  sigprocmask (SIG_BLOCK, NULL, &s);
  printf ("      handler interrupted cs_gsig: %s; tgkill %ld\n",
          er.ip_in ? "yes" : "NO", io[19]);
  printf ("      frame at entry: g16..19 %#lx %#lx %#lx %#lx, gtag %d %d %d "
          "%d; fpcr %#x fpsr %#x pfpfr %#x; bgr %#x; usd_lo %#lx; "
          "cr1_hi.br %#lx\n", er.g[0], er.g[1], er.g[2], er.g[3], er.gtag[0],
          er.gtag[1], er.gtag[2], er.gtag[3], er.fp[0], er.fp[1], er.fp[2],
          er.bgr, er.usd, er.cr1hi & 0xfffffff);
  printf ("      after return: %%g16..19 %#lx %#lx %#lx %#lx, tags %ld %ld "
          "%ld %ld\n", io[8], io[9], io[10], io[11], io[12] & 0xff,
          io[13] & 0xff, io[14] & 0xff, io[15] & 0xff);
  printf ("      after return: SP %+ld (before %#lx), %%b[0] = %#lx "
          "(0xcccc: rbs 6 kept)\n", io[17] - io[16], io[16], io[18]);
  printf ("      after return: rounding %s, flags %#x (fpcr %#x fpsr %#x "
          "pfpfr %#x; before %#x %#x %#x)", rname (r), f, after[0], after[1],
          after[2], before[0], before[1], before[2]);
  if (edit_mode == 'F')
    printf ("; the frame got fpcr %#x fpsr %#x pfpfr %#x", er.put[0],
            er.put[1], er.put[2]);
  printf ("\n      after return: blocked HUP %d, USR2 %d\n",
          sigismember (&s, SIGHUP), sigismember (&s, SIGUSR2));
}

/* 3. %g16..%g31 at handler entry.  */
static struct
{
  long g[16];
  int gtag[16];
  unsigned bgr;
} q3;

static void
q3_c (int sig, siginfo_t *si, void *ucv)
{
  (void) sig;
  (void) si;
  ucontext_t *uc = ucv;
  for (int i = 0; i < 16; i++)
    {
      q3.g[i] = uc->uc_extra.g[i];
      q3.gtag[i] = uc->uc_extra.gtag[i];
    }
  q3.bgr = uc->uc_extra.bgr;
  io[5] = 1;
}

static void
q3_run (int timer, void (*h) (int, siginfo_t *, void *))
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = h;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGUSR1, &sa, NULL);
  sigaction (SIGALRM, &sa, NULL);
  hc = q3_c;
  memset (cs_gval, 0x77, sizeof cs_gval);
  memset (cs_gtag, 0x77, sizeof cs_gtag);
  io[0] = 0x1616161616161616L;
  io[1] = 0x1717171717171717L;
  io[2] = 0x1818181818181818L;
  io[3] = 0x1919191919191919L;
  io[4] = timer ? 0 : SIGUSR1;
  io[5] = 0;
  io[6] = getpid ();
  io[7] = gettid ();
  io[20] = timer;
  if (timer)
    {
      struct itimerval it = { { 0, 0 }, { 0, 20000 } };
      setitimer (ITIMER_REAL, &it, NULL);
    }
  cs_gsig (io);
}

static int sg_mode, sg_nr;

static void
sysg_child (void)
{
  static long io[128];
  static const char *const fast_name[] = { "gettimeofday", "clock_gettime "
    "(CLOCK_MONOTONIC)", "getcpu", "the signal mask query" };
  memset (io, 0, sizeof io);
  for (int n = 0; n < 32; n++)
    io[n] = 0x6700000000000000L + n;
  io[112] = sg_mode & 1 ? sg_nr : __NR_getppid;
  io[110] = (long) &io[100];
  io[111] = sg_nr == 3 ? 8 : (long) &io[102];
  if ((sg_mode & 1) && sg_nr == 1)
    {
      io[110] = CLOCK_MONOTONIC;
      io[111] = (long) &io[100];
    }
  if ((sg_mode & 1) && sg_nr == 0)
    io[111] = 0;
  const char *set = (sg_mode & 6) == 6 ? "%g0..%g11, %g14, %g15 and "
    "%g16..%g31" : sg_mode & 2 ? "%g0..%g11, %g14, %g15" : "%g16..%g31";
  if (sg_mode & 1)
    printf ("    fast call %d (%s), entry 6, %s set:", sg_nr,
            fast_name[sg_nr], set);
  else
    printf ("    getppid, entry 3, %s set:", set);
  fflush (stdout);
  cs_sysg (io, sg_mode);
  for (int n = 0, k = 0; n < 32; n++)
    {
      if (n == 12 || n == 13)
        continue;
      if (!(sg_mode & (n < 16 ? 2 : 4)))
        continue;
      long v = io[32 + n];
      int t = (int) (io[64 + n] & 0xff);
      printf ("%s %%g%d ", k++ % 6 ? "" : "\n     ", n);
      if (v == io[n] && t == 0)
        printf ("kept");
      else
        printf ("%#lx/%d", v, t);
    }
  printf ("\n");
}

static void
q3_print (const char *what)
{
  printf ("    %s: in the handler (value/tag):", what);
  for (int i = 0; i < 16; i++)
    printf ("%s %%g%d %#lx/%d", i % 4 ? "" : "\n     ", 16 + i, cs_gval[i],
            cs_gtag[i]);
  printf ("\n    the frame's copy (uc_extra.g/gtag), bgr %#x:", q3.bgr);
  for (int i = 0; i < 16; i++)
    printf ("%s %%g%d %#lx/%d", i % 4 ? "" : "\n     ", 16 + i, q3.g[i],
            q3.gtag[i]);
  printf ("\n    after return: %%g16..19 %#lx %#lx %#lx %#lx, tags %ld %ld "
          "%ld %ld\n", io[8], io[9], io[10], io[11], io[12] & 0xff,
          io[13] & 0xff, io[14] & 0xff, io[15] & 0xff);
}

static void
fault_rec (int sig, siginfo_t *si, void *uc)
{
  (void) uc;
  char b[160];
  int n = snprintf (b, sizeof b, "      fault: signal %d, si_code %d, "
                    "si_addr %s\n", sig, si->si_code,
                    sym ((unsigned long) si->si_addr));
  write (1, b, n);
  _exit (7);
}

static void (*ns_handler) (int, siginfo_t *, void *);

static void (*diag_fn) (long *);

static void
diag_child (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = fault_rec;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  sigaction (SIGSEGV, &sa, NULL);
  long out[3] = { -1, -1, -1 };
  diag_fn (out);
  printf ("      no fault: the value's tag %ld, the addd's result %#lx tag "
          "%ld\n", out[0] & 0xff, out[1], out[2] & 0xff);
}

static void
empty_add_child (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = fault_rec;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  long out[4] = { -1, -1, -1, -1 };
  cs_empty_add (out);
  printf ("      no fault: the addd gave %#lx, tag %ld\n", out[0],
          out[1] & 0xff);
}

static void
empty_st_child (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = fault_rec;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  static long out[4] = { -1, -1, -1, -1 };
  cs_empty_st (out);
  printf ("      no fault: the std wrote %#lx; filled back with tag %ld; the "
          "addd of it gave %#lx\n", out[2], out[0] & 0xff, out[1]);
}

static long ta_val, ta_tag;

static void
tagadd_child (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = fault_rec;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  static long out[2] = { -1, -1 };
  printf ("    %#018lx tag %ld:", ta_val, ta_tag);
  fflush (stdout);
  cs_tagadd (ta_val, ta_tag, out);
  printf (" no fault: tag read back %ld, addd gave %#lx\n", out[0] & 0xff,
          out[1]);
}

/* A speculative andd/ord of a diagnostic value: does the result keep
   bit 30, and does it still trap a non-speculative addd?  */
static volatile long dv_out[3];
static long dv_mask;
static int dv_isor;

static void
dv_fault (int sig, siginfo_t *si, void *uc)
{
  (void) uc;
  char b[200];
  int n = snprintf (b, sizeof b, " tag %ld, bits %#018lx; fault: signal "
                    "%d, si_code %d, si_addr %s\n", dv_out[0] & 0xff,
                    dv_out[1], sig, si->si_code,
                    sym ((unsigned long) si->si_addr));
  write (1, b, n);
  _exit (7);
}

static void
dv_child (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = dv_fault;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  dv_out[0] = dv_out[1] = dv_out[2] = -1;
  printf ("    %s,sm of it and %#018lx:", dv_isor ? "ord " : "andd", dv_mask);
  fflush (stdout);
  (dv_isor ? cs_dv_or : cs_dv_and) ((long *) dv_out, dv_mask);
  printf (" tag %ld, bits %#018lx; no fault: addd gave %#lx\n",
          dv_out[0] & 0xff, dv_out[1], dv_out[2]);
}

/* A handler that makes a diagnostic value (cs_diag_r) itself.  */
static void
diag_in_handler (int sig, siginfo_t *si, void *uc)
{
  (void) sig;
  (void) si;
  (void) uc;
  long out[3] = { -1, -1, -1 };
  cs_diag_r (out);
  printf ("      no fault: the value's tag %ld, the addd's result %#lx tag "
          "%ld\n", out[0] & 0xff, out[1], out[2] & 0xff);
}

static void
diag_handler_child (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = fault_rec;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  sa.sa_sigaction = diag_in_handler;
  sigaction (SIGUSR1, &sa, NULL);
  raise (SIGUSR1);
}

static void
ns_child (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = fault_rec;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  sigaction (SIGSEGV, &sa, NULL);
  sigaction (SIGBUS, &sa, NULL);
  memset (cs_gns, 0x55, sizeof cs_gns);
  cs_gns[2] = 0;
  cs_gns[3] = 0;
  q3_run (0, ns_handler);
  if (ns_handler == cs_ghandler_add)
    printf ("      no fault; the addd's result %#lx, tag %ld; in the handler "
            "%%g16 = %#lx/%d\n", cs_gns[0], cs_gns[3] & 0xff, cs_gval[0],
            cs_gtag[0]);
  else
    printf ("      no fault; the std wrote %#lx, its memory tag (fill MAS) "
            "%ld; in the handler %%g16 = %#lx/%d\n", cs_gns[1],
            cs_gns[2] & 0xff, cs_gval[0], cs_gtag[0]);
}

/* 4. The chain stack from a handler and from a context.  */
static unsigned long long chain[16384];

void __attribute__ ((noinline))
chain_dump (const char *what)
{
  long sz = 0;
  long r = E2K_SYSCALL (LINUX_SYSCALL64_TRAPNUM, __NR_access_hw_stacks, 5,
                        E2K_GET_CHAIN_STACK_SIZE, NULL, NULL, 0, &sz);
  printf ("    %s: E2K_GET_CHAIN_STACK_SIZE %ld, size %ld (%ld frames)\n",
          what, r, sz, sz / 32);
  if (r || sz <= 0 || sz > (long) sizeof chain)
    return;
  unsigned long long off = 0;
  memset (chain, 0, sz);
  r = E2K_SYSCALL (LINUX_SYSCALL64_TRAPNUM, __NR_access_hw_stacks, 5,
                   E2K_READ_CHAIN_STACK_EX, &off, chain, sz, NULL);
  printf ("    E2K_READ_CHAIN_STACK_EX %ld; frames from the top (#0 = the "
          "last 32 bytes):\n", r);
  long nf = sz / 32;
  for (long k = nf - 1; k >= 0; k--)
    {
      unsigned long long *c = &chain[4 * k];
      unsigned long ip = c[1] & M48 & ~7UL;
      printf ("      #%-2ld +%#6lx  ip %#14lx %-38s pm %llu wbs %#llx wpsz "
              "%#llx ussz %#llx br %#llx\n", nf - 1 - k, k * 32, ip, sym (ip),
              (c[2] >> 57) & 1, (c[2] >> 33) & 0x7f, (c[2] >> 26) & 0x7f,
              (c[3] >> 36) << 4, c[3] & 0xfffffff);
    }
  unsigned long bt[64];
  long n = E2K_SYSCALL (LINUX_SYSCALL64_TRAPNUM, __NR_get_backtrace, 4, bt,
                        64, 0, 0);
  printf ("    get_backtrace (buf, 64, 0, 0) = %ld:", n);
  for (long i = 0; i < n && i < 64; i++)
    printf ("%s %s", i % 4 ? "," : "\n     ", sym (bt[i]));
  n = E2K_SYSCALL (LINUX_SYSCALL64_TRAPNUM, __NR_get_backtrace, 4, bt, 64, 1,
                   0);
  printf ("\n    get_backtrace (buf, 64, 1, 0) = %ld: first %s\n", n,
          n > 0 ? sym (bt[0]) : "-");
  void *ba[64];
  int m = backtrace (ba, 64);
  printf ("    glibc backtrace = %d:", m);
  for (int i = 0; i < m; i++)
    printf ("%s %s", i % 4 ? "," : "\n     ", sym ((unsigned long) ba[i]));
  printf ("\n");
}

static const char *q4_what;

void
q4_c (int sig, siginfo_t *si, void *uc)
{
  (void) sig;
  (void) si;
  (void) uc;
  chain_dump (q4_what);
  io[5] = 1;
}

void __attribute__ ((noinline))
q4_raise_caller (void)
{
  raise (SIGUSR1);
}

void
fctx_dump (void)
{
  chain_dump ("inside a made context (no signal)");
}

static void
part1 (void)
{
  printf ("1. user contexts:\n");
  {
    long r = CHECK (getcontext (&uc_g));
    long f = CHECK (freecontext_e2k (&uc_g));
    printf ("    before any switch: getcontext %s, key %#llx; "
            "freecontext_e2k of it %s\n", err (r), uc_g.uc_mcontext.nr_TIRs,
            err (f));
  }
  maps_snap ("before", 1);
  args_case ();
  link_case ();
  printf ("  uc_link NULL, in a child:\n");
  in_child ("uc_link NULL", null_link_child);
  printf ("  uc_link changed after makecontext_e2k, in a child:\n");
  relink_to_null = 1;
  in_child ("relink", relink_child);
  relink_to_null = 0;
  in_child ("relink", relink_child);
  printf ("  contexts named by key:\n");
  reuse_case ();
  printf ("  the stale saved state of a context made again, in a child:\n");
  in_child ("stale", stale_child);
  printf ("  makecontext_e2k on a busy key, in a child:\n");
  in_child ("busy", busy_child);
  printf ("  a busy context from another thread, in a child:\n");
  in_child ("thread", thread_busy_child);
  printf ("  an unknown key, in a child:\n");
  in_child ("unknown key", esrch_child);
  printf ("  the same, setcontext called directly, in a child:\n");
  in_child ("direct setcontext", direct_setcontext_child);
  printf ("  freecontext_e2k:\n");
  free_case ();
  printf ("  FP control registers per context:\n");
  fp_case ();
  printf ("  signal mask per context:\n");
  sigmask_case ();
  link_more_case ();
}

static void
part2 (void)
{
  static const struct { char m; const char *what; } modes[] = {
    { 0, "frame unchanged" },
    { 'G', "g[0] += 1, g[1] = 0x5151..., g[2] = ~g[2], gtag[3] = 5" },
    { 'M', "uc_sigmask: + USR2, - HUP (HUP blocked before)" },
    { 'F', "uc_extra.fpcr/fpsr/pfpfr = toward zero + divbyzero (registers "
           "left as they were)" },
    { 'U', "usd_lo -= 256" },
    { 'C', "cr1_hi.br: rbs 6 -> 7" },
  };
  printf ("2. sigreturn from an edited frame (SA_SIGINFO; cs_gsig set "
          "%%g16..19 = 0x16.., 0x17.., 0x18.., 0x19.., rounding upward, "
          "HUP blocked):\n");
  edit_mode = 0;
  edit_timer = 0;
  edit_nosig = 1;
  printf ("  tgkill with signal 0 (no handler runs):\n");
  in_child ("no signal", edit_child);
  edit_nosig = 0;
  for (int t = 0; t < 2; t++)
    for (unsigned i = 0; i < sizeof modes / sizeof modes[0]; i++)
      {
        edit_mode = modes[i].m;
        edit_timer = t;
        printf ("  %s, %s:\n", t ? "timer (SIGALRM in a loop)"
                : "tgkill (SIGUSR1)", modes[i].what);
        in_child (modes[i].what, edit_child);
      }
}

static void
part3 (void)
{
  printf ("3. %%g16..%%g31 at handler entry (assembly handler; cs_gsig set "
          "%%g16..19 before the signal):\n");
  q3_run (0, cs_ghandler);
  q3_print ("tgkill");
  q3_run (1, cs_ghandler);
  q3_print ("timer");
  q3_run (0, cs_ghandler_w);
  printf ("    tgkill, a handler that writes 0xdd16..0xdd19 to %%g16..19 "
          "before returning: after return %%g16..19 %#lx %#lx %#lx %#lx, tags "
          "%ld %ld %ld %ld\n", io[8], io[9], io[10], io[11], io[12] & 0xff,
          io[13] & 0xff, io[14] & 0xff, io[15] & 0xff);
  q3_run (1, cs_ghandler_w);
  printf ("    timer, the same handler: after return %%g16..19 %#lx %#lx %#lx "
          "%#lx, tags %ld %ld %ld %ld\n", io[8], io[9], io[10], io[11],
          io[12] & 0xff, io[13] & 0xff, io[14] & 0xff, io[15] & 0xff);
  printf ("  %%g16..%%g19 after a system call that sends no signal (set to "
          "0x16.., 0x17.., 0x18.., 0x19.. just before):\n");
  for (int fast = 0; fast < 2; fast++)
    {
      long out[16];
      memset (out, 0x77, sizeof out);
      cs_empty_tag (out, fast);
      printf ("    %s: %#lx %#lx %#lx %#lx, tags %ld %ld %ld %ld\n",
              fast ? "fast call 3 (siggetmask), entry 6"
              : "getppid, entry 3", out[0], out[1], out[2], out[3],
              out[4] & 0xff, out[5] & 0xff, out[6] & 0xff, out[7] & 0xff);
    }
  printf ("  globals across a system call (%%gN = 0x67000000000000NN just "
          "before; \"kept\" = that value, tag 0; %%g12/%%g13 untouched), each "
          "in a child:\n");
  static const int sg_modes[][2] = { { 6, 0 }, { 7, 3 }, { 7, 1 },
    { 7, 2 }, { 7, 0 } };
  for (unsigned i = 0; i < sizeof sg_modes / sizeof sg_modes[0]; i++)
    {
      sg_mode = sg_modes[i][0];
      sg_nr = sg_modes[i][1];
      in_child ("sysg", sysg_child);
    }
  printf ("  getppid (entry 3) with N pairs handed over (wsz - wbs), and "
          "getcpu (fast call 2) with 6, each in a child:\n");
  static const struct { void (*f) (void); const char *what; } sw[] = {
    { cs_sysw2, "entry 3, 2 pairs" }, { cs_sysw4, "entry 3, 4 pairs" },
    { cs_sysw6, "entry 3, 6 pairs" }, { cs_sysw8, "entry 3, 8 pairs" },
    { cs_sysf6, "entry 6, 6 pairs" } };
  for (unsigned i = 0; i < sizeof sw / sizeof sw[0]; i++)
    in_child (sw[i].what, sw[i].f);
  printf ("  after getppid, a non-speculative addd %%dg16, 1, in a child:\n");
  in_child ("empty addd", empty_add_child);
  printf ("  after getppid, a non-speculative std of %%dg16, a fill load of "
          "it and a non-speculative addd of that, in a child:\n");
  in_child ("empty std", empty_st_child);
  printf ("  puttagd of chosen bits and tags, then a non-speculative addd, "
          "each in a child (si_addr cs_tagadd+0x10: puttagd, +0x38: addd):\n");
  static const long tv[][2] = {
    { 0, 5 }, { 1, 5 }, { 0x4000000040000000L, 5 }, { 0x40000000L, 5 },
    { 0x4000000000000000L, 5 }, { 0x4afafafa4afafafaL, 5 },
    { 0x0afafafa0afafafaL, 5 }, { (long) 0xbfffffffbfffffffUL, 5 },
    { 0x40000000L, 1 }, { 0x4000000000000000L, 1 }, { 0x4000000000000000L, 4 },
    { 0x40000000L, 4 }, { 0x4000000040000000L, 0 } };
  for (unsigned i = 0; i < sizeof tv / sizeof tv[0]; i++)
    {
      ta_val = tv[i][0];
      ta_tag = tv[i][1];
      in_child ("tagadd", tagadd_child);
    }
  printf ("  a diagnostic value (ldd,sm mas=0x3 from 0), andd,sm or ord,sm "
          "of it, then a non-speculative addd, each in a child (si_addr "
          "+0x68: the addd):\n");
  static const struct { int isor; long m; } dv[] = {
    { 0, -1L }, { 0, 0 }, { 0, (long) 0xbfffffffbfffffffUL },
    { 0, 0x00000000ffffffffL }, { 0, 0x0000ffff0000ffffL }, { 1, 0 } };
  for (unsigned i = 0; i < sizeof dv / sizeof dv[0]; i++)
    {
      dv_isor = dv[i].isor;
      dv_mask = dv[i].m;
      in_child ("dv", dv_child);
    }
  printf ("  in a handler, a diagnostic value in a window register then a "
          "non-speculative addd, in a child:\n");
  in_child ("diag in handler", diag_handler_child);
  printf ("  outside a handler, a diagnostic value (ldd,sm mas=0x3 from 0) "
          "then a non-speculative addd of it, in a child:\n");
  printf ("    in %%dg20:\n");
  diag_fn = cs_diag_g;
  in_child ("diag g", diag_child);
  printf ("    in a window register:\n");
  diag_fn = cs_diag_r;
  in_child ("diag r", diag_child);
  printf ("  a non-speculative addd of %%dg16 first thing in the handler, in "
          "a child:\n");
  ns_handler = cs_ghandler_add;
  in_child ("addd", ns_child);
  printf ("  a non-speculative std of %%dg16 first thing in the handler, in "
          "a child:\n");
  ns_handler = cs_ghandler_st;
  in_child ("std", ns_child);
}

void
part4 (void)
{
  printf ("4. the chain stack from a handler ([trampolines] at %#lx):\n",
          tramp_lo);
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = q4_c;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGUSR1, &sa, NULL);
  sigaction (SIGALRM, &sa, NULL);
  q4_what = "SIGUSR1 handler, raise () from q4_raise_caller";
  q4_raise_caller ();
  q4_what = "SIGALRM handler, timer interrupting cs_gsig";
  io[4] = 0;
  io[5] = 0;
  io[20] = 1;
  struct itimerval it = { { 0, 0 }, { 0, 20000 } };
  setitimer (ITIMER_REAL, &it, NULL);
  cs_gsig (io);
  setup (&uc_a, stk_a, &uc_main);
  makecontext_e2k (&uc_a, fctx_dump, 0);
  swapcontext (&uc_main, &uc_a);
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  find_trampolines ();
  if (argc > 1 && !strcmp (argv[1], "cost"))
    {
      cost ();
      return 0;
    }
  struct utsname u;
  uname (&u);
  printf ("kernel %s, [trampolines] %#lx-%#lx\n", u.release, tramp_lo,
          tramp_hi);
  if (argc == 1 || strchr (argv[1], '1'))
    part1 ();
  if (argc == 1 || strchr (argv[1], '2'))
    part2 ();
  if (argc == 1 || strchr (argv[1], '3'))
    part3 ();
  if (argc == 1 || strchr (argv[1], '4'))
    part4 ();
  return 0;
}
