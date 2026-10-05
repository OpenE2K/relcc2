/* SPDX-License-Identifier: GPL-3.0-or-later */
/* MAS kinds a user program can try safely (spec/isa/speculation.md,
   "Other MAS kinds"): a non-speculative load with mod 4 (fill), mod 6
   (I/O), the reserved mod 2 and 3 in ALC0, and the m1 bit (0x10) on a
   load and a store.  Kinds that reach the MMU, caches or physical memory
   (mod 1, mod 7 in ALC2/5, stores with mod 1, 6, 7) and the lock-wait
   mod 5 load are left out on purpose.  mas_mod.s is hand-written.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $LCC -O3 tools/isa-probes/mas_mod.c tools/isa-probes/mas_mod.s -o mas_mod
     ./mas_mod                            # on hardware  */

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#define C(n) void mm_##n (unsigned long *, void *, unsigned long);
#define CASES C (ld_m4_c0) C (ld_m6_c2) C (ld_m6_c0) C (ld_m2_c0) \
  C (ld_m3_c0) C (ld_m1bit) C (ld_m1bit_c2) C (st_m1bit) C (st_m4)
CASES
#undef C

static sigjmp_buf env;
static int code;

static void
handler (int sig, siginfo_t *si, void *uc)
{
  (void) uc;
  code = si->si_code;
  siglongjmp (env, sig);
}

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
  static const struct { const char *n; void (*f) (unsigned long *, void *,
                                                  unsigned long); } c[] = {
#define C(n) { #n, mm_##n },
    CASES
#undef C
  };
  static unsigned long buf[4] __attribute__ ((aligned (64)));
  for (unsigned i = 0; i < sizeof c / sizeof *c; i++)
    {
      unsigned long out = 0;
      buf[0] = 0x1122334455667788UL;
      buf[1] = 0;
      int sig = sigsetjmp (env, 1);
      if (sig)
        printf ("%-12s signal %d code %d\n", c[i].n, sig, code);
      else
        {
          c[i].f (&out, buf, 0xabcdUL);
          printf ("%-12s load %016lx  buf[1] %lx\n", c[i].n, out, buf[1]);
        }
    }
  return 0;
}
