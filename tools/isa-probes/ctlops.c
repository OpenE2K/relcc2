/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Control and system operations from user mode (spec/isa/operations.md):
   each in its own process, printing "ok" or the signal and si_code.
   ctlops.s is hand-written.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $LCC -O3 tools/isa-probes/ctlops.c tools/isa-probes/ctlops.s -o ctlops
     ./ctlops                             # on hardware  */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define OPS(X) X (flushr) X (flushc) X (flushts) X (pref) X (sdisp) \
               X (gettsd) X (vfrpsz) X (settr) X (setei) X (setsft) X (srp) \
               X (crp) X (slrp) X (vfdi) X (set_mark) X (movep) X (done) \
               X (clpandp)

#define DECL(n) void t_##n (void);
OPS (DECL)
#undef DECL

static void
handler (int s, siginfo_t *si, void *uc)
{
  char b[64];
  int n = snprintf (b, sizeof b, "signal %d code %d\n", s, si->si_code);
  (void) uc;
  write (1, b, n);
  _exit (0);
}

int
main (void)
{
#define ENTRY(n) { #n, t_##n },
  static const struct { const char *name; void (*f) (void); } t[] = {
    OPS (ENTRY)
  };
#undef ENTRY
  for (unsigned i = 0; i < sizeof t / sizeof *t; i++)
    {
      fflush (stdout);
      if (fork () == 0)
        {
          struct sigaction sa;
          memset (&sa, 0, sizeof sa);
          sa.sa_sigaction = handler;
          sa.sa_flags = SA_SIGINFO;
          sigaction (SIGILL, &sa, NULL);
          sigaction (SIGSEGV, &sa, NULL);
          sigaction (SIGBUS, &sa, NULL);
          printf ("%-8s ", t[i].name);
          fflush (stdout);
          t[i].f ();
          printf ("ok\n");
          fflush (stdout);
          _exit (0);
        }
      int st;
      wait (&st);
      if (WIFSIGNALED (st))
        printf ("killed by signal %d\n", WTERMSIG (st));
    }
  return 0;
}
