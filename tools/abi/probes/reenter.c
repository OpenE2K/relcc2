/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Re-entering a frame at a label through glibc's jmp_buf, as a GCC
   port's nonlocal goto and __builtin_longjmp can
   (spec/abi/stack-and-unwinding.md).  reenter.s calls _setjmp, writes
   the address of a label over the saved IP (byte 8, %cr0.hi) and calls
   reenter_deep, which longjmps from DEPTH frames further down.  (The
   target is assembly because lcc's &&label is 0.)

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O1 tools/abi/probes/reenter.c tools/abi/probes/reenter.s \
         -o reenter && ./reenter  */

#include <setjmp.h>
#include <stdio.h>

long reenter (jmp_buf, long);
static jmp_buf buf;

void __attribute__ ((noinline))
reenter_deep (long n)
{
  if (n > 0)
    reenter_deep (n - 1);
  else
    longjmp (buf, 1);
  printf ("reenter_deep returned\n");
}

int
main (void)
{
  int ok = 1;
  for (long d = 0; d <= 400; d = d ? d * 2 : 1)
    {
      long r = reenter (buf, d);
      printf ("depth %3ld: %#lx\n", d, r);
      ok &= r == 0x5678;
    }
  printf ("%s\n", ok ? "all re-entered at the label" : "FAILED");
  return !ok;
}
