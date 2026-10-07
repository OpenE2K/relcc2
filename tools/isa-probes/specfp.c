/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What lcc's speculative FP operations do to a program that clears the
   FP sticky flags (spec/isa/speculation.md, "What lcc speculates").

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O3 specfp.c specfp-kern.c -lm -o specfp
     $LCC -O3 specfp.c -c && $LCC -O3 -fno-spec-fp specfp-kern.c -c &&
       $LCC specfp.o specfp-kern.o -lm -o specfp-nospec
     ./specfp 0; ./specfp 1        # on hardware

   Test 0 clears all flags and runs a dot product, test 1 clears
   FE_INEXACT and runs a conditional store loop. On the 4C, 8C2 and 2C3
   the default build dies with SIGILL (ILL_ILLOPN, si_code 2) in both;
   the -fno-spec-fp build runs and sets FE_INEXACT.  */
#include <fenv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
double dot (const double *, const double *, int);
void condstore (double *restrict, const double *restrict, int);
static double a[1000], b[1000], c[1000];
static void
on_sig (int s, siginfo_t *si, void *u)
{
  printf ("  -> signal %d (%s), si_code %d\n", s, strsignal (s), si->si_code);
  fflush (stdout);
  _exit (0);
}
static void
flags (const char *when)
{
  int f = fetestexcept (FE_ALL_EXCEPT);
  printf ("  flags %s: %s%s%s%s%s\n", when, f & FE_INEXACT ? "inexact " : "",
          f & FE_DIVBYZERO ? "divbyzero " : "", f & FE_INVALID ? "invalid " : "",
          f & FE_OVERFLOW ? "overflow " : "", f & FE_UNDERFLOW ? "underflow" : "");
}
int
main (int argc, char **argv)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = on_sig;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, 0);
  sigaction (SIGFPE, &sa, 0);
  for (int i = 0; i < 1000; i++)
    {
      a[i] = 1.0 / (i + 3);
      b[i] = i * 0.1;
    }
  int test = argc > 1 ? atoi (argv[1]) : 0;
  flags ("at start");
  if (test == 0)
    {
      printf ("dot, flags as the kernel set them: %.17g\n", dot (a, b, 1000));
      feclearexcept (FE_ALL_EXCEPT);
      flags ("after feclearexcept");
      double s = dot (a, b, 1000);
      printf ("dot after feclearexcept: %.17g\n", s);
      flags ("after dot");
    }
  else
    {
      feclearexcept (FE_INEXACT);
      flags ("after feclearexcept (FE_INEXACT)");
      condstore (c, b, 1000);
      printf ("condstore after feclearexcept: c[999] = %.17g\n", c[999]);
      flags ("after condstore");
    }
  return 0;
}
