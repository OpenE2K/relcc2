/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Two window questions from the proof-of-concept back end
   (spec/abi/kernel-interface.md, "System calls"; registers.md, window
   limits and nfx):

     wbs  a `call` with a small wbs.  A frame that inherited Q pairs sets
          wsz = W + P and calls with wbs = W, handing over P pairs, with
          Q, P = 1..4 and W = 0, 1, 2, 3, 4, 5, 8: system calls through
          entry 3 (getpid, close (-1)) and entry 6 (fast call 6, which is
          -ENOSYS, and fast getcpu (0, 0)), an ordinary call, and user
          functions that start with the kernel entries' first
          instruction (vfrpsz rpsz = 4 ...).  Also a call handing over
          no pairs and one with wbs beyond the window.  Each case runs in
          a child; a signal's si_code, si_addr, interrupted IP and first
          TIRs are printed.
     ext  the 16-bit part of an extended register (built with movif,
          read back with movfi) in a window register across a page
          fault, a SIGSEGV whose handler fixes the page (also one whose
          handler first recurses 1,000 calls deep, and one taken in a
          96-pair window), system calls
          (getpid, sched_yield, nanosleep), a shallow and a deep call
          (two window shapes), timer interrupts, and SIGALRM every
          100 us; setwd nfx = 1 against nfx = 0; the SIGSEGV cases also
          with the value in %g16 and %g20.  Then whether ordinary
          writes of the register keep the 16-bit part.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc   # v5.8c2, v6.2c3
     $L -O1 tools/abi/probes/pocabi2.c tools/abi/probes/pocabi2.s -o pocabi2
     ./pocabi2 [wbs|cross|ext [N...]]    # on hardware; N...: ext cases to run,
                                         # in this order, in one process  */

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

typedef long (*bfn) (long, long, long);
typedef long (*tfn) (bfn, long, long, long);

long pa_t1 (bfn, long, long, long), pa_t2 (bfn, long, long, long);
long pa_t3 (bfn, long, long, long), pa_t4 (bfn, long, long, long);
long pa_leaf (long, long, long), pa_vf4 (long, long, long);
long pa_vf6 (long, long, long);
long pa_u_p0 (long, long, long), pa_u_over (long, long, long);

#define DECL(k, w, p) long pa_##k##_w##w##_p##p (long, long, long);
#define DECLKP(w, p) DECL (s3, w, p) DECL (s6, w, p) DECL (u, w, p)
#define DECLW(w) DECLKP (w, 1) DECLKP (w, 2) DECLKP (w, 3) DECLKP (w, 4)
DECLW (0) DECLW (1) DECLW (2) DECLW (3) DECLW (4) DECLW (5) DECLW (8)

static const struct frame
{
  int w, p;
  bfn s3, s6, u;
} frames[] = {
#define ENT(w, p) { w, p, pa_s3_w##w##_p##p, pa_s6_w##w##_p##p, pa_u_w##w##_p##p },
#define ENTW(w) ENT (w, 1) ENT (w, 2) ENT (w, 3) ENT (w, 4)
  ENTW (0) ENTW (1) ENTW (2) ENTW (3) ENTW (4) ENTW (5) ENTW (8)
};

static const tfn tramps[5] = { 0, pa_t1, pa_t2, pa_t3, pa_t4 };

#define M48 0xffffffffffffUL

static void
put (const char *s)
{
  ssize_t r = write (1, s, strlen (s));
  (void) r;
}

/* --- part 1: wbs ------------------------------------------------------- */

static char case_name[64];
static struct sym { const char *name; unsigned long a; } syms[3];

static const char *
symname (unsigned long ip, char *buf, size_t n)
{
  const struct sym *best = NULL;
  for (unsigned i = 0; i < sizeof syms / sizeof *syms; i++)
    if (syms[i].name && syms[i].a <= ip && ip - syms[i].a < 0x400
        && (!best || syms[i].a > best->a))
      best = &syms[i];
  if (best)
    snprintf (buf, n, "%s+0x%lx", best->name, ip - best->a);
  else
    snprintf (buf, n, "0x%lx", ip);
  return buf;
}

static void
crash (int sig, siginfo_t *si, void *ucv)
{
  ucontext_t *uc = ucv;
  char line[512], ipb[96];
  unsigned long ip = uc->uc_mcontext.cr0_hi & M48 & ~7UL;
  int n = snprintf (line, sizeof line,
                    "%s: signal %d (%s) code %d addr %p ip %s cr1_lo %#llx "
                    "tirs %llu",
                    case_name, sig, strsignal (sig), si->si_code, si->si_addr,
                    symname (ip, ipb, sizeof ipb), uc->uc_mcontext.cr1_lo,
                    uc->uc_mcontext.nr_TIRs);
  for (unsigned i = 0; i < 2 && i <= uc->uc_mcontext.nr_TIRs; i++)
    n += snprintf (line + n, sizeof line - n, " [hi %#llx lo %#llx]",
                   uc->uc_mcontext.tir_hi[i], uc->uc_mcontext.tir_lo[i]);
  snprintf (line + n, sizeof line - n, "\n");
  put (line);
  _exit (100 + sig);
}

/* Call frame B through the trampoline that hands it Q pairs, in a child.
   B's name is NAME; CALLEE, if any, the user function it calls.  */
static void
run_case (const char *name, int q, const char *kind, bfn b, bfn callee,
          long x, long y, long z, long want)
{
  snprintf (case_name, sizeof case_name, "%s q%d %-8s", name, q, kind);
  syms[0] = (struct sym) { "frame", (unsigned long) b };
  syms[1] = (struct sym) { "tramp", (unsigned long) tramps[q] };
  syms[2] = (struct sym) { callee ? "callee" : NULL, (unsigned long) callee };
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      struct sigaction sa;
      memset (&sa, 0, sizeof sa);
      sa.sa_sigaction = crash;
      sa.sa_flags = SA_SIGINFO;
      sigaction (SIGSEGV, &sa, NULL);
      sigaction (SIGILL, &sa, NULL);
      sigaction (SIGBUS, &sa, NULL);
      if (want == -1000)        /* getpid */
        want = getpid ();
      long r = tramps[q] (b, x, y, z);
      char line[256];
      if (want == -2000)
        snprintf (line, sizeof line, "%s: returned\n", case_name);
      else
        snprintf (line, sizeof line, "%s: returned %ld%s\n", case_name, r,
                  r == want ? "" : "  (unexpected)");
      put (line);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("%s: killed by signal %d (%s)\n", case_name, WTERMSIG (st),
            strsignal (WTERMSIG (st)));
  else if (WEXITSTATUS (st) != 0 && WEXITSTATUS (st) < 100)
    printf ("%s: exit %d\n", case_name, WEXITSTATUS (st));
  fflush (stdout);
}

static void
frame_cases (const struct frame *f, int q, int all)
{
  char name[32];
  snprintf (name, sizeof name, "w%d p%d", f->w, f->p);
  run_case (name, q, "getpid", f->s3, NULL, 20, 0, 0, -1000);
  if (all)
    {
      run_case (name, q, "close-1", f->s3, NULL, 6, -1, 0, -9);
      run_case (name, q, "fast6", f->s6, NULL, 6, 0, 0, -38);
      if (f->p >= 2 && q >= 2)
        run_case (name, q, "getcpu", f->s6, NULL, 2, 0, 0, 0);
    }
  run_case (name, q, "leaf", f->u, pa_leaf, (long) pa_leaf, 41, 0, 42);
  if (all)
    {
      run_case (name, q, "vf4", f->u, pa_vf4, (long) pa_vf4, 41, 0, 42);
      run_case (name, q, "vf6", f->u, pa_vf6, (long) pa_vf6, 41, 0, 42);
    }
}

/* Every frame called with Q = P pairs.  */
static void
wbs (void)
{
  printf ("== wbs: frame wsz = W + P inherits Q = P pairs, calls with "
          "wbs = W\n");
  for (unsigned i = 0; i < sizeof frames / sizeof *frames; i++)
    frame_cases (&frames[i], frames[i].p, 1);
  printf ("== a call handing over no pairs, and one beyond the window "
          "(wsz 4)\n");
  for (int q = 1; q <= 4; q += 3)
    {
      run_case ("p0 wbs=4", q, "leaf", pa_u_p0, pa_leaf, (long) pa_leaf, 0,
                0, -2000);
      run_case ("over wbs=5", q, "leaf", pa_u_over, pa_leaf, (long) pa_leaf,
                0, 0, -2000);
    }
}

/* Frames called with Q != P pairs (Q <= W + P, else setwd is SIGILL).  */
static void
cross (void)
{
  printf ("== cross: frame wsz = W + P inherits Q pairs, calls with "
          "wbs = W\n");
  for (unsigned i = 0; i < sizeof frames / sizeof *frames; i++)
    for (int q = 1; q <= 4; q++)
      if (q != frames[i].p && q <= frames[i].w + frames[i].p)
        frame_cases (&frames[i], q, 0);
}

/* --- part 2: ext ------------------------------------------------------- */

unsigned long pe_fault_nfx1 (unsigned long, unsigned long, long *,
                             unsigned long *);
unsigned long pe_fault_nfx0 (unsigned long, unsigned long, long *,
                             unsigned long *);
unsigned long pe_sys_nfx1 (unsigned long, unsigned long, long,
                           unsigned long *, long, long);
unsigned long pe_sys_nfx0 (unsigned long, unsigned long, long,
                           unsigned long *, long, long);
unsigned long pe_call_nfx1 (unsigned long, unsigned long, long (*) (long),
                            unsigned long *, long);
unsigned long pe_call_nfx0 (unsigned long, unsigned long, long (*) (long),
                            unsigned long *, long);
unsigned long pe_call8_nfx1 (unsigned long, unsigned long, long (*) (long),
                             unsigned long *, long);
unsigned long pe_call8_nfx0 (unsigned long, unsigned long, long (*) (long),
                             unsigned long *, long);
unsigned long pe_spin_nfx1 (unsigned long, unsigned long, long,
                            unsigned long *);
unsigned long pe_spin_nfx0 (unsigned long, unsigned long, long,
                            unsigned long *);
unsigned long pe_faultbig_nfx1 (unsigned long, unsigned long, long *,
                                unsigned long *);
unsigned long pe_faultbig_nfx0 (unsigned long, unsigned long, long *,
                                unsigned long *);
unsigned long pe_gfault (unsigned long, unsigned long, long *,
                         unsigned long *);
unsigned long pe_wr_addd (unsigned long, unsigned long, long *,
                          unsigned long *);
unsigned long pe_wr_adds (unsigned long, unsigned long, long *,
                          unsigned long *);
unsigned long pe_wr_ldd (unsigned long, unsigned long, long *,
                         unsigned long *);
unsigned long pe_wr_faddd (unsigned long, unsigned long, long *,
                           unsigned long *);

static const unsigned long MANT = 0xc90fdaa22168c235UL;
static long pg;
static char *fix_page;
static int segv_deep;
static volatile int nsegv, nalrm;

__attribute__ ((noinline)) static long
deep (long n)
{
  volatile char pad[16];
  pad[0] = (char) n;
  if (n > 0)
    deep (n - 1);
  return pad[0];
}

static void
segv_fix (int sig, siginfo_t *si, void *uc)
{
  (void) sig;
  (void) uc;
  if (fix_page && (char *) si->si_addr >= fix_page
      && (char *) si->si_addr < fix_page + pg)
    {
      if (segv_deep)
        deep (1000);
      mprotect (fix_page, pg, PROT_READ | PROT_WRITE);
      nsegv++;
      return;
    }
  put ("unexpected SIGSEGV\n");
  _exit (3);
}

static void
alrm (int sig)
{
  (void) sig;
  nalrm++;
}

enum { E_MINOR, E_SEGV, E_SEGVDEEP, E_GETPID, E_YIELD, E_SLEEP, E_CALL0,
       E_CALL1000, E_CALL8_0, E_CALL8_1000, E_SPIN, E_SPINALRM, E_GSEGV,
       E_GSEGVDEEP, E_SEGVBIG, E_N };
static const char *const ename[E_N] = {
  "page fault (no signal)", "SIGSEGV, handler fixes the page",
  "SIGSEGV, handler recurses 1000", "system call getpid",
  "system call sched_yield", "system call nanosleep 1 ms",
  "call wsz 12 wbs 8, depth 0", "call wsz 12 wbs 8, depth 1000",
  "call wsz 8 wbs 4, depth 0", "call wsz 8 wbs 4, depth 1000",
  "500 x 2 ms spin, interrupts only", "2000 x 0.1 ms spin, SIGALRM 100 us",
  "%g16, %g20: SIGSEGV, shallow", "%g16, %g20: SIGSEGV, handler deep",
  "SIGSEGV, shallow, window 96 pairs"
};

/* One run of case E with nfx NFX; returns the 16-bit part read back,
   sets *mant_ok and *extra (signals seen, system call result).  */
static unsigned
one (int e, int nfx, unsigned exp, int *mant_ok, long *extra, long spin)
{
  unsigned long out[2] = { 0, 0 }, r = 0;
  char *page = NULL;
  *extra = 0;
  switch (e)
    {
    case E_MINOR:
    case E_SEGV:
    case E_SEGVDEEP:
    case E_GSEGV:
    case E_GSEGVDEEP:
    case E_SEGVBIG:
      page = mmap (NULL, pg, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (e != E_MINOR)
        {
          *(long *) page = 0x1234;
          mprotect (page, pg, PROT_NONE);
          fix_page = page;
          segv_deep = e == E_SEGVDEEP || e == E_GSEGVDEEP;
          nsegv = 0;
        }
      if (e == E_GSEGV || e == E_GSEGVDEEP)
        {
          r = pe_gfault (MANT, exp, (long *) page, out);
          *extra = (long) (out[1] & 0xffff);    /* %g20's 16-bit part */
        }
      else if (e == E_SEGVBIG)
        {
          r = (nfx ? pe_faultbig_nfx1 : pe_faultbig_nfx0) (MANT, exp,
                                                           (long *) page,
                                                           out);
          *extra = nsegv;
        }
      else
        {
          r = (nfx ? pe_fault_nfx1 : pe_fault_nfx0) (MANT, exp,
                                                     (long *) page, out);
          *extra = e != E_MINOR ? nsegv : (long) out[1];
        }
      fix_page = NULL;
      munmap (page, pg);
      break;
    case E_GETPID:
    case E_YIELD:
      r = (nfx ? pe_sys_nfx1 : pe_sys_nfx0) (MANT, exp,
                                             e == E_GETPID ? 20 : 158, out,
                                             0, 0);
      *extra = (long) out[1];
      break;
    case E_SLEEP:
      {
        struct timespec ts = { 0, 1000000 };
        r = (nfx ? pe_sys_nfx1 : pe_sys_nfx0) (MANT, exp, 162, out,
                                               (long) &ts, 0);
        *extra = (long) out[1];
      }
      break;
    case E_CALL0:
    case E_CALL1000:
      r = (nfx ? pe_call_nfx1 : pe_call_nfx0) (MANT, exp, deep, out,
                                               e == E_CALL0 ? 0 : 1000);
      break;
    case E_CALL8_0:
    case E_CALL8_1000:
      r = (nfx ? pe_call8_nfx1 : pe_call8_nfx0) (MANT, exp, deep, out,
                                                 e == E_CALL8_0 ? 0 : 1000);
      break;
    case E_SPIN:
    case E_SPINALRM:
      r = (nfx ? pe_spin_nfx1 : pe_spin_nfx0) (MANT, exp, spin, out);
      *extra = (long) out[1];
      break;
    }
  *mant_ok = out[0] == MANT;
  return r & 0xffff;
}

static void
ext_case (int e)
{
  for (int nfx = 1; nfx >= 0; nfx--)
    {
      if (e == E_SPIN || e == E_SPINALRM)
        {
          /* Many holds; a value is lost if it differs at the end of one.  */
          int k = e == E_SPIN ? 500 : 2000, lost = 0, bad_mant = 0;
          long spin = e == E_SPIN ? 400000 : 20000, cyc = 0, extra;
          char vals[64] = "";
          int nv = 0;
          struct itimerval it = { { 0, 100 }, { 0, 100 } }, off = { 0 };
          nalrm = 0;
          if (e == E_SPINALRM)
            setitimer (ITIMER_REAL, &it, NULL);
          for (int i = 0; i < k; i++)
            {
              int mok;
              unsigned want = 0x4a00 + (i & 0xff);
              unsigned got = one (e, nfx, want, &mok, &extra, spin);
              cyc += extra;
              if (got != want)
                {
                  lost++;
                  if (nv < 6)
                    nv += snprintf (vals + strlen (vals),
                                    sizeof vals - strlen (vals), " %04x",
                                    got);
                }
              bad_mant += !mok;
            }
          if (e == E_SPINALRM)
            setitimer (ITIMER_REAL, &off, NULL);
          printf ("%-34s nfx=%d: lost %d/%d (first:%s)%s  signals %d, "
                  "%ld cycles a hold\n", ename[e], nfx, lost, k,
                  nv ? vals : " -", bad_mant ? "  MANTISSA CHANGED" : "",
                  nalrm, cyc / k);
          continue;
        }
      int reps = 5, lost = 0, bad_mant = 0;
      char vals[128];
      int n = 0;
      long extra[5];
      for (int rep = 0; rep < reps; rep++)
        {
          int mok;
          unsigned want = 0x4a00 + rep;
          unsigned got = one (e, nfx, want, &mok, &extra[rep], 0);
          lost += got != want;
          bad_mant += !mok;
          n += snprintf (vals + n, sizeof vals - n, " %04x", got);
        }
      printf ("%-34s nfx=%d:%s  lost %d/%d%s", ename[e], nfx, vals, lost,
              reps, bad_mant ? "  MANTISSA CHANGED" : "");
      if (e == E_SEGV || e == E_SEGVDEEP || e == E_SEGVBIG)
        printf ("  signals %ld", extra[0]);
      if (e == E_GSEGV || e == E_GSEGVDEEP)
        {
          printf ("  %%g20:");
          for (int rep = 0; rep < reps; rep++)
            printf (" %04lx", extra[rep]);
        }
      if (e >= E_GETPID && e <= E_SLEEP)
        printf ("  result %ld", extra[reps - 1]);
      printf ("\n");
    }
}

static void
ext (int n, char **only)
{
  printf ("== ext: 16-bit part of an extended window register "
          "(want 4a00 + rep)\n");
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = segv_fix;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGSEGV, &sa, NULL);
  memset (&sa, 0, sizeof sa);
  sa.sa_handler = alrm;
  sa.sa_flags = SA_RESTART;
  sigaction (SIGALRM, &sa, NULL);
  if (n > 0)
    {
      for (int i = 0; i < n; i++)
        ext_case (atoi (only[i]));
      return;
    }
  for (int e = 0; e < E_N; e++)
    ext_case (e);

  printf ("== an ordinary write of the register after movif (nfx = 0)\n");
  static const struct { const char *name; unsigned long (*f) (); } wr[] = {
    { "addd 0, 5", (unsigned long (*) ()) pe_wr_addd },
    { "adds 0, 5 (dbl = 0)", (unsigned long (*) ()) pe_wr_adds },
    { "ldd", (unsigned long (*) ()) pe_wr_ldd },
    { "faddd", (unsigned long (*) ()) pe_wr_faddd },
  };
  for (unsigned i = 0; i < sizeof wr / sizeof *wr; i++)
    {
      long mem = 0x3ff0000000000000L;   /* 1.0 */
      unsigned long out[2] = { 0, 0 };
      unsigned long r = wr[i].f (MANT, 0x4abcUL, &mem, out);
      printf ("%-20s: 16-bit part %04lx (movif wrote 4abc), ordinary part "
              "%016lx\n", wr[i].name, r & 0xffff, out[0]);
    }
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  pg = sysconf (_SC_PAGESIZE);
  if (argc < 2 || strcmp (argv[1], "wbs") == 0)
    wbs ();
  if (argc < 2 || strcmp (argv[1], "cross") == 0)
    cross ();
  if (argc < 2 || strcmp (argv[1], "ext") == 0)
    ext (argc > 2 ? argc - 2 : 0, argv + 2);
  return 0;
}
