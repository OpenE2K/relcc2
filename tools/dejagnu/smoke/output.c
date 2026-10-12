/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Smoke test: output check. stdout and stderr arrive merged in the order
   written, and the last line has no newline; the board must return
   exactly "out 1\nerr 2\nout 3".  */
#include <stdio.h>

int main (void)
{
  setvbuf (stdout, NULL, _IONBF, 0);
  printf ("out 1\n");
  fprintf (stderr, "err 2\n");
  printf ("out 3");
  return 0;
}
