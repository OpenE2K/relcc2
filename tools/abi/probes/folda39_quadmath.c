/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The FP exceptions of __float128 arithmetic and of libquadmath as lcc
   built them (spec/abi/registers.md, "MCST glibc's FP bugs";
   docs/spec-poc-findings.md A39; tools/quadmath is the full
   comparison).

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   # or v5.8c2
     $L -O3 tools/abi/probes/folda39_quadmath.c -lquadmath -lm -o quadmath
     ./quadmath                 # on hardware: the system's libquadmath
     LD_LIBRARY_PATH=DIR ./quadmath   # another libquadmath.so.0
     gcc -O2 tools/abi/probes/folda39_quadmath.c -lquadmath -lm -o q-x86
                                # the reference (x86-64 GCC's library)

   Each line is CALL MODE CLEARED -> the result's 32 hex digits (sign
   and exponent first) and the flags set after the call (i z o u x:
   invalid, divide by zero, overflow, underflow, inexact; - none).  MODE
   is the rounding mode (n d u z); CLEARED is "-" for none (processes
   start with all flags set, so then the flags say nothing) or "A" for
   feclearexcept (FE_ALL_EXCEPT) before the call.  Every case runs in a
   child with alarm (1); a SIGILL prints the library and offset.  The
   "arith" lines are the program's own __float128 operations, which lcc
   compiles to calls of libgcc's soft-fp (__divtf3, __multf3, ...).

   Why (MCST's GPL runtime sources, toolchains/src/lcc-1.29.16, and the
   disassembly of what lcc built from them):
   - Soft-fp raises flags through __sfp_handle_exceptions
     (libgcc/config/e2k/sfp-exceptions.c:5-34), which computes 0/0, 1/0,
     max + max, min * min and max - 1 from const doubles under
     optimize (0).  lcc folds the last three: its libgcc.a
     (sfp-exceptions.o) and libgcc_s.so.1 (v3 sysroot, 0x1e4f8-0x1e598)
     store the constants 0x7ff0000000000000, 0 and 0x7fefffffffffffff
     and keep only the two fdivd.  libquadmath.so.0 carries its own copy
     (no __addtf3 among its undefined symbols).  So only invalid and
     divide by zero are ever raised.
   - fmaq (libquadmath/math/fmaq.c:218-225) rounds to odd by or-ing
     fetestexcept (FE_INEXACT) into the last bit after an addition, which
     never raises it here.
   - coshq's overflow is huge * huge of a static const (coshq.c:56,107),
     which lcc folds to +inf, so directed modes get inf.
   - GCC 11's sqrtq (sqrtq.c:23-31) starts from libm's sqrt, which dies
     after feclearexcept (registers.md); GCC 14 replaced that with
     soft-fp (PR 114623).  The trigonometric functions reduce in double
     (rem_pio2q.c), lcc's speculative code.
   - GCC 11's strtoflt128 returns NAN for "-nan" (strtod_l.c:651-690);
     GCC 13 applies the sign (df63f4162c7, PR 94756).

   2026-11-03, lcc 1.29.16 -O3 programs, the 4C (ALT's libquadmath and
   the v3 sysroot's, alike) and the 8C2 (its own, the v5.8c2 sysroot's),
   alike but for the trap offsets:
   - Flags cleared: the arithmetic, expq, coshq and fmaq raise nothing
     (x86: x, o x, u x); 0/0, 1/0, logq (0) and logq (-1) raise i or z
     as on x86.  sqrtq (2), powq (2, 0.5), hypotq (1, 1), asinq and
     acosq (0.75) and tgammaq (30.5) die in __extenddftf2
     (libquadmath+0xc7438 on the 4C), converting libm's diagnostic
     sqrt; sinq (1e10) and j0q (50) die in __quadmath_kernel_rem_pio2
     (+0xf300).  hypotq (3, 4), sinq, cosq, j0q (1), asinq (0.5),
     tgammaq (0.5) and atanq (1) return.
   - Flags left set: fmaq (3, y, 1) is 1, not 1 + 2^-112; coshq (12000)
     is +inf rounding down and toward zero (x86: the largest finite);
     sqrtq (2), powq (2, 0.5) and hypotq (1, 1) are one ulp high
     (...ea96); expq (-12000) rounding up is the smallest subnormal,
     which is right (x86 GCC folds expq.c's TINY * TINY: 0);
     strtoflt128 gives +NaN for "-nan" and "-nan(0x123)".
   x86's own sqrtq (GCC 14 and later) raises no inexact either:
   libquadmath/sfp-machine.h defines no FP_EX_INEXACT, which
   soft-fp.h:110-111 then makes 0.  The 2C3 is not run yet.  */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <fenv.h>
#include <quadmath.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef __float128 Q;

extern char __executable_start[], etext[];

static char line[160];

static Q
raw (uint64_t hi, uint64_t lo)
{
  uint64_t w[2] = { lo, hi };
  Q q;
  memcpy (&q, w, sizeof q);
  return q;
}

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
    where = "quadmath", off = a - __executable_start;
  else if (dladdr (a, &di) && di.dli_fname)
    where = strrchr (di.dli_fname, '/') ? strrchr (di.dli_fname, '/') + 1
                                        : di.dli_fname,
    off = a - (char *) di.dli_fbase;
  int n = snprintf (buf, sizeof buf, "%s -> SIGILL si_code %d at %s+%#lx\n",
                    line, si->si_code, where, off);
  if (write (1, buf, n) < 0)
    _exit (s);
  _exit (s);
}

/* Operands the compiler cannot fold.  */
static volatile Q one = 1, two = 2, three = 3, four = 4, zero = 0, half = 0.5,
  big = 1e4900Q, tiny = 1e-4900Q, e12k = 12000, me12k = -12000, mone = -1,
  q34 = 0.75, fifty = 50, g = 30.5, e10 = 1e10;
static volatile double r_d;
static volatile Q r_q;

enum { DIV13, OVF, UNF, INV, DIVZ, CVT, EXP1, EXPO, EXPU, LOG0, LOGM1,
       FMA, COSH, SQRT, HYPOT, SIN, COS, ASIN, POW, J0, TGAMMA, ATAN,
       HYPOT11, ASIN34, ACOS34, SINBIG, J050, TGAMMA30, NAN1, NAN2, NOPS };

static const char *const opname[] = {
  "arith 1/3", "arith 1e4900*1e4900", "arith 1e-4900*1e-4900",
  "arith 0/0", "arith 1/0", "arith (double) 1/3", "expq (1)",
  "expq (12000)", "expq (-12000)", "logq (0)", "logq (-1)",
  "fmaq (3, y, 1)", "coshq (12000)", "sqrtq (2)", "hypotq (3, 4)",
  "sinq (1)", "cosq (1)", "asinq (0.5)", "powq (2, 0.5)", "j0q (1)",
  "tgammaq (0.5)", "atanq (1)", "hypotq (1, 1)", "asinq (0.75)",
  "acosq (0.75)", "sinq (1e10)", "j0q (50)", "tgammaq (30.5)",
  "strtoflt128 (-nan)",
  "strtoflt128 (-nan(0x123))",
};

static Q
call (int op)
{
  /* fmaq (3, y, 1) with y = 2^-113 * the double-rounded-up 1/3: x * y
     is 2^-113 + 2^-226 exactly, so the exact sum is just above the
     midpoint 1 + 2^-113 and rounds to nearest as 1 + 2^-112; without
     the sticky bit the midpoint rounds to even, 1.  */
  Q y = raw (0x3f8c555555555555ull, 0x5555555555555556ull);
  switch (op)
    {
    case DIV13: return one / three;
    case OVF: return big * big;
    case UNF: return tiny * tiny;
    case INV: return zero / zero;
    case DIVZ: return one / zero;
    case CVT: r_q = one / three; r_d = (double) r_q; return r_d;
    case EXP1: return expq (one);
    case EXPO: return expq (e12k);
    case EXPU: return expq (me12k);
    case LOG0: return logq (zero);
    case LOGM1: return logq (mone);
    case FMA: return fmaq (three, y, one);
    case COSH: return coshq (e12k);
    case SQRT: return sqrtq (two);
    case HYPOT: return hypotq (three, four);
    case SIN: return sinq (one);
    case COS: return cosq (one);
    case ASIN: return asinq (half);
    case POW: return powq (two, half);
    case J0: return j0q (one);
    case TGAMMA: return tgammaq (half);
    case ATAN: return atanq (one);
    case HYPOT11: return hypotq (one, one);
    case ASIN34: return asinq (q34);
    case ACOS34: return acosq (q34);
    case SINBIG: return sinq (e10);
    case J050: return j0q (fifty);
    case TGAMMA30: return tgammaq (g);
    case NAN1: return strtoflt128 ("-nan", NULL);
    default: return strtoflt128 ("-nan(0x123)", NULL);
    }
}

static const int modes[] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD,
                             FE_TOWARDZERO };

static void
child (int op, int mode, int clr)
{
  struct sigaction sa = { 0 };
  sa.sa_sigaction = on_ill;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  alarm (1);
  fesetround (modes[mode]);
  feclearexcept (clr);
  r_q = call (op);
  int f = fetestexcept (FE_ALL_EXCEPT);
  fesetround (FE_TONEAREST);
  Q r = r_q;
  uint64_t w[2];
  memcpy (w, &r, 16);
  printf ("%s -> %016llx%016llx %s%s%s%s%s%s\n", line,
          (unsigned long long) w[1], (unsigned long long) w[0],
          f & FE_INVALID ? "i" : "", f & FE_DIVBYZERO ? "z" : "",
          f & FE_OVERFLOW ? "o" : "", f & FE_UNDERFLOW ? "u" : "",
          f & FE_INEXACT ? "x" : "", f ? "" : "-");
  fflush (stdout);
  _exit (0);
}

static void
run (int op, int mode, int clr)
{
  snprintf (line, sizeof line, "%-26s %c %c", opname[op], "nduz"[mode],
            clr ? 'A' : '-');
  fflush (stdout);
  pid_t p = fork ();
  if (p == 0)
    child (op, mode, clr);
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
  /* Exceptions: every operation with the flags cleared.  */
  for (int op = 0; op < NOPS; op++)
    run (op, 0, FE_ALL_EXCEPT);
  /* Values: the flags left set, so nothing dies.  */
  for (int op = SQRT; op <= TGAMMA30; op++)
    run (op, 0, 0);
  run (FMA, 0, 0);
  for (int m = 0; m < 4; m++)
    run (COSH, m, 0);
  for (int m = 0; m < 4; m++)
    run (EXPU, m, 0);
  return 0;
}
