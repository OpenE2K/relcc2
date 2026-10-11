/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What the kernel interface does for ordinary user code
   (spec/abi/kernel-interface.md): the FP environment across signal
   handlers, the smallest alternate signal stack, where a signal frame
   goes, what happens when the hardware stacks run out, the auxiliary
   vector and address space, %clkr across cores, UPSR.ac, system calls
   through the uapi macros and what calls cost, and the ucontext_t
   layout.  `./sysabi maps` prints only the address space (run it from
   -fPIE -pie and -fPIE -static-pie builds too), `./sysabi deep` only
   the deep recursion in the main process.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O2 sysabi.c sysabi.s -o sysabi -lpthread -lm
     ./sysabi                              # on hardware  */

#define _GNU_SOURCE
#include <errno.h>
#include <fenv.h>
#include <pthread.h>
#include <setjmp.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <asm/e2k_api.h>
#include <asm/unistd.h>

long sa_clkr (void);
long sa_sp (void);
long sa_rd_upsr (void);
void sa_wr_upsr (long);
long sa_ldw (char *);
long sa_ldd (char *);
void sa_down (long);

#define RLIMIT_P_STACK_E2K 10001   /* glibc's numbers (bits/resource.h) */
#define RLIMIT_PC_STACK_E2K 10002

/* Run FN in a child; print how it ended.  */
static void
in_child (const char *what, void (*fn) (void))
{
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      fn ();
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("  [%s: killed by signal %d]\n", what, WTERMSIG (st));
  else if (WEXITSTATUS (st))
    printf ("  [%s: exit %d]\n", what, WEXITSTATUS (st));
  fflush (stdout);
}

/* 1. The FP environment across a handler.  */
static volatile int h_round, h_flags, h_ran;

static void
fp_handler (int sig)
{
  (void) sig;
  h_round = fegetround ();
  h_flags = fetestexcept (FE_ALL_EXCEPT);
  fesetround (FE_TOWARDZERO);
  feclearexcept (FE_ALL_EXCEPT);
  feraiseexcept (FE_OVERFLOW);
  h_ran = 1;
}

static void
fp_case (int async)
{
  signal (async ? SIGALRM : SIGUSR1, fp_handler);
  feclearexcept (FE_ALL_EXCEPT);
  fesetround (FE_UPWARD);
  feraiseexcept (FE_INEXACT);
  h_ran = 0;
  if (async)
    {
      struct itimerval it = { { 0, 0 }, { 0, 20000 } };
      setitimer (ITIMER_REAL, &it, NULL);
      volatile double x = 1.0;
      while (!h_ran)
        x = x * 1.0;
    }
  else
    raise (SIGUSR1);
  int r = fegetround (), f = fetestexcept (FE_ALL_EXCEPT);
  printf ("  %s: the handler saw rounding %s, flags %#x; after it the "
          "interrupted code has rounding %s, flags %#x\n",
          async ? "timer signal" : "raise   ",
          h_round == FE_UPWARD ? "upward (the interrupted code's)"
          : h_round == FE_TONEAREST ? "to nearest (fresh)" : "other",
          h_flags,
          r == FE_UPWARD ? "upward (kept)"
          : r == FE_TOWARDZERO ? "toward zero (the handler's)" : "other",
          f);
  fesetround (FE_TONEAREST);
  feclearexcept (FE_ALL_EXCEPT);
}

/* 2. Alternate signal stacks of several sizes.  */
static volatile long alt_used, alt_frame;
static char *alt_base;
static long alt_size;

static void
alt_handler (int sig, siginfo_t *si, void *uc)
{
  (void) sig;
  (void) uc;
  volatile char here;
  alt_used = alt_base + alt_size - (char *) &here;
  alt_frame = alt_base + alt_size - ((char *) si - 64);
}

static void
alt_case (void)
{
  alt_base = malloc (alt_size);
  stack_t ss = { .ss_sp = alt_base, .ss_size = alt_size, .ss_flags = 0 };
  if (sigaltstack (&ss, NULL))
    {
      printf ("  %6ld bytes: sigaltstack refused (%s)\n", alt_size,
              strerror (errno));
      return;
    }
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = alt_handler;
  sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
  sigaction (SIGUSR1, &sa, NULL);
  alt_used = -1;
  raise (SIGUSR1);
  printf ("  %6ld bytes (base %% 256 = %ld): the handler ran; the frame "
          "starts %ld bytes below the top, the handler's local %ld\n",
          alt_size, (long) alt_base % 256, alt_frame, alt_used);
}

/* 3. Where the frame goes without an alternate stack.  */
static volatile long frame_local, frame_usd, frame_si, frame_uc;
static volatile long frame_tirs, frame_cnt, frame_tc;

static void
frame_handler (int sig, siginfo_t *si, void *ucv)
{
  (void) sig;
  volatile char here;
  ucontext_t *uc = ucv;
  frame_local = (long) &here;
  frame_usd = (long) uc->uc_mcontext.usd_lo & ((1L << 48) - 1);
  frame_si = (long) si;
  frame_uc = (long) uc;
  frame_tirs = (long) uc->uc_mcontext.nr_TIRs;
  frame_cnt = uc->uc_extra.curr_cnt;
  frame_tc = uc->uc_extra.tc_count;
}

/* The arguments a handler without SA_SIGINFO gets.  */
static volatile long plain_a1, plain_a2, plain_local;

static void
plain_handler (int sig, long a1, long a2)
{
  (void) sig;
  volatile char here;
  plain_local = (long) &here;
  plain_a1 = a1;
  plain_a2 = a2;
}

/* A fault: what the frame says about it.  */
static sigjmp_buf fault_env;
static volatile long fault_addr, fault_cell;

static void
fault_handler (int sig, siginfo_t *si, void *ucv)
{
  (void) sig;
  ucontext_t *uc = ucv;
  fault_addr = (long) si->si_addr;
  frame_tirs = (long) uc->uc_mcontext.nr_TIRs;
  frame_cnt = uc->uc_extra.curr_cnt;
  frame_tc = uc->uc_extra.tc_count;
  fault_cell = frame_cnt >= 0 && frame_cnt < 7
               ? (long) uc->uc_mcontext.trap_cell_addr[frame_cnt] : -1;
  siglongjmp (fault_env, 1);
}

/* 4. Running out of the hardware stacks.  */
static volatile long *depth;   /* shared with the parent */
static volatile int *seg_ran;

static int __attribute__ ((noinline))
recurse (int n)
{
  volatile char pad[16];
  pad[0] = (char) n;
  *depth = n;
  return n > 100000000 ? 0 : recurse (n + 1) + pad[0];
}

static void
seg_handler (int sig)
{
  (void) sig;
  *seg_ran = 1;
  _exit (3);
}

static long stack_limit, stack_which;

static void
stack_case (void)
{
  alarm (120);
  if (stack_limit)
    {
      struct rlimit rl = { stack_limit, stack_limit };
      if (setrlimit (stack_which, &rl))
        printf ("  setrlimit: %s\n", strerror (errno));
    }
  static char alt[1 << 16];
  stack_t ss = { .ss_sp = alt, .ss_size = sizeof alt, .ss_flags = 0 };
  sigaltstack (&ss, NULL);
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_handler = seg_handler;
  sa.sa_flags = SA_ONSTACK;
  sigaction (SIGSEGV, &sa, NULL);
  recurse (1);
}

static double
now (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

static int __attribute__ ((noinline))
down (int n)
{
  volatile char pad[16];
  pad[0] = (char) n;
  return n == 0 ? 0 : down (n - 1) + pad[0];
}

static double stamps[64];

static int __attribute__ ((noinline))
down_timed (int n)
{
  volatile char pad[16];
  pad[0] = (char) n;
  if (n % 25000 == 0)
    stamps[n / 25000] = now ();
  return n == 0 ? 0 : down_timed (n - 1) + pad[0];
}

/* The same in a thread (its stack is mapped whole), and in the main
   thread after one large frame grew the stack.  */
static void *
deep_thread (void *arg)
{
  (void) arg;
  stamps[10] = now ();
  down_timed (250000 - 1);
  printf ("    in a thread (32 MiB stack), per 25000 calls:");
  for (int k = 9; k >= 0; k--)
    printf (" %.3f", stamps[k] - stamps[k + 1]);
  printf ("\n");
  return NULL;
}

static void
thread_deep_case (void)
{
  pthread_attr_t a;
  pthread_attr_init (&a);
  pthread_attr_setstacksize (&a, 32 << 20);
  pthread_t t;
  pthread_create (&t, &a, deep_thread, NULL);
  pthread_join (t, NULL);
}

static void __attribute__ ((noinline))
big_frame (void)
{
  volatile char *p = __builtin_alloca (12 << 20);
  p[0] = 0;
}

static void
pregrown_case (void)
{
  double t0 = now ();
  big_frame ();
  double t1 = now ();
  stamps[10] = now ();
  down_timed (250000 - 1);
  printf ("    after one 12 MiB frame (%.3f s), per 25000 calls:", t1 - t0);
  for (int k = 9; k >= 0; k--)
    printf (" %.3f", stamps[k] - stamps[k + 1]);
  printf ("\n");
}

/* The same without a data-stack frame (assembly), each depth in a fresh
   child.  */
static long nodata_n;

static void
nodata_one (void)
{
  double t0 = now ();
  sa_down (nodata_n);
  double t1 = now ();
  sa_down (nodata_n);
  double t2 = now ();
  printf ("    no data frame, depth %6ld: first %.2f s, again %.3f s\n",
          nodata_n, t1 - t0, t2 - t1);
}

static void
nodata_case (void)
{
  for (nodata_n = 25000; nodata_n <= 200000; nodata_n *= 2)
    in_child ("no data frame", nodata_one);
}

static void
deep_case (void)
{
  stamps[10] = now ();
  down_timed (250000 - 1);
  printf ("    first descent to 250000, seconds per 25000 calls:");
  for (int k = 9; k >= 0; k--)
    printf (" %.2f", stamps[k] - stamps[k + 1]);
  printf ("\n");
  stamps[10] = now ();
  down_timed (250000 - 1);
  printf ("    second descent:");
  for (int k = 9; k >= 0; k--)
    printf (" %.3f", stamps[k] - stamps[k + 1]);
  printf ("\n");
}

static void
print_hw_maps (const char *who)
{
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    if (strstr (line, "stack"))
      printf ("    %s: %s", who, line);
  if (f)
    fclose (f);
}

static void *
thread_fn (void *arg)
{
  (void) arg;
  print_hw_maps ("thread");
  return NULL;
}

static void
thread_case (void)
{
  down (100000);
  print_hw_maps ("main");
  pthread_t t;
  pthread_create (&t, NULL, thread_fn, NULL);
  pthread_join (t, NULL);
}

/* 7. %clkr on two CPUs.  */
static volatile long ping, pong_clk;
static int cpu_b;

static void *
clk_peer (void *arg)
{
  (void) arg;
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (cpu_b, &s);
  sched_setaffinity (0, sizeof s, &s);
  ping = 1;
  while (ping != 2)
    ;
  pong_clk = sa_clkr ();
  ping = 3;
  return NULL;
}

static void
pin (int cpu)
{
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (cpu, &s);
  sched_setaffinity (0, sizeof s, &s);
}

/* 8. UPSR.ac.  Nothing may run with ac set but the access under test:
   glibc itself makes misaligned accesses (on v6, printf does).  */
static long ac_saved;
static sigjmp_buf ac_env;
static volatile long ac_sig, ac_code, ac_addr;

static void
ac_handler (int sig, siginfo_t *si, void *uc)
{
  (void) uc;
  sa_wr_upsr (ac_saved);
  ac_sig = sig;
  ac_code = si->si_code;
  ac_addr = (long) si->si_addr;
  siglongjmp (ac_env, 1);
}

static void
ac_try (const char *what, char *p, int dbl)
{
  ac_sig = 0;
  long v = 0;
  if (!sigsetjmp (ac_env, 1))
    {
      sa_wr_upsr (ac_saved | 4);
      v = dbl ? sa_ldd (p) : sa_ldw (p);
      sa_wr_upsr (ac_saved);
    }
  if (ac_sig)
    printf ("  %s: signal %ld, si_code %ld, si_addr = data %+ld, = the "
            "load function %+ld\n", what, ac_sig, ac_code,
            ac_addr - (long) p,
            ac_addr - (long) (dbl ? (void *) sa_ldd : (void *) sa_ldw));
  else
    printf ("  %s: no trap (%ld)\n", what, v);
}

static void
ac_case (void)
{
  static char buf[64] __attribute__ ((aligned (64)));
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = ac_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGBUS, &sa, NULL);
  sigaction (SIGSEGV, &sa, NULL);
  sigaction (SIGILL, &sa, NULL);
  ac_saved = sa_rd_upsr ();
  printf ("  %%upsr is %#lx\n", ac_saved);
  ac_try ("aligned ldd", buf + 8, 1);
  ac_try ("ldw at +1", buf + 1, 0);
  ac_try ("ldw at +2", buf + 2, 0);
  ac_try ("ldd at +4", buf + 4, 1);
  sa_wr_upsr (ac_saved);
}

/* 9. System calls through the kernel's uapi macros, and what a call
   costs.  */
static long
uapi_getpid (void)
{
  return __E2K_SYSCALL_0 (LINUX_SYSCALL64_TRAPNUM, __NR_getpid, 0);
}

static long
uapi_write (long fd, const void *buf, long n)
{
  return E2K_SYSCALL (LINUX_SYSCALL64_TRAPNUM, __NR_write, 3, fd, buf, n);
}

static void
syscall_case (void)
{
  printf ("  uapi __E2K_SYSCALL_0 (getpid) %ld, getpid () %ld\n",
          uapi_getpid (), (long) getpid ());
  fflush (stdout);
  long r = uapi_write (1, "  uapi E2K_SYSCALL (write) printed this\n", 40);
  printf ("  ... and returned %ld; a bad descriptor gives %ld\n", r,
          uapi_write (-1, "", 0));
  struct timespec ts;
  long n = 100000, t0 = sa_clkr ();
  for (long i = 0; i < n; i++)
    getppid ();
  long t1 = sa_clkr ();
  for (long i = 0; i < n; i++)
    clock_gettime (CLOCK_MONOTONIC, &ts);
  long t2 = sa_clkr ();
  for (long i = 0; i < n; i++)
    clock_gettime (CLOCK_PROCESS_CPUTIME_ID, &ts);
  long t3 = sa_clkr ();
  sigset_t set;
  for (long i = 0; i < n; i++)
    sigprocmask (SIG_BLOCK, NULL, &set);
  long t4 = sa_clkr ();
  printf ("  cycles per call: getppid %ld, clock_gettime (MONOTONIC, a "
          "fast call) %ld, clock_gettime (PROCESS_CPUTIME) %ld, "
          "sigprocmask (query, a fast call) %ld\n", (t1 - t0) / n,
          (t2 - t1) / n, (t3 - t2) / n, (t4 - t3) / n);
}

/* ucontext_t layout (what lcc's headers say; printed for reference).  */
#define OFF(f) printf ("    %-28s %#5lx\n", #f, (long) offsetof (ucontext_t, f))

static void
offsets (void)
{
  printf ("  sizeof: ucontext_t %#lx, struct sigcontext %#lx, struct "
          "extra_ucontext %#lx, siginfo_t %#lx\n", (long) sizeof (ucontext_t),
          (long) sizeof (struct sigcontext),
          (long) sizeof (struct extra_ucontext), (long) sizeof (siginfo_t));
  OFF (uc_flags); OFF (uc_link); OFF (uc_stack); OFF (uc_mcontext);
  OFF (uc_mcontext.cr0_hi); OFF (uc_mcontext.cr1_lo); OFF (uc_mcontext.sbr);
  OFF (uc_mcontext.usd_lo); OFF (uc_mcontext.psp_lo);
  OFF (uc_mcontext.pcsp_lo); OFF (uc_mcontext.nr_TIRs);
  OFF (uc_mcontext.tir_lo); OFF (uc_mcontext.tir_hi);
  OFF (uc_mcontext.trap_cell_addr); OFF (uc_mcontext.trap_cell_val);
  OFF (uc_mcontext.trap_cell_tag); OFF (uc_mcontext.trap_cell_info);
  OFF (uc_mcontext.dam); OFF (uc_mcontext.sbbp); OFF (uc_mcontext.mlt);
  OFF (uc_mcontext.upsr); OFF (uc_sigmask); OFF (uc_extra);
  OFF (uc_extra.curr_cnt); OFF (uc_extra.tc_count); OFF (uc_extra.fpcr);
  OFF (uc_extra.pfpfr); OFF (uc_extra.ctpr1); OFF (uc_extra.sc_need_rstrt);
  OFF (uc_extra.ctpr1_hi); OFF (uc_extra.chain_stack_offset);
  OFF (uc_extra.proc_stack_offset); OFF (uc_extra.bgr); OFF (uc_extra.g);
  OFF (uc_extra.gtag); OFF (uc_extra.gext); OFF (uc_extra.gext_tag);
  OFF (uc_extra.lsr); OFF (uc_extra.ilcr); OFF (uc_extra.lsr1);
  OFF (uc_extra.aad); OFF (uc_extra.aasti);
}

int main (int, char **);

static void
maps (void)
{
  void *heap = malloc (100);
  void *big = mmap (NULL, 1 << 20, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  volatile int local;
  printf ("  main %p, heap %p, mmap %p, stack %p\n", (void *) main, heap,
          big, (void *) &local);
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    printf ("    %s", line);
  if (f)
    fclose (f);
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  if (argc > 1 && !strcmp (argv[1], "maps"))
    {
      maps ();
      return 0;
    }
  if (argc > 1 && !strcmp (argv[1], "deep"))
    {
      printf ("  in the main process, no fork:\n");
      deep_case ();
      return 0;
    }
  depth = mmap (NULL, 4096, PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  seg_ran = (volatile int *) (depth + 1);

  printf ("1. FP environment across a signal handler "
          "(main: upward rounding, inexact raised; handler: reads, then "
          "sets toward zero and overflow):\n");
  fp_case (0);
  fp_case (1);

  printf ("2. alternate signal stack sizes (MINSIGSTKSZ %ld, SIGSTKSZ %ld; "
          "sysconf: %ld, %ld):\n", (long) MINSIGSTKSZ, (long) SIGSTKSZ,
          sysconf (_SC_MINSIGSTKSZ), sysconf (_SC_SIGSTKSZ));
  static const long sizes[] = { 2048, 4096, 4608, 4672, 4736, 4800, 4864,
                                4928, 4992, 5120, 8192, 16384 };
  for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++)
    {
      alt_size = sizes[i];
      char what[32];
      snprintf (what, sizeof what, "%ld bytes", sizes[i]);
      in_child (what, alt_case);
    }

  printf ("3. signal frame without an alternate stack:\n");
  {
    struct sigaction sa;
    memset (&sa, 0, sizeof sa);
    sa.sa_sigaction = frame_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction (SIGUSR2, &sa, NULL);
    long sp = sa_sp ();
    raise (SIGUSR2);
    printf ("  our stack pointer %#lx; interrupted USD (in raise) %#lx; "
            "siginfo at USD - %ld, ucontext at siginfo + %ld, the "
            "handler's local at USD - %ld\n", sp, frame_usd,
            frame_usd - frame_si, frame_uc - frame_si,
            frame_usd - frame_local);
    printf ("  raise: nr_TIRs %ld, curr_cnt %ld, tc_count %ld\n",
            frame_tirs, frame_cnt, frame_tc);
    long lo = 1L << 30, hi = 0;
    for (int k = 0; k < 16; k++)
      {
        char *p = __builtin_alloca (16 * k + 8);
        p[0] = 0;
        raise (SIGUSR2);
        long off = frame_usd - (frame_si - 64);
        lo = off < lo ? off : lo;
        hi = off > hi ? off : hi;
      }
    printf ("  over 16 stack pointers 16 bytes apart the frame starts "
            "%ld to %ld bytes below USD\n", lo, hi);
    long uc_local = frame_uc - frame_local;
    sa.sa_handler = (void (*) (int)) plain_handler;
    sa.sa_flags = 0;
    sigaction (SIGUSR2, &sa, NULL);
    raise (SIGUSR2);
    printf ("  without SA_SIGINFO: second argument at the handler's local "
            "+ %ld (with SA_SIGINFO the ucontext is at + %ld, uc_mcontext "
            "at + %ld), third %#lx\n", plain_a1 - plain_local, uc_local,
            uc_local + (long) offsetof (ucontext_t, uc_mcontext), plain_a2);
    sa.sa_sigaction = fault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction (SIGSEGV, &sa, NULL);
    if (!sigsetjmp (fault_env, 1))
      sa_ldd ((char *) 0x1000);
    printf ("  load fault at 0x1000: si_addr %#lx, nr_TIRs %ld, curr_cnt "
            "%ld, tc_count %ld, trap_cell_addr[curr_cnt] %#lx\n",
            fault_addr, frame_tirs, frame_cnt, frame_tc, fault_cell);
    signal (SIGSEGV, SIG_DFL);
  }

  printf ("4. running out of the hardware stacks (recursion, 16-byte "
          "frames; a SIGSEGV handler on an alternate stack):\n");
  {
    struct rlimit p, pc, s;
    getrlimit (RLIMIT_P_STACK_E2K, &p);
    getrlimit (RLIMIT_PC_STACK_E2K, &pc);
    getrlimit (RLIMIT_STACK, &s);
    printf ("  limits: RLIMIT_P_STACK %ld (max %ld), RLIMIT_PC_STACK %ld "
            "(max %ld), RLIMIT_STACK %ld\n", (long) p.rlim_cur,
            (long) p.rlim_max, (long) pc.rlim_cur, (long) pc.rlim_max,
            (long) s.rlim_cur);
  }
  static const struct { const char *what; long which, limit; } lim[] = {
    { "chain stack limit 1 MiB", RLIMIT_PC_STACK_E2K, 1 << 20 },
    { "procedure stack limit 1 MiB", RLIMIT_P_STACK_E2K, 1 << 20 },
    { "default limits", 0, 0 },
  };
  for (unsigned i = 0; i < 3; i++)
    {
      *depth = 0;
      *seg_ran = 0;
      stack_limit = lim[i].limit;
      stack_which = lim[i].which;
      struct timespec t0, t1;
      clock_gettime (CLOCK_MONOTONIC, &t0);
      in_child (lim[i].what, stack_case);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      printf ("  %s: reached depth %ld, the SIGSEGV handler %s (%.2f s)\n",
              lim[i].what, *depth, *seg_ran ? "ran" : "did not run",
              (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);
    }

  printf ("  deep recursion that returns (in a child):\n");
  in_child ("recursion", deep_case);
  nodata_case ();
  in_child ("thread", thread_deep_case);
  in_child ("pregrown", pregrown_case);
  printf ("  a new thread's hardware stacks after the main thread's grew "
          "(depth 100000):\n");
  in_child ("thread", thread_case);

  printf ("5. auxiliary vector:\n");
  {
    const char *plat = (const char *) getauxval (AT_PLATFORM);
    printf ("  AT_HWCAP %#lx, AT_HWCAP2 %#lx, AT_PLATFORM %s, "
            "AT_SYSINFO_EHDR %#lx, AT_PAGESZ %lu, AT_MINSIGSTKSZ %lu\n",
            getauxval (AT_HWCAP), getauxval (AT_HWCAP2),
            plat ? plat : "(none)", getauxval (AT_SYSINFO_EHDR),
            getauxval (AT_PAGESZ), getauxval (51));
    printf ("  E2K: 32 (fast syscalls) %#lx, 34 (system info) %#lx, "
            "35 %#lx\n", getauxval (32), getauxval (34), getauxval (35));
  }

  printf ("6. address space (/proc/self/maps):\n");
  maps ();

  printf ("7. %%clkr across CPUs:\n");
  {
    long ncpu = sysconf (_SC_NPROCESSORS_ONLN);
    for (int k = 0; k < 2; k++)
      {
        cpu_b = k == 0 ? 1 : (int) ncpu - 1;
        if (cpu_b >= ncpu || (k == 1 && cpu_b == 1))
          continue;
        pin (0);
        pthread_t t;
        ping = 0;
        pthread_create (&t, NULL, clk_peer, NULL);
        while (ping != 1)
          ;
        long a0 = sa_clkr ();
        ping = 2;
        while (ping != 3)
          ;
        long a1 = sa_clkr ();
        pthread_join (t, NULL);
        printf ("  CPU 0 vs CPU %d: the other's %%clkr minus ours "
                "%+ld cycles (round trip %ld)\n", cpu_b,
                pong_clk - (a0 + a1) / 2, a1 - a0);
      }
    pin (0);
    long c0 = sa_clkr ();
    pin ((int) ncpu - 1);
    long c1 = sa_clkr ();
    pin (0);
    long c2 = sa_clkr ();
    printf ("  one thread moved 0 -> %ld -> 0: %%clkr %+ld, %+ld\n",
            ncpu - 1, c1 - c0, c2 - c1);
  }

  printf ("8. UPSR.ac (bit 2) and misaligned loads, in a child:\n");
  in_child ("UPSR.ac", ac_case);

  printf ("9. system calls:\n");
  syscall_case ();

  printf ("10. ucontext_t layout:\n");
  offsets ();
  return 0;
}
