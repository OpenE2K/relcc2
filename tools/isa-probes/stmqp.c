/* SPDX-License-Identifier: GPL-3.0-or-later */
/* stmqp, v5's masked 128-bit store (spec/isa/semantics.md): which bytes
   each mask bit writes, and misaligned addresses (an aligned 16-byte
   store never crosses a page, so a masked-off part can't be on an
   unmapped one).  stmqp.s is hand-written.

     LCC5=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc
     $LCC5 -O3 tools/isa-probes/stmqp.c tools/isa-probes/stmqp.s -o stmqp
     ./stmqp                              # on v5 hardware  */

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void stm (void *, unsigned, const unsigned long *);

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
show (const char *what, unsigned char *p, int n)
{
  printf ("%-30s", what);
  for (int k = 0; k < n; k++)
    printf ("%02x%s", p[k], k % 16 == 15 ? " " : "");
  printf ("\n");
}

int
main (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = handler;
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;
  sigaction (SIGSEGV, &sa, NULL);
  sigaction (SIGBUS, &sa, NULL);
  sigaction (SIGILL, &sa, NULL);
  static const unsigned long v[2] __attribute__ ((aligned (16))) = {
    0x0706050403020100UL, 0x0f0e0d0c0b0a0908UL };
  static unsigned char buf[64] __attribute__ ((aligned (16)));
  static const unsigned masks[] = { 0x0000, 0x0001, 0x8000, 0x00ff, 0xff00,
                                    0xaaaa, 0xffff, 0x10000, 0xffff0000 };
  for (unsigned i = 0; i < sizeof masks / sizeof *masks; i++)
    {
      memset (buf, 0xee, sizeof buf);
      stm (buf + 16, masks[i], v);
      char w[40];
      snprintf (w, sizeof w, "mask %#x:", masks[i]);
      show (w, buf + 16, 16);
    }
  /* Misaligned addresses.  */
  for (int off = 1; off < 16; off *= 2)
    {
      memset (buf, 0xee, sizeof buf);
      char w[40];
      snprintf (w, sizeof w, "mask 0xffff at +%d:", off);
      if (sigsetjmp (env, 1))
        {
          printf ("%-30ssignal %d code %d\n", w, sig, code);
          continue;
        }
      stm (buf + 16 + off, 0xffff, v);
      show (w, buf + 16, 32);
    }
  return 0;
}
