/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The stack and window registers read as registers, the way a garbage
   collector, a fiber library or an unwinder would read them
   (spec/abi/stack-and-unwinding.md, "Stack registers").  Only reads:
   nothing writes a state register.

     1. user-mode access: each register read once in a child process;
     2. main thread at several call depths: %psp/%pshtp, %pcsp/%pcshtp
        (read together under a lock-wait load and retried on %MLOCK, as
        glibc's setjmp reads %pcsp/%pcshtp and as MCST libgcc's unused
        E2K_READ_STACK_REGS macro would), %usd, %sbr, %wd, %br, %cr1,
        decoded and compared with /proc/self/maps and with the sizes
        access_hw_stacks reports;
     3. what getcontext stores for the same registers;
     4. the same in a thread;
     5. the same in a makecontext_e2k context;
     6. (`./stackregs grow`, a fresh process) the steps in which the
        kernel grows the main thread's data stack, seen in %usd.hi;
     7. (`./stackregs overflow`) what happens when a makecontext_e2k
        context's data stack, carved from a larger mapping, overflows:
        into mapped memory, a PROT_NONE page, an unmapped page or a
        large unmapped gap.

     The registers are the ones glibc's setjmp and getcontext, MCST's
     libgcc and the kernel's getcontext read, plus %sbr and %br.  MCST's
     qemu stops ("exception handling is unimplemented") on rrd of %psp,
     %pshtp and %sbr and on rrs %br, so run it on hardware.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   # or v5.8c2, v6.2c3
     $L -O2 stackregs.c stackregs.s -o stackregs -lpthread
     ./stackregs; ./stackregs grow; ./stackregs overflow   # on hardware  */

#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#include <asm/e2k_syswork.h>

long sr_usd_lo (void), sr_usd_hi (void), sr_psp_lo (void), sr_psp_hi (void);
long sr_pshtp (void), sr_pcsp_lo (void), sr_pcsp_hi (void), sr_pcshtp (void);
long sr_pcshtp_s (void), sr_sbr (void), sr_wd (void), sr_br_s (void);
long sr_cr1_lo (void), sr_cr1_hi (void), sr_cr1_hi_s (void);
void sr_snap (unsigned long *);

#define VA(x) ((x) & 0xffffffffffffUL)
#define BITS(x, lo, n) (((x) >> (lo)) & ((1UL << (n)) - 1))

static long
sext (unsigned long v, int bits)
{
  return (long) (v << (64 - bits)) >> (64 - bits);
}

/* 1.  */
static const struct
{
  const char *name;
  long (*fn) (void);
  int s;
} regs[] = {
  { "rrd %usd.lo", sr_usd_lo, 0 },   { "rrd %usd.hi", sr_usd_hi, 0 },
  { "rrd %psp.lo", sr_psp_lo, 0 },   { "rrd %psp.hi", sr_psp_hi, 0 },
  { "rrd %pshtp", sr_pshtp, 0 },     { "rrd %pcsp.lo", sr_pcsp_lo, 0 },
  { "rrd %pcsp.hi", sr_pcsp_hi, 0 }, { "rrd %pcshtp", sr_pcshtp, 0 },
  { "rrs %pcshtp", sr_pcshtp_s, 1 }, { "rrd %sbr", sr_sbr, 0 },
  { "rrd %wd", sr_wd, 0 },           { "rrs %br", sr_br_s, 1 },
  { "rrd %cr1.lo", sr_cr1_lo, 0 },   { "rrd %cr1.hi", sr_cr1_hi, 0 },
  { "rrs %cr1.hi", sr_cr1_hi_s, 1 },
};

static void
access_case (void)
{
  for (unsigned i = 0; i < sizeof regs / sizeof regs[0]; i++)
    {
      fflush (stdout);
      pid_t pid = fork ();
      if (pid == 0)
        {
          unsigned long v = regs[i].fn ();
          if (regs[i].s)
            v &= 0xffffffffUL;
          printf ("  %-13s ok    %#018lx\n", regs[i].name, v);
          fflush (stdout);
          _exit (0);
        }
      int st;
      waitpid (pid, &st, 0);
      if (WIFSIGNALED (st))
        printf ("  %-13s killed by signal %d\n", regs[i].name, WTERMSIG (st));
    }
}

/* Mappings.  */
static int
find_map (unsigned long addr, unsigned long *s, unsigned long *e, char *name)
{
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  int found = 0;
  while (f && fgets (line, sizeof line, f))
    {
      unsigned long a, b;
      char nm[256] = "";
      if (sscanf (line, "%lx-%lx %*s %*s %*s %*s %255[^\n]", &a, &b, nm) < 2)
        continue;
      if (addr >= a && addr < b)
        {
          *s = a, *e = b;
          strcpy (name, nm[0] ? nm : "(anonymous)");
          found = 1;
          break;
        }
    }
  if (f)
    fclose (f);
  return found;
}

static void
print_stack_maps (const char *who)
{
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    if (strstr (line, "stack"))
      printf ("    %s maps: %s", who, line);
  if (f)
    fclose (f);
}

static long
hw_size (int mode)
{
  unsigned long sz = 0;
  if (syscall (SYS_access_hw_stacks, (unsigned long) mode, NULL, NULL, 0UL,
               &sz) != 0)
    return -1;
  return (long) sz;
}

struct rec
{
  int depth;
  unsigned long s[17];          /* the snapshot */
  long ps_sys, pcs_sys;         /* access_hw_stacks right after it */
  unsigned long t[17];          /* a second snapshot after the call */
  void *local;                  /* a local of the snapshot's caller */
};

static void __attribute__ ((noinline))
take (struct rec *r, int depth)
{
  volatile char here[16];
  here[0] = 0;
  memset (r->s, 0, sizeof r->s);
  memset (r->t, 0, sizeof r->t);
  r->depth = depth;
  r->local = (void *) here;
  sr_snap (r->s);
  r->ps_sys = hw_size (E2K_GET_PROCEDURE_STACK_SIZE);
  r->pcs_sys = hw_size (E2K_GET_CHAIN_STACK_SIZE);
  sr_snap (r->t);
}

/* Decode one snapshot.  */
static void
print_rec (const struct rec *r, int verbose)
{
  const unsigned long *s = r->s;
  unsigned long pcshtp = s[0], pcsp_lo = s[1], pcsp_hi = s[2];
  unsigned long pshtp = s[3], psp_lo = s[4], psp_hi = s[5];
  unsigned long usd_lo = s[6], usd_hi = s[7], sbr = s[8], wd = s[9];
  unsigned long br = s[10], cr1_lo = s[11], cr1_hi = s[12], sp = s[13];
  long pcs_reg = sext (pcshtp, 11), ps_reg = sext (pshtp, 12);
  unsigned long pcs_ind = pcsp_hi & 0xffffffffUL, ps_ind = psp_hi & 0xffffffffUL;
  long pcs_top = (long) pcs_ind + pcs_reg, ps_top = (long) ps_ind + 2 * ps_reg;
  unsigned long usd_ptr = VA (usd_lo), usd_free = usd_hi >> 32;
  unsigned long ussz = BITS (cr1_hi, 36, 28) << 4;
  long pt_reg = sext (r->t[3], 12), pct_reg = sext (r->t[0], 11);

  printf ("  depth %4d: pshtp %5ld (x2 = %5ld B in registers), psp.ind %8lu,"
          " top %8ld, access_hw_stacks %8ld (diff %ld); after it pshtp %ld\n",
          r->depth, ps_reg, 2 * ps_reg, ps_ind, ps_top, r->ps_sys,
          r->ps_sys - ps_top, pt_reg);
  printf ("              pcshtp %4ld B, pcsp.ind %8lu, top %8ld,"
          " access_hw_stacks %8ld (diff %ld); after it pcshtp %ld\n",
          pcs_reg, pcs_ind, pcs_top, r->pcs_sys, r->pcs_sys - pcs_top,
          pct_reg);
  printf ("              usd.ptr %#lx, free %#lx, bottom %#lx, sbr %#lx,"
          " used %#lx; getsp 0 %s usd.ptr; local - ptr %ld;"
          " cr1.ussz*16 %s usd free\n",
          usd_ptr, usd_free, usd_ptr - usd_free, VA (sbr), VA (sbr) - usd_ptr,
          VA (sp) == usd_ptr ? "==" : "!=", (long) ((char *) r->local
                                                   - (char *) usd_ptr),
          ussz == usd_free ? "==" : "!=");
  if (!verbose)
    return;
  printf ("              raw: pshtp %#lx psp %#lx:%#lx pcshtp %#lx pcsp %#lx:%#lx"
          " usd %#lx:%#lx sbr %#lx\n",
          pshtp, psp_lo, psp_hi, pcshtp, pcsp_lo, pcsp_hi, usd_lo, usd_hi, sbr);
  printf ("              psp: base %#lx rw %lu (bits 59-60) top bits 61-63 %lu,"
          " ind %lu, size %lu\n",
          VA (psp_lo), BITS (psp_lo, 59, 2), BITS (psp_lo, 61, 3),
          ps_ind, psp_hi >> 32);
  printf ("              pcsp: base %#lx rw %lu, ind %lu, size %lu\n",
          VA (pcsp_lo), BITS (pcsp_lo, 59, 2), pcs_ind, pcsp_hi >> 32);
  printf ("              pshtp fields: ind %ld fxind %lu tind %lu fx %lu;"
          " bits 49-63 %#lx\n",
          ps_reg, BITS (pshtp, 16, 11), BITS (pshtp, 32, 11),
          BITS (pshtp, 48, 1), pshtp >> 49);
  printf ("              usd.lo: bits 48-57 %#lx p %lu rw %lu 61-63 %lu;"
          " usd.hi low 32 bits %#lx\n",
          BITS (usd_lo, 48, 10), BITS (usd_lo, 58, 1), BITS (usd_lo, 59, 2),
          BITS (usd_lo, 61, 3), usd_hi & 0xffffffffUL);
  printf ("              wd: base %lu size %lu psize %lu fx %lu dbl %lu"
          " (raw %#lx)\n",
          BITS (wd, 0, 11), BITS (wd, 16, 11), BITS (wd, 32, 11),
          BITS (wd, 48, 1), BITS (wd, 49, 1), wd);
  printf ("              br: rbs %lu rsz %lu rcur %lu psz %lu pcur %lu"
          " (raw %#lx)\n",
          BITS (br, 0, 6), BITS (br, 6, 6), BITS (br, 12, 6), BITS (br, 18, 5),
          BITS (br, 23, 5), br);
  printf ("              cr1.lo: wfx %lu wpsz %lu wbs %lu; cr1.hi: br %#lx"
          " wdbl %lu ussz %#lx\n",
          BITS (cr1_lo, 25, 1), BITS (cr1_lo, 26, 7), BITS (cr1_lo, 33, 7),
          BITS (cr1_hi, 0, 28), BITS (cr1_hi, 35, 1), ussz);
}

static void
print_maps_check (const struct rec *r, const char *who)
{
  const unsigned long *s = r->s;
  unsigned long a, b;
  char nm[256];
  unsigned long psb = VA (s[4]), pss = s[5] >> 32;
  unsigned long pcb = VA (s[1]), pcs = s[2] >> 32;
  unsigned long ptr = VA (s[6]), bottom = ptr - (s[7] >> 32), sbr = VA (s[8]);
  if (find_map (psb, &a, &b, nm))
    printf ("    %s: psp.base %#lx size %#lx; mapping %#lx-%#lx (%#lx) %s;"
            " base %s start, base+size %s end\n",
            who, psb, pss, a, b, b - a, nm, psb == a ? "==" : "!=",
            psb + pss == b ? "==" : "!=");
  if (find_map (pcb, &a, &b, nm))
    printf ("    %s: pcsp.base %#lx size %#lx; mapping %#lx-%#lx (%#lx) %s;"
            " base %s start, base+size %s end\n",
            who, pcb, pcs, a, b, b - a, nm, pcb == a ? "==" : "!=",
            pcb + pcs == b ? "==" : "!=");
  if (find_map (ptr, &a, &b, nm))
    printf ("    %s: usd bottom %#lx, sbr %#lx; mapping %#lx-%#lx %s;"
            " bottom %s start (%+ld), sbr %s end (%+ld)\n",
            who, bottom, sbr, a, b, nm, bottom == a ? "==" : "!=",
            (long) (bottom - a), sbr == b ? "==" : "!=", (long) (sbr - b));
}

static struct rec recs[32];
static int nrec;
static const int depths[] = { 0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 64, 128,
                              1000, -1 };

static int __attribute__ ((noinline))
down (int n, int depth)
{
  volatile char pad[16];
  pad[0] = (char) n;
  for (int k = 0; depths[k] >= 0; k++)
    if (depths[k] == depth)
      take (&recs[nrec++], depth);
  return n == depth ? 0 : down (n, depth + 1) + pad[0];
}

static void
depth_case (const char *who)
{
  nrec = 0;
  down (1000, 0);
  for (int i = 0; i < nrec; i++)
    print_rec (&recs[i], i == 0 || i == nrec - 1);
  print_maps_check (&recs[0], who);
  print_maps_check (&recs[nrec - 1], who);
  print_stack_maps (who);
}

/* 3.  */
static void __attribute__ ((noinline))
ctx_check (void)
{
  ucontext_t uc;
  struct rec r;
  memset (&uc, 0, sizeof uc);
  getcontext (&uc);
  take (&r, 0);
  const unsigned long *s = r.s;
  printf ("  getcontext: sbr %#lx (rrd %#lx), usd %#llx:%#llx (rrd %#lx:%#lx)\n",
          (unsigned long) uc.uc_mcontext.sbr, s[8], uc.uc_mcontext.usd_lo,
          uc.uc_mcontext.usd_hi, s[6], s[7]);
  printf ("  getcontext: psp %#llx:%#llx (rrd %#lx:%#lx, pshtp %ld)\n",
          uc.uc_mcontext.psp_lo, uc.uc_mcontext.psp_hi, s[4], s[5],
          sext (s[3], 12));
  printf ("  getcontext: pcsp %#llx:%#llx (rrd %#lx:%#lx, pcshtp %ld)\n",
          uc.uc_mcontext.pcsp_lo, uc.uc_mcontext.pcsp_hi, s[1], s[2],
          sext (s[0], 11));
}

/* 4.  The handler reads what the kernel put in the signal frame.  */
static volatile unsigned long h_sbr, h_usd_lo, h_usd_hi, h_rrd_sbr;

static void
sbr_handler (int sig, siginfo_t *si, void *ctx)
{
  (void) sig, (void) si;
  ucontext_t *uc = ctx;
  h_sbr = uc->uc_mcontext.sbr;
  h_usd_lo = uc->uc_mcontext.usd_lo;
  h_usd_hi = uc->uc_mcontext.usd_hi;
  h_rrd_sbr = VA (sr_sbr ());
}

static void
signal_check (const char *who)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = sbr_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGUSR1, &sa, NULL);
  raise (SIGUSR1);
  printf ("  %s: signal frame sbr %#lx, usd.ptr %#lx free %#lx;"
          " rrd %%sbr %#lx (in the handler), %#lx (here)\n",
          who, h_sbr, VA (h_usd_lo), h_usd_hi >> 32, h_rrd_sbr,
          VA (sr_sbr ()));
}

static void *
thread_fn (void *arg)
{
  (void) arg;
  pthread_attr_t a;
  void *addr;
  size_t size;
  unsigned long lo = VA (sr_usd_lo ()), fr = sr_usd_hi () >> 32;
  unsigned long sbr = VA (sr_sbr ());
  pthread_getattr_np (pthread_self (), &a);
  pthread_attr_getstack (&a, &addr, &size);
  printf ("  thread: pthread stack %p-%p (%#zx), pthread_self %#lx\n", addr,
          (void *) ((char *) addr + size), size,
          (unsigned long) pthread_self ());
  printf ("  thread entry: usd.ptr %#lx free %#lx bottom %#lx sbr %#lx"
          " (sbr - ptr %ld)\n",
          lo, fr, lo - fr, sbr, (long) (sbr - lo));
  signal_check ("thread");
  depth_case ("thread");
  return NULL;
}

/* 5.  */
static ucontext_t uc_main, uc_fib;
static char *fib_stack;
#define FIB_SIZE (256 << 10)

static void
fib_fn (void)
{
  printf ("  context: uc_stack %p-%p\n", (void *) fib_stack,
          (void *) (fib_stack + FIB_SIZE));
  depth_case ("context");
}

static void
fiber_case (void)
{
  fib_stack = aligned_alloc (4096, FIB_SIZE);
  getcontext (&uc_fib);
  uc_fib.uc_stack.ss_sp = fib_stack;
  uc_fib.uc_stack.ss_size = FIB_SIZE;
  uc_fib.uc_link = &uc_main;
  if (makecontext_e2k (&uc_fib, fib_fn, 0) != 0)
    {
      perror ("makecontext_e2k");
      return;
    }
  swapcontext (&uc_main, &uc_fib);
  printf ("  back in main; freecontext_e2k: %d\n", freecontext_e2k (&uc_fib));
}

/* 6.  */
static unsigned long g_last;
static struct
{
  int depth;
  unsigned long ptr, freesz;
} chg[8192];
static int nchg;

static int __attribute__ ((noinline))
grow_down (int n)
{
  volatile char pad[32];
  pad[0] = (char) n;
  unsigned long hi = sr_usd_hi (), fr = hi >> 32;
  if (fr > g_last && nchg < 8192)
    {
      chg[nchg].depth = n;
      chg[nchg].ptr = VA (sr_usd_lo ());
      chg[nchg].freesz = fr;
      nchg++;
    }
  g_last = fr;
  return n == 0 ? 0 : grow_down (n - 1) + pad[0];
}

static void __attribute__ ((noinline))
alloca_step (long n)
{
  unsigned long before = sr_usd_hi () >> 32, p0 = VA (sr_usd_lo ());
  volatile char *p = __builtin_alloca (n);
  p[0] = 0;
  unsigned long after = sr_usd_hi () >> 32, p1 = VA (sr_usd_lo ());
  printf ("  alloca %ld: free %#lx -> %#lx, ptr moved %ld, free grew by %ld"
          " (+ the allocation: %ld)\n",
          n, before, after, (long) (p0 - p1), (long) (after - before),
          (long) (after - before + (p0 - p1)));
}

static void
grow_case (void)
{
  print_stack_maps ("start");
  unsigned long ptr0 = VA (sr_usd_lo ()), fr0 = sr_usd_hi () >> 32;
  printf ("  start: usd.ptr %#lx free %#lx bottom %#lx sbr %#lx\n", ptr0, fr0,
          ptr0 - fr0, VA (sr_sbr ()));
  fflush (stdout);
  if (fork () == 0)
    {
      alloca_step (200000);     /* more than is free: one growth */
      fflush (stdout);
      _exit (0);
    }
  wait (NULL);
  g_last = fr0;
  grow_down (20000);
  printf ("  recursion of 20000 (32-byte locals): %d growths\n", nchg);
  for (int i = 0; i < nchg; i++)
    {
      long step = i + 1 < nchg ? (long) (chg[i].ptr - chg[i + 1].ptr) : 0;
      if (i < 6 || i == nchg - 1)
        printf ("    at depth %5d: ptr %#lx, free after growth %#lx;"
                " next growth %ld bytes further down\n",
                chg[i].depth, chg[i].ptr, chg[i].freesz, step);
    }
  print_stack_maps ("after");
  alloca_step (70000);
  alloca_step (1 << 20);
  alloca_step (3 << 20);
  print_stack_maps ("end");
}

/* 7.  A context whose data stack sits inside a larger mapping: what a
   getsp below the bottom of uc_stack does, with the memory below it
   (0) mapped, (1) a PROT_NONE page, (2) an unmapped page, (3) unmapped
   all the way down (512 KiB, more than the overflow).  Writes only
   into the probe's own mapping.  A SIGSEGV handler on an alternate stack
   prints the signal's code and address and exits the child.  */
#define OV_AREA (1 << 20)
#define OV_STACK (64 << 10)
static unsigned char *ov_area;

static void __attribute__ ((noinline))
ov_down (int n)
{
  volatile unsigned char *p = __builtin_alloca (32 << 10);
  memset ((void *) p, 0xa5, 32 << 10);
  unsigned long ptr = VA (sr_usd_lo ()), fr = sr_usd_hi () >> 32;
  printf ("    level %d: usd.ptr %#lx, bottom %#lx (%+ld from uc_stack's"
          " base)\n", n, ptr, ptr - fr,
          (long) (ptr - fr - (unsigned long) (ov_area + OV_AREA / 2)));
  if (n > 1)
    ov_down (n - 1);
}

static void
ov_fn (void)
{
  ov_down (4);
}

static char ov_altstack[64 << 10];

static void
ov_segv (int sig, siginfo_t *si, void *ctx)
{
  (void) ctx;
  char buf[160];
  long off = (long) ((unsigned long) si->si_addr
                     - (unsigned long) (ov_area + OV_AREA / 2));
  int n = snprintf (buf, sizeof buf,
                    "  [signal %d, si_code %d, si_addr %p (%+ld from"
                    " uc_stack's base)]\n", sig, si->si_code, si->si_addr,
                    off);
  write (1, buf, n);
  _exit (0);
}

static const char *const ov_names[] = { "mapped below", "guard page below",
                                        "unmapped page below",
                                        "512 KiB unmapped below" };

static void
overflow_case (int kind)
{
  stack_t ss = { .ss_sp = ov_altstack, .ss_size = sizeof ov_altstack };
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = ov_segv;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigaltstack (&ss, NULL);
  sigaction (SIGSEGV, &sa, NULL);
  ov_area = mmap (NULL, OV_AREA, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memset (ov_area, 0x5a, OV_AREA);
  if (kind == 1)
    mprotect (ov_area + OV_AREA / 2 - 4096, 4096, PROT_NONE);
  else if (kind == 2)
    munmap (ov_area + OV_AREA / 2 - 4096, 4096);
  else if (kind == 3)
    munmap (ov_area, OV_AREA / 2);
  getcontext (&uc_fib);
  uc_fib.uc_stack.ss_sp = ov_area + OV_AREA / 2;
  uc_fib.uc_stack.ss_size = OV_STACK;
  uc_fib.uc_link = &uc_main;
  if (makecontext_e2k (&uc_fib, ov_fn, 0) != 0)
    {
      perror ("makecontext_e2k");
      return;
    }
  printf ("  %s: uc_stack %p-%p in a mapping %p-%p; 4 levels of 32 KiB\n",
          ov_names[kind], (void *) (ov_area + OV_AREA / 2),
          (void *) (ov_area + OV_AREA / 2 + OV_STACK), (void *) ov_area,
          (void *) (ov_area + OV_AREA));
  swapcontext (&uc_main, &uc_fib);
  long changed = 0;
  for (long i = 0; kind != 3 && i < OV_AREA / 2 - (kind ? 4096 : 0); i++)
    changed += ov_area[i] != 0x5a;
  printf ("  back: %ld bytes below uc_stack overwritten\n", changed);
}

static void
overflow_child (int guard)
{
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      overflow_case (guard);
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("  [killed by signal %d]\n", WTERMSIG (st));
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  if (argc > 1 && !strcmp (argv[1], "overflow"))
    {
      printf ("7. overflow of a context's data stack\n");
      overflow_child (0);
      overflow_child (1);
      overflow_child (2);
      overflow_child (3);
      return 0;
    }
  if (argc > 1 && !strcmp (argv[1], "grow"))
    {
      printf ("6. growth of the main thread's data stack\n");
      grow_case ();
      return 0;
    }
  printf ("1. user-mode reads (each in a child)\n");
  access_case ();
  printf ("2. main thread\n");
  depth_case ("main");
  printf ("3. getcontext\n");
  ctx_check ();
  signal_check ("main");
  printf ("4. thread\n");
  pthread_t t;
  pthread_create (&t, NULL, thread_fn, NULL);
  pthread_join (t, NULL);
  printf ("5. makecontext_e2k context\n");
  fiber_case ();
  return 0;
}
