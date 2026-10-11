/* SPDX-License-Identifier: GPL-3.0-or-later */
/* -pg with lcc's runtime: gmon.out must name the callers of each
   function (spec/abi/kernel-interface.md, "Profiling").

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O2 -pg pgprof.c -o pgprof && ./pgprof && gprof -b pgprof  */

static volatile double sink;

static void __attribute__ ((noinline))
leaf (int n)
{
  double s = 0;
  for (int i = 0; i < n; i++)
    s += i * 0.5;
  sink = s;
}

static void __attribute__ ((noinline))
twice (int n)
{
  leaf (n);
  leaf (n);
}

int
main (void)
{
  for (int k = 0; k < 200; k++)
    twice (200000);
  for (int k = 0; k < 100; k++)
    leaf (200000);
  return 0;
}
