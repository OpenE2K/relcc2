// SPDX-License-Identifier: GPL-3.0-or-later
// Does MCST's gdb print a variable whose location is several
// DW_OP_implicit_pointer pieces?  GCC describes an optimized lambda that
// captures two variables by reference that way (-O2 -g); gdb 15.2's
// coerce_pieced_ref asserts a single piece (gdb/dwarf2/expr.c:597 at
// OpenE2K/gdb 818fc87ba33; upstream PR gdb/30693).  lcc writes no
// locations at -O2, so the probe needs a GCC for E2K:
//
//   G=toolchains/build-gcc-claude3/gcc
//   $G/xg++ -B$G -O2 -g -c tools/abi/probes/folddebug_lambda.cc \
//     --sysroot=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/fs -o lam.o
//   /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc lam.o -o lam
//   gdb -batch -ex 'break folddebug_lambda.cc:BREAK_LINE' -ex run \
//       -ex 'p lam' -ex 'p lam.__k' ./lam
//
// Results (2026-10-16): ALT's gdb 15.2-alt1.2 on the 4C prints
// {__k = <synthetic pointer>, __argc = <synthetic pointer>} and, for
// lam.__k, "(int &) <synthetic pointer>: <optimized out>"; OS Elbrus's
// 15.2-29.023 on the 8C2 stops with "internal-error: coerce_pieced_ref:
// Assertion `closure->pieces.size () == 1' failed".

volatile int sink;

__attribute__ ((noinline)) void
use (int x)
{
  sink = x;
}

int
main (int argc, char **)
{
  int k = argc * 7;
  auto lam = [&] (int m) { return k * m + argc; };
  use (lam (3));
  use (k);			// BREAK_LINE
  return 0;
}
