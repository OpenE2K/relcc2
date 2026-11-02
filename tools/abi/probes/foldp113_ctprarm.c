/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * foldp113_ctprarm.c: a call after an arm that ends in a noreturn call
 * (`if (bad) { report (1); fail (); } report (2);`).  Both ways start
 * with a call of report, so a compiler may prepare it once, before the
 * test.  A prepare takes no predicate (spec/isa/control-transfer.md,
 * "How lcc uses it"), so if the arm is then predicated, its `disp` of
 * fail runs on the good way too, and the report after the arm, which
 * reads the CTPR prepared before the test, calls fail
 * (doc/gcc-port-plan.md, D3, "Predicated calls").
 *
 * Read with -S, and run on hardware:
 *
 *   /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -O3 -S \
 *       tools/abi/probes/foldp113_ctprarm.c -o -
 *   GCC -O2 tools/abi/probes/foldp113_ctprarm.c -o ctprarm
 *   tools/setup/e2k-scp ctprarm claude@ringo.lan:relcc2/<task>/
 *   tools/setup/e2k-ssh claude@ringo.lan relcc2/<task>/ctprarm; echo $?
 *
 * It prints "ok" and exits 0 when report (2) ran and fail did not; it
 * prints "fail called" and exits 1 otherwise.
 *
 * 2026-11-03 (lcc 1.29.16 -O3, v3 and v5.8c2): lcc prepares report in
 * the function's first instruction, runs the good way predicated
 * (`call %ctpr1 ? %pred0`, then the return under %pred0) and lays the
 * arm out after it as plain code that prepares report and fail again.
 * The proof-of-concept back end 0.5 at -O2 head-merged the two prepares
 * of report before the test (jump2), predicated the arm (ce3) and
 * called fail on the good way: "disp %ctpr1, report; ...; call %ctpr1
 * ? ~%pred0; disp %ctpr1, fail; call %ctpr1 ? ~%pred0; ...; call
 * %ctpr1" [hw: 4C, "fail called"]; its fixed version keeps the arm a
 * branch [hw: 4C, "ok"]; lcc's build prints "ok" [hw: 4C].
 */

#include <stdio.h>
#include <stdlib.h>

static int seen;

__attribute__ ((noinline)) void
report (int x)
{
  seen += x;
}

__attribute__ ((noinline, noreturn)) void
fail (void)
{
  puts ("fail called");
  exit (1);
}

__attribute__ ((noinline)) void
check (void *data, void *expect)
{
  if (data != expect)
    {
      report (1);
      fail ();
    }
  report (2);
}

int
main (void)
{
  int x;
  check (&x, &x);
  if (seen != 2)
    {
      printf ("seen %d\n", seen);
      return 2;
    }
  puts ("ok");
  return 0;
}
