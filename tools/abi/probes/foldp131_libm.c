/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The libm calls gfortran's IEEE tests make, with the sticky FP flags
   cleared as gfortran's _gfortran_ieee_procedure_entry clears them on
   entry to every procedure that uses an IEEE module, and as IEEE_LOGB,
   IEEE_RINT, IEEE_REM and IEEE_NEXT_AFTER clear them around their libm
   call (gcc/fortran/trans-intrinsic.cc, conv_intrinsic_ieee_logb_rint;
   libgfortran/ieee/ieee_helper.c, ieee_procedure_entry):
   spec/abi/registers.md, "MCST glibc's FP bugs";
   docs/spec-poc-findings.md P131.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   # or v5.8c2
     $LCC -O3 -fno-spec-fp tools/abi/probes/foldp131_libm.c -lm -ldl \
         -o foldp131_libm
     $LCC -O0 tools/abi/probes/foldp131_libm.c -lm -ldl -o foldp131_libm-O0
     ./foldp131_libm                     # on hardware, the machine's libm

   Built with -fno-spec-fp or at -O0 so that the probe itself runs no
   speculative FP, and the arguments are converted before the flags are
   cleared; the functions are called through volatile pointers, so lcc
   neither inlines nor folds them.  Each case runs in a child with alarm (1).
   Each line is FUNCTION CLEARED (ARGS) -> OUTCOME, CLEARED "-" for no
   flags cleared (processes start with all six set) and "A" for
   FE_ALL_EXCEPT.  OUTCOME is the result (%a or %La and its bits) and
   the flags after the call, "DIAG" for a result with a nonzero tag
   (gettagd/gettags don't trap), or "SIGILL si_code N at OBJECT+OFFSET",
   OBJECT the shared object (libm.so.6) or the probe.  At -O3 lcc copies
   a call's result out of the return registers with a plain addd (movif
   for long double), so a diagnostic result is a SIGILL in the probe; at
   -O0 it spills the result with std,sm and reloads it with a plain ldd,
   so a diagnostic result comes back as a number with bit 30 of each
   word set (registers.md, "MCST glibc's FP bugs").

   Elbrus 4C (ALT's glibc-core 2.35.0.234-alt2.29.023.2, lcc 1.29.16
   v3) and 8C2 (OS Elbrus 9.5.5's libm, v5.8c2), 2026-11-03, alike
   (only the libm offsets differ): with no flag cleared every case
   works.  With FE_ALL_EXCEPT cleared:
   - logbf (0) and logb (0) return a diagnostic value (SIGILL at the
     probe's copy at -O3; at -O0 logbf's comes back as -inf and logb's
     traps in libc's printf), logbl (0) dies inside libm; logb* of
     1.17, FLT_MAX, FLT_MIN, DBL_MIN, inf and NaN work.
   - sqrtl (-1) and sqrtl (2) die inside libm (as libmchk.c showed),
     sqrtl (4) works.
   - tanl (0.5236) and tanl (pi/4), sinl, cosl and sincosl (0.5236),
     tan (pi/4) and tanf (pi/4) die inside libm; tanl (0) works, and
     tanl (1) works and raises inexact.
   - fmaxf (NaN, 0) dies inside libm; fmaxf (0, NaN), the other fmaxf
     cases, fminf, fmax and fmaxl work.
   Not run on the 2C3 (offline).  */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char __executable_start[], etext[];

typedef float (*ff1) (float);
typedef float (*ff2) (float, float);
typedef double (*fd1) (double);
typedef double (*fd2) (double, double);
typedef long double (*fl1) (long double);
typedef long double (*fl2) (long double, long double);
typedef void (*flsc) (long double, long double *, long double *);

static ff1 volatile p_logbf = logbf, p_tanf = tanf, p_sqrtf = sqrtf;
static ff2 volatile p_fmaxf = fmaxf, p_fminf = fminf;
static fd1 volatile p_logb = logb, p_tan = tan;
static fd2 volatile p_fmax = fmax;
static fl1 volatile p_logbl = logbl, p_tanl = tanl, p_sinl = sinl,
  p_cosl = cosl, p_sqrtl = sqrtl;
static fl2 volatile p_fmaxl = fmaxl;
static flsc volatile p_sincosl = sincosl;

enum ty { F1, F2, D1, D2, L1, L2, LSC };
struct op
{
  const char *name;
  enum ty ty;
  void *volatile *fn;
};

static const struct op ops[] = {
  { "logbf", F1, (void *volatile *) &p_logbf },
  { "tanf", F1, (void *volatile *) &p_tanf },
  { "sqrtf", F1, (void *volatile *) &p_sqrtf },
  { "fmaxf", F2, (void *volatile *) &p_fmaxf },
  { "fminf", F2, (void *volatile *) &p_fminf },
  { "logb", D1, (void *volatile *) &p_logb },
  { "tan", D1, (void *volatile *) &p_tan },
  { "fmax", D2, (void *volatile *) &p_fmax },
  { "logbl", L1, (void *volatile *) &p_logbl },
  { "tanl", L1, (void *volatile *) &p_tanl },
  { "sinl", L1, (void *volatile *) &p_sinl },
  { "cosl", L1, (void *volatile *) &p_cosl },
  { "sqrtl", L1, (void *volatile *) &p_sqrtl },
  { "fmaxl", L2, (void *volatile *) &p_fmaxl },
  { "sincosl", LSC, (void *volatile *) &p_sincosl },
};

static char line[200];

static void
on_ill (int s, siginfo_t *si, void *u)
{
  (void) u;
  char *a = si->si_addr;
  char buf[300];
  Dl_info di;
  const char *where = "?";
  unsigned long off = (unsigned long) a;
  if (a >= __executable_start && a < etext)
    where = "probe", off = a - __executable_start;
  else if (dladdr (a, &di) && di.dli_fname)
    where = strrchr (di.dli_fname, '/') ? strrchr (di.dli_fname, '/') + 1
                                        : di.dli_fname,
    off = a - (char *) di.dli_fbase;
  int n = snprintf (buf, sizeof buf, "%s -> SIGILL si_code %d at %s+%#lx\n",
                    line, si->si_code, where, off);
  write (1, buf, n);
  _exit (s);
}

static void
flags (char *b)
{
  int f = fetestexcept (FE_ALL_EXCEPT);
  sprintf (b, "flags %s%s%s%s%s%s", f & FE_INVALID ? "V" : "",
           f & FE_DIVBYZERO ? "Z" : "", f & FE_OVERFLOW ? "O" : "",
           f & FE_UNDERFLOW ? "U" : "", f & FE_INEXACT ? "X" : "",
           f ? "" : "-");
}

static unsigned
tagd (double v)
{
  unsigned long t;
  __asm__ volatile ("gettagd %1, %0" : "=r" (t) : "r" (v));
  return t;
}

static unsigned
tags (float v)
{
  unsigned t;
  __asm__ volatile ("gettags %1, %0" : "=r" (t) : "r" (v));
  return t;
}

static void
child (const struct op *o, long double x, long double y, int clr)
{
  struct sigaction sa = { 0 };
  sa.sa_sigaction = on_ill;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  alarm (1);
  char fb[64];
  void *fn = *o->fn;
  /* Convert the arguments before the flags are cleared.  */
  volatile float fx = (float) x, fy = (float) y;
  volatile double dx = (double) x, dy = (double) y;
  feclearexcept (clr);
  switch (o->ty)
    {
    case F1:
    case F2:
      {
        float r = o->ty == F1 ? ((ff1) fn) (fx) : ((ff2) fn) (fx, fy);
        unsigned t = tags (r);
        flags (fb);
        if (t)
          printf ("%s -> DIAG tags %#x  %s\n", line, t, fb);
        else
          {
            unsigned b;
            memcpy (&b, &r, 4);
            printf ("%s -> %-24a %08x  %s\n", line, (double) r, b, fb);
          }
        break;
      }
    case D1:
    case D2:
      {
        double r = o->ty == D1 ? ((fd1) fn) (dx) : ((fd2) fn) (dx, dy);
        unsigned t = tagd (r);
        flags (fb);
        if (t)
          printf ("%s -> DIAG tags %#x  %s\n", line, t, fb);
        else
          {
            unsigned long b;
            memcpy (&b, &r, 8);
            printf ("%s -> %-24a %016lx  %s\n", line, r, b, fb);
          }
        break;
      }
    default:
      {
        long double r, c;
        if (o->ty == LSC)
          ((flsc) fn) (x, &r, &c);
        else
          r = o->ty == L1 ? ((fl1) fn) (x) : ((fl2) fn) (x, y);
        flags (fb);
        unsigned char b[16];
        memcpy (b, &r, 16);
        printf ("%s -> %-24La %04x%016lx  %s\n", line, r,
                *(unsigned short *) (b + 8), *(unsigned long *) b, fb);
        break;
      }
    }
  fflush (stdout);
  _exit (0);
}

static void
run (const char *n, long double x, long double y, int clr)
{
  const struct op *o = NULL;
  for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++)
    if (!strcmp (ops[i].name, n))
      o = &ops[i];
  if (!o)
    abort ();
  char args[96];
  if (o->ty == F2 || o->ty == D2 || o->ty == L2)
    snprintf (args, sizeof args, "%La, %La", x, y);
  else
    snprintf (args, sizeof args, "%La", x);
  snprintf (line, sizeof line, "%-6s %s (%s)", n, clr ? "A" : "-", args);
  fflush (stdout);
  pid_t p = fork ();
  if (p == 0)
    child (o, x, y, clr);
  int st;
  waitpid (p, &st, 0);
  if (WIFSIGNALED (st) && WTERMSIG (st) == SIGALRM)
    printf ("%s -> HANG\n", line);
  else if (WIFSIGNALED (st))
    printf ("%s -> killed by signal %d\n", line, WTERMSIG (st));
}

int
main (void)
{
  static const struct
  {
    const char *n;
    long double x, y;
  } c[] = {
    /* ieee_2: IEEE_LOGB of real(4) and real(8).  */
    { "logbf", 1.17L }, { "logbf", -1.17L }, { "logbf", FLT_MAX },
    { "logbf", FLT_MIN }, { "logbf", 0.0L }, { "logbf", INFINITY },
    { "logbf", NAN },
    { "logb", 1.17L }, { "logb", DBL_MIN }, { "logb", 0.0L },
    { "logbl", 1.17L }, { "logbl", 0.0L },
    /* large_3: sqrt of the real(10) kind.  */
    { "sqrtl", -1.0L }, { "sqrtl", 2.0L }, { "sqrtl", 4.0L },
    /* dec_math_1: sin, cos and tan of real(10) radians.  */
    { "tanl", 0.0L }, { "tanl", 0.523598775598298873L },
    { "tanl", 0.785398163397448310L }, { "tanl", 1.0L },
    { "sinl", 0.523598775598298873L }, { "cosl", 0.523598775598298873L },
    { "sincosl", 0.523598775598298873L },
    { "tan", 0.785398163397448310L }, { "tanf", 0.785398163397448310L },
    /* minmax_3/4: IEEE_MAX_NUM (fmax), real(4) first.  */
    { "fmaxf", 0.0L, 0.0L }, { "fmaxf", -0.0L, -0.0L },
    { "fmaxf", 9.0L, 0.0L }, { "fmaxf", 0.0L, 9.0L },
    { "fmaxf", INFINITY, 9.0L }, { "fmaxf", 0.0L, NAN },
    { "fmaxf", NAN, 0.0L }, { "fminf", 0.0L, 9.0L },
    { "fmax", 9.0L, 0.0L }, { "fmax", 0.0L, NAN },
    { "fmaxl", 9.0L, 0.0L }, { "fmaxl", 0.0L, NAN },
    /* Control: an exact square root.  */
    { "sqrtf", 4.0L },
  };
  for (int clr = 0; clr <= 1; clr++)
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++)
      run (c[i].n, c[i].x, c[i].y, clr ? FE_ALL_EXCEPT : 0);
  return 0;
}
