/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Operations binutils lists for v1 and v2 only (protected-mode object
   descriptors), assembled for v2 and run on v3 and later
   (spec/isa/operations-v3.md): each in its own process, printing "ok" or
   the signal and si_code.  addd is the control.  v2ops.s is
   hand-written.

     as -mcpu=elbrus-v2 tools/isa-probes/v2ops.s -o v2ops.o  # MCST gas
     $LCC -O3 tools/isa-probes/v2ops.c v2ops.o -o v2ops
     ./v2ops                              # on hardware  */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define OPS(X) X (cast) X (tdtomp) X (odtoap) X (ldodwd) X (gettd) \
               X (getsod) X (stodwd) X (addd)

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
