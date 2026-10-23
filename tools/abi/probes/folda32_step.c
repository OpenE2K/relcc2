/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Where MCST's gdb stops when "step" enters a function, next to where
   "break" on the function stops (spec/abi/stack-and-unwinding.md, "How
   gdb places breakpoints"; docs/spec-poc-findings.md A32).  work's
   first wide instruction holds its setwd and, once optimized, some of
   its body (the store to sink, the loop's compare).  Run by
   folda32_step.sh, which builds it with the PoC cross at -O0, -Og and
   -O1 and with lcc at -O0 (all -g) and runs gdb on an Elbrus machine:

     G=toolchains/poc-cross/cross-0.5+32-v3/gcc-e2k-0.5+32-v3/bin/e2k-linux-gnu-gcc
     $G -O1 -g tools/abi/probes/folda32_step.c -o step
     gdb -batch -ex 'break work' -ex run -ex 'p $pc' ./step
     gdb -batch -ex 'break main' -ex run -ex step -ex 'p $pc' ./step

   Compare each stop's $pc with work's wide instructions (objdump -d)
   and its line table (readelf --debug-dump=decodedline); gdb prints
   "0x... in work" when the stop is not at the start of an is_stmt
   row.  The program exits with status 0.  Results: folda32_step.sh's
   header.  */

volatile long sink;

__attribute__ ((noinline)) long
work (long k)
{
  sink = k;			/* FIRST_LINE */
  long t = k * 2;
  for (long i = 0; i < k; i++)
    t += i * sink;
  return t;
}

int
main (void)
{
  long r = work (5);		/* CALL_LINE */
  return r != 60;
}
