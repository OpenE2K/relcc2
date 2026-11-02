/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A call-clobbered global register carries a diagnostic value from one
   function into another, as the pairs a caller hands over do
   (spec/abi/calling-convention.md, "Unused parts of a slot";
   doc/gcc-port-plan.md D7, "A register that may hold no value";
   docs/spec-poc-findings.md P124, P129).

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   # or v5.8c2
     $LCC -O1 tools/abi/probes/foldp129_greg.c \
         tools/abi/probes/foldp129_greg.s -o foldp129_greg
     ./foldp129_greg                     # on hardware: every case

   Each case runs in a child.  A case is two letters, WHO then READER.
   WHO says who leaves what in %g24 before READER runs:
     a  g24_after: a callee (g24_spoil) leaves a failed ldd,sm's value,
        and the next callee from the same frame reads %g24
     c  g24_caller: the caller does the ldd,sm itself, then calls READER
     y  g24_sys: as c with a system call (getpid, entry 3) between
     n  as a, but g24_spoil loads from a valid address (5): control
   READER reads %g24, which it never wrote, and stores the result:
     p  std,sm of %g24 and gettagd (no trap expected): bits and tag
     s  plain std            a  addd %dg24, 1
     m  muld %dg24, 3        c  cmpedb %dg24, 3 (1 if equal)
   Output: CASE -> the stored doubleword(s), or the signal, its si_code
   and the offset of the faulting instruction in the probe.

   Elbrus 4C (ALT, lcc 1.29.16 v3 -O1) and 8C2 (OS Elbrus 9.5.5, v5.8c2),
   2026-11-03, alike: ap and cp read 4afafafa4afafafa with tag 5 (the
   failed load's value, still in %g24 in the next callee, whether a
   callee or the caller left it there); as, aa, am, ac and cs, ca, cm,
   cc die with SIGILL ILL_ILLOPN (si_code 2) at the reader's plain std,
   addd, muld or cmpedb; after the system call (y) %g24 is empty (0,
   tag 5) and every reader works (s 0, a 1, m 0, c 0); n stores 5, 6,
   15 and 0.  Not run on the 2C3 (offline).  */

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char __executable_start[];

void g24_spoil (void *);
void g24_peek (long *);
void g24_std (long *);
void g24_addd (long *);
void g24_muld (long *);
void g24_cmp (long *);
void g24_caller (void *, void (*) (long *), long *);
void g24_after (void *, void (*) (long *), long *);
void g24_sys (void *, void (*) (long *), long *);

static long good = 5;
static char line[64];

static void
on_sig (int s, siginfo_t *si, void *u)
{
  (void) u;
  char buf[160];
  int n = snprintf (buf, sizeof buf, "%s -> signal %d si_code %d at +%#lx\n",
                    line, s, si->si_code,
                    (unsigned long) ((char *) si->si_addr
                                     - __executable_start));
  write (1, buf, n);
  _exit (1);
}

static void
run (char who, char rd)
{
  void (*fn) (long *) = rd == 'p' ? g24_peek : rd == 's' ? g24_std
                        : rd == 'a' ? g24_addd : rd == 'm' ? g24_muld
                        : g24_cmp;
  snprintf (line, sizeof line, "%c%c", who, rd);
  fflush (stdout);
  pid_t p = fork ();
  if (p == 0)
    {
      struct sigaction sa;
      memset (&sa, 0, sizeof sa);
      sa.sa_sigaction = on_sig;
      sa.sa_flags = SA_SIGINFO;
      sigaction (SIGILL, &sa, NULL);
      sigaction (SIGSEGV, &sa, NULL);
      alarm (2);
      long out[2] = { 0x1111111111111111L, 0x1111111111111111L };
      void *bad = (void *) 8;
      switch (who)
        {
        case 'a': g24_after (bad, fn, out); break;
        case 'c': g24_caller (bad, fn, out); break;
        case 'y': g24_sys (bad, fn, out); break;
        default: g24_after (&good, fn, out); break;
        }
      if (rd == 'p')
        printf ("%s -> %016lx tag %ld\n", line, out[0], out[1]);
      else
        printf ("%s -> %016lx\n", line, out[0]);
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (p, &st, 0);
  if (WIFSIGNALED (st))
    printf ("%s -> killed by signal %d\n", line, WTERMSIG (st));
}

int
main (int argc, char **argv)
{
  if (argc > 1)
    {
      run (argv[1][0], argv[1][1]);
      return 0;
    }
  static const char who[] = "acyn", rd[] = "psamc";
  for (int i = 0; who[i]; i++)
    for (int j = 0; rd[j]; j++)
      run (who[i], rd[j]);
  return 0;
}
