/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Smoke test: abort (), as a failing GCC execute test does. The board
   must report fail, signal 6 (SIGABRT), and leave no core file.  */
#include <stdlib.h>

volatile int x = 1;

int main (void)
{
  if (x != 2)
    abort ();
  return 0;
}
