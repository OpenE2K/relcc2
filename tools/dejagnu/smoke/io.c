/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Smoke test: arguments and an input file on stdin. Prints the arguments
   and the number of bytes and lines read.  */
#include <stdio.h>

int main (int argc, char **argv)
{
  int c;
  long bytes = 0, lines = 0;
  printf ("argc=%d", argc);
  for (int i = 1; i < argc; i++)
    printf (" [%s]", argv[i]);
  while ((c = getchar ()) != EOF)
    {
      bytes++;
      lines += c == '\n';
    }
  printf (" stdin=%ld/%ld\n", bytes, lines);
  return 0;
}
