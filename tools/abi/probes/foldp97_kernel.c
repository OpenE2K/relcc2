/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What gdb's next does in a modulo-scheduled loop (doc/gcc-port-plan.md,
   D13, "Modulo-scheduled kernels"; spec/abi/stack-and-unwinding.md,
   "How gdb places breakpoints"; docs/spec-poc-findings.md P97).  Build
   with a GCC for E2K that modulo-schedules f's loop, run gdb on the
   machine:

     G=toolchains/poc-cross/cross-0.5+32-v3/gcc-e2k-0.5+32-v3/bin/e2k-linux-gnu-gcc
     $G -O1 -g tools/abi/probes/foldp97_kernel.c -o foldp97_kernel
     gdb -q -batch -ex 'break f' -ex run -ex 'info line *$pc' \
         -ex next -ex 'info line *$pc' -ex next -ex 'info line *$pc' \
         ./foldp97_kernel                              # on hardware

   4C, ALT's gdb 15.2-alt1.2, proof-of-concept GCC 0.5+32 at -O1
   (2026-10-24): the kernel's start carries is_stmt rows for the body's
   lines and the header, with the body's first line ("int t = ...")
   last, so gdb names the kernel by that line.  break f stops at the
   for line, next stops once in the kernel (the body's first line), and
   the next next is after the loop: the 40 iterations run under one
   next.  With the header's marker last, the kernel shows as the for
   line and next from it runs the whole loop.  GCC 16.2.1 and gdb 18.1
   on x86-64, which doesn't pipeline, stop at the for line and each
   body line in every iteration.  */
int a[100], b[100];

__attribute__((noinline)) int
f (int n)
{
  int s = 0;
  for (int i = 0; i < n; i++)
    {
      int t = a[i] * 3;
      b[i] = t + 1;
      s += t;
    }
  return s;
}

int
main (void)
{
  for (int i = 0; i < 100; i++)
    a[i] = i;
  return f (40) & 1;
}
