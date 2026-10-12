/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Leftovers of spec/abi/kernel-interface.md ("Not checked yet"):

   1. System-call entries.  The kernel enables entries 1, 3, 4, 5 and 6
      for a 64-bit task (%osem; kernel include/asm/traps.h,
      osem_calculate).  For entry 3 and for the disabled entries that the
      kernel's trap table names (0, 7, 8, 10, 11, 12, 16, 17, 18): what
      sdisp leaves in %ctpr1 (ta_tag and opc only: the address is
      shifted out in kernleft.s), then a call through it, once per entry,
      in a forked child, with the getpid number in %b[0].  Entry 4 (the
      deprecated one): positive and negative numbers, numbers its old
      table lacks, its compat sigprocmask, and %g16 after it.
   2. %g24..%g31 written under a rotated %bgr and read back under it
      and under cur 0, and written under cur 0 and read under a rotated
      %bgr by addd and as the data of std in ALC2 and ALC5, without a
      signal (the reference); then sigreturn
      from a timer signal that interrupted user code running under
      %bgr.cur 1, 2 and 3 (and the same code left to take only
      interrupts for 100 ms), with the frame's uc_extra.bgr edited, with
      every uc_extra.g[], gtag[] and gext[] entry edited, with tag
      combinations the kernel's check_user_gregs refuses, and with a
      diagnostic value; %bgr as the handler finds it (read by the
      handler's first instruction).  Never touches cr0_hi or anything
      else that moves the IP.
   3. (`./kernleft cost [ROUNDS [CPUA CPUB [CPUFAR]]]`) One-way
      swapcontext cost, %clkr cycles read on the switching thread's own
      core: into a context that the same thread ran last, against one
      that another thread ran last, with the two threads on two cores
      (and on two chips with CPUFAR) and on one core.  ROUNDS defaults
      to 4000.  Run it under the shared timing lock.

   Every case of parts 1 and 2 runs in a forked child with an alarm.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $L -O2 kernleft.c kernleft.s -o kernleft -lpthread
     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc  # or v6.2c3
     $L -O2 -Wa,--defsym,KL_V5=1 kernleft.c kernleft.s -o kernleft -lpthread
     ./kernleft [1|2]; ./kernleft cost [4000 CPUA CPUB [CPUFAR]]  # on hw  */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#define M48 ((1UL << 48) - 1)

long kl_clkr (void);
void kl_gspin (long *);
extern char kl_gspin_end[], kl_gspin_use19[];
void kl_hbgr (int, siginfo_t *, void *);
volatile int kl_hbgr_val;
void kl_grot_ld (long *), kl_grot_alu (long *), kl_grot_st (long *);

#define DECL(n)                                                         \
  long kl_prep_##n (void);                                              \
  long kl_call_##n (long, long, long, long, long *);                    \
  extern char kl_call_##n##_ret[];
DECL (0) DECL (3) DECL (4) DECL (7) DECL (8) DECL (10) DECL (11)
DECL (12) DECL (16) DECL (17) DECL (18)

static const struct
{
  int n;
  long (*prep) (void);
  long (*call) (long, long, long, long, long *);
  char *ret;
} ent[] = {
#define E(n) { n, kl_prep_##n, kl_call_##n, kl_call_##n##_ret }
  E (3), E (4), E (0), E (7), E (8), E (10), E (11), E (12), E (16), E (17),
  E (18)
};

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

/* 1.  System-call entries.  */
static unsigned long stub_lo;
static const char *stub_name;

static const char *
ill_code (int sig, int c)
{
  static const char *const ill[] = { "?", "ILL_ILLOPC", "ILL_ILLOPN",
    "ILL_ILLADR", "ILL_ILLTRP", "ILL_PRVOPC", "ILL_PRVREG", "ILL_COPROC",
    "ILL_BADSTK" };
  if (sig == SIGILL && c > 0 && c <= 8)
    return ill[c];
  if (sig == SIGSEGV && c == SEGV_MAPERR)
    return "SEGV_MAPERR";
  if (sig == SIGSEGV && c == SEGV_ACCERR)
    return "SEGV_ACCERR";
  if (c == SI_USER)
    return "SI_USER";
  if (c == SI_KERNEL)
    return "SI_KERNEL";
  return "other";
}

static void
where (unsigned long a, char *buf)
{
  if (a >= stub_lo && a < stub_lo + 0x2000)
    sprintf (buf, "%s+%#lx", stub_name, a - stub_lo);
  else
    sprintf (buf, "%#lx", a);
}

static void
rep_sig (int sig, siginfo_t *si, void *ucv)
{
  ucontext_t *uc = ucv;
  char b1[96], b2[96];
  where ((unsigned long) si->si_addr, b1);
  where (uc->uc_mcontext.cr0_hi & M48 & ~7UL, b2);
  printf ("      signal %d (%s), si_code %d (%s), si_addr %s, IP %s\n", sig,
          strsignal (sig), si->si_code, ill_code (sig, si->si_code), b1, b2);
  fflush (stdout);
  _exit (0);
}

static void
catch_all (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = rep_sig;
  sa.sa_flags = SA_SIGINFO;
  int s[] = { SIGILL, SIGSEGV, SIGBUS, SIGFPE, SIGTRAP };
  for (unsigned i = 0; i < sizeof s / sizeof s[0]; i++)
    sigaction (s[i], &sa, NULL);
}

static int cur;

static void
prep_child (void)
{
  catch_all ();
  stub_lo = (unsigned long) ent[cur].prep;
  stub_name = "kl_prep";
  long v = ent[cur].prep ();
  printf ("      sdisp %%ctpr1, %d: ta_tag %ld, opc %ld\n", ent[cur].n, v & 7,
          (v >> 3) & 3);
}

static void
call_child (void)
{
  catch_all ();
  stub_lo = (unsigned long) ent[cur].call;
  stub_name = "kl_call";
  printf ("      kl_call_%d: the call is the instruction before +%#lx\n",
          ent[cur].n, (unsigned long) ent[cur].ret - stub_lo);
  fflush (stdout);
  long out[2] = { 0, 0 };
  long r = ent[cur].call (__NR_getpid, 0, 0, 0, out);
  printf ("      returned %ld (getpid %d); %%g16 then %#lx, tag %ld\n", r,
          getpid (), out[0], out[1] & 0xff);
}

static int
changed (const unsigned char *b, int n)
{
  int k = 0;
  for (int i = 0; i < n; i++)
    k += b[i] != 0xaa;
  return k;
}

static void
e4_child (void)
{
  catch_all ();
  stub_lo = (unsigned long) kl_call_4;
  stub_name = "kl_call_4";
  long out[2];
  long r;
  printf ("      getpid %d, getuid %d\n", getpid (), getuid ());
  r = kl_call_4 (20, 0, 0, 0, out);
  printf ("      entry 4, +20 (getpid): %ld; %%g16 then %#lx, tag %ld\n", r,
          out[0], out[1] & 0xff);
  r = kl_call_4 (-20, 0, 0, 0, out);
  printf ("      entry 4, -20 (old table 20, getpid): %ld; %%g16 then %#lx, "
          "tag %ld\n", r, out[0], out[1] & 0xff);
  r = kl_call_4 (-24, 0, 0, 0, out);
  printf ("      entry 4, -24 (old table 24, getuid): %ld\n", r);
  unsigned cpu = 999, node = 999;
  r = kl_call_4 (327, (long) &cpu, (long) &node, 0, out);
  printf ("      entry 4, +327 (getcpu): %ld, cpu %u node %u\n", r, cpu, node);
  r = kl_call_4 (-327, (long) &cpu, (long) &node, 0, out);
  printf ("      entry 4, -327 (old table ends at 325): %ld\n", r);
  r = kl_call_4 (-119, 0, 0, 0, out);
  printf ("      entry 4, -119 (old table: none; 64-bit 119 is sigreturn): "
          "%ld\n", r);
  sigset_t s;
  sigemptyset (&s);
  sigaddset (&s, SIGUSR1);
  sigaddset (&s, SIGUSR2);
  sigprocmask (SIG_BLOCK, &s, NULL);
  unsigned char buf[16];
  memset (buf, 0xaa, sizeof buf);
  r = kl_call_3 (126, SIG_BLOCK, 0, (long) buf, out);
  printf ("      entry 3, 126 (sigprocmask, null set): %ld, %d bytes "
          "written, first word %#x\n", r, changed (buf, 16),
          *(unsigned *) buf);
  memset (buf, 0xaa, sizeof buf);
  r = kl_call_4 (126, SIG_BLOCK, 0, (long) buf, out);
  printf ("      entry 4, +126: %ld, %d bytes written\n", r,
          changed (buf, 16));
  memset (buf, 0xaa, sizeof buf);
  r = kl_call_4 (-126, SIG_BLOCK, 0, (long) buf, out);
  printf ("      entry 4, -126 (old table: compat sigprocmask), 64-bit "
          "pointer %p: %ld, %d bytes written, first word %#x\n", buf, r,
          changed (buf, 16), *(unsigned *) buf);
}

static void
part1 (void)
{
  printf ("1. system-call entries\n");
  printf ("  1a. what sdisp leaves in %%ctpr1 (no transfer)\n");
  for (cur = 0; cur < (int) (sizeof ent / sizeof ent[0]); cur++)
    {
      char w[32];
      sprintf (w, "prep %d", ent[cur].n);
      in_child (w, prep_child, 5);
    }
  printf ("  1b. a call through each (getpid number in %%b[0])\n");
  for (cur = 0; cur < (int) (sizeof ent / sizeof ent[0]); cur++)
    {
      char w[32];
      if (ent[cur].n == 4)
        continue;
      sprintf (w, "call entry %d", ent[cur].n);
      printf ("    entry %d:\n", ent[cur].n);
      in_child (w, call_child, 5);
    }
  printf ("  1c. entry 4\n");
  in_child ("entry 4", e4_child, 5);
}

/* 2.  Edited signal frames and %bgr.  */
static long io[160] __attribute__ ((aligned (16)));
static int fmode, fcode_bgr, fnosig;
static struct
{
  unsigned long g[16], gext[16];
  unsigned char gtag[16], gext_tag[16];
  unsigned bgr;
  int ip_in;
} fr;

#define V(n) (0x5a5a5a5a00000000UL | ((unsigned long) (n) << 8) | (n))
#define ED(i) (0x0b0b000000000000UL + 0x10001UL * (i))
#define DG 0x4444444444444444UL  /* bit 30 set in both halves */
#define EX(i) (0x6e6e6e6e00007e00UL + (i))

void kl_handler_c (int, siginfo_t *, void *);

void
kl_handler_c (int sig, siginfo_t *si, void *ucv)
{
  (void) sig;
  (void) si;
  ucontext_t *uc = ucv;
  unsigned long ip = uc->uc_mcontext.cr0_hi & M48 & ~7UL;
  fr.ip_in = (ip >= (unsigned long) kl_gspin
              && ip < (unsigned long) kl_gspin_end);
  for (int i = 0; i < 16; i++)
    {
      fr.g[i] = uc->uc_extra.g[i];
      fr.gtag[i] = uc->uc_extra.gtag[i];
      fr.gext[i] = uc->uc_extra.gext[i];
      fr.gext_tag[i] = uc->uc_extra.gext_tag[i];
    }
  fr.bgr = uc->uc_extra.bgr;
  switch (fmode)
    {
    case 'b':
      uc->uc_extra.bgr = 0x37f;
      break;
    case 'g':
      for (int i = 0; i < 16; i++)
        {
          uc->uc_extra.g[i] = ED (i);
          uc->uc_extra.gtag[i] = i == 3 || i == 12 ? 5 : 0;
          uc->uc_extra.gext[i] = EX (i);
          uc->uc_extra.gext_tag[i] = 0;
        }
      break;
    case 'q':
      uc->uc_extra.g[1] = ED (1);
      uc->uc_extra.gtag[1] = 3;
      break;
    case 'p':
      uc->uc_extra.g[0] = ED (0);
      uc->uc_extra.gtag[0] = 2;
      break;
    case 'd':
      uc->uc_extra.g[3] = DG;
      uc->uc_extra.gtag[3] = 5;
      break;
    }
  io[20] = 1;
}

/* Name a 64-bit value: one of the values the code loaded (V), one the
   handler wrote (E), or the number.  */
static const char *
vname (unsigned long v, char *b)
{
  for (int n = 16; n < 32; n++)
    if (v == V (n))
      {
        sprintf (b, "V%d", n);
        return b;
      }
  for (int i = 0; i < 16; i++)
    if (v == ED (i))
      {
        sprintf (b, "E%d", i);
        return b;
      }
  sprintf (b, "%#lx", v);
  return b;
}

/* Without a signal: stop kl_gspin from another thread after 100 ms, so
   that only interrupts (timer ticks) enter the kernel meanwhile.  */
static void *
stopper (void *arg)
{
  (void) arg;
  usleep (100000);
  io[20] = 1;
  return NULL;
}

static void
frame_child (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = kl_hbgr;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGALRM, &sa, NULL);
  catch_all ();
  stub_lo = (unsigned long) kl_gspin;
  stub_name = "kl_gspin";
  if (fmode == 'd')
    printf ("      (the addd of %%g19 is at kl_gspin+%#lx)\n",
            (unsigned long) kl_gspin_use19 - stub_lo);
  memset (io, 0, sizeof io);
  io[0] = fcode_bgr;
  for (int n = 16; n < 32; n++)
    io[n - 15] = V (n);
  io[20] = 0;
  kl_hbgr_val = -1;
  fr.bgr = 0xdead;
  pthread_t t;
  if (fnosig)
    pthread_create (&t, NULL, stopper, NULL);
  else
    {
      struct itimerval it = { { 0, 0 }, { 0, 20000 } };
      setitimer (ITIMER_REAL, &it, NULL);
    }
  kl_gspin (io);
  if (fnosig)
    {
      char b[32];
      printf ("      no signal; after 100 ms of spinning: %%bgr %#lx;"
              " %%g24..%%g31:", io[64] & 0x7ff);
      for (int k = 8; k < 16; k++)
        printf (" %s", vname (io[32 + k], b));
      printf ("\n");
      return;
    }
  char b[32];
  printf ("      the code ran under %%bgr %#lx (read back %#lx); the handler "
          "interrupted kl_gspin: %s\n", io[0], io[65] & 0x7ff,
          fr.ip_in ? "yes" : "NO");
  printf ("      the handler's own %%bgr: %#x; the frame's bgr: %#x\n",
          kl_hbgr_val, fr.bgr);
  printf ("      frame g[0..15] (V<n> = what the code put in %%g<n>):");
  for (int i = 0; i < 16; i++)
    printf ("%s %s/%d", i % 8 ? "" : "\n       ", vname (fr.g[i], b),
            fr.gtag[i]);
  printf ("\n      frame gext[0..15]/gext_tag:");
  for (int i = 0; i < 16; i++)
    printf ("%s %#lx/%d", i % 4 ? "" : "\n       ", fr.gext[i],
            fr.gext_tag[i]);
  printf ("\n      after sigreturn: %%bgr %#lx; %%g16..%%g31 value/tag/movfi"
#if defined __iset__ && __iset__ >= 5
          "/high half"
#endif
          ":", io[64] & 0x7ff);
  for (int k = 0; k < 16; k++)
    {
      printf ("%s %%g%d %s/%ld/%#lx", k % 4 ? "" : "\n       ", 16 + k,
              vname (io[32 + k], b), io[48 + k] & 0xff, io[66 + k] & 0xffff);
#if defined __iset__ && __iset__ >= 5
      printf ("/%#lx", io[97 + 2 * k]);
#endif
    }
  printf ("\n");
}

/* Writes of %g24..%g31 under a rotated %bgr, read back under the same
   %bgr and under cur 0, without any signal (the reference for the frame
   cases).  */
static void
grot_child (void)
{
  char b[32];
  for (int cur = 1; cur <= 3; cur++)
    for (int alu = 0; alu < 2; alu++)
      {
        memset (io, 0, sizeof io);
        io[0] = 0xff | (cur << 8);
        for (int n = 16; n < 32; n++)
          io[n - 15] = V (n);
        (alu ? kl_grot_alu : kl_grot_ld) (io);
        printf ("      cur %d, written by %s; read under cur %d:", cur,
                alu ? "addd" : "ldd ", cur);
        for (int k = 0; k < 8; k++)
          printf (" %s", vname (io[32 + k], b));
        printf ("; under cur 0:");
        for (int k = 0; k < 8; k++)
          printf (" %s", vname (io[40 + k], b));
        printf ("\n");
      }
  /* Written under cur 0 (physical 24 + p holds V(24 + p)), then read
     under cur c by an ALU operation and as the data of a store.  */
  for (int cur = 1; cur <= 3; cur++)
    {
      static const char *const how[] = { "addd", "std,2", "std,5" };
      memset (io, 0, sizeof io);
      io[0] = 0xff | (cur << 8);
      for (int n = 16; n < 32; n++)
        io[n - 15] = V (n);
      kl_grot_st (io);
      for (int w = 0; w < 3; w++)
        {
          printf ("      written under cur 0, read under cur %d by %-5s:",
                  cur, how[w]);
          for (int k = 0; k < 8; k++)
            printf (" %s", vname (io[32 + 8 * w + k], b));
          printf ("\n");
        }
    }
}

static void
part2 (void)
{
  static const struct
  {
    int m, code_bgr, nosig;
    const char *what;
  } c[] = {
    { 'n', 0x1ff, 0, "the code sets %bgr 0x1ff (cur 1); frame not edited" },
    { 'n', 0x2ff, 0, "the code sets %bgr 0x2ff (cur 2); frame not edited" },
    { 'n', 0x3ff, 0, "the code sets %bgr 0x3ff (cur 3); frame not edited" },
    { 'n', 0x1ff, 1, "the code sets %bgr 0x1ff (cur 1); no signal, only "
      "interrupts for 100 ms" },
    { 'b', 0x0ff, 0, "the code keeps %bgr 0x0ff; the handler sets the "
      "frame's bgr to 0x37f (cur 3, val 0x7f)" },
    { 'g', 0x0ff, 0, "the handler writes every g[i] = E<i>, gtag 0 except "
      "gtag[3] = gtag[12] = 5, every gext[i] = 0x6e6e6e6e00007e00 + i" },
    { 'q', 0x0ff, 0, "the handler writes g[1] = E1 with gtag[1] = 3 (low "
      "half tag 3)" },
    { 'p', 0x0ff, 0, "the handler writes g[0] = E0 with gtag[0] = 2 (low "
      "half tag 2)" },
    { 'd', 0x0ff, 0, "the handler writes g[3] = 0x4444444444444444 with "
      "gtag[3] = 5 (both halves diagnostic: tag 1, bit 30 set)" },
  };
  printf ("2. %%g24..%%g31 written under a rotated %%bgr, no signal "
          "(V<n> = the value meant for %%g<n>; %%g24..%%g31 in order)\n");
  in_child ("rotation", grot_child, 5);
  printf ("   sigreturn from a timer signal with an edited frame\n");
  for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++)
    {
      printf ("    %s\n", c[i].what);
      fmode = c[i].m;
      fcode_bgr = c[i].code_bgr;
      fnosig = c[i].nosig;
      char w[32];
      sprintf (w, "mode %c, %#x%s", c[i].m, c[i].code_bgr,
               c[i].nosig ? ", no signal" : "");
      in_child (w, frame_child, 5);
    }
}

/* 3.  What a switch into a context costs when another thread ran it
   last.  Two worker threads live until the process exits (a thread that
   exits after a switch corrupts later stacks on the 4C's kernel:
   kernel-interface.md, "Contexts and threads"); each run tells them
   their role and core.  */
#define CSTK (256 * 1024)
static char c_stk[CSTK] __attribute__ ((aligned (256)));
static ucontext_t uc_c, uc_home[2];
static ucontext_t *volatile c_back;
static volatile long c_tin, c_tout;
static int nround, warm = 200;
static volatile int cpu_of[2], role[2], alt, yield_wait, yield_round;
static volatile int phase, done_n, turn, round_no, last_runner;
static long *d_in, *d_back;
static char *d_other;

static void
c_fn (void)
{
  for (;;)
    {
      c_tin = kl_clkr ();
      c_tout = kl_clkr ();
      swapcontext (&uc_c, (ucontext_t *) c_back);
    }
}

static void
pin (int cpu)
{
  cpu_set_t cs;
  CPU_ZERO (&cs);
  CPU_SET (cpu, &cs);
  sched_setaffinity (0, sizeof cs, &cs);
}

/* Role 1: take rounds when turn == me until round_no reaches nround.  */
static void
run_rounds (int me)
{
  for (;;)
    {
      int t;
      while ((t = __atomic_load_n (&turn, __ATOMIC_ACQUIRE)) != me)
        {
          if (t < 0)
            return;
          if (yield_wait)
            sched_yield ();
        }
      int r = round_no;
      if (r >= nround)
        {
          __atomic_store_n (&turn, -1, __ATOMIC_RELEASE);
          return;
        }
      c_back = &uc_home[me];
      long t0 = kl_clkr ();
      swapcontext (&uc_home[me], &uc_c);
      long t3 = kl_clkr ();
      d_in[r] = c_tin - t0;
      d_back[r] = t3 - c_tout;
      d_other[r] = last_runner != me;
      last_runner = me;
      round_no = r + 1;
      if (yield_round)
        sched_yield ();
      __atomic_store_n (&turn, alt ? 1 - me : me, __ATOMIC_RELEASE);
    }
}

static void *
worker (void *arg)
{
  int me = (int) (long) arg, seen = 0;
  for (;;)
    {
      while (__atomic_load_n (&phase, __ATOMIC_ACQUIRE) == seen)
        usleep (1000);
      seen = phase;
      pin (cpu_of[me]);
      if (role[me] == 1)
        run_rounds (me);
      else if (role[me] == 2)
        while (__atomic_load_n (&turn, __ATOMIC_ACQUIRE) >= 0)
          sched_yield ();
      __atomic_fetch_add (&done_n, 1, __ATOMIC_ACQ_REL);
    }
  return NULL;
}

static int
lcmp (const void *a, const void *b)
{
  long x = *(const long *) a, y = *(const long *) b;
  return x < y ? -1 : x > y;
}

static void
stats (const char *what, const long *d, const char *other, int want)
{
  static long s[200000];
  int n = 0;
  for (int r = warm; r < nround; r++)
    if (other[r] == want)
      s[n++] = d[r];
  if (!n)
    return;
  qsort (s, n, sizeof s[0], lcmp);
  printf ("      %-58s n %5d  p10 %6ld  median %6ld  p90 %6ld\n", what, n,
          s[n / 10], s[n / 2], s[n * 9 / 10]);
}

/* rb: worker B's role (0 idle, 1 rounds, 2 yields on its core).  */
static void
cost_run (const char *what, int cpua, int cpub, int rb, int alternate,
          int ywait, int yround)
{
  printf ("    %s\n", what);
  fflush (stdout);
  cpu_of[0] = cpua;
  cpu_of[1] = cpub;
  role[0] = 1;
  role[1] = rb;
  alt = alternate;
  yield_wait = ywait;
  yield_round = yround;
  round_no = 0;
  last_runner = 0;
  done_n = 0;
  memset (&uc_c, 0, sizeof uc_c);
  uc_c.uc_stack.ss_sp = c_stk;
  uc_c.uc_stack.ss_size = CSTK;
  uc_c.uc_link = &uc_home[0];
  if (makecontext_e2k (&uc_c, c_fn, 0))
    {
      perror ("makecontext_e2k");
      return;
    }
  __atomic_store_n (&turn, 0, __ATOMIC_RELEASE);
  __atomic_fetch_add (&phase, 1, __ATOMIC_ACQ_REL);
  while (__atomic_load_n (&done_n, __ATOMIC_ACQUIRE) < 2)
    usleep (1000);
  if (freecontext_e2k (&uc_c))
    perror ("freecontext_e2k");
  stats ("into the context, the same thread ran it last", d_in, d_other, 0);
  stats ("into the context, the other thread ran it last", d_in, d_other, 1);
  stats ("back to the thread's own context", d_back, d_other, 0);
  stats ("back to the thread's own context (after the other kind)", d_back,
         d_other, 1);
}

static void
cost (int argc, char **argv)
{
  long ncpu = sysconf (_SC_NPROCESSORS_ONLN);
  nround = argc > 2 ? atoi (argv[2]) : 4000;
  if (nround > 200000)
    nround = 200000;
  if (nround <= warm)
    nround = warm + 1;
  int ca = argc > 4 ? atoi (argv[3]) : (int) ncpu - 1;
  int cb = argc > 4 ? atoi (argv[4]) : (int) ncpu - 2;
  int cf = argc > 5 ? atoi (argv[5]) : -1;
  d_in = calloc (nround, sizeof (long));
  d_back = calloc (nround, sizeof (long));
  d_other = calloc (nround, 1);
  printf ("3. one-way swapcontext, %%clkr cycles on the switching core "
          "(%d rounds, the first %d dropped)\n", nround, warm);
  pin (ca);
  long t0 = kl_clkr ();
  for (int i = 0; i < 20000; i++)
    syscall (SYS_getppid);
  printf ("    getppid on CPU %d, for scale: %ld\n", ca,
          (kl_clkr () - t0) / 20000);
  pin (ca == 0 ? 1 : 0);
  pthread_t a, b;
  pthread_create (&a, NULL, worker, (void *) 0L);
  pthread_create (&b, NULL, worker, (void *) 1L);
  char w[160];
  sprintf (w, "S: one thread (CPU %d) does every switch", ca);
  cost_run (w, ca, cb, 0, 0, 0, 0);
  sprintf (w, "Sy: one thread (CPU %d) does every switch and yields to a "
           "second thread on its core after each", ca);
  cost_run (w, ca, ca, 2, 0, 1, 1);
  sprintf (w, "X: two threads take turns (CPU %d and %d)", ca, cb);
  cost_run (w, ca, cb, 1, 1, 0, 0);
  sprintf (w, "Xs: two threads take turns on one core (CPU %d), waiting "
           "with sched_yield", ca);
  cost_run (w, ca, ca, 1, 1, 1, 0);
  if (cf >= 0)
    {
      sprintf (w, "Xf: two threads take turns (CPU %d and %d)", ca, cf);
      cost_run (w, ca, cf, 1, 1, 0, 0);
    }
  fflush (stdout);
  _exit (0);   /* the workers stay alive until the process ends */
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  if (argc > 1 && !strcmp (argv[1], "cost"))
    {
      cost (argc, argv);
      return 0;
    }
  int part = argc > 1 ? atoi (argv[1]) : 0;
  if (!part || part == 1)
    part1 ();
  if (!part || part == 2)
    part2 ();
  return 0;
}
