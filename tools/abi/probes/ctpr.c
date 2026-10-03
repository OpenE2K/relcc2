/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Edge cases of the control-transfer preparation registers %ctpr1-3
   (spec/isa/control-transfer.md).

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O1 tools/abi/probes/ctpr.c tools/abi/probes/ctpr.s -o ctpr
     ./ctpr                        # on hardware, or under qemu-e2k

   Each case prints what it returned, or the signal it raised.  */

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

long t_keep_across_call (void);
long t_callee_sees (void);
long t_false_on_emptied (void);
long t_branch_keeps (void);
long t_diag_true (long *);
long t_diag_false (long *);

static sigjmp_buf env;
static volatile int sig, code;

static void
handler (int s, siginfo_t *si, void *uc)
{
  (void) uc;
  sig = s;
  code = si->si_code;
  siglongjmp (env, 1);
}

static void
run (const char *what, long (*f) (void), const char *meaning)
{
  sig = 0;
  if (sigsetjmp (env, 1) == 0)
    {
      long r = f ();
      printf ("%-20s returned %ld  (%s)\n", what, r, meaning);
    }
  else
    printf ("%-20s signal %d (%s), si_code %d\n", what, sig, strsignal (sig),
            code);
}

static long diag_true (void) { return t_diag_true ((long *) 8); }
static long diag_false (void) { return t_diag_false ((long *) 8); }

int
main (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = handler;
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;
  sigaction (SIGILL, &sa, NULL);
  sigaction (SIGSEGV, &sa, NULL);
  sigaction (SIGBUS, &sa, NULL);
  setvbuf (stdout, NULL, _IOLBF, 0);

  run ("keep_across_call", t_keep_across_call,
       "1 = %ctpr2 survived the call, 0 = fell through");
  run ("callee_sees", t_callee_sees,
       "1 = the callee used the caller's %ctpr2");
  run ("false_on_emptied", t_false_on_emptied,
       "1 = ct ? false on an emptied %ctpr2 did not trap");
  run ("branch_keeps", t_branch_keeps,
       "1 = %ctpr2 survived a taken ct through %ctpr1");
  run ("diag_true", diag_true,
       "ct through a %ctpr loaded from a diagnostic value");
  run ("diag_false", diag_false,
       "1 = movtd of a diagnostic value and ct ? false did not trap");
  return 0;
}
