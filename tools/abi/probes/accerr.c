/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Which signal, si_code and si_addr the kernel gives for each way a
   program can run off its data stack, and for plain bad accesses, so
   that the kernels can be compared (spec/abi/kernel-interface.md,
   "Overflow signals": the 4C's ALT kernel reports a failed growth of
   the data stack as SEGV_ACCERR, the 8C2's and the 2C3's as
   SEGV_BNDERR).  The 4C's kernel is MCST's 6.1-1.9 tree, whose
   force_sigsegv_array_bounds sends SEGV_ACCERR; from 6.1-1.17 on its
   successor force_sigsegv_expand_stack sends SEGV_BNDERR (OpenE2K/linux
   commits 6a2673970ea0 and d639a5da9813, arch/e2k/kernel/traps.c).  The
   getsp case whose amount the kernel cannot read gives SEGV_BNDERR on
   both trees (GETSP_OP_FAIL), so it tells the two apart.

   Every case runs in a forked child with an alarm and a SIGSEGV/SIGBUS
   handler on an alternate stack.  The handler prints the signal, its
   code and address (relative to the regions the case set up and to the
   interrupted USD's bottom) and the /proc/self/maps line holding the
   address, then exits the child.  Frames are filled from the top down,
   64 bytes apart, so the first store that faults is the highest one.
   After each case the parent prints the kernel log lines that appeared
   meanwhile and mention a stack or a signal (dmesg; needs
   kernel.dmesg_restrict 0).

   Documented calls only (mmap, mprotect, munmap, pthread_*, getcontext,
   makecontext_e2k, swapcontext, setrlimit, sigaction, sigaltstack);
   every write goes into memory the probe mapped or its own stacks.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc  # or v5.8c2, v6.2c3
     $L -O2 accerr.c getsp.s -o accerr -lpthread         # getsp.s: gs_g8
     ./accerr [CASE]                                     # on hardware  */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#define M48 ((1UL << 48) - 1)
#define VA(x) ((unsigned long) (x) & M48)
#define KIB(n) ((size_t) (n) << 10)
#define PAGE 4096UL

/* The current bottom of the data stack: USD.lo's pointer minus
   USD.hi's size.  */
static unsigned long last_sp;

static unsigned long
usd_bottom (void)
{
  unsigned long lo, hi;
  /* One statement, so that no getsp lands between the two reads.  */
  __asm__ volatile ("rrd %%usd.lo, %0\n\trrd %%usd.hi, %1"
                    : "=r" (lo), "=r" (hi));
  last_sp = VA (lo);
  return VA (lo) - (hi >> 32);
}

/* Named address ranges of the current case.  */
struct region
{
  const char *name;
  unsigned long lo, hi;
};
static struct region rg[8];
static int nrg;

static void
region (const char *name, const void *lo, size_t len)
{
  if (nrg < 8)
    rg[nrg++] = (struct region) { name, (unsigned long) lo,
                                  (unsigned long) lo + len };
}

static unsigned long entry_bottom, last_bottom;
static int levels;

static const char *
map_of (unsigned long a)
{
  static char out[300];
  unsigned long below = 0, above = ~0UL;
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[256];
  snprintf (out, sizeof out, "?");
  while (f && fgets (line, sizeof line, f))
    {
      unsigned long lo, hi;
      if (sscanf (line, "%lx-%lx", &lo, &hi) != 2)
        continue;
      if (a >= lo && a < hi)
        {
          line[strcspn (line, "\n")] = 0;
          snprintf (out, sizeof out, "%s", line);
          fclose (f);
          return out;
        }
      if (hi <= a && hi > below)
        below = hi;
      if (lo > a && lo < above)
        above = lo;
    }
  if (f)
    fclose (f);
  snprintf (out, sizeof out, "unmapped (the gap %#lx-%#lx)", below, above);
  return out;
}

static const char *
code_name (int sig, int code)
{
  if (code == SI_KERNEL)
    return "SI_KERNEL";
  if (code == SI_USER)
    return "SI_USER";
  if (sig == SIGSEGV)
    switch (code)
      {
      case SEGV_MAPERR: return "SEGV_MAPERR";
      case SEGV_ACCERR: return "SEGV_ACCERR";
#ifdef SEGV_BNDERR
      case SEGV_BNDERR: return "SEGV_BNDERR";
#endif
      }
  if (sig == SIGBUS)
    switch (code)
      {
      case BUS_ADRALN: return "BUS_ADRALN";
      case BUS_ADRERR: return "BUS_ADRERR";
      case BUS_OBJERR: return "BUS_OBJERR";
      }
  return "?";
}

static void
on_fault (int sig, siginfo_t *si, void *ctx)
{
  ucontext_t *uc = ctx;
  unsigned long a = (unsigned long) si->si_addr;
  unsigned long isp = VA (uc->uc_mcontext.usd_lo);
  unsigned long ibot = isp - (uc->uc_mcontext.usd_hi >> 32);
  printf ("    RESULT %s %s (si_code %d), si_addr %#lx", strsignal (sig),
          code_name (sig, si->si_code), si->si_code, a);
  for (int i = 0; i < nrg; i++)
    if (a >= rg[i].lo && a < rg[i].hi)
      printf (" = %s + %#lx (its top - %#lx)", rg[i].name, a - rg[i].lo,
              rg[i].hi - a);
  for (int i = 0; i < nrg; i++)
    if (a == rg[i].lo - 1)
      printf (" = %s - 1", rg[i].name);
  printf ("\n    interrupted USD bottom %#lx: si_addr %+ld from it",
          ibot, (long) (a - ibot));
  if (entry_bottom)
    printf ("; bottom at the start %#lx; after the last of %d frames"
            " USD %#lx, bottom %#lx", entry_bottom, levels, last_sp,
            last_bottom);
  printf ("\n");
#ifdef si_lower
  if (sig == SIGSEGV && si->si_code == SEGV_BNDERR)
    printf ("    si_lower %p, si_upper %p\n", si->si_lower, si->si_upper);
#endif
  printf ("    si_addr is in: %s\n", map_of (a));
  fflush (stdout);
  _exit (0);
}

static char altbuf[KIB (64)], altbuf_t[KIB (64)];

static void
handlers (char *alt)
{
  stack_t ss = { .ss_sp = alt, .ss_size = KIB (64) };
  sigaltstack (&ss, NULL);
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = on_fault;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset (&sa.sa_mask);
  sigaction (SIGSEGV, &sa, NULL);
  sigaction (SIGBUS, &sa, NULL);
}

/* N frames of FRAME bytes, each from alloca (so each starts with a
   getsp) and filled from its top down.  */
static size_t frame;

static void __attribute__ ((noinline))
dive (int n)
{
  volatile unsigned char *p = __builtin_alloca (frame);
  for (long i = (long) frame - 8; i >= 0; i -= 64)
    p[i] = 0xa5;
  p[0] = 0xa5;
  last_bottom = usd_bottom ();
  levels++;
  if (n > 1)
    dive (n - 1);
  p[0] = 1;
}

/* The same with a fixed 4 KiB array, so that the getsp takes a
   literal rather than a register.  */
static void __attribute__ ((noinline))
dive_fixed (int n)
{
  volatile unsigned char p[KIB (4)];
  for (long i = (long) sizeof p - 8; i >= 0; i -= 64)
    p[i] = 0xa5;
  p[0] = 0xa5;
  last_bottom = usd_bottom ();
  levels++;
  if (n > 1)
    dive_fixed (n - 1);
  p[0] = 1;
}

static void
dive_from_here (size_t fr, int n)
{
  entry_bottom = last_bottom = usd_bottom ();
  frame = fr;
  printf ("    data stack bottom at the start %#lx (%s)\n", entry_bottom,
          map_of (entry_bottom));
  fflush (stdout);
  if (fr)
    dive (n);
  else
    dive_fixed (n);
  printf ("    RESULT none: %d frames of %#zx bytes ran\n", levels, fr);
}

/* --- Plain accesses, far from any stack.  */

static volatile long sink;

static unsigned char *
one_page (int prot, int unmap)
{
  unsigned char *p = mmap (NULL, 3 * PAGE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (unmap)
    munmap (p + PAGE, PAGE);
  else
    mprotect (p + PAGE, PAGE, prot);
  region ("the page", p + PAGE, PAGE);
  return p + PAGE + 0x40;
}

static void c_unmapped_load (void) { sink = *(volatile long *) one_page (0, 1); }
static void c_unmapped_store (void) { *(volatile long *) one_page (0, 1) = 1; }
static void c_null_load (void) { sink = *(volatile long *) 0x40; }
static void c_none_load (void) { sink = *(volatile long *) one_page (PROT_NONE, 0); }
static void c_none_store (void) { *(volatile long *) one_page (PROT_NONE, 0) = 1; }
static void c_ro_store (void) { *(volatile long *) one_page (PROT_READ, 0) = 1; }

/* Just below the main thread's [stack] mapping, without a getsp.  */
static unsigned long
below_stack (void)
{
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[256];
  unsigned long lo = 0, hi = 0;
  while (f && fgets (line, sizeof line, f))
    if (strstr (line, "[stack]"))
      sscanf (line, "%lx-%lx", &lo, &hi);
  if (f)
    fclose (f);
  region ("[stack]", (void *) lo, hi - lo);
  printf ("    [stack] %#lx-%#lx; data stack bottom %#lx\n", lo, hi,
          usd_bottom ());
  return lo - 0x40;
}

static void c_below_stack_load (void) { sink = *(volatile long *) below_stack (); }
static void c_below_stack_store (void) { *(volatile long *) below_stack () = 1; }

/* --- glibc's thread stacks: the PROT_NONE guard below them.  */

static int thread_kind;

static void *
glibc_thread (void *arg)
{
  (void) arg;
  handlers (altbuf_t);
  pthread_attr_t a;
  void *addr;
  size_t size, guard = 0;
  pthread_getattr_np (pthread_self (), &a);
  pthread_attr_getstack (&a, &addr, &size);
  pthread_attr_getguardsize (&a, &guard);
  unsigned char *g = (unsigned char *) addr - guard;
  region ("the guard", g, guard);
  region ("the stack", addr, size);
  printf ("    stack %p-%p, guard %#zx: %s\n", addr,
          (void *) ((char *) addr + size), guard, map_of ((unsigned long) g));
  fflush (stdout);
  switch (thread_kind)
    {
    case 0:
      *(volatile long *) (g + 0x40) = 1;
      break;
    case 1:
      sink = *(volatile long *) (g + 0x40);
      break;
    case 2:
      dive_from_here (KIB (4), 64);
      break;
    }
  printf ("    RESULT none\n");
  return NULL;
}

static void
run_glibc_thread (int kind)
{
  thread_kind = kind;
  pthread_attr_t a;
  pthread_attr_init (&a);
  pthread_attr_setstacksize (&a, KIB (64));
  pthread_t t;
  int r = pthread_create (&t, &a, glibc_thread, NULL);
  if (r)
    printf ("    pthread_create: %s\n", strerror (r));
  else
    pthread_join (t, NULL);
}

static void c_guard_store (void) { run_glibc_thread (0); }
static void c_guard_load (void) { run_glibc_thread (1); }
static void c_guard_frames (void) { run_glibc_thread (2); }

/* --- User stacks (pthread_attr_setstack, makecontext_e2k): a 64 KiB
   stack at AREA + 1 MiB in a mapping of 1 MiB + 64 KiB + 64 KiB, and
   below it
     'u' nothing (the lower 1 MiB unmapped),
     'g' a PROT_NONE page, then nothing,
     'h' one unmapped page, then the rest of the mapping,
     'H' a PROT_NONE page, a 64 KiB hole, then the rest of the mapping.  */

#define USTACK KIB (64)
static unsigned char *area, *ust;
static size_t ufr;
static int unr;

static void
user_stack (int below)
{
  size_t len = KIB (1024) + USTACK + KIB (64);
  area = mmap (NULL, len, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ust = area + KIB (1024);
  region ("the stack", ust, USTACK);
  switch (below)
    {
    case 'u':
      munmap (area, KIB (1024));
      break;
    case 'g':
      mprotect (ust - PAGE, PAGE, PROT_NONE);
      munmap (area, KIB (1024) - PAGE);
      region ("the guard", ust - PAGE, PAGE);
      break;
    case 'h':
      munmap (ust - PAGE, PAGE);
      region ("the hole", ust - PAGE, PAGE);
      region ("the mapping below", area, KIB (1024) - PAGE);
      break;
    case 'H':
      mprotect (ust - PAGE, PAGE, PROT_NONE);
      munmap (ust - PAGE - KIB (64), KIB (64));
      region ("the guard", ust - PAGE, PAGE);
      region ("the hole", ust - PAGE - KIB (64), KIB (64));
      region ("the mapping below", area, KIB (1024) - PAGE - KIB (64));
      break;
    }
  printf ("    stack %p-%p; below it: %s\n", (void *) ust,
          (void *) (ust + USTACK), map_of ((unsigned long) ust - 1));
}

static void *
user_thread (void *arg)
{
  (void) arg;
  handlers (altbuf_t);
  dive_from_here (ufr, unr);
  return NULL;
}

static void
run_user_thread (int below, size_t fr, int n)
{
  user_stack (below);
  ufr = fr;
  unr = n;
  pthread_attr_t a;
  pthread_attr_init (&a);
  pthread_attr_setstack (&a, ust, USTACK);
  pthread_t t;
  int r = pthread_create (&t, &a, user_thread, NULL);
  if (r)
    printf ("    pthread_create: %s\n", strerror (r));
  else
    pthread_join (t, NULL);
}

static void c_thread_unmapped (void) { run_user_thread ('u', KIB (4), 64); }
static void c_thread_unmapped_fixed (void) { run_user_thread ('u', 0, 64); }
static void c_thread_guard_unmapped (void) { run_user_thread ('g', KIB (4), 64); }
static void c_thread_hole_mapped (void) { run_user_thread ('h', KIB (4), 64); }
static void c_thread_guard_hole_mapped (void) { run_user_thread ('H', KIB (96), 2); }

static ucontext_t uc_main, uc_ctx;

static void
ctx_fn (void)
{
  dive_from_here (ufr, unr);
}

static void
run_context (int below, size_t fr, int n)
{
  user_stack (below);
  ufr = fr;
  unr = n;
  getcontext (&uc_ctx);
  uc_ctx.uc_stack.ss_sp = ust;
  uc_ctx.uc_stack.ss_size = USTACK;
  uc_ctx.uc_link = &uc_main;
  if (makecontext_e2k (&uc_ctx, ctx_fn, 0) != 0)
    {
      printf ("    makecontext_e2k: %s\n", strerror (errno));
      return;
    }
  swapcontext (&uc_main, &uc_ctx);
}

static void c_context_unmapped (void) { run_context ('u', KIB (4), 64); }
static void c_context_hole_mapped (void) { run_context ('h', KIB (4), 64); }

/* --- The main thread.  */

static void
c_main_rlimit (void)
{
  struct rlimit rl;
  getrlimit (RLIMIT_STACK, &rl);
  rl.rlim_cur = KIB (1024);
  setrlimit (RLIMIT_STACK, &rl);
  below_stack ();
  printf ("    RLIMIT_STACK %#lx\n", (unsigned long) rl.rlim_cur);
  dive_from_here (KIB (64), 64);
}

/* --- A getsp the kernel cannot decode: the amount in %g8, which it does
   not save (getsp.s gs_g8: getsp of minus its argument).  4 MiB is more
   than the main thread's USD covers, so the getsp traps.  */

long gs_g8 (long);

static void
c_getsp_g8 (void)
{
  region ("gs_g8", (void *) gs_g8, 0x200);
  printf ("    gs_g8 at %p; [stack] bottom %#lx\n", (void *) gs_g8,
          usd_bottom ());
  fflush (stdout);
  sink = gs_g8 (KIB (4096));
  printf ("    RESULT none\n");
}

/* --- The chain stack (no data stack involved).  */

static long __attribute__ ((noinline))
rec (long n)
{
  if (n == 0)
    return sink;
  long v = rec (n - 1);
  sink = v;
  return v + n;
}

static void
c_chain_rlimit (void)
{
#ifdef RLIMIT_PC_STACK
  struct rlimit rl;
  getrlimit (RLIMIT_PC_STACK, &rl);
  rl.rlim_cur = KIB (1024);
  setrlimit (RLIMIT_PC_STACK, &rl);
  printf ("    RLIMIT_PC_STACK %#lx; 100,000 nested calls\n",
          (unsigned long) rl.rlim_cur);
  fflush (stdout);
  sink = rec (100000);
  printf ("    RESULT none\n");
#else
  printf ("    RLIMIT_PC_STACK not defined\n");
#endif
}

struct ccase
{
  const char *name;
  void (*fn) (void);
};

static const struct ccase cases[] = {
  { "load from an unmapped page (mmap, munmap)", c_unmapped_load },
  { "store to an unmapped page", c_unmapped_store },
  { "load from address 0x40", c_null_load },
  { "load from a PROT_NONE page", c_none_load },
  { "store to a PROT_NONE page", c_none_store },
  { "store to a PROT_READ page", c_ro_store },
  { "load 64 bytes below the main thread's [stack], no getsp",
    c_below_stack_load },
  { "store 64 bytes below the main thread's [stack], no getsp",
    c_below_stack_store },
  { "glibc thread (64 KiB): store into the guard page", c_guard_store },
  { "glibc thread (64 KiB): load from the guard page", c_guard_load },
  { "glibc thread (64 KiB): 4 KiB frames into the guard", c_guard_frames },
  { "user thread stack, nothing mapped below: 4 KiB frames",
    c_thread_unmapped },
  { "user thread stack, nothing mapped below: 4 KiB arrays (literal getsp)",
    c_thread_unmapped_fixed },
  { "user thread stack, PROT_NONE page then nothing: 4 KiB frames",
    c_thread_guard_unmapped },
  { "user thread stack, one unmapped page then mapped: 4 KiB frames",
    c_thread_hole_mapped },
  { "user thread stack, PROT_NONE page, 64 KiB hole, mapped: 96 KiB frame",
    c_thread_guard_hole_mapped },
  { "made context, nothing mapped below: 4 KiB frames", c_context_unmapped },
  { "made context, one unmapped page then mapped: 4 KiB frames",
    c_context_hole_mapped },
  { "main thread, RLIMIT_STACK 1 MiB: 64 KiB frames", c_main_rlimit },
  { "main thread, RLIMIT_PC_STACK 1 MiB: deep recursion", c_chain_rlimit },
  { "main thread, getsp of 4 MiB with the amount in %g8", c_getsp_g8 },
};

/* The kernel log: the last line now, and the lines after it that
   mention a stack, a signal or this program.  (Matching by text, not by
   time stamp: the 2C3's printk clock runs about 0.13 s behind
   /proc/uptime.)  */
static char klog_last[512];
static int klog_n;              /* how many lines read like klog_last */

static void
klog_mark (void)
{
  FILE *p = popen ("dmesg 2>&1", "r");
  char line[512];
  klog_last[0] = 0;
  klog_n = 0;
  while (p && fgets (line, sizeof line, p))
    if (!strcmp (line, klog_last))
      klog_n++;
    else
      {
        snprintf (klog_last, sizeof klog_last, "%s", line);
        klog_n = 1;
      }
  if (p)
    pclose (p);
}

static void
klog_since_mark (void)
{
  FILE *p = popen ("dmesg 2>&1", "r");
  char line[512];
  int after = klog_last[0] == 0, seen = 0;
  while (p && fgets (line, sizeof line, p))
    {
      if (!after)
        {
          seen = strcmp (line, klog_last) ? 0 : seen + 1;
          after = seen == klog_n;
          continue;
        }
      if (strstr (line, "stack") || strstr (line, "SIG")
          || strstr (line, "accerr") || strstr (line, "signal"))
        printf ("    kernel log: %s", line);
    }
  if (p)
    pclose (p);
}

static void
in_child (const struct ccase *c)
{
  printf ("%s\n", c->name);
  fflush (stdout);
  klog_mark ();
  pid_t pid = fork ();
  if (pid == 0)
    {
      struct rlimit no = { 0, 0 };
      setrlimit (RLIMIT_CORE, &no);
      alarm (20);
      nrg = 0;
      handlers (altbuf);
      c->fn ();
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("    RESULT child killed by %s\n", strsignal (WTERMSIG (st)));
  usleep (100000);
  klog_since_mark ();
  fflush (stdout);
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  int only = argc > 1 ? atoi (argv[1]) : -1;
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++)
    if (only < 0 || (size_t) only == i)
      {
        printf ("%zu. ", i);
        in_child (&cases[i]);
      }
  return 0;
}
