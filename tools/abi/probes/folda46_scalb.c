/* SPDX-License-Identifier: GPL-3.0-or-later */
/* MCST glibc's scalb, scalbf and scalbl for an exponent of 2^63 or more
   (spec/abi/registers.md, "MCST glibc's FP bugs";
   docs/spec-poc-findings.md A46).

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   # or v5.8c2
     $L -O3 tools/abi/probes/folda46_scalb.c -lm -o scalb
     $L -O3 -static tools/abi/probes/folda46_scalb.c -lm -o scalb-static
     ./scalb                   # on hardware

   Each line is FUNCTION CLEARED (X, FN) -> RESULT, its bits, errno and
   the flags set after the call (V invalid, Z divide by zero, O
   overflow, U underflow, X inexact).  CLEARED is the flags cleared
   before the call: "-" none (processes start with all six set, so the
   flags show nothing then), "V" FE_INVALID, "A" FE_ALL_EXCEPT.  Every
   case runs in a child with alarm (1); a SIGILL (a speculative FP
   operation meeting a cleared flag, registers.md) prints where it hit.
   The functions are called through volatile pointers.

   glibc's tests (math/libm-test-scalb.inc:129-144) want scalb (x,
   +-max) to overflow or underflow with no invalid.  MCST's e_scalb.c
   for __iset__ <= 3 (sysdeps/e2k/fpu/e_scalb.c:40-70) computes
   k += (LL) fn before it tests fn >= 5000 or <= -5000; for |fn| >= 2^63
   the conversion gives 0x8000000000000000 and raises invalid, and k
   takes the underflow branch.  e_scalbf.c:55-60 and e_scalbl.c:50-56
   do the same.  For __iset__ >= 4 they clamp fn first and use fscaled,
   fscales and fxscalesx (:72-80, :74-82, :69-75).

   2026-11-03, lcc 1.29.16 -O3:
   - v3 builds: the 4C's ALT libm, the v3 sysroot's static libm.a (on
     the 4C and on the 8C2) and lcc 1.31.05's v3 libm.a (glibc 2.41).
     With the flags left set, scalb (1, fn) is +0 for fn 0x1p63, 1e300
     and DBL_MAX (want +inf with O X), scalb (-3, 0x1p64) is -0, and
     scalbf (1, 0x1p63f), (1, FLT_MAX), scalbl (1, 0x1p63L), (1,
     0x1p100L), (1, LDBL_MAX) are +0.  fn 0x1p62 and 2000 overflow
     right, every negative fn gives +0, and scalb (0x1p-1074, DBL_MAX)
     is +inf (k wraps around).  With FE_INVALID cleared, each fn of
     2^63 or more dies with SIGILL inside libm: the conversion is
     fdtoidtr,1,sm at __scalb_finite (4C libm+0x95400), whose
     diagnostic result the first plain use (+0x95610) traps on; a
     negative one still gives +0 and raises no invalid.
   - v5 builds: the 8C2's libm and the v5.8c2 static libm.a: every value
     right, and with FE_INVALID cleared no invalid and no trap.
   - Both: with all flags cleared, every call whose result is inexact,
     overflows or underflows dies with SIGILL inside libm (scalb (1, 10)
     returns), as most of MCST's libm does.
   The 2C3 (v6) is not run yet.  */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
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

static double (*volatile p_scalb) (double, double) = scalb;
static float (*volatile p_scalbf) (float, float) = scalbf;
static long double (*volatile p_scalbl) (long double, long double) = scalbl;

static char line[160];

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
    where = "scalb", off = a - __executable_start;
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

/* T 0 double, 1 float, 2 long double.  */
static void
child (int t, long double x, long double fn, int clr)
{
  struct sigaction sa = { 0 };
  sa.sa_sigaction = on_ill;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  alarm (1);
  char fb[64];
  feclearexcept (clr);
  errno = 0;
  if (t == 0)
    {
      double r = p_scalb ((double) x, (double) fn);
      int e = errno;
      flags (fb, e);
      unsigned long b;
      memcpy (&b, &r, 8);
      printf ("%s -> %-22a %016lx      %s\n", line, r, b, fb);
    }
  else if (t == 1)
    {
      float r = p_scalbf ((float) x, (float) fn);
      int e = errno;
      flags (fb, e);
      unsigned b;
      memcpy (&b, &r, 4);
      printf ("%s -> %-22a %08x              %s\n", line, (double) r, b, fb);
    }
  else
    {
      long double r = p_scalbl (x, fn);
      int e = errno;
      flags (fb, e);
      unsigned char b[16];
      memcpy (b, &r, 16);
      printf ("%s -> %-22La %04x%016lx  %s\n", line, r,
              *(unsigned short *) (b + 8), *(unsigned long *) b, fb);
    }
  fflush (stdout);
  _exit (0);
}

static void
run (int t, long double x, long double fn, const char *fns, int clr)
{
  static const char *const tn[] = { "scalb", "scalbf", "scalbl" };
  snprintf (line, sizeof line, "%-6s %s (%La, %s)", tn[t],
            clr == 0 ? "-" : clr == FE_INVALID ? "V" : "A", x, fns);
  fflush (stdout);
  pid_t p = fork ();
  if (p == 0)
    child (t, x, fn, clr);
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
    int t;
    long double x, fn;
    const char *fns;
  } c[] = {
    { 0, 1, 10, "10" }, { 0, 1, 2000, "2000" }, { 0, 1, -2000, "-2000" },
    { 0, 1, 0x1p62, "0x1p62" }, { 0, 1, 0x1p63, "0x1p63" },
    { 0, 1, -0x1p62, "-0x1p62" }, { 0, 1, -0x1p63, "-0x1p63" },
    { 0, 1, 1e300, "1e300" }, { 0, 1, DBL_MAX, "DBL_MAX" },
    { 0, 1, -DBL_MAX, "-DBL_MAX" }, { 0, DBL_MAX, -DBL_MAX, "-DBL_MAX" },
    { 0, -3, 0x1p64, "0x1p64" }, { 0, 0x1p-1074, DBL_MAX, "DBL_MAX" },
    { 1, 1, 10, "10" }, { 1, 1, 200, "200" }, { 1, 1, 0x1p62, "0x1p62" },
    { 1, 1, 0x1p63, "0x1p63" }, { 1, 1, -0x1p63, "-0x1p63" },
    { 1, 1, FLT_MAX, "FLT_MAX" }, { 1, 1, -FLT_MAX, "-FLT_MAX" },
    { 2, 1, 10, "10" }, { 2, 1, 20000, "20000" },
    { 2, 1, 0x1p62L, "0x1p62" }, { 2, 1, 0x1p63L, "0x1p63" },
    { 2, 1, -0x1p63L, "-0x1p63" }, { 2, 1, 0x1p100L, "0x1p100" },
    { 2, 1, LDBL_MAX, "LDBL_MAX" }, { 2, 1, -LDBL_MAX, "-LDBL_MAX" },
  };
  static const int clrs[] = { 0, FE_INVALID, FE_ALL_EXCEPT };
  for (unsigned k = 0; k < 3; k++)
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++)
      run (c[i].t, c[i].x, c[i].fn, c[i].fns, clrs[k]);
  return 0;
}
