/* SPDX-License-Identifier: GPL-3.0-or-later */
/* MCST glibc's libm in directed rounding modes and with the sticky FP
   flags cleared (spec/abi/registers.md, "MCST glibc's FP bugs").

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc   # v5.8c2, v6.2c3
     $LCC -O3 tools/abi/probes/libmchk.c -lm -o libmchk
     $LCC -O0 tools/abi/probes/libmchk.c -lm -o libmchk-O0
     $LCC -O3 -fno-spec-fp tools/abi/probes/libmchk.c -lm -o libmchk-nsf
     $LCC -O3 -static tools/abi/probes/libmchk.c -lm -o libmchk-static
     ./libmchk [sqrt|err|fmod]          # on hardware; no argument: all

   Each line is FUNCTION MODE CLEARED (ARGS) -> OUTCOME.  MODE is the
   rounding mode set with fesetround; CLEARED the flags cleared before
   the call: "-" none (the kernel starts processes with all six set),
   "V" FE_INVALID, "X" FE_INEXACT, "A" FE_ALL_EXCEPT.  Every case runs in
   a child with alarm (1): "HANG" means the call did not return within a
   second.  A double or float result with a nonzero tag would print as
   "DIAG" (gettagd doesn't trap on a diagnostic operand), but lcc first
   copies a call's result out of %db[0] with a plain addd (movif for
   long double), so a diagnostic result shows as "SIGILL si_code 2 at
   libmchk+OFFSET", OFFSET being that copy; a SIGILL inside a shared
   library names it with the offset from its load address.  Otherwise
   the line has the result (%a and its bits), errno and the flags set
   after the call.

   The d* cases call sqrt and sqrtf directly, so they show lcc's own
   expansion (an inline root, `,sm' at -O3, and a libm call for negative
   and NaN operands whose result is used); the others call through a
   volatile pointer, so lcc can neither inline nor fold them.  rootfirst
   computes the root with plain fsqrtid/fsqrttd and then calls sqrt for
   errno only, ignoring its result; rootlast calls first.

   Elbrus 4C (ALT's libm), 8C2 and 2C3 (lcc's sysroot libm), glibc
   2.35, alike:
   - FE_DOWNWARD: fmod and remainder never return when the operands'
     exponents differ by more than 26 (31 in the 2C3's v6 libm) and the
     reduction reaches an exact zero (fmod (0x1p52, 2), 0x1p30 % 2 on
     the 4C and 8C2 only, fmod (0x1p1023, 0.5), remainder (0x1p52, 2));
     the other exact zeros of fmod, fmodf, fmodl, remainder, remainderf
     and remainderl have the wrong sign (8 % 2 gives -0, -8 % 2 gives
     +0), except fmod (0x1.8p40, 3).  Nonzero results and the other
     three modes are right.  The static libm and glibc 2.41 (lcc
     1.31.05) do the same.
   - FE_INVALID cleared: sqrt, sqrtf, sqrtl (-1 or -inf) and fmod,
     fmodf, fmodl (x, 0) die with SIGILL inside the shared libm
     (__kernel_standard storing its speculative 0/0); rootfirst works,
     rootlast dies the same way.  The static libm returns NaN, EDOM, V.
   - FE_INEXACT cleared: sqrt (2) and sqrtf (2) return a diagnostic
     value (SIGILL in the caller; the 2C3's sqrtf traps inside libm),
     sqrtl (2) dies inside libm; the static libm is right.
     lcc's own -O3 inline root does the same (dsqrt; not with
     -fno-spec-fp or -O0).
   - All flags cleared: sin (1), exp (1), acos (0.5), atan (1),
     pow (2, 0.5), hypot (1, 1), fmod (5.5, 2), remainder (5.5, 2) die
     too, static or shared; log (2) and cbrt (2) don't.  At -O0 lcc
     stores the result with std,sm, so some come back as numbers with
     bit 30 of each word set (sqrt (2) as 0x7ff6a09e667f3bcd) instead.
   - acos (2), asin (2), log (-1), log10 (-1), logf (-1), log (0),
     exp (1000), pow (0, -1) set errno but raise no flag.  */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fenv.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char __executable_start[], etext[];

typedef double (*fd1) (double);
typedef double (*fd2) (double, double);
typedef float (*ff1) (float);
typedef float (*ff2) (float, float);
typedef long double (*fl1) (long double);
typedef long double (*fl2) (long double, long double);

static fd1 volatile p_sqrt = sqrt, p_log = log, p_acos = acos, p_asin = asin,
  p_acosh = acosh, p_atanh = atanh, p_sin = sin, p_exp = exp, p_cbrt = cbrt,
  p_atan = atan, p_log10 = log10;
static fd2 volatile p_fmod = fmod, p_rem = remainder, p_pow = pow,
  p_hypot = hypot;
static ff1 volatile p_sqrtf = sqrtf, p_logf = logf;
static ff2 volatile p_fmodf = fmodf, p_remf = remainderf;
static fl1 volatile p_sqrtl = sqrtl;
static fl2 volatile p_fmodl = fmodl, p_reml = remainderl;

__attribute__ ((noinline)) static double
d_sqrt (double x)
{
  return sqrt (x);
}

__attribute__ ((noinline)) static float
d_sqrtf (float x)
{
  return sqrtf (x);
}

/* The root as plain (non-speculative) operations, then libm only for
   errno, as doc/gcc-port-plan.md D8 recommends; "rootlast" calls
   first, as GCC's call DCE arranges it.  */
static double
asm_root (double x)
{
  double t, r;
  __asm__ volatile ("fsqrtid %1, %0" : "=r" (t) : "r" (x));
  __asm__ volatile ("fsqrttd %1, %2, %0" : "=r" (r) : "r" (x), "r" (t));
  return r;
}

__attribute__ ((noinline)) static double
rootfirst (double x)
{
  double r = asm_root (x);
  if (isless (x, 0.0))
    (void) p_sqrt (x);
  return r;
}

__attribute__ ((noinline)) static double
rootlast (double x)
{
  if (isless (x, 0.0))
    (void) p_sqrt (x);
  return asm_root (x);
}

enum ty { D, F, L };
struct op
{
  const char *name;
  enum ty ty;
  int nargs;
};

static const struct op ops[] = {
  { "sqrt", D, 1 }, { "sqrtf", F, 1 }, { "sqrtl", L, 1 },
  { "dsqrt", D, 1 }, { "dsqrtf", F, 1 }, { "rootfirst", D, 1 },
  { "rootlast", D, 1 },
  { "fmod", D, 2 }, { "fmodf", F, 2 }, { "fmodl", L, 2 },
  { "remainder", D, 2 }, { "remainderf", F, 2 }, { "remainderl", L, 2 },
  { "pow", D, 2 }, { "hypot", D, 2 },
  { "log", D, 1 }, { "logf", F, 1 }, { "acos", D, 1 }, { "asin", D, 1 },
  { "acosh", D, 1 }, { "atanh", D, 1 }, { "sin", D, 1 }, { "exp", D, 1 },
  { "cbrt", D, 1 }, { "atan", D, 1 }, { "log10", D, 1 },
};

static int
opn (const char *n)
{
  for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++)
    if (!strcmp (ops[i].name, n))
      return i;
  abort ();
}

static const char *const modename[] = { "near", "down", "up", "zero" };
static const int modes[] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD,
                             FE_TOWARDZERO };
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
    where = "libmchk", off = a - __executable_start;
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
flags (char *b, int e)
{
  int f = fetestexcept (FE_ALL_EXCEPT);
  sprintf (b, "errno %-6s flags %s%s%s%s%s%s", e == EDOM ? "EDOM"
           : e == ERANGE ? "ERANGE" : e ? "other" : "0",
           f & FE_INVALID ? "V" : "", f & FE_DIVBYZERO ? "Z" : "",
           f & FE_OVERFLOW ? "O" : "", f & FE_UNDERFLOW ? "U" : "",
           f & FE_INEXACT ? "X" : "", f ? "" : "-");
}

/* The tags of a double or float result, without a non-speculative
   use.  */
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
child (int op, double x, double y, int mode, int clr)
{
  struct sigaction sa = { 0 };
  sa.sa_sigaction = on_ill;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  alarm (1);
  char fb[64];
  const char *n = ops[op].name;
  fesetround (modes[mode]);
  feclearexcept (clr);
  errno = 0;
  switch (ops[op].ty)
    {
    case D:
      {
        double r;
        if (!strcmp (n, "sqrt")) r = p_sqrt (x);
        else if (!strcmp (n, "dsqrt")) r = d_sqrt (x);
        else if (!strcmp (n, "rootfirst")) r = rootfirst (x);
        else if (!strcmp (n, "rootlast")) r = rootlast (x);
        else if (!strcmp (n, "fmod")) r = p_fmod (x, y);
        else if (!strcmp (n, "remainder")) r = p_rem (x, y);
        else if (!strcmp (n, "pow")) r = p_pow (x, y);
        else if (!strcmp (n, "hypot")) r = p_hypot (x, y);
        else if (!strcmp (n, "log")) r = p_log (x);
        else if (!strcmp (n, "acos")) r = p_acos (x);
        else if (!strcmp (n, "asin")) r = p_asin (x);
        else if (!strcmp (n, "acosh")) r = p_acosh (x);
        else if (!strcmp (n, "atanh")) r = p_atanh (x);
        else if (!strcmp (n, "sin")) r = p_sin (x);
        else if (!strcmp (n, "exp")) r = p_exp (x);
        else if (!strcmp (n, "cbrt")) r = p_cbrt (x);
        else if (!strcmp (n, "atan")) r = p_atan (x);
        else r = p_log10 (x);
        unsigned t = tagd (r);
        int e = errno;
        flags (fb, e);
        fesetround (FE_TONEAREST);
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
    case F:
      {
        float r;
        if (!strcmp (n, "sqrtf")) r = p_sqrtf (x);
        else if (!strcmp (n, "dsqrtf")) r = d_sqrtf (x);
        else if (!strcmp (n, "fmodf")) r = p_fmodf (x, y);
        else if (!strcmp (n, "remainderf")) r = p_remf (x, y);
        else r = p_logf (x);
        unsigned t = tags (r);
        int e = errno;
        flags (fb, e);
        fesetround (FE_TONEAREST);
        if (t)
          printf ("%s -> DIAG tags %#x  %s\n", line, t, fb);
        else
          {
            unsigned b;
            memcpy (&b, &r, 4);
            printf ("%s -> %-24a %08x          %s\n", line, (double) r, b,
                    fb);
          }
        break;
      }
    case L:
      {
        long double r;
        if (!strcmp (n, "sqrtl")) r = p_sqrtl (x);
        else if (!strcmp (n, "fmodl")) r = p_fmodl (x, y);
        else r = p_reml (x, y);
        int e = errno;
        flags (fb, e);
        fesetround (FE_TONEAREST);
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
run (const char *n, double x, double y, int mode, int clr)
{
  int op = opn (n);
  static const char *const cn[] = { "-", "V", "X", "A" };
  int ci = clr == 0 ? 0 : clr == FE_INVALID ? 1 : clr == FE_INEXACT ? 2 : 3;
  char args[64];
  if (ops[op].nargs == 1)
    snprintf (args, sizeof args, "%a", x);
  else
    snprintf (args, sizeof args, "%a, %a", x, y);
  snprintf (line, sizeof line, "%-10s %-4s %s (%s)", n, modename[mode],
            cn[ci], args);
  fflush (stdout);
  pid_t p = fork ();
  if (p == 0)
    child (op, x, y, mode, clr);
  int st;
  waitpid (p, &st, 0);
  if (WIFSIGNALED (st) && WTERMSIG (st) == SIGALRM)
    printf ("%s -> HANG\n", line);
  else if (WIFSIGNALED (st))
    printf ("%s -> killed by signal %d\n", line, WTERMSIG (st));
}

static const int clrs[] = { 0, FE_INVALID, FE_INEXACT, FE_ALL_EXCEPT };

static void
sqrt_cases (void)
{
  static const double xs[] = { -1.0, 2.0, 4.0, -0.0, -INFINITY, NAN,
                               0x1p-1060 };
  static const char *const fs[] = { "sqrt", "sqrtf", "sqrtl", "dsqrt",
                                     "dsqrtf", "rootfirst", "rootlast" };
  for (unsigned f = 0; f < 7; f++)
    for (unsigned c = 0; c < 4; c++)
      for (unsigned i = 0; i < sizeof xs / sizeof *xs; i++)
        if (f == 0 || i < 3)
          run (fs[f], xs[i], 0, 0, clrs[c]);
}

static void
err_cases (void)
{
  static const struct
  {
    const char *n;
    double x, y;
  } e[] = {
    { "log", -1.0 }, { "log", 0.0 }, { "log", 2.0 }, { "logf", -1.0 },
    { "log10", -1.0 }, { "acos", 2.0 }, { "acos", -1.0 }, { "acos", 0.5 },
    { "asin", 2.0 }, { "acosh", 0.5 }, { "atanh", 2.0 }, { "sin", 1.0 },
    { "exp", 1.0 }, { "exp", 1000.0 }, { "cbrt", 2.0 }, { "atan", 1.0 },
    { "pow", -1.0, 0.5 }, { "pow", 2.0, 0.5 }, { "pow", 0.0, -1.0 },
    { "hypot", 1.0, 1.0 }, { "fmod", 1.0, 0.0 }, { "fmod", INFINITY, 1.0 },
    { "fmod", 5.5, 2.0 }, { "fmodf", 1.0, 0.0 }, { "fmodl", 1.0, 0.0 },
    { "remainder", 1.0, 0.0 }, { "remainder", 5.5, 2.0 },
  };
  for (unsigned c = 0; c < 4; c += 1 + (c == 1))
    for (unsigned i = 0; i < sizeof e / sizeof *e; i++)
      run (e[i].n, e[i].x, e[i].y, 0, clrs[c]);
}

static void
fmod_cases (void)
{
  static const struct
  {
    const char *n;
    double x, y;
  } e[] = {
    { "fmod", 0x1p52, 2 }, { "fmod", -0x1p52, 2 }, { "fmod", 0x1p40, 2 },
    { "fmod", 0x1p30, 2 }, { "fmod", 0x1p27, 2 }, { "fmod", 0x1p26, 2 },
    { "fmod", 0x1p1023, 0.5 }, { "fmod", 8, 2 }, { "fmod", -8, 2 },
    { "fmod", 6, 3 }, { "fmod", -6, 3 }, { "fmod", 1.5, 0.5 },
    { "fmod", 0x1.8p40, 3 }, { "fmod", -8, 3 }, { "fmod", 5, 3 },
    { "fmod", -1e20, 3 }, { "fmod", -1e300, 7 }, { "fmod", 2, 2 },
    { "fmod", 5e-324, 5e-324 }, { "fmod", 0x1p-1060, 0x1p-1070 },
    { "fmodf", 0x1p40, 2 }, { "fmodf", -0x1p40, 2 }, { "fmodf", 8, 2 },
    { "fmodf", -8, 2 }, { "fmodf", 5, 3 },
    { "fmodl", 0x1p64, 2 }, { "fmodl", -0x1p64, 2 }, { "fmodl", 0x1p40, 2 },
    { "fmodl", 0x1p30, 2 }, { "fmodl", 8, 2 }, { "fmodl", -8, 2 },
    { "fmodl", 5, 3 },
    { "remainder", 0x1p52, 2 }, { "remainder", 8, 2 },
    { "remainder", -8, 2 }, { "remainderf", 0x1p40, 2 },
    { "remainderf", 8, 2 }, { "remainderf", -8, 2 },
    { "remainderl", 0x1p64, 2 }, { "remainderl", 8, 2 },
    { "remainderl", -8, 2 },
  };
  for (unsigned m = 0; m < 4; m++)
    for (unsigned i = 0; i < sizeof e / sizeof *e; i++)
      run (e[i].n, e[i].x, e[i].y, m, 0);
}

int
main (int argc, char **argv)
{
  const char *w = argc > 1 ? argv[1] : "all";
  int all = !strcmp (w, "all");
  if (all || !strcmp (w, "sqrt"))
    sqrt_cases ();
  if (all || !strcmp (w, "err"))
    err_cases ();
  if (all || !strcmp (w, "fmod"))
    fmod_cases ();
  return 0;
}
