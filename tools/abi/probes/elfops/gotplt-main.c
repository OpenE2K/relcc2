/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Driver for gotplt.s: call through @GOTPLT slots, twice each (the
   first call may go through lazy binding).  */
#include <stdio.h>

long call_gotplt_ext (long);
long call_gotplt_loc (long);

int
main (void)
{
  long a = call_gotplt_ext (40), b = call_gotplt_ext (41);
  long c = call_gotplt_loc (40), d = call_gotplt_loc (41);
  printf ("gotplt ext %ld %ld (expect 41 42), local %ld %ld (expect 42 43)\n",
          a, b, c, d);
  return !(a == 41 && b == 42 && c == 42 && d == 43);
}
