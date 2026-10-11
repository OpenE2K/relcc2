/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What glibc's <fenv.h> functions do to %pfpfr's fz (bit 15) and daz
   (bit 6) bits, and what %pfpfr a program starts with (with -ffast-math
   lcc links MCST's crtfastmath.o, which sets both); and what
   fesetenv (FE_DFL_ENV) leaves: MCST's glibc writes %pfpfr = 0x3d << 7
   and %fpcr = 0x3d, so the denormal exception is unmasked in both and
   %fpcr's precision control is 0 (single).

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O2 tools/isa-probes/fenvlib.c -o fenvlib -lm
     $LCC -O2 -fno-spec-fp tools/isa-probes/fenvlib.c -o fenvlib-nsf -lm
     $LCC -O2 -ffast-math tools/isa-probes/fenvlib.c -o fenvlib-fast -lm
     ./fenvlib; ./fenvlib-nsf; ./fenvlib-fast   # on hardware

   lcc's -O2 makes FP operations speculative (,sm); with an exception
   unmasked they give a diagnostic value and the program gets SIGILL
   where -fno-spec-fp code gets SIGFPE (speculation.md), so the
   FE_DFL_ENV case runs in a child.  */

#define _GNU_SOURCE
#include <fenv.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned
rd (void)
{
  unsigned x;
  __asm__ volatile ("rrs %%pfpfr, %0" : "=r" (x));
  return x;
}

static void
wr (unsigned x)
{
  __asm__ volatile ("rws %0, %%pfpfr" : : "r" (x));
}

#define FZDAZ 0x8040u

static void
show (const char *what)
{
  unsigned x = rd ();
  printf ("  %-34s %%pfpfr %#06x: fz %u daz %u\n", what, x, x >> 15 & 1,
          x >> 6 & 1);
}

static sigjmp_buf jb;
static volatile int got;

static void
fpe (int sig, siginfo_t *si, void *uc)
{
  (void) uc;
  got = sig << 8 | si->si_code;
  siglongjmp (jb, 1);
}

/* After fesetenv (FE_DFL_ENV): does a denormal operand trap, and how
   precise is long double?  */
static void
dfl (void)
{
  struct sigaction sa = { 0 };
  sa.sa_sigaction = fpe;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGFPE, &sa, NULL);
  volatile double one = 1.0, den = 4.9e-324, r;
  volatile long double a = 1.0L, b = 3.0L, q0, q1;
  unsigned c;
  q0 = a / b;
  fesetenv (FE_DFL_ENV);
  __asm__ volatile ("rrs %%fpcr, %0" : "=r" (c));
  q1 = a / b;
  got = 0;
  if (!sigsetjmp (jb, 1))
    r = one + den;
  fesetenv (FE_DFL_ENV);
  printf ("  after fesetenv (FE_DFL_ENV): %%pfpfr %#x, %%fpcr %#x; "
          "1.0 + denormal: %s (signal %d, si_code %d); long double 1/3 "
          "%La (before: %La)\n", rd (), c, got ? "trapped" : "no trap",
          got >> 8, got & 0xff, (long double) q1, (long double) q0);
  (void) r;
}

int
main (void)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  fenv_t e, start;
  fegetenv (&start);
  show ("at main");
  wr (rd () | FZDAZ);
  show ("after setting fz and daz");
  fesetround (FE_UPWARD);
  show ("fesetround (FE_UPWARD)");
  fesetround (FE_TONEAREST);
  feclearexcept (FE_ALL_EXCEPT);
  show ("feclearexcept");
  feholdexcept (&e);
  show ("feholdexcept");
  feupdateenv (&e);
  show ("feupdateenv");
  fegetenv (&e);
  fesetenv (FE_DFL_ENV);
  show ("fesetenv (FE_DFL_ENV)");
  fesetenv (&e);
  show ("fesetenv (saved by fegetenv)");
  fenv_t f;
  fegetenv (&f);
  printf ("  fenv_t: __pfpfr %#x\n", (unsigned) f.__pfpfr);
  feenableexcept (FE_DIVBYZERO);
  show ("feenableexcept (FE_DIVBYZERO)");
  fedisableexcept (FE_DIVBYZERO);
  fesetenv (FE_NOMASK_ENV);
  show ("fesetenv (FE_NOMASK_ENV)");
  fesetenv (&start);
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      dfl ();
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("  after fesetenv (FE_DFL_ENV), 1.0 + denormal: killed by "
            "signal %d\n", WTERMSIG (st));
  return 0;
}
