/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Smoke test: 1 MiB of output (16384 lines of 64 bytes), then "end"
   without a newline. The board must return all of it.  */
#include <stdio.h>

int main (void)
{
  for (int i = 0; i < 16384; i++)
    printf ("%05d %057d\n", i, 0);
  printf ("end");
  return 0;
}
